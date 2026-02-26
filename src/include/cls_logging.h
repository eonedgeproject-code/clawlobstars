/*
 * ClawLobstars - Logging Subsystem
 * Structured logging with severity levels, sinks, and rotation
 */

#ifndef CLS_LOGGING_H
#define CLS_LOGGING_H

#include "cls_framework.h"
#include <stdio.h>

/* ============================================================
 * Log Levels
 * ============================================================ */
typedef enum {
    CLS_LOG_TRACE   = 0,
    CLS_LOG_DEBUG   = 1,
    CLS_LOG_INFO    = 2,
    CLS_LOG_WARN    = 3,
    CLS_LOG_ERROR   = 4,
    CLS_LOG_FATAL   = 5,
    CLS_LOG_OFF     = 6
} cls_log_level_t;

/* ============================================================
 * Log Sink Types
 * ============================================================ */
typedef enum {
    CLS_SINK_STDOUT  = 0x01,
    CLS_SINK_STDERR  = 0x02,
    CLS_SINK_FILE    = 0x04,
    CLS_SINK_BUFFER  = 0x08,
    CLS_SINK_CUSTOM  = 0x10
} cls_log_sink_t;

/* ============================================================
 * Structures
 * ============================================================ */
#define CLS_LOG_MAX_MSG      512
#define CLS_LOG_MAX_TAG       32
#define CLS_LOG_MAX_SINKS      8
#define CLS_LOG_RING_SIZE    256

typedef struct {
    uint64_t         timestamp_us;
    cls_log_level_t  level;
    uint32_t         agent_id;
    char             tag[CLS_LOG_MAX_TAG];
    char             message[CLS_LOG_MAX_MSG];
    const char      *file;
    int              line;
} cls_log_entry_t;

typedef void (*cls_log_callback_t)(const cls_log_entry_t *entry, void *userdata);

typedef struct {
    cls_log_sink_t   type;
    cls_log_level_t  min_level;
    FILE            *fp;
    cls_log_callback_t callback;
    void            *userdata;
    bool             active;
} cls_sink_config_t;

typedef struct {
    cls_log_level_t   global_level;
    cls_sink_config_t sinks[CLS_LOG_MAX_SINKS];
    uint32_t          sink_count;
    uint32_t          agent_id;

    /* Ring buffer for in-memory log */
    cls_log_entry_t   ring[CLS_LOG_RING_SIZE];
    uint32_t          ring_head;
    uint32_t          ring_count;

    /* Stats */
    uint64_t          total_logged;
    uint64_t          dropped;
    bool              initialized;
} cls_logger_t;

/* ============================================================
 * API
 * ============================================================ */
cls_status_t cls_log_init(cls_logger_t *logger, cls_log_level_t level, uint32_t agent_id);
cls_status_t cls_log_add_sink(cls_logger_t *logger, cls_log_sink_t type,
                               cls_log_level_t min_level, FILE *fp);
cls_status_t cls_log_add_callback(cls_logger_t *logger, cls_log_callback_t cb,
                                   void *userdata, cls_log_level_t min_level);
cls_status_t cls_log_write(cls_logger_t *logger, cls_log_level_t level,
                            const char *tag, const char *file, int line,
                            const char *fmt, ...);
cls_status_t cls_log_get_recent(const cls_logger_t *logger, cls_log_entry_t *out,
                                 uint32_t max_entries, uint32_t *out_count);
cls_status_t cls_log_clear(cls_logger_t *logger);
void         cls_log_destroy(cls_logger_t *logger);

const char  *cls_log_level_str(cls_log_level_t level);

/* Convenience macros */
#define CLS_TRACE(logger, tag, fmt, ...) \
    cls_log_write(logger, CLS_LOG_TRACE, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define CLS_DEBUG(logger, tag, fmt, ...) \
    cls_log_write(logger, CLS_LOG_DEBUG, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define CLS_INFO(logger, tag, fmt, ...) \
    cls_log_write(logger, CLS_LOG_INFO, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define CLS_WARN(logger, tag, fmt, ...) \
    cls_log_write(logger, CLS_LOG_WARN, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define CLS_ERROR(logger, tag, fmt, ...) \
    cls_log_write(logger, CLS_LOG_ERROR, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define CLS_FATAL(logger, tag, fmt, ...) \
    cls_log_write(logger, CLS_LOG_FATAL, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif /* CLS_LOGGING_H */
