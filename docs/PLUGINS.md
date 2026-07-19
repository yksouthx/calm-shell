# Plugins

A calm-shell plugin is a directory containing a `plugin.calm` manifest and an
entry script:

```
my-plugin/
├── plugin.calm
└── init.sh
```

```calm
# plugin.calm
name = "my-plugin"
version = "0.1.0"
entry = "init.sh"
```

`entry` is a path relative to the plugin's own directory. On every shell
launch, `shell/execute.c` sources it (`. "path/to/init.sh"`) as part of the
prelude, right alongside the shell functions from your own `functions.calm`
-- so `init.sh` can define shell functions, set up aliases via `alias`, or
just run setup code, exactly as if you'd written it into your own config.

## Installing

```sh
calm plugin install ./local-plugin-directory
calm plugin install https://github.com/someone/some-calm-plugin.git
```

A URL (`http(s)://`, `git@...`, or anything ending in `.git`) is fetched with
`git clone --depth 1`; anything else is treated as a local directory and
copied with `cp -r`. Either way, installation stages into a temporary
directory first and only replaces the final `~/.config/calm-shell/plugins/<name>/`
once a `plugin.calm` with a valid `entry` has actually been found -- a failed
clone or a directory that turns out not to be a plugin never leaves a
half-installed mess behind.

The installed directory's name comes from the manifest's own `name` field
when present, falling back to the source's last path component.

## Listing

```sh
calm plugin list
```

## Writing one

There's no special plugin API beyond "this shell script gets sourced into
every session" -- a plugin is just shell code. A minimal but complete
example:

```sh
# init.sh
greet() {
    echo "hello from my-plugin, $1"
}
```

```calm
# plugin.calm
name = "greeter"
version = "1.0.0"
entry = "init.sh"
```

After `calm plugin install ./greeter`, `greet world` works in any new shell
session like any other function or builtin.
