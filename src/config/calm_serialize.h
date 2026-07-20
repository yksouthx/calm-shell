/* Renders an already-parsed CalmDocument section back into `.calm`
 * source text. The one place calm-shell turns "a CalmSection's
 * entries" into "text calm_parse_file can read back" -- used
 * anywhere a generated overlay file is built from pass-through
 * sections of a source document, currently config/profile.c
 * (profile_active.calm) and config/plugin_loader.c
 * (plugins_active.calm). Kept small and section-shaped rather than a
 * general CalmDocument-to-text dump: neither caller ever needs to
 * serialize a whole document, just specific named sections out of
 * one. */
#ifndef CALM_CONFIG_CALM_SERIALIZE_H
#define CALM_CONFIG_CALM_SERIALIZE_H

#include "config/calmconf.h"
#include "util/strbuf.h"

/* Appends `[section]\nkey = value\n...\n\n` to `sb` for the named
 * section of `doc`, in the section's own entry order. A no-op (writes
 * nothing) if the section doesn't exist or has no entries -- an empty
 * `[section]` header with nothing under it isn't useful output and
 * would just be noise in the generated file. */
void calm_serialize_section(StrBuf *sb, const CalmDocument *doc, const char *section);

#endif /* CALM_CONFIG_CALM_SERIALIZE_H */
