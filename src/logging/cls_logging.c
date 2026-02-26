/*
 * ClawLobstars - Logging Subsystem Implementation
 * Structured logging with multi-sink output and ring buffer
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "../include/cls_framework.h"
#include "../include/cls_logging.h"

static uint64_t log_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

const char *cls_log_level_str(cls_log_level_t level) {
    switch (level) {
    case CLS_LOG_TRACE: return "TRACE";
    case CLS_LOG_DEBUG: return "DEBUG";
    case CLS_LOG_INFO:  return "INFO ";
    case CLS_LOG_WARN:  return "WARN ";
    case CLS_LOG_ERROR: return "ERROR";
    case CLS_LOG_FATAL: return "FATAL";
    default:            return "?????";
    }
}

/* ============================================================
 * Initialization
 * ============================================================ */

cls_status_t cls_log_init(cls_logger_t *logger, cls_log_level_t level,
                           uint32_t agent_id) {
    if (!logger) return CLS_ERR_INVALID;
    memset(logger, 0, sizeof(cls_logger_t));

    logger->global_level = level;
    logger->agent_id = agent_id;
    logger->initialized = true;

    return CLS_OK;
}

/* ============================================================
 * Sink Management
 * ============================================================ */

cls_status_t cls_log_add_sink(cls_logger_t *logger, cls_log_sink_t type,
                               cls_log_level_t min_level, FILE *fp) {
    if (!logger || !logger->initialized) return CLS_ERR_INVALID;
    if (logger->sink_count >= CLS_LOG_MAX_SINKS) return CLS_ERR_OVERFLOW;

    cls_sink_config_t *sink = &logger->sinks[logger->sink_count];
    sink->type = type;
    sink->min_level = min_level;
    sink->active = true;

    switch (type) {
    case CLS_SINK_STDOUT:
        sink->fp = stdout;
        break;
    case CLS_SINK_STDERR:
        sink->fp = stderr;
        break;
    case CLS_SINK_FILE:
        if (!fp) return CLS_ERR_INVALID;
        sink->fp = fp;
        break;
    case CLS_SINK_BUFFER:
        sink->fp = NULL; /* ring buffer only */
        break;
    default:
        return CLS_ERR_INVALID;
    }

    logger->sink_count++;
    return CLS_OK;
}

cls_status_t cls_log_add_callback(cls_logger_t *logger, cls_log_callback_t cb,
                                   void *userdata, cls_log_level_t min_level) {
    if (!logger || !logger->initialized || !cb) return CLS_ERR_INVALID;
    if (logger->sink_count >= CLS_LOG_MAX_SINKS) return CLS_ERR_OVERFLOW;

    cls_sink_config_t *sink = &logger->sinks[logger->sink_count];
    sink->type = CLS_SINK_CUSTOM;
    sink->min_level = min_level;
    sink->callback = cb;
    sink->userdata = userdata;
    sink->active = true;

    logger->sink_count++;
    return CLS_OK;
}

/* ============================================================
 * Log Writing
 * ============================================================ */

static void log_emit_to_sinks(cls_logger_t *logger, const cls_log_entry_t *entry) {
    for (uint32_t i = 0; i < logger->sink_count; i++) {
        cls_sink_config_t *sink = &logger->sinks[i];
        if (!sink->active || entry->level < sink->min_level) continue;

        if (sink->type == CLS_SINK_CUSTOM && sink->callback) {
            sink->callback(entry, sink->userdata);
            continue;
        }

        if (sink->fp) {
            /* Format: [LEVEL] [agent:ID] [tag] file:line - message */
            fprintf(sink->fp, "[%s] [agent:%u] [%s] %s:%d - %s\n",
                    cls_log_level_str(entry->level),
                    entry->agent_id,
                    entry->tag,
                    entry->file ? entry->file : "?",
                    entry->line,
                    entry->message);
            fflush(sink->fp);
        }
    }
}

static void log_push_ring(cls_logger_t *logger, const cls_log_entry_t *entry) {
    uint32_t idx = logger->ring_head;
    logger->ring[idx] = *entry;
    logger->ring_head = (logger->ring_head + 1) % CLS_LOG_RING_SIZE;
    if (logger->ring_count < CLS_LOG_RING_SIZE) {
        logger->ring_count++;
    }
}

cls_status_t cls_log_write(cls_logger_t *logger, cls_log_level_t level,
                            const char *tag, const char *file, int line,
                            const char *fmt, ...) {
    if (!logger || !logger->initialized) return CLS_ERR_INVALID;

    /* Filter by global level */
    if (level < logger->global_level) return CLS_OK;

    /* Build entry */
    cls_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    entry.timestamp_us = log_time_us();
    entry.level = level;
    entry.agent_id = logger->agent_id;
    entry.file = file;
    entry.line = line;

    if (tag) {
        strncpy(entry.tag, tag, CLS_LOG_MAX_TAG - 1);
    } else {
        strncpy(entry.tag, "CORE", CLS_LOG_MAX_TAG - 1);
    }

    /* Format message */
    va_list args;
    va_start(args, fmt);
    vsnprintf(entry.message, CLS_LOG_MAX_MSG, fmt, args);
    va_end(args);

    /* Push to ring buffer */
    log_push_ring(logger, &entry);

    /* Emit to sinks */
    log_emit_to_sinks(logger, &entry);

    logger->total_logged++;
    return CLS_OK;
}

/* ============================================================
 * Query & Maintenance
 * ============================================================ */

cls_status_t cls_log_get_recent(const cls_logger_t *logger, cls_log_entry_t *out,
                                 uint32_t max_entries, uint32_t *out_count) {
    if (!logger || !out || !out_count) return CLS_ERR_INVALID;

    uint32_t count = (max_entries < logger->ring_count) ? max_entries : logger->ring_count;
    *out_count = count;

    /* Read from ring in reverse chronological order */
    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (logger->ring_head + CLS_LOG_RING_SIZE - 1 - i) % CLS_LOG_RING_SIZE;
        out[i] = logger->ring[idx];
    }

    return CLS_OK;
}

cls_status_t cls_log_clear(cls_logger_t *logger) {
    if (!logger) return CLS_ERR_INVALID;
    logger->ring_head = 0;
    logger->ring_count = 0;
    memset(logger->ring, 0, sizeof(logger->ring));
    return CLS_OK;
}

void cls_log_destroy(cls_logger_t *logger) {
    if (!logger) return;

    /* Close file sinks (but not stdout/stderr) */
    for (uint32_t i = 0; i < logger->sink_count; i++) {
        if (logger->sinks[i].type == CLS_SINK_FILE && logger->sinks[i].fp) {
            fclose(logger->sinks[i].fp);
        }
    }

    memset(logger, 0, sizeof(cls_logger_t));
}
