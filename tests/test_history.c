#include "term/history.h"

#include <stdlib.h>
#include <string.h>

#include "test_framework.h"
#include "util/fsutil.h"

static void test_ignore_dups_and_cap(int *run, int *failed) {
    History hist;
    history_load(NULL, /* max_entries */ 3, /* ignore_dups */ true, &hist);

    history_add(&hist, "one");
    history_add(&hist, "one"); /* immediate duplicate, should be skipped */
    history_add(&hist, "two");
    history_add(&hist, "three");
    history_add(&hist, "four"); /* pushes count past cap of 3, drops "one" */

    CHECK(run, failed, hist.count == 3);
    if (hist.count == 3) {
        CHECK_STR_EQ(run, failed, hist.entries[0], "two");
        CHECK_STR_EQ(run, failed, hist.entries[1], "three");
        CHECK_STR_EQ(run, failed, hist.entries[2], "four");
    }

    history_free(&hist);
}

static void test_persistence_round_trip(int *run, int *failed) {
    char tmpdir_template[] = "/tmp/calm_hist_test_XXXXXX";
    char *tmpdir = mkdtemp(tmpdir_template);
    CHECK(run, failed, tmpdir != NULL);
    if (!tmpdir) {
        return;
    }
    char *hist_path = join_path(tmpdir, "history");

    History hist;
    history_load(hist_path, 100, false, &hist);
    history_add(&hist, "echo hello");
    history_add(&hist, "line with\nan embedded newline");
    history_free(&hist); /* writes the final, capped contents to disk */

    History reloaded;
    history_load(hist_path, 100, false, &reloaded);
    CHECK(run, failed, reloaded.count == 2);
    if (reloaded.count == 2) {
        CHECK_STR_EQ(run, failed, reloaded.entries[0], "echo hello");
        CHECK_STR_EQ(run, failed, reloaded.entries[1], "line with\nan embedded newline");
    }
    history_free(&reloaded);
    free(hist_path);
}

static void test_fuzzy_search_orders_and_dedupes(int *run, int *failed) {
    History hist;
    history_load(NULL, 0, false, &hist);
    history_add(&hist, "git status");
    history_add(&hist, "git commit -m fix");
    history_add(&hist, "git status"); /* repeated later; fuzzy search should de-dupe by text */

    StrList matches = history_fuzzy_search(&hist, "gst", 10);
    CHECK(run, failed, matches.count >= 1);

    size_t status_count = 0;
    for (size_t i = 0; i < matches.count; i++) {
        if (strcmp(matches.items[i], "git status") == 0) {
            status_count++;
        }
    }
    CHECK(run, failed, status_count == 1); /* de-duplicated despite two identical entries */

    strlist_free(&matches);
    history_free(&hist);
}

void run_history_tests(int *run, int *failed) {
    test_ignore_dups_and_cap(run, failed);
    test_persistence_round_trip(run, failed);
    test_fuzzy_search_orders_and_dedupes(run, failed);
}
