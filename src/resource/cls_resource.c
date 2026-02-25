/*
 * ClawLobstars - Resource Management Implementation
 * CPU/memory monitoring, health checks, auto-recovery
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "../include/cls_framework.h"

static uint64_t cls_res_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

cls_status_t cls_resource_init(cls_resource_mgr_t *mgr, const cls_resource_limits_t *limits,
                                uint32_t max_recoveries) {
    if (!mgr || !limits) return CLS_ERR_INVALID;

    memset(mgr, 0, sizeof(cls_resource_mgr_t));
    mgr->limits = *limits;
    mgr->health = CLS_HEALTH_OK;

    if (max_recoveries > 0) {
        mgr->recoveries = (cls_recovery_action_t *)calloc(max_recoveries, sizeof(cls_recovery_action_t));
        if (!mgr->recoveries) return CLS_ERR_NOMEM;
        mgr->max_recoveries = max_recoveries;
    }

    return CLS_OK;
}

cls_status_t cls_resource_update(cls_resource_mgr_t *mgr) {
    if (!mgr) return CLS_ERR_INVALID;

    cls_resource_snap_t snap;
    memset(&snap, 0, sizeof(snap));
    snap.timestamp_us = cls_res_time_us();

    /* Read /proc/self/status for memory (Linux-specific) */
    FILE *f = fopen("/proc/self/status", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                unsigned long kb = 0;
                sscanf(line + 6, "%lu", &kb);
                snap.mem_used = kb * 1024;
            }
            if (strncmp(line, "VmSize:", 7) == 0) {
                unsigned long kb = 0;
                sscanf(line + 7, "%lu", &kb);
                snap.mem_total = kb * 1024;
            }
        }
        fclose(f);
    }

    if (snap.mem_total > 0) {
        snap.mem_usage = (float)snap.mem_used / (float)snap.mem_total;
    }

    /* Read /proc/self/stat for CPU time approximation */
    f = fopen("/proc/self/stat", "r");
    if (f) {
        unsigned long utime = 0, stime = 0;
        /* Skip pid, comm, state and other fields to get utime(14) and stime(15) */
        int field = 0;
        char c;
        bool in_parens = false;
        while ((c = (char)fgetc(f)) != EOF) {
            if (c == '(') in_parens = true;
            if (c == ')') in_parens = false;
            if (!in_parens && c == ' ') {
                field++;
                if (field == 13) {
                    if (fscanf(f, "%lu %lu", &utime, &stime) != 2) {
                        utime = 0; stime = 0;
                    }
                    break;
                }
            }
        }
        fclose(f);

        /* Rough CPU usage estimate based on total user+system ticks */
        static unsigned long prev_total = 0;
        unsigned long total = utime + stime;
        if (prev_total > 0 && total > prev_total) {
            snap.cpu_usage = CLS_CLAMP((float)(total - prev_total) / 100.0f, 0.0f, 1.0f);
        }
        prev_total = total;
    }

    /* Store snapshot */
    mgr->current = snap;
    uint32_t idx = mgr->history_head;
    mgr->history[idx] = snap;
    mgr->history_head = (mgr->history_head + 1) % CLS_RES_HISTORY_SIZE;
    if (mgr->history_count < CLS_RES_HISTORY_SIZE) mgr->history_count++;

    /* Update health status */
    cls_health_status_t prev_health = mgr->health;
    mgr->health = CLS_HEALTH_OK;

    if (snap.cpu_usage > mgr->limits.cpu_critical_threshold ||
        snap.mem_usage > mgr->limits.mem_critical_threshold) {
        mgr->health = CLS_HEALTH_CRITICAL;
        mgr->total_criticals++;
    } else if (snap.cpu_usage > mgr->limits.cpu_warn_threshold ||
               snap.mem_usage > mgr->limits.mem_warn_threshold) {
        mgr->health = CLS_HEALTH_WARN;
        mgr->total_warnings++;
    }

    /* Auto-recover if health degraded */
    if (mgr->health > CLS_HEALTH_OK && prev_health <= CLS_HEALTH_OK) {
        cls_resource_check_and_recover(mgr);
    }

    return CLS_OK;
}

cls_status_t cls_resource_snapshot(const cls_resource_mgr_t *mgr, cls_resource_snap_t *snap) {
    if (!mgr || !snap) return CLS_ERR_INVALID;
    *snap = mgr->current;
    return CLS_OK;
}

cls_health_status_t cls_resource_health(const cls_resource_mgr_t *mgr) {
    return mgr ? mgr->health : CLS_HEALTH_FAILED;
}

bool cls_resource_can_alloc(const cls_resource_mgr_t *mgr, size_t bytes) {
    if (!mgr) return false;
    if (mgr->limits.mem_max_bytes == 0) return true;
    return (mgr->current.mem_used + bytes) <= mgr->limits.mem_max_bytes;
}

float cls_resource_available_cpu(const cls_resource_mgr_t *mgr) {
    if (!mgr) return 0.0f;
    return 1.0f - mgr->current.cpu_usage;
}

cls_status_t cls_resource_get_history(const cls_resource_mgr_t *mgr,
                                       cls_resource_snap_t *snaps,
                                       uint32_t *count, uint32_t max_count) {
    if (!mgr || !snaps || !count) return CLS_ERR_INVALID;

    uint32_t to_copy = CLS_MIN(mgr->history_count, max_count);
    uint32_t start;

    if (mgr->history_count < CLS_RES_HISTORY_SIZE) {
        start = 0;
    } else {
        start = mgr->history_head;
    }

    for (uint32_t i = 0; i < to_copy; i++) {
        uint32_t idx = (start + i) % CLS_RES_HISTORY_SIZE;
        snaps[i] = mgr->history[idx];
    }

    *count = to_copy;
    return CLS_OK;
}

float cls_resource_avg_cpu(const cls_resource_mgr_t *mgr, uint32_t last_n) {
    if (!mgr || mgr->history_count == 0) return 0.0f;

    uint32_t count = CLS_MIN(last_n, mgr->history_count);
    float sum = 0.0f;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx;
        if (mgr->history_head >= i + 1) {
            idx = mgr->history_head - i - 1;
        } else {
            idx = CLS_RES_HISTORY_SIZE - (i + 1 - mgr->history_head);
        }
        sum += mgr->history[idx].cpu_usage;
    }

    return sum / (float)count;
}

float cls_resource_avg_mem(const cls_resource_mgr_t *mgr, uint32_t last_n) {
    if (!mgr || mgr->history_count == 0) return 0.0f;

    uint32_t count = CLS_MIN(last_n, mgr->history_count);
    float sum = 0.0f;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx;
        if (mgr->history_head >= i + 1) {
            idx = mgr->history_head - i - 1;
        } else {
            idx = CLS_RES_HISTORY_SIZE - (i + 1 - mgr->history_head);
        }
        sum += mgr->history[idx].mem_usage;
    }

    return sum / (float)count;
}

cls_status_t cls_resource_add_recovery(cls_resource_mgr_t *mgr,
                                        const cls_recovery_action_t *action) {
    if (!mgr || !action || !action->recovery_fn) return CLS_ERR_INVALID;
    if (mgr->recovery_count >= mgr->max_recoveries) return CLS_ERR_OVERFLOW;

    mgr->recoveries[mgr->recovery_count] = *action;
    mgr->recovery_count++;
    return CLS_OK;
}

cls_status_t cls_resource_check_and_recover(cls_resource_mgr_t *mgr) {
    if (!mgr) return CLS_ERR_INVALID;

    for (uint32_t i = 0; i < mgr->recovery_count; i++) {
        cls_recovery_action_t *act = &mgr->recoveries[i];

        if (mgr->health >= act->trigger_status) {
            bool recovered = act->recovery_fn(act->ctx);
            if (recovered) {
                mgr->total_recoveries++;
                mgr->health = CLS_HEALTH_OK;
                return CLS_OK;
            }
        }
    }

    return CLS_ERR_INTERNAL;
}

cls_status_t cls_resource_gc(cls_resource_mgr_t *mgr, cls_memory_ctx_t *memory) {
    if (!mgr) return CLS_ERR_INVALID;

    if (memory) {
        uint32_t pruned = cls_memory_prune(memory);
        CLS_UNUSED(pruned);
    }

    return CLS_OK;
}

void cls_resource_destroy(cls_resource_mgr_t *mgr) {
    if (!mgr) return;
    free(mgr->recoveries);
    mgr->recoveries = NULL;
    memset(mgr, 0, sizeof(cls_resource_mgr_t));
}
