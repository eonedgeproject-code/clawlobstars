/*
 * ClawLobstars - Communication Bus
 * Internal/external message routing, protocol handling, event sync
 */

#ifndef CLS_COMM_H
#define CLS_COMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLS_COMM_MAX_SUBSCRIBERS 32
#define CLS_COMM_QUEUE_SIZE      256

/* Message types */
typedef enum {
    CLS_MSG_PERCEPTION   = 0x01,
    CLS_MSG_COGNITIVE    = 0x02,
    CLS_MSG_PLANNING     = 0x03,
    CLS_MSG_ACTION       = 0x04,
    CLS_MSG_MEMORY       = 0x05,
    CLS_MSG_KNOWLEDGE    = 0x06,
    CLS_MSG_TRAINING     = 0x07,
    CLS_MSG_MULTIAGENT   = 0x08,
    CLS_MSG_SECURITY     = 0x09,
    CLS_MSG_RESOURCE     = 0x0A,
    CLS_MSG_SYSTEM       = 0x0B,
    CLS_MSG_HEARTBEAT    = 0xFE,
    CLS_MSG_CUSTOM       = 0xFF
} cls_msg_type_t;

/* Message delivery mode */
typedef enum {
    CLS_DELIVER_FIRE_FORGET = 0,
    CLS_DELIVER_RELIABLE    = 1,
    CLS_DELIVER_BROADCAST   = 2
} cls_deliver_mode_t;

/* Subscriber callback */
typedef void (*cls_msg_handler_fn)(const cls_msg_t *msg, void *user_ctx);

/* Subscription entry */
typedef struct {
    uint16_t            msg_type_filter;  /* 0 = all messages */
    cls_msg_handler_fn  handler;
    void               *user_ctx;
    bool                active;
    uint32_t            sub_id;
} cls_subscription_t;

/* Message queue entry */
typedef struct {
    cls_msg_t           msg;
    uint8_t             payload_buf[512];
    cls_deliver_mode_t  mode;
    uint8_t             retry_count;
    bool                occupied;
} cls_msg_queue_entry_t;

/* Communication bus context */
struct cls_comm_bus {
    cls_subscription_t     subscribers[CLS_COMM_MAX_SUBSCRIBERS];
    uint32_t               subscriber_count;
    uint32_t               next_sub_id;
    cls_msg_queue_entry_t  queue[CLS_COMM_QUEUE_SIZE];
    uint32_t               queue_head;
    uint32_t               queue_tail;
    uint32_t               queue_count;
    uint64_t               msgs_sent;
    uint64_t               msgs_delivered;
    uint64_t               msgs_dropped;
    uint32_t               local_agent_id;
};

/* ---- API ---- */

cls_status_t cls_comm_init(cls_comm_bus_t *bus, uint32_t local_agent_id);

/* Subscribe to messages */
cls_status_t cls_comm_subscribe(cls_comm_bus_t *bus, uint16_t msg_type_filter,
                                 cls_msg_handler_fn handler, void *user_ctx,
                                 uint32_t *out_sub_id);
cls_status_t cls_comm_unsubscribe(cls_comm_bus_t *bus, uint32_t sub_id);

/* Publish message */
cls_status_t cls_comm_publish(cls_comm_bus_t *bus, const cls_msg_t *msg,
                               cls_deliver_mode_t mode);

/* Send directly to specific subscriber type */
cls_status_t cls_comm_send(cls_comm_bus_t *bus, uint16_t msg_type,
                            const void *payload, size_t payload_len,
                            uint32_t dst_agent);

/* Broadcast to all subscribers */
cls_status_t cls_comm_broadcast(cls_comm_bus_t *bus, uint16_t msg_type,
                                 const void *payload, size_t payload_len);

/* Process queued messages (call in main loop) */
uint32_t cls_comm_process(cls_comm_bus_t *bus, uint32_t max_process);

/* Flush all queued messages */
cls_status_t cls_comm_flush(cls_comm_bus_t *bus);

/* Stats */
void cls_comm_stats(const cls_comm_bus_t *bus, uint64_t *sent,
                     uint64_t *delivered, uint64_t *dropped);

void cls_comm_destroy(cls_comm_bus_t *bus);

#ifdef __cplusplus
}
#endif

#endif /* CLS_COMM_H */
