#include "term/prompt.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "exec/git_status.h"
#include "util/fsutil.h"
#include "util/strbuf.h"
#include "util/xalloc.h"

char *display_path(void) {
    char *cwd = current_working_dir();
    if (!cwd) {
        return xstrdup("?");
    }
    char *home = home_dir();
    if (home) {
        size_t home_len = strlen(home);
        if (strncmp(cwd, home, home_len) == 0) {
            char *result = xsprintf("~%s", cwd + home_len);
            free(home);
            free(cwd);
            return result;
        }
        free(home);
    }
    return cwd;
}

static char *hostname_str(void) {
    char buf[256];
    if (gethostname(buf, sizeof(buf)) != 0 || buf[0] == '\0') {
        return xstrdup("localhost");
    }
    buf[sizeof(buf) - 1] = '\0';
    return xstrdup(buf);
}

static void paint_into(StrBuf *sb, const Theme *theme, const char *key, const char *text) {
    char *painted = theme_paint(theme, key, text);
    strbuf_append(sb, painted);
    free(painted);
}

void render_prompt_box(const Theme *theme, const char *icon) {
    StrBuf sb;
    strbuf_init(&sb);

    const char *user = getenv("USER");
    if (!user || user[0] == '\0') {
        user = "user";
    }
    char *host = hostname_str();
    char *path = display_path();
    char *user_host = xsprintf("%s@%s", user, host);

    paint_into(&sb, theme, "border", "\xE2\x95\xAD\xE2\x94\x80 ");
    paint_into(&sb, theme, "path", user_host);
    strbuf_append(&sb, "\n");

    paint_into(&sb, theme, "border", "\xE2\x94\x82  ");
    strbuf_append(&sb, icon);
    strbuf_append(&sb, " ");
    paint_into(&sb, theme, "calm_purple", "Calm-shell");
    strbuf_append(&sb, "\n");

    paint_into(&sb, theme, "border", "\xE2\x94\x82  ");
    paint_into(&sb, theme, "path", path);
    strbuf_append(&sb, "\n");

    GitStatus status;
    if (git_status(&status)) {
        paint_into(&sb, theme, "border", "\xE2\x94\x82  ");
        char *label = xsprintf("git:%s", status.branch);
        paint_into(&sb, theme, "gentle_pink", label);
        free(label);
        strbuf_append(&sb, " ");
        if (status.clean) {
            paint_into(&sb, theme, "git_clean", "\xE2\x9C\x93");
        } else {
            paint_into(&sb, theme, "git_dirty", "\xE2\x9C\x97");
        }
        strbuf_append(&sb, "\n");
        free(status.branch);
    }

    paint_into(&sb, theme, "border", "\xE2\x95\xB0\xE2\x94\x80 ");
    paint_into(&sb, theme, "arrow", "\xE2\x9D\xAF ");

    char *rendered = strbuf_take(&sb);
    fputs(rendered, stdout);
    fflush(stdout);
    free(rendered);
    free(host);
    free(path);
    free(user_host);
}
