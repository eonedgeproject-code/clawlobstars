/*
 * ClawLobstars AI Agent Framework
 * ================================
 * Lightweight AI agent framework in C for edge computing
 * and embedded systems.
 *
 * License: GPL-3.0
 * Version: 0.4.0
 */

#ifndef CLS_FRAMEWORK_H
#define CLS_FRAMEWORK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Version Info
 * ============================================================ */
#define CLS_VERSION_MAJOR 0
#define CLS_VERSION_MINOR 4
#define CLS_VERSION_PATCH 0
#define CLS_VERSION_STRING "0.4.0"

/* ============================================================
 * Status Codes
 * ============================================================ */
typedef enum {
    CLS_OK              = 0,
    CLS_ERR_NOMEM       = -1,
    CLS_ERR_INVALID     = -2,
    CLS_ERR_TIMEOUT     = -3,
    CLS_ERR_OVERFLOW    = -4,
    CLS_ERR_NOT_FOUND   = -5,
    CLS_ERR_BUSY        = -6,
    CLS_ERR_SECURITY    = -7,
    CLS_ERR_IO          = -8,
    CLS_ERR_STATE       = -9,
    CLS_ERR_INTERNAL    = -99
} cls_status_t;

/* ============================================================
 * Agent States
 * ============================================================ */
typedef enum {
    CLS_STATE_INIT      = 0x00,
    CLS_STATE_READY     = 0x01,
    CLS_STATE_ACTIVE    = 0x02,
    CLS_STATE_PLANNING  = 0x03,
    CLS_STATE_EXECUTING = 0x04,
    CLS_STATE_TRAINING  = 0x05,
    CLS_STATE_RECOVERY  = 0x0E,
    CLS_STATE_ERROR     = 0xFF
} cls_agent_state_t;

/* ============================================================
 * Security Levels
 * ============================================================ */
typedef enum {
    CLS_SEC_NONE    = 0,
    CLS_SEC_LOW     = 1,
    CLS_SEC_MEDIUM  = 2,
    CLS_SEC_HIGH    = 3,
    CLS_SEC_MAX     = 4
} cls_security_level_t;

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
    CLS_LOG_NONE    = 255
} cls_log_level_t;

/* ============================================================
 * Configuration
 * ============================================================ */
typedef struct {
    uint32_t                agent_id;
    size_t                  memory_size;
    uint32_t                max_sensors;
    uint32_t                inference_hz;
    cls_security_level_t    security_level;
    cls_log_level_t         log_level;
    const char             *agent_name;
} cls_config_t;

/* Default configuration macro */
#define CLS_CONFIG_DEFAULT { \
    .agent_id       = 0,    \
    .memory_size    = 262144, /* 256KB */ \
    .max_sensors    = 8,    \
    .inference_hz   = 100,  \
    .security_level = CLS_SEC_MEDIUM, \
    .log_level      = CLS_LOG_WARN,   \
    .agent_name     = "cls-agent-0"   \
}

/* ============================================================
 * Forward Declarations
 * ============================================================ */
typedef struct cls_agent         cls_agent_t;
typedef struct cls_memory_ctx    cls_memory_ctx_t;
typedef struct cls_perception    cls_perception_t;
typedef struct cls_cognitive     cls_cognitive_t;
typedef struct cls_planner       cls_planner_t;
typedef struct cls_action_exec   cls_action_exec_t;
typedef struct cls_comm_bus      cls_comm_bus_t;
typedef struct cls_multiagent    cls_multiagent_t;
typedef struct cls_security_ctx  cls_security_ctx_t;
typedef struct cls_resource_mgr  cls_resource_mgr_t;
typedef struct cls_telemetry     cls_telemetry_t;
typedef struct cls_scheduler     cls_scheduler_t;
typedef struct cls_network       cls_network_t;

/* ============================================================
 * Callback Types
 * ============================================================ */
typedef void (*cls_log_fn)(cls_log_level_t level, const char *module, const char *msg);
typedef void (*cls_event_fn)(uint32_t event_id, const void *data, size_t len);
typedef cls_status_t (*cls_sensor_read_fn)(void *sensor_ctx, void *buf, size_t *len);
typedef cls_status_t (*cls_action_fn)(uint32_t action_id, const void *params, size_t len);

/* ============================================================
 * Data Frame (Perception I/O)
 * ============================================================ */
typedef struct {
    uint32_t    sensor_id;
    uint64_t    timestamp_us;
    uint16_t    data_type;
    uint16_t    flags;
    size_t      payload_len;
    const void *payload;
} cls_frame_t;

/* ============================================================
 * Decision Output (Cognitive)
 * ============================================================ */
typedef struct {
    uint32_t    action_id;
    float       confidence;
    uint32_t    priority;
    size_t      params_len;
    void       *params;
} cls_decision_t;

/* ============================================================
 * Message (Communication)
 * ============================================================ */
typedef struct {
    uint32_t    src_agent;
    uint32_t    dst_agent;
    uint16_t    msg_type;
    uint16_t    flags;
    uint64_t    timestamp_us;
    size_t      payload_len;
    const void *payload;
} cls_msg_t;

/* ============================================================
 * Utility Macros
 * ============================================================ */
#define CLS_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define CLS_MIN(a, b)       ((a) < (b) ? (a) : (b))
#define CLS_MAX(a, b)       ((a) > (b) ? (a) : (b))
#define CLS_CLAMP(x, lo, hi) CLS_MIN(CLS_MAX(x, lo), hi)

#define CLS_UNUSED(x)       ((void)(x))

/* Status check macros */
#define CLS_IS_OK(s)        ((s) == CLS_OK)
#define CLS_IS_ERR(s)       ((s) != CLS_OK)
#define CLS_CHECK(expr)     do { cls_status_t _s = (expr); if (CLS_IS_ERR(_s)) return _s; } while(0)

/* ============================================================
 * Module Includes
 * ============================================================ */
#include "cls_memory.h"
#include "cls_perception.h"
#include "cls_cognitive.h"
#include "cls_planning.h"
#include "cls_action.h"
#include "cls_knowledge.h"
#include "cls_comm.h"
#include "cls_multiagent.h"
#include "cls_security.h"
#include "cls_training.h"
#include "cls_resource.h"
#include "cls_telemetry.h"
#include "cls_scheduler.h"
#include "cls_network.h"
#include "cls_logging.h"
#include "cls_config.h"
#include "cls_diagnostics.h"
#include "cls_plugin.h"
#include "cls_agent.h"

#ifdef __cplusplus
}
#endif

#endif /* CLS_FRAMEWORK_H */
