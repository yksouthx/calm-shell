#include "term/emulator.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

const char *term_emulator_name(TermEmulatorKind kind) {
    switch (kind) {
        case TERM_EMU_GHOSTTY:
            return "ghostty";
        case TERM_EMU_KITTY:
            return "kitty";
        case TERM_EMU_ALACRITTY:
            return "alacritty";
        case TERM_EMU_WEZTERM:
            return "wezterm";
        case TERM_EMU_FOOT:
            return "foot";
        case TERM_EMU_KONSOLE:
            return "konsole";
        case TERM_EMU_GNOME_TERMINAL:
            return "gnome-terminal";
        case TERM_EMU_UNKNOWN:
        default:
            return "unknown";
    }
}

TermEmulatorKind term_emulator_from_name(const char *name) {
    if (!name || name[0] == '\0') {
        return TERM_EMU_UNKNOWN;
    }
    static const TermEmulatorKind kinds[] = {
        TERM_EMU_GHOSTTY, TERM_EMU_KITTY,   TERM_EMU_ALACRITTY,     TERM_EMU_WEZTERM,
        TERM_EMU_FOOT,    TERM_EMU_KONSOLE, TERM_EMU_GNOME_TERMINAL,
    };
    for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
        if (strcmp(name, term_emulator_name(kinds[i])) == 0) {
            return kinds[i];
        }
    }
    return TERM_EMU_UNKNOWN;
}

static bool env_set(const char *name) {
    const char *v = getenv(name);
    return v != NULL && v[0] != '\0';
}

static bool env_equals(const char *name, const char *value) {
    const char *v = getenv(name);
    return v != NULL && strcmp(v, value) == 0;
}

static bool env_prefix(const char *name, const char *prefix) {
    const char *v = getenv(name);
    return v != NULL && strncmp(v, prefix, strlen(prefix)) == 0;
}

TermEmulatorKind term_emulator_detect(const char *override_name) {
    if (override_name && override_name[0] != '\0') {
        TermEmulatorKind k = term_emulator_from_name(override_name);
        if (k != TERM_EMU_UNKNOWN) {
            return k;
        }
        /* Unrecognized override: fall through to real detection
         * rather than pretending nothing is there. */
    }

    /* Each check below is the signal that emulator's own docs (or, in
     * KDE/GNOME's case, its D-Bus session bootstrap) document as set
     * on every session, GUI login shell or plain interactive one
     * alike. Order only matters where signals could otherwise
     * collide, which in practice they don't -- each emulator's marker
     * is unique to it. */

    if (env_set("GHOSTTY_RESOURCES_DIR") || env_equals("TERM", "xterm-ghostty")) {
        return TERM_EMU_GHOSTTY;
    }
    if (env_set("KITTY_WINDOW_ID") || env_equals("TERM", "xterm-kitty")) {
        return TERM_EMU_KITTY;
    }
    if (env_set("WEZTERM_PANE") || env_set("WEZTERM_EXECUTABLE") || env_equals("TERM_PROGRAM", "WezTerm")) {
        return TERM_EMU_WEZTERM;
    }
    if (env_set("ALACRITTY_SOCKET") || env_set("ALACRITTY_LOG") || env_equals("TERM", "alacritty")) {
        return TERM_EMU_ALACRITTY;
    }
    if (env_set("KONSOLE_VERSION") || env_set("KONSOLE_DBUS_SESSION")) {
        return TERM_EMU_KONSOLE;
    }
    if (env_set("GNOME_TERMINAL_SCREEN") || env_set("GNOME_TERMINAL_SERVICE")) {
        return TERM_EMU_GNOME_TERMINAL;
    }
    if (env_prefix("TERM", "foot")) {
        return TERM_EMU_FOOT;
    }

    return TERM_EMU_UNKNOWN;
}
