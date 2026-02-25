/*
 * ClawLobstars - Agent Core Implementation
 * Main agent lifecycle management
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "../include/cls_framework.h"

/* ============================================================
 * Internal Helpers
 * ============================================================ */

static uint64_t cls_agent_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static void cls_default_log(cls_log_level_t level, const char *module, const char *msg) {
    static const char *level_str[] = {
        "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
    };
    if (level > CLS_LOG_FATAL) return;
    fprintf(stderr, "[CLS][%s][%s] %s\n", level_str[level], module, msg);
}

static void cls_agent_log(cls_agent_t *agent, cls_log_level_t level,
                           const char *module, const char *msg) {
    if (level < agent->config.log_level) return;
    if (agent->log_fn)
        agent->log_fn(level, module, msg);
    else
        cls_default_log(level, module, msg);
}

/* ============================================================
 * API Implementation
 * ============================================================ */

cls_status_t cls_agent_init(cls_agent_t *agent, const cls_config_t *cfg) {
    if (!agent || !cfg)
        return CLS_ERR_INVALID;

    memset(agent, 0, sizeof(cls_agent_t));
    agent->config = *cfg;
    agent->id = cfg->agent_id;
    agent->name = cfg->agent_name;
    agent->state = CLS_STATE_INIT;
    agent->log_fn = cls_default_log;

    cls_agent_log(agent, CLS_LOG_INFO, "CORE", "Initializing ClawLobstars agent...");

    /* Initialize memory subsystem */
    agent->memory = (cls_memory_ctx_t *)calloc(1, sizeof(cls_memory_ctx_t));
    if (!agent->memory) {
        cls_agent_log(agent, CLS_LOG_FATAL, "CORE", "Failed to allocate memory context");
        return CLS_ERR_NOMEM;
    }

    cls_status_t status = cls_memory_init(agent->memory, cfg->memory_size);
    if (CLS_IS_ERR(status)) {
        cls_agent_log(agent, CLS_LOG_FATAL, "MEMORY", "Memory init failed");
        free(agent->memory);
        agent->memory = NULL;
        return status;
    }

    /* Initialize perception engine */
    agent->perception = (cls_perception_t *)calloc(1, sizeof(cls_perception_t));
    if (!agent->perception) {
        cls_agent_log(agent, CLS_LOG_FATAL, "CORE", "Failed to allocate perception engine");
        cls_memory_destroy(agent->memory);
        free(agent->memory);
        return CLS_ERR_NOMEM;
    }

    status = cls_perception_init(agent->perception, cfg->max_sensors);
    if (CLS_IS_ERR(status)) {
        cls_agent_log(agent, CLS_LOG_FATAL, "PERCEPTION", "Perception init failed");
        cls_memory_destroy(agent->memory);
        free(agent->memory);
        free(agent->perception);
        return status;
    }

    /* Initialize cognitive system */
    agent->cognitive = (cls_cognitive_t *)calloc(1, sizeof(cls_cognitive_t));
    if (!agent->cognitive) {
        cls_agent_log(agent, CLS_LOG_FATAL, "CORE", "Failed to allocate cognitive system");
        cls_perception_destroy(agent->perception);
        free(agent->perception);
        cls_memory_destroy(agent->memory);
        free(agent->memory);
        return CLS_ERR_NOMEM;
    }

    status = cls_cognitive_init(agent->cognitive, CLS_MODEL_RULE_BASED);
    if (CLS_IS_ERR(status)) {
        cls_agent_log(agent, CLS_LOG_FATAL, "COGNITIVE", "Cognitive init failed");
        cls_perception_destroy(agent->perception);
        free(agent->perception);
        cls_memory_destroy(agent->memory);
        free(agent->memory);
        free(agent->cognitive);
        return status;
    }

    agent->cycle_count = 0;
    agent->uptime_us = 0;
    agent->last_step_us = cls_agent_time_us();
    agent->initialized = true;
    agent->shutting_down = false;
    agent->state = CLS_STATE_READY;

    cls_agent_log(agent, CLS_LOG_INFO, "CORE",
                  "Agent initialized successfully. Status: READY");

    return CLS_OK;
}

cls_status_t cls_agent_step(cls_agent_t *agent) {
    if (!agent || !agent->initialized)
        return CLS_ERR_STATE;

    if (agent->shutting_down)
        return CLS_ERR_STATE;

    uint64_t step_start = cls_agent_time_us();
    agent->state = CLS_STATE_ACTIVE;

    /* Phase 1: Perception — poll all sensors */
    cls_status_t status = cls_perception_poll(agent->perception);
    if (CLS_IS_ERR(status) && status != CLS_ERR_NOT_FOUND) {
        cls_agent_log(agent, CLS_LOG_WARN, "PERCEPTION", "Sensor poll returned error");
    }

    /* Phase 2: Memory — prune expired entries */
    uint32_t pruned = cls_memory_prune(agent->memory);
    if (pruned > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Pruned %u expired entries", pruned);
        cls_agent_log(agent, CLS_LOG_DEBUG, "MEMORY", buf);
    }

    /* Phase 3: Cognitive — inference cycle */
    agent->state = CLS_STATE_PLANNING;
    /* NOTE: In a full implementation, perception output would feed into
     * cognitive inference here. For now, the step increments cycle count
     * and maintains the agent loop timing. */

    /* Phase 4: Return to ready */
    agent->state = CLS_STATE_READY;
    agent->cycle_count++;

    uint64_t step_end = cls_agent_time_us();
    agent->last_step_us = step_end - step_start;
    agent->uptime_us += agent->last_step_us;

    return CLS_OK;
}

cls_agent_state_t cls_agent_get_state(const cls_agent_t *agent) {
    if (!agent) return CLS_STATE_ERROR;
    return agent->state;
}

void cls_agent_set_logger(cls_agent_t *agent, cls_log_fn fn) {
    if (agent) agent->log_fn = fn;
}

void cls_agent_set_event_handler(cls_agent_t *agent, cls_event_fn fn) {
    if (agent) agent->event_fn = fn;
}

cls_status_t cls_agent_feed(cls_agent_t *agent, const cls_frame_t *frame) {
    if (!agent || !frame || !agent->initialized)
        return CLS_ERR_INVALID;

    cls_percept_t percept = {0};
    cls_status_t status = cls_perception_process(agent->perception, frame, &percept);
    if (CLS_IS_ERR(status)) return status;

    /* Store perception result in memory */
    char key[128];
    snprintf(key, sizeof(key), "percept:%u:%llu",
             frame->sensor_id, (unsigned long long)frame->timestamp_us);

    cls_memory_store_ttl(agent->memory, key, &percept, sizeof(percept), 300);

    /* Notify event handler if anomaly detected */
    if (percept.classification >= CLS_EVENT_ANOMALY && agent->event_fn) {
        agent->event_fn(percept.classification, &percept, sizeof(percept));
    }

    return CLS_OK;
}

cls_status_t cls_agent_get_decision(const cls_agent_t *agent, cls_decision_t *decision) {
    if (!agent || !decision) return CLS_ERR_INVALID;
    /* Placeholder — returns last cognitive decision */
    CLS_UNUSED(agent);
    memset(decision, 0, sizeof(cls_decision_t));
    return CLS_OK;
}

void cls_agent_stats(const cls_agent_t *agent, uint64_t *cycles, uint64_t *uptime) {
    if (!agent) return;
    if (cycles)  *cycles = agent->cycle_count;
    if (uptime)  *uptime = agent->uptime_us;
}

cls_status_t cls_agent_shutdown(cls_agent_t *agent) {
    if (!agent || !agent->initialized)
        return CLS_ERR_STATE;

    agent->shutting_down = true;
    cls_agent_log(agent, CLS_LOG_INFO, "CORE", "Shutting down agent...");

    /* Flush memory */
    cls_memory_prune(agent->memory);

    agent->state = CLS_STATE_INIT;
    cls_agent_log(agent, CLS_LOG_INFO, "CORE", "Agent shutdown complete");

    return CLS_OK;
}

void cls_agent_destroy(cls_agent_t *agent) {
    if (!agent) return;

    if (agent->cognitive) {
        cls_cognitive_destroy(agent->cognitive);
        free(agent->cognitive);
        agent->cognitive = NULL;
    }

    if (agent->perception) {
        cls_perception_destroy(agent->perception);
        free(agent->perception);
        agent->perception = NULL;
    }

    if (agent->memory) {
        cls_memory_destroy(agent->memory);
        free(agent->memory);
        agent->memory = NULL;
    }

    agent->initialized = false;
}

const char *cls_version(void) {
    return CLS_VERSION_STRING;
}
