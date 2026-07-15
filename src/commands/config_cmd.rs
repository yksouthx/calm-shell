use crate::config;
use anyhow::Result;
use colored::Colorize;
use std::env;
use std::process::Command as Proc;

pub fn open() -> Result<()> {
    config::ensure_scaffold()?;
    let path = config::config_file()?;

    let editor = env::var("EDITOR").unwrap_or_else(|_| "nano".to_string());

    println!(
        "{} Opening {} with {editor}",
        "→".truecolor(137, 180, 250),
        path.display()
    );

    let status = Proc::new(&editor).arg(&path).status();

    match status {
        Ok(s) if s.success() => Ok(()),
        Ok(s) => {
            anyhow::bail!("{editor} exited with status {s}");
        }
        Err(e) => {
            anyhow::bail!("failed to launch {editor}: {e}. Set $EDITOR and try again.");
        }
    }
}
