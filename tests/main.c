#include <stdio.h>

void run_calmconf_tests(int *run, int *failed);
void run_strutil_tests(int *run, int *failed);
void run_strbuf_tests(int *run, int *failed);
void run_theme_tests(int *run, int *failed);
void run_profile_tests(int *run, int *failed);
void run_repair_tests(int *run, int *failed);
void run_app_sync_tests(int *run, int *failed);
void run_color_scheme_tests(int *run, int *failed);
void run_emulator_tests(int *run, int *failed);
void run_alias_table_tests(int *run, int *failed);
void run_dir_stack_tests(int *run, int *failed);
void run_history_tests(int *run, int *failed);
void run_edit_buffer_tests(int *run, int *failed);
void run_execute_tests(int *run, int *failed);

int main(void) {
    int run = 0, failed = 0;

    run_strutil_tests(&run, &failed);
    run_strbuf_tests(&run, &failed);
    run_calmconf_tests(&run, &failed);
    run_theme_tests(&run, &failed);
    run_profile_tests(&run, &failed);
    run_repair_tests(&run, &failed);
    run_app_sync_tests(&run, &failed);
    run_color_scheme_tests(&run, &failed);
    run_emulator_tests(&run, &failed);
    run_alias_table_tests(&run, &failed);
    run_dir_stack_tests(&run, &failed);
    run_history_tests(&run, &failed);
    run_edit_buffer_tests(&run, &failed);
    run_execute_tests(&run, &failed);

    printf("%d checks, %d failed\n", run, failed);
    return failed == 0 ? 0 : 1;
}
