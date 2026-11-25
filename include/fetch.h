#ifndef PROJETO2_FETCH_H
#define PROJETO2_FETCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

bool fetch_record(FILE *data, uint64_t offset, char **buffer,
                  size_t *capacity, size_t *length);

#endif
