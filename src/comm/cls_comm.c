/*
 * ClawLobstars - Communication Bus Implementation
 * Pub/sub message routing with ring buffer queue
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/cls_framework.h"

static uint64_t cls_comm_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

cls_status_t cls_comm_init(cls_comm_bus_t *bus, uint32_t local_agent_id) {
    if (!bus) return CLS_ERR_INVALID;

    memset(bus, 0, sizeof(cls_comm_bus_t));
    bus->local_agent_id = local_agent_id;
    bus->next_sub_id = 1;
    return CLS_OK;
}

cls_status_t cls_comm_subscribe(cls_comm_bus_t *bus, uint16_t msg_type_filter,
                                 cls_msg_handler_fn handler, void *user_ctx,
                                 uint32_t *out_sub_id) {
    if (!bus || !handler) return CLS_ERR_INVALID;
    if (bus->subscriber_count >= CLS_COMM_MAX_SUBSCRIBERS) return CLS_ERR_OVERFLOW;

    cls_subscription_t *sub = &bus->subscribers[bus->subscriber_count];
    sub->msg_type_filter = msg_type_filter;
    sub->handler = handler;
    sub->user_ctx = user_ctx;
    sub->active = true;
    sub->sub_id = bus->next_sub_id++;

    if (out_sub_id) *out_sub_id = sub->sub_id;
    bus->subscriber_count++;
    return CLS_OK;
}

cls_status_t cls_comm_unsubscribe(cls_comm_bus_t *bus, uint32_t sub_id) {
    if (!bus) return CLS_ERR_INVALID;

    for (uint32_t i = 0; i < bus->subscriber_count; i++) {
        if (bus->subscribers[i].sub_id == sub_id) {
            bus->subscribers[i].active = false;
            return CLS_OK;
        }
    }
    return CLS_ERR_NOT_FOUND;
}

static cls_status_t enqueue_msg(cls_comm_bus_t *bus, const cls_msg_t *msg,
                                 cls_deliver_mode_t mode) {
    if (bus->queue_count >= CLS_COMM_QUEUE_SIZE) {
        bus->msgs_dropped++;
        return CLS_ERR_OVERFLOW;
    }

    cls_msg_queue_entry_t *entry = &bus->queue[bus->queue_tail];
    entry->msg = *msg;
    entry->mode = mode;
    entry->retry_count = 0;
    entry->occupied = true;

    /* Copy payload into internal buffer */
    if (msg->payload && msg->payload_len > 0) {
        size_t copy_len = CLS_MIN(msg->payload_len, sizeof(entry->payload_buf));
        memcpy(entry->payload_buf, msg->payload, copy_len);
        entry->msg.payload = entry->payload_buf;
        entry->msg.payload_len = copy_len;
    }

    bus->queue_tail = (bus->queue_tail + 1) % CLS_COMM_QUEUE_SIZE;
    bus->queue_count++;
    bus->msgs_sent++;
    return CLS_OK;
}

cls_status_t cls_comm_publish(cls_comm_bus_t *bus, const cls_msg_t *msg,
                               cls_deliver_mode_t mode) {
    if (!bus || !msg) return CLS_ERR_INVALID;
    return enqueue_msg(bus, msg, mode);
}

cls_status_t cls_comm_send(cls_comm_bus_t *bus, uint16_t msg_type,
                            const void *payload, size_t payload_len,
                            uint32_t dst_agent) {
    if (!bus) return CLS_ERR_INVALID;

    cls_msg_t msg = {
        .src_agent = bus->local_agent_id,
        .dst_agent = dst_agent,
        .msg_type = msg_type,
        .flags = 0,
        .timestamp_us = cls_comm_time_us(),
        .payload_len = payload_len,
        .payload = payload
    };

    return enqueue_msg(bus, &msg, CLS_DELIVER_RELIABLE);
}

cls_status_t cls_comm_broadcast(cls_comm_bus_t *bus, uint16_t msg_type,
                                 const void *payload, size_t payload_len) {
    if (!bus) return CLS_ERR_INVALID;

    cls_msg_t msg = {
        .src_agent = bus->local_agent_id,
        .dst_agent = 0,  /* 0 = broadcast */
        .msg_type = msg_type,
        .flags = 0,
        .timestamp_us = cls_comm_time_us(),
        .payload_len = payload_len,
        .payload = payload
    };

    return enqueue_msg(bus, &msg, CLS_DELIVER_BROADCAST);
}

uint32_t cls_comm_process(cls_comm_bus_t *bus, uint32_t max_process) {
    if (!bus) return 0;

    uint32_t processed = 0;

    while (bus->queue_count > 0 && processed < max_process) {
        cls_msg_queue_entry_t *entry = &bus->queue[bus->queue_head];
        if (!entry->occupied) break;

        /* Deliver to matching subscribers */
        for (uint32_t i = 0; i < bus->subscriber_count; i++) {
            cls_subscription_t *sub = &bus->subscribers[i];
            if (!sub->active) continue;

            /* Check type filter (0 = receive all) */
            if (sub->msg_type_filter != 0 &&
                sub->msg_type_filter != entry->msg.msg_type)
                continue;

            /* For non-broadcast, check destination */
            if (entry->mode != CLS_DELIVER_BROADCAST &&
                entry->msg.dst_agent != 0 &&
                entry->msg.dst_agent != bus->local_agent_id)
                continue;

            sub->handler(&entry->msg, sub->user_ctx);
            bus->msgs_delivered++;
        }

        entry->occupied = false;
        bus->queue_head = (bus->queue_head + 1) % CLS_COMM_QUEUE_SIZE;
        bus->queue_count--;
        processed++;
    }

    return processed;
}

cls_status_t cls_comm_flush(cls_comm_bus_t *bus) {
    if (!bus) return CLS_ERR_INVALID;

    while (bus->queue_count > 0) {
        cls_comm_process(bus, CLS_COMM_QUEUE_SIZE);
    }
    return CLS_OK;
}

void cls_comm_stats(const cls_comm_bus_t *bus, uint64_t *sent,
                     uint64_t *delivered, uint64_t *dropped) {
    if (!bus) return;
    if (sent)      *sent = bus->msgs_sent;
    if (delivered)  *delivered = bus->msgs_delivered;
    if (dropped)   *dropped = bus->msgs_dropped;
}

void cls_comm_destroy(cls_comm_bus_t *bus) {
    if (!bus) return;
    memset(bus, 0, sizeof(cls_comm_bus_t));
}
