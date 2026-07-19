#include "term/raw_terminal.h"

#include <stdlib.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

static struct termios g_orig_termios;
static bool g_raw_mode_active = false;

void raw_terminal_disable(void) {
    if (g_raw_mode_active) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
        g_raw_mode_active = false;
    }
}

bool raw_terminal_enable(void) {
    if (tcgetattr(STDIN_FILENO, &g_orig_termios) != 0) {
        return false;
    }
    struct termios raw = g_orig_termios;
    raw.c_iflag &= (tcflag_t) ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= (tcflag_t) ~(OPOST);
    raw.c_lflag &= (tcflag_t) ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        return false;
    }
    g_raw_mode_active = true;
    atexit(raw_terminal_disable);
    return true;
}

/* Peeks whether more input is immediately available, to distinguish a
 * standalone Escape keypress from the start of a `\x1b[` arrow-key
 * escape sequence without blocking indefinitely. */
static bool input_ready_within(int millis) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = {.tv_sec = 0, .tv_usec = millis * 1000};
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

int raw_terminal_read_key(void) {
    unsigned char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n <= 0) {
        return -1;
    }
    if (c != 27) {
        return c;
    }
    if (!input_ready_within(50)) {
        return KEY_ESCAPE;
    }
    unsigned char seq0;
    if (read(STDIN_FILENO, &seq0, 1) <= 0) {
        return KEY_ESCAPE;
    }
    if (seq0 != '[' && seq0 != 'O') {
        return KEY_ESCAPE;
    }
    unsigned char seq1;
    if (read(STDIN_FILENO, &seq1, 1) <= 0) {
        return KEY_ESCAPE;
    }
    switch (seq1) {
        case 'A':
            return KEY_UP;
        case 'B':
            return KEY_DOWN;
        case 'C':
            return KEY_RIGHT;
        case 'D':
            return KEY_LEFT;
        case 'H':
            return KEY_HOME;
        case 'F':
            return KEY_END;
        case '3': {
            /* Delete key sends ESC [ 3 ~ */
            unsigned char tilde;
            if (read(STDIN_FILENO, &tilde, 1) <= 0) {
                return KEY_ESCAPE;
            }
            return KEY_DEL;
        }
        default:
            return KEY_ESCAPE;
    }
}
