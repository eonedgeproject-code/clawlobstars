/*
 * ClawLobstars - Memory Interface Implementation
 * Hash-table based key-value store with TTL support
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/cls_framework.h"

/* ============================================================
 * Internal Structures
 * ============================================================ */

#define CLS_MEM_BUCKET_COUNT    256
#define CLS_MEM_KEY_MAX         128

typedef struct cls_mem_entry {
    char                    key[CLS_MEM_KEY_MAX];
    void                   *data;
    cls_mem_entry_meta_t    meta;
    struct cls_mem_entry   *next;   /* Chaining for collisions */
} cls_mem_entry_t;

typedef struct {
    cls_mem_entry_t *buckets[CLS_MEM_BUCKET_COUNT];
} cls_mem_table_t;

/* ============================================================
 * Internal Helpers
 * ============================================================ */

static uint64_t cls_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* FNV-1a hash */
static uint32_t cls_hash(const char *key) {
    uint32_t hash = 2166136261u;
    while (*key) {
        hash ^= (uint8_t)*key++;
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t cls_bucket_idx(uint32_t hash) {
    return hash & (CLS_MEM_BUCKET_COUNT - 1);
}

static cls_mem_table_t *cls_get_table(cls_memory_ctx_t *ctx) {
    return (cls_mem_table_t *)ctx->pool;
}

static bool cls_entry_expired(const cls_mem_entry_t *entry) {
    if (entry->meta.ttl_seconds == 0) return false;
    uint64_t now = cls_time_us();
    uint64_t expiry = entry->meta.created_at + (uint64_t)entry->meta.ttl_seconds * 1000000ULL;
    return now > expiry;
}

/* ============================================================
 * API Implementation
 * ============================================================ */

cls_status_t cls_memory_init(cls_memory_ctx_t *ctx, size_t pool_size) {
    if (!ctx || pool_size < sizeof(cls_mem_table_t))
        return CLS_ERR_INVALID;

    cls_mem_table_t *table = (cls_mem_table_t *)calloc(1, sizeof(cls_mem_table_t));
    if (!table) return CLS_ERR_NOMEM;

    ctx->pool = table;
    ctx->pool_size = pool_size;
    ctx->used = sizeof(cls_mem_table_t);
    ctx->entry_count = 0;
    ctx->max_entries = (uint32_t)(pool_size / (sizeof(cls_mem_entry_t) + 64));
    ctx->hit_count = 0;
    ctx->miss_count = 0;

    return CLS_OK;
}

cls_status_t cls_memory_store(cls_memory_ctx_t *ctx, const char *key,
                               const void *data, size_t len) {
    return cls_memory_store_ttl(ctx, key, data, len, 0);
}

cls_status_t cls_memory_store_ttl(cls_memory_ctx_t *ctx, const char *key,
                                   const void *data, size_t len, uint32_t ttl_sec) {
    if (!ctx || !key || !data || len == 0)
        return CLS_ERR_INVALID;

    if (strlen(key) >= CLS_MEM_KEY_MAX)
        return CLS_ERR_OVERFLOW;

    /* Prevent absurdly large allocations */
    if (len > ctx->capacity / 2)
        return CLS_ERR_OVERFLOW;

    cls_mem_table_t *table = cls_get_table(ctx);
    uint32_t hash = cls_hash(key);
    uint32_t idx = cls_bucket_idx(hash);

    /* Check if key already exists — update in place */
    cls_mem_entry_t *entry = table->buckets[idx];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            size_t old_len = entry->meta.data_len;
            void *new_data = realloc(entry->data, len);
            if (!new_data) return CLS_ERR_NOMEM;

            memcpy(new_data, data, len);
            entry->data = new_data;
            entry->meta.data_len = len;
            entry->meta.accessed_at = cls_time_us();
            entry->meta.access_count++;
            entry->meta.ttl_seconds = ttl_sec;

            /* Fix: update used-size tracking on resize */
            ctx->used = ctx->used - old_len + len;
            return CLS_OK;
        }
        entry = entry->next;
    }

    /* New entry */
    if (ctx->entry_count >= ctx->max_entries)
        return CLS_ERR_OVERFLOW;

    entry = (cls_mem_entry_t *)calloc(1, sizeof(cls_mem_entry_t));
    if (!entry) return CLS_ERR_NOMEM;

    entry->data = malloc(len);
    if (!entry->data) {
        free(entry);
        return CLS_ERR_NOMEM;
    }

    strncpy(entry->key, key, CLS_MEM_KEY_MAX - 1);
    memcpy(entry->data, data, len);
    entry->meta.hash = hash;
    entry->meta.created_at = cls_time_us();
    entry->meta.accessed_at = entry->meta.created_at;
    entry->meta.access_count = 1;
    entry->meta.ttl_seconds = ttl_sec;
    entry->meta.data_len = len;

    /* Insert at head of bucket chain */
    entry->next = table->buckets[idx];
    table->buckets[idx] = entry;

    ctx->entry_count++;
    ctx->used += sizeof(cls_mem_entry_t) + len;

    return CLS_OK;
}

cls_status_t cls_memory_retrieve(cls_memory_ctx_t *ctx, const char *key,
                                  void *buf, size_t *len) {
    if (!ctx || !key || !buf || !len)
        return CLS_ERR_INVALID;

    cls_mem_table_t *table = cls_get_table(ctx);
    uint32_t idx = cls_bucket_idx(cls_hash(key));

    cls_mem_entry_t *entry = table->buckets[idx];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            if (cls_entry_expired(entry)) {
                ctx->miss_count++;
                return CLS_ERR_NOT_FOUND;
            }

            if (*len < entry->meta.data_len) {
                *len = entry->meta.data_len;
                return CLS_ERR_OVERFLOW;
            }

            memcpy(buf, entry->data, entry->meta.data_len);
            *len = entry->meta.data_len;
            entry->meta.accessed_at = cls_time_us();
            entry->meta.access_count++;
            ctx->hit_count++;
            return CLS_OK;
        }
        entry = entry->next;
    }

    ctx->miss_count++;
    return CLS_ERR_NOT_FOUND;
}

bool cls_memory_exists(cls_memory_ctx_t *ctx, const char *key) {
    if (!ctx || !key) return false;

    cls_mem_table_t *table = cls_get_table(ctx);
    uint32_t idx = cls_bucket_idx(cls_hash(key));

    cls_mem_entry_t *entry = table->buckets[idx];
    while (entry) {
        if (strcmp(entry->key, key) == 0 && !cls_entry_expired(entry))
            return true;
        entry = entry->next;
    }
    return false;
}

cls_status_t cls_memory_delete(cls_memory_ctx_t *ctx, const char *key) {
    if (!ctx || !key) return CLS_ERR_INVALID;

    cls_mem_table_t *table = cls_get_table(ctx);
    uint32_t idx = cls_bucket_idx(cls_hash(key));

    cls_mem_entry_t *prev = NULL;
    cls_mem_entry_t *entry = table->buckets[idx];

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            if (prev)
                prev->next = entry->next;
            else
                table->buckets[idx] = entry->next;

            ctx->used -= sizeof(cls_mem_entry_t) + entry->meta.data_len;
            ctx->entry_count--;
            free(entry->data);
            free(entry);
            return CLS_OK;
        }
        prev = entry;
        entry = entry->next;
    }

    return CLS_ERR_NOT_FOUND;
}

cls_status_t cls_memory_query(cls_memory_ctx_t *ctx, const cls_query_t *query,
                               cls_result_set_t *results) {
    if (!ctx || !query || !results)
        return CLS_ERR_INVALID;

    results->count = 0;
    cls_mem_table_t *table = cls_get_table(ctx);
    uint32_t max = query->max_results > 0 ? query->max_results : 64;
    if (max > 64) max = 64;

    for (uint32_t i = 0; i < CLS_MEM_BUCKET_COUNT && results->count < max; i++) {
        cls_mem_entry_t *entry = table->buckets[i];
        while (entry && results->count < max) {
            if (cls_entry_expired(entry)) {
                entry = entry->next;
                continue;
            }

            /* Simple wildcard: NULL or "*" matches everything */
            bool match = (!query->key_pattern || strcmp(query->key_pattern, "*") == 0);

            /* Prefix match if pattern ends with '*' */
            if (!match && query->key_pattern) {
                size_t plen = strlen(query->key_pattern);
                if (plen > 0 && query->key_pattern[plen - 1] == '*') {
                    match = (strncmp(entry->key, query->key_pattern, plen - 1) == 0);
                } else {
                    match = (strcmp(entry->key, query->key_pattern) == 0);
                }
            }

            /* Time filters */
            if (match && query->created_after > 0)
                match = (entry->meta.created_at >= query->created_after);
            if (match && query->created_before > 0)
                match = (entry->meta.created_at <= query->created_before);

            if (match) {
                uint32_t ri = results->count;
                results->entries[ri].key = entry->key;
                results->entries[ri].data = entry->data;
                results->entries[ri].data_len = entry->meta.data_len;
                results->entries[ri].meta = entry->meta;
                results->count++;
            }

            entry = entry->next;
        }
    }

    return CLS_OK;
}

uint32_t cls_memory_prune(cls_memory_ctx_t *ctx) {
    if (!ctx) return 0;

    uint32_t pruned = 0;
    cls_mem_table_t *table = cls_get_table(ctx);

    for (uint32_t i = 0; i < CLS_MEM_BUCKET_COUNT; i++) {
        cls_mem_entry_t *prev = NULL;
        cls_mem_entry_t *entry = table->buckets[i];

        while (entry) {
            cls_mem_entry_t *next = entry->next;
            if (cls_entry_expired(entry)) {
                if (prev)
                    prev->next = next;
                else
                    table->buckets[i] = next;

                ctx->used -= sizeof(cls_mem_entry_t) + entry->meta.data_len;
                ctx->entry_count--;
                free(entry->data);
                free(entry);
                pruned++;
            } else {
                prev = entry;
            }
            entry = next;
        }
    }

    return pruned;
}

void cls_memory_stats(const cls_memory_ctx_t *ctx,
                       size_t *used, size_t *total, uint32_t *entries) {
    if (!ctx) return;
    if (used)    *used = ctx->used;
    if (total)   *total = ctx->pool_size;
    if (entries) *entries = ctx->entry_count;
}

void cls_memory_destroy(cls_memory_ctx_t *ctx) {
    if (!ctx || !ctx->pool) return;

    cls_mem_table_t *table = cls_get_table(ctx);
    for (uint32_t i = 0; i < CLS_MEM_BUCKET_COUNT; i++) {
        cls_mem_entry_t *entry = table->buckets[i];
        while (entry) {
            cls_mem_entry_t *next = entry->next;
            free(entry->data);
            free(entry);
            entry = next;
        }
    }

    free(table);
    ctx->pool = NULL;
    ctx->used = 0;
    ctx->entry_count = 0;
}
