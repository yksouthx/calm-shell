/* Raw-mode terminal control and low-level key reading. Everything
 * above this layer (edit_buffer, line_editor) deals in logical
 * keypresses, never raw bytes or termios directly. */
#ifndef CALM_TERM_RAW_TERMINAL_H
#define CALM_TERM_RAW_TERMINAL_H

#include <stdbool.h>

typedef enum {
    KEY_NONE = 0,
    KEY_UP = 1000,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_HOME,
    KEY_END,
    KEY_DEL,
    KEY_ESCAPE,
} SpecialKey;

/* Puts stdin into raw mode (no echo, no line buffering, signals
 * delivered as literal bytes rather than generating SIGINT/SIGQUIT).
 * Registers an atexit() handler so a crash or normal exit always
 * restores the original terminal settings. Returns false if stdin
 * isn't a terminal capable of it. */
bool raw_terminal_enable(void);
void raw_terminal_disable(void);

/* Reads one logical keypress: a plain byte (0-255), or one of the
 * SpecialKey constants for arrows/Home/End/Delete/Escape. Returns -1
 * on read error/EOF. Blocks until a key is available. */
int raw_terminal_read_key(void);

#endif /* CALM_TERM_RAW_TERMINAL_H */
