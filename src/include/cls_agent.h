/*
 * ClawLobstars - Agent Core
 * Manages agent lifecycle, configuration, and health
 */

#ifndef CLS_AGENT_H
#define CLS_AGENT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Agent structure */
struct cls_agent {
    uint32_t            id;
    const char         *name;
    cls_agent_state_t   state;
    cls_config_t        config;

    /* Subsystem pointers */
    cls_memory_ctx_t   *memory;
    cls_perception_t   *perception;
    cls_cognitive_t    *cognitive;

    /* Runtime stats */
    uint64_t            cycle_count;
    uint64_t            uptime_us;
    uint64_t            last_step_us;

    /* Callbacks */
    cls_log_fn          log_fn;
    cls_event_fn        event_fn;

    /* Internal state */
    bool                initialized;
    bool                shutting_down;
};

/* ---- API ---- */

/* Initialize agent with configuration */
cls_status_t cls_agent_init(cls_agent_t *agent, const cls_config_t *cfg);

/* Execute one full processing cycle:
 * perception -> cognitive -> planning -> action
 */
cls_status_t cls_agent_step(cls_agent_t *agent);

/* Get current agent state */
cls_agent_state_t cls_agent_get_state(const cls_agent_t *agent);

/* Set log callback */
void cls_agent_set_logger(cls_agent_t *agent, cls_log_fn fn);

/* Set event callback */
void cls_agent_set_event_handler(cls_agent_t *agent, cls_event_fn fn);

/* Feed input data to agent */
cls_status_t cls_agent_feed(cls_agent_t *agent, const cls_frame_t *frame);

/* Get last decision */
cls_status_t cls_agent_get_decision(const cls_agent_t *agent, cls_decision_t *decision);

/* Get runtime statistics */
void cls_agent_stats(const cls_agent_t *agent, uint64_t *cycles, uint64_t *uptime_us);

/* Graceful shutdown */
cls_status_t cls_agent_shutdown(cls_agent_t *agent);

/* Free all resources */
void cls_agent_destroy(cls_agent_t *agent);

/* Get version string */
const char *cls_version(void);

#ifdef __cplusplus
}
#endif

#endif /* CLS_AGENT_H */
