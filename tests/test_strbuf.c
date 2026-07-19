#include "util/strbuf.h"

#include <stdlib.h>

#include "test_framework.h"

static void test_append_and_take(int *run, int *failed) {
    StrBuf sb;
    strbuf_init(&sb);
    strbuf_append(&sb, "hello");
    strbuf_append_char(&sb, ' ');
    strbuf_append_n(&sb, "world!!!", 5);
    CHECK_STR_EQ(run, failed, sb.data, "hello world");
    CHECK(run, failed, sb.len == 11);

    char *taken = strbuf_take(&sb);
    CHECK_STR_EQ(run, failed, taken, "hello world");
    /* strbuf_take() must leave the buffer usable, empty, and freeable. */
    CHECK(run, failed, sb.len == 0);
    strbuf_append(&sb, "second use");
    CHECK_STR_EQ(run, failed, sb.data, "second use");

    free(taken);
    strbuf_free(&sb);
}

static void test_growth_beyond_initial_capacity(int *run, int *failed) {
    StrBuf sb;
    strbuf_init(&sb);
    for (int i = 0; i < 1000; i++) {
        strbuf_append(&sb, "0123456789");
    }
    CHECK(run, failed, sb.len == 10000);
    strbuf_free(&sb);
}

void run_strbuf_tests(int *run, int *failed) {
    test_append_and_take(run, failed);
    test_growth_beyond_initial_capacity(run, failed);
}
