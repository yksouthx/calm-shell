/* The set of command names the line editor treats as "known" -- every
 * $PATH executable plus every alias/function name from config, built
 * once at startup rather than touching the filesystem per keystroke.
 * Backs both syntax highlighting (is this first word a real command?)
 * and Tab completion. */
#ifndef CALM_TERM_KNOWN_COMMANDS_H
#define CALM_TERM_KNOWN_COMMANDS_H

#include <stddef.h>

#include "config/calmconf.h"
#include "util/strlist.h"

/* Builtins that aren't $PATH executables or config-defined
 * aliases/functions, but should still tab-complete and highlight like
 * any other command. */
extern const char *const CALM_BUILTIN_COMMANDS[];
extern const size_t CALM_BUILTIN_COMMANDS_COUNT;

/* Scans $PATH plus [aliases], [directory.aliases], and [functions] in
 * `cfg`, returning the sorted, de-duplicated union. Caller must
 * strlist_free() the result. */
StrList known_commands_build(const CalmDocument *cfg);

#endif /* CALM_TERM_KNOWN_COMMANDS_H */
