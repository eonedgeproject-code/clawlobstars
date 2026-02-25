/*
 * ClawLobstars - Knowledge Graph
 * Ontology management, semantic retrieval, conceptual linking
 */

#ifndef CLS_KNOWLEDGE_H
#define CLS_KNOWLEDGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLS_KG_NAME_MAX     64
#define CLS_KG_MAX_EDGES    16

/* Relation types */
typedef enum {
    CLS_REL_IS_A        = 0,   /* Inheritance */
    CLS_REL_HAS_A       = 1,   /* Composition */
    CLS_REL_PART_OF     = 2,   /* Membership */
    CLS_REL_CAUSES      = 3,   /* Causality */
    CLS_REL_REQUIRES    = 4,   /* Dependency */
    CLS_REL_RELATED     = 5,   /* General association */
    CLS_REL_OPPOSITE    = 6,   /* Negation/contrast */
    CLS_REL_CUSTOM      = 255
} cls_relation_t;

/* Knowledge node */
typedef struct {
    uint32_t        node_id;
    char            name[CLS_KG_NAME_MAX];
    float           weight;             /* Node importance 0.0-1.0 */
    float           embedding[32];      /* Compact feature vector */
    uint32_t        edge_targets[CLS_KG_MAX_EDGES];
    cls_relation_t  edge_types[CLS_KG_MAX_EDGES];
    float           edge_weights[CLS_KG_MAX_EDGES];
    uint32_t        edge_count;
    uint64_t        created_at;
    uint64_t        accessed_at;
    uint32_t        access_count;
} cls_kg_node_t;

/* Query result */
typedef struct {
    uint32_t        node_id;
    float           relevance;
    const cls_kg_node_t *node;
} cls_kg_result_t;

/* Knowledge graph context */
typedef struct cls_knowledge {
    cls_kg_node_t  *nodes;
    uint32_t        node_count;
    uint32_t        max_nodes;
    uint32_t        next_node_id;
    uint64_t        total_queries;
} cls_knowledge_t;

/* ---- API ---- */

cls_status_t cls_knowledge_init(cls_knowledge_t *kg, uint32_t max_nodes);

/* Node CRUD */
cls_status_t cls_knowledge_add_node(cls_knowledge_t *kg, const char *name,
                                     const float *embedding, uint32_t *out_id);
cls_status_t cls_knowledge_remove_node(cls_knowledge_t *kg, uint32_t node_id);
cls_kg_node_t *cls_knowledge_get_node(cls_knowledge_t *kg, uint32_t node_id);
cls_kg_node_t *cls_knowledge_find_by_name(cls_knowledge_t *kg, const char *name);

/* Edge management */
cls_status_t cls_knowledge_add_edge(cls_knowledge_t *kg, uint32_t from_id,
                                     uint32_t to_id, cls_relation_t rel, float weight);
cls_status_t cls_knowledge_remove_edge(cls_knowledge_t *kg, uint32_t from_id, uint32_t to_id);

/* Queries */
cls_status_t cls_knowledge_query_related(cls_knowledge_t *kg, uint32_t node_id,
                                          cls_relation_t rel_filter,
                                          cls_kg_result_t *results, uint32_t *count);

/* Semantic similarity search (cosine on embeddings) */
cls_status_t cls_knowledge_search(cls_knowledge_t *kg, const float *query_embedding,
                                   cls_kg_result_t *results, uint32_t max_results,
                                   uint32_t *result_count);

/* Path finding between nodes (BFS) */
cls_status_t cls_knowledge_find_path(cls_knowledge_t *kg, uint32_t from_id, uint32_t to_id,
                                      uint32_t *path, uint32_t *path_len, uint32_t max_depth);

/* Serialization */
cls_status_t cls_knowledge_save(const cls_knowledge_t *kg, void *buf, size_t *len);
cls_status_t cls_knowledge_load(cls_knowledge_t *kg, const void *buf, size_t len);

void cls_knowledge_destroy(cls_knowledge_t *kg);

#ifdef __cplusplus
}
#endif

#endif /* CLS_KNOWLEDGE_H */
