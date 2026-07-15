use anyhow::{Context, Result};
use std::fs;
use std::path::PathBuf;

/// Root config dir: ~/.config/calm-shell/
pub fn config_dir() -> Result<PathBuf> {
    let base = dirs::config_dir().context("could not resolve $HOME/.config")?;
    Ok(base.join("calm-shell"))
}

pub fn themes_dir() -> Result<PathBuf> {
    Ok(config_dir()?.join("themes"))
}

pub fn plugins_dir() -> Result<PathBuf> {
    Ok(config_dir()?.join("plugins"))
}

pub fn history_dir() -> Result<PathBuf> {
    Ok(config_dir()?.join("history"))
}

pub fn config_file() -> Result<PathBuf> {
    Ok(config_dir()?.join("config.calm"))
}

pub fn aliases_file() -> Result<PathBuf> {
    Ok(config_dir()?.join("aliases.calm"))
}

pub fn functions_file() -> Result<PathBuf> {
    Ok(config_dir()?.join("functions.calm"))
}

// The topic-file split below mirrors the classic oh-my-zsh layout
// (github.com/mattjj/my-oh-my-zsh: environment.zsh, directory.zsh,
// history.zsh, keyboard.zsh, terminal.zsh, ...) — one small, focused
// .calm file per concern instead of one growing monolith. Their
// spectrum.zsh (256-color palette definitions) is the one topic that
// doesn't get a file here, since that job is already done by
// themes/*.json + the theme engine.

pub fn environment_file() -> Result<PathBuf> {
    Ok(config_dir()?.join("environment.calm"))
}

pub fn directory_file() -> Result<PathBuf> {
    Ok(config_dir()?.join("directory.calm"))
}

pub fn history_settings_file() -> Result<PathBuf> {
    Ok(config_dir()?.join("history.calm"))
}

pub fn keyboard_file() -> Result<PathBuf> {
    Ok(config_dir()?.join("keyboard.calm"))
}

pub fn terminal_file() -> Result<PathBuf> {
    Ok(config_dir()?.join("terminal.calm"))
}

/// Ensures the full ~/.config/calm-shell/ tree exists, creating the
/// default calm-lavender theme and the topic-file scaffold on first run.
pub fn ensure_scaffold() -> Result<()> {
    let dirs_to_make = [
        config_dir()?,
        themes_dir()?,
        plugins_dir()?,
        history_dir()?,
    ];
    for d in dirs_to_make {
        fs::create_dir_all(&d).with_context(|| format!("creating {}", d.display()))?;
    }

    let defaults: [(PathBuf, &str); 8] = [
        (config_file()?, default_config_calm()),
        (environment_file()?, default_environment_calm()),
        (directory_file()?, default_directory_calm()),
        (history_settings_file()?, default_history_calm()),
        (keyboard_file()?, default_keyboard_calm()),
        (terminal_file()?, default_terminal_calm()),
        (aliases_file()?, default_aliases_calm()),
        (functions_file()?, default_functions_calm()),
    ];
    for (path, contents) in defaults {
        if !path.exists() {
            fs::write(&path, contents).with_context(|| format!("creating {}", path.display()))?;
        }
    }

    let lavender = themes_dir()?.join("calm-lavender.json");
    if !lavender.exists() {
        fs::write(&lavender, default_lavender_theme())?;
    }

    Ok(())
}

fn default_config_calm() -> &'static str {
    r#"# Calm-shell configuration
# See: https://github.com/s0uth09/calm-shell for the full .calm format guide
#
# This file is the entry point only — each concern lives in its own
# topic file (environment, directory, history, keyboard, terminal,
# aliases, functions), included below. Layout inspired by the classic
# oh-my-zsh split: github.com/mattjj/my-oh-my-zsh

[shell]
theme = "calm-lavender"
greeting = true

[prompt]
show_git = true
icon = "🌙"

include "environment.calm"
include "directory.calm"
include "history.calm"
include "keyboard.calm"
include "terminal.calm"
include "aliases.calm"
include "functions.calm"
"#
}

fn default_environment_calm() -> &'static str {
    r#"# Environment variables Calm-shell exports on launch

[environment]
EDITOR = "nvim"
PAGER = "less"
LANG = "en_US.UTF-8"
"#
}

fn default_directory_calm() -> &'static str {
    r#"# Directory navigation behavior and shortcuts

[directory]
# Typing a bare directory name (no spaces, no other command) cds into it.
auto_cd = true
# How many entries `pushd`/`popd`/`dirs` remember; oldest is dropped first.
dir_stack_size = 20

[directory.aliases]
".." = "cd .."
"..." = "cd ../.."
"...." = "cd ../../.."
"-" = "cd -"
"#
}

fn default_history_calm() -> &'static str {
    r#"# Shell history behavior

[history]
size = 10000
# `save` also feeds history capacity (the larger of the two wins); kept
# distinct from `size` for familiarity if you're used to zsh's HISTSIZE
# vs SAVEHIST split.
save = 10000
# Not wired yet — every entry is currently kept, duplicates included.
ignore_dups = true
# Not wired yet — history is per-session (one file, but not merged live
# across concurrently open calm sessions).
share_across_sessions = true
"#
}

fn default_keyboard_calm() -> &'static str {
    r#"# Keybindings

[keyboard]
# "emacs" or "vi" — this one is real and does switch reedline's edit mode.
edit_mode = "emacs"

# Not wired yet — these four happen to already match reedline's built-in
# emacs defaults, so they're accurate as documentation, but changing a
# value here won't actually remap anything yet.
[keyboard.bindings]
"ctrl+r" = "history-search-backward"
"ctrl+a" = "beginning-of-line"
"ctrl+e" = "end-of-line"
"alt+backspace" = "backward-kill-word"
"#
}

fn default_terminal_calm() -> &'static str {
    r#"# Terminal integration behavior

[terminal]
# Sets the terminal tab/window title to "calm — <cwd>" each prompt.
set_title = true
# Rings the terminal bell on calm's own errors (bad cd/pushd/popd target,
# a command that couldn't be spawned at all) — not on every failing
# command's nonzero exit status.
bell = false
# Not wired yet — `calm doctor` checks $COLORTERM independently, but
# nothing currently degrades the palette for terminals without truecolor.
truecolor = true
"#
}

fn default_aliases_calm() -> &'static str {
    r#"# Calm-shell aliases — key = command

[aliases]
ll = "ls -la"
gs = "git status"
gc = "git commit"
gp = "git push"
"#
}

fn default_functions_calm() -> &'static str {
    r##"# Calm-shell functions — triple-quoted blocks hold raw shell bodies

[functions]
mkcd = """
mkdir -p "$1" && cd "$1"
"""
"##
}

fn default_lavender_theme() -> &'static str {
    r##"{
  "name": "calm-lavender",
  "display_name": "Calm Lavender",
  "colors": {
    "calm_purple": "#CBA6F7",
    "soft_blue": "#89B4FA",
    "gentle_pink": "#F5C2E7",
    "mint_green": "#A6E3A1",
    "warm_yellow": "#F9E2AF",
    "soft_red": "#F38BA8",
    "cloud_gray": "#BAC2DE"
  },
  "prompt": {
    "icon": "🌙",
    "border_color": "cloud_gray",
    "path_color": "soft_blue",
    "git_clean_color": "mint_green",
    "git_dirty_color": "soft_red",
    "arrow_color": "calm_purple"
  }
}
"##
}
