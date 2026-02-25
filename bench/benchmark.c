/*
 * ClawLobstars — Performance Benchmark
 * Measures throughput and latency across all modules
 *
 * Build: make bench
 * Run:   ./build/bin/cls_bench
 */

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../src/include/cls_framework.h"

static uint64_t now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

#define BENCH(name, iters, code) do { \
    uint64_t _s = now_us(); \
    for (int _i = 0; _i < (iters); _i++) { code; } \
    uint64_t _e = now_us(); \
    double _us = (double)(_e - _s) / (double)(iters); \
    double _ops = (iters) / ((double)(_e - _s) / 1000000.0); \
    printf("  %-35s %8.2f µs/op  %12.0f ops/s\n", name, _us, _ops); \
} while(0)

int main(void) {
    printf("\n  \033[32m╔══════════════════════════════════════════════════╗\033[0m\n");
    printf("  \033[32m║     CLAWLOBSTARS BENCHMARK v0.1.0-dev            ║\033[0m\n");
    printf("  \033[32m╚══════════════════════════════════════════════════╝\033[0m\n\n");

    /* Memory */
    printf("  \033[33m── MEMORY ──\033[0m\n");
    cls_memory_ctx_t mem;
    cls_memory_init(&mem, 1024 * 256);

    BENCH("memory_store", 100000, {
        char key[32]; snprintf(key, sizeof(key), "bench:%d", _i);
        cls_memory_store(&mem, key, "benchmark-value", 16);
    });

    BENCH("memory_retrieve", 100000, {
        char key[32]; snprintf(key, sizeof(key), "bench:%d", _i % 1000);
        char buf[64]; size_t len = sizeof(buf);
        cls_memory_retrieve(&mem, key, buf, &len);
    });

    BENCH("memory_exists", 100000, {
        char key[32]; snprintf(key, sizeof(key), "bench:%d", _i % 1000);
        cls_memory_exists(&mem, key);
    });

    cls_memory_destroy(&mem);

    /* Cognitive */
    printf("\n  \033[33m── COGNITIVE ──\033[0m\n");
    float features[] = {0.5f, 0.3f, 0.7f, 0.9f, 0.2f, 0.8f, 0.4f, 0.6f};
    cls_input_t input = {.features = features, .feature_count = 8};
    cls_decision_t dec;

    cls_cognitive_t cog_rb; cls_cognitive_init(&cog_rb, CLS_MODEL_RULE_BASED);
    BENCH("infer_rule_based", 1000000, {
        cls_cognitive_infer(&cog_rb, &input, &dec);
    });
    cls_cognitive_destroy(&cog_rb);

    cls_cognitive_t cog_dt; cls_cognitive_init(&cog_dt, CLS_MODEL_DECISION_TREE);
    BENCH("infer_decision_tree", 1000000, {
        cls_cognitive_infer(&cog_dt, &input, &dec);
    });
    cls_cognitive_destroy(&cog_dt);

    cls_cognitive_t cog_by; cls_cognitive_init(&cog_by, CLS_MODEL_BAYESIAN);
    BENCH("infer_bayesian", 1000000, {
        cls_cognitive_infer(&cog_by, &input, &dec);
    });
    cls_cognitive_destroy(&cog_by);

    /* Knowledge Graph */
    printf("\n  \033[33m── KNOWLEDGE GRAPH ──\033[0m\n");
    cls_knowledge_t kg;
    cls_knowledge_init(&kg, 1024);

    BENCH("knowledge_add_node", 1000, {
        float emb[32] = {(float)_i * 0.001f};
        uint32_t nid;
        cls_knowledge_add_node(&kg, "bench_node", emb, &nid);
    });

    float q[32] = {0.5f, 0.5f};
    cls_kg_result_t res[10]; uint32_t cnt;
    BENCH("knowledge_search (1000 nodes)", 10000, {
        cls_knowledge_search(&kg, q, res, 5, &cnt);
    });

    cls_knowledge_destroy(&kg);

    /* Comm Bus */
    printf("\n  \033[33m── COMM BUS ──\033[0m\n");
    cls_comm_bus_t bus; cls_comm_init(&bus, 1);

    BENCH("comm_publish", 100000, {
        cls_comm_broadcast(&bus, CLS_MSG_SYSTEM, "b", 2);
        cls_comm_process(&bus, 1);
    });

    cls_comm_destroy(&bus);

    /* Planning */
    printf("\n  \033[33m── PLANNING ──\033[0m\n");
    cls_planner_t pl; cls_planner_init(&pl, 1024, 8);
    cls_decision_t decs[] = {
        {.action_id=1,.confidence=0.9f,.priority=80},
        {.action_id=2,.confidence=0.7f,.priority=60},
        {.action_id=3,.confidence=0.5f,.priority=40},
    };

    BENCH("planner_generate", 10000, {
        cls_plan_t *p;
        cls_planner_generate(&pl, decs, 3, &p);
    });

    cls_planner_destroy(&pl);

    /* Security */
    printf("\n  \033[33m── SECURITY ──\033[0m\n");
    cls_security_ctx_t sec;
    cls_security_init(&sec, CLS_SEC_HIGH);
    uint8_t key[] = "benchmark-key";
    cls_security_set_key(&sec, key, sizeof(key));

    char plain[128] = "This is a test message for encryption benchmarking!!";
    char cipher[128], decrypted[128];

    BENCH("security_encrypt (52 bytes)", 1000000, {
        cls_security_encrypt(&sec, plain, 52, cipher);
    });

    BENCH("security_decrypt (52 bytes)", 1000000, {
        cls_security_decrypt(&sec, cipher, 52, decrypted);
    });

    BENCH("security_hash", 1000000, {
        uint8_t h[32];
        cls_security_hash("benchmark data", 14, h);
    });

    uint8_t cred[] = "creds";
    cls_auth_token_t tok;
    cls_security_auth(&sec, 1, cred, sizeof(cred), &tok);
    BENCH("security_validate_token", 1000000, {
        cls_security_validate_token(&sec, &tok);
    });

    cls_security_destroy(&sec);

    /* Agent Loop */
    printf("\n  \033[33m── AGENT LOOP ──\033[0m\n");
    cls_agent_t agent;
    cls_config_t cfg = CLS_CONFIG_DEFAULT;
    cfg.agent_id = 1; cfg.agent_name = "bench";
    cfg.memory_size = 1024*64; cfg.max_sensors = 2;
    cfg.log_level = CLS_LOG_FATAL; /* Suppress output */
    cls_agent_init(&agent, &cfg);

    BENCH("agent_step (full cycle)", 100000, {
        cls_agent_step(&agent);
    });

    cls_agent_shutdown(&agent);
    cls_agent_destroy(&agent);

    /* Summary */
    printf("\n  \033[32m══════════════════════════════════════════════════\033[0m\n");
    printf("  Benchmark complete.\n");
    printf("  \033[32m══════════════════════════════════════════════════\033[0m\n\n");

    return 0;
}
