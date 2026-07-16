//! Wires reedline in as calm's line editor.
//!
//! Replaces the plain `io::stdin().read_line()` loop with a real line
//! editor: theme-driven multi-line prompt, syntax highlighting (command
//! word / strings / flags), fish-style history autosuggestions (reedline's
//! own `DefaultHinter`, which is literally built for this), persistent
//! history with Up/Down + Ctrl+R search, and Tab completion (commands at
//! the first word, filesystem paths everywhere else — see
//! `CalmCompleter`) rendered through reedline's own `ColumnarMenu`.
//! keyboard.calm's `edit_mode` actually does something now too (emacs vs
//! vi keybindings).
//!
//! Scope honestly: the highlighter is a single-pass tokenizer (command
//! word, quoted strings, `-flag`s), not a real shell grammar — good
//! enough to be genuinely useful, not a claim to full POSIX parsing.
//! `history.calm`'s `size`/`save` both feed reedline's history capacity
//! now (see `build_engine`); `ignore_dups`/`share_across_sessions` still
//! aren't wired to anything, noted rather than silently pretended-away.
//! Same for `keyboard.calm`'s `[keyboard.bindings]` table and
//! `terminal.calm`'s `truecolor` flag: `edit_mode` (emacs/vi) is real,
//! but the four bindings in the scaffold happen to already match
//! reedline's emacs defaults rather than actually being read from config
//! — remapping a binding in that file currently does nothing. `truecolor`
//! is likewise unread; `calm doctor` checks `$COLORTERM` independently
//! but nothing degrades the palette for terminals without it yet.

use crate::calm_format::CalmDocument;
use crate::config;
use crate::git;
use crate::theme_engine::Theme;
use nu_ansi_term::{Color as NuColor, Style};
use reedline::{
    default_emacs_keybindings, default_vi_insert_keybindings, default_vi_normal_keybindings,
    ColumnarMenu, Completer, DefaultHinter, Emacs, FileBackedHistory, Highlighter, KeyCode,
    KeyModifiers, Keybindings, ListMenu, MenuBuilder, OutputMode, Prompt, PromptEditMode,
    PromptHistorySearch, Reedline, ReedlineEvent, ReedlineMenu, Span, StyledText, Suggestion, Vi,
};
use std::borrow::Cow;
use std::collections::HashSet;
use std::env;
use std::path::{Path, PathBuf};

fn theme_color(theme: &Theme, key: &str) -> NuColor {
    match theme.rgb(key) {
        Some((r, g, b)) => NuColor::Rgb(r, g, b),
        None => NuColor::Default,
    }
}

/// Shared with `repl.rs` for the terminal-title OSC sequence, so both the
/// prompt and the title format the cwd identically (home-relative `~/...`).
pub(crate) fn display_path() -> String {
    let cwd = env::current_dir().unwrap_or_default();
    let cwd_str = cwd.display().to_string();
    match dirs::home_dir() {
        Some(home) => {
            let home_str = home.display().to_string();
            cwd_str.strip_prefix(&home_str).map(|s| format!("~{s}")).unwrap_or(cwd_str)
        }
        None => cwd_str,
    }
}

fn hostname() -> String {
    let mut buf = vec![0u8; 256];
    // SAFETY: buf is a valid, writable buffer of the given length; gethostname
    // writes at most buf.len() bytes and null-terminates on success.
    let ret = unsafe { libc::gethostname(buf.as_mut_ptr() as *mut libc::c_char, buf.len()) };
    if ret != 0 {
        return "endeavouros".to_string();
    }
    let end = buf.iter().position(|&b| b == 0).unwrap_or(buf.len());
    let name = String::from_utf8_lossy(&buf[..end]).to_string();
    if name.is_empty() {
        "endeavouros".to_string()
    } else {
        name
    }
}

/// Scans $PATH once at startup for executable names, so the highlighter
/// can color "is this a real command" per keystroke via a HashSet lookup
/// instead of hitting the filesystem on every character typed.
fn scan_path_commands() -> HashSet<String> {
    let mut found = HashSet::new();
    if let Ok(path_var) = env::var("PATH") {
        for dir in path_var.split(':') {
            let Ok(entries) = std::fs::read_dir(dir) else { continue };
            for entry in entries.filter_map(|e| e.ok()) {
                if let Some(name) = entry.file_name().to_str() {
                    found.insert(name.to_string());
                }
            }
        }
    }
    found
}

/// Theme-driven multi-line prompt matching calm-shell's box design.
/// The whole box (all rows, including the final `╰─ ❯`) is rendered by
/// `render_prompt_left`; the indicator is left empty rather than split
/// out, since our arrow is already part of the box's last line.
pub struct CalmPrompt {
    border: NuColor,
    path: NuColor,
    git_label: NuColor,
    git_clean: NuColor,
    git_dirty: NuColor,
    arrow: NuColor,
    accent: NuColor,
    icon: String,
}

impl CalmPrompt {
    pub fn new(theme: &Theme, icon: &str) -> CalmPrompt {
        CalmPrompt {
            border: theme_color(theme, "border"),
            path: theme_color(theme, "path"),
            git_label: theme_color(theme, "gentle_pink"),
            git_clean: theme_color(theme, "git_clean"),
            git_dirty: theme_color(theme, "git_dirty"),
            arrow: theme_color(theme, "arrow"),
            accent: theme_color(theme, "calm_purple"),
            icon: icon.to_string(),
        }
    }

    fn paint(&self, color: NuColor, text: &str) -> String {
        Style::new().fg(color).paint(text).to_string()
    }
}

impl Prompt for CalmPrompt {
    fn render_prompt_left(&self) -> Cow<'_, str> {
        let user = env::var("USER").unwrap_or_else(|_| "user".to_string());
        let host = hostname();
        let path = display_path();

        let mut out = String::new();
        out.push_str(&self.paint(self.border, "╭─ "));
        out.push_str(&self.paint(self.path, &format!("{user}@{host}")));
        out.push('\n');
        out.push_str(&self.paint(self.border, "│  "));
        out.push_str(&self.icon);
        out.push(' ');
        out.push_str(&self.paint(self.accent, "Calm-shell"));
        out.push('\n');
        out.push_str(&self.paint(self.border, "│  "));
        out.push_str(&self.paint(self.path, &path));
        out.push('\n');

        if let Some(status) = git::status() {
            let (marker, color) = if status.clean { ("✓", self.git_clean) } else { ("✗", self.git_dirty) };
            out.push_str(&self.paint(self.border, "│  "));
            out.push_str(&self.paint(self.git_label, &format!("git:{}", status.branch)));
            out.push(' ');
            out.push_str(&self.paint(color, marker));
            out.push('\n');
        }

        out.push_str(&self.paint(self.border, "╰─ "));
        out.push_str(&self.paint(self.arrow, "❯ "));

        Cow::Owned(out)
    }

    fn render_prompt_right(&self) -> Cow<'_, str> {
        Cow::Borrowed("")
    }

    fn render_prompt_indicator(&self, _prompt_mode: PromptEditMode) -> Cow<'_, str> {
        Cow::Borrowed("")
    }

    fn render_prompt_multiline_indicator(&self) -> Cow<'_, str> {
        Cow::Borrowed(":::  ")
    }

    fn render_prompt_history_search_indicator(&self, _history_search: PromptHistorySearch) -> Cow<'_, str> {
        Cow::Borrowed("(reverse-search) ")
    }
}

/// A single-pass tokenizer highlighting the command word (green if it
/// resolves to a builtin/alias/function/$PATH executable, red if not),
/// quoted strings, and `-flag`-shaped words. Not a shell parser — no
/// pipes/subshells/operators awareness — but real, working, visible
/// syntax highlighting rather than a stub.
pub struct CalmHighlighter {
    known: HashSet<String>,
    command_color: NuColor,
    unknown_color: NuColor,
    string_color: NuColor,
    flag_color: NuColor,
}

impl CalmHighlighter {
    pub fn new(theme: &Theme, known: HashSet<String>) -> CalmHighlighter {
        CalmHighlighter {
            known,
            command_color: theme_color(theme, "git_clean"),
            unknown_color: theme_color(theme, "soft_red"),
            string_color: theme_color(theme, "warm_yellow"),
            flag_color: theme_color(theme, "soft_blue"),
        }
    }

    fn is_known_command(&self, word: &str) -> bool {
        if word.is_empty() {
            return false;
        }
        if word == "cd" || word == "exit" || word == "quit" || self.known.contains(word) {
            return true;
        }
        if word.contains('/') {
            return std::path::Path::new(word).is_file();
        }
        self.known.contains(word)
    }
}

impl Highlighter for CalmHighlighter {
    fn highlight(&self, line: &str, _cursor: usize) -> StyledText {
        let mut styled = StyledText::new();
        let mut chars = line.char_indices().peekable();
        let mut first_word = true;

        while let Some(&(start, c)) = chars.peek() {
            if c.is_whitespace() {
                let mut end = start + c.len_utf8();
                chars.next();
                while let Some(&(i, c2)) = chars.peek() {
                    if !c2.is_whitespace() {
                        break;
                    }
                    end = i + c2.len_utf8();
                    chars.next();
                }
                styled.push((Style::new(), line[start..end].to_string()));
                continue;
            }

            if c == '"' || c == '\'' {
                let quote = c;
                chars.next();
                let mut end = start + c.len_utf8();
                for (i, c2) in chars.by_ref() {
                    end = i + c2.len_utf8();
                    if c2 == quote {
                        break;
                    }
                }
                styled.push((Style::new().fg(self.string_color), line[start..end].to_string()));
                first_word = false;
                continue;
            }

            let mut end = start;
            while let Some(&(i, c2)) = chars.peek() {
                if c2.is_whitespace() || c2 == '"' || c2 == '\'' {
                    break;
                }
                end = i + c2.len_utf8();
                chars.next();
            }
            let word = &line[start..end];
            let style = if first_word {
                if self.is_known_command(word) {
                    Style::new().fg(self.command_color).bold()
                } else {
                    Style::new().fg(self.unknown_color)
                }
            } else if word.starts_with('-') {
                Style::new().fg(self.flag_color)
            } else {
                Style::new()
            };
            styled.push((style, word.to_string()));
            first_word = false;
        }

        styled
    }
}

/// Builtins that don't show up in `known_commands` (they're not $PATH
/// executables or config-defined aliases/functions) but should still
/// tab-complete like any other command.
const BUILTIN_COMMANDS: &[&str] = &["cd", "pushd", "popd", "dirs", "exit", "quit"];

/// Tab-completion. Deliberately not reedline's `DefaultCompleter` (a flat
/// word-list matcher) — completion here has two genuinely different modes
/// depending on cursor position: the first word completes against known
/// *commands*, everything after it completes against the *filesystem*.
/// The menu/keybinding/rendering plumbing this plugs into is entirely
/// reedline's (see `build_engine`); this struct only decides *what* the
/// candidates are.
pub struct CalmCompleter {
    known_commands: Vec<String>,
}

impl CalmCompleter {
    pub fn new(known: &HashSet<String>) -> CalmCompleter {
        let mut known_commands: Vec<String> = known.iter().cloned().collect();
        known_commands.sort();
        CalmCompleter { known_commands }
    }

    fn complete_commands(&self, word: &str, start: usize, pos: usize) -> Vec<Suggestion> {
        let mut names: Vec<&str> = BUILTIN_COMMANDS
            .iter()
            .copied()
            .chain(self.known_commands.iter().map(String::as_str))
            .filter(|c| c.starts_with(word))
            .collect();
        names.sort_unstable();
        names.dedup();
        names
            .into_iter()
            .map(|name| Suggestion {
                value: name.to_string(),
                span: Span::new(start, pos),
                append_whitespace: true,
                ..Default::default()
            })
            .collect()
    }

    fn complete_paths(&self, word: &str, start: usize, pos: usize) -> Vec<Suggestion> {
        // Split "some/dir/partial" into the directory to scan and the
        // filename prefix to match, keeping the directory portion exactly
        // as typed (`~/`, `../`, relative, absolute) so the suggestion we
        // hand back still fits what the user already wrote.
        let (typed_dir, file_prefix) = match word.rfind('/') {
            Some(i) => (&word[..=i], &word[i + 1..]),
            None => ("", word),
        };

        let scan_dir: PathBuf = if typed_dir.is_empty() {
            env::current_dir().unwrap_or_default()
        } else {
            PathBuf::from(crate::calm_format::expand(typed_dir))
        };

        let Ok(entries) = std::fs::read_dir(&scan_dir) else {
            return Vec::new();
        };

        let mut suggestions: Vec<Suggestion> = entries
            .filter_map(|e| e.ok())
            .filter_map(|entry| {
                let name = entry.file_name().to_str()?.to_string();
                if !name.starts_with(file_prefix) {
                    return None;
                }
                // Same convention as bash/zsh globbing: dotfiles only show
                // up once the user has actually started typing a dot.
                if name.starts_with('.') && !file_prefix.starts_with('.') {
                    return None;
                }
                let is_dir = entry.file_type().map(|t| t.is_dir()).unwrap_or(false);
                let mut value = format!("{typed_dir}{name}");
                if is_dir {
                    value.push('/');
                }
                Some(Suggestion {
                    value,
                    span: Span::new(start, pos),
                    // Leave the cursor right after a directory's `/` so a
                    // directory can be completed again immediately,
                    // rather than inserting a trailing space mid-path.
                    append_whitespace: !is_dir,
                    ..Default::default()
                })
            })
            .collect();
        suggestions.sort_by(|a, b| a.value.cmp(&b.value));
        suggestions
    }
}

impl Completer for CalmCompleter {
    fn complete(&mut self, line: &str, pos: usize) -> Vec<Suggestion> {
        let before_cursor = &line[..pos];
        let word_start = before_cursor
            .rfind(char::is_whitespace)
            .map(|i| i + 1)
            .unwrap_or(0);
        let word = &before_cursor[word_start..];
        let is_first_word = before_cursor[..word_start].trim().is_empty();

        if is_first_word && !word.contains('/') {
            self.complete_commands(word, word_start, pos)
        } else {
            self.complete_paths(word, word_start, pos)
        }
    }
}

/// Reads reedline's own history file directly rather than going through
/// its `History` trait — that trait's search is Prefix/Substring/Exact
/// only (there's no fuzzy variant to hook into), so fuzzy matching has to
/// happen on our side regardless. The on-disk format is one entry per
/// line, with embedded literal newlines escaped as `<\n>`
/// (`reedline::FileBackedHistory`'s own `NEWLINE_ESCAPE`) — matched here
/// so multi-line history entries round-trip instead of getting mangled.
fn read_history_entries(path: &Path) -> Vec<String> {
    let Ok(content) = std::fs::read_to_string(path) else {
        return Vec::new();
    };
    content
        .lines()
        .map(|line| line.replace("<\\n>", "\n"))
        .filter(|line| !line.trim().is_empty())
        .collect()
}

/// Fuzzy history search (Ctrl+R) — most-recent-first, deduplicated, and
/// scored with `fuzzy-matcher`'s skim algorithm rather than a hand-rolled
/// one. Re-reads the history file at construction time only (once per
/// `calm` process start); this session's own new entries won't show up
/// in the fuzzy menu until the next session, which is a fair trade for
/// not re-reading the file on every keystroke of the search query.
struct FuzzyHistoryCompleter {
    /// Most recent first, no duplicate command lines.
    entries: Vec<String>,
    matcher: fuzzy_matcher::skim::SkimMatcherV2,
}

impl FuzzyHistoryCompleter {
    fn new(history_path: Option<&Path>) -> FuzzyHistoryCompleter {
        let mut entries = history_path.map(read_history_entries).unwrap_or_default();
        entries.reverse();
        let mut seen = HashSet::new();
        entries.retain(|e| seen.insert(e.clone()));
        FuzzyHistoryCompleter {
            entries,
            matcher: fuzzy_matcher::skim::SkimMatcherV2::default(),
        }
    }
}

/// How many rows the fuzzy-history menu shows at once — enough to be
/// useful without turning into a full-screen takeover.
const FUZZY_HISTORY_LIMIT: usize = 50;

impl Completer for FuzzyHistoryCompleter {
    fn complete(&mut self, line: &str, pos: usize) -> Vec<Suggestion> {
        use fuzzy_matcher::FuzzyMatcher;

        let query = &line[..pos];
        let to_suggestion = |value: &str| Suggestion {
            value: value.to_string(),
            // The menu is configured with `OutputMode::FullBuffer` (see
            // `build_engine`), which replaces the whole line on accept
            // regardless of this span — the exact range here doesn't
            // matter, it just needs to be valid.
            span: Span::new(0, pos),
            append_whitespace: false,
            ..Default::default()
        };

        if query.is_empty() {
            return self
                .entries
                .iter()
                .take(FUZZY_HISTORY_LIMIT)
                .map(|e| to_suggestion(e))
                .collect();
        }

        let mut scored: Vec<(i64, &String)> = self
            .entries
            .iter()
            .filter_map(|entry| {
                self.matcher
                    .fuzzy_match(entry, query)
                    .map(|score| (score, entry))
            })
            .collect();
        scored.sort_by(|a, b| b.0.cmp(&a.0));
        scored.truncate(FUZZY_HISTORY_LIMIT);
        scored.into_iter().map(|(_, e)| to_suggestion(e)).collect()
    }
}

/// Builds reedline's completion menu, bound to Tab. Kept as its own
/// function since both edit modes (emacs/vi insert) need the same
/// keybinding wired in.
fn add_completion_keybinding(kb: &mut Keybindings) {
    kb.add_binding(
        KeyModifiers::NONE,
        KeyCode::Tab,
        ReedlineEvent::UntilFound(vec![
            ReedlineEvent::Menu("completion_menu".to_string()),
            ReedlineEvent::MenuNext,
        ]),
    );
}

/// Rebinds Ctrl+R from reedline's default `SearchHistory` (an incremental
/// *substring* reverse-i-search) to the fuzzy history menu instead — the
/// fzf convention, and the one this shell's target audience is most
/// likely already muscle-memory'd to.
fn add_fuzzy_history_keybinding(kb: &mut Keybindings) {
    kb.add_binding(
        KeyModifiers::CONTROL,
        KeyCode::Char('r'),
        ReedlineEvent::UntilFound(vec![
            ReedlineEvent::Menu("fuzzy_history_menu".to_string()),
            ReedlineEvent::MenuNext,
        ]),
    );
}

/// hinter, persistent history, and emacs/vi edit mode driven by
/// keyboard.calm. `known_commands` should already include $PATH
/// executables plus alias/function names (see `known_commands` below).
pub fn build_engine(theme: &Theme, cfg: &CalmDocument, known: HashSet<String>) -> Reedline {
    let mut engine = Reedline::create();

    let history_path = config::history_dir().ok().map(|d| d.join("reedline_history.txt"));
    if let Some(path) = history_path.clone() {
        // `size` (in-memory/session history) and `save` (how much gets
        // persisted to disk) are two separate knobs in the zsh layout this
        // config format is modeled on, but reedline's FileBackedHistory
        // only exposes one capacity for both. Rather than silently drop
        // `save` on the floor, take whichever of the two is larger — a
        // user who bumped `save` expecting more persisted history gets it.
        let capacity = [
            cfg.get("history", "size").and_then(|v| v.as_int()),
            cfg.get("history", "save").and_then(|v| v.as_int()),
        ]
        .into_iter()
        .flatten()
        .filter(|n| *n > 0)
        .max()
        .unwrap_or(10_000) as usize;
        // If the history file can't be created (e.g. permissions), the
        // shell still runs — just without persisted history for this
        // session, rather than failing to start.
        if let Ok(history) = FileBackedHistory::with_file(capacity, path) {
            engine = engine.with_history(Box::new(history));
        }
    }

    let hint_style = Style::new().italic().fg(theme_color(theme, "cloud_gray"));
    engine = engine.with_hinter(Box::new(DefaultHinter::default().with_style(hint_style)));
    engine = engine.with_highlighter(Box::new(CalmHighlighter::new(theme, known.clone())));

    engine = engine.with_completer(Box::new(CalmCompleter::new(&known)));
    let completion_menu = ColumnarMenu::default()
        .with_name("completion_menu")
        .with_selected_text_style(Style::new().fg(theme_color(theme, "calm_purple")).bold().reverse())
        .with_text_style(Style::new().fg(theme_color(theme, "cloud_gray")))
        .with_match_text_style(Style::new().fg(theme_color(theme, "warm_yellow")))
        .with_selected_match_text_style(Style::new().fg(theme_color(theme, "warm_yellow")).bold().reverse())
        .with_description_text_style(Style::new().fg(theme_color(theme, "soft_blue")));
    engine = engine.with_menu(ReedlineMenu::EngineCompleter(Box::new(completion_menu)));

    // Ctrl+R: fuzzy history search (fzf's convention, not reedline's
    // built-in substring reverse-i-search) via its own independent
    // Completer/Menu pair — `ReedlineMenu::WithCompleter` exists
    // specifically so a menu doesn't have to share the engine's main
    // (command/path) completer.
    let fuzzy_completer: Box<dyn Completer + Send> =
        Box::new(FuzzyHistoryCompleter::new(history_path.as_deref()));
    let fuzzy_menu = ListMenu::default()
        .with_name("fuzzy_history_menu")
        .with_output_mode(OutputMode::FullBuffer)
        .with_selected_text_style(Style::new().fg(theme_color(theme, "calm_purple")).bold().reverse())
        .with_text_style(Style::new().fg(theme_color(theme, "cloud_gray")))
        .with_match_text_style(Style::new().fg(theme_color(theme, "warm_yellow")))
        .with_selected_match_text_style(Style::new().fg(theme_color(theme, "warm_yellow")).bold().reverse());
    engine = engine.with_menu(ReedlineMenu::WithCompleter {
        menu: Box::new(fuzzy_menu),
        completer: fuzzy_completer,
    });

    let vi_mode = cfg.get_str("keyboard", "edit_mode") == Some("vi");
    let edit_mode: Box<dyn reedline::EditMode> = if vi_mode {
        let mut insert_kb = default_vi_insert_keybindings();
        add_completion_keybinding(&mut insert_kb);
        add_fuzzy_history_keybinding(&mut insert_kb);
        Box::new(Vi::new(insert_kb, default_vi_normal_keybindings()))
    } else {
        let mut kb = default_emacs_keybindings();
        add_completion_keybinding(&mut kb);
        add_fuzzy_history_keybinding(&mut kb);
        Box::new(Emacs::new(kb))
    };
    engine.with_edit_mode(edit_mode)
}

/// Builds the known-command set the highlighter checks against: $PATH
/// executables plus every alias/function name from config, computed once
/// at startup rather than touching the filesystem per keystroke.
pub fn known_commands(cfg: &CalmDocument) -> HashSet<String> {
    let mut known = scan_path_commands();
    for section_name in ["aliases", "directory.aliases", "functions"] {
        if let Some(section) = cfg.section(section_name) {
            known.extend(section.keys().cloned());
        }
    }
    known
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::sync::{Mutex, OnceLock};

    // `complete_paths` reads the process-wide current directory, and
    // cargo runs tests in parallel threads within the same process — so
    // any test that calls `set_current_dir` has to be serialized against
    // the others, or they'll intermittently clobber each other's cwd.
    fn cwd_test_lock() -> &'static Mutex<()> {
        static LOCK: OnceLock<Mutex<()>> = OnceLock::new();
        LOCK.get_or_init(|| Mutex::new(()))
    }

    fn completer(commands: &[&str]) -> CalmCompleter {
        let known: HashSet<String> = commands.iter().map(|s| s.to_string()).collect();
        CalmCompleter::new(&known)
    }

    #[test]
    fn completes_first_word_against_known_commands() {
        let mut c = completer(&["git", "grep", "go"]);
        let mut names: Vec<String> = c.complete("g", 1).into_iter().map(|s| s.value).collect();
        names.sort();
        assert_eq!(names, vec!["git", "go", "grep"]);
    }

    #[test]
    fn first_word_completion_includes_builtins() {
        let mut c = completer(&[]);
        let names: Vec<String> = c.complete("pu", 2).into_iter().map(|s| s.value).collect();
        assert_eq!(names, vec!["pushd"]);
    }

    #[test]
    fn empty_prefix_does_not_panic_and_returns_matches() {
        let mut c = completer(&["ls"]);
        let names: Vec<String> = c.complete("", 0).into_iter().map(|s| s.value).collect();
        assert!(names.contains(&"ls".to_string()));
        assert!(names.contains(&"cd".to_string()));
    }

    #[test]
    fn second_word_completes_filesystem_paths_not_commands() {
        let _guard = cwd_test_lock().lock().unwrap();
        let dir = std::env::temp_dir().join(format!("calm-completer-test-{}", std::process::id()));
        fs::create_dir_all(dir.join("subdir")).unwrap();
        fs::write(dir.join("readme.txt"), "").unwrap();
        let prev = std::env::current_dir().unwrap();
        std::env::set_current_dir(&dir).unwrap();

        let mut c = completer(&["git"]); // "git" must NOT show up for path completion
        let line = "cat re";
        let suggestions = c.complete(line, line.len());
        let values: Vec<String> = suggestions.iter().map(|s| s.value.clone()).collect();

        std::env::set_current_dir(prev).ok();
        fs::remove_dir_all(&dir).ok();

        assert!(values.contains(&"readme.txt".to_string()));
        assert!(!values.contains(&"git".to_string()));
    }

    #[test]
    fn directory_suggestions_get_trailing_slash_and_no_whitespace() {
        let _guard = cwd_test_lock().lock().unwrap();
        let dir = std::env::temp_dir().join(format!("calm-completer-test-dir-{}", std::process::id()));
        fs::create_dir_all(dir.join("nested")).unwrap();
        let prev = std::env::current_dir().unwrap();
        std::env::set_current_dir(&dir).unwrap();

        let mut c = completer(&[]);
        let line = "cd ne";
        let suggestions = c.complete(line, line.len());

        std::env::set_current_dir(prev).ok();
        fs::remove_dir_all(&dir).ok();

        assert_eq!(suggestions.len(), 1);
        assert_eq!(suggestions[0].value, "nested/");
        assert!(!suggestions[0].append_whitespace);
    }

    #[test]
    fn dotfiles_are_hidden_unless_prefix_starts_with_dot() {
        let _guard = cwd_test_lock().lock().unwrap();
        let dir = std::env::temp_dir().join(format!("calm-completer-test-dot-{}", std::process::id()));
        fs::create_dir_all(&dir).unwrap();
        fs::write(dir.join(".hidden"), "").unwrap();
        fs::write(dir.join("visible"), "").unwrap();
        let prev = std::env::current_dir().unwrap();
        std::env::set_current_dir(&dir).unwrap();

        let mut c = completer(&[]);
        let without_dot = c.complete("ls ", 3);
        let with_dot = c.complete("ls .", 4);

        std::env::set_current_dir(prev).ok();
        fs::remove_dir_all(&dir).ok();

        assert!(without_dot.iter().any(|s| s.value == "visible"));
        assert!(!without_dot.iter().any(|s| s.value == ".hidden"));
        assert!(with_dot.iter().any(|s| s.value == ".hidden"));
    }

    fn write_history_file(lines: &[&str]) -> PathBuf {
        let path = std::env::temp_dir().join(format!(
            "calm-history-test-{}-{}.txt",
            std::process::id(),
            lines.len()
        ));
        fs::write(&path, lines.join("\n") + "\n").unwrap();
        path
    }

    #[test]
    fn fuzzy_history_empty_query_returns_recent_first() {
        let path = write_history_file(&["git status", "cd Projects", "ls -la"]);
        let mut c = FuzzyHistoryCompleter::new(Some(&path));
        fs::remove_file(&path).ok();

        let values: Vec<String> = c.complete("", 0).into_iter().map(|s| s.value).collect();
        // Most recent entry (last line in the file) comes first.
        assert_eq!(values, vec!["ls -la", "cd Projects", "git status"]);
    }

    #[test]
    fn fuzzy_history_matches_non_contiguous_subsequence() {
        let path = write_history_file(&["docker compose up --build"]);
        let mut c = FuzzyHistoryCompleter::new(Some(&path));
        fs::remove_file(&path).ok();

        // "dcu" is a subsequence of "docker compose up", not a substring —
        // exactly the case reedline's built-in substring search can't do.
        let line = "dcu";
        let values: Vec<String> = c
            .complete(line, line.len())
            .into_iter()
            .map(|s| s.value)
            .collect();
        assert_eq!(values, vec!["docker compose up --build"]);
    }

    #[test]
    fn fuzzy_history_deduplicates_repeated_commands() {
        let path = write_history_file(&["ls", "cd ..", "ls"]);
        let c = FuzzyHistoryCompleter::new(Some(&path));
        fs::remove_file(&path).ok();

        assert_eq!(c.entries, vec!["ls".to_string(), "cd ..".to_string()]);
    }

    #[test]
    fn fuzzy_history_missing_file_yields_no_entries_not_a_panic() {
        let missing = std::env::temp_dir().join("calm-history-does-not-exist.txt");
        let mut c = FuzzyHistoryCompleter::new(Some(&missing));
        assert_eq!(c.complete("anything", 8), Vec::new());
    }
}
