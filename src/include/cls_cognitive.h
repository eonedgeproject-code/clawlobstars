/*
 * ClawLobstars - Cognitive System
 * Inference, decision-making, and adaptive learning
 */

#ifndef CLS_COGNITIVE_H
#define CLS_COGNITIVE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Model types */
typedef enum {
    CLS_MODEL_DECISION_TREE = 0,
    CLS_MODEL_NEURAL_NET    = 1,
    CLS_MODEL_BAYESIAN      = 2,
    CLS_MODEL_RULE_BASED    = 3,
    CLS_MODEL_CUSTOM        = 255
} cls_model_type_t;

/* Inference input */
typedef struct {
    float      *features;
    uint32_t    feature_count;
    uint64_t    timestamp_us;
    uint32_t    context_id;
} cls_input_t;

/* Training data sample */
typedef struct {
    cls_input_t     input;
    float          *labels;
    uint32_t        label_count;
    float           weight;
} cls_training_sample_t;

/* Training data batch */
typedef struct {
    cls_training_sample_t  *samples;
    uint32_t                sample_count;
    uint32_t                epoch;
    float                   learning_rate;
} cls_training_data_t;

/* Model performance metrics */
typedef struct {
    float       accuracy;
    float       loss;
    float       inference_time_us;
    uint64_t    total_inferences;
    uint64_t    total_training_steps;
} cls_model_metrics_t;

/* Cognitive system context */
struct cls_cognitive {
    cls_model_type_t    model_type;
    void               *model_data;
    size_t              model_size;
    cls_model_metrics_t metrics;
    float               confidence_threshold;
    uint32_t            max_decisions;
    bool                is_trained;
};

/* ---- API ---- */

/* Initialize cognitive system */
cls_status_t cls_cognitive_init(cls_cognitive_t *cog, cls_model_type_t model_type);

/* Load model weights from buffer */
cls_status_t cls_cognitive_load_model(cls_cognitive_t *cog, const void *data, size_t len);

/* Save model weights to buffer */
cls_status_t cls_cognitive_save_model(cls_cognitive_t *cog, void *buf, size_t *len);

/* Run inference cycle */
cls_status_t cls_cognitive_infer(cls_cognitive_t *cog, const cls_input_t *input,
                                  cls_decision_t *decision);

/* Run batch inference */
cls_status_t cls_cognitive_infer_batch(cls_cognitive_t *cog, const cls_input_t *inputs,
                                        uint32_t count, cls_decision_t *decisions);

/* Train model with data batch */
cls_status_t cls_cognitive_train(cls_cognitive_t *cog, const cls_training_data_t *data);

/* Get model metrics */
void cls_cognitive_get_metrics(const cls_cognitive_t *cog, cls_model_metrics_t *metrics);

/* Reset model to initial state */
cls_status_t cls_cognitive_reset(cls_cognitive_t *cog);

/* Set confidence threshold */
void cls_cognitive_set_threshold(cls_cognitive_t *cog, float threshold);

/* Destroy cognitive system */
void cls_cognitive_destroy(cls_cognitive_t *cog);

#ifdef __cplusplus
}
#endif

#endif /* CLS_COGNITIVE_H */
