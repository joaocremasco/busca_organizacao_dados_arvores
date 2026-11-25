#ifndef PROJETO2_UTIL_H
#define PROJETO2_UTIL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

char *str_dup(const char *text);
bool write_u32(FILE *file, uint32_t value);
bool write_u64(FILE *file, uint64_t value);
bool read_u32(FILE *file, uint32_t *value);
bool read_u64(FILE *file, uint64_t *value);
bool file_remaining(FILE *file, uint64_t *remaining);

#endif
