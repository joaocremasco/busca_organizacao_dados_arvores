#define _POSIX_C_SOURCE 200809L
#include "util.h"

#include <stdlib.h>
#include <string.h>

char *str_dup(const char *text) {
    size_t length;
    char *copy;
    if (text == NULL) return NULL;
    length = strlen(text) + 1;
    copy = malloc(length);
    if (copy != NULL) memcpy(copy, text, length);
    return copy;
}

bool write_u32(FILE *file, uint32_t value) {
    unsigned char bytes[4];
    unsigned int i;
    for (i = 0; i < 4; i++)
        bytes[i] = (unsigned char)(value >> (8 * i));
    return fwrite(bytes, 1, 4, file) == 4;
}

bool write_u64(FILE *file, uint64_t value) {
    unsigned char bytes[8];
    unsigned int i;
    for (i = 0; i < 8; i++)
        bytes[i] = (unsigned char)(value >> (8 * i));
    return fwrite(bytes, 1, 8, file) == 8;
}

bool read_u32(FILE *file, uint32_t *value) {
    unsigned char bytes[4];
    unsigned int i;
    uint32_t result = 0;
    if (fread(bytes, 1, 4, file) != 4) return false;
    for (i = 0; i < 4; i++)
        result |= (uint32_t)bytes[i] << (8 * i);
    *value = result;
    return true;
}

bool read_u64(FILE *file, uint64_t *value) {
    unsigned char bytes[8];
    unsigned int i;
    uint64_t result = 0;
    if (fread(bytes, 1, 8, file) != 8) return false;
    for (i = 0; i < 8; i++)
        result |= (uint64_t)bytes[i] << (8 * i);
    *value = result;
    return true;
}

bool file_remaining(FILE *file, uint64_t *remaining) {
    off_t current = ftello(file);
    off_t end;
    if (current < 0 || fseeko(file, 0, SEEK_END) != 0) return false;
    end = ftello(file);
    if (end < current || fseeko(file, current, SEEK_SET) != 0) return false;
    *remaining = (uint64_t)(end - current);
    return true;
}
