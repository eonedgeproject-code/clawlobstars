/*
 * ClawLobstars - Planning & Strategy Implementation
 * DAG-based task scheduling with dependency resolution
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/cls_framework.h"

static uint64_t cls_plan_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

cls_status_t cls_planner_init(cls_planner_t *planner, uint32_t max_plans, uint32_t max_goals) {
    if (!planner || max_plans == 0 || max_goals == 0)
        return CLS_ERR_INVALID;

    memset(planner, 0, sizeof(cls_planner_t));

    planner->plans = (cls_plan_t *)calloc(max_plans, sizeof(cls_plan_t));
    if (!planner->plans) return CLS_ERR_NOMEM;

    planner->goals = (cls_goal_t *)calloc(max_goals, sizeof(cls_goal_t));
    if (!planner->goals) {
        free(planner->plans);
        return CLS_ERR_NOMEM;
    }

    planner->max_plans = max_plans;
    planner->max_goals = max_goals;
    return CLS_OK;
}

/* ---- Goal Management ---- */

cls_status_t cls_planner_add_goal(cls_planner_t *planner, const cls_goal_t *goal) {
    if (!planner || !goal) return CLS_ERR_INVALID;
    if (planner->goal_count >= planner->max_goals) return CLS_ERR_OVERFLOW;

    planner->goals[planner->goal_count] = *goal;
    planner->goal_count++;
    return CLS_OK;
}

cls_status_t cls_planner_remove_goal(cls_planner_t *planner, uint32_t goal_id) {
    if (!planner) return CLS_ERR_INVALID;
    for (uint32_t i = 0; i < planner->goal_count; i++) {
        if (planner->goals[i].goal_id == goal_id) {
            memmove(&planner->goals[i], &planner->goals[i + 1],
                    (planner->goal_count - i - 1) * sizeof(cls_goal_t));
            planner->goal_count--;
            return CLS_OK;
        }
    }
    return CLS_ERR_NOT_FOUND;
}

cls_status_t cls_planner_update_goal(cls_planner_t *planner, uint32_t goal_id, float progress) {
    cls_goal_t *g = cls_planner_get_goal(planner, goal_id);
    if (!g) return CLS_ERR_NOT_FOUND;
    g->progress = progress;
    if (progress >= 1.0f) g->achieved = true;
    return CLS_OK;
}

cls_goal_t *cls_planner_get_goal(cls_planner_t *planner, uint32_t goal_id) {
    if (!planner) return NULL;
    for (uint32_t i = 0; i < planner->goal_count; i++) {
        if (planner->goals[i].goal_id == goal_id)
            return &planner->goals[i];
    }
    return NULL;
}

/* ---- Plan Generation ---- */

cls_status_t cls_planner_generate(cls_planner_t *planner, const cls_decision_t *decisions,
                                   uint32_t decision_count, cls_plan_t **out_plan) {
    if (!planner || !decisions || decision_count == 0 || !out_plan)
        return CLS_ERR_INVALID;
    if (planner->plan_count >= planner->max_plans)
        return CLS_ERR_OVERFLOW;

    cls_plan_t *plan = &planner->plans[planner->plan_count];
    memset(plan, 0, sizeof(cls_plan_t));

    plan->plan_id = (uint32_t)planner->plans_generated + 1;
    plan->status = CLS_PLAN_PENDING;
    plan->max_tasks = decision_count * 2;  /* Room for subtasks */
    plan->tasks = (cls_task_t *)calloc(plan->max_tasks, sizeof(cls_task_t));
    if (!plan->tasks) return CLS_ERR_NOMEM;

    plan->created_at = cls_plan_time_us();

    /* Convert decisions to tasks, sorted by priority (descending) */
    for (uint32_t i = 0; i < decision_count; i++) {
        if (decisions[i].confidence < 0.1f) continue;  /* Skip low-confidence */

        cls_task_t *task = &plan->tasks[plan->task_count];
        task->task_id = plan->task_count + 1;
        task->action_id = decisions[i].action_id;
        task->priority = (cls_priority_t)CLS_CLAMP(decisions[i].priority / 25, 0, 3);
        task->status = CLS_PLAN_PENDING;
        task->cost_estimate = 1.0f - decisions[i].confidence;
        task->reward_estimate = decisions[i].confidence;
        task->dep_count = 0;
        task->params = decisions[i].params;
        task->params_len = decisions[i].params_len;
        task->deadline_us = 0;

        plan->total_cost += task->cost_estimate;
        plan->total_reward += task->reward_estimate;
        plan->task_count++;
    }

    /* Calculate success probability */
    if (plan->task_count > 0) {
        plan->success_probability = plan->total_reward / (float)plan->task_count;
    }

    plan->status = CLS_PLAN_ACTIVE;
    planner->plan_count++;
    planner->plans_generated++;
    *out_plan = plan;

    return CLS_OK;
}

cls_status_t cls_plan_add_task(cls_plan_t *plan, const cls_task_t *task) {
    if (!plan || !task) return CLS_ERR_INVALID;
    if (plan->task_count >= plan->max_tasks) return CLS_ERR_OVERFLOW;

    plan->tasks[plan->task_count] = *task;
    plan->task_count++;
    plan->total_cost += task->cost_estimate;
    plan->total_reward += task->reward_estimate;
    return CLS_OK;
}

/* Get next task with all dependencies met */
cls_status_t cls_plan_next_task(cls_plan_t *plan, cls_task_t **out_task) {
    if (!plan || !out_task) return CLS_ERR_INVALID;

    cls_task_t *best = NULL;

    for (uint32_t i = 0; i < plan->task_count; i++) {
        cls_task_t *t = &plan->tasks[i];
        if (t->status != CLS_PLAN_PENDING) continue;

        /* Check all dependencies are complete */
        bool deps_met = true;
        for (uint32_t d = 0; d < t->dep_count; d++) {
            bool found_complete = false;
            for (uint32_t j = 0; j < plan->task_count; j++) {
                if (plan->tasks[j].task_id == t->depends_on[d] &&
                    plan->tasks[j].status == CLS_PLAN_COMPLETE) {
                    found_complete = true;
                    break;
                }
            }
            if (!found_complete) { deps_met = false; break; }
        }

        if (!deps_met) continue;

        /* Pick highest priority */
        if (!best || t->priority > best->priority) {
            best = t;
        }
    }

    if (!best) return CLS_ERR_NOT_FOUND;
    *out_task = best;
    return CLS_OK;
}

cls_status_t cls_plan_complete_task(cls_plan_t *plan, uint32_t task_id, bool success) {
    if (!plan) return CLS_ERR_INVALID;

    for (uint32_t i = 0; i < plan->task_count; i++) {
        if (plan->tasks[i].task_id == task_id) {
            plan->tasks[i].status = success ? CLS_PLAN_COMPLETE : CLS_PLAN_FAILED;
            plan->tasks[i].completed_at = cls_plan_time_us();

            /* Check if all tasks complete */
            bool all_done = true;
            bool any_failed = false;
            for (uint32_t j = 0; j < plan->task_count; j++) {
                if (plan->tasks[j].status == CLS_PLAN_PENDING ||
                    plan->tasks[j].status == CLS_PLAN_ACTIVE) {
                    all_done = false;
                }
                if (plan->tasks[j].status == CLS_PLAN_FAILED) {
                    any_failed = true;
                }
            }

            if (all_done) {
                plan->status = any_failed ? CLS_PLAN_FAILED : CLS_PLAN_COMPLETE;
            }
            return CLS_OK;
        }
    }
    return CLS_ERR_NOT_FOUND;
}

cls_status_t cls_planner_evaluate(cls_planner_t *planner, const cls_plan_t *plan,
                                   cls_strategy_eval_t *eval) {
    if (!planner || !plan || !eval) return CLS_ERR_INVALID;
    CLS_UNUSED(planner);

    memset(eval, 0, sizeof(cls_strategy_eval_t));

    eval->expected_utility = plan->total_reward - plan->total_cost;
    eval->resource_cost = plan->total_cost;
    eval->risk_score = 1.0f - plan->success_probability;
    eval->feasible = (plan->task_count > 0 && plan->success_probability > 0.3f);

    /* Estimate time based on task count */
    eval->time_estimate_us = (float)plan->task_count * 10000.0f;

    return CLS_OK;
}

cls_status_t cls_planner_replan(cls_planner_t *planner, cls_plan_t *failed_plan,
                                 cls_plan_t **out_plan) {
    if (!planner || !failed_plan || !out_plan)
        return CLS_ERR_INVALID;

    /* Collect incomplete tasks from failed plan and regenerate */
    uint32_t remaining = 0;
    cls_decision_t fallback_decisions[64];

    for (uint32_t i = 0; i < failed_plan->task_count && remaining < 64; i++) {
        if (failed_plan->tasks[i].status == CLS_PLAN_PENDING ||
            failed_plan->tasks[i].status == CLS_PLAN_FAILED) {
            fallback_decisions[remaining].action_id = failed_plan->tasks[i].action_id;
            fallback_decisions[remaining].confidence = failed_plan->tasks[i].reward_estimate * 0.8f;
            fallback_decisions[remaining].priority = failed_plan->tasks[i].priority;
            fallback_decisions[remaining].params = failed_plan->tasks[i].params;
            fallback_decisions[remaining].params_len = failed_plan->tasks[i].params_len;
            remaining++;
        }
    }

    if (remaining == 0) return CLS_ERR_NOT_FOUND;

    failed_plan->status = CLS_PLAN_CANCELLED;
    return cls_planner_generate(planner, fallback_decisions, remaining, out_plan);
}

void cls_plan_destroy(cls_plan_t *plan) {
    if (!plan) return;
    free(plan->tasks);
    plan->tasks = NULL;
    plan->task_count = 0;
}

void cls_planner_destroy(cls_planner_t *planner) {
    if (!planner) return;
    for (uint32_t i = 0; i < planner->plan_count; i++) {
        cls_plan_destroy(&planner->plans[i]);
    }
    free(planner->plans);
    free(planner->goals);
    planner->plans = NULL;
    planner->goals = NULL;
}
