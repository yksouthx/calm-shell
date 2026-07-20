#include "config/paths.h"

#include <stdlib.h>
#include <stddef.h>

#include "util/fsutil.h"

char *config_dir(void) {
    char *base = xdg_config_dir();
    if (!base) {
        return NULL;
    }
    char *out = join_path(base, "calm-shell");
    free(base);
    return out;
}

static char *config_dir_join(const char *name) {
    char *dir = config_dir();
    if (!dir) {
        return NULL;
    }
    char *out = join_path(dir, name);
    free(dir);
    return out;
}

char *themes_dir(void) {
    return config_dir_join("themes");
}
char *plugins_dir(void) {
    return config_dir_join("plugins");
}
char *history_dir(void) {
    return config_dir_join("history");
}
char *profiles_dir(void) {
    return config_dir_join("profiles");
}
char *config_file(void) {
    return config_dir_join("config.calm");
}
char *aliases_file(void) {
    return config_dir_join("aliases.calm");
}
char *functions_file(void) {
    return config_dir_join("functions.calm");
}
char *environment_file(void) {
    return config_dir_join("environment.calm");
}
char *directory_file(void) {
    return config_dir_join("directory.calm");
}
char *history_settings_file(void) {
    return config_dir_join("history.calm");
}
char *keyboard_file(void) {
    return config_dir_join("keyboard.calm");
}
char *terminal_file(void) {
    return config_dir_join("terminal.calm");
}
char *active_theme_marker_file(void) {
    return config_dir_join(".active_theme");
}
char *profile_active_file(void) {
    return config_dir_join("profile_active.calm");
}
char *active_profile_marker_file(void) {
    return config_dir_join(".active_profile");
}
char *backups_dir(void) {
    return config_dir_join(".backups");
}
char *plugins_state_file(void) {
    return config_dir_join("plugins.calm");
}
char *plugins_active_file(void) {
    return config_dir_join("plugins_active.calm");
}
