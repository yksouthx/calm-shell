#include "config/calm_serialize.h"

#include <stdio.h>
#include <string.h>

#include "util/xalloc.h"

/* Quoting a key defensively (always, not just when the bare-word form
 * would fail) keeps this serializer from needing to duplicate
 * calmconf's own "does this need quotes" rules -- a quoted key is
 * always valid input to the parser, whether or not it strictly needed
 * to be. Escaping mirrors calmconf's own unescape(): only `"` and `\`
 * are special, so those are the only two bytes doubled up here. */
static void write_quoted(StrBuf *sb, const char *s) {
    strbuf_append_char(sb, '"');
    for (const char *p = s; *p; p++) {
        if (*p == '"' || *p == '\\') {
            strbuf_append_char(sb, '\\');
        }
        strbuf_append_char(sb, *p);
    }
    strbuf_append_char(sb, '"');
}

static void write_scalar(StrBuf *sb, const CalmValue *v) {
    char numbuf[64];
    switch (v->type) {
        case CALM_STRING:
            write_quoted(sb, v->as.string);
            break;
        case CALM_BOOL:
            strbuf_append(sb, v->as.boolean ? "true" : "false");
            break;
        case CALM_INT:
            snprintf(numbuf, sizeof(numbuf), "%lld", v->as.integer);
            strbuf_append(sb, numbuf);
            break;
        case CALM_FLOAT:
            snprintf(numbuf, sizeof(numbuf), "%g", v->as.real);
            strbuf_append(sb, numbuf);
            break;
        case CALM_LIST:
            /* One level of list is all any pass-through section here
             * ever needs (e.g. `enabled = [...]`) -- a nested list
             * would be an authoring mistake in the source file, not
             * something calm-shell's own sections produce, so it's
             * written out empty rather than recursing. */
            strbuf_append_char(sb, '[');
            for (size_t i = 0; i < v->as.list.count; i++) {
                if (i > 0) {
                    strbuf_append(sb, ", ");
                }
                const CalmValue *item = &v->as.list.items[i];
                if (item->type == CALM_LIST) {
                    strbuf_append(sb, "[]");
                } else {
                    write_scalar(sb, item);
                }
            }
            strbuf_append_char(sb, ']');
            break;
    }
}

void calm_serialize_section(StrBuf *sb, const CalmDocument *doc, const char *section) {
    const CalmSection *s = calm_document_section(doc, section);
    if (!s || s->count == 0) {
        return;
    }
    strbuf_append_char(sb, '[');
    strbuf_append(sb, section);
    strbuf_append(sb, "]\n");
    for (size_t i = 0; i < s->count; i++) {
        write_quoted(sb, s->entries[i].key);
        strbuf_append(sb, " = ");
        if (s->entries[i].value.type == CALM_STRING && strcmp(section, "functions") == 0) {
            /* [functions] bodies are meant to be read back as raw
             * shell text, not re-escaped through the one-line string
             * path -- a triple-quoted block round-trips them exactly,
             * the same shape scaffold.c's own default_functions_calm()
             * uses. */
            strbuf_append(sb, "\"\"\"\n");
            strbuf_append(sb, s->entries[i].value.as.string);
            strbuf_append(sb, "\n\"\"\"");
        } else {
            write_scalar(sb, &s->entries[i].value);
        }
        strbuf_append_char(sb, '\n');
    }
    strbuf_append_char(sb, '\n');
}
