/*
 * ClawLobstars - Training Pipeline
 * Coordinates training, behavior modification, performance tracking
 */

#ifndef CLS_TRAINING_H
#define CLS_TRAINING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLS_TRAIN_MAX_BUFFER    1024
#define CLS_TRAIN_MAX_SNAPSHOTS 8

/* Training mode */
typedef enum {
    CLS_TRAIN_OFFLINE    = 0,  /* Batch training on stored data */
    CLS_TRAIN_ONLINE     = 1,  /* Incremental learning per sample */
    CLS_TRAIN_REPLAY     = 2   /* Experience replay from buffer */
} cls_train_mode_t;

/* Experience replay buffer entry */
typedef struct {
    float      *state;
    uint32_t    state_dim;
    uint32_t    action_taken;
    float       reward;
    float      *next_state;
    bool        terminal;
    uint64_t    timestamp_us;
} cls_experience_t;

/* Training snapshot (model checkpoint) */
typedef struct {
    uint32_t    snapshot_id;
    void       *model_data;
    size_t      model_size;
    float       loss;
    float       accuracy;
    uint64_t    created_at;
    uint32_t    epoch;
} cls_snapshot_t;

/* Training metrics */
typedef struct {
    float       current_loss;
    float       best_loss;
    float       current_accuracy;
    float       best_accuracy;
    float       learning_rate;
    uint32_t    current_epoch;
    uint64_t    total_samples;
    uint64_t    total_updates;
    float       avg_reward;
    float       cumulative_reward;
} cls_train_metrics_t;

/* Training pipeline context */
typedef struct cls_training {
    cls_train_mode_t     mode;
    cls_experience_t    *replay_buffer;
    uint32_t             buffer_count;
    uint32_t             buffer_capacity;
    uint32_t             buffer_head;
    cls_snapshot_t       snapshots[CLS_TRAIN_MAX_SNAPSHOTS];
    uint32_t             snapshot_count;
    uint32_t             next_snapshot_id;
    cls_train_metrics_t  metrics;
    cls_cognitive_t     *cognitive;      /* Reference to cognitive module */
    float                learning_rate;
    float                discount_factor;
    float                epsilon;        /* Exploration rate */
    float                epsilon_decay;
    float                epsilon_min;
    bool                 training_active;
} cls_training_t;

/* ---- API ---- */

cls_status_t cls_training_init(cls_training_t *train, cls_cognitive_t *cog,
                                cls_train_mode_t mode, uint32_t buffer_capacity);

/* Configuration */
void cls_training_set_lr(cls_training_t *train, float lr);
void cls_training_set_discount(cls_training_t *train, float gamma);
void cls_training_set_epsilon(cls_training_t *train, float eps, float decay, float min);

/* Experience replay */
cls_status_t cls_training_add_experience(cls_training_t *train, const cls_experience_t *exp);
cls_status_t cls_training_sample_batch(cls_training_t *train, cls_experience_t *batch,
                                        uint32_t batch_size, uint32_t *out_count);

/* Training execution */
cls_status_t cls_training_step(cls_training_t *train);
cls_status_t cls_training_train_batch(cls_training_t *train, const cls_training_data_t *data);
cls_status_t cls_training_train_from_replay(cls_training_t *train, uint32_t batch_size);

/* Exploration vs exploitation */
uint32_t cls_training_select_action(cls_training_t *train, const cls_decision_t *decisions,
                                     uint32_t decision_count);

/* Snapshots */
cls_status_t cls_training_save_snapshot(cls_training_t *train);
cls_status_t cls_training_load_snapshot(cls_training_t *train, uint32_t snapshot_id);
cls_status_t cls_training_rollback_best(cls_training_t *train);

/* Metrics */
void cls_training_get_metrics(const cls_training_t *train, cls_train_metrics_t *metrics);
void cls_training_reset_metrics(cls_training_t *train);

/* Start/stop */
cls_status_t cls_training_start(cls_training_t *train);
cls_status_t cls_training_stop(cls_training_t *train);

void cls_training_destroy(cls_training_t *train);

#ifdef __cplusplus
}
#endif

#endif /* CLS_TRAINING_H */
