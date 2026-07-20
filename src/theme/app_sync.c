#include "theme/app_sync.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "theme/color_scheme.h"
#include "util/fsutil.h"
#include "util/strbuf.h"
#include "util/strutil.h"
#include "util/xalloc.h"

/* ---- shared helpers (deliberately duplicated from term/emulator_sync.c
 * rather than exported from it -- each app's directive syntax differs
 * enough, and each TU's copy is small enough, that a shared header
 * would need more parameters than it would save lines). ---- */

static char *config_subdir(const char *app) {
    char *base = xdg_config_dir();
    if (!base) {
        return NULL;
    }
    char *out = join_path(base, app);
    free(base);
    return out;
}

static const char *strip_hash(const char *hex) {
    return (hex[0] == '#') ? hex + 1 : hex;
}

static char *hex_of_semantic(const Theme *t, const char *key, unsigned char fallback_r, unsigned char fallback_g,
                              unsigned char fallback_b) {
    unsigned char r, g, b;
    if (!theme_rgb(t, key, &r, &g, &b)) {
        r = fallback_r;
        g = fallback_g;
        b = fallback_b;
    }
    return xsprintf("#%02X%02X%02X", r, g, b);
}

/* Same idempotent one-line-include pattern as
 * emulator_sync.c:ensure_include_line() -- see that file's comment
 * for the reasoning. Duplicated rather than shared because the two
 * modules don't otherwise depend on each other and this is the only
 * piece they'd need to share. */
static bool ensure_include_line(const char *main_config_path, const char *generated_basename,
                                 const char *directive_line, const char *comment_prefix) {
    if (file_contains(main_config_path, generated_basename)) {
        return true;
    }
    if (!path_exists(main_config_path)) {
        return overwrite_file(main_config_path, directive_line);
    }
    char *addition = xsprintf("\n%s Added by calm-shell (`calm sync`) -- keeps this app's theme\n"
                               "%s matched to Calm-shell's active theme.\n%s",
                               comment_prefix, comment_prefix, directive_line);
    bool ok = append_to_file(main_config_path, addition);
    free(addition);
    return ok;
}

/* Replaces `key=`'s value under `[section]` in `ini_text` if present,
 * inserts it right after the section header if the section exists but
 * the key doesn't, or appends a new `[section]` block if the section
 * itself is missing. Returns a newly allocated string; never mutates
 * `ini_text`. Used for the two apps here (Cava, Fuzzel) whose config
 * format has no include/import mechanism at all -- patching just the
 * handful of keys calm-shell owns is the only way to sync their color
 * without either overwriting the user's whole file or leaving their
 * customizations to those exact keys stuck on an old theme. */
static char *ini_set(const char *ini_text, const char *section, const char *key, const char *value) {
    size_t line_count = 0;
    char **lines = ini_text ? split_lines(ini_text, &line_count) : NULL;
    char *want_header = xsprintf("[%s]", section);
    size_t key_len = strlen(key);
    bool top_level = section[0] == '\0'; /* no [section] headers at all, e.g. btop.conf, Helix's config.toml */

    StrBuf out;
    strbuf_init(&out);
    bool in_section = top_level;
    bool section_found = top_level;
    bool key_written = false;
    for (size_t i = 0; i < line_count; i++) {
        char *trimmed = xstrdup(lines[i]);
        trim_in_place(trimmed);
        if (!top_level && trimmed[0] == '[') {
            if (in_section && !key_written) {
                char *kv = xsprintf("%s = %s\n", key, value);
                strbuf_append(&out, kv);
                free(kv);
                key_written = true;
            }
            in_section = strcmp(trimmed, want_header) == 0;
            if (in_section) {
                section_found = true;
            }
            strbuf_append(&out, lines[i]);
            strbuf_append(&out, "\n");
        } else if (in_section && !key_written && strncmp(trimmed, key, key_len) == 0 &&
                   (trimmed[key_len] == '=' || trimmed[key_len] == ' ')) {
            char *kv = xsprintf("%s = %s\n", key, value);
            strbuf_append(&out, kv);
            free(kv);
            key_written = true;
        } else {
            strbuf_append(&out, lines[i]);
            strbuf_append(&out, "\n");
        }
        free(trimmed);
    }
    if (in_section && !key_written) {
        char *kv = xsprintf("%s = %s\n", key, value);
        strbuf_append(&out, kv);
        free(kv);
        key_written = true;
    }
    if (!section_found) {
        char *block = xsprintf("[%s]\n%s = %s\n", section, key, value);
        strbuf_append(&out, block);
        free(block);
    }

    free(want_header);
    if (lines) {
        free_lines(lines, line_count);
    }
    return strbuf_take(&out);
}

static void add_entry(AppSyncResult *r, const char *app, bool synced, char *detail_owned) {
    r->items = xrealloc(r->items, (r->count + 1) * sizeof(AppSyncEntry));
    r->items[r->count].app = xstrdup(app);
    r->items[r->count].synced = synced;
    r->items[r->count].detail = detail_owned;
    r->count++;
}

void app_sync_result_free(AppSyncResult *r) {
    for (size_t i = 0; i < r->count; i++) {
        free(r->items[i].app);
        free(r->items[i].detail);
    }
    free(r->items);
    r->items = NULL;
    r->count = 0;
}

/* ---- Cava ---------------------------------------------------------------
 * ~/.config/cava/config is one flat INI file with no include support
 * (https://github.com/karlstav/cava -- config.md documents no
 * mechanism for splitting it up). Patches `[color]` in place. */

static void sync_cava(const Theme *theme, AppSyncResult *out) {
    char *dir = config_subdir("cava");
    if (!dir || !mkdir_recursive(dir)) {
        free(dir);
        add_entry(out, "cava", false, xstrdup("could not create ~/.config/cava"));
        return;
    }
    TermColorScheme cs;
    color_scheme_build(theme, &cs);

    char *path = join_path(dir, "config");
    char *existing = read_file_to_string(path);
    char *bg_val = xsprintf("'%s'", cs.background);
    char *fg_val = xsprintf("'%s'", cs.foreground);

    char *content = existing ? xstrdup(existing) : xstrdup("[general]\nbars = 0\n\n[color]\ngradient = 1\n");
    char *step;
    step = ini_set(content, "color", "background", bg_val);
    free(content);
    content = step;
    step = ini_set(content, "color", "foreground", fg_val);
    free(content);
    content = step;
    step = ini_set(content, "color", "gradient", "1");
    free(content);
    content = step;
    step = ini_set(content, "color", "gradient_count", "6");
    free(content);
    content = step;
    /* Six-stop gradient across the theme's red/yellow/green/cyan/
     * blue/magenta ANSI slots -- a spectrum sweep rather than a flat
     * single color, since that's what Cava's gradient mode is for. */
    static const int gradient_slots[6] = {1, 3, 2, 6, 4, 5};
    for (int i = 0; i < 6; i++) {
        char key[32];
        snprintf(key, sizeof(key), "gradient_color_%d", i + 1);
        char *val = xsprintf("'%s'", cs.ansi[gradient_slots[i]]);
        step = ini_set(content, "color", key, val);
        free(val);
        free(content);
        content = step;
    }

    bool wrote = overwrite_file(path, content);
    add_entry(out, "cava", wrote,
              wrote ? xstrdup("cava: [color] keys in config updated in place")
                    : xstrdup("cava: failed to write ~/.config/cava/config"));

    free(content);
    free(fg_val);
    free(bg_val);
    free(existing);
    free(path);
    color_scheme_free(&cs);
    free(dir);
}

/* ---- btop -----------------------------------------------------------
 * btop's theme mechanism IS a separate named file: ~/.config/btop/
 * themes/<name>.theme, selected by `color_theme = "<name>"` in
 * btop.conf. That one key is patched the same way Konsole's profile
 * ColorScheme= key is (a single-key INI-ish patch), never the rest
 * of btop.conf. */

static void sync_btop(const Theme *theme, AppSyncResult *out) {
    char *dir = config_subdir("btop");
    char *themes_subdir = dir ? join_path(dir, "themes") : NULL;
    if (!dir || !themes_subdir || !mkdir_recursive(themes_subdir)) {
        free(themes_subdir);
        free(dir);
        add_entry(out, "btop", false, xstrdup("could not create ~/.config/btop/themes"));
        return;
    }
    TermColorScheme cs;
    color_scheme_build(theme, &cs);
    char *accent = hex_of_semantic(theme, "arrow", 0xCB, 0xA6, 0xF7);
    char *ok_color = hex_of_semantic(theme, "git_clean", 0xA6, 0xE3, 0xA1);
    char *warn_color = hex_of_semantic(theme, "git_dirty", 0xF9, 0xE2, 0xAF);
    char *danger_color = hex_of_semantic(theme, "git_dirty", 0xF3, 0x8B, 0xA8);

    StrBuf sb;
    strbuf_init(&sb);
    strbuf_append(&sb, "# Generated by calm-shell (`calm sync`) -- edits here are overwritten\n"
                        "# on the next sync. Change the active Calm-shell theme instead.\n\n");
    char *chunk = xsprintf(
        "theme[main_bg]=\"%s\"\ntheme[main_fg]=\"%s\"\ntheme[title]=\"%s\"\n"
        "theme[hi_fg]=\"%s\"\ntheme[selected_bg]=\"%s\"\ntheme[selected_fg]=\"%s\"\n"
        "theme[inactive_fg]=\"%s\"\ntheme[graph_text]=\"%s\"\ntheme[proc_misc]=\"%s\"\n"
        "theme[cpu_box]=\"%s\"\ntheme[mem_box]=\"%s\"\ntheme[net_box]=\"%s\"\ntheme[proc_box]=\"%s\"\n"
        "theme[div_line]=\"%s\"\ntheme[temp_start]=\"%s\"\ntheme[temp_mid]=\"%s\"\ntheme[temp_end]=\"%s\"\n"
        "theme[cpu_start]=\"%s\"\ntheme[cpu_mid]=\"%s\"\ntheme[cpu_end]=\"%s\"\n"
        "theme[free_start]=\"%s\"\ntheme[free_mid]=\"%s\"\ntheme[free_end]=\"%s\"\n"
        "theme[cached_start]=\"%s\"\ntheme[cached_mid]=\"%s\"\ntheme[cached_end]=\"%s\"\n"
        "theme[available_start]=\"%s\"\ntheme[available_mid]=\"%s\"\ntheme[available_end]=\"%s\"\n"
        "theme[used_start]=\"%s\"\ntheme[used_mid]=\"%s\"\ntheme[used_end]=\"%s\"\n"
        "theme[download_start]=\"%s\"\ntheme[download_mid]=\"%s\"\ntheme[download_end]=\"%s\"\n"
        "theme[upload_start]=\"%s\"\ntheme[upload_mid]=\"%s\"\ntheme[upload_end]=\"%s\"\n"
        "theme[process_start]=\"%s\"\ntheme[process_mid]=\"%s\"\ntheme[process_end]=\"%s\"\n",
        cs.background, cs.foreground, accent, cs.foreground, accent, cs.background, cs.ansi[8], cs.foreground,
        cs.foreground, accent, accent, accent, accent, accent, ok_color, warn_color, danger_color, ok_color,
        warn_color, danger_color, ok_color, warn_color, danger_color, ok_color, warn_color, danger_color, ok_color,
        warn_color, danger_color, danger_color, warn_color, ok_color, accent, ok_color, danger_color, accent,
        ok_color, danger_color, accent, ok_color, danger_color);
    strbuf_append(&sb, chunk);
    free(chunk);

    char *contents = strbuf_take(&sb);
    char *theme_path = join_path(themes_subdir, "calm-shell.theme");
    bool wrote = overwrite_file(theme_path, contents);
    free(contents);

    bool wired = false;
    if (wrote) {
        char *conf_path = join_path(dir, "btop.conf");
        char *existing = read_file_to_string(conf_path);
        char *base = existing ? existing : xstrdup("");
        char *patched = ini_set(base, "", "color_theme", "\"calm-shell\"");
        wired = overwrite_file(conf_path, patched);
        free(patched);
        free(base);
        free(conf_path);
    }

    add_entry(out, "btop", wrote,
              wrote ? (wired ? xstrdup("btop: theme synced (btop.conf now uses the calm-shell theme)")
                              : xstrdup("btop: wrote calm-shell.theme, but could not update btop.conf"))
                    : xstrdup("btop: failed to write themes/calm-shell.theme"));

    free(theme_path);
    free(accent);
    free(ok_color);
    free(warn_color);
    free(danger_color);
    color_scheme_free(&cs);
    free(themes_subdir);
    free(dir);
}

/* ---- Yazi -------------------------------------------------------------
 * ~/.config/yazi/theme.toml *is* Yazi's whole theme -- there's no
 * separate "pick a theme by name" indirection to sync through the way
 * btop/Helix have, so (like Konsole's .colorscheme file) it's simply
 * fully owned and regenerated. Only the small set of colors that
 * matter across every flavor (a handful of `[flavor]` keys) are set;
 * everything else Yazi defaults on its own. */

static void sync_yazi(const Theme *theme, AppSyncResult *out) {
    char *dir = config_subdir("yazi");
    if (!dir || !mkdir_recursive(dir)) {
        free(dir);
        add_entry(out, "yazi", false, xstrdup("could not create ~/.config/yazi"));
        return;
    }
    TermColorScheme cs;
    color_scheme_build(theme, &cs);
    char *accent = hex_of_semantic(theme, "arrow", 0xCB, 0xA6, 0xF7);
    char *ok_color = hex_of_semantic(theme, "git_clean", 0xA6, 0xE3, 0xA1);
    char *warn_color = hex_of_semantic(theme, "git_dirty", 0xF3, 0x8B, 0xA8);

    StrBuf sb;
    strbuf_init(&sb);
    strbuf_append(&sb, "# Generated by calm-shell (`calm sync`) -- edits here are overwritten\n"
                        "# on the next sync. Change the active Calm-shell theme instead.\n\n[flavor]\n"
                        "use = \"\"\n\n[manager]\n");
    char *chunk = xsprintf("cwd = { fg = \"%s\" }\nhovered = { fg = \"%s\", bg = \"%s\" }\n"
                            "find_keyword = { fg = \"%s\", bold = true }\n"
                            "marker_selected = { fg = \"%s\", bg = \"%s\" }\n"
                            "marker_copied = { fg = \"%s\", bg = \"%s\" }\n"
                            "border_symbol = \"│\"\nborder_style = { fg = \"%s\" }\n\n[status]\n"
                            "separator_open = \"\"\nseparator_close = \"\"\n"
                            "separator_style = { fg = \"%s\", bg = \"%s\" }\n"
                            "mode_normal = { fg = \"%s\", bg = \"%s\", bold = true }\n\n[input]\n"
                            "border = { fg = \"%s\" }\ntitle = {}\nvalue = {}\nselected = { reversed = true }\n",
                            accent, cs.background, accent, warn_color, cs.background, ok_color, cs.background,
                            accent, cs.ansi[8], cs.background, cs.background, accent, cs.background, accent);
    strbuf_append(&sb, chunk);
    free(chunk);

    char *contents = strbuf_take(&sb);
    char *path = join_path(dir, "theme.toml");
    bool wrote = overwrite_file(path, contents);
    add_entry(out, "yazi", wrote,
              wrote ? xstrdup("yazi: theme.toml regenerated")
                    : xstrdup("yazi: failed to write ~/.config/yazi/theme.toml"));

    free(contents);
    free(path);
    free(accent);
    free(ok_color);
    free(warn_color);
    color_scheme_free(&cs);
    free(dir);
}

/* ---- Neovim -----------------------------------------------------------
 * A Lua colorscheme module is a real, loadable Neovim plugin surface:
 * write `~/.config/nvim/lua/calm-shell-theme.lua` (a
 * `vim.api.nvim_set_hl` script) and, once, ensure `init.lua` requires
 * it. If the user's config is Vimscript (`init.vim`) instead, we
 * still write the Lua module -- it's `:luafile`-able by hand -- but
 * only auto-wire the include for `init.lua`, since splicing Lua into
 * an existing Vimscript file isn't a single safe line the way it is
 * for a real init.lua. */

static void sync_neovim(const Theme *theme, AppSyncResult *out) {
    char *dir = config_subdir("nvim");
    char *lua_dir = dir ? join_path(dir, "lua") : NULL;
    if (!dir || !lua_dir || !mkdir_recursive(lua_dir)) {
        free(lua_dir);
        free(dir);
        add_entry(out, "neovim", false, xstrdup("could not create ~/.config/nvim/lua"));
        return;
    }
    TermColorScheme cs;
    color_scheme_build(theme, &cs);
    char *accent = hex_of_semantic(theme, "arrow", 0xCB, 0xA6, 0xF7);
    char *ok_color = hex_of_semantic(theme, "git_clean", 0xA6, 0xE3, 0xA1);
    char *warn_color = hex_of_semantic(theme, "git_dirty", 0xF9, 0xE2, 0xAF);
    char *danger_color = hex_of_semantic(theme, "git_dirty", 0xF3, 0x8B, 0xA8);

    StrBuf sb;
    strbuf_init(&sb);
    strbuf_append(&sb, "-- Generated by calm-shell (`calm sync`) -- edits here are overwritten\n"
                        "-- on the next sync. Change the active Calm-shell theme instead.\n"
                        "local hl = vim.api.nvim_set_hl\n\n");
    char *chunk = xsprintf(
        "hl(0, \"Normal\", { fg = \"%s\", bg = \"%s\" })\n"
        "hl(0, \"CursorLine\", { bg = \"%s\" })\n"
        "hl(0, \"Visual\", { bg = \"%s\" })\n"
        "hl(0, \"Comment\", { fg = \"%s\", italic = true })\n"
        "hl(0, \"Constant\", { fg = \"%s\" })\n"
        "hl(0, \"String\", { fg = \"%s\" })\n"
        "hl(0, \"Function\", { fg = \"%s\", bold = true })\n"
        "hl(0, \"Keyword\", { fg = \"%s\", bold = true })\n"
        "hl(0, \"Type\", { fg = \"%s\" })\n"
        "hl(0, \"Identifier\", { fg = \"%s\" })\n"
        "hl(0, \"DiagnosticError\", { fg = \"%s\" })\n"
        "hl(0, \"DiagnosticWarn\", { fg = \"%s\" })\n"
        "hl(0, \"DiagnosticOk\", { fg = \"%s\" })\n"
        "hl(0, \"StatusLine\", { fg = \"%s\", bg = \"%s\" })\n"
        "hl(0, \"LineNr\", { fg = \"%s\" })\n"
        "hl(0, \"CursorLineNr\", { fg = \"%s\", bold = true })\n",
        cs.foreground, cs.background, cs.ansi[8], cs.ansi[8], cs.ansi[8], cs.ansi[3], cs.ansi[2], accent, danger_color,
        accent, cs.ansi[4], danger_color, warn_color, ok_color, cs.foreground, cs.ansi[8], cs.ansi[8], accent);
    strbuf_append(&sb, chunk);
    free(chunk);

    char *contents = strbuf_take(&sb);
    char *generated_path = join_path(lua_dir, "calm-shell-theme.lua");
    bool wrote = overwrite_file(generated_path, contents);
    free(contents);

    bool wired = false;
    if (wrote) {
        char *init_lua = join_path(dir, "init.lua");
        char *directive = xstrdup("require(\"calm-shell-theme\")\n");
        if (!path_exists(init_lua) || file_contains(init_lua, "calm-shell-theme")) {
            wired = ensure_include_line(init_lua, "calm-shell-theme", directive, "--");
        } else {
            /* An existing init.lua is left untouched by default the
             * same way Alacritty's populated [general] table is --
             * appending a bare `require` is safe Lua, but only once
             * we're sure it isn't landing in the middle of another
             * plugin manager's setup block. */
            char *addition = xsprintf("\n-- Added by calm-shell (`calm sync`) -- loads the generated\n"
                                       "-- calm-shell-theme.lua colorscheme.\n%s",
                                       directive);
            wired = append_to_file(init_lua, addition);
            free(addition);
        }
        free(directive);
        free(init_lua);
    }

    add_entry(out, "neovim", wrote,
              wrote ? (wired ? xstrdup("neovim: theme synced (init.lua now requires calm-shell-theme)")
                              : xstrdup("neovim: wrote lua/calm-shell-theme.lua, but could not update init.lua"))
                    : xstrdup("neovim: failed to write lua/calm-shell-theme.lua"));

    free(generated_path);
    free(accent);
    free(ok_color);
    free(warn_color);
    free(danger_color);
    color_scheme_free(&cs);
    free(lua_dir);
    free(dir);
}

/* ---- Helix --------------------------------------------------------------
 * Helix themes are named files under ~/.config/helix/themes/<name>.toml,
 * selected by `theme = "<name>"` in config.toml -- structurally the
 * same indirection as btop, so the same pattern applies: write the
 * theme file, patch one key. */

static void sync_helix(const Theme *theme, AppSyncResult *out) {
    char *dir = config_subdir("helix");
    char *themes_subdir = dir ? join_path(dir, "themes") : NULL;
    if (!dir || !themes_subdir || !mkdir_recursive(themes_subdir)) {
        free(themes_subdir);
        free(dir);
        add_entry(out, "helix", false, xstrdup("could not create ~/.config/helix/themes"));
        return;
    }
    TermColorScheme cs;
    color_scheme_build(theme, &cs);
    char *accent = hex_of_semantic(theme, "arrow", 0xCB, 0xA6, 0xF7);
    char *ok_color = hex_of_semantic(theme, "git_clean", 0xA6, 0xE3, 0xA1);
    char *warn_color = hex_of_semantic(theme, "git_dirty", 0xF9, 0xE2, 0xAF);
    char *danger_color = hex_of_semantic(theme, "git_dirty", 0xF3, 0x8B, 0xA8);

    StrBuf sb;
    strbuf_init(&sb);
    strbuf_append(&sb, "# Generated by calm-shell (`calm sync`) -- edits here are overwritten\n"
                        "# on the next sync. Change the active Calm-shell theme instead.\n\n");
    char *chunk = xsprintf(
        "\"ui.background\" = { bg = \"%s\" }\n\"ui.text\" = \"%s\"\n"
        "\"ui.cursor\" = { fg = \"%s\", modifiers = [\"reversed\"] }\n"
        "\"ui.selection\" = { bg = \"%s\" }\n\"ui.linenr\" = \"%s\"\n"
        "\"ui.linenr.selected\" = { fg = \"%s\", modifiers = [\"bold\"] }\n"
        "\"ui.statusline\" = { fg = \"%s\", bg = \"%s\" }\n"
        "\"ui.help\" = { fg = \"%s\", bg = \"%s\" }\n"
        "\"ui.virtual\" = \"%s\"\n\"comment\" = { fg = \"%s\", modifiers = [\"italic\"] }\n"
        "\"constant\" = \"%s\"\n\"string\" = \"%s\"\n\"function\" = \"%s\"\n\"keyword\" = \"%s\"\n"
        "\"type\" = \"%s\"\n\"variable\" = \"%s\"\n"
        "\"diagnostic.error\" = { underline = { color = \"%s\", style = \"curl\" } }\n"
        "\"diagnostic.warning\" = { underline = { color = \"%s\", style = \"curl\" } }\n"
        "\"error\" = \"%s\"\n\"warning\" = \"%s\"\n\"info\" = \"%s\"\n",
        cs.background, cs.foreground, cs.background, cs.ansi[8], cs.ansi[8], accent, cs.foreground, cs.ansi[8],
        cs.foreground, cs.ansi[8], cs.ansi[8], cs.ansi[8], cs.ansi[3], cs.ansi[2], accent, danger_color, accent,
        cs.ansi[4], danger_color, warn_color, danger_color, warn_color, ok_color);
    strbuf_append(&sb, chunk);
    free(chunk);

    char *contents = strbuf_take(&sb);
    char *theme_path = join_path(themes_subdir, "calm-shell.toml");
    bool wrote = overwrite_file(theme_path, contents);
    free(contents);

    bool wired = false;
    if (wrote) {
        char *conf_path = join_path(dir, "config.toml");
        char *existing = read_file_to_string(conf_path);
        char *base = existing ? existing : xstrdup("");
        char *patched = ini_set(base, "", "theme", "\"calm-shell\"");
        wired = overwrite_file(conf_path, patched);
        free(patched);
        free(base);
        free(conf_path);
    }

    add_entry(out, "helix", wrote,
              wrote ? (wired ? xstrdup("helix: theme synced (config.toml now uses the calm-shell theme)")
                              : xstrdup("helix: wrote themes/calm-shell.toml, but could not update config.toml"))
                    : xstrdup("helix: failed to write themes/calm-shell.toml"));

    free(theme_path);
    free(accent);
    free(ok_color);
    free(warn_color);
    free(danger_color);
    color_scheme_free(&cs);
    free(themes_subdir);
    free(dir);
}

/* ---- Hyprland -----------------------------------------------------------
 * hyprland.conf supports `source = <path>` (documented, repeatable,
 * resolved at load time) -- the same include shape as Kitty/Foot. */

static void sync_hyprland(const Theme *theme, AppSyncResult *out) {
    char *dir = config_subdir("hypr");
    if (!dir || !mkdir_recursive(dir)) {
        free(dir);
        add_entry(out, "hyprland", false, xstrdup("could not create ~/.config/hypr"));
        return;
    }
    TermColorScheme cs;
    color_scheme_build(theme, &cs);
    char *accent = hex_of_semantic(theme, "arrow", 0xCB, 0xA6, 0xF7);

    StrBuf sb;
    strbuf_init(&sb);
    strbuf_append(&sb, "# Generated by calm-shell (`calm sync`) -- edits here are overwritten\n"
                        "# on the next sync. Change the active Calm-shell theme instead.\n\n"
                        "general {\n");
    char *chunk = xsprintf("    col.active_border = rgb(%s)\n    col.inactive_border = rgb(%s)\n}\n\ndecoration {\n"
                            "    col.shadow = rgba(%s99)\n}\n\ngroup {\n    col.border_active = rgb(%s)\n"
                            "    col.border_inactive = rgb(%s)\n}\n",
                            strip_hash(accent), strip_hash(cs.ansi[8]), strip_hash(cs.background), strip_hash(accent),
                            strip_hash(cs.ansi[8]));
    strbuf_append(&sb, chunk);
    free(chunk);

    char *contents = strbuf_take(&sb);
    char *generated_path = join_path(dir, "calm-shell-theme.conf");
    bool wrote = overwrite_file(generated_path, contents);
    free(contents);

    bool wired = false;
    if (wrote) {
        char *main_conf = join_path(dir, "hyprland.conf");
        char *directive = xsprintf("source = %s\n", generated_path);
        wired = ensure_include_line(main_conf, "calm-shell-theme.conf", directive, "#");
        free(directive);
        free(main_conf);
    }

    add_entry(out, "hyprland", wrote,
              wrote ? (wired
                            ? xstrdup("hyprland: theme synced (hyprland.conf now sources calm-shell-theme.conf)")
                            : xstrdup("hyprland: wrote calm-shell-theme.conf, but could not update hyprland.conf"))
                    : xstrdup("hyprland: failed to write calm-shell-theme.conf"));

    free(generated_path);
    free(accent);
    color_scheme_free(&cs);
    free(dir);
}

/* ---- Waybar / Wofi --------------------------------------------------
 * Both are GTK-CSS-styled; both support the standard CSS `@import`
 * at-rule, so both get the exact same treatment as Kitty's `include`
 * -- write a companion stylesheet of just color variables, ensure one
 * @import line. Shared implementation parameterized by app name, since
 * the two are otherwise identical. */

static void sync_gtk_css_app(const Theme *theme, const char *app, AppSyncResult *out) {
    char *dir = config_subdir(app);
    if (!dir || !mkdir_recursive(dir)) {
        free(dir);
        char *detail = xsprintf("could not create ~/.config/%s", app);
        add_entry(out, app, false, detail);
        return;
    }
    TermColorScheme cs;
    color_scheme_build(theme, &cs);
    char *accent = hex_of_semantic(theme, "arrow", 0xCB, 0xA6, 0xF7);
    char *ok_color = hex_of_semantic(theme, "git_clean", 0xA6, 0xE3, 0xA1);
    char *danger_color = hex_of_semantic(theme, "git_dirty", 0xF3, 0x8B, 0xA8);

    StrBuf sb;
    strbuf_init(&sb);
    strbuf_append(&sb, "/* Generated by calm-shell (`calm sync`) -- edits here are overwritten\n"
                        " * on the next sync. Change the active Calm-shell theme instead. */\n\n@define-color "
                        "background ");
    char *chunk = xsprintf("%s;\n@define-color foreground %s;\n@define-color accent %s;\n"
                            "@define-color ok %s;\n@define-color danger %s;\n@define-color subtle %s;\n",
                            cs.background, cs.foreground, accent, ok_color, danger_color, cs.ansi[8]);
    strbuf_append(&sb, chunk);
    free(chunk);

    char *contents = strbuf_take(&sb);
    char *generated_path = join_path(dir, "calm-shell-theme.css");
    bool wrote = overwrite_file(generated_path, contents);
    free(contents);

    bool wired = false;
    if (wrote) {
        char *style_path = join_path(dir, "style.css");
        if (file_contains(style_path, "calm-shell-theme.css")) {
            wired = true;
        } else if (!path_exists(style_path)) {
            wired = overwrite_file(style_path, "@import \"calm-shell-theme.css\";\n");
        } else {
            /* CSS's only comment syntax is the block form (slash-star
             * ... star-slash) -- rather than reuse the
             * single-line-comment-shaped ensure_include_line() helper,
             * append the @import with a proper block comment directly. */
            char *addition = xstrdup("\n/* Added by calm-shell (`calm sync`) -- keeps this app's theme\n"
                                       "   matched to Calm-shell's active theme. */\n"
                                       "@import \"calm-shell-theme.css\";\n");
            wired = append_to_file(style_path, addition);
            free(addition);
        }
        free(style_path);
    }

    char *detail;
    if (!wrote) {
        detail = xsprintf("%s: failed to write calm-shell-theme.css", app);
    } else if (wired) {
        detail = xsprintf("%s: theme synced (style.css now imports calm-shell-theme.css)", app);
    } else {
        detail = xsprintf("%s: wrote calm-shell-theme.css, but could not update style.css", app);
    }
    add_entry(out, app, wrote, detail);

    free(generated_path);
    free(accent);
    free(ok_color);
    free(danger_color);
    color_scheme_free(&cs);
    free(dir);
}

/* ---- Fuzzel ---------------------------------------------------------
 * fuzzel.ini is a flat INI with no include directive
 * (https://man.archlinux.org/man/fuzzel.ini.5) -- same key-patch
 * treatment as Cava. */

static void sync_fuzzel(const Theme *theme, AppSyncResult *out) {
    char *dir = config_subdir("fuzzel");
    if (!dir || !mkdir_recursive(dir)) {
        free(dir);
        add_entry(out, "fuzzel", false, xstrdup("could not create ~/.config/fuzzel"));
        return;
    }
    TermColorScheme cs;
    color_scheme_build(theme, &cs);
    char *accent = hex_of_semantic(theme, "arrow", 0xCB, 0xA6, 0xF7);

    /* fuzzel.ini colors are 8-digit RRGGBBAA hex, no leading '#'. */
    char *background = xsprintf("%sff", strip_hash(cs.background));
    char *text = xsprintf("%sff", strip_hash(cs.foreground));
    char *match = xsprintf("%sff", strip_hash(accent));
    char *selection = xsprintf("%sff", strip_hash(cs.ansi[8]));
    char *border = xsprintf("%sff", strip_hash(accent));

    char *path = join_path(dir, "fuzzel.ini");
    char *existing = read_file_to_string(path);
    char *content = existing ? xstrdup(existing) : xstrdup("[main]\nfont=monospace:size=12\n");
    char *step;
    step = ini_set(content, "colors", "background", background);
    free(content);
    content = step;
    step = ini_set(content, "colors", "text", text);
    free(content);
    content = step;
    step = ini_set(content, "colors", "match", match);
    free(content);
    content = step;
    step = ini_set(content, "colors", "selection", selection);
    free(content);
    content = step;
    step = ini_set(content, "colors", "selection-text", text);
    free(content);
    content = step;
    step = ini_set(content, "colors", "border", border);
    free(content);
    content = step;

    bool wrote = overwrite_file(path, content);
    add_entry(out, "fuzzel", wrote,
              wrote ? xstrdup("fuzzel: [colors] keys in fuzzel.ini updated in place")
                    : xstrdup("fuzzel: failed to write ~/.config/fuzzel/fuzzel.ini"));

    free(content);
    free(existing);
    free(path);
    free(background);
    free(text);
    free(match);
    free(selection);
    free(border);
    free(accent);
    color_scheme_free(&cs);
    free(dir);
}

void app_sync_write_all(const Theme *theme, AppSyncResult *out) {
    sync_cava(theme, out);
    sync_btop(theme, out);
    sync_yazi(theme, out);
    sync_neovim(theme, out);
    sync_helix(theme, out);
    sync_hyprland(theme, out);
    sync_gtk_css_app(theme, "waybar", out);
    sync_gtk_css_app(theme, "wofi", out);
    sync_fuzzel(theme, out);
}
