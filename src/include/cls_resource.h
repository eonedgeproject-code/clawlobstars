/*
 * ClawLobstars - Resource Management
 * CPU/memory/IO monitoring, load balancing, failure recovery
 */

#ifndef CLS_RESOURCE_H
#define CLS_RESOURCE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLS_RES_HISTORY_SIZE 64

/* Resource type */
typedef enum {
    CLS_RES_CPU      = 0,
    CLS_RES_MEMORY   = 1,
    CLS_RES_IO       = 2,
    CLS_RES_NETWORK  = 3,
    CLS_RES_CUSTOM   = 255
} cls_resource_type_t;

/* Health status */
typedef enum {
    CLS_HEALTH_OK       = 0,
    CLS_HEALTH_WARN     = 1,
    CLS_HEALTH_CRITICAL = 2,
    CLS_HEALTH_FAILED   = 3
} cls_health_status_t;

/* Resource snapshot */
typedef struct {
    float       cpu_usage;       /* 0.0 - 1.0 */
    size_t      mem_used;
    size_t      mem_total;
    float       mem_usage;       /* 0.0 - 1.0 */
    uint64_t    io_read_bytes;
    uint64_t    io_write_bytes;
    uint64_t    timestamp_us;
} cls_resource_snap_t;

/* Resource limits */
typedef struct {
    float       cpu_warn_threshold;
    float       cpu_critical_threshold;
    float       mem_warn_threshold;
    float       mem_critical_threshold;
    size_t      mem_max_bytes;
} cls_resource_limits_t;

/* Recovery action */
typedef struct {
    uint32_t             action_id;
    cls_health_status_t  trigger_status;
    cls_resource_type_t  resource_type;
    bool               (*recovery_fn)(void *ctx);
    void                *ctx;
} cls_recovery_action_t;

/* Resource manager context */
struct cls_resource_mgr {
    cls_resource_snap_t   history[CLS_RES_HISTORY_SIZE];
    uint32_t              history_head;
    uint32_t              history_count;
    cls_resource_snap_t   current;
    cls_resource_limits_t limits;
    cls_health_status_t   health;
    cls_recovery_action_t *recoveries;
    uint32_t              recovery_count;
    uint32_t              max_recoveries;
    uint64_t              total_warnings;
    uint64_t              total_criticals;
    uint64_t              total_recoveries;
};

/* ---- API ---- */

cls_status_t cls_resource_init(cls_resource_mgr_t *mgr, const cls_resource_limits_t *limits,
                                uint32_t max_recoveries);

/* Monitoring */
cls_status_t cls_resource_update(cls_resource_mgr_t *mgr);
cls_status_t cls_resource_snapshot(const cls_resource_mgr_t *mgr, cls_resource_snap_t *snap);
cls_health_status_t cls_resource_health(const cls_resource_mgr_t *mgr);

/* Resource check (can we allocate N bytes?) */
bool cls_resource_can_alloc(const cls_resource_mgr_t *mgr, size_t bytes);
float cls_resource_available_cpu(const cls_resource_mgr_t *mgr);

/* History / trending */
cls_status_t cls_resource_get_history(const cls_resource_mgr_t *mgr,
                                       cls_resource_snap_t *snaps,
                                       uint32_t *count, uint32_t max_count);
float cls_resource_avg_cpu(const cls_resource_mgr_t *mgr, uint32_t last_n);
float cls_resource_avg_mem(const cls_resource_mgr_t *mgr, uint32_t last_n);

/* Recovery actions */
cls_status_t cls_resource_add_recovery(cls_resource_mgr_t *mgr,
                                        const cls_recovery_action_t *action);
cls_status_t cls_resource_check_and_recover(cls_resource_mgr_t *mgr);

/* Manual GC trigger */
cls_status_t cls_resource_gc(cls_resource_mgr_t *mgr, cls_memory_ctx_t *memory);

void cls_resource_destroy(cls_resource_mgr_t *mgr);

#ifdef __cplusplus
}
#endif

#endif /* CLS_RESOURCE_H */
