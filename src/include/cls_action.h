/*
 * ClawLobstars - Action Executor
 * Executes planned operations, validates, monitors, and supports rollback
 */

#ifndef CLS_ACTION_H
#define CLS_ACTION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Action execution status */
typedef enum {
    CLS_ACTION_IDLE       = 0,
    CLS_ACTION_RUNNING    = 1,
    CLS_ACTION_SUCCESS    = 2,
    CLS_ACTION_FAILED     = 3,
    CLS_ACTION_ROLLEDBACK = 4,
    CLS_ACTION_TIMEOUT    = 5
} cls_action_status_t;

/* Action handler registration */
typedef struct {
    uint32_t        action_id;
    const char     *name;
    cls_action_fn   execute_fn;
    cls_action_fn   rollback_fn;     /* NULL if not rollbackable */
    uint32_t        timeout_ms;
    cls_priority_t  min_priority;
} cls_action_handler_t;

/* Action execution record */
typedef struct {
    uint32_t            exec_id;
    uint32_t            action_id;
    cls_action_status_t status;
    uint64_t            started_at;
    uint64_t            completed_at;
    uint64_t            duration_us;
    int32_t             result_code;
    bool                rolled_back;
} cls_action_record_t;

/* Action executor context */
struct cls_action_exec {
    cls_action_handler_t  *handlers;
    uint32_t               handler_count;
    uint32_t               max_handlers;
    cls_action_record_t   *history;
    uint32_t               history_count;
    uint32_t               max_history;
    uint32_t               next_exec_id;
    uint64_t               total_executed;
    uint64_t               total_success;
    uint64_t               total_failed;
    uint64_t               total_rollbacks;
};

/* ---- API ---- */

cls_status_t cls_action_init(cls_action_exec_t *exec, uint32_t max_handlers, uint32_t max_history);

/* Register action handler */
cls_status_t cls_action_register(cls_action_exec_t *exec, const cls_action_handler_t *handler);
cls_status_t cls_action_unregister(cls_action_exec_t *exec, uint32_t action_id);

/* Execute an action */
cls_status_t cls_action_execute(cls_action_exec_t *exec, uint32_t action_id,
                                 const void *params, size_t params_len,
                                 cls_action_record_t *record);

/* Execute a task from planner */
cls_status_t cls_action_execute_task(cls_action_exec_t *exec, cls_task_t *task,
                                      cls_action_record_t *record);

/* Rollback last action */
cls_status_t cls_action_rollback(cls_action_exec_t *exec, uint32_t exec_id);

/* Query execution history */
cls_status_t cls_action_get_record(const cls_action_exec_t *exec, uint32_t exec_id,
                                    cls_action_record_t *record);
uint32_t cls_action_history_count(const cls_action_exec_t *exec);

/* Get stats */
void cls_action_stats(const cls_action_exec_t *exec, uint64_t *executed,
                       uint64_t *success, uint64_t *failed, uint64_t *rollbacks);

void cls_action_destroy(cls_action_exec_t *exec);

#ifdef __cplusplus
}
#endif

#endif /* CLS_ACTION_H */
