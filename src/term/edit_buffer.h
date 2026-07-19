/* The in-progress input line: a growable byte buffer plus a cursor
 * position, and the small set of editing operations the line editor's
 * keymap dispatches to.
 *
 * Scope, honestly: editing operates at the byte level, not full
 * Unicode grapheme-cluster granularity -- fine for the ASCII-heavy
 * commands a shell prompt actually sees, but a pasted multi-byte
 * character's cursor math can be a little off. */
#ifndef CALM_TERM_EDIT_BUFFER_H
#define CALM_TERM_EDIT_BUFFER_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    size_t cursor;
} EditBuf;

void editbuf_init(EditBuf *b);
void editbuf_free(EditBuf *b);

/* Replaces the whole buffer with `s`, moving the cursor to the end. */
void editbuf_set(EditBuf *b, const char *s);

void editbuf_insert(EditBuf *b, char c);
void editbuf_delete_before_cursor(EditBuf *b);
void editbuf_delete_at_cursor(EditBuf *b);
void editbuf_kill_to_end(EditBuf *b);
void editbuf_kill_whole_line(EditBuf *b);
/* Deletes the word immediately before the cursor, bash Ctrl+W style
 * (trailing whitespace, then the run of non-whitespace before it). */
void editbuf_kill_word_before(EditBuf *b);

#endif /* CALM_TERM_EDIT_BUFFER_H */
