use clap::{Parser, Subcommand};

/// Calm-shell — Fish's simplicity + Zsh's power + HyDE's beauty + Arch's flexibility.
#[derive(Parser)]
#[command(name = "calm", version, about, long_about = None)]
pub struct Cli {
    #[command(subcommand)]
    pub command: Option<Command>,
}

#[derive(Subcommand)]
pub enum Command {
    /// Manage Calm-shell themes
    Theme {
        #[command(subcommand)]
        action: ThemeAction,
    },
    /// Manage Calm-shell plugins
    Plugin {
        #[command(subcommand)]
        action: PluginAction,
    },
    /// Open the Calm-shell configuration in $EDITOR
    Config,
    /// Diagnose system compatibility (Arch/EndeavourOS, Hyprland, deps)
    Doctor,
}

#[derive(Subcommand)]
pub enum ThemeAction {
    /// List all installed themes
    List,
    /// Set the active theme
    Set {
        /// Theme name, e.g. calm-lavender
        name: String,
    },
}

#[derive(Subcommand)]
pub enum PluginAction {
    /// Install a plugin by name
    Install {
        /// Plugin name
        name: String,
    },
}
