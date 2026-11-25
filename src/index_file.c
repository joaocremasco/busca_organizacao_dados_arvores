#define _POSIX_C_SOURCE 200809L
#include "index_file.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define INDEX_VERSION 3U
#define KEY_TYPE_STRING 1U
#define CHECKSUM_BLOCK 4096U


static const char *base_name(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash == NULL ? path : slash + 1;
}

static uint64_t hash_bytes(uint64_t hash, const unsigned char *bytes, size_t count) {
    size_t i;
    for (i = 0; i < count; i++) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static bool hash_header(FILE *file, uint64_t *hash) {
    uint64_t value = UINT64_C(14695981039346656037);
    size_t count = 0;
    int ch;
    while ((ch = fgetc(file)) != EOF) {
        unsigned char byte = (unsigned char)ch;
        value = hash_bytes(value, &byte, 1);
        count++;
        if (ch == '\n') break;
        if (count > 1048576) return false;
    }
    if (ferror(file) || count == 0) return false;
    *hash = value;
    return true;
}

static bool hash_block(FILE *file, uint64_t offset, size_t count, uint64_t *hash) {
    unsigned char buffer[CHECKSUM_BLOCK];
    size_t read_count;
    if (fseeko(file, (off_t)offset, SEEK_SET) != 0) return false;
    read_count = fread(buffer, 1, count, file);
    if (read_count != count) return false;
    *hash = hash_bytes(*hash, buffer, count);
    return true;
}

static bool data_checksum(const char *path, uint64_t size, uint64_t *checksum) {
    FILE *file = fopen(path, "rb");
    uint64_t hash;
    size_t block = size < CHECKSUM_BLOCK ? (size_t)size : CHECKSUM_BLOCK;
    bool ok;
    if (file == NULL) return false;
    ok = hash_header(file, &hash) && hash_block(file, 0, block, &hash) &&
         hash_block(file, size - block, block, &hash);
    if (fclose(file) != 0) ok = false;
    if (ok) *checksum = hash;
    return ok;
}

bool index_file_create(INDEX_FILE *index, const char *input, const char *key,
                       TREE_KIND kind, TREE *tree, uint64_t records,
                       const TREE_STATS *stats) {
    struct stat info;
    memset(index, 0, sizeof(*index));
    index->tree = tree;
    if (stat(input, &info) != 0 || info.st_size < 0) {
        index_file_destroy(index);
        return false;
    }
    index->key = str_dup(key);
    index->base_name = str_dup(base_name(input));
    index->kind = kind;
    index->data_size = (uint64_t)info.st_size;
    index->data_mtime = (int64_t)info.st_mtime;
    index->input_records = records;
    index->distinct_keys = (uint64_t)stats->distinct;
    if (index->key == NULL || index->base_name == NULL ||
        !data_checksum(input, index->data_size, &index->checksum)) {
        index_file_destroy(index);
        return false;
    }
    return true;
}

static bool write_text(FILE *file, const char *text) {
    size_t length = strlen(text);
    if (length > UINT32_MAX) return false;
    return write_u32(file, (uint32_t)length) &&
           fwrite(text, 1, length, file) == length;
}

bool index_file_save(const char *path, const INDEX_FILE *index) {
    static const unsigned char magic[8] = {'A', 'E', 'D', '1', 'I', 'D', 'X', '2'};
    FILE *file = fopen(path, "wb");
    bool ok;
    if (file == NULL) {
        fprintf(stderr, "Não foi possível criar o arquivo de índice '%s': %s\n",
                path, strerror(errno));
        return false;
    }
    ok = fwrite(magic, 1, sizeof(magic), file) == sizeof(magic) &&
         write_u32(file, INDEX_VERSION) && write_u32(file, (uint32_t)index->kind) &&
         write_u32(file, KEY_TYPE_STRING) && write_text(file, index->key) &&
         write_text(file, index->base_name) && write_u64(file, index->data_size) &&
         write_u64(file, (uint64_t)index->data_mtime) &&
         write_u64(file, index->input_records) &&
         write_u64(file, index->distinct_keys) &&
         write_u64(file, index->checksum) && tree_write(file, index->tree);
    if (fclose(file) != 0) ok = false;
    if (!ok) fprintf(stderr, "Falha ao gravar índice '%s'\n", path);
    return ok;
}

static bool read_text(FILE *file, char **text, uint32_t max_length) {
    uint32_t length;
    char *value;
    if (!read_u32(file, &length) || length > max_length) return false;
    value = malloc((size_t)length + 1);
    if (value == NULL || fread(value, 1, length, file) != length ||
        memchr(value, '\0', length) != NULL) {
        free(value);
        return false;
    }
    value[length] = '\0';
    *text = value;
    return true;
}

static bool read_header(FILE *file, INDEX_FILE *index) {
    static const unsigned char expected_magic[8] = {'A', 'E', 'D', '1', 'I', 'D', 'X', '2'};
    unsigned char magic[8];
    uint32_t version, kind, key_type;
    uint64_t mtime;
    if (fread(magic, 1, sizeof(magic), file) != sizeof(magic) ||
        memcmp(magic, expected_magic, sizeof(magic)) != 0) {
        fprintf(stderr, "Índice inválido: assinatura desconhecida\n");
        return false;
    }
    if (!read_u32(file, &version) || version != INDEX_VERSION) {
        fprintf(stderr, "Índice inválido: versão desconhecida\n");
        return false;
    }
    if (!read_u32(file, &kind) || kind < TREE_ABB || kind > TREE_RB) {
        fprintf(stderr, "Índice inválido: tipo de árvore inesperado\n");
        return false;
    }
    if (!read_u32(file, &key_type) || key_type != KEY_TYPE_STRING ||
        !read_text(file, &index->key, 65535) || !read_text(file, &index->base_name, 4096) ||
        !read_u64(file, &index->data_size) || !read_u64(file, &mtime) ||
        !read_u64(file, &index->input_records)) {
        fprintf(stderr, "Índice inválido: cabeçalho truncado\n");
        return false;
    }
    index->data_mtime = (int64_t)mtime;
    index->kind = (TREE_KIND)kind;
    return true;
}

static bool validate_data(const char *input, INDEX_FILE *index) {
    struct stat info;
    uint64_t checksum;
    if (stat(input, &info) != 0) {
        fprintf(stderr, "Não foi possível abrir a base '%s': %s\n", input, strerror(errno));
        return false;
    }
    if ((uint64_t)info.st_size != index->data_size) {
        fprintf(stderr, "Índice incompatível: tamanho da base diferente\n");
        return false;
    }
    if ((int64_t)info.st_mtime != index->data_mtime) {
        fprintf(stderr, "Índice incompatível: data de modificação diferente\n");
        return false;
    }
    if (strcmp(base_name(input), index->base_name) != 0) {
        fprintf(stderr, "Índice incompatível: nome da base diferente\n");
        return false;
    }
    if (!data_checksum(input, index->data_size, &checksum) || checksum != index->checksum) {
        fprintf(stderr, "Índice incompatível: checksum da base diferente\n");
        return false;
    }
    return true;
}

static bool load_tree(FILE *file, INDEX_FILE *index, uint64_t distinct) {
    uint64_t total_records = 0;
    int trailing;
    index->tree = tree_read(file, index->kind, index->data_size, &total_records);
    if (index->tree == NULL) {
        fprintf(stderr, "Índice inválido: topologia da árvore corrompida\n");
        return false;
    }
    if (total_records != index->input_records) {
        fprintf(stderr, "Índice inválido: contagem de offsets incorreta\n");
        return false;
    }
    if ((uint64_t)tree_stats(index->tree).distinct != distinct) {
        fprintf(stderr, "Índice inválido: contagem de chaves distintas incorreta\n");
        return false;
    }
    trailing = fgetc(file);
    if (trailing != EOF || ferror(file)) {
        fprintf(stderr, "Índice inválido: dados excedentes após a árvore\n");
        return false;
    }
    return true;
}

bool index_file_load(const char *path, const char *input, INDEX_FILE *index) {
    FILE *file;
    uint64_t distinct;
    bool ok;
    memset(index, 0, sizeof(*index));
    file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Não foi possível abrir índice '%s': %s\n", path, strerror(errno));
        return false;
    }
    ok = read_header(file, index);
    if (ok && (!read_u64(file, &distinct) || !read_u64(file, &index->checksum))) {
        fprintf(stderr, "Índice inválido: metadados truncados\n");
        ok = false;
    }
    if (ok) ok = validate_data(input, index);
    if (ok) ok = load_tree(file, index, distinct);
    if (fclose(file) != 0) ok = false;
    if (!ok) {
        index_file_destroy(index);
    }
    return ok;
}

void index_file_destroy(INDEX_FILE *index) {
    if (index == NULL) return;
    free(index->key);
    free(index->base_name);
    tree_destroy(&index->tree);
    memset(index, 0, sizeof(*index));
}
