use crate::calm_format;
use crate::config;
use anyhow::Result;
use colored::Colorize;
use std::env;
use std::path::Path;
use std::process::Command as Proc;

enum Status {
    Ok(String),
    Warn(String),
    Fail(String),
}

fn print_status(label: &str, status: Status) {
    let (icon, msg): (colored::ColoredString, String) = match status {
        Status::Ok(m) => ("✓".truecolor(166, 227, 161), m),
        Status::Warn(m) => ("!".truecolor(249, 226, 175), m),
        Status::Fail(m) => ("✗".truecolor(243, 139, 168), m),
    };
    println!("  {icon} {:<28} {}", label, msg.dimmed());
}

fn is_arch_based() -> Status {
    if let Ok(os_release) = std::fs::read_to_string("/etc/os-release") {
        let lower = os_release.to_lowercase();
        if lower.contains("endeavouros") {
            Status::Ok("EndeavourOS detected".into())
        } else if lower.contains("arch") {
            Status::Ok("Arch-based distro detected".into())
        } else {
            Status::Warn("not Arch-based — some features may not work".into())
        }
    } else {
        Status::Warn("/etc/os-release not found".into())
    }
}

fn command_exists(cmd: &str) -> bool {
    Proc::new("which")
        .arg(cmd)
        .output()
        .map(|o| o.status.success())
        .unwrap_or(false)
}

fn check_git() -> Status {
    if command_exists("git") {
        Status::Ok("found".into())
    } else {
        Status::Fail("not found — install with `pacman -S git`".into())
    }
}

fn check_hyprland() -> Status {
    if env::var("HYPRLAND_INSTANCE_SIGNATURE").is_ok() || command_exists("hyprctl") {
        Status::Ok("Hyprland detected".into())
    } else {
        Status::Warn("not running under Hyprland".into())
    }
}

fn check_truecolor() -> Status {
    match env::var("COLORTERM") {
        Ok(v) if v.contains("truecolor") || v.contains("24bit") => {
            Status::Ok("truecolor supported".into())
        }
        _ => Status::Warn("COLORTERM not set — pastel palette may render inaccurately".into()),
    }
}

fn check_config_dir() -> Result<Status> {
    config::ensure_scaffold()?;
    let dir = config::config_dir()?;
    if Path::new(&dir).exists() {
        Ok(Status::Ok(dir.display().to_string()))
    } else {
        Ok(Status::Fail("could not create config dir".into()))
    }
}

fn check_calm_config() -> Status {
    match config::config_file().and_then(|p| calm_format::parse_file(&p)) {
        Ok(_) => Status::Ok("config.calm parses cleanly".into()),
        Err(e) => Status::Fail(format!("config.calm failed to parse: {e}")),
    }
}

fn check_editor() -> Status {
    match env::var("EDITOR") {
        Ok(e) => Status::Ok(e),
        Err(_) => Status::Warn("$EDITOR not set — `calm config` will fall back to nano".into()),
    }
}

pub fn run() -> Result<()> {
    println!("{}", "Calm-shell doctor".truecolor(203, 166, 247).bold());
    println!("{}", "Diagnosing system compatibility...\n".dimmed());

    print_status("OS", is_arch_based());
    print_status("git", check_git());
    print_status("Hyprland", check_hyprland());
    print_status("Terminal colors", check_truecolor());
    print_status("$EDITOR", check_editor());
    print_status("Config directory", check_config_dir()?);
    print_status("config.calm", check_calm_config());

    println!("\n{}", "Done.".dimmed());
    Ok(())
}
