# Architecture

## Language choices

**C is the implementation language for everything performance- or
correctness-critical** -- the shell core, the `.calm` parser, process
management, terminal/line-editing, and configuration handling. That's the
whole program; calm-shell doesn't have a part of its runtime that isn't one of
those things.

**CMake is the build system.** It's the one addition to C, and it earns its
place on concrete grounds: out-of-source builds, `ctest` integration for the
test suite, and dependency/compiler-feature detection that a hand-rolled
Makefile would otherwise reimplement. There's no Python, no shell-script
build step, no second language in the build at all -- `cmake --build` and
`ctest` are the entire toolchain.

Everything else that might have been a separate scripting layer -- plugin
installation, config-file editing -- shells out to existing, well-understood
Unix tools (`git clone`, `cp`, `mv`, `$EDITOR`) via `exec/process.c` rather
than reimplementing directory-tree copying or a git client in C. That's a
deliberate choice to keep the C surface area focused on the things only the
shell itself can do.

## Module map

```
src/
├── main.c              entry point -> cli/cli.c
├── version.h
├── util/                dependency-free helpers used everywhere else
│   ├── xalloc            malloc/realloc/strdup wrappers that abort on OOM
│   ├── strbuf              growable string builder
│   ├── strutil               trim/split/starts_with/ends_with
│   ├── strlist                  growable array of owned strings
│   └── fsutil                     path joins, file I/O, mkdir -p, cwd
├── config/               the .calm format and where files live
│   ├── calmconf            the parser itself -- see docs/CONFIGURATION.md
│   ├── paths                  ~/.config/calm-shell/* path getters
│   └── scaffold                  first-run default-file generation
├── theme/                theme loading -- reuses calmconf, not a second parser
├── term/                  the interactive line editor, built in layers:
│   ├── raw_terminal          termios raw mode + key reading
│   ├── edit_buffer              the in-progress input line + cursor
│   ├── known_commands              $PATH + config aliases/functions, for:
│   ├── highlight                     live syntax coloring
│   ├── completion                     Tab completion (commands, paths)
│   ├── history                          persistent history + fuzzy Ctrl+R
│   ├── prompt                             the theme-driven prompt box
│   └── line_editor                          ties the above into read_line()
├── exec/                  process-spawning primitives (fork/exec/pipe)
│   ├── process              generic: foreground run, capture stdout, PATH scan
│   └── git_status              prompt's git segment, built on process.c
├── shell/                  shell semantics
│   ├── alias_table            alias expansion
│   ├── dir_stack                the pushd/popd/dirs stack
│   ├── builtins                    cd/pushd/popd/dirs/exit, cwd bookkeeping
│   ├── execute                       /bin/sh -c dispatch + cwd sync trick
│   └── repl                            the main loop, wiring all of it up
└── cli/                     `calm <subcommand>` outside the interactive shell
    ├── cli                    dispatch table
    └── commands/                shell, theme, plugin, config, doctor
```

Each pair of files -- a `.h` describing *why* something exists and what it
promises, a `.c` implementing it -- is meant to be readable on its own. The
header comments carry the design rationale; the source files carry the how.

## Why fork/exec + `/bin/sh -c`, not a hand-rolled shell grammar

Calm-shell's own parser only needs to recognize "does this line look like a
builtin with no shell metacharacters" (`shell/execute.c`'s
`execute_needs_full_shell`) -- everything else (pipes, redirection,
`&&`/`||`/`;`, globbing, command substitution) is handed to a real POSIX
shell. Reimplementing POSIX shell grammar to avoid that dependency would be a
large, correctness-hazardous undertaking for a very small win: `/bin/sh` is
already installed everywhere calm-shell would run.

The one piece of state a forked subshell can't hand back through its exit
status is its own working directory -- if a typed line contains its own `cd`
(`cd foo && ls`), that `cd` happens in the subshell and would otherwise be
invisible back in the interactive shell. `execute_run()` solves this without
a temp file: the generated script writes its final `pwd` to a dedicated pipe
(fd 3) right before exiting, and the parent does a bounded, non-blocking read
of that pipe *after* `waitpid()` returns -- so a backgrounded job within the
line (`sleep 100 &`) holding its own inherited copy of that fd open can never
make the shell hang waiting for it to close.

## Why the `.calm` parser doesn't use a parser generator

The grammar is a single-pass, line-oriented format (see
`docs/CONFIGURATION.md`) -- there's no recursive structure beyond
one-level-deep lists and the literal-copy triple-quote blocks, both of which
a straightforward character scanner handles directly. A generated
lexer/parser would add a build-time dependency and a generated-code layer to
read through, for a grammar that's genuinely small enough to read as
straight-line C.
