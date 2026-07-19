#include "config/calmconf.h"

#include <stdlib.h>
#include <string.h>

#include "test_framework.h"
#include "util/fsutil.h"
#include "util/xalloc.h"

static void expect_parse_ok(int *run, int *failed, const char *src, CalmDocument *out) {
    char *err = NULL;
    bool ok = calm_parse(src, out, &err);
    CHECK(run, failed, ok);
    if (!ok) {
        fprintf(stderr, "  parse error: %s\n", err ? err : "(none)");
    }
    free(err);
}

static void test_scalars(int *run, int *failed) {
    CalmDocument doc;
    expect_parse_ok(run, failed, "[shell]\n"
                                  "theme = \"calm-lavender\"\n"
                                  "greeting = true\n"
                                  "quiet = false\n"
                                  "retries = 3\n"
                                  "negative = -7\n"
                                  "timeout = 1.5\n"
                                  "bare = hello\n",
                    &doc);

    CHECK_STR_EQ(run, failed, calm_document_get_str(&doc, "shell", "theme"), "calm-lavender");
    CHECK(run, failed, calm_document_get_bool(&doc, "shell", "greeting", false) == true);
    CHECK(run, failed, calm_document_get_bool(&doc, "shell", "quiet", true) == false);
    CHECK(run, failed, calm_document_get_int(&doc, "shell", "retries", -1) == 3);
    CHECK(run, failed, calm_document_get_int(&doc, "shell", "negative", 0) == -7);
    CHECK(run, failed, calm_document_get_float(&doc, "shell", "timeout", 0.0) == 1.5);
    CHECK_STR_EQ(run, failed, calm_document_get_str(&doc, "shell", "bare"), "hello");

    /* An int is also usable as a float, but not vice versa. */
    CHECK(run, failed, calm_document_get_float(&doc, "shell", "retries", 0.0) == 3.0);
    CHECK(run, failed, calm_document_get_int(&doc, "shell", "timeout", -99) == -99);

    calm_document_free(&doc);
}

static void test_comments(int *run, int *failed) {
    CalmDocument doc;
    expect_parse_ok(run, failed,
                     "# a whole-line comment\n"
                     "[colors] # trailing comment on a section\n"
                     "hex = \"#CBA6F7\"  # trailing comment, must not eat the quoted value\n"
                     "  # indented comment line\n"
                     "note = \"contains a literal # character\"\n",
                     &doc);

    CHECK_STR_EQ(run, failed, calm_document_get_str(&doc, "colors", "hex"), "#CBA6F7");
    CHECK_STR_EQ(run, failed, calm_document_get_str(&doc, "colors", "note"), "contains a literal # character");

    calm_document_free(&doc);
}

static void test_lists(int *run, int *failed) {
    CalmDocument doc;
    expect_parse_ok(run, failed, "[shell]\ntags = [\"fast\", \"quiet\", 3, true]\n", &doc);

    const CalmValue *v = calm_document_get(&doc, "shell", "tags");
    CHECK(run, failed, v != NULL && v->type == CALM_LIST);
    if (v && v->type == CALM_LIST) {
        CHECK(run, failed, v->as.list.count == 4);
        CHECK(run, failed, v->as.list.items[0].type == CALM_STRING);
        CHECK_STR_EQ(run, failed, v->as.list.items[0].as.string, "fast");
        CHECK(run, failed, v->as.list.items[2].type == CALM_INT && v->as.list.items[2].as.integer == 3);
        CHECK(run, failed, v->as.list.items[3].type == CALM_BOOL && v->as.list.items[3].as.boolean == true);
    }

    calm_document_free(&doc);
}

static void test_dotted_sections_and_quoted_keys(int *run, int *failed) {
    CalmDocument doc;
    expect_parse_ok(run, failed, "[directory.aliases]\n"
                                  "\"..\" = \"cd ..\"\n"
                                  "\"ctrl+r\" = \"history-search-backward\"\n",
                    &doc);

    CHECK_STR_EQ(run, failed, calm_document_get_str(&doc, "directory.aliases", ".."), "cd ..");
    CHECK_STR_EQ(run, failed, calm_document_get_str(&doc, "directory.aliases", "ctrl+r"), "history-search-backward");

    calm_document_free(&doc);
}

static void test_triple_quoted_block(int *run, int *failed) {
    CalmDocument doc;
    expect_parse_ok(run, failed, "[functions]\n"
                                  "mkcd = \"\"\"\n"
                                  "mkdir -p \"$1\" && cd \"$1\"  # keeps a literal comment char\n"
                                  "echo done\n"
                                  "\"\"\"\n",
                    &doc);

    const char *body = calm_document_get_str(&doc, "functions", "mkcd");
    CHECK(run, failed, body != NULL);
    if (body) {
        CHECK(run, failed, strstr(body, "mkdir -p \"$1\" && cd \"$1\"  # keeps a literal comment char") != NULL);
        CHECK(run, failed, strstr(body, "echo done") != NULL);
    }

    calm_document_free(&doc);
}

static void test_single_line_triple_quote(int *run, int *failed) {
    CalmDocument doc;
    expect_parse_ok(run, failed, "[functions]\nnote = \"\"\"one liner\"\"\"\n", &doc);
    CHECK_STR_EQ(run, failed, calm_document_get_str(&doc, "functions", "note"), "one liner");
    calm_document_free(&doc);
}

static void test_unterminated_triple_quote_errors(int *run, int *failed) {
    CalmDocument doc;
    char *err = NULL;
    bool ok = calm_parse("[functions]\nbroken = \"\"\"\nno closing marker\n", &doc, &err);
    CHECK(run, failed, !ok);
    CHECK(run, failed, err != NULL);
    free(err);
}

static void test_missing_equals_errors(int *run, int *failed) {
    CalmDocument doc;
    char *err = NULL;
    bool ok = calm_parse("[shell]\nthis is not key value\n", &doc, &err);
    CHECK(run, failed, !ok);
    free(err);
}

static void test_later_definition_wins(int *run, int *failed) {
    CalmDocument doc;
    expect_parse_ok(run, failed, "[shell]\ntheme = \"a\"\ntheme = \"b\"\n", &doc);
    CHECK_STR_EQ(run, failed, calm_document_get_str(&doc, "shell", "theme"), "b");
    calm_document_free(&doc);
}

static void test_expand(int *run, int *failed) {
    setenv("CALM_TEST_VAR", "hello", 1);
    char *expanded = calm_expand("$CALM_TEST_VAR world ${CALM_TEST_VAR}!");
    CHECK_STR_EQ(run, failed, expanded, "hello world hello!");
    free(expanded);
}

static void write_file(const char *path, const char *contents) {
    FILE *f = fopen(path, "w");
    fputs(contents, f);
    fclose(f);
}

static void test_include(int *run, int *failed) {
    char tmpdir_template[] = "/tmp/calm_test_XXXXXX";
    char *tmpdir = mkdtemp(tmpdir_template);
    CHECK(run, failed, tmpdir != NULL);
    if (!tmpdir) {
        return;
    }

    char *main_path = join_path(tmpdir, "main.calm");
    char *included_path = join_path(tmpdir, "included.calm");
    write_file(included_path, "[aliases]\nll = \"ls -la\"\n");
    write_file(main_path, "[shell]\ntheme = \"x\"\ninclude \"included.calm\"\n");

    CalmDocument doc;
    char *err = NULL;
    bool ok = calm_parse_file(main_path, &doc, &err);
    CHECK(run, failed, ok);
    if (ok) {
        CHECK_STR_EQ(run, failed, calm_document_get_str(&doc, "aliases", "ll"), "ls -la");
        calm_document_free(&doc);
    } else {
        fprintf(stderr, "  include parse error: %s\n", err ? err : "(none)");
    }
    free(err);
    free(main_path);
    free(included_path);
}

static void test_include_cycle_is_a_noop_not_a_crash(int *run, int *failed) {
    char tmpdir_template[] = "/tmp/calm_test_XXXXXX";
    char *tmpdir = mkdtemp(tmpdir_template);
    CHECK(run, failed, tmpdir != NULL);
    if (!tmpdir) {
        return;
    }

    char *a_path = join_path(tmpdir, "a.calm");
    char *b_path = join_path(tmpdir, "b.calm");
    write_file(a_path, "[shell]\nfrom_a = \"1\"\ninclude \"b.calm\"\n");
    write_file(b_path, "[shell]\nfrom_b = \"1\"\ninclude \"a.calm\"\n");

    CalmDocument doc;
    char *err = NULL;
    bool ok = calm_parse_file(a_path, &doc, &err);
    CHECK(run, failed, ok);
    if (ok) {
        CHECK_STR_EQ(run, failed, calm_document_get_str(&doc, "shell", "from_a"), "1");
        CHECK_STR_EQ(run, failed, calm_document_get_str(&doc, "shell", "from_b"), "1");
        calm_document_free(&doc);
    }
    free(err);
    free(a_path);
    free(b_path);
}

void run_calmconf_tests(int *run, int *failed) {
    test_scalars(run, failed);
    test_comments(run, failed);
    test_lists(run, failed);
    test_dotted_sections_and_quoted_keys(run, failed);
    test_triple_quoted_block(run, failed);
    test_single_line_triple_quote(run, failed);
    test_unterminated_triple_quote_errors(run, failed);
    test_missing_equals_errors(run, failed);
    test_later_definition_wins(run, failed);
    test_expand(run, failed);
    test_include(run, failed);
    test_include_cycle_is_a_noop_not_a_crash(run, failed);
}
