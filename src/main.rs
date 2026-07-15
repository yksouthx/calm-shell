mod calm_format;
mod cli;
mod commands;
mod config;
mod git;
mod line_editor;
mod repl;
mod theme_engine;

use clap::Parser;
use cli::{Cli, Command, PluginAction, ThemeAction};
use colored::Colorize;

fn main() {
    let cli = Cli::parse();

    let result = match cli.command {
        None => commands::shell::launch(),
        Some(Command::Theme { action }) => match action {
            ThemeAction::List => commands::theme::list(),
            ThemeAction::Set { name } => commands::theme::set(&name),
        },
        Some(Command::Plugin { action }) => match action {
            PluginAction::Install { name } => commands::plugin::install(&name),
        },
        Some(Command::Config) => commands::config_cmd::open(),
        Some(Command::Doctor) => commands::doctor::run(),
    };

    if let Err(e) = result {
        eprintln!("{} {e}", "error:".truecolor(243, 139, 168).bold());
        std::process::exit(1);
    }
}
