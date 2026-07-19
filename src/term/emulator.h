/* Detects which terminal emulator is hosting the current session, by
 * the environment variable each emulator is known to set on every
 * session it spawns (not just login shells -- these survive into
 * plain interactive shells too). Detection is env-only and cheap: no
 * process walking, no querying the window system. */
#ifndef CALM_TERM_EMULATOR_H
#define CALM_TERM_EMULATOR_H

typedef enum {
    TERM_EMU_UNKNOWN = 0,
    TERM_EMU_GHOSTTY,
    TERM_EMU_KITTY,
    TERM_EMU_ALACRITTY,
    TERM_EMU_WEZTERM,
    TERM_EMU_FOOT,
    TERM_EMU_KONSOLE,
    TERM_EMU_GNOME_TERMINAL,
} TermEmulatorKind;

/* The machine-readable name used in terminal.calm's
 * `[sync] terminal_override` and in `calm sync` output ("kitty",
 * "gnome-terminal", ...). Returns "unknown" for TERM_EMU_UNKNOWN. */
const char *term_emulator_name(TermEmulatorKind kind);

/* Parses a name back to a kind (case-sensitive, matching
 * term_emulator_name's spelling). Returns TERM_EMU_UNKNOWN for
 * anything unrecognized, including NULL/empty. */
TermEmulatorKind term_emulator_from_name(const char *name);

/* Detects the terminal emulator hosting the current session.
 *
 * `override_name`, if non-NULL and non-empty, is trusted outright and
 * short-circuits detection entirely -- this is what lets a session
 * nested inside tmux/screen or reached over ssh (where the outer
 * emulator's env vars don't survive the hop) still get synced, via
 * terminal.calm's `[sync] terminal_override`. An override that
 * doesn't parse to a known emulator falls through to normal
 * detection rather than silently doing nothing. */
TermEmulatorKind term_emulator_detect(const char *override_name);

#endif /* CALM_TERM_EMULATOR_H */
