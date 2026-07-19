#include "term/edit_buffer.h"

#include "test_framework.h"

static void test_insert_and_delete(int *run, int *failed) {
    EditBuf b;
    editbuf_init(&b);
    editbuf_insert(&b, 'h');
    editbuf_insert(&b, 'i');
    CHECK_STR_EQ(run, failed, b.data, "hi");
    CHECK(run, failed, b.cursor == 2);

    editbuf_delete_before_cursor(&b);
    CHECK_STR_EQ(run, failed, b.data, "h");

    editbuf_set(&b, "hello world");
    b.cursor = 5; /* just after "hello" */
    editbuf_kill_word_before(&b);
    CHECK_STR_EQ(run, failed, b.data, " world");

    editbuf_set(&b, "hello world");
    b.cursor = 5;
    editbuf_kill_to_end(&b);
    CHECK_STR_EQ(run, failed, b.data, "hello");

    editbuf_set(&b, "anything");
    editbuf_kill_whole_line(&b);
    CHECK_STR_EQ(run, failed, b.data, "");
    CHECK(run, failed, b.cursor == 0);

    editbuf_free(&b);
}

void run_edit_buffer_tests(int *run, int *failed) {
    test_insert_and_delete(run, failed);
}
