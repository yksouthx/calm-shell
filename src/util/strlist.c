#include "util/strlist.h"

#include <stdlib.h>
#include <string.h>

#include "util/xalloc.h"

void strlist_init(StrList *list) {
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

void strlist_add(StrList *list, const char *item) {
    if (list->count == list->cap) {
        list->cap = list->cap == 0 ? 8 : list->cap * 2;
        list->items = xrealloc(list->items, list->cap * sizeof(char *));
    }
    list->items[list->count++] = xstrdup(item);
}

bool strlist_contains(const StrList *list, const char *item) {
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->items[i], item) == 0) {
            return true;
        }
    }
    return false;
}

static int str_cmp_qsort(const void *a, const void *b) {
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}

void strlist_sort_unique(StrList *list) {
    if (list->count == 0) {
        return;
    }
    qsort(list->items, list->count, sizeof(char *), str_cmp_qsort);
    size_t w = 1;
    for (size_t r = 1; r < list->count; r++) {
        if (strcmp(list->items[r], list->items[w - 1]) != 0) {
            list->items[w++] = list->items[r];
        } else {
            free(list->items[r]);
        }
    }
    list->count = w;
}

void strlist_free(StrList *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}
