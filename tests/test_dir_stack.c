#include "shell/dir_stack.h"

#include <stdlib.h>

#include "test_framework.h"

static void test_push_pop(int *run, int *failed) {
    DirStack s;
    dir_stack_init(&s, 0);
    dir_stack_push(&s, "/a");
    dir_stack_push(&s, "/b");
    CHECK(run, failed, s.count == 2);

    char *top = dir_stack_pop(&s);
    CHECK_STR_EQ(run, failed, top, "/b");
    free(top);
    CHECK(run, failed, s.count == 1);

    char *next = dir_stack_pop(&s);
    CHECK_STR_EQ(run, failed, next, "/a");
    free(next);

    CHECK(run, failed, dir_stack_pop(&s) == NULL);
    dir_stack_free(&s);
}

static void test_max_size_drops_oldest(int *run, int *failed) {
    DirStack s;
    dir_stack_init(&s, 2);
    dir_stack_push(&s, "/a");
    dir_stack_push(&s, "/b");
    dir_stack_push(&s, "/c"); /* should drop "/a" */
    CHECK(run, failed, s.count == 2);
    CHECK_STR_EQ(run, failed, s.dirs[0], "/b");
    CHECK_STR_EQ(run, failed, s.dirs[1], "/c");
    dir_stack_free(&s);
}

void run_dir_stack_tests(int *run, int *failed) {
    test_push_pop(run, failed);
    test_max_size_drops_oldest(run, failed);
}
