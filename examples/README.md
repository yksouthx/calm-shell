# Calm-shell config examples

Drop-in references for customizing calm-shell, beyond the defaults
`calm` scaffolds into `~/.config/calm-shell/` on first run.

## How to use these

Each `.calm` file here has the same name as the file it's meant to
replace under `~/.config/calm-shell/`. To use one wholesale:

```sh
cp examples/aliases.calm ~/.config/calm-shell/aliases.calm
```

Or just open the example, copy the lines you actually want, and paste
them into your own file under the matching `[section]`. Nothing here
needs the whole file replaced to be useful.

| File | What it shows |
|---|---|
| `aliases.calm` | A fuller alias set (git shortcuts, `rm -i`/`mv -i`/`cp -i` safety nets, docker) than the four defaults |
| `functions.calm` | Multi-line shell functions beyond the default `mkcd`: `gcm` (commit with message), `backup`, `extract` |
| `directory.calm` | `auto_cd` plus named shortcuts to specific directories (`~dl`, `~docs`, ...) — point these at your own paths |
| `history.calm` | A 50,000-entry history instead of the 10,000 default |
| `keyboard.calm` | Switching to vi edit mode instead of the default emacs |
| `terminal.calm` | Turning the error bell on |
| `environment.calm` | A larger set of exported environment variables |
| `themes/calm-mint.json` | A complete custom theme, as a second option alongside the bundled Calm Lavender |

## Using the example theme

```sh
cp examples/themes/calm-mint.json ~/.config/calm-shell/themes/calm-mint.json
calm theme set calm-mint
```

`calm theme list` shows every theme currently available (bundled and
your own), with a marker next to whichever one is active.

### Writing your own theme

Copy `themes/calm-mint.json` as a starting point. The schema:

```json
{
  "name": "...",            // must match the filename (without .json)
  "display_name": "...",    // shown by `calm theme list` / the banner
  "colors": { "key": "#RRGGBB", ... },
  "prompt": {
    "icon": "...",
    "border_color": "...",      // must reference a key in "colors"
    "path_color": "...",
    "git_clean_color": "...",
    "git_dirty_color": "...",
    "arrow_color": "..."
  }
}
```

One thing worth knowing before you build a theme from scratch: the
line editor's syntax highlighting, hints, and completion menu look up
six specific color names *directly* — `calm_purple`, `soft_blue`,
`gentle_pink`, `warm_yellow`, `soft_red`, and `cloud_gray` — rather
than through the `prompt` section's indirection. A theme is missing
real color in the editor itself (not just the prompt) if `colors`
doesn't define all six, even though nothing enforces that at load
time — a missing one just quietly renders as plain, uncolored text.
`themes/calm-mint.json` defines all six for exactly this reason; the
project's test suite checks it (and the bundled default) for this on
every build, so treat that file as the canonical example of a
complete theme rather than a minimal one.

### Getting the theme's own icon to show

If your `~/.config/calm-shell/config.calm` still has the default
`[prompt] icon = "🌙"` line, it overrides whatever icon your active
theme sets (`calm-mint.json` sets `🌿`) — that's an intentional
per-user override, not a bug. Delete that line, or change it, if you
want the theme's own icon to show through.
