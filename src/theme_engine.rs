use crate::commands::theme::current_theme;
use crate::config;
use anyhow::{Context, Result};
use colored::{Color, ColoredString, Colorize};
use serde::Deserialize;
use std::collections::HashMap;
use std::fs;

#[derive(Debug, Deserialize)]
pub struct ThemeFile {
    pub name: String,
    pub display_name: String,
    pub colors: HashMap<String, String>,
    pub prompt: PromptTheme,
}

#[derive(Debug, Deserialize)]
pub struct PromptTheme {
    pub icon: String,
    pub border_color: String,
    pub path_color: String,
    pub git_clean_color: String,
    pub git_dirty_color: String,
    pub arrow_color: String,
}

pub struct Theme {
    file: ThemeFile,
    palette: HashMap<String, Color>,
}

impl Theme {
    /// Loads whichever theme is currently active (falls back to
    /// calm-lavender if none is set or the active one is missing/corrupt).
    pub fn load_active() -> Result<Theme> {
        let name = current_theme().unwrap_or_else(|_| "calm-lavender".to_string());
        Theme::load(&name).or_else(|_| Theme::load("calm-lavender"))
    }

    pub fn load(name: &str) -> Result<Theme> {
        let path = config::themes_dir()?.join(format!("{name}.calm"));
        let raw = fs::read_to_string(&path)
            .with_context(|| format!("reading theme file {}", path.display()))?;
        let file: ThemeFile = serde_json::from_str(&raw)
            .with_context(|| format!("parsing theme file {}", path.display()))?;

        let mut palette = HashMap::new();
        for (key, hex) in &file.colors {
            if let Some(rgb) = parse_hex(hex) {
                palette.insert(key.clone(), Color::TrueColor { r: rgb.0, g: rgb.1, b: rgb.2 });
            }
        }

        Ok(Theme { file, palette })
    }

    pub fn display_name(&self) -> &str {
        &self.file.display_name
    }

    pub fn icon(&self) -> &str {
        &self.file.prompt.icon
    }

    /// Maps a semantic prompt-role key (e.g. "border", "arrow") to the
    /// palette color name it's configured to use, or passes through
    /// direct palette keys (e.g. "calm_purple") unchanged.
    fn resolve_color_name<'a>(&'a self, semantic_key: &'a str) -> &'a str {
        match semantic_key {
            "border" => &self.file.prompt.border_color,
            "path" => &self.file.prompt.path_color,
            "git_clean" => &self.file.prompt.git_clean_color,
            "git_dirty" => &self.file.prompt.git_dirty_color,
            "arrow" => &self.file.prompt.arrow_color,
            other => other,
        }
    }

    /// Colors `text` using the palette color referenced by semantic key
    /// (e.g. "border_color" -> looks up file.prompt.border_color -> palette).
    /// Falls back to plain, uncolored text if the key or color is missing.
    pub fn paint(&self, semantic_key: &str, text: &str) -> ColoredString {
        match self.palette.get(self.resolve_color_name(semantic_key)) {
            Some(color) => text.color(*color),
            None => text.normal(),
        }
    }

    /// Same resolution as `paint`, but returns the raw (r, g, b) tuple
    /// instead of a `colored`-crate-styled string. For consumers that use
    /// a different color type (reedline/nu-ansi-term for the line editor).
    pub fn rgb(&self, semantic_key: &str) -> Option<(u8, u8, u8)> {
        match self.palette.get(self.resolve_color_name(semantic_key)) {
            Some(Color::TrueColor { r, g, b }) => Some((*r, *g, *b)),
            _ => None,
        }
    }
}

fn parse_hex(hex: &str) -> Option<(u8, u8, u8)> {
    let hex = hex.trim_start_matches('#');
    // Guard against multi-byte UTF-8 before slicing by byte index: a
    // hand-edited theme.json could contain a stray non-ASCII character
    // (smart quote, emoji, stray unicode) that happens to leave the
    // string at 6 *bytes* while misaligning char boundaries, which would
    // otherwise panic on `&hex[0..2]`.
    if hex.len() != 6 || !hex.is_ascii() {
        return None;
    }
    let r = u8::from_str_radix(&hex[0..2], 16).ok()?;
    let g = u8::from_str_radix(&hex[2..4], 16).ok()?;
    let b = u8::from_str_radix(&hex[4..6], 16).ok()?;
    Some((r, g, b))
}
