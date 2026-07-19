#include "term/emulator.h"

#include <stddef.h>
#include <stdlib.h>

#include "test_framework.h"

/* Every env var any detection path reads, so each sub-test can start
 * from a clean slate regardless of what the real environment (or an
 * earlier test) happened to set. */
static void clear_all_signals(void) {
    unsetenv("GHOSTTY_RESOURCES_DIR");
    unsetenv("KITTY_WINDOW_ID");
    unsetenv("WEZTERM_PANE");
    unsetenv("WEZTERM_EXECUTABLE");
    unsetenv("ALACRITTY_SOCKET");
    unsetenv("ALACRITTY_LOG");
    unsetenv("KONSOLE_VERSION");
    unsetenv("KONSOLE_DBUS_SESSION");
    unsetenv("GNOME_TERMINAL_SCREEN");
    unsetenv("GNOME_TERMINAL_SERVICE");
    unsetenv("TERM_PROGRAM");
    unsetenv("TERM");
}

static void test_name_roundtrip(int *run, int *failed) {
    TermEmulatorKind all[] = {TERM_EMU_GHOSTTY,  TERM_EMU_KITTY,   TERM_EMU_ALACRITTY,     TERM_EMU_WEZTERM,
                               TERM_EMU_FOOT,     TERM_EMU_KONSOLE, TERM_EMU_GNOME_TERMINAL};
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        CHECK(run, failed, term_emulator_from_name(term_emulator_name(all[i])) == all[i]);
    }
    CHECK(run, failed, term_emulator_from_name("not-a-real-terminal") == TERM_EMU_UNKNOWN);
    CHECK(run, failed, term_emulator_from_name(NULL) == TERM_EMU_UNKNOWN);
    CHECK_STR_EQ(run, failed, term_emulator_name(TERM_EMU_UNKNOWN), "unknown");
}

static void test_detects_each_signal(int *run, int *failed) {
    clear_all_signals();
    setenv("GHOSTTY_RESOURCES_DIR", "/opt/ghostty", 1);
    CHECK(run, failed, term_emulator_detect(NULL) == TERM_EMU_GHOSTTY);

    clear_all_signals();
    setenv("KITTY_WINDOW_ID", "1", 1);
    CHECK(run, failed, term_emulator_detect(NULL) == TERM_EMU_KITTY);

    clear_all_signals();
    setenv("WEZTERM_PANE", "0", 1);
    CHECK(run, failed, term_emulator_detect(NULL) == TERM_EMU_WEZTERM);

    clear_all_signals();
    setenv("TERM", "alacritty", 1);
    CHECK(run, failed, term_emulator_detect(NULL) == TERM_EMU_ALACRITTY);

    clear_all_signals();
    setenv("KONSOLE_VERSION", "24.0", 1);
    CHECK(run, failed, term_emulator_detect(NULL) == TERM_EMU_KONSOLE);

    clear_all_signals();
    setenv("GNOME_TERMINAL_SCREEN", "1", 1);
    CHECK(run, failed, term_emulator_detect(NULL) == TERM_EMU_GNOME_TERMINAL);

    clear_all_signals();
    setenv("TERM", "foot-extra", 1);
    CHECK(run, failed, term_emulator_detect(NULL) == TERM_EMU_FOOT);

    clear_all_signals();
    CHECK(run, failed, term_emulator_detect(NULL) == TERM_EMU_UNKNOWN);
}

static void test_override_wins(int *run, int *failed) {
    clear_all_signals();
    setenv("KITTY_WINDOW_ID", "1", 1);
    /* A recognized override short-circuits real detection entirely. */
    CHECK(run, failed, term_emulator_detect("alacritty") == TERM_EMU_ALACRITTY);
    /* An unrecognized override falls back to real detection instead
     * of silently reporting "unknown". */
    CHECK(run, failed, term_emulator_detect("not-a-real-terminal") == TERM_EMU_KITTY);
    /* Empty string behaves the same as no override. */
    CHECK(run, failed, term_emulator_detect("") == TERM_EMU_KITTY);
    clear_all_signals();
}

void run_emulator_tests(int *run, int *failed) {
    test_name_roundtrip(run, failed);
    test_detects_each_signal(run, failed);
    test_override_wins(run, failed);
}
