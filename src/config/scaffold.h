/* First-run scaffolding: creates ~/.config/calm-shell/ and populates it
 * with the default topic files and bundled theme, without ever
 * overwriting a file the user already has. */
#ifndef CALM_CONFIG_SCAFFOLD_H
#define CALM_CONFIG_SCAFFOLD_H

#include <stdbool.h>

/* Ensures the full ~/.config/calm-shell/ tree exists, creating the
 * default calm-lavender theme and the topic-file scaffold on first
 * run. Returns true on success. */
bool ensure_scaffold(void);

/* The bundled calm-lavender theme, in `.calm` form, exposed so `calm
 * doctor` and the theme engine's own tests can validate the actual
 * shipped content instead of a second, hand-copied reference that
 * could drift. */
const char *default_lavender_theme(void);

#endif /* CALM_CONFIG_SCAFFOLD_H */
