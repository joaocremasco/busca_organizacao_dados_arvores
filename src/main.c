#define _POSIX_C_SOURCE 200809L
#include "../include/reader.h"
#include "../include/tree.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

typedef struct {
    char *path;
    char *key;
    TREE_KIND kind;
    TREE *tree;
    off_t data_size;
    time_t data_mtime;
    long data_mtime_nsec;
    dev_t data_device;
    ino_t data_inode;
} INDEX;

typedef struct { uint64_t *items; size_t count, capacity; } OFFSETS;

static double now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0.0;
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static char *copy_text(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p != NULL) memcpy(p, s, n);
    return p;
}

static TREE_KIND parse_kind(const char *s) {
    if (strcmp(s, "abb") == 0) return TREE_ABB;
    if (strcmp(s, "avl") == 0) return TREE_AVL;
    if (strcmp(s, "rubro_negra") == 0) return TREE_RB;
    return 0;
}

static const char *kind_name(TREE_KIND kind) {
    if (kind == TREE_ABB) return "abb";
    if (kind == TREE_AVL) return "avl";
    return "rubro_negra";
}

static const char *option_value(int argc, char **argv, const char *name, int *count) {
    const char *value = NULL;
    *count = 0;
    for (int i = 2; i + 1 < argc; ++i) {
        if (strcmp(argv[i], name) == 0) { value = argv[i + 1]; (*count)++; }
    }
    return value;
}

static bool write_index(const char *path, INDEX *index, uint64_t input_records) {
    static const char magic[8] = {'A','E','D','1','I','D','X','2'};
    FILE *file = fopen(path, "wb");
    if (file == NULL) return false;
    uint32_t version = 2, kind = (uint32_t)index->kind, key_type = 1;
    uint32_t key_len = (uint32_t)strlen(index->key);
    uint32_t path_len = (uint32_t)strlen(index->path);
    uint64_t distinct = (uint64_t)tree_distinct_keys(index->tree);
    bool ok = fwrite(magic, 1, sizeof(magic), file) == sizeof(magic) &&
        fwrite(&version, sizeof(version), 1, file) == 1 &&
        fwrite(&kind, sizeof(kind), 1, file) == 1 &&
        fwrite(&key_type, sizeof(key_type), 1, file) == 1 &&
        fwrite(&key_len, sizeof(key_len), 1, file) == 1 &&
        fwrite(index->key, 1, key_len, file) == key_len &&
        fwrite(&path_len, sizeof(path_len), 1, file) == 1 &&
        fwrite(index->path, 1, path_len, file) == path_len &&
        fwrite(&index->data_size, sizeof(index->data_size), 1, file) == 1 &&
        fwrite(&index->data_mtime, sizeof(index->data_mtime), 1, file) == 1 &&
        fwrite(&index->data_mtime_nsec, sizeof(index->data_mtime_nsec), 1, file) == 1 &&
        fwrite(&index->data_device, sizeof(index->data_device), 1, file) == 1 &&
        fwrite(&index->data_inode, sizeof(index->data_inode), 1, file) == 1 &&
        fwrite(&input_records, sizeof(input_records), 1, file) == 1 &&
        fwrite(&distinct, sizeof(distinct), 1, file) == 1 && tree_write(file, index->tree);
    if (fclose(file) != 0) ok = false;
    return ok;
}

static bool read_index(const char *path, const char *input, INDEX *index) {
    static const char expected[8] = {'A','E','D','1','I','D','X','2'};
    char magic[8]; uint32_t version, kind, key_type, key_len, path_len;
    uint64_t input_records, distinct;
    struct stat st;
    memset(index, 0, sizeof(*index));
    FILE *file = fopen(path, "rb");
    if (file == NULL) return false;
    bool ok = fread(magic, 1, sizeof(magic), file) == sizeof(magic) &&
        memcmp(magic, expected, sizeof(magic)) == 0 &&
        fread(&version, sizeof(version), 1, file) == 1 && version == 2 &&
        fread(&kind, sizeof(kind), 1, file) == 1 && kind >= TREE_ABB && kind <= TREE_RB &&
        fread(&key_type, sizeof(key_type), 1, file) == 1 && key_type == 1 &&
        fread(&key_len, sizeof(key_len), 1, file) == 1 && key_len > 0 && key_len < 65536;
    if (!ok) { fclose(file); return false; }
    index->key = malloc((size_t)key_len + 1);
    if (index->key == NULL || fread(index->key, 1, key_len, file) != key_len ||
        fread(&path_len, sizeof(path_len), 1, file) != 1 || path_len == 0 || path_len >= 1048576) {
        free(index->key); fclose(file); return false;
    }
    index->key[key_len] = '\0';
    char *stored_path = malloc((size_t)path_len + 1);
    if (stored_path == NULL || fread(stored_path, 1, path_len, file) != path_len ||
        fread(&index->data_size, sizeof(index->data_size), 1, file) != 1 ||
        fread(&index->data_mtime, sizeof(index->data_mtime), 1, file) != 1 ||
        fread(&index->data_mtime_nsec, sizeof(index->data_mtime_nsec), 1, file) != 1 ||
        fread(&index->data_device, sizeof(index->data_device), 1, file) != 1 ||
        fread(&index->data_inode, sizeof(index->data_inode), 1, file) != 1 ||
        fread(&input_records, sizeof(input_records), 1, file) != 1 ||
        fread(&distinct, sizeof(distinct), 1, file) != 1) {
        free(stored_path); free(index->key); fclose(file); return false;
    }
    stored_path[path_len] = '\0';
    if (stat(input, &st) != 0 || st.st_size != index->data_size ||
        st.st_mtime != index->data_mtime || st.st_mtim.tv_nsec != index->data_mtime_nsec) {
        free(stored_path); free(index->key); fclose(file); return false;
    }
    if (st.st_dev != index->data_device || st.st_ino != index->data_inode) {
        free(stored_path); free(index->key); fclose(file); return false;
    }
    (void)input_records;
    index->path = stored_path;
    index->kind = (TREE_KIND)kind;
    index->tree = tree_read(file, index->kind);
    bool tree_ok = index->tree != NULL;
    int close_status = fclose(file);
    if (!tree_ok || tree_distinct_keys(index->tree) != distinct || close_status != 0) {
        tree_destroy(&index->tree); free(index->path); free(index->key); return false;
    }
    return true;
}

static void index_release(INDEX *index) {
    if (index == NULL) return;
    free(index->path); free(index->key); tree_destroy(&index->tree);
    memset(index, 0, sizeof(*index));
}

static int run_index(int argc, char **argv) {
    int count;
    const char *input = option_value(argc, argv, "--input", &count);
    const char *key = option_value(argc, argv, "--key", &count);
    const char *tree_text = option_value(argc, argv, "--tree", &count);
    const char *index_path = option_value(argc, argv, "--index", &count);
    const char *reader_name = option_value(argc, argv, "--reader", &count);
    const char *limit_text = option_value(argc, argv, "--limit", &count);
    if (input == NULL || key == NULL || tree_text == NULL || index_path == NULL || reader_name == NULL) {
        fprintf(stderr, "Uso: trabalho --mode index --reader nasa|games --input CSV --tree abb|avl|rubro_negra --key COLUNA --index ARQUIVO [--limit N]\n");
        return 2;
    }
    TREE_KIND kind = parse_kind(tree_text);
    if (kind == 0) { fprintf(stderr, "Tipo de árvore inválido\n"); return 2; }
    uint64_t limit = limit_text == NULL ? 0 : strtoull(limit_text, NULL, 10);
    struct stat st;
    if (stat(input, &st) != 0) { perror(input); return 1; }
    INDEX index = {0};
    index.path = copy_text(input); index.key = copy_text(key); index.kind = kind;
    index.data_size = st.st_size; index.data_mtime = st.st_mtime;
    index.data_mtime_nsec = st.st_mtim.tv_nsec;
    index.data_device = st.st_dev; index.data_inode = st.st_ino;
    index.tree = tree_create(kind);
    READER *reader = reader_open(input, reader_name);
    ITEM_STORE *store = item_store_create();
    if (index.path == NULL || index.key == NULL || index.tree == NULL || reader == NULL || store == NULL) {
        fprintf(stderr, "Falha ao iniciar a indexação ou abrir a base\n");
        index_release(&index); reader_close(&reader); item_store_destroy(&store); return 1;
    }
    bool found_column = false;
    for (size_t i = 0; i < reader_column_count(reader); ++i)
        if (strcmp(reader_column_name(reader, i), key) == 0) found_column = true;
    if (!found_column) {
        fprintf(stderr, "Coluna '%s' não existe na base\n", key);
        index_release(&index); reader_close(&reader); item_store_destroy(&store); return 1;
    }
    double start = now_ms();
    uint64_t input_records = 0;
    READER_STATUS status = READER_OK;
    while ((limit == 0 || input_records < limit) && status == READER_OK) {
        ITEM *item = NULL;
        status = reader_next(reader, store, &item);
        if (status == READER_OK) {
            const char *value = item_get(item, key);
            if (value == NULL || !tree_insert(index.tree, value, reader_last_offset(reader))) {
                status = READER_ERROR;
                break;
            }
            input_records++;
            item_store_destroy(&store);
            store = item_store_create();
            if (store == NULL) { status = READER_ERROR; break; }
        }
    }
    double build_ms = now_ms() - start;
    if (status == READER_ERROR || store == NULL) {
        fprintf(stderr, "Falha ao indexar: %s\n", reader_error(reader));
        index_release(&index); reader_close(&reader); item_store_destroy(&store); return 1;
    }
    start = now_ms();
    bool saved = write_index(index_path, &index, input_records);
    double save_ms = now_ms() - start;
    printf("STATUS=%s\nTREE=%s\nKEY=%s\nINPUT_RECORDS=%" PRIu64 "\nINDEXED_RECORDS=%" PRIu64 "\nDISTINCT_KEYS=%zu\nTREE_HEIGHT=%zu\nINSERT_COMPARISONS=%" PRIu64 "\nROTATIONS=%" PRIu64 "\nRECOLORS=%" PRIu64 "\nBUILD_MS=%.3f\nSAVE_INDEX_MS=%.3f\n",
        saved ? "OK" : "ERROR", kind_name(kind), key, input_records, input_records,
        tree_distinct_keys(index.tree), tree_height(index.tree), tree_comparisons(index.tree),
        tree_rotations(index.tree), tree_recolors(index.tree), build_ms, save_ms);
    index_release(&index); reader_close(&reader); item_store_destroy(&store);
    return saved ? 0 : 1;
}

static bool offsets_add(OFFSETS *set, uint64_t value) {
    if (set->count == set->capacity) {
        size_t cap = set->capacity == 0 ? 32 : set->capacity * 2;
        uint64_t *items = realloc(set->items, cap * sizeof(*items));
        if (items == NULL) return false;
        set->items = items; set->capacity = cap;
    }
    set->items[set->count++] = value;
    return true;
}

static int compare_offsets(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return x < y ? -1 : x > y ? 1 : 0;
}

static size_t offsets_unique(OFFSETS *set) {
    if (set->count == 0) return 0;
    qsort(set->items, set->count, sizeof(*set->items), compare_offsets);
    size_t unique = 1;
    for (size_t i = 1; i < set->count; ++i)
        if (set->items[i] != set->items[unique - 1])
            set->items[unique++] = set->items[i];
    set->count = unique;
    return unique;
}

/* Copia um registro CSV cru; respeita quebras de linha dentro de campos citados. */
static bool copy_csv_record(FILE *input, FILE *output) {
    bool quoted = false, wrote = false;
    int ch;
    while ((ch = fgetc(input)) != EOF) {
        wrote = true;
        if (fputc(ch, output) == EOF) return false;
        if (ch == '"') {
            if (quoted) {
                int next = fgetc(input);
                if (next == '"') {
                    if (fputc(next, output) == EOF) return false;
                    continue;
                }
                quoted = false;
                if (next != EOF) ungetc(next, input);
            } else quoted = true;
        } else if (ch == '\n' && !quoted) return true;
    }
    return wrote && !ferror(input);
}

static int run_search(int argc, char **argv) {
    int count;
    const char *input = option_value(argc, argv, "--input", &count);
    const char *query = option_value(argc, argv, "--query", &count);
    const char *output = option_value(argc, argv, "--output", &count);
    size_t index_count = 0;
    for (int i = 2; i + 1 < argc; ++i) if (strcmp(argv[i], "--index") == 0) index_count++;
    if (input == NULL || query == NULL || output == NULL || index_count == 0) {
        fprintf(stderr, "Uso: trabalho --mode search --reader nasa|games --input CSV --index IDX [--index IDX] --query ARQUIVO --output CSV\n");
        return 2;
    }
    INDEX *indices = calloc(index_count, sizeof(*indices));
    if (indices == NULL) return 1;
    struct stat st;
    if (stat(input, &st) != 0) { perror(input); free(indices); return 1; }
    double start = now_ms();
    size_t loaded = 0;
    bool ok = true;
    for (int i = 2; i + 1 < argc; ++i) {
        if (strcmp(argv[i], "--index") == 0) {
            if (!read_index(argv[i + 1], input, &indices[loaded])) { ok = false; break; }
            loaded++;
        }
    }
    double load_ms = now_ms() - start;
    if (!ok) {
        fprintf(stderr, "Índice inválido ou incompatível com o arquivo de dados\n");
        for (size_t i = 0; i < loaded; ++i) index_release(&indices[i]);
        free(indices); return 1;
    }
    FILE *queries = fopen(query, "r");
    if (queries == NULL) { perror(query); for (size_t i=0;i<loaded;i++) index_release(&indices[i]); free(indices); return 1; }
    OFFSETS offsets = {0};
    char *line = NULL; size_t cap = 0; ssize_t length;
    uint64_t query_keys = 0, search_comparisons = 0;
    start = now_ms();
    while ((length = getline(&line, &cap, queries)) >= 0) {
        if (length > 0 && line[length - 1] == '\n') line[--length] = '\0';
        if (length > 0 && line[length - 1] == '\r') line[--length] = '\0';
        if (length == 0) continue;
        char *first = strchr(line, '|');
        char *second = first == NULL ? NULL : strchr(first + 1, '|');
        if (first == NULL || second == NULL || strncmp(line, "KEY|", 4) != 0) { ok = false; break; }
        *second = '\0';
        const char *column = first + 1, *key = second + 1;
        INDEX *selected = NULL;
        for (size_t i = 0; i < loaded; ++i) if (strcmp(indices[i].key, column) == 0) selected = &indices[i];
        if (selected == NULL) { ok = false; break; }
        TREE_OFFSETS matches;
        query_keys++;
        if (tree_find(selected->tree, key, &matches))
            for (size_t i = 0; i < matches.count; ++i) if (!offsets_add(&offsets, matches.items[i])) { ok = false; break; }
        if (!ok) break;
    }
    fclose(queries); free(line);
    for (size_t i = 0; i < loaded; ++i) search_comparisons += tree_comparisons(indices[i].tree);
    double search_ms = now_ms() - start;
    if (ok) offsets_unique(&offsets);
    const char *reader_name = option_value(argc, argv, "--reader", &count);
    READER *reader = ok ? reader_open(input, reader_name == NULL ? "nasa" : reader_name) : NULL;
    FILE *data = ok ? fopen(input, "rb") : NULL;
    FILE *out = ok ? fopen(output, "wb") : NULL;
    if (reader == NULL || data == NULL || out == NULL) ok = false;
    double fetch_ms = 0, write_ms = 0;
    if (ok) {
        start = now_ms();
        if (!copy_csv_record(data, out)) ok = false;
        write_ms += now_ms() - start;
        for (size_t i = 0; ok && i < offsets.count; ++i) {
            char *record = NULL;
            size_t record_length = 0;
            FILE *buffer = open_memstream(&record, &record_length);
            if (buffer == NULL) { ok = false; break; }
            start = now_ms();
            bool fetched = fseeko(data, (off_t)offsets.items[i], SEEK_SET) == 0 &&
                           copy_csv_record(data, buffer);
            if (fclose(buffer) != 0) fetched = false;
            fetch_ms += now_ms() - start;
            if (!fetched) { free(record); ok = false; break; }
            start = now_ms();
            if (fwrite(record, 1, record_length, out) != record_length) ok = false;
            write_ms += now_ms() - start;
            free(record);
        }
    }
    if (out != NULL) {
        start = now_ms();
        if (fflush(out) != 0) ok = false;
        write_ms += now_ms() - start;
        if (fclose(out) != 0) ok = false;
    }
    if (data != NULL) fclose(data);
    reader_close(&reader);
    printf("STATUS=%s\nINDEX_COUNT=%zu\nQUERY_KEYS=%" PRIu64 "\nSEARCH_COMPARISONS=%" PRIu64 "\nMATCHED_OFFSETS=%zu\nOUTPUT_RECORDS=%zu\nLOAD_INDEX_MS=%.3f\nSEARCH_MS=%.3f\nFETCH_MS=%.3f\nWRITE_MS=%.3f\nTOTAL_MS=%.3f\n",
        ok ? "OK" : "ERROR", loaded, query_keys, search_comparisons, offsets.count, offsets.count,
        load_ms, search_ms, fetch_ms, write_ms, load_ms + search_ms + fetch_ms + write_ms);
    for (size_t i = 0; i < loaded; ++i) index_release(&indices[i]);
    free(indices); free(offsets.items);
    if (!ok) fprintf(stderr, "Falha na consulta, nos índices ou na leitura/escrita dos registros\n");
    return ok ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc < 3 || strcmp(argv[1], "--mode") != 0) {
        fprintf(stderr, "Modos: --mode index | --mode search\n");
        return 2;
    }
    if (strcmp(argv[2], "index") == 0) return run_index(argc, argv);
    if (strcmp(argv[2], "search") == 0) return run_search(argc, argv);
    fprintf(stderr, "Modo inválido: %s\n", argv[2]);
    return 2;
}
