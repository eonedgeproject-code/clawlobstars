/*
 * ClawLobstars - Multi-Agent Operations
 * Agent discovery, collaboration, resource sharing, conflict resolution
 */

#ifndef CLS_MULTIAGENT_H
#define CLS_MULTIAGENT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLS_MA_MAX_PEERS     32
#define CLS_MA_MAX_PROPOSALS 16

/* Peer agent status */
typedef enum {
    CLS_PEER_UNKNOWN     = 0,
    CLS_PEER_DISCOVERED  = 1,
    CLS_PEER_CONNECTED   = 2,
    CLS_PEER_COLLABORATING = 3,
    CLS_PEER_DISCONNECTED = 4
} cls_peer_status_t;

/* Collaboration types */
typedef enum {
    CLS_COLLAB_TASK_SHARE  = 0,   /* Split work */
    CLS_COLLAB_CONSENSUS   = 1,   /* Agree on decision */
    CLS_COLLAB_AUCTION     = 2,   /* Bid for tasks */
    CLS_COLLAB_BLACKBOARD  = 3    /* Shared knowledge */
} cls_collab_type_t;

/* Peer agent descriptor */
typedef struct {
    uint32_t            agent_id;
    char                name[64];
    cls_peer_status_t   status;
    float               capability_score;
    float               trust_score;
    uint64_t            last_heartbeat;
    uint32_t            tasks_shared;
    uint32_t            tasks_completed;
} cls_peer_t;

/* Collaboration proposal */
typedef struct {
    uint32_t            proposal_id;
    uint32_t            from_agent;
    cls_collab_type_t   type;
    uint32_t            task_id;
    float               bid_value;
    bool                accepted;
    uint64_t            created_at;
    uint64_t            expires_at;
} cls_proposal_t;

/* Collaboration callback */
typedef cls_status_t (*cls_collab_fn)(const cls_proposal_t *proposal, void *user_ctx);

/* Agent list for discovery results */
typedef struct {
    cls_peer_t  peers[CLS_MA_MAX_PEERS];
    uint32_t    count;
} cls_agent_list_t;

/* Multi-agent context */
struct cls_multiagent {
    uint32_t            local_id;
    cls_peer_t          peers[CLS_MA_MAX_PEERS];
    uint32_t            peer_count;
    cls_proposal_t      proposals[CLS_MA_MAX_PROPOSALS];
    uint32_t            proposal_count;
    uint32_t            next_proposal_id;
    cls_collab_fn       collab_callback;
    void               *collab_ctx;
    cls_comm_bus_t     *comm_bus;
    uint64_t            total_collaborations;
    uint64_t            total_conflicts_resolved;
};

/* ---- API ---- */

cls_status_t cls_multiagent_init(cls_multiagent_t *ma, uint32_t local_id, cls_comm_bus_t *bus);

/* Discovery */
cls_status_t cls_multiagent_discover(cls_multiagent_t *ma, cls_agent_list_t *peers);
cls_status_t cls_multiagent_register_peer(cls_multiagent_t *ma, const cls_peer_t *peer);
cls_status_t cls_multiagent_remove_peer(cls_multiagent_t *ma, uint32_t agent_id);
cls_peer_t  *cls_multiagent_get_peer(cls_multiagent_t *ma, uint32_t agent_id);

/* Heartbeat */
cls_status_t cls_multiagent_send_heartbeat(cls_multiagent_t *ma);
cls_status_t cls_multiagent_check_peers(cls_multiagent_t *ma, uint64_t timeout_us);

/* Messaging */
cls_status_t cls_multiagent_send(cls_multiagent_t *ma, uint32_t peer_id, const cls_msg_t *msg);
cls_status_t cls_multiagent_broadcast(cls_multiagent_t *ma, const cls_msg_t *msg);

/* Collaboration */
cls_status_t cls_multiagent_propose(cls_multiagent_t *ma, uint32_t peer_id,
                                     cls_collab_type_t type, uint32_t task_id,
                                     float bid_value, uint32_t *out_proposal_id);
cls_status_t cls_multiagent_respond(cls_multiagent_t *ma, uint32_t proposal_id, bool accept);
cls_status_t cls_multiagent_on_collab(cls_multiagent_t *ma, cls_collab_fn callback, void *ctx);

/* Conflict resolution (voting-based) */
cls_status_t cls_multiagent_vote(cls_multiagent_t *ma, uint32_t topic_id,
                                  float vote_value);
cls_status_t cls_multiagent_get_consensus(cls_multiagent_t *ma, uint32_t topic_id,
                                           float *result, uint32_t *vote_count);

/* Resource sharing */
cls_status_t cls_multiagent_share_knowledge(cls_multiagent_t *ma, uint32_t peer_id,
                                             const void *data, size_t len);

void cls_multiagent_destroy(cls_multiagent_t *ma);

#ifdef __cplusplus
}
#endif

#endif /* CLS_MULTIAGENT_H */
