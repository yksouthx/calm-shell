#ifndef CALM_COMMANDS_THEME_H
#define CALM_COMMANDS_THEME_H

/* Returns the name of the currently active theme (newly allocated,
 * caller must free()), defaulting to "calm-lavender" if no marker file
 * has been written yet. */
char *current_theme(void);

/* `calm theme list` */
int theme_cmd_list(void);
/* `calm theme set <name>` */
int theme_cmd_set(const char *name);

#endif /* CALM_COMMANDS_THEME_H */
