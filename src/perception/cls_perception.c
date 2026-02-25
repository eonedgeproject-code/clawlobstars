/*
 * ClawLobstars - Perception Engine Implementation
 */

#include <stdlib.h>
#include <string.h>
#include "../include/cls_framework.h"

cls_status_t cls_perception_init(cls_perception_t *p, uint32_t max_sensors) {
    if (!p || max_sensors == 0)
        return CLS_ERR_INVALID;

    p->sensors = (cls_sensor_t *)calloc(max_sensors, sizeof(cls_sensor_t));
    if (!p->sensors) return CLS_ERR_NOMEM;

    p->sensor_count = 0;
    p->max_sensors = max_sensors;
    p->frames_processed = 0;
    p->event_callback = NULL;
    p->normalizer_ctx = NULL;

    return CLS_OK;
}

cls_status_t cls_perception_register(cls_perception_t *p, const cls_sensor_t *sensor) {
    if (!p || !sensor)
        return CLS_ERR_INVALID;

    if (p->sensor_count >= p->max_sensors)
        return CLS_ERR_OVERFLOW;

    /* Check for duplicate ID */
    for (uint32_t i = 0; i < p->sensor_count; i++) {
        if (p->sensors[i].id == sensor->id)
            return CLS_ERR_INVALID;
    }

    p->sensors[p->sensor_count] = *sensor;
    p->sensors[p->sensor_count].active = true;
    p->sensor_count++;

    return CLS_OK;
}

cls_status_t cls_perception_unregister(cls_perception_t *p, uint32_t sensor_id) {
    if (!p) return CLS_ERR_INVALID;

    for (uint32_t i = 0; i < p->sensor_count; i++) {
        if (p->sensors[i].id == sensor_id) {
            /* Shift remaining sensors */
            memmove(&p->sensors[i], &p->sensors[i + 1],
                    (p->sensor_count - i - 1) * sizeof(cls_sensor_t));
            p->sensor_count--;
            return CLS_OK;
        }
    }

    return CLS_ERR_NOT_FOUND;
}

cls_status_t cls_perception_process(cls_perception_t *p, const cls_frame_t *frame,
                                     cls_percept_t *output) {
    if (!p || !frame || !output)
        return CLS_ERR_INVALID;

    /* Basic normalization — populate output */
    memset(output, 0, sizeof(cls_percept_t));
    output->sensor_id = frame->sensor_id;
    output->timestamp_us = frame->timestamp_us;
    output->classification = CLS_EVENT_NORMAL;
    output->confidence = 1.0f;
    output->features = NULL;
    output->feature_count = 0;

    /* Simple anomaly detection: check payload bounds */
    if (frame->payload_len == 0 || !frame->payload) {
        output->classification = CLS_EVENT_ANOMALY;
        output->confidence = 0.9f;
    }

    p->frames_processed++;

    /* Fire event callback if anomaly */
    if (output->classification >= CLS_EVENT_ANOMALY && p->event_callback) {
        p->event_callback(output->classification, output, sizeof(cls_percept_t));
    }

    return CLS_OK;
}

cls_status_t cls_perception_poll(cls_perception_t *p) {
    if (!p) return CLS_ERR_INVALID;

    bool any_read = false;

    for (uint32_t i = 0; i < p->sensor_count; i++) {
        if (!p->sensors[i].active || !p->sensors[i].read_fn)
            continue;

        uint8_t buf[4096];
        size_t len = sizeof(buf);

        cls_status_t status = p->sensors[i].read_fn(
            p->sensors[i].user_ctx, buf, &len);

        if (CLS_IS_OK(status) && len > 0) {
            cls_frame_t frame = {
                .sensor_id = p->sensors[i].id,
                .timestamp_us = 0, /* Would use real clock */
                .data_type = (uint16_t)p->sensors[i].type,
                .flags = 0,
                .payload_len = len,
                .payload = buf
            };

            cls_percept_t percept;
            cls_perception_process(p, &frame, &percept);
            any_read = true;
        }
    }

    return any_read ? CLS_OK : CLS_ERR_NOT_FOUND;
}

void cls_perception_on_event(cls_perception_t *p, cls_event_fn callback) {
    if (p) p->event_callback = callback;
}

cls_sensor_t *cls_perception_get_sensor(cls_perception_t *p, uint32_t sensor_id) {
    if (!p) return NULL;

    for (uint32_t i = 0; i < p->sensor_count; i++) {
        if (p->sensors[i].id == sensor_id)
            return &p->sensors[i];
    }
    return NULL;
}

void cls_perception_destroy(cls_perception_t *p) {
    if (!p) return;
    free(p->sensors);
    p->sensors = NULL;
    p->sensor_count = 0;
}
