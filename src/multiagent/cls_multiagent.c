/*
 * ClawLobstars - Multi-Agent Operations Implementation
 * Discovery, collaboration, voting-based conflict resolution
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/cls_framework.h"

static uint64_t cls_ma_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

cls_status_t cls_multiagent_init(cls_multiagent_t *ma, uint32_t local_id, cls_comm_bus_t *bus) {
    if (!ma) return CLS_ERR_INVALID;

    memset(ma, 0, sizeof(cls_multiagent_t));
    ma->local_id = local_id;
    ma->comm_bus = bus;
    ma->next_proposal_id = 1;
    return CLS_OK;
}

cls_status_t cls_multiagent_discover(cls_multiagent_t *ma, cls_agent_list_t *peers) {
    if (!ma || !peers) return CLS_ERR_INVALID;

    peers->count = 0;
    for (uint32_t i = 0; i < ma->peer_count && peers->count < CLS_MA_MAX_PEERS; i++) {
        if (ma->peers[i].status >= CLS_PEER_DISCOVERED) {
            peers->peers[peers->count] = ma->peers[i];
            peers->count++;
        }
    }

    /* Broadcast discovery request via comm bus */
    if (ma->comm_bus) {
        cls_comm_broadcast(ma->comm_bus, CLS_MSG_MULTIAGENT,
                           &ma->local_id, sizeof(uint32_t));
    }

    return CLS_OK;
}

cls_status_t cls_multiagent_register_peer(cls_multiagent_t *ma, const cls_peer_t *peer) {
    if (!ma || !peer) return CLS_ERR_INVALID;
    if (ma->peer_count >= CLS_MA_MAX_PEERS) return CLS_ERR_OVERFLOW;

    /* Check duplicate */
    for (uint32_t i = 0; i < ma->peer_count; i++) {
        if (ma->peers[i].agent_id == peer->agent_id) {
            ma->peers[i] = *peer;  /* Update existing */
            return CLS_OK;
        }
    }

    ma->peers[ma->peer_count] = *peer;
    if (ma->peers[ma->peer_count].status == CLS_PEER_UNKNOWN)
        ma->peers[ma->peer_count].status = CLS_PEER_DISCOVERED;
    ma->peers[ma->peer_count].last_heartbeat = cls_ma_time_us();
    ma->peer_count++;
    return CLS_OK;
}

cls_status_t cls_multiagent_remove_peer(cls_multiagent_t *ma, uint32_t agent_id) {
    if (!ma) return CLS_ERR_INVALID;
    for (uint32_t i = 0; i < ma->peer_count; i++) {
        if (ma->peers[i].agent_id == agent_id) {
            memmove(&ma->peers[i], &ma->peers[i + 1],
                    (ma->peer_count - i - 1) * sizeof(cls_peer_t));
            ma->peer_count--;
            return CLS_OK;
        }
    }
    return CLS_ERR_NOT_FOUND;
}

cls_peer_t *cls_multiagent_get_peer(cls_multiagent_t *ma, uint32_t agent_id) {
    if (!ma) return NULL;
    for (uint32_t i = 0; i < ma->peer_count; i++) {
        if (ma->peers[i].agent_id == agent_id)
            return &ma->peers[i];
    }
    return NULL;
}

cls_status_t cls_multiagent_send_heartbeat(cls_multiagent_t *ma) {
    if (!ma || !ma->comm_bus) return CLS_ERR_INVALID;

    uint64_t now = cls_ma_time_us();
    return cls_comm_broadcast(ma->comm_bus, CLS_MSG_HEARTBEAT, &now, sizeof(uint64_t));
}

cls_status_t cls_multiagent_check_peers(cls_multiagent_t *ma, uint64_t timeout_us) {
    if (!ma) return CLS_ERR_INVALID;

    uint64_t now = cls_ma_time_us();
    for (uint32_t i = 0; i < ma->peer_count; i++) {
        if (ma->peers[i].status >= CLS_PEER_CONNECTED &&
            (now - ma->peers[i].last_heartbeat) > timeout_us) {
            ma->peers[i].status = CLS_PEER_DISCONNECTED;
        }
    }
    return CLS_OK;
}

cls_status_t cls_multiagent_send(cls_multiagent_t *ma, uint32_t peer_id, const cls_msg_t *msg) {
    if (!ma || !msg || !ma->comm_bus) return CLS_ERR_INVALID;

    /* Verify peer exists and is connected */
    cls_peer_t *peer = cls_multiagent_get_peer(ma, peer_id);
    if (!peer || peer->status < CLS_PEER_CONNECTED) return CLS_ERR_STATE;

    cls_msg_t send_msg = *msg;
    send_msg.src_agent = ma->local_id;
    send_msg.dst_agent = peer_id;
    send_msg.timestamp_us = cls_ma_time_us();

    return cls_comm_publish(ma->comm_bus, &send_msg, CLS_DELIVER_RELIABLE);
}

cls_status_t cls_multiagent_broadcast(cls_multiagent_t *ma, const cls_msg_t *msg) {
    if (!ma || !msg || !ma->comm_bus) return CLS_ERR_INVALID;

    cls_msg_t bcast = *msg;
    bcast.src_agent = ma->local_id;
    bcast.dst_agent = 0;
    return cls_comm_publish(ma->comm_bus, &bcast, CLS_DELIVER_BROADCAST);
}

/* ---- Collaboration ---- */

cls_status_t cls_multiagent_propose(cls_multiagent_t *ma, uint32_t peer_id,
                                     cls_collab_type_t type, uint32_t task_id,
                                     float bid_value, uint32_t *out_proposal_id) {
    if (!ma) return CLS_ERR_INVALID;
    if (ma->proposal_count >= CLS_MA_MAX_PROPOSALS) return CLS_ERR_OVERFLOW;

    cls_proposal_t *prop = &ma->proposals[ma->proposal_count];
    prop->proposal_id = ma->next_proposal_id++;
    prop->from_agent = ma->local_id;
    prop->type = type;
    prop->task_id = task_id;
    prop->bid_value = bid_value;
    prop->accepted = false;
    prop->created_at = cls_ma_time_us();
    prop->expires_at = prop->created_at + 30000000ULL;  /* 30s timeout */

    ma->proposal_count++;
    if (out_proposal_id) *out_proposal_id = prop->proposal_id;

    /* Send proposal via comm bus */
    if (ma->comm_bus) {
        cls_comm_send(ma->comm_bus, CLS_MSG_MULTIAGENT, prop,
                       sizeof(cls_proposal_t), peer_id);
    }

    return CLS_OK;
}

cls_status_t cls_multiagent_respond(cls_multiagent_t *ma, uint32_t proposal_id, bool accept) {
    if (!ma) return CLS_ERR_INVALID;

    for (uint32_t i = 0; i < ma->proposal_count; i++) {
        if (ma->proposals[i].proposal_id == proposal_id) {
            uint64_t now = cls_ma_time_us();
            if (now > ma->proposals[i].expires_at) return CLS_ERR_TIMEOUT;

            ma->proposals[i].accepted = accept;
            if (accept) ma->total_collaborations++;
            return CLS_OK;
        }
    }
    return CLS_ERR_NOT_FOUND;
}

cls_status_t cls_multiagent_on_collab(cls_multiagent_t *ma, cls_collab_fn callback, void *ctx) {
    if (!ma) return CLS_ERR_INVALID;
    ma->collab_callback = callback;
    ma->collab_ctx = ctx;
    return CLS_OK;
}

/* ---- Voting-based Consensus ---- */

/* Simple vote storage using proposals structure */
cls_status_t cls_multiagent_vote(cls_multiagent_t *ma, uint32_t topic_id,
                                  float vote_value) {
    if (!ma) return CLS_ERR_INVALID;
    if (ma->proposal_count >= CLS_MA_MAX_PROPOSALS) return CLS_ERR_OVERFLOW;

    cls_proposal_t *vote = &ma->proposals[ma->proposal_count];
    vote->proposal_id = ma->next_proposal_id++;
    vote->from_agent = ma->local_id;
    vote->type = CLS_COLLAB_CONSENSUS;
    vote->task_id = topic_id;
    vote->bid_value = vote_value;
    vote->created_at = cls_ma_time_us();
    ma->proposal_count++;

    /* Broadcast vote */
    if (ma->comm_bus) {
        cls_comm_broadcast(ma->comm_bus, CLS_MSG_MULTIAGENT, vote, sizeof(cls_proposal_t));
    }

    return CLS_OK;
}

cls_status_t cls_multiagent_get_consensus(cls_multiagent_t *ma, uint32_t topic_id,
                                           float *result, uint32_t *vote_count) {
    if (!ma || !result || !vote_count) return CLS_ERR_INVALID;

    float sum = 0.0f;
    uint32_t count = 0;

    for (uint32_t i = 0; i < ma->proposal_count; i++) {
        if (ma->proposals[i].type == CLS_COLLAB_CONSENSUS &&
            ma->proposals[i].task_id == topic_id) {
            sum += ma->proposals[i].bid_value;
            count++;
        }
    }

    if (count == 0) return CLS_ERR_NOT_FOUND;

    *result = sum / (float)count;
    *vote_count = count;
    ma->total_conflicts_resolved++;
    return CLS_OK;
}

cls_status_t cls_multiagent_share_knowledge(cls_multiagent_t *ma, uint32_t peer_id,
                                             const void *data, size_t len) {
    if (!ma || !data || !ma->comm_bus) return CLS_ERR_INVALID;
    return cls_comm_send(ma->comm_bus, CLS_MSG_KNOWLEDGE, data, len, peer_id);
}

void cls_multiagent_destroy(cls_multiagent_t *ma) {
    if (!ma) return;
    memset(ma, 0, sizeof(cls_multiagent_t));
}
