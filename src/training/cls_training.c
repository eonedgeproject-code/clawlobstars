/*
 * ClawLobstars - Training Pipeline Implementation
 * Experience replay buffer, epsilon-greedy, model snapshots
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/cls_framework.h"

static uint64_t cls_train_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* Simple PRNG for replay sampling */
static uint32_t cls_train_rand_state = 12345;
static uint32_t cls_train_rand(void) {
    cls_train_rand_state ^= cls_train_rand_state << 13;
    cls_train_rand_state ^= cls_train_rand_state >> 17;
    cls_train_rand_state ^= cls_train_rand_state << 5;
    return cls_train_rand_state;
}

static float cls_train_randf(void) {
    return (float)(cls_train_rand() & 0xFFFF) / 65535.0f;
}

cls_status_t cls_training_init(cls_training_t *train, cls_cognitive_t *cog,
                                cls_train_mode_t mode, uint32_t buffer_capacity) {
    if (!train || !cog) return CLS_ERR_INVALID;

    memset(train, 0, sizeof(cls_training_t));
    train->cognitive = cog;
    train->mode = mode;
    train->learning_rate = 0.001f;
    train->discount_factor = 0.99f;
    train->epsilon = 1.0f;
    train->epsilon_decay = 0.995f;
    train->epsilon_min = 0.01f;

    if (buffer_capacity > 0) {
        train->replay_buffer = (cls_experience_t *)calloc(buffer_capacity, sizeof(cls_experience_t));
        if (!train->replay_buffer) return CLS_ERR_NOMEM;
        train->buffer_capacity = buffer_capacity;
    }

    /* Seed PRNG with time */
    cls_train_rand_state = (uint32_t)(cls_train_time_us() & 0xFFFFFFFF);

    return CLS_OK;
}

void cls_training_set_lr(cls_training_t *train, float lr) {
    if (train) train->learning_rate = lr;
}

void cls_training_set_discount(cls_training_t *train, float gamma) {
    if (train) train->discount_factor = gamma;
}

void cls_training_set_epsilon(cls_training_t *train, float eps, float decay, float min) {
    if (!train) return;
    train->epsilon = eps;
    train->epsilon_decay = decay;
    train->epsilon_min = min;
}

/* ---- Experience Replay ---- */

cls_status_t cls_training_add_experience(cls_training_t *train, const cls_experience_t *exp) {
    if (!train || !exp || !train->replay_buffer) return CLS_ERR_INVALID;

    uint32_t idx = train->buffer_head;
    cls_experience_t *slot = &train->replay_buffer[idx];

    /* Free old data if overwriting */
    free(slot->state);
    free(slot->next_state);

    *slot = *exp;

    /* Deep copy state arrays */
    if (exp->state && exp->state_dim > 0) {
        slot->state = (float *)malloc(exp->state_dim * sizeof(float));
        if (slot->state) memcpy(slot->state, exp->state, exp->state_dim * sizeof(float));
    }
    if (exp->next_state && exp->state_dim > 0) {
        slot->next_state = (float *)malloc(exp->state_dim * sizeof(float));
        if (slot->next_state) memcpy(slot->next_state, exp->next_state, exp->state_dim * sizeof(float));
    }

    slot->timestamp_us = cls_train_time_us();
    train->buffer_head = (train->buffer_head + 1) % train->buffer_capacity;
    if (train->buffer_count < train->buffer_capacity) train->buffer_count++;

    return CLS_OK;
}

cls_status_t cls_training_sample_batch(cls_training_t *train, cls_experience_t *batch,
                                        uint32_t batch_size, uint32_t *out_count) {
    if (!train || !batch || !out_count || !train->replay_buffer)
        return CLS_ERR_INVALID;

    if (train->buffer_count == 0) {
        *out_count = 0;
        return CLS_ERR_NOT_FOUND;
    }

    uint32_t actual = CLS_MIN(batch_size, train->buffer_count);

    for (uint32_t i = 0; i < actual; i++) {
        uint32_t idx = cls_train_rand() % train->buffer_count;
        batch[i] = train->replay_buffer[idx];
        /* Note: shallow copy — caller should not free these */
    }

    *out_count = actual;
    return CLS_OK;
}

/* ---- Training Execution ---- */

cls_status_t cls_training_step(cls_training_t *train) {
    if (!train || !train->training_active) return CLS_ERR_STATE;

    cls_status_t status;

    switch (train->mode) {
    case CLS_TRAIN_REPLAY:
        status = cls_training_train_from_replay(train, 32);
        break;
    case CLS_TRAIN_ONLINE:
    case CLS_TRAIN_OFFLINE:
    default:
        /* For online: process single most recent experience */
        if (train->buffer_count > 0) {
            uint32_t latest = (train->buffer_head == 0) ?
                              train->buffer_capacity - 1 : train->buffer_head - 1;
            cls_experience_t *exp = &train->replay_buffer[latest];

            if (exp->state && exp->state_dim > 0) {
                cls_input_t input = {
                    .features = exp->state,
                    .feature_count = exp->state_dim,
                    .timestamp_us = exp->timestamp_us,
                    .context_id = 0
                };

                cls_decision_t decision;
                status = cls_cognitive_infer(train->cognitive, &input, &decision);

                /* Update reward tracking */
                train->metrics.cumulative_reward += exp->reward;
                train->metrics.total_samples++;
                train->metrics.avg_reward = train->metrics.cumulative_reward /
                                             (float)train->metrics.total_samples;
            } else {
                status = CLS_OK;
            }
        } else {
            status = CLS_OK;
        }
        break;
    }

    /* Decay epsilon */
    if (train->epsilon > train->epsilon_min) {
        train->epsilon *= train->epsilon_decay;
        if (train->epsilon < train->epsilon_min)
            train->epsilon = train->epsilon_min;
    }

    train->metrics.current_epoch++;
    train->metrics.total_updates++;
    train->metrics.learning_rate = train->learning_rate;

    return status;
}

cls_status_t cls_training_train_batch(cls_training_t *train, const cls_training_data_t *data) {
    if (!train || !data || !train->cognitive) return CLS_ERR_INVALID;

    cls_status_t status = cls_cognitive_train(train->cognitive, data);

    if (CLS_IS_OK(status)) {
        cls_model_metrics_t cog_metrics;
        cls_cognitive_get_metrics(train->cognitive, &cog_metrics);

        train->metrics.current_loss = cog_metrics.loss;
        train->metrics.total_samples += data->sample_count;
        train->metrics.total_updates++;

        if (train->metrics.best_loss == 0.0f || cog_metrics.loss < train->metrics.best_loss) {
            train->metrics.best_loss = cog_metrics.loss;
        }
    }

    return status;
}

cls_status_t cls_training_train_from_replay(cls_training_t *train, uint32_t batch_size) {
    if (!train || !train->replay_buffer || !train->cognitive)
        return CLS_ERR_INVALID;

    if (train->buffer_count < batch_size) return CLS_OK; /* Not enough data yet */

    /* Sample batch */
    cls_experience_t *batch = (cls_experience_t *)calloc(batch_size, sizeof(cls_experience_t));
    if (!batch) return CLS_ERR_NOMEM;

    uint32_t actual;
    cls_training_sample_batch(train, batch, batch_size, &actual);

    /* Convert experiences to training samples */
    float total_reward = 0.0f;

    for (uint32_t i = 0; i < actual; i++) {
        if (!batch[i].state || batch[i].state_dim == 0) continue;

        cls_input_t input = {
            .features = batch[i].state,
            .feature_count = batch[i].state_dim,
            .timestamp_us = batch[i].timestamp_us,
            .context_id = 0
        };

        /* Predict current value */
        cls_decision_t current_pred;
        cls_cognitive_infer(train->cognitive, &input, &current_pred);

        /* Compute TD target */
        float target = batch[i].reward;

        /* Fix: clip reward to prevent NaN/inf propagation */
        if (target != target) target = 0.0f;  /* NaN check */
        if (target > 100.0f) target = 100.0f;
        if (target < -100.0f) target = -100.0f;

        if (!batch[i].terminal && batch[i].next_state) {
            cls_input_t next_input = {
                .features = batch[i].next_state,
                .feature_count = batch[i].state_dim,
                .timestamp_us = batch[i].timestamp_us,
                .context_id = 0
            };
            cls_decision_t next_pred;
            cls_cognitive_infer(train->cognitive, &next_input, &next_pred);
            target += train->discount_factor * next_pred.confidence;
        }

        total_reward += batch[i].reward;
    }

    /* Update metrics */
    train->metrics.total_samples += actual;
    train->metrics.total_updates++;
    if (actual > 0) {
        train->metrics.avg_reward = total_reward / (float)actual;
    }

    free(batch);
    return CLS_OK;
}

/* ---- Exploration ---- */

uint32_t cls_training_select_action(cls_training_t *train, const cls_decision_t *decisions,
                                     uint32_t decision_count) {
    if (!train || !decisions || decision_count == 0) return 0;

    /* Epsilon-greedy */
    if (cls_train_randf() < train->epsilon) {
        /* Explore: random action */
        return cls_train_rand() % decision_count;
    }

    /* Exploit: best action by confidence */
    uint32_t best = 0;
    float best_conf = decisions[0].confidence;
    for (uint32_t i = 1; i < decision_count; i++) {
        if (decisions[i].confidence > best_conf) {
            best_conf = decisions[i].confidence;
            best = i;
        }
    }
    return best;
}

/* ---- Snapshots ---- */

cls_status_t cls_training_save_snapshot(cls_training_t *train) {
    if (!train || !train->cognitive) return CLS_ERR_INVALID;

    uint32_t idx = train->snapshot_count % CLS_TRAIN_MAX_SNAPSHOTS;
    cls_snapshot_t *snap = &train->snapshots[idx];

    /* Free old snapshot data */
    free(snap->model_data);
    memset(snap, 0, sizeof(cls_snapshot_t));

    snap->snapshot_id = train->next_snapshot_id++;
    snap->created_at = cls_train_time_us();
    snap->epoch = train->metrics.current_epoch;
    snap->loss = train->metrics.current_loss;
    snap->accuracy = train->metrics.current_accuracy;

    /* Save model if available */
    if (train->cognitive->model_data && train->cognitive->model_size > 0) {
        snap->model_data = malloc(train->cognitive->model_size);
        if (snap->model_data) {
            memcpy(snap->model_data, train->cognitive->model_data, train->cognitive->model_size);
            snap->model_size = train->cognitive->model_size;
        }
    }

    if (train->snapshot_count < CLS_TRAIN_MAX_SNAPSHOTS)
        train->snapshot_count++;

    return CLS_OK;
}

cls_status_t cls_training_load_snapshot(cls_training_t *train, uint32_t snapshot_id) {
    if (!train || !train->cognitive) return CLS_ERR_INVALID;

    for (uint32_t i = 0; i < train->snapshot_count && i < CLS_TRAIN_MAX_SNAPSHOTS; i++) {
        if (train->snapshots[i].snapshot_id == snapshot_id) {
            cls_snapshot_t *snap = &train->snapshots[i];
            if (snap->model_data && snap->model_size > 0) {
                return cls_cognitive_load_model(train->cognitive, snap->model_data, snap->model_size);
            }
            return CLS_ERR_NOT_FOUND;
        }
    }
    return CLS_ERR_NOT_FOUND;
}

cls_status_t cls_training_rollback_best(cls_training_t *train) {
    if (!train) return CLS_ERR_INVALID;

    /* Find snapshot with lowest loss */
    cls_snapshot_t *best = NULL;
    for (uint32_t i = 0; i < train->snapshot_count && i < CLS_TRAIN_MAX_SNAPSHOTS; i++) {
        if (!best || train->snapshots[i].loss < best->loss) {
            best = &train->snapshots[i];
        }
    }

    if (!best) return CLS_ERR_NOT_FOUND;
    return cls_training_load_snapshot(train, best->snapshot_id);
}

void cls_training_get_metrics(const cls_training_t *train, cls_train_metrics_t *metrics) {
    if (!train || !metrics) return;
    *metrics = train->metrics;
}

void cls_training_reset_metrics(cls_training_t *train) {
    if (!train) return;
    memset(&train->metrics, 0, sizeof(cls_train_metrics_t));
    train->metrics.learning_rate = train->learning_rate;
}

cls_status_t cls_training_start(cls_training_t *train) {
    if (!train) return CLS_ERR_INVALID;
    train->training_active = true;
    return CLS_OK;
}

cls_status_t cls_training_stop(cls_training_t *train) {
    if (!train) return CLS_ERR_INVALID;
    train->training_active = false;
    return CLS_OK;
}

void cls_training_destroy(cls_training_t *train) {
    if (!train) return;

    if (train->replay_buffer) {
        for (uint32_t i = 0; i < train->buffer_capacity; i++) {
            free(train->replay_buffer[i].state);
            free(train->replay_buffer[i].next_state);
        }
        free(train->replay_buffer);
    }

    for (uint32_t i = 0; i < CLS_TRAIN_MAX_SNAPSHOTS; i++) {
        free(train->snapshots[i].model_data);
    }

    memset(train, 0, sizeof(cls_training_t));
}
