#include "cli.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool add_index(CLI_ARGS *args, const char *path) {
    char **grown = realloc(args->index_paths,
                           (args->index_count + 1) * sizeof(*grown));
    if (grown == NULL) return false;
    args->index_paths = grown;
    args->index_paths[args->index_count++] = (char *)path;
    return true;
}

static bool parse_limit(const char *text, uint64_t *limit) {
    char *end;
    unsigned long long value;
    if (text[0] == '-') return false;
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') return false;
    *limit = (uint64_t)value;
    return true;
}

static bool set_option(CLI_ARGS *args, const char *name, const char *value) {
    if (strcmp(name, "--mode") == 0 && args->mode == NULL) args->mode = value;
    else if (strcmp(name, "--reader") == 0 && args->reader == NULL) args->reader = value;
    else if (strcmp(name, "--input") == 0 && args->input == NULL) args->input = value;
    else if (strcmp(name, "--tree") == 0 && args->tree_name == NULL) args->tree_name = value;
    else if (strcmp(name, "--key") == 0 && args->key == NULL) args->key = value;
    else if (strcmp(name, "--query") == 0 && args->query == NULL) args->query = value;
    else if (strcmp(name, "--output") == 0 && args->output == NULL) args->output = value;
    else if (strcmp(name, "--index") == 0) return add_index(args, value);
    else if (strcmp(name, "--limit") == 0 && !args->has_limit) {
        args->has_limit = 1;
        return parse_limit(value, &args->limit);
    } else return false;
    return true;
}

void cli_usage(const char *program) {
    fprintf(stderr, "Uso: %s --mode index|search --reader nasa|games --input CSV ", program);
    fprintf(stderr, "[--tree abb|avl|rubro_negra --key COLUNA --index IDX --limit N] ");
    fprintf(stderr, "[--index IDX ... --query ARQUIVO --output CSV]\n");
}

bool cli_parse(int argc, char **argv, CLI_ARGS *args) {
    int i;
    memset(args, 0, sizeof(*args));
    for (i = 1; i < argc; i++) {
        const char *name = argv[i];
        const char *value;
        if (name[0] != '-' || i + 1 >= argc || strncmp(argv[i + 1], "--", 2) == 0) {
            fprintf(stderr, "Opção desconhecida ou sem valor: %s\n", name);
            return false;
        }
        value = argv[++i];
        if (!set_option(args, name, value)) {
            fprintf(stderr, "Opção repetida ou valor inválido: %s\n", name);
            return false;
        }
    }
    if (args->mode == NULL || args->input == NULL || args->reader == NULL ||
        (strcmp(args->reader, "nasa") != 0 && strcmp(args->reader, "games") != 0) ||
        (strcmp(args->mode, "index") != 0 && strcmp(args->mode, "search") != 0)) {
        fprintf(stderr, "Modo, leitor ou arquivo de entrada inválido/ausente\n");
        return false;
    }
    return true;
}

void cli_destroy(CLI_ARGS *args) {
    if (args == NULL) return;
    free(args->index_paths);
    memset(args, 0, sizeof(*args));
}
