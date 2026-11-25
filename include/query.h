#ifndef PROJETO2_QUERY_H
#define PROJETO2_QUERY_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *column;
    char *key;
} QUERY;

typedef struct {
    QUERY *items;
    size_t count;
    size_t capacity;
} QUERY_LIST;

bool query_read_file(const char *path, QUERY_LIST *queries, size_t *bad_line);
void query_list_destroy(QUERY_LIST *queries);

#endif
