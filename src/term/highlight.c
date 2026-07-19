#include "term/highlight.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>

#include "term/known_commands.h"
#include "util/strbuf.h"
#include "util/xalloc.h"

static bool is_known_command(const StrList *known, const char *word) {
    if (word[0] == '\0') {
        return false;
    }
    for (size_t i = 0; i < CALM_BUILTIN_COMMANDS_COUNT; i++) {
        if (strcmp(word, CALM_BUILTIN_COMMANDS[i]) == 0) {
            return true;
        }
    }
    if (strchr(word, '/') != NULL) {
        struct stat st;
        return stat(word, &st) == 0 && S_ISREG(st.st_mode);
    }
    return strlist_contains(known, word);
}

static void paint_into(StrBuf *sb, const Theme *theme, const char *key, const char *text) {
    char *painted = theme_paint(theme, key, text);
    strbuf_append(sb, painted);
    free(painted);
}

char *highlight_line(const Theme *theme, const StrList *known, const char *line) {
    StrBuf out;
    strbuf_init(&out);
    size_t len = strlen(line);
    size_t i = 0;
    bool first_word = true;

    while (i < len) {
        if (isspace((unsigned char)line[i])) {
            size_t start = i;
            while (i < len && isspace((unsigned char)line[i])) {
                i++;
            }
            strbuf_append_n(&out, line + start, i - start);
            continue;
        }
        if (line[i] == '"' || line[i] == '\'') {
            char quote = line[i];
            size_t start = i;
            i++;
            while (i < len && line[i] != quote) {
                i++;
            }
            if (i < len) {
                i++; /* consume closing quote */
            }
            char *chunk = xstrndup(line + start, i - start);
            paint_into(&out, theme, "warm_yellow", chunk);
            free(chunk);
            first_word = false;
            continue;
        }
        size_t start = i;
        while (i < len && !isspace((unsigned char)line[i]) && line[i] != '"' && line[i] != '\'') {
            i++;
        }
        char *word = xstrndup(line + start, i - start);
        if (first_word) {
            paint_into(&out, theme, is_known_command(known, word) ? "git_clean" : "soft_red", word);
        } else if (word[0] == '-') {
            paint_into(&out, theme, "soft_blue", word);
        } else {
            strbuf_append(&out, word);
        }
        free(word);
        first_word = false;
    }

    return strbuf_take(&out);
}
