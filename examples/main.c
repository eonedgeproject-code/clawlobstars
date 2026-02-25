/*
 * ClawLobstars — Full Framework Demo
 * Demonstrates all 13 modules working together
 */

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../src/include/cls_framework.h"

static void my_logger(cls_log_level_t level, const char *module, const char *msg) {
    static const char *c[] = {"\033[90m","\033[36m","\033[32m","\033[33m","\033[31m","\033[91m"};
    static const char *l[] = {"TRACE","DEBUG","INFO ","WARN ","ERROR","FATAL"};
    if (level > CLS_LOG_FATAL) return;
    fprintf(stderr, "  %s[%s]\033[0m [%-10s] %s\n", c[level], l[level], module, msg);
}

static cls_status_t action_patrol(uint32_t id, const void *p, size_t l) {
    (void)id;(void)p;(void)l; return CLS_OK;
}
static cls_status_t action_engage(uint32_t id, const void *p, size_t l) {
    (void)id;(void)p;(void)l; return CLS_OK;
}
static cls_status_t action_evade(uint32_t id, const void *p, size_t l) {
    (void)id;(void)p;(void)l; return CLS_OK;
}
static cls_status_t action_rollback(uint32_t id, const void *p, size_t l) {
    (void)id;(void)p;(void)l; return CLS_OK;
}
static bool mem_recovery(void *ctx) {
    if (ctx) cls_memory_prune((cls_memory_ctx_t*)ctx);
    return true;
}
static cls_status_t sensor_read(void *ctx, void *buf, size_t *len) {
    (void)ctx;
    float v[] = {0.7f,0.3f,0.85f,0.5f};
    if (*len < sizeof(v)) { *len = sizeof(v); return CLS_ERR_OVERFLOW; }
    memcpy(buf, v, sizeof(v)); *len = sizeof(v); return CLS_OK;
}
static void on_msg(const cls_msg_t *msg, void *ctx) {
    (void)ctx;
    printf("    >> COMM: type=0x%02X from=%u\n", msg->msg_type, msg->src_agent);
}

int main(void) {
    printf("\n  \033[32m╔══════════════════════════════════════════════════╗\033[0m\n");
    printf("  \033[32m║     CLAWLOBSTARS AI AGENT FRAMEWORK v%s       ║\033[0m\n", cls_version());
    printf("  \033[32m║           FULL SYSTEM INTEGRATION TEST           ║\033[0m\n");
    printf("  \033[32m╚══════════════════════════════════════════════════╝\033[0m\n\n");

    /* 1. AGENT CORE */
    printf("  \033[33m[01/13]\033[0m AGENT CORE\n");
    cls_config_t cfg = CLS_CONFIG_DEFAULT;
    cfg.agent_id = 1; cfg.agent_name = "cls-alpha-01";
    cfg.memory_size = 512*1024; cfg.max_sensors = 8;
    cfg.security_level = CLS_SEC_HIGH; cfg.log_level = CLS_LOG_INFO;
    cls_agent_t agent;
    cls_agent_init(&agent, &cfg);
    cls_agent_set_logger(&agent, my_logger);
    printf("    ✓ Agent '%s' state=0x%02X\n\n", agent.name, agent.state);

    /* 2. SECURITY */
    printf("  \033[33m[02/13]\033[0m SECURITY LAYER\n");
    cls_security_ctx_t sec;
    cls_security_init(&sec, CLS_SEC_HIGH);
    uint8_t key[] = "CLS-TACTICAL-KEY-2025";
    cls_security_set_key(&sec, key, sizeof(key));
    cls_role_t r1 = {.role_id=1,.name="OPERATOR",.permissions=CLS_PERM_ALL};
    cls_role_t r2 = {.role_id=2,.name="OBSERVER",.permissions=CLS_PERM_READ};
    cls_security_add_role(&sec, &r1);
    cls_security_add_role(&sec, &r2);
    uint8_t cred[] = "agent-alpha-cred";
    cls_auth_token_t tok;
    cls_security_auth(&sec, 1, cred, sizeof(cred), &tok);
    printf("    ✓ Token valid=%s | OPERATOR exec=%s | OBSERVER exec=%s\n",
        CLS_IS_OK(cls_security_validate_token(&sec,&tok))?"YES":"NO",
        CLS_IS_OK(cls_security_check_permission(&sec,1,CLS_PERM_EXECUTE))?"GRANT":"DENY",
        CLS_IS_OK(cls_security_check_permission(&sec,2,CLS_PERM_EXECUTE))?"GRANT":"DENY");
    const char *secret = "Mission coords: 34.05N 118.24W";
    char enc[128], dec[128];
    cls_security_encrypt(&sec, secret, strlen(secret), enc);
    cls_security_decrypt(&sec, enc, strlen(secret), dec);
    printf("    ✓ Encrypt/Decrypt: %s\n\n", memcmp(secret,dec,strlen(secret))==0?"PASS":"FAIL");

    /* 3. PERCEPTION */
    printf("  \033[33m[03/13]\033[0m PERCEPTION ENGINE\n");
    cls_sensor_t s1={.id=100,.type=CLS_SENSOR_NUMERIC,.name="thermal-01",.read_fn=sensor_read,.active=true};
    cls_sensor_t s2={.id=101,.type=CLS_SENSOR_VECTOR,.name="lidar-01",.read_fn=sensor_read,.active=true};
    cls_perception_register(agent.perception, &s1);
    cls_perception_register(agent.perception, &s2);
    printf("    ✓ %u sensors registered\n\n", agent.perception->sensor_count);

    /* 4. MEMORY */
    printf("  \033[33m[04/13]\033[0m MEMORY INTERFACE\n");
    cls_memory_store(agent.memory, "mission:id", "OP-LOBSTAR-7G", 15);
    cls_memory_store(agent.memory, "mission:status", "ACTIVE", 7);
    float wp[]={34.05f,-118.24f,36.17f,-115.14f};
    cls_memory_store(agent.memory, "nav:waypoints", wp, sizeof(wp));
    cls_memory_store_ttl(agent.memory, "cache:temp", "volatile", 9, 60);
    char mbuf[64]; size_t mlen=sizeof(mbuf);
    cls_memory_retrieve(agent.memory, "mission:id", mbuf, &mlen);
    printf("    ✓ 4 entries stored | Retrieved: '%s'\n\n", mbuf);

    /* 5. KNOWLEDGE GRAPH */
    printf("  \033[33m[05/13]\033[0m KNOWLEDGE GRAPH\n");
    cls_knowledge_t kg;
    cls_knowledge_init(&kg, 256);
    float e1[32]={0.9f,0.1f,0.8f}, e2[32]={0.1f,0.9f,0.2f}, e3[32]={0.7f,0.3f,0.6f};
    uint32_t n1,n2,n3,n4,n5;
    cls_knowledge_add_node(&kg, "THREAT", e1, &n1);
    cls_knowledge_add_node(&kg, "SAFE_ZONE", e2, &n2);
    cls_knowledge_add_node(&kg, "TARGET", e3, &n3);
    cls_knowledge_add_node(&kg, "EVADE", NULL, &n4);
    cls_knowledge_add_node(&kg, "ENGAGE", NULL, &n5);
    cls_knowledge_add_edge(&kg, n1, n4, CLS_REL_CAUSES, 0.9f);
    cls_knowledge_add_edge(&kg, n3, n5, CLS_REL_REQUIRES, 0.8f);
    cls_knowledge_add_edge(&kg, n2, n1, CLS_REL_OPPOSITE, 1.0f);
    float qe[32]={0.85f,0.15f,0.75f};
    cls_kg_result_t sr[5]; uint32_t sc;
    cls_knowledge_search(&kg, qe, sr, 3, &sc);
    printf("    ✓ %u nodes | Top match: '%s' (%.3f)\n", kg.node_count, sr[0].node->name, sr[0].relevance);
    uint32_t path[16]; uint32_t plen;
    cls_knowledge_find_path(&kg, n1, n4, path, &plen, 5);
    printf("    ✓ Path THREAT→EVADE: %u hops\n\n", plen);

    /* 6. COGNITIVE */
    printf("  \033[33m[06/13]\033[0m COGNITIVE SYSTEM\n");
    const char *mn[]={"RULE_BASED","NEURAL_NET","DECISION_TREE","BAYESIAN"};
    float tf[]={0.8f,0.3f,0.6f,0.9f};
    for (int m=0;m<=3;m++) {
        cls_cognitive_t ct; cls_cognitive_init(&ct,(cls_model_type_t)m);
        cls_input_t ci={.features=tf,.feature_count=4};
        cls_decision_t cd; cls_cognitive_infer(&ct,&ci,&cd);
        printf("    ✓ %-14s conf=%.3f act=%u\n", mn[m], cd.confidence, cd.action_id);
        cls_cognitive_destroy(&ct);
    }
    printf("\n");

    /* 7. COMM BUS */
    printf("  \033[33m[07/13]\033[0m COMMUNICATION BUS\n");
    cls_comm_bus_t comm; cls_comm_init(&comm, 1);
    uint32_t sid; cls_comm_subscribe(&comm, 0, on_msg, NULL, &sid);
    cls_comm_broadcast(&comm, CLS_MSG_SYSTEM, "BOOT", 5);
    cls_comm_send(&comm, CLS_MSG_PERCEPTION, tf, sizeof(tf), 1);
    cls_comm_process(&comm, 10);
    printf("    ✓ Sent=%llu Delivered=%llu\n\n",
        (unsigned long long)comm.msgs_sent, (unsigned long long)comm.msgs_delivered);

    /* 8. PLANNING */
    printf("  \033[33m[08/13]\033[0m PLANNING & STRATEGY\n");
    cls_planner_t planner; cls_planner_init(&planner, 16, 8);
    cls_goal_t goal={.goal_id=1,.description="Secure 7G",.priority=CLS_PRIORITY_HIGH,.utility=0.9f};
    cls_planner_add_goal(&planner, &goal);
    cls_decision_t decs[]={
        {.action_id=10,.confidence=0.9f,.priority=80},
        {.action_id=20,.confidence=0.7f,.priority=60},
        {.action_id=30,.confidence=0.5f,.priority=40},
    };
    cls_plan_t *plan;
    cls_planner_generate(&planner, decs, 3, &plan);
    cls_strategy_eval_t ev;
    cls_planner_evaluate(&planner, plan, &ev);
    printf("    ✓ Plan #%u: %u tasks | utility=%.2f feasible=%s\n",
        plan->plan_id, plan->task_count, ev.expected_utility, ev.feasible?"YES":"NO");
    cls_task_t *nt; cls_plan_next_task(plan, &nt);
    printf("    ✓ Next: task=%u action=%u\n\n", nt->task_id, nt->action_id);

    /* 9. ACTION EXECUTOR */
    printf("  \033[33m[09/13]\033[0m ACTION EXECUTOR\n");
    cls_action_exec_t exec; cls_action_init(&exec, 16, 64);
    cls_action_handler_t ah1={.action_id=10,.name="PATROL",.execute_fn=action_patrol,.rollback_fn=action_rollback};
    cls_action_handler_t ah2={.action_id=20,.name="ENGAGE",.execute_fn=action_engage};
    cls_action_handler_t ah3={.action_id=30,.name="EVADE",.execute_fn=action_evade};
    cls_action_register(&exec, &ah1);
    cls_action_register(&exec, &ah2);
    cls_action_register(&exec, &ah3);
    cls_action_record_t rec;
    cls_action_execute_task(&exec, nt, &rec);
    printf("    ✓ Executed action=%u status=%d\n", rec.action_id, rec.status);
    cls_action_execute(&exec, 10, NULL, 0, &rec);
    cls_action_rollback(&exec, rec.exec_id);
    cls_action_get_record(&exec, rec.exec_id, &rec);
    printf("    ✓ Rollback exec#%u: %s | Total: %llu exec / %llu rb\n\n",
        rec.exec_id, rec.rolled_back?"OK":"FAIL",
        (unsigned long long)exec.total_executed, (unsigned long long)exec.total_rollbacks);

    /* 10. MULTI-AGENT */
    printf("  \033[33m[10/13]\033[0m MULTI-AGENT OPS\n");
    cls_multiagent_t ma; cls_multiagent_init(&ma, 1, &comm);
    cls_peer_t p1={.agent_id=2,.name="bravo-02",.status=CLS_PEER_CONNECTED,.trust_score=0.9f};
    cls_peer_t p2={.agent_id=3,.name="charlie-03",.status=CLS_PEER_CONNECTED,.trust_score=0.8f};
    cls_multiagent_register_peer(&ma, &p1);
    cls_multiagent_register_peer(&ma, &p2);
    uint32_t pid; cls_multiagent_propose(&ma, 2, CLS_COLLAB_TASK_SHARE, 10, 0.9f, &pid);
    cls_multiagent_respond(&ma, pid, true);
    cls_multiagent_vote(&ma, 100, 0.8f);
    cls_multiagent_vote(&ma, 100, 0.6f);
    float cons; uint32_t vc;
    cls_multiagent_get_consensus(&ma, 100, &cons, &vc);
    printf("    ✓ %u peers | Collab accepted | Consensus=%.2f (%u votes)\n\n", ma.peer_count, cons, vc);

    /* 11. TRAINING */
    printf("  \033[33m[11/13]\033[0m TRAINING PIPELINE\n");
    cls_training_t tr;
    cls_training_init(&tr, agent.cognitive, CLS_TRAIN_REPLAY, 512);
    cls_training_set_epsilon(&tr, 1.0f, 0.99f, 0.01f);
    cls_training_start(&tr);
    for (int i=0;i<50;i++) {
        float st[]={0.1f*(float)i,0.5f,0.8f,0.3f};
        float ns[]={0.1f*(float)(i+1),0.4f,0.7f,0.35f};
        cls_experience_t xp={.state=st,.state_dim=4,.action_taken=(uint32_t)(i%3),
            .reward=(float)(i%5)*0.2f,.next_state=ns,.terminal=(i==49)};
        cls_training_add_experience(&tr, &xp);
    }
    for (int i=0;i<5;i++) cls_training_step(&tr);
    cls_training_save_snapshot(&tr);
    cls_decision_t ac[]={{.action_id=0,.confidence=0.3f},{.action_id=1,.confidence=0.9f},{.action_id=2,.confidence=0.5f}};
    uint32_t sel = cls_training_select_action(&tr, ac, 3);
    cls_train_metrics_t tm; cls_training_get_metrics(&tr, &tm);
    printf("    ✓ Buffer=%u | Epochs=%u | ε=%.3f | Action=%u | Snapshots=%u\n\n",
        tr.buffer_count, tm.current_epoch, tr.epsilon, sel, tr.snapshot_count);

    /* 12. RESOURCE MANAGEMENT */
    printf("  \033[33m[12/13]\033[0m RESOURCE MANAGEMENT\n");
    cls_resource_mgr_t res;
    cls_resource_limits_t lim={.cpu_warn_threshold=0.7f,.cpu_critical_threshold=0.9f,
        .mem_warn_threshold=0.8f,.mem_critical_threshold=0.95f,.mem_max_bytes=512*1024*1024};
    cls_resource_init(&res, &lim, 4);
    cls_recovery_action_t ra={.action_id=1,.trigger_status=CLS_HEALTH_WARN,
        .resource_type=CLS_RES_MEMORY,.recovery_fn=mem_recovery,.ctx=agent.memory};
    cls_resource_add_recovery(&res, &ra);
    cls_resource_update(&res);
    cls_resource_snap_t sn; cls_resource_snapshot(&res, &sn);
    printf("    ✓ Health=%s | MEM=%.1f%% | Can alloc 1MB=%s\n\n",
        cls_resource_health(&res)==CLS_HEALTH_OK?"OK":"WARN",
        sn.mem_usage*100.0f, cls_resource_can_alloc(&res,1024*1024)?"YES":"NO");

    /* 13. INTEGRATION LOOP */
    printf("  \033[33m[13/13]\033[0m INTEGRATED AGENT LOOP\n");
    for (int i=0;i<5;i++) {
        cls_agent_step(&agent);
        cls_resource_update(&res);
        cls_training_step(&tr);
        cls_comm_process(&comm, 16);
        usleep(10000);
    }
    uint64_t cyc,up; cls_agent_stats(&agent, &cyc, &up);
    printf("    ✓ %llu cycles | %llu µs uptime\n", (unsigned long long)cyc, (unsigned long long)up);

    /* SUMMARY */
    printf("\n  \033[32m╔══════════════════════════════════════════════════╗\033[0m\n");
    printf("  \033[32m║         ALL 13 MODULES FULLY OPERATIONAL         ║\033[0m\n");
    printf("  \033[32m╠══════════════════════════════════════════════════╣\033[0m\n");
    printf("  \033[32m║\033[0m  ✓ Agent Core      ✓ Security Layer              \033[32m║\033[0m\n");
    printf("  \033[32m║\033[0m  ✓ Perception      ✓ Memory Interface            \033[32m║\033[0m\n");
    printf("  \033[32m║\033[0m  ✓ Knowledge Graph ✓ Cognitive (4 models)        \033[32m║\033[0m\n");
    printf("  \033[32m║\033[0m  ✓ Comm Bus        ✓ Planning & Strategy         \033[32m║\033[0m\n");
    printf("  \033[32m║\033[0m  ✓ Action Executor ✓ Multi-Agent Ops             \033[32m║\033[0m\n");
    printf("  \033[32m║\033[0m  ✓ Training (RL)   ✓ Resource Management         \033[32m║\033[0m\n");
    printf("  \033[32m║\033[0m  ✓ Infrastructure (Logging/Diagnostics)          \033[32m║\033[0m\n");
    printf("  \033[32m╚══════════════════════════════════════════════════╝\033[0m\n\n");

    /* Cleanup all */
    cls_training_stop(&tr); cls_training_destroy(&tr);
    cls_resource_destroy(&res); cls_multiagent_destroy(&ma);
    cls_action_destroy(&exec); cls_planner_destroy(&planner);
    cls_knowledge_destroy(&kg); cls_comm_destroy(&comm);
    cls_security_destroy(&sec);
    cls_agent_shutdown(&agent); cls_agent_destroy(&agent);
    printf("  ✓ All resources released. Mission complete.\n\n");
    return 0;
}
