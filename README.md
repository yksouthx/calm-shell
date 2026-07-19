# Calm-shell

A calm, pastel, interactive shell for Linux. Minimal distraction, fast
workflow, friendly for beginners and powerful for advanced users.

```
╭─ user@host
│  🌙 Calm-shell
│  ~/Projects/Calm-shell
│  git:main ✓
╰─ ❯
```

Written in C, built with CMake. See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
for why (short version: C for everything that touches the terminal, a process,
or a config file; CMake because it's a proper build system, not a second
programming language).

## Installation

```bash
git clone https://github.com/s0uth09/calm-shell.git
cd calm-shell
cmake -S . -B build
cmake --build build
sudo cmake --install build   # installs to /usr/local/bin by default;
                              # pass -DCMAKE_INSTALL_PREFIX=... to change it
```

Requires a C11 compiler (`gcc`/`clang`) and CMake >= 3.16. No other build or
runtime dependencies -- calm-shell links only against the C standard library
and POSIX system calls, and shells out to `/bin/sh`, `git`, and `$EDITOR` at
runtime for the handful of things those already do well (see
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)).

Run the test suite with `ctest --test-dir build` (or `cmake --build build
--target test`), and turn on `-DCALM_WERROR=ON` at configure time to build
with warnings-as-errors, the same as CI does.

## Usage

```
calm                          open Calm-shell
calm theme list                list installed themes
calm theme set calm-lavender   switch the active theme
calm plugin install <src>      install a plugin (local dir or git URL)
calm plugin list               list installed plugins
calm config edit                open config.calm in $EDITOR
calm config path                 print the config directory path
calm config check                 validate config.calm
calm doctor                        diagnose environment issues
```

## Configuration

Everything lives under `~/.config/calm-shell/`, split into small topic files
rather than one growing config:

```
~/.config/calm-shell/
├── config.calm        # entry point, includes everything below
├── environment.calm   # exported env vars
├── directory.calm     # auto_cd, dir stack, cd shortcuts
├── history.calm       # history size, dedup
├── keyboard.calm      # edit mode, keybindings
├── terminal.calm      # title, bell, truecolor
├── aliases.calm       # command aliases
├── functions.calm     # shell function bodies
├── themes/
│   └── calm-lavender.calm
└── plugins/
```

Every one of those is `.calm` -- one format, one native parser
(`src/config/calmconf.c`), shared by shell config, themes, and plugin
manifests alike. Full syntax guide: [`docs/CONFIGURATION.md`](docs/CONFIGURATION.md).

```calm
[shell]
theme = "calm-lavender"
greeting = true

[functions]
mkcd = """
mkdir -p "$1" && cd "$1"
"""

include "aliases.calm"
```

### Examples

[`examples/`](examples/) has drop-in `.calm` files -- a fuller alias set,
a couple of extra functions, and a complete second theme (`calm-mint`) --
copy whichever ones you want over the matching file in
`~/.config/calm-shell/`. See [`examples/README.md`](examples/README.md).

## Status

**Working:** the CLI (`theme`, `plugin`, `config`, `doctor`), and the
interactive shell -- a raw-terminal line editor (single-pass syntax
highlighting for the command word / quoted strings / flags, Up/Down history
browsing, and Tab completion -- commands at the first word, filesystem paths
everywhere else, with directory matches getting a trailing `/`), fuzzy
history search on Ctrl+R (fzf's convention -- a non-contiguous
ordered-subsequence scorer), aliases, `[functions]` and installed plugins
(sourced as a preamble before each command), a `cd`/`pushd`/`popd`/`dirs`
builtin set plus `auto_cd` (bare directory names cd into themselves) and
directory-change syncing for everything else (so `mkcd foo`, or a plain
`mkdir x && cd x`, actually leaves you in the new directory, via a
non-blocking pipe read that can't hang on a backgrounded job), environment
variables, the terminal title, an error bell that rings for calm-shell's own
mistakes (not a command's ordinary nonzero exit), history capped and
deduplicated per `history.calm`'s `size`/`ignore_dups`, `terminal.calm`'s
`truecolor` flag (plus automatic `$NO_COLOR` support), and Ctrl+C that
interrupts the running command without killing the shell. The test suite
(`ctest`) covers the `.calm` parser (sections, scalars, lists, multiline
blocks, comments, includes and include cycles, error cases), theme loading,
alias expansion, the directory stack, history (dedup, capacity trimming,
disk round-trip, fuzzy search), the edit buffer, and command-routing logic.

**Known scope boundaries** (documented in code, not silently papered over):
the highlighter is a tokenizer (command word / strings / flags), not a full
shell grammar -- no pipe/operator awareness, and completion inherits that
same word-boundary-only view of the line (no flag-aware or per-command
completion specs). Multiple Tab matches print as a plain list rather than an
interactive menu. Line editing operates at the byte level, not full Unicode
grapheme-cluster granularity. Vi mode (`keyboard.calm`'s `edit_mode =
"vi"`) covers common movement/insert commands (`h`/`l`/`0`/`$`/`x`/`i`/`a`/
Esc), not a full modal-editing vocabulary. `export`ed variables from a
function or plugin don't sync back the way directory changes do.
`history.calm`'s `share_across_sessions` isn't wired (history is per-process).
`keyboard.calm`'s `[keyboard.bindings]` table documents bindings that happen
to match the editor's emacs defaults but isn't actually remappable yet. This
session's own new commands don't show up in the fuzzy history menu until the
next session (history is read once at startup). No job control (`bg`/`fg`;
`&`-backgrounding runs but isn't tracked).

## License

MIT -- see [LICENSE](LICENSE).
