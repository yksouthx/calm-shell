#include "shell/alias_table.h"

#include <stdlib.h>

#include "config/calmconf.h"
#include "test_framework.h"

static void test_expand(int *run, int *failed) {
    CalmDocument doc;
    char *err = NULL;
    bool ok = calm_parse("[aliases]\nll = \"ls -la\"\n"
                          "[directory.aliases]\n\"..\" = \"cd ..\"\n",
                          &doc, &err);
    CHECK(run, failed, ok);
    free(err);
    if (!ok) {
        return;
    }

    AliasTable table = alias_table_load(&doc);

    char *whole = alias_table_expand(&table, "..");
    CHECK_STR_EQ(run, failed, whole, "cd ..");
    free(whole);

    char *first_word = alias_table_expand(&table, "ll -a extra");
    CHECK_STR_EQ(run, failed, first_word, "ls -la -a extra");
    free(first_word);

    char *unchanged = alias_table_expand(&table, "echo hi");
    CHECK_STR_EQ(run, failed, unchanged, "echo hi");
    free(unchanged);

    alias_table_free(&table);
    calm_document_free(&doc);
}

void run_alias_table_tests(int *run, int *failed) {
    test_expand(run, failed);
}
