#ifndef PROJETO2_INDEX_FILE_H
#define PROJETO2_INDEX_FILE_H

#include "tree.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char *key;
    char *base_name;
    TREE_KIND kind;
    TREE *tree;
    uint64_t data_size;
    int64_t data_mtime;
    uint64_t input_records;
    uint64_t checksum;
    uint64_t distinct_keys;
} INDEX_FILE;

bool index_file_create(INDEX_FILE *index, const char *input, const char *key,
                       TREE_KIND kind, TREE *tree, uint64_t records,
                       const TREE_STATS *stats);
bool index_file_save(const char *path, const INDEX_FILE *index);
bool index_file_load(const char *path, const char *input, INDEX_FILE *index);
void index_file_destroy(INDEX_FILE *index);

#endif
