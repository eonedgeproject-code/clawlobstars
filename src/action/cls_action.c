/*
 * ClawLobstars - Action Executor Implementation
 * Execute, validate, monitor, rollback actions
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/cls_framework.h"

static uint64_t cls_action_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

cls_status_t cls_action_init(cls_action_exec_t *exec, uint32_t max_handlers, uint32_t max_history) {
    if (!exec || max_handlers == 0) return CLS_ERR_INVALID;

    memset(exec, 0, sizeof(cls_action_exec_t));

    exec->handlers = (cls_action_handler_t *)calloc(max_handlers, sizeof(cls_action_handler_t));
    if (!exec->handlers) return CLS_ERR_NOMEM;

    exec->history = (cls_action_record_t *)calloc(max_history, sizeof(cls_action_record_t));
    if (!exec->history) {
        free(exec->handlers);
        return CLS_ERR_NOMEM;
    }

    exec->max_handlers = max_handlers;
    exec->max_history = max_history;
    exec->next_exec_id = 1;
    return CLS_OK;
}

cls_status_t cls_action_register(cls_action_exec_t *exec, const cls_action_handler_t *handler) {
    if (!exec || !handler || !handler->execute_fn) return CLS_ERR_INVALID;
    if (exec->handler_count >= exec->max_handlers) return CLS_ERR_OVERFLOW;

    /* Check duplicate */
    for (uint32_t i = 0; i < exec->handler_count; i++) {
        if (exec->handlers[i].action_id == handler->action_id)
            return CLS_ERR_INVALID;
    }

    exec->handlers[exec->handler_count] = *handler;
    exec->handler_count++;
    return CLS_OK;
}

cls_status_t cls_action_unregister(cls_action_exec_t *exec, uint32_t action_id) {
    if (!exec) return CLS_ERR_INVALID;
    for (uint32_t i = 0; i < exec->handler_count; i++) {
        if (exec->handlers[i].action_id == action_id) {
            memmove(&exec->handlers[i], &exec->handlers[i + 1],
                    (exec->handler_count - i - 1) * sizeof(cls_action_handler_t));
            exec->handler_count--;
            return CLS_OK;
        }
    }
    return CLS_ERR_NOT_FOUND;
}

static cls_action_handler_t *find_handler(cls_action_exec_t *exec, uint32_t action_id) {
    for (uint32_t i = 0; i < exec->handler_count; i++) {
        if (exec->handlers[i].action_id == action_id)
            return &exec->handlers[i];
    }
    return NULL;
}

static void record_action(cls_action_exec_t *exec, const cls_action_record_t *rec) {
    uint32_t idx = exec->history_count % exec->max_history;
    exec->history[idx] = *rec;
    if (exec->history_count < exec->max_history) exec->history_count++;
}

cls_status_t cls_action_execute(cls_action_exec_t *exec, uint32_t action_id,
                                 const void *params, size_t params_len,
                                 cls_action_record_t *record) {
    if (!exec || !record) return CLS_ERR_INVALID;

    cls_action_handler_t *handler = find_handler(exec, action_id);
    if (!handler) return CLS_ERR_NOT_FOUND;

    cls_action_record_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.exec_id = exec->next_exec_id++;
    rec.action_id = action_id;
    rec.started_at = cls_action_time_us();
    rec.status = CLS_ACTION_RUNNING;

    /* Execute the action handler */
    cls_status_t result = handler->execute_fn(action_id, params, params_len);

    rec.completed_at = cls_action_time_us();
    rec.duration_us = rec.completed_at - rec.started_at;
    rec.result_code = (int32_t)result;

    if (CLS_IS_OK(result)) {
        rec.status = CLS_ACTION_SUCCESS;
        exec->total_success++;
    } else {
        rec.status = CLS_ACTION_FAILED;
        exec->total_failed++;
    }

    exec->total_executed++;
    record_action(exec, &rec);
    *record = rec;

    return result;
}

cls_status_t cls_action_execute_task(cls_action_exec_t *exec, cls_task_t *task,
                                      cls_action_record_t *record) {
    if (!exec || !task || !record) return CLS_ERR_INVALID;

    task->started_at = cls_action_time_us();
    task->status = CLS_PLAN_ACTIVE;

    cls_status_t status = cls_action_execute(exec, task->action_id,
                                              task->params, task->params_len, record);

    task->completed_at = cls_action_time_us();
    task->status = CLS_IS_OK(status) ? CLS_PLAN_COMPLETE : CLS_PLAN_FAILED;

    return status;
}

cls_status_t cls_action_rollback(cls_action_exec_t *exec, uint32_t exec_id) {
    if (!exec) return CLS_ERR_INVALID;

    /* Find the record */
    cls_action_record_t *rec = NULL;
    for (uint32_t i = 0; i < exec->history_count && i < exec->max_history; i++) {
        if (exec->history[i].exec_id == exec_id) {
            rec = &exec->history[i];
            break;
        }
    }
    if (!rec) return CLS_ERR_NOT_FOUND;
    if (rec->rolled_back) return CLS_ERR_STATE;

    /* Find handler with rollback function */
    cls_action_handler_t *handler = find_handler(exec, rec->action_id);
    if (!handler || !handler->rollback_fn) return CLS_ERR_INVALID;

    cls_status_t result = handler->rollback_fn(rec->action_id, NULL, 0);

    if (CLS_IS_OK(result)) {
        rec->rolled_back = true;
        rec->status = CLS_ACTION_ROLLEDBACK;
        exec->total_rollbacks++;
    }

    return result;
}

cls_status_t cls_action_get_record(const cls_action_exec_t *exec, uint32_t exec_id,
                                    cls_action_record_t *record) {
    if (!exec || !record) return CLS_ERR_INVALID;
    for (uint32_t i = 0; i < exec->history_count && i < exec->max_history; i++) {
        if (exec->history[i].exec_id == exec_id) {
            *record = exec->history[i];
            return CLS_OK;
        }
    }
    return CLS_ERR_NOT_FOUND;
}

uint32_t cls_action_history_count(const cls_action_exec_t *exec) {
    return exec ? exec->history_count : 0;
}

void cls_action_stats(const cls_action_exec_t *exec, uint64_t *executed,
                       uint64_t *success, uint64_t *failed, uint64_t *rollbacks) {
    if (!exec) return;
    if (executed)  *executed  = exec->total_executed;
    if (success)   *success   = exec->total_success;
    if (failed)    *failed    = exec->total_failed;
    if (rollbacks) *rollbacks = exec->total_rollbacks;
}

void cls_action_destroy(cls_action_exec_t *exec) {
    if (!exec) return;
    free(exec->handlers);
    free(exec->history);
    exec->handlers = NULL;
    exec->history = NULL;
}
