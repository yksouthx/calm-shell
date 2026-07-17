/* Renders the branded banner (config.calm-driven), then hands off to
 * the real line editor / REPL for the interactive loop. */
#include "shell.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../calm_format.h"
#include "../config.h"
#include "../repl.h"
#include "../theme.h"
#include "../util.h"

int shell_cmd_launch(void) {
    if (!ensure_scaffold()) {
        fprintf(stderr, "error: could not set up config directory\n");
        return 1;
    }

    Theme theme;
    if (!theme_load_active(&theme)) {
        fprintf(stderr, "error: could not load any theme (not even the bundled calm-lavender)\n");
        return 1;
    }

    /* config.calm drives whether the banner/greeting shows and lets the
     * prompt icon be overridden per-user without touching the theme
     * file. A parse error anywhere in the include chain used to fall
     * back to a blank, empty config with zero indication anything was
     * wrong -- every alias, function, and setting the user wrote would
     * silently vanish. Loud beats silent here: the shell still starts,
     * but the user finds out immediately. */
    char *config_path = config_file();
    CalmDocument cfg;
    char *err = NULL;
    bool cfg_ok = config_path && calm_parse_file(config_path, &cfg, &err);
    free(config_path);
    if (!cfg_ok) {
        fprintf(stderr, "calm: could not load config, starting with defaults instead: %s\n",
                err ? err : "unknown error");
        free(err);
        calm_document_init(&cfg);
    }

    bool greeting = calm_document_get_bool(&cfg, "shell", "greeting", true);
    const char *icon_override = calm_document_get_str(&cfg, "prompt", "icon");
    const char *icon = icon_override ? icon_override : theme.icon;

    if (greeting) {
        char *purple = theme_paint(&theme, "calm_purple", "Calm-shell");
        printf("%s  %s\n", icon, purple);
        free(purple);

        char *label = xsprintf("theme: %s", theme.display_name);
        char *painted_label = theme_paint(&theme, "cloud_gray", label);
        free(label);
        printf("%s\n\n", painted_label);
        free(painted_label);
    }

    int result = repl_run(&theme, &cfg, icon);

    calm_document_free(&cfg);
    theme_free(&theme);
    return result;
}
