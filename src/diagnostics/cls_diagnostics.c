/*
 * ClawLobstars - Diagnostics Subsystem Implementation
 * Health probes, metrics, watchdog timer, and runtime profiling
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "../include/cls_framework.h"
#include "../include/cls_diagnostics.h"

static uint64_t diag_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* ============================================================
 * Init
 * ============================================================ */

cls_status_t cls_diag_init(cls_diagnostics_t *diag) {
    if (!diag) return CLS_ERR_INVALID;
    memset(diag, 0, sizeof(cls_diagnostics_t));
    diag->start_time_us = diag_time_us();
    diag->initialized = true;
    return CLS_OK;
}

/* ============================================================
 * Health Probes
 * ============================================================ */

cls_status_t cls_diag_register_probe(cls_diagnostics_t *diag, const char *name,
                                      cls_health_probe_fn fn, void *userdata,
                                      uint64_t interval_us) {
    if (!diag || !diag->initialized || !name || !fn) return CLS_ERR_INVALID;
    if (diag->probe_count >= CLS_DIAG_MAX_PROBES) return CLS_ERR_OVERFLOW;

    cls_health_probe_t *p = &diag->probes[diag->probe_count];
    memset(p, 0, sizeof(cls_health_probe_t));

    strncpy(p->name, name, CLS_DIAG_NAME_LEN - 1);
    p->probe = fn;
    p->userdata = userdata;
    p->check_interval_us = interval_us;
    p->last_status = CLS_HEALTH_OK;
    p->active = true;

    diag->probe_count++;
    return CLS_OK;
}

cls_status_t cls_diag_run_checks(cls_diagnostics_t *diag) {
    if (!diag || !diag->initialized) return CLS_ERR_INVALID;

    uint64_t now = diag_time_us();

    for (uint32_t i = 0; i < diag->probe_count; i++) {
        cls_health_probe_t *p = &diag->probes[i];
        if (!p->active) continue;

        /* Check interval */
        if (p->last_check_us > 0 &&
            (now - p->last_check_us) < p->check_interval_us) {
            continue;
        }

        cls_health_status_t status = p->probe(p->userdata);
        p->last_status = status;
        p->last_check_us = now;

        if (status >= CLS_HEALTH_CRITICAL) {
            p->consecutive_fails++;
        } else {
            p->consecutive_fails = 0;
        }
    }

    /* Record snapshot */
    cls_diag_snapshot_t snap;
    snap.timestamp_us = now;
    snap.probes_ok = 0;
    snap.probes_degraded = 0;
    snap.probes_critical = 0;

    for (uint32_t i = 0; i < diag->probe_count; i++) {
        if (!diag->probes[i].active) continue;
        switch (diag->probes[i].last_status) {
        case CLS_HEALTH_OK:       snap.probes_ok++; break;
        case CLS_HEALTH_DEGRADED: snap.probes_degraded++; break;
        case CLS_HEALTH_CRITICAL:
        case CLS_HEALTH_DEAD:     snap.probes_critical++; break;
        }
    }

    snap.overall = cls_diag_overall_health(diag);

    /* Push to history ring */
    diag->history[diag->history_head] = snap;
    diag->history_head = (diag->history_head + 1) % CLS_DIAG_HISTORY_SIZE;
    if (diag->history_count < CLS_DIAG_HISTORY_SIZE) diag->history_count++;

    diag->total_checks++;
    return CLS_OK;
}

cls_health_status_t cls_diag_overall_health(const cls_diagnostics_t *diag) {
    if (!diag || !diag->initialized) return CLS_HEALTH_DEAD;

    cls_health_status_t worst = CLS_HEALTH_OK;

    for (uint32_t i = 0; i < diag->probe_count; i++) {
        if (!diag->probes[i].active) continue;
        if (diag->probes[i].last_status > worst) {
            worst = diag->probes[i].last_status;
        }
    }

    /* Check watchdog */
    if (diag->watchdog_enabled && cls_diag_watchdog_expired(diag)) {
        if (worst < CLS_HEALTH_CRITICAL) worst = CLS_HEALTH_CRITICAL;
    }

    return worst;
}

/* ============================================================
 * Metrics
 * ============================================================ */

static cls_metric_t *diag_find_or_create_metric(cls_diagnostics_t *diag,
                                                  const char *name,
                                                  cls_metric_type_t type) {
    /* Find existing */
    for (uint32_t i = 0; i < diag->metric_count; i++) {
        if (diag->metrics[i].active &&
            strcmp(diag->metrics[i].name, name) == 0) {
            return &diag->metrics[i];
        }
    }

    /* Create new */
    if (diag->metric_count >= CLS_DIAG_MAX_METRICS) return NULL;

    cls_metric_t *m = &diag->metrics[diag->metric_count];
    memset(m, 0, sizeof(cls_metric_t));
    strncpy(m->name, name, CLS_DIAG_NAME_LEN - 1);
    m->type = type;
    m->min = 1e18;
    m->max = -1e18;
    m->active = true;
    diag->metric_count++;

    return m;
}

cls_status_t cls_diag_counter_inc(cls_diagnostics_t *diag, const char *name,
                                   double amount) {
    if (!diag || !diag->initialized || !name) return CLS_ERR_INVALID;

    cls_metric_t *m = diag_find_or_create_metric(diag, name, CLS_METRIC_COUNTER);
    if (!m) return CLS_ERR_OVERFLOW;

    m->value += amount;
    m->count++;
    m->sum += amount;
    m->last_update_us = diag_time_us();

    if (m->value < m->min) m->min = m->value;
    if (m->value > m->max) m->max = m->value;

    return CLS_OK;
}

cls_status_t cls_diag_gauge_set(cls_diagnostics_t *diag, const char *name,
                                 double value) {
    if (!diag || !diag->initialized || !name) return CLS_ERR_INVALID;

    cls_metric_t *m = diag_find_or_create_metric(diag, name, CLS_METRIC_GAUGE);
    if (!m) return CLS_ERR_OVERFLOW;

    m->value = value;
    m->count++;
    m->sum += value;
    m->last_update_us = diag_time_us();

    if (value < m->min) m->min = value;
    if (value > m->max) m->max = value;

    return CLS_OK;
}

cls_status_t cls_diag_timer_record(cls_diagnostics_t *diag, const char *name,
                                    uint64_t duration_us) {
    if (!diag || !diag->initialized || !name) return CLS_ERR_INVALID;

    cls_metric_t *m = diag_find_or_create_metric(diag, name, CLS_METRIC_TIMER);
    if (!m) return CLS_ERR_OVERFLOW;

    double d = (double)duration_us;
    m->value = d;  /* last recorded */
    m->count++;
    m->sum += d;
    m->last_update_us = diag_time_us();

    if (d < m->min) m->min = d;
    if (d > m->max) m->max = d;

    return CLS_OK;
}

cls_status_t cls_diag_get_metric(const cls_diagnostics_t *diag, const char *name,
                                  cls_metric_t *out) {
    if (!diag || !diag->initialized || !name || !out) return CLS_ERR_INVALID;

    for (uint32_t i = 0; i < diag->metric_count; i++) {
        if (diag->metrics[i].active &&
            strcmp(diag->metrics[i].name, name) == 0) {
            *out = diag->metrics[i];
            return CLS_OK;
        }
    }
    return CLS_ERR_NOT_FOUND;
}

/* ============================================================
 * Watchdog
 * ============================================================ */

cls_status_t cls_diag_watchdog_enable(cls_diagnostics_t *diag, uint64_t timeout_us) {
    if (!diag || !diag->initialized || timeout_us == 0) return CLS_ERR_INVALID;
    diag->watchdog_timeout_us = timeout_us;
    diag->watchdog_last_pet = diag_time_us();
    diag->watchdog_enabled = true;
    return CLS_OK;
}

cls_status_t cls_diag_watchdog_pet(cls_diagnostics_t *diag) {
    if (!diag || !diag->initialized) return CLS_ERR_INVALID;
    diag->watchdog_last_pet = diag_time_us();
    return CLS_OK;
}

bool cls_diag_watchdog_expired(const cls_diagnostics_t *diag) {
    if (!diag || !diag->watchdog_enabled) return false;
    uint64_t now = diag_time_us();
    return (now - diag->watchdog_last_pet) > diag->watchdog_timeout_us;
}

/* ============================================================
 * Reports
 * ============================================================ */

cls_status_t cls_diag_snapshot(cls_diagnostics_t *diag, cls_diag_snapshot_t *out) {
    if (!diag || !out) return CLS_ERR_INVALID;

    out->timestamp_us = diag_time_us();
    out->probes_ok = 0;
    out->probes_degraded = 0;
    out->probes_critical = 0;

    for (uint32_t i = 0; i < diag->probe_count; i++) {
        if (!diag->probes[i].active) continue;
        switch (diag->probes[i].last_status) {
        case CLS_HEALTH_OK:       out->probes_ok++; break;
        case CLS_HEALTH_DEGRADED: out->probes_degraded++; break;
        case CLS_HEALTH_CRITICAL:
        case CLS_HEALTH_DEAD:     out->probes_critical++; break;
        }
    }

    out->overall = cls_diag_overall_health(diag);
    return CLS_OK;
}

uint64_t cls_diag_uptime_us(const cls_diagnostics_t *diag) {
    if (!diag || !diag->initialized) return 0;
    return diag_time_us() - diag->start_time_us;
}

cls_status_t cls_diag_dump(const cls_diagnostics_t *diag, FILE *fp) {
    if (!diag || !fp) return CLS_ERR_INVALID;

    const char *health_str[] = {"OK", "DEGRADED", "CRITICAL", "DEAD"};

    fprintf(fp, "=== DIAGNOSTICS REPORT ===\n");
    fprintf(fp, "Uptime: %.2f s\n", (double)cls_diag_uptime_us(diag) / 1e6);
    fprintf(fp, "Overall: %s\n", health_str[cls_diag_overall_health(diag)]);
    fprintf(fp, "Total checks: %lu\n\n", (unsigned long)diag->total_checks);

    fprintf(fp, "--- Health Probes ---\n");
    for (uint32_t i = 0; i < diag->probe_count; i++) {
        if (!diag->probes[i].active) continue;
        fprintf(fp, "  [%s] %s (fails: %u)\n",
                health_str[diag->probes[i].last_status],
                diag->probes[i].name,
                diag->probes[i].consecutive_fails);
    }

    fprintf(fp, "\n--- Metrics ---\n");
    for (uint32_t i = 0; i < diag->metric_count; i++) {
        if (!diag->metrics[i].active) continue;
        cls_metric_t *m = (cls_metric_t *)&diag->metrics[i];

        const char *type_str = "???";
        switch (m->type) {
        case CLS_METRIC_COUNTER: type_str = "counter"; break;
        case CLS_METRIC_GAUGE:   type_str = "gauge"; break;
        case CLS_METRIC_TIMER:   type_str = "timer"; break;
        }

        fprintf(fp, "  %s (%s): val=%.2f min=%.2f max=%.2f avg=%.2f n=%lu\n",
                m->name, type_str, m->value,
                m->count > 0 ? m->min : 0.0,
                m->count > 0 ? m->max : 0.0,
                m->count > 0 ? m->sum / (double)m->count : 0.0,
                (unsigned long)m->count);
    }

    if (diag->watchdog_enabled) {
        fprintf(fp, "\n--- Watchdog ---\n");
        fprintf(fp, "  Status: %s\n",
                cls_diag_watchdog_expired(diag) ? "EXPIRED" : "OK");
        fprintf(fp, "  Timeout: %.2f s\n",
                (double)diag->watchdog_timeout_us / 1e6);
    }

    return CLS_OK;
}

void cls_diag_destroy(cls_diagnostics_t *diag) {
    if (!diag) return;
    memset(diag, 0, sizeof(cls_diagnostics_t));
}
