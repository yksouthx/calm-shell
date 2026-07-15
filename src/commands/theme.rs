use crate::config;
use anyhow::{bail, Context, Result};
use colored::Colorize;
use serde_json::Value;
use std::fs;

fn active_theme_marker_path() -> Result<std::path::PathBuf> {
    Ok(config::config_dir()?.join(".active_theme"))
}

pub fn current_theme() -> Result<String> {
    let marker = active_theme_marker_path()?;
    if marker.exists() {
        Ok(fs::read_to_string(marker)?.trim().to_string())
    } else {
        Ok("calm-lavender".to_string())
    }
}

pub fn list() -> Result<()> {
    config::ensure_scaffold()?;
    let dir = config::themes_dir()?;
    let active = current_theme()?;

    let mut found = false;
    for entry in fs::read_dir(&dir).with_context(|| format!("reading {}", dir.display()))? {
        let entry = entry?;
        let path = entry.path();
        if path.extension().and_then(|e| e.to_str()) != Some("json") {
            continue;
        }
        found = true;

        let raw = fs::read_to_string(&path)?;
        let parsed: Value = serde_json::from_str(&raw).unwrap_or(Value::Null);
        let name = parsed
            .get("name")
            .and_then(|v| v.as_str())
            .unwrap_or_else(|| path.file_stem().and_then(|s| s.to_str()).unwrap_or("?"))
            .to_string();
        let display = parsed
            .get("display_name")
            .and_then(|v| v.as_str())
            .unwrap_or(&name)
            .to_string();

        if name == active {
            println!("  {} {}  ({})", "●".truecolor(203, 166, 247), display.bold(), name);
        } else {
            println!("  {} {}  ({})", "○".dimmed(), display, name);
        }
    }

    if !found {
        println!("{}", "No themes installed.".truecolor(243, 139, 168));
    }

    Ok(())
}

pub fn set(name: &str) -> Result<()> {
    config::ensure_scaffold()?;
    let theme_file = config::themes_dir()?.join(format!("{name}.json"));
    if !theme_file.exists() {
        bail!(
            "theme '{name}' not found in {}. Run `calm theme list` to see installed themes.",
            config::themes_dir()?.display()
        );
    }

    // Persist the active theme marker.
    fs::write(active_theme_marker_path()?, name)?;

    // Keep `theme = "<name>"` under [shell] in sync inside config.calm.
    let config_path = config::config_file()?;
    let existing = fs::read_to_string(&config_path).unwrap_or_default();
    let mut found = false;
    let mut lines: Vec<String> = existing
        .lines()
        .map(|l| {
            if l.trim_start().starts_with("theme") && l.contains('=') && !found {
                found = true;
                format!("theme = \"{name}\"")
            } else {
                l.to_string()
            }
        })
        .collect();
    if !found {
        if !lines.iter().any(|l| l.trim() == "[shell]") {
            lines.push("[shell]".to_string());
        }
        lines.push(format!("theme = \"{name}\""));
    }
    fs::write(&config_path, lines.join("\n") + "\n")?;

    println!(
        "{} Theme set to {}",
        "✓".truecolor(166, 227, 161),
        name.truecolor(203, 166, 247).bold()
    );
    Ok(())
}
