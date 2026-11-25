#include "../include/tree_entry.h"

#include <stdlib.h>
#include <string.h>

bool tree_entry_init(TREE_ENTRY *entry, const char *key, uint64_t offset) {
    if (entry == NULL || key == NULL) return false;
    memset(entry, 0, sizeof(*entry));
    size_t length = strlen(key) + 1;
    entry->key = malloc(length);
    entry->offsets = malloc(sizeof(*entry->offsets));
    if (entry->key == NULL || entry->offsets == NULL) {
        tree_entry_destroy(entry);
        return false;
    }
    memcpy(entry->key, key, length);
    entry->offsets[0] = offset;
    entry->offset_count = entry->offset_capacity = 1;
    return true;
}

bool tree_entry_append(TREE_ENTRY *entry, uint64_t offset) {
    if (entry == NULL) return false;
    if (entry->offset_count == entry->offset_capacity) {
        size_t capacity = entry->offset_capacity == 0 ? 4 : entry->offset_capacity * 2;
        uint64_t *offsets = realloc(entry->offsets, capacity * sizeof(*offsets));
        if (offsets == NULL) return false;
        entry->offsets = offsets;
        entry->offset_capacity = capacity;
    }
    entry->offsets[entry->offset_count++] = offset;
    return true;
}

void tree_entry_destroy(TREE_ENTRY *entry) {
    if (entry == NULL) return;
    free(entry->key);
    free(entry->offsets);
    memset(entry, 0, sizeof(*entry));
}

bool tree_entry_write(FILE *file, const TREE_ENTRY *entry) {
    if (file == NULL || entry == NULL || entry->key == NULL || entry->offset_count > UINT64_MAX)
        return false;
    size_t length = strlen(entry->key);
    if (length > UINT32_MAX) return false;
    uint32_t key_length = (uint32_t)length;
    uint64_t offset_count = (uint64_t)entry->offset_count;
    return fwrite(&key_length, sizeof(key_length), 1, file) == 1 &&
        fwrite(entry->key, 1, key_length, file) == key_length &&
        fwrite(&offset_count, sizeof(offset_count), 1, file) == 1 &&
        fwrite(entry->offsets, sizeof(*entry->offsets), entry->offset_count, file) == entry->offset_count;
}

bool tree_entry_read(FILE *file, TREE_ENTRY *entry) {
    if (file == NULL || entry == NULL) return false;
    memset(entry, 0, sizeof(*entry));
    uint32_t key_length;
    uint64_t offset_count;
    if (fread(&key_length, sizeof(key_length), 1, file) != 1 || key_length > 1048576) return false;
    entry->key = malloc((size_t)key_length + 1);
    if (entry->key == NULL || fread(entry->key, 1, key_length, file) != key_length ||
        fread(&offset_count, sizeof(offset_count), 1, file) != 1 ||
        offset_count > SIZE_MAX / sizeof(*entry->offsets)) {
        tree_entry_destroy(entry);
        return false;
    }
    entry->key[key_length] = '\0';
    entry->offset_count = entry->offset_capacity = (size_t)offset_count;
    if (offset_count != 0) {
        entry->offsets = malloc((size_t)offset_count * sizeof(*entry->offsets));
        if (entry->offsets == NULL || fread(entry->offsets, sizeof(*entry->offsets),
                                            (size_t)offset_count, file) != (size_t)offset_count) {
            tree_entry_destroy(entry);
            return false;
        }
    }
    return true;
}
