/* First-run scaffolding: creates ~/.config/calm-shell/ and populates it
 * with the default topic files and bundled theme, without ever
 * overwriting a file the user already has. */
#ifndef CALM_CONFIG_SCAFFOLD_H
#define CALM_CONFIG_SCAFFOLD_H

#include <stdbool.h>
#include <stddef.h>

/* Ensures the full ~/.config/calm-shell/ tree exists, creating the
 * default calm-lavender theme and the topic-file scaffold on first
 * run. Returns true on success. */
bool ensure_scaffold(void);

/* The bundled calm-lavender theme, in `.calm` form, exposed so `calm
 * doctor` and the theme engine's own tests can validate the actual
 * shipped content instead of a second, hand-copied reference that
 * could drift. */
const char *default_lavender_theme(void);

/* One entry per topic file scaffold.c knows how to (re)create: its
 * path getter and its default-content generator. */
typedef struct {
    char *(*path_fn)(void);
    const char *(*content_fn)(void);
    const char *label; /* filename, for human-readable repair/doctor output */
} ScaffoldDefault;

/* Returns the fixed table backing ensure_scaffold(), so `calm repair`
 * can restore one specific corrupt topic file to its known-good
 * default without a second, hand-maintained copy of this list. */
const ScaffoldDefault *scaffold_defaults(size_t *out_count);

#endif /* CALM_CONFIG_SCAFFOLD_H */
