#include "shell/execute.h"

#include "test_framework.h"

static void test_needs_full_shell(int *run, int *failed) {
    CHECK(run, failed, !execute_needs_full_shell("cd /tmp"));
    CHECK(run, failed, !execute_needs_full_shell("ls -la"));
    CHECK(run, failed, execute_needs_full_shell("cd /tmp && ls"));
    CHECK(run, failed, execute_needs_full_shell("ls | grep foo"));
    CHECK(run, failed, execute_needs_full_shell("echo hi; echo bye"));
    CHECK(run, failed, execute_needs_full_shell("echo $HOME"));
    CHECK(run, failed, execute_needs_full_shell("cat file.txt > out.txt"));

    /* Shell metacharacters inside quotes are literal arguments, not
     * operators -- a quoted `cd "foo;bar"` is still a single-target
     * cd and should stay on the builtin fast path. */
    CHECK(run, failed, !execute_needs_full_shell("cd \"foo;bar\""));
    CHECK(run, failed, !execute_needs_full_shell("echo 'a && b'"));
}

void run_execute_tests(int *run, int *failed) {
    test_needs_full_shell(run, failed);
}
