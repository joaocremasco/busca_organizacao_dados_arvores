#define _POSIX_C_SOURCE 200809L
#include "fetch.h"

#include <stdlib.h>

static bool append_char(char **buffer, size_t *length, size_t *capacity, int ch) {
    char *grown;
    size_t next;
    if (*length + 1 >= *capacity) {
        next = *capacity == 0 ? 256 : *capacity * 2;
        if (next <= *capacity) return false;
        grown = realloc(*buffer, next);
        if (grown == NULL) return false;
        *buffer = grown;
        *capacity = next;
    }
    (*buffer)[(*length)++] = (char)ch;
    return true;
}

bool fetch_record(FILE *data, uint64_t offset, char **buffer,
                  size_t *capacity, size_t *length) {
    bool quoted = false;
    int ch;
    if (fseeko(data, (off_t)offset, SEEK_SET) != 0) return false;
    *length = 0;
    while ((ch = fgetc(data)) != EOF) {
        if (!append_char(buffer, length, capacity, ch)) return false;
        if (ch == '"') {
            if (quoted) {
                int next = fgetc(data);
                if (next == '"') {
                    if (!append_char(buffer, length, capacity, next)) return false;
                } else {
                    quoted = false;
                    if (next != EOF) ungetc(next, data);
                }
            } else {
                quoted = true;
            }
        } else if (ch == '\n' && !quoted) {
            break;
        }
    }
    if (ferror(data) || quoted || *length == 0) return false;
    return true;
}
