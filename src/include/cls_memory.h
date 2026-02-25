/*
 * ClawLobstars - Memory Interface
 * Manages persistent and volatile storage with cache optimization
 */

#ifndef CLS_MEMORY_H
#define CLS_MEMORY_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Memory entry metadata */
typedef struct {
    uint32_t    hash;
    uint64_t    created_at;
    uint64_t    accessed_at;
    uint32_t    access_count;
    uint32_t    ttl_seconds;
    size_t      data_len;
} cls_mem_entry_meta_t;

/* Memory context */
struct cls_memory_ctx {
    void       *pool;           /* Raw memory pool */
    size_t      pool_size;      /* Total pool size */
    size_t      used;           /* Currently used bytes */
    uint32_t    entry_count;    /* Number of stored entries */
    uint32_t    max_entries;    /* Maximum entries */
    uint32_t    hit_count;      /* Cache hits */
    uint32_t    miss_count;     /* Cache misses */
};

/* Query structure */
typedef struct {
    const char *key_pattern;    /* Key pattern (supports * wildcard) */
    uint64_t    created_after;  /* Filter: created after timestamp */
    uint64_t    created_before; /* Filter: created before timestamp */
    uint32_t    max_results;    /* Max results to return */
} cls_query_t;

/* Result set */
typedef struct {
    uint32_t    count;
    struct {
        const char *key;
        const void *data;
        size_t      data_len;
        cls_mem_entry_meta_t meta;
    } entries[64]; /* Fixed-size result buffer */
} cls_result_set_t;

/* ---- API ---- */

/* Initialize memory context with given pool size */
cls_status_t cls_memory_init(cls_memory_ctx_t *ctx, size_t pool_size);

/* Store data with key */
cls_status_t cls_memory_store(cls_memory_ctx_t *ctx, const char *key,
                               const void *data, size_t len);

/* Store data with TTL (auto-expiry) */
cls_status_t cls_memory_store_ttl(cls_memory_ctx_t *ctx, const char *key,
                                   const void *data, size_t len, uint32_t ttl_sec);

/* Retrieve data by key */
cls_status_t cls_memory_retrieve(cls_memory_ctx_t *ctx, const char *key,
                                  void *buf, size_t *len);

/* Check if key exists */
bool cls_memory_exists(cls_memory_ctx_t *ctx, const char *key);

/* Delete entry by key */
cls_status_t cls_memory_delete(cls_memory_ctx_t *ctx, const char *key);

/* Query with pattern matching */
cls_status_t cls_memory_query(cls_memory_ctx_t *ctx, const cls_query_t *query,
                               cls_result_set_t *results);

/* Prune expired entries */
uint32_t cls_memory_prune(cls_memory_ctx_t *ctx);

/* Get memory statistics */
void cls_memory_stats(const cls_memory_ctx_t *ctx,
                       size_t *used, size_t *total, uint32_t *entries);

/* Destroy and free all memory */
void cls_memory_destroy(cls_memory_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* CLS_MEMORY_H */
