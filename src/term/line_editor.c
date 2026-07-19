#include "term/line_editor.h"

#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "term/completion.h"
#include "term/edit_buffer.h"
#include "term/highlight.h"
#include "term/raw_terminal.h"
#include "util/strutil.h"
#include "util/xalloc.h"

/* Redraws just the input line (not the whole prompt box): clears the
 * current line, reprints the arrow + highlighted buffer, and
 * repositions the terminal cursor to match `buf->cursor`. */
static void redraw_input(const Theme *theme, const StrList *known, const EditBuf *buf) {
    fputs("\r\x1b[K", stdout);
    char *arrow = theme_paint(theme, "arrow", "\xE2\x9D\xAF ");
    fputs(arrow, stdout);
    free(arrow);

    char *highlighted = highlight_line(theme, known, buf->data);
    fputs(highlighted, stdout);
    free(highlighted);

    /* Move the terminal cursor back from the end of the (plain-text)
     * buffer to `buf->cursor`, in raw byte units. Styling codes don't
     * move the cursor, so this only needs to account for the visible
     * character count, which for our ANSI-wrapped output is just
     * buf->len (styling adds zero-width escape codes around, not
     * inside, each visible run). */
    size_t back = buf->len - buf->cursor;
    if (back > 0) {
        printf("\x1b[%zuD", back);
    }
    fflush(stdout);
}

/* Runs the Ctrl+R fuzzy history search sub-mode. On accept, fills
 * `buf` with the chosen entry (cursor at end) and returns true. On
 * cancel (Esc/Ctrl+G), leaves `buf` untouched and returns false. */
static bool fuzzy_history_search(History *hist, EditBuf *buf) {
    char query[256] = {0};
    size_t qlen = 0;
    size_t selected = 0;

    while (true) {
        StrList matches = history_fuzzy_search(hist, query, CALM_FUZZY_HISTORY_LIMIT);
        if (selected >= matches.count) {
            selected = matches.count > 0 ? matches.count - 1 : 0;
        }

        fputs("\r\x1b[K", stdout);
        printf("(reverse-i-search)`%s': %s", query, matches.count > 0 ? matches.items[selected] : "");
        fflush(stdout);

        int key = raw_terminal_read_key();
        if (key == -1 || key == KEY_ESCAPE || key == 7 /* Ctrl+G */) {
            strlist_free(&matches);
            fputs("\r\x1b[K", stdout);
            fflush(stdout);
            return false;
        }
        if (key == '\r' || key == '\n') {
            bool accepted = matches.count > 0;
            if (accepted) {
                editbuf_set(buf, matches.items[selected]);
            }
            strlist_free(&matches);
            fputs("\r\x1b[K", stdout);
            fflush(stdout);
            return accepted;
        }
        if (key == 18 /* Ctrl+R again: cycle to next match */) {
            if (matches.count > 0) {
                selected = (selected + 1) % matches.count;
            }
        } else if (key == 127 || key == 8) {
            if (qlen > 0) {
                query[--qlen] = '\0';
                selected = 0;
            }
        } else if (key >= 32 && key < 127 && qlen + 1 < sizeof(query)) {
            query[qlen++] = (char)key;
            query[qlen] = '\0';
            selected = 0;
        }
        strlist_free(&matches);
    }
}

/* Minimal vi normal-mode handling: movement (h/l/0/$), mode switches
 * (i/a back to insert), and delete-under-cursor (x). */
static bool handle_vi_normal_key(int key, EditBuf *buf, bool *insert_mode) {
    switch (key) {
        case 'i':
            *insert_mode = true;
            return true;
        case 'a':
            if (buf->cursor < buf->len) {
                buf->cursor++;
            }
            *insert_mode = true;
            return true;
        case 'h':
        case KEY_LEFT:
            if (buf->cursor > 0) {
                buf->cursor--;
            }
            return true;
        case 'l':
        case KEY_RIGHT:
            if (buf->cursor < buf->len) {
                buf->cursor++;
            }
            return true;
        case '0':
            buf->cursor = 0;
            return true;
        case '$':
            buf->cursor = buf->len > 0 ? buf->len - 1 : 0;
            return true;
        case 'x':
            editbuf_delete_at_cursor(buf);
            return true;
        default:
            return false;
    }
}

/* Splices `replacement` in over the word ending at `buf->cursor`,
 * appending a trailing space unless the replacement already ends in
 * '/' (a completed directory name, where a space would be wrong). */
static void apply_completion(EditBuf *buf, size_t word_start, const char *replacement) {
    size_t tail_len = buf->len - buf->cursor;
    char *tail = xstrndup(buf->data + buf->cursor, tail_len);

    buf->data[word_start] = '\0';
    buf->len = word_start;
    buf->cursor = word_start;
    for (const char *p = replacement; *p; p++) {
        editbuf_insert(buf, *p);
    }
    if (!ends_with(replacement, "/")) {
        editbuf_insert(buf, ' ');
    }
    for (const char *p = tail; *p; p++) {
        editbuf_insert(buf, *p);
    }
    buf->cursor -= tail_len;
    free(tail);
}

static void handle_tab(const StrList *known, EditBuf *buf) {
    size_t word_start = buf->cursor;
    while (word_start > 0 && !isspace((unsigned char)buf->data[word_start - 1])) {
        word_start--;
    }
    char *word = xstrndup(buf->data + word_start, buf->cursor - word_start);
    bool is_first_word = true;
    for (size_t i = 0; i < word_start; i++) {
        if (!isspace((unsigned char)buf->data[i])) {
            is_first_word = false;
            break;
        }
    }

    StrList matches =
        (is_first_word && strchr(word, '/') == NULL) ? complete_commands(known, word) : complete_paths(word);

    if (matches.count == 1) {
        apply_completion(buf, word_start, matches.items[0]);
    } else if (matches.count > 1) {
        fputs("\r\n", stdout);
        for (size_t i = 0; i < matches.count; i++) {
            printf("%s  ", matches.items[i]);
        }
        fputs("\r\n", stdout);
    }
    strlist_free(&matches);
    free(word);
}

LineSignal line_editor_read_line(const Theme *theme, const StrList *known, History *hist, bool vi_mode,
                                  char **out_line) {
    *out_line = NULL;

    if (!raw_terminal_enable()) {
        return LINE_ERROR;
    }

    EditBuf buf;
    editbuf_init(&buf);
    bool vi_insert_mode = true; /* line editing starts in an insert-like state regardless of mode */
    long history_cursor = (long)hist->count; /* one past the newest entry == "not browsing history" */
    char *saved_line_before_history = NULL;

    redraw_input(theme, known, &buf);

    LineSignal result = LINE_ERROR;
    while (true) {
        int key = raw_terminal_read_key();
        if (key == -1) {
            result = LINE_ERROR;
            break;
        }

        if (key == '\r' || key == '\n') {
            fputs("\r\n", stdout);
            *out_line = xstrdup(buf.data);
            if (!is_blank(buf.data)) {
                history_add(hist, buf.data);
            }
            result = LINE_SUCCESS;
            break;
        }
        if (key == 3 /* Ctrl+C */) {
            fputs("^C\r\n", stdout);
            result = LINE_CTRLC;
            break;
        }
        if (key == 4 /* Ctrl+D */) {
            if (buf.len == 0) {
                fputs("\r\n", stdout);
                result = LINE_CTRLD;
                break;
            }
            editbuf_delete_at_cursor(&buf);
            redraw_input(theme, known, &buf);
            continue;
        }

        if (vi_mode && !vi_insert_mode) {
            if (handle_vi_normal_key(key, &buf, &vi_insert_mode)) {
                redraw_input(theme, known, &buf);
            }
            continue; /* unrecognized normal-mode keys are ignored, never inserted */
        }
        if (vi_mode && key == KEY_ESCAPE) {
            vi_insert_mode = false;
            if (buf.cursor > 0) {
                buf.cursor--;
            }
            redraw_input(theme, known, &buf);
            continue;
        }

        switch (key) {
            case 1 /* Ctrl+A */:
            case KEY_HOME:
                buf.cursor = 0;
                break;
            case 5 /* Ctrl+E */:
            case KEY_END:
                buf.cursor = buf.len;
                break;
            case 11 /* Ctrl+K */:
                editbuf_kill_to_end(&buf);
                break;
            case 21 /* Ctrl+U */:
                editbuf_kill_whole_line(&buf);
                break;
            case 23 /* Ctrl+W */:
                editbuf_kill_word_before(&buf);
                break;
            case 127:
            case 8:
                editbuf_delete_before_cursor(&buf);
                break;
            case KEY_DEL:
                editbuf_delete_at_cursor(&buf);
                break;
            case KEY_LEFT:
                if (buf.cursor > 0) {
                    buf.cursor--;
                }
                break;
            case KEY_RIGHT:
                if (buf.cursor < buf.len) {
                    buf.cursor++;
                }
                break;
            case KEY_UP:
                if (hist->count > 0 && history_cursor > 0) {
                    if (history_cursor == (long)hist->count) {
                        free(saved_line_before_history);
                        saved_line_before_history = xstrdup(buf.data);
                    }
                    history_cursor--;
                    editbuf_set(&buf, hist->entries[history_cursor]);
                }
                break;
            case KEY_DOWN:
                if (history_cursor < (long)hist->count) {
                    history_cursor++;
                    if (history_cursor == (long)hist->count) {
                        editbuf_set(&buf, saved_line_before_history ? saved_line_before_history : "");
                    } else {
                        editbuf_set(&buf, hist->entries[history_cursor]);
                    }
                }
                break;
            case 9 /* Tab */:
                handle_tab(known, &buf);
                break;
            case 18 /* Ctrl+R */:
                fuzzy_history_search(hist, &buf);
                break;
            default:
                if (key >= 32 && key < 256 && key != 127) {
                    editbuf_insert(&buf, (char)key);
                }
                break;
        }

        redraw_input(theme, known, &buf);
    }

    free(saved_line_before_history);
    editbuf_free(&buf);
    raw_terminal_disable();
    return result;
}
