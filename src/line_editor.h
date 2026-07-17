/* A raw-terminal line editor: theme-driven multi-line prompt box,
 * lightweight single-pass syntax highlighting (command word / quoted
 * strings / flags), Tab completion (commands at the first word,
 * filesystem paths everywhere else), persistent history with Up/Down
 * and a fuzzy Ctrl+R search, and a basic emacs/vi edit-mode split
 * driven by keyboard.calm's `edit_mode`.
 *
 * Scope honestly: the highlighter is a single-pass tokenizer, not a
 * real shell grammar. Editing operates at the byte level rather than
 * full Unicode grapheme-cluster granularity -- fine for the
 * ASCII-heavy commands a shell prompt actually sees, but a pasted
 * multi-byte character's cursor math can be a little off. Vi mode
 * covers the common movement/insert commands (h/l/0/$/x/i/a/Esc), not
 * a full modal-editing vocabulary. */
#ifndef CALM_LINE_EDITOR_H
#define CALM_LINE_EDITOR_H

#include <stdbool.h>
#include <stddef.h>

#include "calm_format.h"
#include "theme.h"

typedef enum {
    LINE_SUCCESS,
    LINE_CTRLC,
    LINE_CTRLD,
    LINE_ERROR,
} LineSignal;

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} StrList;

void strlist_init(StrList *list);
void strlist_add(StrList *list, const char *item);
bool strlist_contains(const StrList *list, const char *item);
void strlist_free(StrList *list);

typedef struct {
    char **entries; /* oldest first */
    size_t count;
    size_t cap;
    char *path; /* NULL if history couldn't be located */
} History;

bool history_load(const char *path, History *out);
/* Appends `line` to both the in-memory list and the on-disk file
 * (best-effort -- a write failure doesn't stop the shell running). */
void history_add(History *hist, const char *line);
void history_free(History *hist);

/* Builds the known-command set the highlighter/completer check
 * against: $PATH executables plus every alias/function name from
 * config, computed once at startup rather than touching the
 * filesystem per keystroke. */
StrList known_commands_build(const CalmDocument *cfg);

/* Shared with repl.c for the terminal-title OSC sequence, so both the
 * prompt and the title format the cwd identically (home-relative
 * `~/...`). Caller must free() the result. */
char *display_path(void);

/* Prints the theme-driven multi-line prompt box (user@host, icon +
 * name, cwd, git status line if in a repo, and the final arrow),
 * ending on the same line as the arrow with no trailing newline. */
void render_prompt_box(const Theme *theme, const char *icon);

/* Reads one line of input in raw mode, with the prompt box already
 * printed. Returns the line (newly allocated, caller must free()) via
 * *out_line on LINE_SUCCESS. */
LineSignal line_editor_read_line(const Theme *theme, const StrList *known, History *hist, bool vi_mode,
                                  char **out_line);

#endif /* CALM_LINE_EDITOR_H */
