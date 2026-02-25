/*
 * ClawLobstars - Knowledge Graph Implementation
 * Node/edge management, cosine similarity search, BFS path finding
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../include/cls_framework.h"

static uint64_t cls_kg_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

cls_status_t cls_knowledge_init(cls_knowledge_t *kg, uint32_t max_nodes) {
    if (!kg || max_nodes == 0) return CLS_ERR_INVALID;

    memset(kg, 0, sizeof(cls_knowledge_t));
    kg->nodes = (cls_kg_node_t *)calloc(max_nodes, sizeof(cls_kg_node_t));
    if (!kg->nodes) return CLS_ERR_NOMEM;

    kg->max_nodes = max_nodes;
    kg->next_node_id = 1;
    return CLS_OK;
}

cls_status_t cls_knowledge_add_node(cls_knowledge_t *kg, const char *name,
                                     const float *embedding, uint32_t *out_id) {
    if (!kg || !name) return CLS_ERR_INVALID;
    if (kg->node_count >= kg->max_nodes) return CLS_ERR_OVERFLOW;

    cls_kg_node_t *node = &kg->nodes[kg->node_count];
    memset(node, 0, sizeof(cls_kg_node_t));

    node->node_id = kg->next_node_id++;
    strncpy(node->name, name, CLS_KG_NAME_MAX - 1);
    node->weight = 1.0f;
    node->created_at = cls_kg_time_us();
    node->accessed_at = node->created_at;
    node->access_count = 1;

    if (embedding) {
        memcpy(node->embedding, embedding, sizeof(float) * 32);
    }

    kg->node_count++;
    if (out_id) *out_id = node->node_id;
    return CLS_OK;
}

cls_status_t cls_knowledge_remove_node(cls_knowledge_t *kg, uint32_t node_id) {
    if (!kg) return CLS_ERR_INVALID;

    for (uint32_t i = 0; i < kg->node_count; i++) {
        if (kg->nodes[i].node_id == node_id) {
            /* Remove all edges pointing to this node */
            for (uint32_t j = 0; j < kg->node_count; j++) {
                if (j == i) continue;
                cls_kg_node_t *other = &kg->nodes[j];
                for (uint32_t e = 0; e < other->edge_count; ) {
                    if (other->edge_targets[e] == node_id) {
                        memmove(&other->edge_targets[e], &other->edge_targets[e + 1],
                                (other->edge_count - e - 1) * sizeof(uint32_t));
                        memmove(&other->edge_types[e], &other->edge_types[e + 1],
                                (other->edge_count - e - 1) * sizeof(cls_relation_t));
                        memmove(&other->edge_weights[e], &other->edge_weights[e + 1],
                                (other->edge_count - e - 1) * sizeof(float));
                        other->edge_count--;
                    } else {
                        e++;
                    }
                }
            }

            memmove(&kg->nodes[i], &kg->nodes[i + 1],
                    (kg->node_count - i - 1) * sizeof(cls_kg_node_t));
            kg->node_count--;
            return CLS_OK;
        }
    }
    return CLS_ERR_NOT_FOUND;
}

cls_kg_node_t *cls_knowledge_get_node(cls_knowledge_t *kg, uint32_t node_id) {
    if (!kg) return NULL;
    for (uint32_t i = 0; i < kg->node_count; i++) {
        if (kg->nodes[i].node_id == node_id) {
            kg->nodes[i].accessed_at = cls_kg_time_us();
            kg->nodes[i].access_count++;
            return &kg->nodes[i];
        }
    }
    return NULL;
}

cls_kg_node_t *cls_knowledge_find_by_name(cls_knowledge_t *kg, const char *name) {
    if (!kg || !name) return NULL;
    for (uint32_t i = 0; i < kg->node_count; i++) {
        if (strcmp(kg->nodes[i].name, name) == 0)
            return &kg->nodes[i];
    }
    return NULL;
}

cls_status_t cls_knowledge_add_edge(cls_knowledge_t *kg, uint32_t from_id,
                                     uint32_t to_id, cls_relation_t rel, float weight) {
    if (!kg) return CLS_ERR_INVALID;

    cls_kg_node_t *from = cls_knowledge_get_node(kg, from_id);
    if (!from) return CLS_ERR_NOT_FOUND;
    if (from->edge_count >= CLS_KG_MAX_EDGES) return CLS_ERR_OVERFLOW;

    /* Verify target exists */
    if (!cls_knowledge_get_node(kg, to_id)) return CLS_ERR_NOT_FOUND;

    /* Check duplicate */
    for (uint32_t i = 0; i < from->edge_count; i++) {
        if (from->edge_targets[i] == to_id && from->edge_types[i] == rel) {
            from->edge_weights[i] = weight;  /* Update weight */
            return CLS_OK;
        }
    }

    from->edge_targets[from->edge_count] = to_id;
    from->edge_types[from->edge_count] = rel;
    from->edge_weights[from->edge_count] = weight;
    from->edge_count++;
    return CLS_OK;
}

cls_status_t cls_knowledge_remove_edge(cls_knowledge_t *kg, uint32_t from_id, uint32_t to_id) {
    if (!kg) return CLS_ERR_INVALID;

    cls_kg_node_t *from = cls_knowledge_get_node(kg, from_id);
    if (!from) return CLS_ERR_NOT_FOUND;

    for (uint32_t i = 0; i < from->edge_count; i++) {
        if (from->edge_targets[i] == to_id) {
            memmove(&from->edge_targets[i], &from->edge_targets[i + 1],
                    (from->edge_count - i - 1) * sizeof(uint32_t));
            memmove(&from->edge_types[i], &from->edge_types[i + 1],
                    (from->edge_count - i - 1) * sizeof(cls_relation_t));
            memmove(&from->edge_weights[i], &from->edge_weights[i + 1],
                    (from->edge_count - i - 1) * sizeof(float));
            from->edge_count--;
            return CLS_OK;
        }
    }
    return CLS_ERR_NOT_FOUND;
}

cls_status_t cls_knowledge_query_related(cls_knowledge_t *kg, uint32_t node_id,
                                          cls_relation_t rel_filter,
                                          cls_kg_result_t *results, uint32_t *count) {
    if (!kg || !results || !count) return CLS_ERR_INVALID;

    cls_kg_node_t *node = cls_knowledge_get_node(kg, node_id);
    if (!node) return CLS_ERR_NOT_FOUND;

    uint32_t found = 0;
    for (uint32_t i = 0; i < node->edge_count; i++) {
        if (rel_filter != CLS_REL_CUSTOM && node->edge_types[i] != rel_filter)
            continue;

        cls_kg_node_t *target = cls_knowledge_get_node(kg, node->edge_targets[i]);
        if (!target) continue;

        results[found].node_id = target->node_id;
        results[found].relevance = node->edge_weights[i];
        results[found].node = target;
        found++;
    }

    kg->total_queries++;
    *count = found;
    return CLS_OK;
}

/* Cosine similarity between two vectors */
static float cosine_similarity(const float *a, const float *b, uint32_t dim) {
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    float denom = sqrtf(norm_a) * sqrtf(norm_b);
    return (denom > 1e-8f) ? (dot / denom) : 0.0f;
}

cls_status_t cls_knowledge_search(cls_knowledge_t *kg, const float *query_embedding,
                                   cls_kg_result_t *results, uint32_t max_results,
                                   uint32_t *result_count) {
    if (!kg || !query_embedding || !results || !result_count)
        return CLS_ERR_INVALID;

    /* Compute similarity for all nodes */
    typedef struct { uint32_t idx; float sim; } scored_t;
    scored_t *scores = (scored_t *)calloc(kg->node_count, sizeof(scored_t));
    if (!scores) return CLS_ERR_NOMEM;

    for (uint32_t i = 0; i < kg->node_count; i++) {
        scores[i].idx = i;
        scores[i].sim = cosine_similarity(query_embedding, kg->nodes[i].embedding, 32);
    }

    /* Simple selection sort for top-K */
    uint32_t k = CLS_MIN(max_results, kg->node_count);
    for (uint32_t i = 0; i < k; i++) {
        uint32_t best = i;
        for (uint32_t j = i + 1; j < kg->node_count; j++) {
            if (scores[j].sim > scores[best].sim) best = j;
        }
        if (best != i) {
            scored_t tmp = scores[i];
            scores[i] = scores[best];
            scores[best] = tmp;
        }
        results[i].node_id = kg->nodes[scores[i].idx].node_id;
        results[i].relevance = scores[i].sim;
        results[i].node = &kg->nodes[scores[i].idx];
    }

    free(scores);
    kg->total_queries++;
    *result_count = k;
    return CLS_OK;
}

/* BFS path finding */
cls_status_t cls_knowledge_find_path(cls_knowledge_t *kg, uint32_t from_id, uint32_t to_id,
                                      uint32_t *path, uint32_t *path_len, uint32_t max_depth) {
    if (!kg || !path || !path_len) return CLS_ERR_INVALID;
    if (max_depth == 0) max_depth = 10;

    /* BFS with parent tracking */
    uint32_t *queue = (uint32_t *)calloc(kg->node_count, sizeof(uint32_t));
    int32_t  *parent = (int32_t *)calloc(kg->max_nodes + 1, sizeof(int32_t));
    bool     *visited = (bool *)calloc(kg->max_nodes + 1, sizeof(bool));

    if (!queue || !parent || !visited) {
        free(queue); free(parent); free(visited);
        return CLS_ERR_NOMEM;
    }

    memset(parent, -1, (kg->max_nodes + 1) * sizeof(int32_t));

    uint32_t head = 0, tail = 0;
    queue[tail++] = from_id;
    visited[from_id] = true;
    bool found = false;

    while (head < tail && !found) {
        uint32_t current = queue[head++];

        cls_kg_node_t *node = cls_knowledge_get_node(kg, current);
        if (!node) continue;

        for (uint32_t i = 0; i < node->edge_count && !found; i++) {
            uint32_t neighbor = node->edge_targets[i];
            if (neighbor <= kg->max_nodes && !visited[neighbor]) {
                visited[neighbor] = true;
                parent[neighbor] = (int32_t)current;
                queue[tail++] = neighbor;

                if (neighbor == to_id) found = true;
            }
        }
    }

    if (!found) {
        free(queue); free(parent); free(visited);
        *path_len = 0;
        return CLS_ERR_NOT_FOUND;
    }

    /* Reconstruct path */
    uint32_t temp_path[128];
    uint32_t len = 0;
    int32_t cur = (int32_t)to_id;
    while (cur != -1 && len < 128 && len < max_depth) {
        temp_path[len++] = (uint32_t)cur;
        if ((uint32_t)cur == from_id) break;
        cur = parent[cur];
    }

    /* Reverse into output */
    *path_len = len;
    for (uint32_t i = 0; i < len; i++) {
        path[i] = temp_path[len - 1 - i];
    }

    free(queue); free(parent); free(visited);
    return CLS_OK;
}

cls_status_t cls_knowledge_save(const cls_knowledge_t *kg, void *buf, size_t *len) {
    if (!kg || !len) return CLS_ERR_INVALID;

    size_t needed = sizeof(uint32_t) * 2 + kg->node_count * sizeof(cls_kg_node_t);
    if (!buf) { *len = needed; return CLS_OK; }
    if (*len < needed) { *len = needed; return CLS_ERR_OVERFLOW; }

    uint8_t *p = (uint8_t *)buf;
    memcpy(p, &kg->node_count, sizeof(uint32_t)); p += sizeof(uint32_t);
    memcpy(p, &kg->next_node_id, sizeof(uint32_t)); p += sizeof(uint32_t);
    memcpy(p, kg->nodes, kg->node_count * sizeof(cls_kg_node_t));

    *len = needed;
    return CLS_OK;
}

cls_status_t cls_knowledge_load(cls_knowledge_t *kg, const void *buf, size_t len) {
    if (!kg || !buf || len < sizeof(uint32_t) * 2) return CLS_ERR_INVALID;

    const uint8_t *p = (const uint8_t *)buf;
    uint32_t count, next_id;
    memcpy(&count, p, sizeof(uint32_t)); p += sizeof(uint32_t);
    memcpy(&next_id, p, sizeof(uint32_t)); p += sizeof(uint32_t);

    if (count > kg->max_nodes) return CLS_ERR_OVERFLOW;
    if (len < sizeof(uint32_t) * 2 + count * sizeof(cls_kg_node_t))
        return CLS_ERR_INVALID;

    memcpy(kg->nodes, p, count * sizeof(cls_kg_node_t));
    kg->node_count = count;
    kg->next_node_id = next_id;
    return CLS_OK;
}

void cls_knowledge_destroy(cls_knowledge_t *kg) {
    if (!kg) return;
    free(kg->nodes);
    kg->nodes = NULL;
    kg->node_count = 0;
}
