//! The interactive loop behind bare `calm`.
//!
//! Line editing itself (raw-mode input, autosuggestions, syntax
//! highlighting, history search) is reedline — see `line_editor.rs` for
//! the Prompt/Highlighter implementations and engine setup. This module
//! owns everything *around* that: environment variables, `[aliases]` +
//! `[directory.aliases]` expansion, `[functions]` and installed plugins
//! (both sourced as a preamble before each command — see
//! `build_prelude`), the `cd`/`pushd`/`popd`/`dirs` builtins plus
//! `auto_cd`, directory-change syncing for everything else (see
//! `sync_cwd_from_state_file`), the terminal-title OSC sequence, the
//! error bell, and Ctrl+C signal handling around spawned commands.

use crate::calm_format::CalmDocument;
use crate::config;
use crate::line_editor;
use crate::theme_engine::Theme;
use anyhow::Result;
use reedline::Signal;
use std::cell::RefCell;
use std::collections::HashMap;
use std::env;
use std::io::Write;
use std::os::unix::process::CommandExt;
use std::path::{Path, PathBuf};
use std::process::Command;

pub struct ReplContext {
    aliases: HashMap<String, String>,
    /// Shell source prepended to every spawned command: function
    /// definitions from functions.calm plus each installed plugin's
    /// init.sh, concatenated once at startup (these don't change mid
    /// session) rather than rebuilt per command.
    prelude: String,
    /// `directory.calm`'s `auto_cd` — a bare, unambiguous directory name
    /// typed at the prompt (no spaces, not a builtin) changes into it
    /// instead of failing with "command not found".
    auto_cd: bool,
    /// `terminal.calm`'s `bell` — whether calm's own error paths (failed
    /// `cd`/`pushd`/`popd`, a command that couldn't be spawned at all)
    /// ring the terminal bell. Off by default, matching the scaffold.
    bell: bool,
    /// Backing store for `pushd`/`popd`/`dirs`, capped at
    /// `directory.calm`'s `dir_stack_size`. `RefCell` because `ReplContext`
    /// is held immutably through the read loop (see `run`) but the stack
    /// still needs to mutate as commands run.
    dir_stack: RefCell<Vec<PathBuf>>,
    dir_stack_size: usize,
}

impl ReplContext {
    pub fn load(cfg: &CalmDocument) -> ReplContext {
        apply_environment(cfg);

        let mut aliases = HashMap::new();
        if let Some(section) = cfg.section("aliases") {
            for (k, v) in section {
                if let Some(s) = v.as_str() {
                    aliases.insert(k.clone(), s.to_string());
                }
            }
        }
        if let Some(section) = cfg.section("directory.aliases") {
            for (k, v) in section {
                if let Some(s) = v.as_str() {
                    aliases.insert(k.clone(), s.to_string());
                }
            }
        }

        let prelude = build_prelude(cfg);
        let auto_cd = cfg.get_bool("directory", "auto_cd").unwrap_or(false);
        let bell = cfg.get_bool("terminal", "bell").unwrap_or(false);
        let dir_stack_size = cfg
            .get("directory", "dir_stack_size")
            .and_then(|v| v.as_int())
            .filter(|n| *n > 0)
            .unwrap_or(20) as usize;

        ReplContext {
            aliases,
            prelude,
            auto_cd,
            bell,
            dir_stack: RefCell::new(Vec::new()),
            dir_stack_size,
        }
    }

    /// Pushes a directory onto the `pushd`/`popd` stack, dropping the
    /// oldest entry once `dir_stack_size` is exceeded (same "bounded
    /// history" shape as `history.calm`'s `size`).
    fn push_dir(&self, path: PathBuf) {
        let mut stack = self.dir_stack.borrow_mut();
        stack.push(path);
        if stack.len() > self.dir_stack_size {
            stack.remove(0);
        }
    }

    fn pop_dir(&self) -> Option<PathBuf> {
        self.dir_stack.borrow_mut().pop()
    }

    fn dir_stack_snapshot(&self) -> Vec<PathBuf> {
        self.dir_stack.borrow().clone()
    }

    /// Expands a typed line against the alias table. Whole-line matches
    /// (e.g. typing exactly `..`) take priority over first-word matches
    /// (e.g. typing `ll -h` where `ll` is aliased), matching how shell
    /// aliases behave.
    fn expand(&self, line: &str) -> String {
        if let Some(target) = self.aliases.get(line) {
            return target.clone();
        }
        let mut parts = line.splitn(2, char::is_whitespace);
        let first = parts.next().unwrap_or("");
        let rest = parts.next().unwrap_or("");
        match self.aliases.get(first) {
            Some(target) if rest.is_empty() => target.clone(),
            Some(target) => format!("{target} {rest}"),
            None => line.to_string(),
        }
    }
}

fn apply_environment(cfg: &CalmDocument) {
    if let Some(section) = cfg.section("environment") {
        for (k, v) in section {
            if let Some(s) = v.as_str() {
                env::set_var(k, s);
            }
        }
    }
}

/// Builds the shell preamble sourced before every command: one `name() { ... }`
/// per entry in `[functions]`, followed by each installed plugin's `init.sh`.
///
/// A function or plugin's `cd` *does* persist now (see
/// `sync_cwd_from_state_file`) — this preamble still runs inside the
/// spawned `sh -c`, but that subshell reports its final `$PWD` back
/// afterward. `export`ed variables don't sync back yet, though: same
/// mechanism could extend to cover that, just not done in this pass.
fn build_prelude(cfg: &CalmDocument) -> String {
    let mut prelude = String::new();

    if let Some(section) = cfg.section("functions") {
        for (name, body) in section {
            if let Some(body) = body.as_str() {
                prelude.push_str(name);
                prelude.push_str("() {\n");
                prelude.push_str(body);
                prelude.push_str("\n}\n");
            }
        }
    }

    if let Ok(plugins_dir) = config::plugins_dir() {
        if let Ok(entries) = std::fs::read_dir(&plugins_dir) {
            for entry in entries.filter_map(|e| e.ok()) {
                let init = entry.path().join("init.sh");
                if let Ok(contents) = std::fs::read_to_string(&init) {
                    prelude.push_str(&format!("# plugin: {}\n", entry.file_name().to_string_lossy()));
                    prelude.push_str(&contents);
                    prelude.push('\n');
                }
            }
        }
    }

    prelude
}

pub fn run(theme: &Theme, cfg: &CalmDocument, icon: &str) -> Result<()> {
    let ctx = ReplContext::load(cfg);

    // The shell and any foreground child share the terminal's foreground
    // process group (no job control / setpgid here), so a Ctrl+C at the
    // terminal delivers SIGINT to both. Ignoring it here means `calm`
    // itself survives; `run_command` resets it to default disposition in
    // the child so the actual running command still gets interrupted
    // normally. reedline puts the terminal in raw mode while editing, which
    // disables ISIG entirely (Ctrl+C arrives as a literal byte, translated
    // to Signal::CtrlC below, never a real SIGINT) — so this ignore only
    // actually matters for the "child command is running" window, when the
    // terminal is back in normal mode.
    unsafe {
        libc::signal(libc::SIGINT, libc::SIG_IGN);
    }

    let known = line_editor::known_commands(cfg);
    let mut engine = line_editor::build_engine(theme, cfg, known);
    let prompt = line_editor::CalmPrompt::new(theme, icon);

    // `terminal.calm`'s `set_title` — was scaffolded into every user's
    // config from day one but never actually read anywhere; wired here.
    let set_title = cfg.get_bool("terminal", "set_title").unwrap_or(true);

    // One fixed path per calm process — each command's report overwrites
    // it, no per-command temp-file churn needed.
    let state_file = env::temp_dir().join(format!("calm-shell-state-{}", std::process::id()));

    loop {
        if set_title {
            update_terminal_title();
        }
        match engine.read_line(&prompt) {
            Ok(Signal::Success(line)) => {
                let line = line.trim();
                if line.is_empty() {
                    continue;
                }
                if line == "exit" || line == "quit" {
                    break;
                }
                let expanded = ctx.expand(line);
                if let Some(dir) = auto_cd_target(&expanded, ctx.auto_cd) {
                    apply_cwd_change(&dir);
                    continue;
                }
                run_command(&expanded, &ctx, &state_file);
            }
            Ok(Signal::CtrlC) => {
                // No foreground job at the prompt itself — matches bash's
                // "fresh prompt" behavior on an idle Ctrl+C.
                continue;
            }
            Ok(Signal::CtrlD) => {
                break;
            }
            Ok(_) => {
                // Other Signal variants (non-exhaustive enum) — no-op,
                // redraw on the next loop iteration.
                continue;
            }
            Err(e) => {
                eprintln!("calm: line editor error: {e}");
                break;
            }
        }
    }

    let _ = std::fs::remove_file(&state_file);
    Ok(())
}

/// Updates the process's cwd and OLDPWD together. Shared by the `cd`
/// builtin and `sync_cwd_from_state_file`, so both keep OLDPWD consistent
/// the same way regardless of which path triggered the change.
fn apply_cwd_change(new_dir: &Path) {
    let prev = env::current_dir().ok();
    if let Err(e) = env::set_current_dir(new_dir) {
        eprintln!("cd: {}: {e}", new_dir.display());
        return;
    }
    if let Some(p) = prev {
        env::set_var("OLDPWD", p);
    }
}

/// Reads back the `$PWD` a spawned command's subshell reported (see the
/// `printf` appended in `run_command`) and applies it if it's a real,
/// different directory. This is what makes `cd` inside a function or
/// plugin — or a plain `mkdir foo && cd foo` typed directly — actually
/// stick, without needing a persistent shell process: the state travels
/// through a side channel (a fixed temp file, NUL-terminated so an
/// unusual path containing a newline still parses correctly) that never
/// touches the command's real stdout/stderr.
fn sync_cwd_from_state_file(state_file: &Path) {
    let Ok(contents) = std::fs::read(state_file) else { return };
    let Some(nul_pos) = contents.iter().position(|&b| b == 0) else { return };
    let Ok(reported) = std::str::from_utf8(&contents[..nul_pos]) else { return };
    let reported_path = PathBuf::from(reported);
    if !reported_path.is_dir() {
        return;
    }
    if env::current_dir().ok().as_deref() != Some(reported_path.as_path()) {
        apply_cwd_change(&reported_path);
    }
}

/// Sets the terminal tab/window title to `calm — <cwd>` via the standard
/// OSC 0 escape sequence. Best-effort: if the terminal doesn't understand
/// OSC 0 it just shows a few harmless stray bytes, nothing breaks.
fn update_terminal_title() {
    print!("\x1b]0;calm — {}\x07", line_editor::display_path());
    let _ = std::io::stdout().flush();
}

/// `directory.calm`'s `auto_cd`: a bare word with no whitespace that isn't
/// one of the directory-changing builtins itself, and that resolves (after
/// `~`/`$VAR` expansion) to a real directory, is treated as `cd <word>`
/// rather than being handed to `sh` to fail on with "command not found".
fn auto_cd_target(line: &str, enabled: bool) -> Option<PathBuf> {
    if !enabled {
        return None;
    }
    let line = line.trim();
    if line.is_empty() || line.chars().any(char::is_whitespace) {
        return None;
    }
    if matches!(line, "cd" | "pushd" | "popd" | "dirs" | "exit" | "quit") {
        return None;
    }
    let path = PathBuf::from(crate::calm_format::expand(line));
    path.is_dir().then_some(path)
}

/// Rings the terminal bell if `terminal.calm`'s `bell` is enabled. Used on
/// calm's own error paths (`cd`/`pushd`/`popd` failures, a command that
/// couldn't even be spawned) — not on every nonzero exit status, which
/// would make `bell = true` unbearably noisy for normal `grep`/`test`-style
/// commands that fail on purpose.
fn ring_bell(enabled: bool) {
    if enabled {
        print!("\x07");
        let _ = std::io::stdout().flush();
    }
}

/// Prints the directory stack the way `dirs`/`pushd`/`popd` conventionally
/// do: current directory first, then the stack from most to least recent.
fn print_dir_stack(ctx: &ReplContext) {
    let cwd = env::current_dir().unwrap_or_default();
    print!("{}", cwd.display());
    for path in ctx.dir_stack_snapshot().iter().rev() {
        print!(" {}", path.display());
    }
    println!();
}

fn run_command(command: &str, ctx: &ReplContext, state_file: &Path) {
    let trimmed = command.trim();

    // `cd`, `pushd`, `popd`, and `dirs` stay fast, dependency-free builtins
    // — no subprocess spawn, no state-file round trip. Everything else
    // (functions, plugins, compound lines like `mkdir x && cd x`) goes
    // through the sync mechanism below instead.
    if trimmed == "cd" || trimmed.starts_with("cd ") {
        let target = trimmed.strip_prefix("cd").unwrap_or("").trim();
        let dest = if target.is_empty() {
            dirs::home_dir()
        } else if target == "-" {
            env::var("OLDPWD").ok().map(PathBuf::from)
        } else {
            Some(PathBuf::from(crate::calm_format::expand(target)))
        };

        match dest {
            Some(path) if path.is_dir() => apply_cwd_change(&path),
            Some(path) => {
                eprintln!("cd: {}: not a directory", path.display());
                ring_bell(ctx.bell);
            }
            None => {
                eprintln!("cd: could not resolve target directory");
                ring_bell(ctx.bell);
            }
        }
        return;
    }

    if trimmed == "dirs" {
        print_dir_stack(ctx);
        return;
    }

    if trimmed == "popd" {
        match ctx.pop_dir() {
            Some(path) => {
                apply_cwd_change(&path);
                print_dir_stack(ctx);
            }
            None => {
                eprintln!("popd: directory stack empty");
                ring_bell(ctx.bell);
            }
        }
        return;
    }

    if trimmed == "pushd" || trimmed.starts_with("pushd ") {
        let target = trimmed.strip_prefix("pushd").unwrap_or("").trim();
        if target.is_empty() {
            eprintln!("pushd: no directory specified");
            ring_bell(ctx.bell);
            return;
        }
        let path = PathBuf::from(crate::calm_format::expand(target));
        if !path.is_dir() {
            eprintln!("pushd: {}: not a directory", path.display());
            ring_bell(ctx.bell);
            return;
        }
        if let Ok(cwd) = env::current_dir() {
            ctx.push_dir(cwd);
        }
        apply_cwd_change(&path);
        print_dir_stack(ctx);
        return;
    }

    let mut script = if ctx.prelude.is_empty() {
        trimmed.to_string()
    } else {
        format!("{}\n{trimmed}", ctx.prelude)
    };
    // Runs unconditionally after the command (newline sequencing, not
    // `&&`), so even a failed command still reports where it left us.
    // >&3 would be cleaner than a temp file but needs pre_exec fd-juggling
    // for no real benefit here — this is simple and it's a single write.
    script.push_str(&format!(
        "\nprintf '%s\\0' \"$PWD\" > '{}' 2>/dev/null",
        state_file.display()
    ));

    let mut cmd = Command::new("sh");
    cmd.arg("-c").arg(&script);
    // SAFETY: only async-signal-safe calls (signal(2) itself) in the
    // post-fork, pre-exec child.
    unsafe {
        cmd.pre_exec(|| {
            libc::signal(libc::SIGINT, libc::SIG_DFL);
            libc::signal(libc::SIGQUIT, libc::SIG_DFL);
            Ok(())
        });
    }
    let status = cmd.status();
    if let Err(e) = status {
        eprintln!("calm: failed to run `{trimmed}`: {e}");
        ring_bell(ctx.bell);
    }
    sync_cwd_from_state_file(state_file);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn auto_cd_disabled_never_triggers() {
        let dir = env::temp_dir();
        assert_eq!(auto_cd_target(dir.to_str().unwrap(), false), None);
    }

    #[test]
    fn auto_cd_ignores_multi_word_lines() {
        let dir = env::temp_dir();
        let line = format!("{} extra", dir.display());
        assert_eq!(auto_cd_target(&line, true), None);
    }

    #[test]
    fn auto_cd_ignores_builtin_names() {
        assert_eq!(auto_cd_target("cd", true), None);
        assert_eq!(auto_cd_target("pushd", true), None);
        assert_eq!(auto_cd_target("popd", true), None);
        assert_eq!(auto_cd_target("dirs", true), None);
        assert_eq!(auto_cd_target("exit", true), None);
    }

    #[test]
    fn auto_cd_ignores_nonexistent_or_non_directory_words() {
        assert_eq!(auto_cd_target("this-directory-does-not-exist-xyz", true), None);
    }

    #[test]
    fn auto_cd_matches_a_real_directory() {
        let dir = env::temp_dir();
        let target = auto_cd_target(dir.to_str().unwrap(), true);
        assert_eq!(target, Some(dir));
    }

    #[test]
    fn dir_stack_pushes_pops_and_caps_at_size() {
        let ctx = ReplContext {
            aliases: HashMap::new(),
            prelude: String::new(),
            auto_cd: false,
            bell: false,
            dir_stack: RefCell::new(Vec::new()),
            dir_stack_size: 2,
        };
        ctx.push_dir(PathBuf::from("/one"));
        ctx.push_dir(PathBuf::from("/two"));
        ctx.push_dir(PathBuf::from("/three"));
        // Oldest ("/one") dropped once the cap of 2 is exceeded.
        assert_eq!(
            ctx.dir_stack_snapshot(),
            vec![PathBuf::from("/two"), PathBuf::from("/three")]
        );
        assert_eq!(ctx.pop_dir(), Some(PathBuf::from("/three")));
        assert_eq!(ctx.pop_dir(), Some(PathBuf::from("/two")));
        assert_eq!(ctx.pop_dir(), None);
    }
}
