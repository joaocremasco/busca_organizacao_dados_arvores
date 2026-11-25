#include "item.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

#define MAX_ITEM_OFFSETS UINT64_C(10000000)

bool item_init(ITEM *item, const char *key, uint64_t offset) {
    if (item == NULL || key == NULL) return false;
    memset(item, 0, sizeof(*item));
    item->key = str_dup(key);
    item->offsets = malloc(sizeof(*item->offsets));
    if (item->key == NULL || item->offsets == NULL) {
        item_destroy(item);
        return false;
    }
    item->offsets[0] = offset;
    item->offset_count = 1;
    item->offset_capacity = 1;
    return true;
}

bool item_append(ITEM *item, uint64_t offset) {
    size_t capacity;
    uint64_t *grown;
    if (item == NULL) return false;
    if (item->offset_count == item->offset_capacity) {
        capacity = item->offset_capacity == 0 ? 4 : item->offset_capacity * 2;
        if (capacity < item->offset_capacity) return false;
        grown = realloc(item->offsets, capacity * sizeof(*grown));
        if (grown == NULL) return false;
        item->offsets = grown;
        item->offset_capacity = capacity;
    }
    item->offsets[item->offset_count++] = offset;
    return true;
}

void item_destroy(ITEM *item) {
    if (item == NULL) return;
    free(item->key);
    free(item->offsets);
    memset(item, 0, sizeof(*item));
}

bool item_write(FILE *file, const ITEM *item) {
    size_t length;
    size_t i;
    if (file == NULL || item == NULL || item->key == NULL || item->offset_count == 0)
        return false;
    length = strlen(item->key);
    if (length > UINT32_MAX) return false;
    if (!write_u32(file, (uint32_t)length) || !write_u64(file, (uint64_t)item->offset_count))
        return false;
    if (fwrite(item->key, 1, length, file) != length) return false;
    for (i = 0; i < item->offset_count; i++) {
        if (!write_u64(file, item->offsets[i])) return false;
    }
    return true;
}

bool item_read(FILE *file, ITEM *item, uint64_t data_size,
               uint64_t *record_count) {
    uint32_t key_length;
    uint64_t offset_count, remaining, i, offset;
    if (file == NULL || item == NULL || record_count == NULL) return false;
    memset(item, 0, sizeof(*item));
    if (!read_u32(file, &key_length) || key_length > 1048576 ||
        !read_u64(file, &offset_count) || offset_count == 0) return false;
    if (offset_count > MAX_ITEM_OFFSETS ||
        !file_remaining(file, &remaining) || key_length > remaining) return false;
    remaining -= key_length;
    if (offset_count > remaining / 8 || offset_count > (uint64_t)(size_t)-1 / sizeof(*item->offsets))
        return false;
    item->key = malloc((size_t)key_length + 1);
    item->offsets = malloc((size_t)offset_count * sizeof(*item->offsets));
    if (item->key == NULL || item->offsets == NULL ||
        fread(item->key, 1, key_length, file) != key_length) {
        item_destroy(item);
        return false;
    }
    item->key[key_length] = '\0';
    for (i = 0; i < offset_count; i++) {
        if (!read_u64(file, &offset) || offset >= data_size) {
            item_destroy(item);
            return false;
        }
        item->offsets[i] = offset;
    }
    if (*record_count > UINT64_MAX - offset_count) {
        item_destroy(item);
        return false;
    }
    item->offset_count = (size_t)offset_count;
    item->offset_capacity = (size_t)offset_count;
    *record_count += offset_count;
    return true;
}
