# Calm-shell

> Fish's simplicity + Zsh's power + HyDE's beauty + Arch's flexibility.

A calm, pastel, Arch-native interactive shell for EndeavourOS. Minimal
distraction, fast workflow, friendly for beginners and powerful for
advanced Linux users.

```
╭─ user@endeavouros
│  🌙 Calm-shell
│  ~/Projects/Calm-shell
│  git:main ✓
╰─ ❯
```

## Installation

### AUR (recommended, on Arch/EndeavourOS)

Using an AUR helper such as `yay` or `paru`:

```bash
yay -S calm-shell        # latest tagged release
# or
yay -S calm-shell-git    # build from the latest master
```

### Manual (any POSIX system with a C compiler)

```bash
git clone https://github.com/s0uth09/calm-shell.git
cd calm-shell
make
sudo make install    # installs to /usr/local/bin by default; set PREFIX to change
```

Requires a C11 compiler (`gcc`/`clang`) and `make`. No other build or
runtime dependencies — calm-shell links only against the C standard
library and POSIX system calls.

## Usage

```
calm                          Open Calm-shell
calm theme list               List installed themes
calm theme set calm-lavender  Switch the active theme
calm plugin install <name>    Scaffold a new plugin
calm config                   Open config.calm in $EDITOR
calm doctor                   Diagnose system compatibility
```

## Configuration

Everything lives under `~/.config/calm-shell/`, split into small topic
files rather than one growing config — the same shape as the classic
oh-my-zsh layout (`environment.zsh`, `directory.zsh`, `history.zsh`, ...):

```
~/.config/calm-shell/
├── config.calm        # entry point, includes everything below
├── environment.calm   # exported env vars
├── directory.calm     # auto_cd, dir stack, cd shortcuts
├── history.calm       # history size/dedup/session sharing
├── keyboard.calm      # edit mode, keybindings
├── terminal.calm      # title, bell, truecolor
├── aliases.calm       # command aliases
├── functions.calm     # shell function bodies
├── themes/
│   └── calm-lavender.json
├── plugins/
└── history/
```

### The `.calm` format

```calm
# comments start with #
[section]
key = "string value"
flag = true
count = 3
tags = ["fast", "quiet"]

[functions]
mkcd = """
mkdir -p "$1" && cd "$1"
"""

include "aliases.calm"
```

Sections group related keys, values are typed (string / bool / int /
list), triple-quoted blocks hold multi-line shell bodies, and `include`
pulls in another `.calm` file relative to the current one.

### Themes

A theme is a JSON file in `themes/` defining the pastel palette and how
the prompt maps semantic roles (border, path, git-clean, git-dirty,
arrow) onto that palette. `calm theme set <name>` switches the active
theme; `calm doctor` checks your terminal actually supports truecolor
so the palette renders correctly.

### Examples

[`examples/`](examples/) has drop-in `.calm` files for every topic
above (a fuller alias set, extra functions, more directory shortcuts, a
bigger history, vi mode, the error bell) plus a complete second theme
— copy whichever ones you want over the matching file in
`~/.config/calm-shell/`. See [`examples/README.md`](examples/README.md)
for specifics, including a note on what a custom theme needs to define
to get full editor coloring, not just prompt coloring.

## Status

**Working:** the CLI (`theme`, `plugin`, `config`, `doctor`), and the
interactive shell — a raw-terminal line editor (single-pass syntax
highlighting for the command word / quoted strings / flags, Up/Down
history browsing, and Tab completion — commands at the first word,
filesystem paths everywhere else), fuzzy history search on Ctrl+R
(fzf's convention — a non-contiguous ordered-subsequence scorer, hand-
rolled rather than pulled from a library), aliases, `[functions]` and
installed plugins (sourced as a preamble before each command), a
`cd`/`pushd`/`popd`/`dirs` builtin set plus `auto_cd` (bare directory
names cd into themselves) and directory-change syncing for everything
else (so `mkcd foo`, or a plain `mkdir x && cd x`, actually leaves you
in the new directory), environment variables, the terminal title
(`terminal.calm`'s `set_title`), an error bell (`bell`), and Ctrl+C
that interrupts the running command without killing the shell.
`calm_format.c` (the config parser) has a test suite covering
sections, scalars, lists, multiline blocks, includes, and error cases;
the JSON parser, theme loading, and a handful of `util.c` helpers are
covered too (`make test`).

**Known scope boundaries** (documented in code, not silently papered
over): the highlighter is a tokenizer (command word / strings / flags),
not a full shell grammar — no pipe/operator awareness, and completion
inherits that same word-boundary-only view of the line (no flag-aware
or per-command completion specs). Multiple Tab matches print as a
plain list rather than an interactive menu. Line editing operates at
the byte level, not full Unicode grapheme-cluster granularity — fine
for the ASCII-heavy commands a shell prompt actually sees, but a pasted
multi-byte character's cursor math can be a little off. Vi mode
(`keyboard.calm`'s `edit_mode = "vi"`) covers common movement/insert
commands (`h`/`l`/`0`/`$`/`x`/`i`/`a`/Esc), not a full modal-editing
vocabulary (no `dd`/`dw`/counts). `export`ed variables from a function
or plugin don't sync back the way directory changes now do.
`history.calm`'s `ignore_dups`/`share_across_sessions` aren't wired yet
(`size`/`save` are read but history isn't capacity-trimmed in memory
yet, only ever appended to on disk). `keyboard.calm`'s
`[keyboard.bindings]` table documents bindings that happen to match the
editor's emacs defaults but isn't actually remappable yet.
`terminal.calm`'s `truecolor` flag isn't read (`calm doctor` checks
`$COLORTERM` independently of it). This session's own new commands
don't show up in the fuzzy history menu until the next session (the
menu reads the history file once at startup, not per-keystroke). No
job control (`bg`/`fg`, `&`-backgrounding isn't tracked). Hyprland
integration is currently detection-only in `calm doctor`.

## License

MIT — see [LICENSE](LICENSE).
