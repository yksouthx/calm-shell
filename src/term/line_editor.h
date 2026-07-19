/* The interactive line editor: raw-mode input, highlighting, Tab
 * completion, Up/Down history browsing, and a fuzzy Ctrl+R search,
 * assembled from raw_terminal / edit_buffer / highlight / completion /
 * history.
 *
 * Scope, honestly: vi mode (keyboard.calm's `edit_mode = "vi"`) covers
 * common movement/insert commands (h/l/0/$/x/i/a/Esc), not a full
 * modal-editing vocabulary (no dd/dw/counts). */
#ifndef CALM_TERM_LINE_EDITOR_H
#define CALM_TERM_LINE_EDITOR_H

#include "term/history.h"
#include "term/known_commands.h" /* StrList of known commands, re-exported for callers */
#include "theme/theme.h"

typedef enum {
    LINE_SUCCESS,
    LINE_CTRLC,
    LINE_CTRLD,
    LINE_ERROR,
} LineSignal;

/* Reads one line of input in raw mode, with the prompt box already
 * printed. Returns the line (newly allocated, caller must free()) via
 * *out_line on LINE_SUCCESS; a successful, non-blank line is also
 * appended to `hist`. */
LineSignal line_editor_read_line(const Theme *theme, const StrList *known, History *hist, bool vi_mode,
                                  char **out_line);

#endif /* CALM_TERM_LINE_EDITOR_H */
