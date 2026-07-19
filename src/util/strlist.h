/* A growable array of owned strings -- used anywhere the codebase needs
 * a small, dependency-free dynamic set/list of C strings (known
 * commands, completion candidates, include-cycle tracking, ...). */
#ifndef CALM_UTIL_STRLIST_H
#define CALM_UTIL_STRLIST_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} StrList;

void strlist_init(StrList *list);
void strlist_add(StrList *list, const char *item);
bool strlist_contains(const StrList *list, const char *item);
/* Sorts lexicographically and removes adjacent duplicates. */
void strlist_sort_unique(StrList *list);
void strlist_free(StrList *list);

#endif /* CALM_UTIL_STRLIST_H */
