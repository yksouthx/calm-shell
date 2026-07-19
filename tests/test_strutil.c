#include "util/strutil.h"

#include <stdlib.h>
#include <string.h>

#include "test_framework.h"
#include "util/xalloc.h"

static void test_starts_ends_with(int *run, int *failed) {
    CHECK(run, failed, starts_with("hello world", "hello"));
    CHECK(run, failed, !starts_with("hello world", "world"));
    CHECK(run, failed, ends_with("hello world", "world"));
    CHECK(run, failed, !ends_with("hello world", "hello"));
    CHECK(run, failed, starts_with("", ""));
}

static void test_is_blank(int *run, int *failed) {
    CHECK(run, failed, is_blank(""));
    CHECK(run, failed, is_blank("   \t\n"));
    CHECK(run, failed, !is_blank("  x  "));
}

static void test_trim(int *run, int *failed) {
    char s1[] = "   hello   ";
    trim_in_place(s1);
    CHECK_STR_EQ(run, failed, s1, "hello");

    char s2[] = "hello   ";
    rtrim_in_place(s2);
    CHECK_STR_EQ(run, failed, s2, "hello");

    char s3[] = "   ";
    trim_in_place(s3);
    CHECK_STR_EQ(run, failed, s3, "");
}

static void test_split_lines(int *run, int *failed) {
    size_t count = 0;
    char **lines = split_lines("a\nb\r\nc\n", &count);
    CHECK(run, failed, count == 3);
    if (count == 3) {
        CHECK_STR_EQ(run, failed, lines[0], "a");
        CHECK_STR_EQ(run, failed, lines[1], "b");
        CHECK_STR_EQ(run, failed, lines[2], "c");
    }
    free_lines(lines, count);

    size_t empty_count = 0;
    char **empty_lines = split_lines("", &empty_count);
    CHECK(run, failed, empty_count == 0);
    free_lines(empty_lines, empty_count);
}

void run_strutil_tests(int *run, int *failed) {
    test_starts_ends_with(run, failed);
    test_is_blank(run, failed);
    test_trim(run, failed);
    test_split_lines(run, failed);
}
