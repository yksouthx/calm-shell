# The `.calm` configuration format

Every piece of Calm-shell configuration -- shell behavior, aliases, keybindings,
environment variables, plugin manifests, and themes -- is written in `.calm`, one
small format with one native parser (`src/config/calmconf.c`). There's no second
format to learn for themes, no third for plugin manifests.

## Layout on disk

```
~/.config/calm-shell/
├── config.calm          # entry point; includes the files below
├── environment.calm      # [environment]
├── directory.calm        # [directory], [directory.aliases]
├── history.calm           # [history]
├── keyboard.calm           # [keyboard], [keyboard.bindings]
├── terminal.calm            # [terminal]
├── aliases.calm               # [aliases]
├── functions.calm               # [functions]
├── themes/
│   └── calm-lavender.calm         # bundled default theme
├── plugins/
│   └── <plugin-name>/
│       ├── plugin.calm              # manifest: name, version, entry
│       └── ...                       # the entry script + anything else
└── .active_theme                  # one line: the active theme's name
```

`calm` creates this whole tree with sensible defaults the first time it runs, and
never overwrites a file you already have.

## Syntax

```calm
# A comment runs to the end of the line, anywhere it's not inside quotes.

[shell]
theme = "calm-lavender"    # string
greeting = true             # bool: true / false
retries = 3                 # int
timeout = 1.5                # float
tags = ["fast", "quiet"]      # list (any mix of scalar types)
bare_word = hello              # unquoted values are strings too

[directory.aliases]    # a dotted name is just a naming convention --
".." = "cd .."           # sections are a flat namespace, not a nested tree
"..." = "cd ../.."

[functions]
mkcd = """
mkdir -p "$1" && cd "$1"    # a comment INSIDE a triple-quoted block is left
"""                          # alone -- it's part of the shell body itself

include "aliases.calm"    # pulls in another file, resolved relative to
                           # this file's own directory
```

### Values

| Type   | Example                    | Notes                                            |
|--------|-----------------------------|---------------------------------------------------|
| string | `"calm-lavender"`, `hello`   | quotes are only required for whitespace/`#`/ambiguity |
| bool   | `true`, `false`               |                                                     |
| int    | `3`, `-7`                       |                                                     |
| float  | `1.5`, `-0.25`                    |                                                     |
| list   | `["fast", "quiet", 3, true]`         | commas at the top level split items; nested `[...]` and quoted commas don't |

### Comments

A `#` starts a comment unless it's inside a quoted string:

```calm
hex = "#CBA6F7"        # the # in the hex code is safe -- it's quoted
retries = 3 # this whole trailing bit is a comment
```

The one place comments are never stripped is inside a triple-quoted block --
a shell function's own `#` comments need to survive untouched.

### Triple-quoted blocks

For anything long or shell-body-shaped, use `"""`:

```calm
greet = """
echo "hello, $1"
echo "today is $(date)"
"""
```

The content is copied verbatim -- no escaping, no comment stripping -- between
the opening and closing `"""`. A one-liner also works: `note = """just this"""`.

### Quoted keys

Most keys are bare words (`theme`, `greeting`), but a key containing a space,
`+`, or other punctuation needs quotes:

```calm
[keyboard.bindings]
"ctrl+r" = "history-search-backward"
```

### `include`

```calm
include "aliases.calm"
```

Resolved relative to the including file's own directory. A file is only ever
included once even if referenced from multiple places (by canonical path), so
an accidental include cycle is a no-op, not a crash, and a shared file included
from two different topic files just works.

## Topic files

| File               | Section(s)                          | Purpose                              |
|--------------------|--------------------------------------|----------------------------------------|
| `config.calm`      | `[shell]`, `[prompt]`                  | entry point + top-level shell/prompt settings |
| `environment.calm` | `[environment]`                          | env vars exported on launch (`~`/`$VAR` expanded) |
| `directory.calm`   | `[directory]`, `[directory.aliases]`       | `auto_cd`, `dir_stack_size`, cd shortcuts |
| `history.calm`     | `[history]`                                  | `size`, `ignore_dups`, `share_across_sessions` |
| `keyboard.calm`    | `[keyboard]`, `[keyboard.bindings]`            | `edit_mode` ("emacs"/"vi") |
| `terminal.calm`    | `[terminal]`, `[appearance]`, `[sync]`           | `set_title`, `bell`, `truecolor`; font/cursor/opacity/padding/decorations; terminal-emulator + Fastfetch sync toggles |
| `aliases.calm`     | `[aliases]`                                        | `key = command` |
| `functions.calm`   | `[functions]`                                        | `name = """shell body"""` |

See each bundled default file's own comments for the full, current list of keys
-- they're generated fresh into `~/.config/calm-shell/` the first time you run
`calm`, and are the authoritative reference for what's actually wired up.

### Known scope boundaries

A couple of settings are read and documented but not (yet) acted on:

- `keyboard.bindings` -- the four bundled entries describe the line editor's
  actual emacs-mode defaults, but changing a value here doesn't remap
  anything yet.
- `history.share_across_sessions` -- history is per-process; two calm-shell
  sessions open at once don't merge their history live.

## Themes

A theme is a `.calm` file in `themes/`, with a `[colors]` palette and a
`[prompt]` section mapping semantic roles (`border_color`, `path_color`,
`arrow_color`, ...) to palette keys:

```calm
name = "calm-lavender"
display_name = "Calm Lavender"

[colors]
calm_purple = "#CBA6F7"
cloud_gray  = "#BAC2DE"

[prompt]
icon         = "🌙"
border_color = "cloud_gray"
arrow_color  = "calm_purple"
```

Switch themes with `calm theme set <name>`, or override just the prompt icon
in your own `config.calm`'s `[prompt] icon = "..."` without forking the whole
theme.

A theme can also set whole-terminal colors, in a `[terminal]` section (yes,
themes and `terminal.calm` both have a `[terminal]` section -- they're
different files, and this one is about color, not behavior):

```calm
[terminal]
background = "#1B1823"
foreground = "#E5E1F0"
cursor     = "calm_purple"   # a palette key or a literal "#RRGGBB" both work
```

All three are optional; a theme that omits `[terminal]` entirely still syncs
fine, with a neutral dark background/foreground and the prompt arrow's own
color for the cursor.

## Terminal & Fastfetch sync

`calm sync` (also run automatically by `theme set` and once per new shell
session) detects the terminal emulator you're running in -- Ghostty, Kitty,
Alacritty, WezTerm, Foot, Konsole, or GNOME Terminal -- and regenerates that
emulator's theme from the active `.calm` theme plus `terminal.calm`'s
`[appearance]` section (font, cursor style/blink, opacity, padding, window
decorations). It never overwrites your own config by hand: it writes a
`calm-shell-theme.*` file the emulator owns nothing of, and adds a single
`include`/`import` line to your real config to pull it in (once, idempotently
-- re-running sync doesn't duplicate it).

It also regenerates `~/.config/calm-shell/fastfetch.jsonc` -- Fastfetch's
logo and key/title colors follow the same palette -- and runs `fastfetch
--config` with it at the start of each session, so the two conveniently
never drift out of sync with each other.

All of this is controlled from terminal.calm's `[sync]` section:

```calm
[sync]
sync_terminal          = true
sync_fastfetch          = true
run_fastfetch_on_start  = true
terminal_override       = ""   # e.g. "kitty" -- useful under tmux/ssh
```

Detection reads environment variables each emulator sets on every session
(`KITTY_WINDOW_ID`, `WEZTERM_PANE`, ...), which don't survive a tmux/ssh hop
from the real host terminal. `terminal_override` sidesteps that by naming
the emulator outright. Neither `fastfetch` nor any particular terminal
emulator being installed is required -- sync for that piece is skipped
quietly, not an error.

## Plugins

A plugin is a directory under `plugins/` with a `plugin.calm` manifest:

```calm
name = "my-plugin"
version = "0.1.0"
entry = "init.sh"
```

`entry` is sourced (`. "path/to/init.sh"`) into every shell session's
prelude, alongside your `[functions]` -- so a plugin can define shell
functions and aliases exactly like your own config can. Install one with
`calm plugin install <local-directory-or-git-url>`.
