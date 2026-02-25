/*
 * ClawLobstars - Planning & Strategy Module
 * Task scheduling, plan generation, goal management
 */

#ifndef CLS_PLANNING_H
#define CLS_PLANNING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Plan status */
typedef enum {
    CLS_PLAN_PENDING    = 0,
    CLS_PLAN_ACTIVE     = 1,
    CLS_PLAN_COMPLETE   = 2,
    CLS_PLAN_FAILED     = 3,
    CLS_PLAN_CANCELLED  = 4
} cls_plan_status_t;

/* Goal priority */
typedef enum {
    CLS_PRIORITY_LOW      = 0,
    CLS_PRIORITY_NORMAL   = 1,
    CLS_PRIORITY_HIGH     = 2,
    CLS_PRIORITY_CRITICAL = 3
} cls_priority_t;

/* Single task within a plan */
typedef struct {
    uint32_t            task_id;
    uint32_t            action_id;
    cls_priority_t      priority;
    cls_plan_status_t   status;
    float               cost_estimate;
    float               reward_estimate;
    uint32_t            depends_on[8];
    uint32_t            dep_count;
    void               *params;
    size_t              params_len;
    uint64_t            deadline_us;
    uint64_t            started_at;
    uint64_t            completed_at;
} cls_task_t;

/* Goal */
typedef struct {
    uint32_t            goal_id;
    const char         *description;
    cls_priority_t      priority;
    float               progress;       /* 0.0 - 1.0 */
    float               utility;        /* Expected value */
    bool                achieved;
} cls_goal_t;

/* Execution plan */
typedef struct {
    uint32_t            plan_id;
    cls_plan_status_t   status;
    cls_task_t         *tasks;
    uint32_t            task_count;
    uint32_t            max_tasks;
    float               total_cost;
    float               total_reward;
    float               success_probability;
    uint64_t            created_at;
} cls_plan_t;

/* Strategy evaluation result */
typedef struct {
    float               expected_utility;
    float               risk_score;
    float               resource_cost;
    float               time_estimate_us;
    bool                feasible;
} cls_strategy_eval_t;

/* Planner context */
struct cls_planner {
    cls_plan_t         *plans;
    uint32_t            plan_count;
    uint32_t            max_plans;
    cls_goal_t         *goals;
    uint32_t            goal_count;
    uint32_t            max_goals;
    uint64_t            plans_generated;
    uint64_t            plans_completed;
    uint64_t            plans_failed;
};

/* ---- API ---- */

cls_status_t cls_planner_init(cls_planner_t *planner, uint32_t max_plans, uint32_t max_goals);

/* Goal management */
cls_status_t cls_planner_add_goal(cls_planner_t *planner, const cls_goal_t *goal);
cls_status_t cls_planner_remove_goal(cls_planner_t *planner, uint32_t goal_id);
cls_status_t cls_planner_update_goal(cls_planner_t *planner, uint32_t goal_id, float progress);
cls_goal_t  *cls_planner_get_goal(cls_planner_t *planner, uint32_t goal_id);

/* Plan generation from decisions */
cls_status_t cls_planner_generate(cls_planner_t *planner, const cls_decision_t *decisions,
                                   uint32_t decision_count, cls_plan_t **out_plan);

/* Add task to plan */
cls_status_t cls_plan_add_task(cls_plan_t *plan, const cls_task_t *task);

/* Get next executable task (respecting dependencies) */
cls_status_t cls_plan_next_task(cls_plan_t *plan, cls_task_t **out_task);

/* Mark task complete/failed */
cls_status_t cls_plan_complete_task(cls_plan_t *plan, uint32_t task_id, bool success);

/* Evaluate strategy feasibility */
cls_status_t cls_planner_evaluate(cls_planner_t *planner, const cls_plan_t *plan,
                                   cls_strategy_eval_t *eval);

/* Replan: generate fallback plan on failure */
cls_status_t cls_planner_replan(cls_planner_t *planner, cls_plan_t *failed_plan,
                                 cls_plan_t **out_plan);

/* Cleanup */
void cls_plan_destroy(cls_plan_t *plan);
void cls_planner_destroy(cls_planner_t *planner);

#ifdef __cplusplus
}
#endif

#endif /* CLS_PLANNING_H */
