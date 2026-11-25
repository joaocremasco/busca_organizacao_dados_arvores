#ifndef PROJETO2_CLI_H
#define PROJETO2_CLI_H

#include "tree.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *mode;
    const char *reader;
    const char *input;
    const char *tree_name;
    const char *key;
    const char *query;
    const char *output;
    const char *index_path;
    char **index_paths;
    size_t index_count;
    uint64_t limit;
    int has_limit;
} CLI_ARGS;

bool cli_parse(int argc, char **argv, CLI_ARGS *args);
void cli_destroy(CLI_ARGS *args);
void cli_usage(const char *program);

#endif
