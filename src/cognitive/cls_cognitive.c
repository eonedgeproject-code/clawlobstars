/*
 * ClawLobstars - Cognitive System Implementation
 * Rule-based engine with inference framework
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../include/cls_framework.h"

static uint64_t cls_cog_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

cls_status_t cls_cognitive_init(cls_cognitive_t *cog, cls_model_type_t model_type) {
    if (!cog) return CLS_ERR_INVALID;

    memset(cog, 0, sizeof(cls_cognitive_t));
    cog->model_type = model_type;
    cog->model_data = NULL;
    cog->model_size = 0;
    cog->confidence_threshold = 0.5f;
    cog->max_decisions = 16;
    cog->is_trained = false;

    memset(&cog->metrics, 0, sizeof(cls_model_metrics_t));

    return CLS_OK;
}

cls_status_t cls_cognitive_load_model(cls_cognitive_t *cog, const void *data, size_t len) {
    if (!cog || !data || len == 0)
        return CLS_ERR_INVALID;

    void *model = malloc(len);
    if (!model) return CLS_ERR_NOMEM;

    memcpy(model, data, len);

    if (cog->model_data)
        free(cog->model_data);

    cog->model_data = model;
    cog->model_size = len;
    cog->is_trained = true;

    return CLS_OK;
}

cls_status_t cls_cognitive_save_model(cls_cognitive_t *cog, void *buf, size_t *len) {
    if (!cog || !buf || !len)
        return CLS_ERR_INVALID;

    if (!cog->model_data || cog->model_size == 0)
        return CLS_ERR_NOT_FOUND;

    if (*len < cog->model_size) {
        *len = cog->model_size;
        return CLS_ERR_OVERFLOW;
    }

    memcpy(buf, cog->model_data, cog->model_size);
    *len = cog->model_size;

    return CLS_OK;
}

cls_status_t cls_cognitive_infer(cls_cognitive_t *cog, const cls_input_t *input,
                                  cls_decision_t *decision) {
    if (!cog || !input || !decision)
        return CLS_ERR_INVALID;

    uint64_t start = cls_cog_time_us();

    memset(decision, 0, sizeof(cls_decision_t));

    /* Rule-based inference (default model) */
    switch (cog->model_type) {
    case CLS_MODEL_RULE_BASED: {
        /* Simple threshold-based decision */
        float sum = 0.0f;
        for (uint32_t i = 0; i < input->feature_count; i++) {
            sum += input->features[i];
        }

        float avg = (input->feature_count > 0) ?
                     (sum / (float)input->feature_count) : 0.0f;

        decision->confidence = avg;
        decision->action_id = (avg > cog->confidence_threshold) ? 1 : 0;
        decision->priority = (uint32_t)(avg * 100.0f);
        break;
    }

    case CLS_MODEL_NEURAL_NET: {
        /* Simple feedforward neural net: input -> hidden(16) -> output(1) */
        /* Weights stored in model_data as flat floats */
        if (!cog->model_data || !cog->is_trained) {
            decision->confidence = 0.0f;
            decision->action_id = 0;
            break;
        }

        float *weights = (float *)cog->model_data;
        uint32_t in_dim = input->feature_count;
        uint32_t hidden_dim = 16;

        /* Hidden layer: relu(W1 * x + b1) */
        float hidden[16];
        for (uint32_t h = 0; h < hidden_dim; h++) {
            float val = weights[in_dim * hidden_dim + h]; /* bias */
            for (uint32_t i = 0; i < in_dim && i < 32; i++) {
                val += input->features[i] * weights[i * hidden_dim + h];
            }
            hidden[h] = (val > 0.0f) ? val : 0.0f; /* ReLU */
        }

        /* Output layer: sigmoid(W2 * hidden + b2) */
        uint32_t offset = in_dim * hidden_dim + hidden_dim;
        float out_val = weights[offset + hidden_dim]; /* bias */
        for (uint32_t h = 0; h < hidden_dim; h++) {
            out_val += hidden[h] * weights[offset + h];
        }
        /* Sigmoid */
        float sigmoid = 1.0f / (1.0f + expf(-out_val));

        decision->confidence = sigmoid;
        decision->action_id = (sigmoid > cog->confidence_threshold) ? 1 : 0;
        decision->priority = (uint32_t)(sigmoid * 100.0f);
        break;
    }

    case CLS_MODEL_DECISION_TREE: {
        /* Simple decision tree: split on each feature vs threshold */
        float score = 0.0f;
        for (uint32_t i = 0; i < input->feature_count; i++) {
            if (input->features[i] > 0.5f) score += 1.0f;
            else score -= 0.5f;
        }
        float norm = (input->feature_count > 0) ?
                      score / (float)input->feature_count : 0.0f;
        norm = (norm + 1.0f) / 2.0f; /* Normalize to 0-1 */

        decision->confidence = norm;
        decision->action_id = (norm > cog->confidence_threshold) ? 1 : 0;
        decision->priority = (uint32_t)(norm * 100.0f);
        break;
    }

    case CLS_MODEL_BAYESIAN: {
        /* Naive Bayes: assume features are independent probabilities */
        float log_odds = 0.0f;
        for (uint32_t i = 0; i < input->feature_count; i++) {
            float p = input->features[i];
            p = (p < 0.01f) ? 0.01f : (p > 0.99f ? 0.99f : p);
            log_odds += logf(p / (1.0f - p));
        }
        float prob = 1.0f / (1.0f + expf(-log_odds));

        decision->confidence = prob;
        decision->action_id = (prob > cog->confidence_threshold) ? 1 : 0;
        decision->priority = (uint32_t)(prob * 100.0f);
        break;
    }

    case CLS_MODEL_CUSTOM:
        /* Placeholder — would delegate to specific engine */
        decision->confidence = 0.0f;
        decision->action_id = 0;
        break;
    }

    uint64_t end = cls_cog_time_us();
    cog->metrics.inference_time_us = (float)(end - start);
    cog->metrics.total_inferences++;

    return CLS_OK;
}

cls_status_t cls_cognitive_infer_batch(cls_cognitive_t *cog, const cls_input_t *inputs,
                                        uint32_t count, cls_decision_t *decisions) {
    if (!cog || !inputs || !decisions || count == 0)
        return CLS_ERR_INVALID;

    for (uint32_t i = 0; i < count; i++) {
        cls_status_t status = cls_cognitive_infer(cog, &inputs[i], &decisions[i]);
        if (CLS_IS_ERR(status)) return status;
    }

    return CLS_OK;
}

cls_status_t cls_cognitive_train(cls_cognitive_t *cog, const cls_training_data_t *data) {
    if (!cog || !data || !data->samples || data->sample_count == 0)
        return CLS_ERR_INVALID;

    /* Placeholder training loop */
    float total_loss = 0.0f;

    for (uint32_t i = 0; i < data->sample_count; i++) {
        const cls_training_sample_t *sample = &data->samples[i];

        /* Run inference on sample input */
        cls_decision_t pred;
        cls_cognitive_infer(cog, &sample->input, &pred);

        /* Compute simple MSE loss against labels */
        if (sample->label_count > 0 && sample->labels) {
            float diff = pred.confidence - sample->labels[0];
            total_loss += diff * diff;
        }
    }

    cog->metrics.loss = total_loss / (float)data->sample_count;
    cog->metrics.total_training_steps += data->sample_count;
    cog->is_trained = true;

    return CLS_OK;
}

void cls_cognitive_get_metrics(const cls_cognitive_t *cog, cls_model_metrics_t *metrics) {
    if (!cog || !metrics) return;
    *metrics = cog->metrics;
}

cls_status_t cls_cognitive_reset(cls_cognitive_t *cog) {
    if (!cog) return CLS_ERR_INVALID;

    if (cog->model_data) {
        free(cog->model_data);
        cog->model_data = NULL;
    }
    cog->model_size = 0;
    cog->is_trained = false;
    memset(&cog->metrics, 0, sizeof(cls_model_metrics_t));

    return CLS_OK;
}

void cls_cognitive_set_threshold(cls_cognitive_t *cog, float threshold) {
    if (cog) cog->confidence_threshold = threshold;
}

void cls_cognitive_destroy(cls_cognitive_t *cog) {
    if (!cog) return;
    if (cog->model_data) {
        free(cog->model_data);
        cog->model_data = NULL;
    }
    cog->model_size = 0;
}
