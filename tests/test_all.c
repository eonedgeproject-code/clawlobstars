/*
 * ClawLobstars — Unit Test Suite
 * Tests all 13 modules with pass/fail reporting
 *
 * Build: make test
 * Run:   ./build/bin/cls_test
 */

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../src/include/cls_framework.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  %-50s", name); \
    fflush(stdout); \
} while(0)

#define PASS() do { \
    tests_passed++; \
    printf("\033[32m✓ PASS\033[0m\n"); \
} while(0)

#define FAIL(reason) do { \
    tests_failed++; \
    printf("\033[31m✗ FAIL\033[0m  %s\n", reason); \
} while(0)

#define ASSERT(cond, reason) do { \
    if (cond) { PASS(); } else { FAIL(reason); } \
} while(0)

/* ============================================================ */

static void test_memory(void) {
    printf("\n  \033[33m── MEMORY ──\033[0m\n");
    cls_memory_ctx_t mem;

    TEST("memory_init");
    ASSERT(CLS_IS_OK(cls_memory_init(&mem, 1024 * 64)), "init failed");

    TEST("memory_store");
    ASSERT(CLS_IS_OK(cls_memory_store(&mem, "key1", "hello", 6)), "store failed");

    TEST("memory_retrieve");
    char buf[64]; size_t len = sizeof(buf);
    cls_status_t s = cls_memory_retrieve(&mem, "key1", buf, &len);
    ASSERT(CLS_IS_OK(s) && strcmp(buf, "hello") == 0, "retrieve mismatch");

    TEST("memory_exists");
    ASSERT(cls_memory_exists(&mem, "key1") == true, "should exist");

    TEST("memory_not_exists");
    ASSERT(cls_memory_exists(&mem, "nokey") == false, "should not exist");

    TEST("memory_delete");
    cls_memory_delete(&mem, "key1");
    ASSERT(cls_memory_exists(&mem, "key1") == false, "should be deleted");

    TEST("memory_ttl_store");
    ASSERT(CLS_IS_OK(cls_memory_store_ttl(&mem, "ttl1", "temp", 5, 1)), "ttl store failed");

    TEST("memory_overwrite");
    cls_memory_store(&mem, "ow", "first", 6);
    cls_memory_store(&mem, "ow", "second", 7);
    len = sizeof(buf);
    cls_memory_retrieve(&mem, "ow", buf, &len);
    ASSERT(strcmp(buf, "second") == 0, "overwrite failed");

    TEST("memory_query_wildcard");
    cls_memory_store(&mem, "prefix:a", "aa", 3);
    cls_memory_store(&mem, "prefix:b", "bb", 3);
    ASSERT(cls_memory_exists(&mem, "prefix:a") && cls_memory_exists(&mem, "prefix:b"),
           "prefixed entries should exist");

    TEST("memory_stats");
    size_t used, total; uint32_t entries;
    cls_memory_stats(&mem, &used, &total, &entries);
    ASSERT(entries > 0 && total == 1024 * 64, "stats wrong");

    cls_memory_destroy(&mem);
}

static cls_status_t dummy_sensor_read(void *ctx, void *buf, size_t *len) {
    (void)ctx; float v = 0.5f;
    if (*len < sizeof(float)) return CLS_ERR_OVERFLOW;
    memcpy(buf, &v, sizeof(float)); *len = sizeof(float);
    return CLS_OK;
}

static void test_perception(void) {
    printf("\n  \033[33m── PERCEPTION ──\033[0m\n");
    cls_perception_t perc;
    cls_perception_init(&perc, 4);

    TEST("perception_init");
    ASSERT(perc.sensor_count == 0, "should start empty");

    TEST("perception_register");
    cls_sensor_t s = {.id=1,.type=CLS_SENSOR_NUMERIC,.name="test",.read_fn=dummy_sensor_read,.active=true};
    ASSERT(CLS_IS_OK(cls_perception_register(&perc, &s)), "register failed");

    TEST("perception_sensor_count");
    ASSERT(perc.sensor_count == 1, "count should be 1");

    TEST("perception_overflow");
    for (int i=2;i<=4;i++) {
        cls_sensor_t si = {.id=(uint32_t)i,.type=CLS_SENSOR_NUMERIC,.name="s",.read_fn=dummy_sensor_read,.active=true};
        cls_perception_register(&perc, &si);
    }
    cls_sensor_t extra = {.id=99,.name="x",.read_fn=dummy_sensor_read,.active=true};
    ASSERT(cls_perception_register(&perc, &extra) == CLS_ERR_OVERFLOW, "should overflow at max");

    TEST("perception_unregister");
    ASSERT(CLS_IS_OK(cls_perception_unregister(&perc, 1)), "unregister failed");

    cls_perception_destroy(&perc);
}

static void test_cognitive(void) {
    printf("\n  \033[33m── COGNITIVE ──\033[0m\n");
    float features[] = {0.8f, 0.3f, 0.6f, 0.9f};
    cls_input_t input = {.features = features, .feature_count = 4};
    cls_decision_t dec;

    TEST("cognitive_rule_based");
    cls_cognitive_t cog; cls_cognitive_init(&cog, CLS_MODEL_RULE_BASED);
    cls_status_t s = cls_cognitive_infer(&cog, &input, &dec);
    ASSERT(CLS_IS_OK(s) && dec.confidence > 0.0f, "rule inference failed");
    cls_cognitive_destroy(&cog);

    TEST("cognitive_decision_tree");
    cls_cognitive_init(&cog, CLS_MODEL_DECISION_TREE);
    s = cls_cognitive_infer(&cog, &input, &dec);
    ASSERT(CLS_IS_OK(s) && dec.confidence > 0.0f, "tree inference failed");
    cls_cognitive_destroy(&cog);

    TEST("cognitive_bayesian");
    cls_cognitive_init(&cog, CLS_MODEL_BAYESIAN);
    s = cls_cognitive_infer(&cog, &input, &dec);
    ASSERT(CLS_IS_OK(s) && dec.confidence > 0.0f, "bayesian inference failed");
    cls_cognitive_destroy(&cog);

    TEST("cognitive_batch_inference");
    cls_cognitive_init(&cog, CLS_MODEL_RULE_BASED);
    cls_input_t inputs[3] = {input, input, input};
    cls_decision_t decs[3];
    s = cls_cognitive_infer_batch(&cog, inputs, 3, decs);
    ASSERT(CLS_IS_OK(s), "batch inference failed");
    cls_cognitive_destroy(&cog);

    TEST("cognitive_metrics");
    cls_cognitive_init(&cog, CLS_MODEL_RULE_BASED);
    cls_cognitive_infer(&cog, &input, &dec);
    cls_model_metrics_t m; cls_cognitive_get_metrics(&cog, &m);
    ASSERT(m.total_inferences == 1, "metrics wrong");
    cls_cognitive_destroy(&cog);
}

static void test_planning(void) {
    printf("\n  \033[33m── PLANNING ──\033[0m\n");
    cls_planner_t pl;

    TEST("planner_init");
    ASSERT(CLS_IS_OK(cls_planner_init(&pl, 8, 4)), "init failed");

    TEST("planner_add_goal");
    cls_goal_t g = {.goal_id=1,.description="test",.priority=CLS_PRIORITY_HIGH,.utility=0.9f};
    ASSERT(CLS_IS_OK(cls_planner_add_goal(&pl, &g)), "add goal failed");

    TEST("planner_get_goal");
    ASSERT(cls_planner_get_goal(&pl, 1) != NULL, "goal not found");

    TEST("planner_generate_plan");
    cls_decision_t decs[] = {
        {.action_id=10,.confidence=0.9f,.priority=80},
        {.action_id=20,.confidence=0.7f,.priority=60},
    };
    cls_plan_t *plan;
    ASSERT(CLS_IS_OK(cls_planner_generate(&pl, decs, 2, &plan)), "generate failed");

    TEST("plan_task_count");
    ASSERT(plan->task_count == 2, "should have 2 tasks");

    TEST("plan_next_task");
    cls_task_t *t;
    ASSERT(CLS_IS_OK(cls_plan_next_task(plan, &t)), "next task failed");

    TEST("plan_complete_task");
    ASSERT(CLS_IS_OK(cls_plan_complete_task(plan, t->task_id, true)), "complete failed");

    TEST("planner_evaluate");
    cls_strategy_eval_t ev;
    ASSERT(CLS_IS_OK(cls_planner_evaluate(&pl, plan, &ev)), "eval failed");

    TEST("planner_remove_goal");
    ASSERT(CLS_IS_OK(cls_planner_remove_goal(&pl, 1)), "remove goal failed");

    cls_planner_destroy(&pl);
}

static cls_status_t action_ok_fn(uint32_t id, const void *p, size_t l) {
    (void)id;(void)p;(void)l; return CLS_OK;
}
static cls_status_t action_fail_fn(uint32_t id, const void *p, size_t l) {
    (void)id;(void)p;(void)l; return CLS_ERR_INTERNAL;
}
static cls_status_t action_rb_fn(uint32_t id, const void *p, size_t l) {
    (void)id;(void)p;(void)l; return CLS_OK;
}

static void test_action(void) {
    printf("\n  \033[33m── ACTION EXECUTOR ──\033[0m\n");
    cls_action_exec_t ex;

    TEST("action_init");
    ASSERT(CLS_IS_OK(cls_action_init(&ex, 8, 32)), "init failed");

    TEST("action_register");
    cls_action_handler_t h = {.action_id=1,.name="test",.execute_fn=action_ok_fn,.rollback_fn=action_rb_fn};
    ASSERT(CLS_IS_OK(cls_action_register(&ex, &h)), "register failed");

    TEST("action_execute_success");
    cls_action_record_t rec;
    cls_status_t s = cls_action_execute(&ex, 1, NULL, 0, &rec);
    ASSERT(CLS_IS_OK(s) && rec.status == CLS_ACTION_SUCCESS, "execute failed");

    TEST("action_execute_failure");
    cls_action_handler_t hf = {.action_id=2,.name="fail",.execute_fn=action_fail_fn};
    cls_action_register(&ex, &hf);
    s = cls_action_execute(&ex, 2, NULL, 0, &rec);
    ASSERT(!CLS_IS_OK(s) && rec.status == CLS_ACTION_FAILED, "should fail");

    TEST("action_rollback");
    cls_action_execute(&ex, 1, NULL, 0, &rec);
    ASSERT(CLS_IS_OK(cls_action_rollback(&ex, rec.exec_id)), "rollback failed");

    TEST("action_stats");
    uint64_t total, ok, fail, rb;
    cls_action_stats(&ex, &total, &ok, &fail, &rb);
    ASSERT(total == 3 && rb == 1, "stats wrong");

    cls_action_destroy(&ex);
}

static void test_knowledge(void) {
    printf("\n  \033[33m── KNOWLEDGE GRAPH ──\033[0m\n");
    cls_knowledge_t kg;

    TEST("knowledge_init");
    ASSERT(CLS_IS_OK(cls_knowledge_init(&kg, 64)), "init failed");

    TEST("knowledge_add_node");
    uint32_t id1, id2;
    float emb1[32] = {1.0f, 0.0f}; float emb2[32] = {0.0f, 1.0f};
    ASSERT(CLS_IS_OK(cls_knowledge_add_node(&kg, "NodeA", emb1, &id1)), "add failed");
    cls_knowledge_add_node(&kg, "NodeB", emb2, &id2);

    TEST("knowledge_find_by_name");
    ASSERT(cls_knowledge_find_by_name(&kg, "NodeA") != NULL, "not found");

    TEST("knowledge_add_edge");
    ASSERT(CLS_IS_OK(cls_knowledge_add_edge(&kg, id1, id2, CLS_REL_RELATED, 0.8f)), "edge failed");

    TEST("knowledge_search_cosine");
    float q[32] = {0.9f, 0.1f};
    cls_kg_result_t res[3]; uint32_t cnt;
    cls_knowledge_search(&kg, q, res, 2, &cnt);
    ASSERT(cnt == 2 && res[0].node_id == id1, "search wrong — closest should be NodeA");

    TEST("knowledge_find_path");
    uint32_t path[16]; uint32_t plen;
    cls_status_t s = cls_knowledge_find_path(&kg, id1, id2, path, &plen, 5);
    ASSERT(CLS_IS_OK(s) && plen == 2, "path failed");

    TEST("knowledge_remove_node");
    ASSERT(CLS_IS_OK(cls_knowledge_remove_node(&kg, id2)), "remove failed");

    TEST("knowledge_save_load");
    size_t sz = 0;
    cls_knowledge_save(&kg, NULL, &sz);
    void *buf = malloc(sz);
    cls_knowledge_save(&kg, buf, &sz);
    cls_knowledge_t kg2; cls_knowledge_init(&kg2, 64);
    s = cls_knowledge_load(&kg2, buf, sz);
    ASSERT(CLS_IS_OK(s) && kg2.node_count == kg.node_count, "save/load mismatch");
    free(buf);
    cls_knowledge_destroy(&kg2);
    cls_knowledge_destroy(&kg);
}

static int comm_recv_count = 0;
static void comm_handler(const cls_msg_t *m, void *ctx) { (void)m;(void)ctx; comm_recv_count++; }

static void test_comm(void) {
    printf("\n  \033[33m── COMM BUS ──\033[0m\n");
    cls_comm_bus_t bus;

    TEST("comm_init");
    ASSERT(CLS_IS_OK(cls_comm_init(&bus, 1)), "init failed");

    TEST("comm_subscribe");
    uint32_t sid;
    ASSERT(CLS_IS_OK(cls_comm_subscribe(&bus, 0, comm_handler, NULL, &sid)), "sub failed");

    TEST("comm_broadcast_and_process");
    comm_recv_count = 0;
    cls_comm_broadcast(&bus, CLS_MSG_SYSTEM, "hi", 3);
    cls_comm_process(&bus, 10);
    ASSERT(comm_recv_count == 1, "should receive 1 message");

    TEST("comm_send_and_process");
    comm_recv_count = 0;
    cls_comm_send(&bus, CLS_MSG_PERCEPTION, "data", 5, 1);
    cls_comm_process(&bus, 10);
    ASSERT(comm_recv_count == 1, "should receive directed msg");

    TEST("comm_stats");
    uint64_t sent, del, drop;
    cls_comm_stats(&bus, &sent, &del, &drop);
    ASSERT(sent == 2 && del >= 2, "stats wrong");

    TEST("comm_unsubscribe");
    ASSERT(CLS_IS_OK(cls_comm_unsubscribe(&bus, sid)), "unsub failed");

    cls_comm_destroy(&bus);
}

static void test_multiagent(void) {
    printf("\n  \033[33m── MULTI-AGENT ──\033[0m\n");
    cls_comm_bus_t bus; cls_comm_init(&bus, 1);
    cls_multiagent_t ma;

    TEST("multiagent_init");
    ASSERT(CLS_IS_OK(cls_multiagent_init(&ma, 1, &bus)), "init failed");

    TEST("multiagent_register_peer");
    cls_peer_t p = {.agent_id=2,.name="peer",.status=CLS_PEER_CONNECTED,.trust_score=0.9f};
    ASSERT(CLS_IS_OK(cls_multiagent_register_peer(&ma, &p)), "register failed");

    TEST("multiagent_get_peer");
    ASSERT(cls_multiagent_get_peer(&ma, 2) != NULL, "peer not found");

    TEST("multiagent_propose");
    uint32_t pid;
    ASSERT(CLS_IS_OK(cls_multiagent_propose(&ma, 2, CLS_COLLAB_TASK_SHARE, 1, 0.8f, &pid)), "propose failed");

    TEST("multiagent_respond");
    ASSERT(CLS_IS_OK(cls_multiagent_respond(&ma, pid, true)), "respond failed");

    TEST("multiagent_vote_consensus");
    cls_multiagent_vote(&ma, 50, 0.7f);
    cls_multiagent_vote(&ma, 50, 0.9f);
    float cons; uint32_t vc;
    cls_status_t s = cls_multiagent_get_consensus(&ma, 50, &cons, &vc);
    ASSERT(CLS_IS_OK(s) && vc == 2, "consensus failed");

    TEST("multiagent_remove_peer");
    ASSERT(CLS_IS_OK(cls_multiagent_remove_peer(&ma, 2)), "remove failed");

    cls_multiagent_destroy(&ma);
    cls_comm_destroy(&bus);
}

static void test_security(void) {
    printf("\n  \033[33m── SECURITY ──\033[0m\n");
    cls_security_ctx_t sec;

    TEST("security_init");
    ASSERT(CLS_IS_OK(cls_security_init(&sec, CLS_SEC_HIGH)), "init failed");

    TEST("security_set_key");
    uint8_t key[] = "test-key-2025";
    ASSERT(CLS_IS_OK(cls_security_set_key(&sec, key, sizeof(key))), "set key failed");

    TEST("security_add_role");
    cls_role_t r = {.role_id=1,.name="admin",.permissions=CLS_PERM_ALL};
    ASSERT(CLS_IS_OK(cls_security_add_role(&sec, &r)), "add role failed");

    TEST("security_check_permission_grant");
    ASSERT(CLS_IS_OK(cls_security_check_permission(&sec, 1, CLS_PERM_EXECUTE)), "should grant");

    TEST("security_check_permission_deny");
    cls_role_t r2 = {.role_id=2,.name="viewer",.permissions=CLS_PERM_READ};
    cls_security_add_role(&sec, &r2);
    ASSERT(cls_security_check_permission(&sec, 2, CLS_PERM_EXECUTE) == CLS_ERR_SECURITY, "should deny");

    TEST("security_auth_token");
    uint8_t cred[] = "credentials";
    cls_auth_token_t tok;
    ASSERT(CLS_IS_OK(cls_security_auth(&sec, 1, cred, sizeof(cred), &tok)), "auth failed");

    TEST("security_validate_token");
    ASSERT(CLS_IS_OK(cls_security_validate_token(&sec, &tok)), "token should be valid");

    TEST("security_revoke_token");
    cls_security_revoke_token(&sec, &tok);
    ASSERT(cls_security_validate_token(&sec, &tok) == CLS_ERR_SECURITY, "should be revoked");

    TEST("security_encrypt_decrypt");
    const char *msg = "secret message 12345";
    char enc[64], dec[64];
    cls_security_encrypt(&sec, msg, strlen(msg), enc);
    cls_security_decrypt(&sec, enc, strlen(msg), dec);
    ASSERT(memcmp(msg, dec, strlen(msg)) == 0, "encrypt/decrypt mismatch");

    TEST("security_hash");
    uint8_t h1[32], h2[32];
    cls_security_hash("abc", 3, h1);
    cls_security_hash("abc", 3, h2);
    ASSERT(memcmp(h1, h2, 32) == 0, "same input should produce same hash");

    TEST("security_audit_log");
    cls_audit_entry_t entries[16]; uint32_t cnt;
    cls_security_get_audit(&sec, entries, &cnt, 16);
    ASSERT(cnt > 0, "should have audit entries");

    cls_security_destroy(&sec);
}

static void test_training(void) {
    printf("\n  \033[33m── TRAINING ──\033[0m\n");
    cls_cognitive_t cog; cls_cognitive_init(&cog, CLS_MODEL_RULE_BASED);
    cls_training_t tr;

    TEST("training_init");
    ASSERT(CLS_IS_OK(cls_training_init(&tr, &cog, CLS_TRAIN_REPLAY, 128)), "init failed");

    TEST("training_set_params");
    cls_training_set_lr(&tr, 0.01f);
    cls_training_set_epsilon(&tr, 1.0f, 0.99f, 0.05f);
    ASSERT(tr.epsilon == 1.0f, "epsilon wrong");

    TEST("training_add_experience");
    float st[] = {0.5f, 0.3f}; float ns[] = {0.6f, 0.4f};
    cls_experience_t xp = {.state=st,.state_dim=2,.action_taken=1,.reward=1.0f,.next_state=ns};
    ASSERT(CLS_IS_OK(cls_training_add_experience(&tr, &xp)), "add exp failed");

    TEST("training_buffer_count");
    ASSERT(tr.buffer_count == 1, "buffer should have 1");

    TEST("training_start_stop");
    cls_training_start(&tr);
    ASSERT(tr.training_active == true, "should be active");
    cls_training_stop(&tr);
    ASSERT(tr.training_active == false, "should be inactive");

    TEST("training_step");
    cls_training_start(&tr);
    for (int i=0;i<20;i++) {
        float s[] = {0.1f*(float)i, 0.5f}; float n[] = {0.1f*(float)(i+1), 0.4f};
        cls_experience_t e = {.state=s,.state_dim=2,.action_taken=(uint32_t)(i%2),.reward=0.5f,.next_state=n};
        cls_training_add_experience(&tr, &e);
    }
    ASSERT(CLS_IS_OK(cls_training_step(&tr)), "step failed");

    TEST("training_epsilon_decay");
    float old_eps = tr.epsilon;
    cls_training_step(&tr);
    ASSERT(tr.epsilon < old_eps, "epsilon should decay");

    TEST("training_snapshot");
    ASSERT(CLS_IS_OK(cls_training_save_snapshot(&tr)), "snapshot failed");

    TEST("training_select_action");
    cls_decision_t acts[] = {{.confidence=0.2f},{.confidence=0.9f}};
    tr.epsilon = 0.0f; /* Force exploit */
    uint32_t sel = cls_training_select_action(&tr, acts, 2);
    ASSERT(sel == 1, "should pick highest confidence");

    TEST("training_metrics");
    cls_train_metrics_t m; cls_training_get_metrics(&tr, &m);
    ASSERT(m.total_updates > 0, "should have updates");

    cls_training_destroy(&tr);
    cls_cognitive_destroy(&cog);
}

static bool resource_rec_fn(void *ctx) { (void)ctx; return true; }

static void test_resource(void) {
    printf("\n  \033[33m── RESOURCE MANAGEMENT ──\033[0m\n");
    cls_resource_mgr_t res;
    cls_resource_limits_t lim = {.cpu_warn_threshold=0.7f,.cpu_critical_threshold=0.9f,
        .mem_warn_threshold=0.8f,.mem_critical_threshold=0.95f,.mem_max_bytes=512*1024*1024};

    TEST("resource_init");
    ASSERT(CLS_IS_OK(cls_resource_init(&res, &lim, 4)), "init failed");

    TEST("resource_update");
    ASSERT(CLS_IS_OK(cls_resource_update(&res)), "update failed");

    TEST("resource_health");
    ASSERT(cls_resource_health(&res) <= CLS_HEALTH_WARN, "health should be OK or WARN");

    TEST("resource_snapshot");
    cls_resource_snap_t sn;
    ASSERT(CLS_IS_OK(cls_resource_snapshot(&res, &sn)), "snapshot failed");

    TEST("resource_can_alloc");
    ASSERT(cls_resource_can_alloc(&res, 1024) == true, "should be able to alloc 1KB");

    TEST("resource_avg_cpu");
    cls_resource_update(&res);
    float avg = cls_resource_avg_cpu(&res, 2);
    ASSERT(avg >= 0.0f && avg <= 1.0f, "avg cpu out of range");

    TEST("resource_add_recovery");
    cls_recovery_action_t ra = {.action_id=1,.trigger_status=CLS_HEALTH_WARN,.recovery_fn=resource_rec_fn};
    ASSERT(CLS_IS_OK(cls_resource_add_recovery(&res, &ra)), "add recovery failed");

    cls_resource_destroy(&res);
}

static void test_agent_integration(void) {
    printf("\n  \033[33m── AGENT INTEGRATION ──\033[0m\n");
    cls_agent_t agent;
    cls_config_t cfg = CLS_CONFIG_DEFAULT;
    cfg.agent_id = 99; cfg.agent_name = "test-agent";
    cfg.memory_size = 1024*64; cfg.max_sensors = 2;

    TEST("agent_init");
    ASSERT(CLS_IS_OK(cls_agent_init(&agent, &cfg)), "init failed");

    TEST("agent_state_ready");
    ASSERT(agent.state == CLS_STATE_READY, "should be READY");

    TEST("agent_step");
    ASSERT(CLS_IS_OK(cls_agent_step(&agent)), "step failed");

    TEST("agent_multi_step");
    for (int i=0;i<10;i++) cls_agent_step(&agent);
    uint64_t cyc, up;
    cls_agent_stats(&agent, &cyc, &up);
    ASSERT(cyc == 11, "should be 11 cycles");

    TEST("agent_shutdown");
    ASSERT(CLS_IS_OK(cls_agent_shutdown(&agent)), "shutdown failed");

    TEST("agent_state_shutdown");
    ASSERT(agent.state == CLS_STATE_INIT, "should be INIT after shutdown");

    cls_agent_destroy(&agent);
}

/* ============================================================ */

int main(void) {
    printf("\n  \033[32m╔══════════════════════════════════════════════════╗\033[0m\n");
    printf("  \033[32m║     CLAWLOBSTARS TEST SUITE v0.1.0-dev           ║\033[0m\n");
    printf("  \033[32m╚══════════════════════════════════════════════════╝\033[0m\n");

    test_memory();
    test_perception();
    test_cognitive();
    test_planning();
    test_action();
    test_knowledge();
    test_comm();
    test_multiagent();
    test_security();
    test_training();
    test_resource();
    test_agent_integration();

    printf("\n  \033[32m══════════════════════════════════════════════════\033[0m\n");
    printf("  RESULTS: %d tests | \033[32m%d passed\033[0m", tests_run, tests_passed);
    if (tests_failed > 0) printf(" | \033[31m%d failed\033[0m", tests_failed);
    printf("\n  \033[32m══════════════════════════════════════════════════\033[0m\n\n");

    return tests_failed > 0 ? 1 : 0;
}
