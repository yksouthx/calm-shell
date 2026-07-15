use crate::calm_format;
use crate::config;
use crate::theme_engine::Theme;
use anyhow::Result;

/// Entry point for bare `calm` — launches the interactive shell.
///
/// Renders the branded banner (config.calm-driven), then hands off to
/// the real line editor (see `line_editor.rs` / `repl.rs`) for the
/// interactive loop: autosuggestions, syntax highlighting, aliases,
/// functions/plugins, the `cd` builtin, and history.
pub fn launch() -> Result<()> {
    config::ensure_scaffold()?;
    let theme = Theme::load_active()?;

    // config.calm drives whether the banner/greeting shows and lets the
    // prompt icon be overridden per-user without touching the theme file.
    let cfg = calm_format::parse_file(&config::config_file()?).unwrap_or_default();
    let greeting = cfg.get_bool("shell", "greeting").unwrap_or(true);
    let icon_override = cfg.get_str("prompt", "icon").map(str::to_string);
    let icon: &str = icon_override.as_deref().unwrap_or_else(|| theme.icon());

    if greeting {
        println!("{}  {}", icon, theme.paint("calm_purple", "Calm-shell"));
        println!("{}", theme.paint("cloud_gray", &format!("theme: {}", theme.display_name())));
        println!();
    }

    crate::repl::run(&theme, &cfg, icon)
}
