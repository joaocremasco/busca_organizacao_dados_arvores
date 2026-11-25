#include "app.h"
#include "fetch.h"
#include "index_file.h"
#include "offset_set.h"
#include "query.h"
#include "reader.h"
#include "timer.h"
#include "tree.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    INDEX_FILE *index;
    const QUERY *query;
    TREE_OFFSETS matches;
} QUERY_RESULT;

typedef struct {
    TREE_OFFSETS offsets;
} RESULT_REF;

typedef struct {
    INDEX_FILE *indices;
    size_t loaded;
    QUERY_LIST queries;
    QUERY_RESULT *results;
    OFFSET_SET offsets;
} SEARCH_DATA;

static bool write_one_record(FILE *data, FILE *out, uint64_t offset,
                             char **buffer, size_t *capacity, size_t *length,
                             double *fetch_ms, double *write_ms);

static TREE_KIND parse_kind(const char *name) {
    if (strcmp(name, "abb") == 0) return TREE_ABB;
    if (strcmp(name, "avl") == 0) return TREE_AVL;
    if (strcmp(name, "rubro_negra") == 0) return TREE_RB;
    return 0;
}

static const char *kind_name(TREE_KIND kind) {
    if (kind == TREE_ABB) return "abb";
    if (kind == TREE_AVL) return "avl";
    return "rubro_negra";
}

static bool valid_index_args(const CLI_ARGS *args, TREE_KIND *kind) {
    if (args->tree_name == NULL || args->key == NULL || args->index_count != 1) {
        fprintf(stderr, "Indexação exige --tree, --key e um --index\n");
        return false;
    }
    *kind = parse_kind(args->tree_name);
    if (*kind == 0) fprintf(stderr, "Tipo de árvore inválido: %s\n", args->tree_name);
    return *kind != 0;
}

static bool build_tree(const CLI_ARGS *args, READER *reader, TREE *tree,
                       int column, uint64_t *input_records,
                       uint64_t *indexed_records, double *read_ms,
                       double *insert_ms, double *build_ms) {
    char **fields;
    size_t field_count;
    uint64_t offset;
    READER_STATUS status = READER_OK;
    double total_start = timer_now_ms();
    while ((args->limit == 0 || *input_records < args->limit) && status == READER_OK) {
        double start = timer_now_ms();
        status = reader_next(reader, &fields, &field_count, &offset);
        *read_ms += timer_now_ms() - start;
        if (status != READER_OK) continue;
        if ((size_t)column >= field_count) return false;
        (*input_records)++;
        start = timer_now_ms();
        if (!tree_insert(tree, fields[column], offset)) return false;
        *insert_ms += timer_now_ms() - start;
        (*indexed_records)++;
    }
    *build_ms = timer_now_ms() - total_start;
    if (status == READER_ERROR) {
        fprintf(stderr, "Falha ao ler a base: %s\n", reader_error(reader));
        return false;
    }
    return true;
}

static void print_index_metrics(bool ok, TREE_KIND kind, const char *key,
                                uint64_t input_records, uint64_t indexed_records,
                                const TREE_STATS *stats, double build_ms,
                                double save_ms, double read_ms, double insert_ms) {
    printf("STATUS=%s\nTREE=%s\nKEY=%s\n", ok ? "OK" : "ERROR",
           kind_name(kind), key == NULL ? "" : key);
    printf("INPUT_RECORDS=%llu\nINDEXED_RECORDS=%llu\nDISTINCT_KEYS=%llu\n",
           (unsigned long long)input_records, (unsigned long long)indexed_records,
           (unsigned long long)stats->distinct);
    printf("TREE_HEIGHT=%llu\nINSERT_COMPARISONS=%llu\nROTATIONS=%llu\nRECOLORS=%llu\n",
           (unsigned long long)stats->height, (unsigned long long)stats->comparisons,
           (unsigned long long)stats->rotations, (unsigned long long)stats->recolors);
    printf("READ_MS=%.3f\nINSERT_MS=%.3f\nBUILD_MS=%.3f\nSAVE_INDEX_MS=%.3f\n",
           read_ms, insert_ms, build_ms, save_ms);
}

static bool prepare_index(const CLI_ARGS *args, TREE_KIND kind,
                          READER **reader, TREE **tree, int *column) {
    *reader = reader_open(args->input, args->reader);
    *tree = tree_create(kind);
    if (*reader == NULL || *tree == NULL) {
        fprintf(stderr, "Não foi possível iniciar leitor e árvore para '%s'\n", args->input);
        return false;
    }
    *column = reader_column_index(*reader, args->key);
    if (*column < 0) fprintf(stderr, "Coluna '%s' não existe na base\n", args->key);
    return *column >= 0;
}

static bool save_built_index(const CLI_ARGS *args, TREE_KIND kind, TREE **tree,
                             uint64_t records, const TREE_STATS *stats,
                             double *save_ms) {
    INDEX_FILE index;
    double start = timer_now_ms();
    bool ok = index_file_create(&index, args->input, args->key, kind, *tree,
                                records, stats);
    *tree = NULL;
    if (ok) {
        ok = index_file_save(args->index_paths[0], &index);
        index_file_destroy(&index);
    }
    *save_ms = timer_now_ms() - start;
    return ok;
}

static int app_index(const CLI_ARGS *args) {
    TREE_KIND kind = 0;
    READER *reader = NULL;
    TREE *tree = NULL;
    TREE_STATS stats = {0};
    uint64_t input_records = 0, indexed_records = 0;
    double build_ms = 0, save_ms = 0, read_ms = 0, insert_ms = 0;
    bool ok = valid_index_args(args, &kind);
    int column = -1;
    if (ok) ok = prepare_index(args, kind, &reader, &tree, &column);
    if (ok) ok = build_tree(args, reader, tree, column, &input_records,
                            &indexed_records, &read_ms, &insert_ms, &build_ms);
    if (ok) {
        stats = tree_stats(tree);
        ok = save_built_index(args, kind, &tree, indexed_records, &stats, &save_ms);
    }
    if (!ok) fprintf(stderr, "Falha ao executar indexação\n");
    if (reader != NULL) reader_close(&reader);
    if (tree != NULL) tree_destroy(&tree);
    print_index_metrics(ok, kind, args->key, input_records, indexed_records,
                        &stats, build_ms, save_ms, read_ms, insert_ms);
    return ok ? 0 : 1;
}

static int result_pointer_compare(const void *left, const void *right) {
    const RESULT_REF *a = left;
    const RESULT_REF *b = right;
    uintptr_t x = (uintptr_t)a->offsets.items;
    uintptr_t y = (uintptr_t)b->offsets.items;
    return x < y ? -1 : x > y ? 1 : 0;
}

static INDEX_FILE *find_index(INDEX_FILE *indices, size_t count, const char *key) {
    size_t i;
    for (i = 0; i < count; i++) {
        if (strcmp(indices[i].key, key) == 0) return &indices[i];
    }
    return NULL;
}

static bool load_indices(const CLI_ARGS *args, INDEX_FILE *indices,
                         size_t *loaded, double *load_ms) {
    size_t i, j;
    double start = timer_now_ms();
    for (i = 0; i < args->index_count; i++) {
        if (!index_file_load(args->index_paths[i], args->input, &indices[i])) {
            *load_ms = timer_now_ms() - start;
            return false;
        }
        (*loaded)++;
        for (j = 0; j < i; j++) {
            if (strcmp(indices[i].key, indices[j].key) == 0) {
                fprintf(stderr, "Índices repetidos para a coluna '%s'\n", indices[i].key);
                *load_ms = timer_now_ms() - start;
                return false;
            }
        }
    }
    *load_ms = timer_now_ms() - start;
    return true;
}

static QUERY_RESULT *map_queries(const QUERY_LIST *queries, INDEX_FILE *indices,
                                 size_t index_count) {
    QUERY_RESULT *results;
    size_t i;
    results = calloc(queries->count, sizeof(*results));
    if (results == NULL && queries->count != 0) return NULL;
    for (i = 0; i < queries->count; i++) {
        results[i].index = find_index(indices, index_count, queries->items[i].column);
        results[i].query = &queries->items[i];
        if (results[i].index == NULL) {
            fprintf(stderr, "coluna '%s' não possui índice fornecido\n",
                    queries->items[i].column);
            free(results);
            return NULL;
        }
    }
    return results;
}

static bool run_tree_search(QUERY_RESULT *queries, size_t count,
                            uint64_t *comparisons) {
    size_t i;
    for (i = 0; i < count; i++) {
        tree_find(queries[i].index->tree, queries[i].query->key,
                  &queries[i].matches, comparisons);
    }
    return true;
}

static double timed_tree_search(QUERY_RESULT *queries, size_t count,
                                uint64_t *comparisons,
                                bool *ok) {
    double start = timer_now_ms();
    *ok = run_tree_search(queries, count, comparisons);
    return timer_now_ms() - start;
}

static bool merge_results(QUERY_RESULT *results, size_t count,
                          OFFSET_SET *offsets, uint64_t *matched) {
    RESULT_REF *refs;
    size_t i, found = 0, unique = 0;
    refs = malloc(count * sizeof(*refs));
    if (refs == NULL && count != 0) return false;
    for (i = 0; i < count; i++) {
        if (*matched > UINT64_MAX - results[i].matches.count) {
            free(refs);
            return false;
        }
        *matched += (uint64_t)results[i].matches.count;
        if (results[i].matches.count != 0)
            refs[found++].offsets = results[i].matches;
    }
    if (found > 1) {
        qsort(refs, found, sizeof(*refs), result_pointer_compare);
    }
    for (i = 0; i < found; i++) {
        if (i == 0 || refs[i].offsets.items != refs[i - 1].offsets.items)
            refs[unique++] = refs[i];
    }
    for (i = 0; i < unique; i++) {
        size_t j;
        for (j = 0; j < refs[i].offsets.count; j++) {
            if (!offset_set_add(offsets, refs[i].offsets.items[j])) {
                free(refs);
                return false;
            }
        }
    }
    offset_set_sort_unique(offsets);
    free(refs);
    return true;
}

static bool write_one_record(FILE *data, FILE *out, uint64_t offset,
                             char **buffer, size_t *capacity, size_t *length,
                             double *fetch_ms, double *write_ms) {
    double start = timer_now_ms();
    bool ok = fetch_record(data, offset, buffer, capacity, length);
    *fetch_ms += timer_now_ms() - start;
    if (!ok) return false;
    start = timer_now_ms();
    ok = fwrite(*buffer, 1, *length, out) == *length;
    *write_ms += timer_now_ms() - start;
    return ok;
}

static bool write_output(const char *input, const char *output,
                         OFFSET_SET *offsets, double *fetch_ms,
                         double *write_ms) {
    FILE *data = fopen(input, "rb");
    FILE *out = fopen(output, "wb");
    char *buffer = NULL;
    size_t capacity = 0, length = 0, i;
    bool ok = data != NULL && out != NULL;
    if (data == NULL) fprintf(stderr, "Não foi possível abrir a base '%s': %s\n", input, strerror(errno));
    if (out == NULL) fprintf(stderr, "Não foi possível criar a saída '%s': %s\n", output, strerror(errno));
    if (ok) ok = write_one_record(data, out, 0, &buffer, &capacity, &length,
                                  fetch_ms, write_ms);
    for (i = 0; ok && i < offsets->count; i++)
        ok = write_one_record(data, out, offsets->items[i], &buffer, &capacity,
                              &length, fetch_ms, write_ms);
    free(buffer);
    if (out != NULL) {
        double start = timer_now_ms();
        if (fflush(out) != 0) ok = false;
        *write_ms += timer_now_ms() - start;
        start = timer_now_ms();
        if (fclose(out) != 0) ok = false;
        *write_ms += timer_now_ms() - start;
    }
    if (data != NULL && fclose(data) != 0) ok = false;
    return ok;
}

static void destroy_indices(INDEX_FILE *indices, size_t count) {
    size_t i;
    for (i = 0; i < count; i++)
        index_file_destroy(&indices[i]);
    free(indices);
}

static void print_search_metrics(bool ok, size_t index_count, size_t query_count,
                                 uint64_t comparisons, uint64_t matched,
                                 size_t output_records, double load_ms,
                                 double search_ms, double dedup_ms, double fetch_ms,
                                 double write_ms, double total_ms) {
    printf("STATUS=%s\nINDEX_COUNT=%llu\nQUERY_KEYS=%llu\n", ok ? "OK" : "ERROR",
           (unsigned long long)index_count, (unsigned long long)query_count);
    printf("SEARCH_COMPARISONS=%llu\nMATCHED_OFFSETS=%llu\nOUTPUT_RECORDS=%llu\n",
           (unsigned long long)comparisons, (unsigned long long)matched,
           (unsigned long long)output_records);
    printf("LOAD_INDEX_MS=%.3f\nSEARCH_MS=%.3f\nDEDUP_MS=%.3f\n",
           load_ms, search_ms, dedup_ms);
    printf("FETCH_MS=%.3f\nWRITE_MS=%.3f\nTOTAL_MS=%.3f\n",
           fetch_ms, write_ms, total_ms);
}

static bool prepare_search(const CLI_ARGS *args, SEARCH_DATA *data,
                           double *load_ms) {
    size_t bad_line = 0;
    if (args->index_count == 0 || args->query == NULL || args->output == NULL) {
        fprintf(stderr, "Busca exige --index, --query e --output\n");
        return false;
    }
    data->indices = calloc(args->index_count, sizeof(*data->indices));
    if (data->indices == NULL) return false;
    if (!load_indices(args, data->indices, &data->loaded, load_ms)) return false;
    if (!query_read_file(args->query, &data->queries, &bad_line)) {
        if (bad_line != 0)
            fprintf(stderr, "%s:%llu: formato inválido\n", args->query,
                    (unsigned long long)bad_line);
        else fprintf(stderr, "Não foi possível ler consultas '%s'\n", args->query);
        return false;
    }
    data->results = map_queries(&data->queries, data->indices, data->loaded);
    return data->results != NULL || data->queries.count == 0;
}

static bool execute_search(const CLI_ARGS *args, SEARCH_DATA *data,
                           uint64_t *comparisons, uint64_t *matched,
                           size_t *output_count, double *search_ms,
                           double *dedup_ms, double *fetch_ms,
                           double *write_ms) {
    bool ok;
    *search_ms = timed_tree_search(data->results, data->queries.count,
                                   comparisons, &ok);
    if (!ok) return false;
    double start = timer_now_ms();
    ok = merge_results(data->results, data->queries.count,
                       &data->offsets, matched);
    *dedup_ms = timer_now_ms() - start;
    *output_count = data->offsets.count;
    return ok && write_output(args->input, args->output, &data->offsets,
                              fetch_ms, write_ms);
}

static void search_data_destroy(SEARCH_DATA *data) {
    query_list_destroy(&data->queries);
    free(data->results);
    offset_set_destroy(&data->offsets);
    destroy_indices(data->indices, data->loaded);
}

static int app_search(const CLI_ARGS *args) {
    double total_start = timer_now_ms();
    double load_ms = 0, search_ms = 0, dedup_ms = 0, fetch_ms = 0, write_ms = 0;
    SEARCH_DATA data = {0};
    uint64_t comparisons = 0, matched = 0;
    size_t query_count = 0, output_count = 0;
    bool ok = prepare_search(args, &data, &load_ms);
    query_count = data.queries.count;
    if (ok) ok = execute_search(args, &data, &comparisons, &matched,
                                &output_count, &search_ms, &dedup_ms,
                                &fetch_ms, &write_ms);
    if (!ok) fprintf(stderr, "Falha ao executar a busca\n");
    search_data_destroy(&data);
    print_search_metrics(ok, data.loaded, query_count, comparisons, matched,
                         output_count, load_ms, search_ms, dedup_ms,
                         fetch_ms, write_ms, timer_now_ms() - total_start);
    return ok ? 0 : 1;
}

int app_run(const CLI_ARGS *args) {
    if (strcmp(args->mode, "index") == 0) return app_index(args);
    return app_search(args);
}
