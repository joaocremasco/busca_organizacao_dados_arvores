#include "query.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool query_append(QUERY_LIST *queries, const char *column, const char *key) {
    QUERY *grown;
    QUERY query;
    size_t capacity;
    query.column = str_dup(column);
    query.key = str_dup(key);
    if (query.column == NULL || query.key == NULL) {
        free(query.column);
        free(query.key);
        return false;
    }
    if (queries->count == queries->capacity) {
        capacity = queries->capacity == 0 ? 16 : queries->capacity * 2;
        grown = realloc(queries->items, capacity * sizeof(*grown));
        if (grown == NULL) {
            free(query.column);
            free(query.key);
            return false;
        }
        queries->items = grown;
        queries->capacity = capacity;
    }
    queries->items[queries->count++] = query;
    return true;
}

static bool parse_query_line(char *line, QUERY_LIST *queries) {
    char *separator;
    if (strncmp(line, "KEY|", 4) != 0) return false;
    separator = strchr(line + 4, '|');
    if (separator == NULL || separator == line + 4) return false;
    *separator = '\0';
    return query_append(queries, line + 4, separator + 1);
}

bool query_read_file(const char *path, QUERY_LIST *queries, size_t *bad_line) {
    FILE *file = fopen(path, "r");
    char line[65536];
    size_t line_number = 0;
    if (file == NULL) return false;
    while (fgets(line, sizeof(line), file) != NULL) {
        size_t length;
        bool ended_line;
        line_number++;
        length = strlen(line);
        ended_line = length > 0 && line[length - 1] == '\n';
        if (ended_line) line[--length] = '\0';
        if (length > 0 && line[length - 1] == '\r') line[--length] = '\0';
        if (length == 0) continue;
        if (!ended_line) {
            int ch = fgetc(file);
            if (ch != EOF && ch != '\n') {
                *bad_line = line_number;
                fclose(file);
                return false;
            }
        }
        if (!parse_query_line(line, queries)) {
            *bad_line = line_number;
            fclose(file);
            return false;
        }
    }
    if (ferror(file)) {
        fclose(file);
        return false;
    }
    return fclose(file) == 0;
}

void query_list_destroy(QUERY_LIST *queries) {
    size_t i;
    if (queries == NULL) return;
    for (i = 0; i < queries->count; i++) {
        free(queries->items[i].column);
        free(queries->items[i].key);
    }
    free(queries->items);
    memset(queries, 0, sizeof(*queries));
}
