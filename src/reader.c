#define _POSIX_C_SOURCE 200809L
#include "reader.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct reader {
    FILE *file;
    char *reader_name;
    char delimiter;
    char **columns;
    size_t column_count;
    char *line;
    size_t line_capacity;
    char **fields;
    size_t fields_capacity;
    size_t line_length;
    char error[256];
};

static bool grow_buffer(char **buffer, size_t *capacity, size_t needed) {
    char *grown;
    size_t next = *capacity == 0 ? 128 : *capacity;
    while (next < needed) {
        if (next > (size_t)-1 / 2) return false;
        next *= 2;
    }
    if (next == *capacity) return true;
    grown = realloc(*buffer, next);
    if (grown == NULL) return false;
    *buffer = grown;
    *capacity = next;
    return true;
}

static bool append_input_char(READER *reader, size_t *length, int *quoted, int ch) {
    if (!grow_buffer(&reader->line, &reader->line_capacity, *length + 2)) return false;
    reader->line[(*length)++] = (char)ch;
    if (ch != '"') return true;
    if (!*quoted) {
        *quoted = 1;
        return true;
    }
    int next = fgetc(reader->file);
    if (next == '"') {
        if (!grow_buffer(&reader->line, &reader->line_capacity, *length + 2)) return false;
        reader->line[(*length)++] = '"';
    } else {
        *quoted = 0;
        if (next != EOF) ungetc(next, reader->file);
    }
    return true;
}

static int read_line(READER *reader) {
    size_t length = 0;
    int quoted = 0;
    int ch;
    while ((ch = fgetc(reader->file)) != EOF) {
        if (ch == '\n' && !quoted) break;
        if (!append_input_char(reader, &length, &quoted, ch)) {
            snprintf(reader->error, sizeof(reader->error), "Sem memória para ler registro");
            return -1;
        }
    }
    if (ferror(reader->file)) {
        snprintf(reader->error, sizeof(reader->error), "Erro ao ler arquivo CSV");
        return -1;
    }
    if (length == 0 && ch == EOF) return 0;
    if (quoted) {
        snprintf(reader->error, sizeof(reader->error), "Campo CSV com aspas não fechadas");
        return -1;
    }
    if (length > 0 && reader->line[length - 1] == '\r') length--;
    reader->line[length] = '\0';
    reader->line_length = length;
    return 1;
}

static char detect_delimiter(const char *line) {
    const char options[] = {',', ';', '\t'};
    size_t counts[3] = {0, 0, 0};
    int quoted = 0;
    size_t i, j, best = 0;
    for (i = 0; line[i] != '\0'; i++) {
        if (line[i] == '"') {
            if (quoted && line[i + 1] == '"') i++;
            else quoted = !quoted;
        } else if (!quoted) {
            for (j = 0; j < 3; j++) {
                if (line[i] == options[j]) counts[j]++;
            }
        }
    }
    for (i = 1; i < 3; i++) {
        if (counts[i] > counts[best]) best = i;
    }
    return options[best];
}

static bool fields_reserve(READER *reader, size_t needed) {
    char **grown;
    size_t next = reader->fields_capacity == 0 ? 8 : reader->fields_capacity;
    while (next < needed) next *= 2;
    if (next == reader->fields_capacity) return true;
    grown = realloc(reader->fields, next * sizeof(*grown));
    if (grown == NULL) return false;
    reader->fields = grown;
    reader->fields_capacity = next;
    return true;
}

static bool split_record(READER *reader, size_t *count_out) {
    char *read = reader->line;
    char *write = reader->line;
    size_t count = 0;
    for (;;) {
        int quoted;
        bool more = false;
        if (!fields_reserve(reader, count + 1)) return false;
        reader->fields[count++] = write;
        quoted = *read == '"';
        if (quoted) read++;
        while (*read != '\0') {
            if (quoted && read[0] == '"' && read[1] == '"') {
                *write++ = '"';
                read += 2;
            } else if (quoted && *read == '"') {
                quoted = 0;
                read++;
                if (*read != reader->delimiter && *read != '\0') return false;
            } else if (!quoted && *read == reader->delimiter) {
                read++;
                more = true;
                break;
            } else {
                *write++ = *read++;
            }
        }
        if (quoted) return false;
        *write++ = '\0';
        if (!more) break;
    }
    *count_out = count;
    return true;
}

static bool read_header(READER *reader) {
    size_t count, i;
    int status = read_line(reader);
    if (status != 1) return false;
    reader->delimiter = detect_delimiter(reader->line);
    if (!split_record(reader, &count)) return false;
    if (strcmp(reader->reader_name, "games") == 0) {
        for (i = 0; i < count; i++) {
            if (strcmp(reader->fields[i], "DiscountDLC count") == 0) {
                if (!fields_reserve(reader, count + 1)) return false;
                memmove(&reader->fields[i + 2], &reader->fields[i + 1],
                        (count - i - 1) * sizeof(*reader->fields));
                reader->fields[i] = "Discount";
                reader->fields[i + 1] = "DLC count";
                count++;
                break;
            }
        }
    }
    reader->columns = calloc(count, sizeof(*reader->columns));
    if (reader->columns == NULL) return false;
    reader->column_count = count;
    for (i = 0; i < count; i++) {
        reader->columns[i] = str_dup(reader->fields[i]);
        if (reader->columns[i] == NULL) return false;
    }
    return true;
}

READER *reader_open(const char *path, const char *reader_name) {
    READER *reader;
    if (path == NULL || reader_name == NULL) return NULL;
    if (strcmp(reader_name, "nasa") != 0 && strcmp(reader_name, "games") != 0) return NULL;
    reader = calloc(1, sizeof(*reader));
    if (reader == NULL) return NULL;
    reader->reader_name = str_dup(reader_name);
    reader->file = fopen(path, "rb");
    if (reader->reader_name == NULL || reader->file == NULL || !read_header(reader)) {
        reader_close(&reader);
        return NULL;
    }
    return reader;
}

void reader_close(READER **reader) {
    size_t i;
    if (reader == NULL || *reader == NULL) return;
    if ((*reader)->file != NULL) fclose((*reader)->file);
    for (i = 0; i < (*reader)->column_count; i++)
        free((*reader)->columns[i]);
    free((*reader)->columns);
    free((*reader)->reader_name);
    free((*reader)->line);
    free((*reader)->fields);
    free(*reader);
    *reader = NULL;
}

READER_STATUS reader_next(READER *reader, char ***fields, size_t *field_count,
                          uint64_t *offset) {
    int status;
    off_t start;
    size_t count;
    if (reader == NULL || fields == NULL || field_count == NULL || offset == NULL)
        return READER_ERROR;
    for (;;) {
        start = ftello(reader->file);
        if (start < 0) return READER_ERROR;
        status = read_line(reader);
        if (status == 0) return READER_EOF;
        if (status < 0) return READER_ERROR;
        if (reader->line_length == 0) continue;
        break;
    }
    if (!split_record(reader, &count) || count != reader->column_count) {
        snprintf(reader->error, sizeof(reader->error), "Quantidade de campos incorreta ou CSV inválido");
        return READER_ERROR;
    }
    *fields = reader->fields;
    *field_count = count;
    *offset = (uint64_t)start;
    return READER_OK;
}

int reader_column_index(const READER *reader, const char *name) {
    size_t i;
    if (reader == NULL || name == NULL) return -1;
    for (i = 0; i < reader->column_count; i++) {
        if (strcmp(reader->columns[i], name) == 0) return (int)i;
    }
    return -1;
}

size_t reader_column_count(const READER *reader) {
    return reader == NULL ? 0 : reader->column_count;
}

const char *reader_column_name(const READER *reader, size_t index) {
    return reader != NULL && index < reader->column_count ? reader->columns[index] : NULL;
}

char reader_delimiter(const READER *reader) {
    return reader == NULL ? ',' : reader->delimiter;
}

const char *reader_error(const READER *reader) {
    return reader == NULL ? "leitor inválido" : reader->error;
}
