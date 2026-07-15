//! Wires reedline in as calm's line editor.
//!
//! Replaces the plain `io::stdin().read_line()` loop with a real line
//! editor: theme-driven multi-line prompt, syntax highlighting (command
//! word / strings / flags), fish-style history autosuggestions (reedline's
//! own `DefaultHinter`, which is literally built for this), persistent
//! history with Up/Down + Ctrl+R search. keyboard.calm's `edit_mode`
//! actually does something now too (emacs vs vi keybindings).
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
    DefaultHinter, Emacs, FileBackedHistory, Highlighter, Prompt, PromptEditMode,
    PromptHistorySearch, Reedline, StyledText, Vi,
};
use std::borrow::Cow;
use std::collections::HashSet;
use std::env;

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
    fn render_prompt_left(&self) -> Cow<str> {
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

    fn render_prompt_right(&self) -> Cow<str> {
        Cow::Borrowed("")
    }

    fn render_prompt_indicator(&self, _prompt_mode: PromptEditMode) -> Cow<str> {
        Cow::Borrowed("")
    }

    fn render_prompt_multiline_indicator(&self) -> Cow<str> {
        Cow::Borrowed(":::  ")
    }

    fn render_prompt_history_search_indicator(&self, _history_search: PromptHistorySearch) -> Cow<str> {
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

/// Builds the full Reedline engine: highlighter, fish-style history
/// hinter, persistent history, and emacs/vi edit mode driven by
/// keyboard.calm. `known_commands` should already include $PATH
/// executables plus alias/function names (see `known_commands` below).
pub fn build_engine(theme: &Theme, cfg: &CalmDocument, known: HashSet<String>) -> Reedline {
    let mut engine = Reedline::create();

    let history_path = config::history_dir().ok().map(|d| d.join("reedline_history.txt"));
    if let Some(path) = history_path {
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
    engine = engine.with_highlighter(Box::new(CalmHighlighter::new(theme, known)));

    let vi_mode = cfg.get_str("keyboard", "edit_mode") == Some("vi");
    let edit_mode: Box<dyn reedline::EditMode> = if vi_mode {
        Box::new(Vi::new(default_vi_insert_keybindings(), default_vi_normal_keybindings()))
    } else {
        Box::new(Emacs::new(default_emacs_keybindings()))
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
