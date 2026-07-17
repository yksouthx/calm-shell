/* A small, dependency-free test runner (no framework needed for a
 * project this size). Each TEST(...) registers itself; main() runs
 * them all and reports a pass/fail summary, exiting non-zero on any
 * failure so `make test` fails the build the way `cargo test` did. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "calm_format.h"
#include "config.h"
#include "json.h"
#include "theme.h"
#include "util.h"

static int g_tests_run = 0;
static int g_tests_failed = 0;
static const char *g_current_test = NULL;

#define TEST(name) static void test_##name(void)
#define RUN(name)                    \
    do {                             \
        g_current_test = #name;      \
        g_tests_run++;               \
        test_##name();               \
    } while (0)

#define CHECK(cond)                                                                    \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            fprintf(stderr, "FAIL %s: %s:%d: %s\n", g_current_test, __FILE__, __LINE__, #cond); \
            g_tests_failed++;                                                           \
        }                                                                               \
    } while (0)

#define CHECK_STR_EQ(a, b)                                                                                 \
    do {                                                                                                    \
        const char *_a = (a);                                                                               \
        const char *_b = (b);                                                                                \
        if (_a == NULL || _b == NULL || strcmp(_a, _b) != 0) {                                               \
            fprintf(stderr, "FAIL %s: %s:%d: \"%s\" != \"%s\"\n", g_current_test, __FILE__, __LINE__,        \
                     _a ? _a : "(null)", _b ? _b : "(null)");                                                 \
            g_tests_failed++;                                                                                \
        }                                                                                                    \
    } while (0)

/* --- calm_format tests ----------------------------------------------- */

TEST(parses_sections_and_scalars) {
    CalmDocument doc;
    char *err = NULL;
    bool ok = calm_parse("\n[shell]\ntheme = \"calm-lavender\"\ngreeting = true\nretries = 3\n", &doc, &err);
    CHECK(ok);
    CHECK_STR_EQ(calm_document_get_str(&doc, "shell", "theme"), "calm-lavender");
    CHECK(calm_document_get_bool(&doc, "shell", "greeting", false) == true);
    CHECK(calm_document_get_int(&doc, "shell", "retries", -1) == 3);
    calm_document_free(&doc);
}

TEST(parses_lists) {
    CalmDocument doc;
    char *err = NULL;
    bool ok = calm_parse("tags = [\"fast\", \"quiet\"]\n", &doc, &err);
    CHECK(ok);
    const CalmValue *v = calm_document_get(&doc, "", "tags");
    CHECK(v != NULL && v->type == CALM_LIST);
    CHECK(v->item_count == 2);
    CHECK_STR_EQ(v->items[0].str, "fast");
    CHECK_STR_EQ(v->items[1].str, "quiet");
    calm_document_free(&doc);
}

TEST(bare_unquoted_word_is_a_string) {
    CalmDocument doc;
    char *err = NULL;
    CHECK(calm_parse("theme = calm-lavender\n", &doc, &err));
    CHECK_STR_EQ(calm_document_get_str(&doc, "", "theme"), "calm-lavender");
    calm_document_free(&doc);
}

TEST(comments_and_blank_lines_are_skipped) {
    CalmDocument doc;
    char *err = NULL;
    CHECK(calm_parse("# a comment\n\n[shell]\n# another\ngreeting = true\n", &doc, &err));
    CHECK(calm_document_get_bool(&doc, "shell", "greeting", false) == true);
    calm_document_free(&doc);
}

TEST(multiline_triple_quoted_block) {
    CalmDocument doc;
    char *err = NULL;
    CHECK(calm_parse("[functions]\nmkcd = \"\"\"\nmkdir -p \"$1\" && cd \"$1\"\n\"\"\"\n", &doc, &err));
    CHECK_STR_EQ(calm_document_get_str(&doc, "functions", "mkcd"), "mkdir -p \"$1\" && cd \"$1\"");
    calm_document_free(&doc);
}

TEST(single_line_triple_quoted_block) {
    CalmDocument doc;
    char *err = NULL;
    CHECK(calm_parse("greeting = \"\"\"hello\"\"\"\n", &doc, &err));
    CHECK_STR_EQ(calm_document_get_str(&doc, "", "greeting"), "hello");
    calm_document_free(&doc);
}

TEST(unterminated_triple_quote_errors) {
    CalmDocument doc;
    char *err = NULL;
    bool ok = calm_parse("body = \"\"\"\nunterminated\n", &doc, &err);
    CHECK(!ok);
    free(err);
}

TEST(later_section_wins_on_duplicate_key) {
    CalmDocument doc;
    char *err = NULL;
    CHECK(calm_parse("[a]\nx = 1\n[a]\nx = 2\n", &doc, &err));
    CHECK(calm_document_get_int(&doc, "a", "x", -1) == 2);
    calm_document_free(&doc);
}

TEST(malformed_line_without_equals_errors) {
    CalmDocument doc;
    char *err = NULL;
    bool ok = calm_parse("[shell]\nthis is not valid\n", &doc, &err);
    CHECK(!ok);
    free(err);
}

TEST(include_resolves_relative_to_including_file_and_merges) {
    char dir[256];
    snprintf(dir, sizeof(dir), "/tmp/calm-shell-ctest-%d", (int)getpid());
    mkdir(dir, 0755);
    char main_path[300], inc_path[300];
    snprintf(main_path, sizeof(main_path), "%s/main.calm", dir);
    snprintf(inc_path, sizeof(inc_path), "%s/aliases.calm", dir);

    FILE *f = fopen(inc_path, "w");
    fputs("[aliases]\nll = \"ls -la\"\n", f);
    fclose(f);
    f = fopen(main_path, "w");
    fputs("[shell]\ntheme = \"calm-lavender\"\ninclude \"aliases.calm\"\n", f);
    fclose(f);

    CalmDocument doc;
    char *err = NULL;
    CHECK(calm_parse_file(main_path, &doc, &err));
    CHECK_STR_EQ(calm_document_get_str(&doc, "shell", "theme"), "calm-lavender");
    CHECK_STR_EQ(calm_document_get_str(&doc, "aliases", "ll"), "ls -la");
    calm_document_free(&doc);

    unlink(main_path);
    unlink(inc_path);
    rmdir(dir);
}

TEST(include_cycle_is_bounded_not_infinite) {
    char dir[256];
    snprintf(dir, sizeof(dir), "/tmp/calm-shell-ctest-cycle-%d", (int)getpid());
    mkdir(dir, 0755);
    char a_path[300], b_path[300];
    snprintf(a_path, sizeof(a_path), "%s/a.calm", dir);
    snprintf(b_path, sizeof(b_path), "%s/b.calm", dir);

    FILE *f = fopen(a_path, "w");
    fputs("include \"b.calm\"\n", f);
    fclose(f);
    f = fopen(b_path, "w");
    fputs("include \"a.calm\"\n", f);
    fclose(f);

    CalmDocument doc;
    char *err = NULL;
    bool ok = calm_parse_file(a_path, &doc, &err);
    CHECK(!ok);
    free(err);

    unlink(a_path);
    unlink(b_path);
    rmdir(dir);
}

TEST(expand_handles_home_and_env_vars) {
    setenv("CALM_TEST_VAR", "value", 1);
    char *a = calm_expand("$CALM_TEST_VAR");
    CHECK_STR_EQ(a, "value");
    free(a);
    char *b = calm_expand("${CALM_TEST_VAR}");
    CHECK_STR_EQ(b, "value");
    free(b);
    char *c = calm_expand("no vars here");
    CHECK_STR_EQ(c, "no vars here");
    free(c);
    char *d = calm_expand("$");
    CHECK_STR_EQ(d, "$");
    free(d);
}

TEST(expand_tilde_prefixes_home) {
    char *home = home_dir();
    if (home) {
        char *expanded = calm_expand("~/Projects");
        char *want = xsprintf("%s/Projects", home);
        CHECK_STR_EQ(expanded, want);
        free(expanded);
        free(want);
        free(home);
    }
}

TEST(quoted_keys_have_their_quotes_stripped) {
    CalmDocument doc;
    char *err = NULL;
    CHECK(calm_parse("[directory.aliases]\n\"..\" = \"cd ..\"\n\"~dl\" = \"cd ~/Downloads\"\n", &doc, &err));
    CHECK_STR_EQ(calm_document_get_str(&doc, "directory.aliases", ".."), "cd ..");
    CHECK_STR_EQ(calm_document_get_str(&doc, "directory.aliases", "~dl"), "cd ~/Downloads");
    CHECK(calm_document_get_str(&doc, "directory.aliases", "\"..\"") == NULL);
    calm_document_free(&doc);
}

TEST(unquoted_keys_are_unaffected) {
    CalmDocument doc;
    char *err = NULL;
    CHECK(calm_parse("[aliases]\nll = \"ls -la\"\n", &doc, &err));
    CHECK_STR_EQ(calm_document_get_str(&doc, "aliases", "ll"), "ls -la");
    calm_document_free(&doc);
}

TEST(shipped_examples_all_parse_cleanly) {
    DIR *d = opendir("examples");
    CHECK(d != NULL);
    if (!d) {
        return;
    }
    int checked = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (!ends_with(entry->d_name, ".calm")) {
            continue;
        }
        char path[512];
        snprintf(path, sizeof(path), "examples/%s", entry->d_name);
        char *contents = read_file_to_string(path);
        CHECK(contents != NULL);
        if (!contents) {
            continue;
        }
        CalmDocument doc;
        char *err = NULL;
        bool ok = calm_parse(contents, &doc, &err);
        if (!ok) {
            fprintf(stderr, "FAIL %s: examples/%s failed to parse: %s\n", g_current_test, entry->d_name,
                     err ? err : "?");
            g_tests_failed++;
        } else {
            calm_document_free(&doc);
        }
        free(err);
        free(contents);
        checked++;
    }
    closedir(d);
    CHECK(checked > 0);
}

/* --- json tests --------------------------------------------------------- */

TEST(json_parses_flat_object) {
    char *err = NULL;
    JsonValue *v = json_parse("{\"a\": \"x\", \"b\": true, \"c\": 3}", &err);
    CHECK(v != NULL);
    CHECK_STR_EQ(json_as_string(json_object_get(v, "a")), "x");
    CHECK(json_object_get(v, "b")->boolean == true);
    CHECK(json_object_get(v, "c")->number == 3);
    json_free(v);
}

TEST(json_parses_nested_object) {
    char *err = NULL;
    JsonValue *v = json_parse("{\"outer\": {\"inner\": \"deep\"}}", &err);
    CHECK(v != NULL);
    const JsonValue *outer = json_object_get(v, "outer");
    CHECK(outer != NULL);
    CHECK_STR_EQ(json_as_string(json_object_get(outer, "inner")), "deep");
    json_free(v);
}

TEST(json_rejects_trailing_garbage) {
    char *err = NULL;
    JsonValue *v = json_parse("{}garbage", &err);
    CHECK(v == NULL);
    free(err);
}

/* --- theme tests --------------------------------------------------------- */

static const char *REQUIRED_DIRECT_PALETTE_KEYS[] = {"calm_purple", "cloud_gray",  "gentle_pink",
                                                       "soft_blue",   "soft_red",    "warm_yellow"};

TEST(bundled_default_theme_has_all_required_keys_and_valid_hex) {
    char *err = NULL;
    JsonValue *root = json_parse(default_lavender_theme(), &err);
    CHECK(root != NULL);
    const JsonValue *colors = json_object_get(root, "colors");
    CHECK(colors != NULL);
    for (size_t i = 0; i < sizeof(REQUIRED_DIRECT_PALETTE_KEYS) / sizeof(REQUIRED_DIRECT_PALETTE_KEYS[0]); i++) {
        const JsonValue *c = json_object_get(colors, REQUIRED_DIRECT_PALETTE_KEYS[i]);
        CHECK(c != NULL);
    }
    json_free(root);
}

/* --- util tests ----------------------------------------------------------- */

TEST(trim_in_place_strips_both_ends) {
    char buf[] = "   hello world  \t\n";
    trim_in_place(buf);
    CHECK_STR_EQ(buf, "hello world");
}

TEST(starts_with_and_ends_with) {
    CHECK(starts_with("hello world", "hello"));
    CHECK(!starts_with("hello", "hello world"));
    CHECK(ends_with("theme.json", ".json"));
    CHECK(!ends_with("theme.json", ".calm"));
}

TEST(xsprintf_formats_correctly) {
    char *s = xsprintf("%s-%d", "calm", 5);
    CHECK_STR_EQ(s, "calm-5");
    free(s);
}

TEST(split_lines_handles_crlf_and_no_trailing_newline) {
    size_t count = 0;
    char **lines = split_lines("a\r\nb\nc", &count);
    CHECK(count == 3);
    CHECK_STR_EQ(lines[0], "a");
    CHECK_STR_EQ(lines[1], "b");
    CHECK_STR_EQ(lines[2], "c");
    free_lines(lines, count);
}

int main(void) {
    RUN(parses_sections_and_scalars);
    RUN(parses_lists);
    RUN(bare_unquoted_word_is_a_string);
    RUN(comments_and_blank_lines_are_skipped);
    RUN(multiline_triple_quoted_block);
    RUN(single_line_triple_quoted_block);
    RUN(unterminated_triple_quote_errors);
    RUN(later_section_wins_on_duplicate_key);
    RUN(malformed_line_without_equals_errors);
    RUN(include_resolves_relative_to_including_file_and_merges);
    RUN(include_cycle_is_bounded_not_infinite);
    RUN(expand_handles_home_and_env_vars);
    RUN(expand_tilde_prefixes_home);
    RUN(quoted_keys_have_their_quotes_stripped);
    RUN(unquoted_keys_are_unaffected);
    RUN(shipped_examples_all_parse_cleanly);
    RUN(json_parses_flat_object);
    RUN(json_parses_nested_object);
    RUN(json_rejects_trailing_garbage);
    RUN(bundled_default_theme_has_all_required_keys_and_valid_hex);
    RUN(trim_in_place_strips_both_ends);
    RUN(starts_with_and_ends_with);
    RUN(xsprintf_formats_correctly);
    RUN(split_lines_handles_crlf_and_no_trailing_newline);

    if (g_tests_failed == 0) {
        printf("test result: ok. %d passed; 0 failed\n", g_tests_run);
        return 0;
    }
    printf("test result: FAILED. %d run; %d failed\n", g_tests_run, g_tests_failed);
    return 1;
}
