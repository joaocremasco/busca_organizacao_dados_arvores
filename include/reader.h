#ifndef PROJETO2_READER_H
#define PROJETO2_READER_H

#include <stddef.h>
#include <stdint.h>

typedef struct reader READER;
typedef enum { READER_ERROR = -1, READER_EOF = 0, READER_OK = 1 } READER_STATUS;

READER *reader_open(const char *path, const char *reader_name);
void reader_close(READER **reader);
/* Linhas vazias são ignoradas; os campos duram até a próxima chamada. */
READER_STATUS reader_next(READER *reader, char ***fields, size_t *field_count,
                          uint64_t *offset);
int reader_column_index(const READER *reader, const char *name);
size_t reader_column_count(const READER *reader);
const char *reader_column_name(const READER *reader, size_t index);
char reader_delimiter(const READER *reader);
const char *reader_error(const READER *reader);

#endif
