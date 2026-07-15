use crate::config;
use anyhow::{bail, Result};
use colored::Colorize;
use std::fs;

/// Installs a plugin by name.
///
/// NOTE: there is no remote plugin registry yet. This scaffolds the local
/// plugin directory and a manifest stub so the plugin loader has something
/// concrete to discover, and is the natural place to wire in a real
/// registry/downloader later.
pub fn install(name: &str) -> Result<()> {
    config::ensure_scaffold()?;
    let plugin_dir = config::plugins_dir()?.join(name);

    if plugin_dir.exists() {
        bail!("plugin '{name}' is already installed at {}", plugin_dir.display());
    }

    fs::create_dir_all(&plugin_dir)?;
    let manifest = plugin_dir.join("plugin.json");
    fs::write(
        &manifest,
        format!(
            r#"{{
  "name": "{name}",
  "version": "0.1.0",
  "entry": "init.sh"
}}
"#
        ),
    )?;

    let entry = plugin_dir.join("init.sh");
    fs::write(&entry, format!("#!/usr/bin/env bash\n# {name} plugin entrypoint\n"))?;

    println!(
        "{} Installed plugin {} → {}",
        "✓".truecolor(166, 227, 161),
        name.truecolor(203, 166, 247).bold(),
        plugin_dir.display()
    );
    Ok(())
}
