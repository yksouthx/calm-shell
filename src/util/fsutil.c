#include "util/fsutil.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util/strutil.h"
#include "util/xalloc.h"

static int g_last_errno = 0;

char *read_file_to_string(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        g_last_errno = errno;
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        g_last_errno = errno;
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        g_last_errno = errno;
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *buf = xmalloc((size_t)size + 1);
    size_t got = fread(buf, 1, (size_t)size, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

int errno_snapshot(void) {
    return g_last_errno;
}

char *dirname_of(const char *path) {
    const char *slash = strrchr(path, '/');
    if (!slash) {
        return xstrdup(".");
    }
    if (slash == path) {
        return xstrdup("/");
    }
    return xstrndup(path, (size_t)(slash - path));
}

char *join_path(const char *dir, const char *file) {
    size_t dlen = strlen(dir);
    bool needs_slash = dlen > 0 && dir[dlen - 1] != '/';
    return xsprintf("%s%s%s", dir, needs_slash ? "/" : "", file);
}

char *home_dir(void) {
    const char *home = getenv("HOME");
    if (home && home[0] != '\0') {
        return xstrdup(home);
    }
    return NULL;
}

char *xdg_config_dir(void) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] != '\0') {
        return xstrdup(xdg);
    }
    char *home = home_dir();
    if (!home) {
        return NULL;
    }
    char *out = join_path(home, ".config");
    free(home);
    return out;
}

char *xdg_data_dir(void) {
    const char *xdg = getenv("XDG_DATA_HOME");
    if (xdg && xdg[0] != '\0') {
        return xstrdup(xdg);
    }
    char *home = home_dir();
    if (!home) {
        return NULL;
    }
    char *local = join_path(home, ".local");
    char *out = join_path(local, "share");
    free(local);
    free(home);
    return out;
}

bool path_is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool path_is_file(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

bool path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

bool mkdir_recursive(const char *path) {
    if (mkdir(path, 0755) == 0 || errno == EEXIST) {
        return true;
    }
    if (errno != ENOENT) {
        return false;
    }
    char *parent = dirname_of(path);
    bool ok = strcmp(parent, path) != 0 && mkdir_recursive(parent);
    free(parent);
    if (!ok) {
        return false;
    }
    return mkdir(path, 0755) == 0 || errno == EEXIST;
}

bool overwrite_file(const char *path, const char *contents) {
    FILE *f = fopen(path, "w");
    if (!f) {
        g_last_errno = errno;
        return false;
    }
    size_t len = strlen(contents);
    bool ok = fwrite(contents, 1, len, f) == len;
    if (!ok) {
        g_last_errno = errno;
    }
    fclose(f);
    return ok;
}

bool append_to_file(const char *path, const char *contents) {
    FILE *f = fopen(path, "a");
    if (!f) {
        g_last_errno = errno;
        return false;
    }
    size_t len = strlen(contents);
    bool ok = fwrite(contents, 1, len, f) == len;
    if (!ok) {
        g_last_errno = errno;
    }
    fclose(f);
    return ok;
}

bool file_contains(const char *path, const char *needle) {
    char *contents = read_file_to_string(path);
    if (!contents) {
        return false;
    }
    bool found = strstr(contents, needle) != NULL;
    free(contents);
    return found;
}

char *current_working_dir(void) {
    size_t cap = 256;
    char *buf = xmalloc(cap);
    while (getcwd(buf, cap) == NULL) {
        if (errno != ERANGE) {
            free(buf);
            return NULL;
        }
        cap *= 2;
        buf = xrealloc(buf, cap);
    }
    return buf;
}
