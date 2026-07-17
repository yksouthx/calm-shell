/* Installs a plugin by name.
 *
 * NOTE: there is no remote plugin registry yet. This scaffolds the local
 * plugin directory and a manifest stub so the plugin loader has something
 * concrete to discover, and is the natural place to wire in a real
 * registry/downloader later. */
#include "plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../config.h"
#include "../util.h"

int plugin_cmd_install(const char *name) {
    /* A plugin name that contains a path separator or ".." could escape
     * plugins_dir() entirely (e.g. "../../etc/cron.d/x") -- reject it
     * outright rather than trusting it as a bare directory-name
     * component. */
    if (strchr(name, '/') != NULL || strcmp(name, "..") == 0 || strcmp(name, ".") == 0) {
        fprintf(stderr, "error: invalid plugin name '%s'\n", name);
        return 1;
    }

    if (!ensure_scaffold()) {
        fprintf(stderr, "error: could not set up config directory\n");
        return 1;
    }

    char *plugins = plugins_dir();
    char *plugin_dir = join_path(plugins, name);
    free(plugins);

    struct stat st;
    if (stat(plugin_dir, &st) == 0) {
        fprintf(stderr, "error: plugin '%s' is already installed at %s\n", name, plugin_dir);
        free(plugin_dir);
        return 1;
    }

    if (mkdir(plugin_dir, 0755) != 0) {
        fprintf(stderr, "error: could not create %s\n", plugin_dir);
        free(plugin_dir);
        return 1;
    }

    char *manifest_path = join_path(plugin_dir, "plugin.json");
    FILE *manifest = fopen(manifest_path, "w");
    if (manifest) {
        fprintf(manifest, "{\n  \"name\": \"%s\",\n  \"version\": \"0.1.0\",\n  \"entry\": \"init.sh\"\n}\n", name);
        fclose(manifest);
    }
    free(manifest_path);

    char *entry_path = join_path(plugin_dir, "init.sh");
    FILE *entry = fopen(entry_path, "w");
    if (entry) {
        fprintf(entry, "#!/usr/bin/env bash\n# %s plugin entrypoint\n", name);
        fclose(entry);
    }
    free(entry_path);

    printf("\x1b[38;2;166;227;161m\xE2\x9C\x93\x1b[0m Installed plugin \x1b[38;2;203;166;247m\x1b[1m%s\x1b[0m -> %s\n",
           name, plugin_dir);
    free(plugin_dir);
    return 0;
}
