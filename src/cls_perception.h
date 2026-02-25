/*
 * ClawLobstars - Perception Engine
 * Collects and processes raw sensor input
 */

#ifndef CLS_PERCEPTION_H
#define CLS_PERCEPTION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sensor types */
typedef enum {
    CLS_SENSOR_GENERIC   = 0,
    CLS_SENSOR_NUMERIC   = 1,
    CLS_SENSOR_VECTOR    = 2,
    CLS_SENSOR_IMAGE     = 3,
    CLS_SENSOR_AUDIO     = 4,
    CLS_SENSOR_NETWORK   = 5,
    CLS_SENSOR_CUSTOM    = 255
} cls_sensor_type_t;

/* Sensor descriptor */
typedef struct {
    uint32_t            id;
    cls_sensor_type_t   type;
    const char         *name;
    cls_sensor_read_fn  read_fn;
    void               *user_ctx;
    uint32_t            poll_hz;
    bool                active;
} cls_sensor_t;

/* Event classification */
typedef enum {
    CLS_EVENT_NONE      = 0,
    CLS_EVENT_NORMAL    = 1,
    CLS_EVENT_ANOMALY   = 2,
    CLS_EVENT_ALERT     = 3,
    CLS_EVENT_CRITICAL  = 4
} cls_event_class_t;

/* Processed perception output */
typedef struct {
    uint32_t            sensor_id;
    uint64_t            timestamp_us;
    cls_event_class_t   classification;
    float               confidence;
    float              *features;
    uint32_t            feature_count;
} cls_percept_t;

/* Perception engine context */
struct cls_perception {
    cls_sensor_t   *sensors;
    uint32_t        sensor_count;
    uint32_t        max_sensors;
    uint64_t        frames_processed;
    cls_event_fn    event_callback;
    void           *normalizer_ctx;
};

/* ---- API ---- */

/* Initialize perception engine */
cls_status_t cls_perception_init(cls_perception_t *p, uint32_t max_sensors);

/* Register a sensor source */
cls_status_t cls_perception_register(cls_perception_t *p, const cls_sensor_t *sensor);

/* Unregister a sensor */
cls_status_t cls_perception_unregister(cls_perception_t *p, uint32_t sensor_id);

/* Process incoming data frame */
cls_status_t cls_perception_process(cls_perception_t *p, const cls_frame_t *frame,
                                     cls_percept_t *output);

/* Poll all active sensors */
cls_status_t cls_perception_poll(cls_perception_t *p);

/* Set event callback */
void cls_perception_on_event(cls_perception_t *p, cls_event_fn callback);

/* Get sensor by ID */
cls_sensor_t *cls_perception_get_sensor(cls_perception_t *p, uint32_t sensor_id);

/* Destroy perception engine */
void cls_perception_destroy(cls_perception_t *p);

#ifdef __cplusplus
}
#endif

#endif /* CLS_PERCEPTION_H */
