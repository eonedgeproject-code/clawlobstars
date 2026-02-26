/*
 * ClawLobstars - Diagnostics Subsystem
 * Runtime profiling, health checks, metrics collection, and watchdog
 */

#ifndef CLS_DIAGNOSTICS_H
#define CLS_DIAGNOSTICS_H

#include "cls_framework.h"

/* ============================================================
 * Constants
 * ============================================================ */
#define CLS_DIAG_MAX_PROBES     32
#define CLS_DIAG_MAX_METRICS    64
#define CLS_DIAG_HISTORY_SIZE   64
#define CLS_DIAG_NAME_LEN       48

/* ============================================================
 * Types
 * ============================================================ */
typedef enum {
    CLS_HEALTH_OK       = 0,
    CLS_HEALTH_DEGRADED = 1,
    CLS_HEALTH_CRITICAL = 2,
    CLS_HEALTH_DEAD     = 3
} cls_health_status_t;

typedef enum {
    CLS_METRIC_COUNTER  = 0,   /* monotonically increasing */
    CLS_METRIC_GAUGE    = 1,   /* current value */
    CLS_METRIC_TIMER    = 2    /* duration in microseconds */
} cls_metric_type_t;

/* Health probe callback */
typedef cls_health_status_t (*cls_health_probe_fn)(void *userdata);

typedef struct {
    char                name[CLS_DIAG_NAME_LEN];
    cls_health_probe_fn probe;
    void               *userdata;
    cls_health_status_t last_status;
    uint64_t            last_check_us;
    uint64_t            check_interval_us;
    uint32_t            consecutive_fails;
    bool                active;
} cls_health_probe_t;

typedef struct {
    char              name[CLS_DIAG_NAME_LEN];
    cls_metric_type_t type;
    double            value;
    double            min;
    double            max;
    double            sum;
    uint64_t          count;
    uint64_t          last_update_us;
    bool              active;
} cls_metric_t;

typedef struct {
    uint64_t           timestamp_us;
    cls_health_status_t overall;
    uint32_t           probes_ok;
    uint32_t           probes_degraded;
    uint32_t           probes_critical;
} cls_diag_snapshot_t;

typedef struct {
    /* Health probes */
    cls_health_probe_t  probes[CLS_DIAG_MAX_PROBES];
    uint32_t            probe_count;

    /* Metrics */
    cls_metric_t        metrics[CLS_DIAG_MAX_METRICS];
    uint32_t            metric_count;

    /* History ring */
    cls_diag_snapshot_t history[CLS_DIAG_HISTORY_SIZE];
    uint32_t            history_head;
    uint32_t            history_count;

    /* Watchdog */
    uint64_t            watchdog_timeout_us;
    uint64_t            watchdog_last_pet;
    bool                watchdog_enabled;

    /* Uptime */
    uint64_t            start_time_us;
    uint64_t            total_checks;
    bool                initialized;
} cls_diagnostics_t;

/* ============================================================
 * API
 * ============================================================ */
cls_status_t cls_diag_init(cls_diagnostics_t *diag);

/* Health probes */
cls_status_t cls_diag_register_probe(cls_diagnostics_t *diag, const char *name,
                                      cls_health_probe_fn fn, void *userdata,
                                      uint64_t interval_us);
cls_status_t cls_diag_run_checks(cls_diagnostics_t *diag);
cls_health_status_t cls_diag_overall_health(const cls_diagnostics_t *diag);

/* Metrics */
cls_status_t cls_diag_counter_inc(cls_diagnostics_t *diag, const char *name, double amount);
cls_status_t cls_diag_gauge_set(cls_diagnostics_t *diag, const char *name, double value);
cls_status_t cls_diag_timer_record(cls_diagnostics_t *diag, const char *name, uint64_t duration_us);
cls_status_t cls_diag_get_metric(const cls_diagnostics_t *diag, const char *name,
                                  cls_metric_t *out);

/* Watchdog */
cls_status_t cls_diag_watchdog_enable(cls_diagnostics_t *diag, uint64_t timeout_us);
cls_status_t cls_diag_watchdog_pet(cls_diagnostics_t *diag);
bool         cls_diag_watchdog_expired(const cls_diagnostics_t *diag);

/* Reports */
cls_status_t cls_diag_snapshot(cls_diagnostics_t *diag, cls_diag_snapshot_t *out);
uint64_t     cls_diag_uptime_us(const cls_diagnostics_t *diag);
cls_status_t cls_diag_dump(const cls_diagnostics_t *diag, FILE *fp);
void         cls_diag_destroy(cls_diagnostics_t *diag);

#endif /* CLS_DIAGNOSTICS_H */
