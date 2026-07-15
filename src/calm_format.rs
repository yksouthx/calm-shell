//! The `.calm` config format.
//!
//! Design goals: read like Fish config, no build-a-config-language ambition.
//! Sections group related keys, values are typed (string / bool / int / list),
//! and triple-quoted strings let `functions.calm` hold real shell bodies
//! without the parser needing to understand shell syntax.
//!
//! ```calm
//! # comment
//! [shell]
//! theme = "calm-lavender"
//! greeting = true
//! retries = 3
//! tags = ["fast", "quiet"]
//!
//! [functions]
//! mkcd = """
//! mkdir -p "$1" && cd "$1"
//! """
//!
//! include "aliases.calm"
//! ```

use anyhow::{bail, Context, Result};
use std::collections::HashMap;
use std::env;
use std::fs;
use std::path::Path;

#[derive(Debug, Clone, PartialEq)]
pub enum CalmValue {
    Str(String),
    Bool(bool),
    Int(i64),
    List(Vec<CalmValue>),
}

impl CalmValue {
    pub fn as_str(&self) -> Option<&str> {
        match self {
            CalmValue::Str(s) => Some(s),
            _ => None,
        }
    }
    pub fn as_bool(&self) -> Option<bool> {
        match self {
            CalmValue::Bool(b) => Some(*b),
            _ => None,
        }
    }
    pub fn as_int(&self) -> Option<i64> {
        match self {
            CalmValue::Int(i) => Some(*i),
            _ => None,
        }
    }
    /// List-typed values (e.g. `tags = ["fast", "quiet"]`) parse correctly
    /// but nothing in calm-shell reads one back out yet — no feature
    /// currently consumes a list config value. Kept (not deleted) because
    /// removing list parsing would silently break any `.calm` file a user
    /// has already written with one; `#[allow(dead_code)]` documents the
    /// gap instead of hiding it behind a fabricated caller.
    #[allow(dead_code)]
    pub fn as_list(&self) -> Option<&[CalmValue]> {
        match self {
            CalmValue::List(items) => Some(items),
            _ => None,
        }
    }
}

#[derive(Debug, Default)]
pub struct CalmDocument {
    pub sections: HashMap<String, HashMap<String, CalmValue>>,
}

impl CalmDocument {
    pub fn section(&self, name: &str) -> Option<&HashMap<String, CalmValue>> {
        self.sections.get(name)
    }

    pub fn get(&self, section: &str, key: &str) -> Option<&CalmValue> {
        self.sections.get(section)?.get(key)
    }

    pub fn get_str(&self, section: &str, key: &str) -> Option<&str> {
        self.get(section, key)?.as_str()
    }

    pub fn get_bool(&self, section: &str, key: &str) -> Option<bool> {
        self.get(section, key)?.as_bool()
    }

    fn merge(&mut self, other: CalmDocument) {
        for (section, kvs) in other.sections {
            let entry = self.sections.entry(section).or_default();
            for (k, v) in kvs {
                entry.insert(k, v);
            }
        }
    }
}

/// Parses a `.calm` file, following `include "..."` directives (resolved
/// relative to the including file's directory). Include cycles/depth are
/// bounded to avoid infinite recursion from a misconfigured file.
pub fn parse_file(path: &Path) -> Result<CalmDocument> {
    parse_file_inner(path, 0)
}

fn parse_file_inner(path: &Path, depth: u8) -> Result<CalmDocument> {
    if depth > 8 {
        bail!("include depth exceeded 8 while parsing {}", path.display());
    }
    let raw = fs::read_to_string(path)
        .with_context(|| format!("reading calm config {}", path.display()))?;

    let mut doc = parse(&raw).with_context(|| format!("parsing {}", path.display()))?;

    for include_path in extract_includes(&raw) {
        let resolved = path
            .parent()
            .unwrap_or_else(|| Path::new("."))
            .join(&include_path);
        let included = parse_file_inner(&resolved, depth + 1)
            .with_context(|| format!("including {} from {}", resolved.display(), path.display()))?;
        doc.merge(included);
    }

    Ok(doc)
}

fn extract_includes(raw: &str) -> Vec<String> {
    raw.lines()
        .filter_map(|line| {
            let line = line.trim();
            let rest = line.strip_prefix("include ")?;
            let rest = rest.trim();
            let unquoted = rest.strip_prefix('"')?.strip_suffix('"')?;
            Some(unquoted.to_string())
        })
        .collect()
}

/// Parses `.calm` source text (without resolving includes — use
/// `parse_file` for that).
pub fn parse(input: &str) -> Result<CalmDocument> {
    let mut doc = CalmDocument::default();
    let mut current_section = String::new();
    doc.sections.entry(current_section.clone()).or_default();

    let lines: Vec<&str> = input.lines().collect();
    let mut i = 0;
    while i < lines.len() {
        let raw_line = lines[i];
        let line = raw_line.trim();

        if line.is_empty() || line.starts_with('#') || line.starts_with("include ") {
            i += 1;
            continue;
        }

        if let Some(name) = line.strip_prefix('[').and_then(|s| s.strip_suffix(']')) {
            current_section = name.trim().to_string();
            doc.sections.entry(current_section.clone()).or_default();
            i += 1;
            continue;
        }

        let (key, value_part) = line
            .split_once('=')
            .with_context(|| format!("line {}: expected `key = value`, got: {line}", i + 1))?;
        let key = key.trim().to_string();
        let value_part = value_part.trim();

        if value_part.starts_with("\"\"\"") {
            // Multiline block: collect lines until a line that is exactly `"""`
            // (or the same line closes it, for single-line triple-quoted values).
            let after_open = &value_part[3..];
            if let Some(closed) = after_open.strip_suffix("\"\"\"") {
                doc.sections
                    .entry(current_section.clone())
                    .or_default()
                    .insert(key, CalmValue::Str(closed.to_string()));
                i += 1;
                continue;
            }

            let mut body = Vec::new();
            if !after_open.is_empty() {
                body.push(after_open.to_string());
            }
            i += 1;
            let mut closed = false;
            while i < lines.len() {
                if lines[i].trim_end() == "\"\"\"" {
                    closed = true;
                    i += 1;
                    break;
                }
                body.push(lines[i].to_string());
                i += 1;
            }
            if !closed {
                bail!("unterminated triple-quoted block for key `{key}` starting near line {}", i);
            }
            doc.sections
                .entry(current_section.clone())
                .or_default()
                .insert(key, CalmValue::Str(body.join("\n")));
            continue;
        }

        let value = parse_scalar(value_part)
            .with_context(|| format!("line {}: could not parse value `{value_part}`", i + 1))?;
        doc.sections.entry(current_section.clone()).or_default().insert(key, value);
        i += 1;
    }

    Ok(doc)
}

fn parse_scalar(raw: &str) -> Result<CalmValue> {
    let raw = raw.trim();

    if let Some(inner) = raw.strip_prefix('"').and_then(|s| s.strip_suffix('"')) {
        return Ok(CalmValue::Str(unescape(inner)));
    }
    if raw == "true" {
        return Ok(CalmValue::Bool(true));
    }
    if raw == "false" {
        return Ok(CalmValue::Bool(false));
    }
    if let Ok(n) = raw.parse::<i64>() {
        return Ok(CalmValue::Int(n));
    }
    if let Some(inner) = raw.strip_prefix('[').and_then(|s| s.strip_suffix(']')) {
        let items = split_top_level_commas(inner)
            .into_iter()
            .filter(|s| !s.trim().is_empty())
            .map(|item| parse_scalar(item.trim()))
            .collect::<Result<Vec<_>>>()?;
        return Ok(CalmValue::List(items));
    }
    if raw.is_empty() {
        bail!("empty value");
    }
    // Bare, unquoted word — accept as a plain string for ergonomics
    // (e.g. `theme = calm-lavender` without quotes).
    Ok(CalmValue::Str(raw.to_string()))
}

fn split_top_level_commas(s: &str) -> Vec<String> {
    let mut items = Vec::new();
    let mut depth = 0i32;
    let mut in_quotes = false;
    let mut current = String::new();
    for c in s.chars() {
        match c {
            '"' => {
                in_quotes = !in_quotes;
                current.push(c);
            }
            '[' if !in_quotes => {
                depth += 1;
                current.push(c);
            }
            ']' if !in_quotes => {
                depth -= 1;
                current.push(c);
            }
            ',' if !in_quotes && depth == 0 => {
                items.push(current.clone());
                current.clear();
            }
            _ => current.push(c),
        }
    }
    if !current.trim().is_empty() {
        items.push(current);
    }
    items
}

fn unescape(s: &str) -> String {
    s.replace("\\\"", "\"").replace("\\\\", "\\")
}

/// Expands a leading `~` to $HOME and `$VAR` / `${VAR}` references anywhere
/// in the string. Intended for values pulled out of a parsed CalmDocument
/// that represent paths or commands (e.g. alias targets).
pub fn expand(value: &str) -> String {
    let mut out = String::new();
    let mut chars = value.chars().peekable();

    if value.starts_with('~') {
        if let Some(home) = dirs::home_dir() {
            out.push_str(&home.display().to_string());
        }
        chars.next();
    }

    while let Some(c) = chars.next() {
        if c == '$' {
            if chars.peek() == Some(&'{') {
                chars.next();
                let mut name = String::new();
                for nc in chars.by_ref() {
                    if nc == '}' {
                        break;
                    }
                    name.push(nc);
                }
                out.push_str(&env::var(&name).unwrap_or_default());
            } else {
                let mut name = String::new();
                while let Some(&nc) = chars.peek() {
                    if nc.is_alphanumeric() || nc == '_' {
                        name.push(nc);
                        chars.next();
                    } else {
                        break;
                    }
                }
                if name.is_empty() {
                    out.push('$');
                } else {
                    out.push_str(&env::var(&name).unwrap_or_default());
                }
            }
        } else {
            out.push(c);
        }
    }

    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    #[test]
    fn parses_sections_and_scalars() {
        let doc = parse(
            r#"
            [shell]
            theme = "calm-lavender"
            greeting = true
            retries = 3
            "#,
        )
        .unwrap();
        assert_eq!(doc.get_str("shell", "theme"), Some("calm-lavender"));
        assert_eq!(doc.get_bool("shell", "greeting"), Some(true));
        assert_eq!(doc.get("shell", "retries").and_then(|v| v.as_int()), Some(3));
    }

    #[test]
    fn parses_lists() {
        let doc = parse(r#"tags = ["fast", "quiet"]"#).unwrap();
        let list = doc.get("", "tags").unwrap().as_list().unwrap();
        assert_eq!(list, &[CalmValue::Str("fast".into()), CalmValue::Str("quiet".into())]);
    }

    #[test]
    fn bare_unquoted_word_is_a_string() {
        // Documented ergonomic exception: `theme = calm-lavender` without
        // quotes should still parse, matching config.rs's own comment.
        let doc = parse("theme = calm-lavender").unwrap();
        assert_eq!(doc.get_str("", "theme"), Some("calm-lavender"));
    }

    #[test]
    fn comments_and_blank_lines_are_skipped() {
        let doc = parse("# a comment\n\n[shell]\n# another\ngreeting = true\n").unwrap();
        assert_eq!(doc.get_bool("shell", "greeting"), Some(true));
    }

    #[test]
    fn multiline_triple_quoted_block() {
        let doc = parse(
            "[functions]\nmkcd = \"\"\"\nmkdir -p \"$1\" && cd \"$1\"\n\"\"\"\n",
        )
        .unwrap();
        assert_eq!(
            doc.get_str("functions", "mkcd"),
            Some("mkdir -p \"$1\" && cd \"$1\"")
        );
    }

    #[test]
    fn single_line_triple_quoted_block() {
        let doc = parse("greeting = \"\"\"hello\"\"\"\n").unwrap();
        assert_eq!(doc.get_str("", "greeting"), Some("hello"));
    }

    #[test]
    fn unterminated_triple_quote_errors() {
        let result = parse("body = \"\"\"\nunterminated\n");
        assert!(result.is_err());
    }

    #[test]
    fn later_section_wins_on_duplicate_key() {
        let doc = parse("[a]\nx = 1\n[a]\nx = 2\n").unwrap();
        assert_eq!(doc.get("a", "x").and_then(|v| v.as_int()), Some(2));
    }

    #[test]
    fn malformed_line_without_equals_errors() {
        assert!(parse("[shell]\nthis is not valid").is_err());
    }

    #[test]
    fn include_resolves_relative_to_including_file_and_merges() {
        let dir = std::env::temp_dir().join(format!("calm-shell-test-{}", std::process::id()));
        fs::create_dir_all(&dir).unwrap();
        let main = dir.join("main.calm");
        let included = dir.join("aliases.calm");
        fs::write(&included, "[aliases]\nll = \"ls -la\"\n").unwrap();
        fs::write(&main, "[shell]\ntheme = \"calm-lavender\"\ninclude \"aliases.calm\"\n").unwrap();

        let doc = parse_file(&main).unwrap();
        assert_eq!(doc.get_str("shell", "theme"), Some("calm-lavender"));
        assert_eq!(doc.get_str("aliases", "ll"), Some("ls -la"));

        fs::remove_dir_all(&dir).ok();
    }

    #[test]
    fn include_cycle_is_bounded_not_infinite() {
        let dir = std::env::temp_dir().join(format!("calm-shell-test-cycle-{}", std::process::id()));
        fs::create_dir_all(&dir).unwrap();
        let a = dir.join("a.calm");
        let b = dir.join("b.calm");
        fs::write(&a, "include \"b.calm\"\n").unwrap();
        fs::write(&b, "include \"a.calm\"\n").unwrap();

        // Must return an error (depth exceeded), not hang or stack-overflow.
        let result = parse_file(&a);
        assert!(result.is_err());

        fs::remove_dir_all(&dir).ok();
    }

    #[test]
    fn expand_handles_home_and_env_vars() {
        std::env::set_var("CALM_TEST_VAR", "value");
        assert_eq!(expand("$CALM_TEST_VAR"), "value");
        assert_eq!(expand("${CALM_TEST_VAR}"), "value");
        assert_eq!(expand("no vars here"), "no vars here");
        assert_eq!(expand("$"), "$");
    }

    #[test]
    fn expand_tilde_prefixes_home() {
        if let Some(home) = dirs::home_dir() {
            let expanded = expand("~/Projects");
            assert_eq!(expanded, format!("{}/Projects", home.display()));
        }
    }

    #[test]
    fn split_top_level_commas_respects_nesting_and_quotes() {
        let items = split_top_level_commas(r#""a,b", [1, 2], 3"#);
        assert_eq!(items, vec![r#""a,b""#, " [1, 2]", " 3"]);
    }
}
