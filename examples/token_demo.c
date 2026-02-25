/*
 * ClawLobstars — $CLAW Token Integration Demo
 * Staking, governance, revenue, licensing, vesting
 */

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/include/cls_framework.h"
#include "../src/include/cls_token.h"

static void print_tier(cls_staking_tier_t t) {
    const char *names[] = {"NONE","SCOUT","OPERATIVE","COMMANDER","ADMIRAL"};
    printf("%s", names[t]);
}

int main(void) {
    printf("\n  \033[33m╔══════════════════════════════════════════════════╗\033[0m\n");
    printf("  \033[33m║     $CLAW TOKEN INTEGRATION — FULL DEMO         ║\033[0m\n");
    printf("  \033[33m╚══════════════════════════════════════════════════╝\033[0m\n\n");

    /* ======== 1. INIT ======== */
    printf("  \033[33m[1/7]\033[0m TOKEN ENGINE INIT\n");
    cls_token_engine_t engine;
    cls_token_init(&engine, 64, 16);
    cls_token_set_price(&engine, 0.042);

    cls_comm_bus_t bus; cls_comm_init(&bus, 1);
    cls_token_set_comm(&engine, &bus);

    cls_token_supply_t sup;
    cls_token_get_supply(&engine, &sup);
    printf("    ✓ Total supply:  %llu CLAW (1B)\n", (unsigned long long)(sup.total_supply / 1000000000ULL));
    printf("    ✓ Circulating:   %llu CLAW\n", (unsigned long long)(sup.circulating / 1000000000ULL));
    printf("    ✓ Treasury:      %llu CLAW\n", (unsigned long long)(sup.treasury / 1000000000ULL));
    printf("    ✓ Vesting lock:  %llu CLAW\n", (unsigned long long)(sup.locked_vesting / 1000000000ULL));
    printf("    ✓ Price: $%.4f | MCap: $%.0f\n\n", sup.price_usd, sup.market_cap);

    /* ======== 2. STAKING ======== */
    printf("  \033[33m[2/7]\033[0m STAKING\n");

    /* Simulate 4 wallets staking at different tiers */
    uint8_t w1[32] = {0x01}; uint32_t s1;
    uint8_t w2[32] = {0x02}; uint32_t s2;
    uint8_t w3[32] = {0x03}; uint32_t s3;
    uint8_t w4[32] = {0x04}; uint32_t s4;

    cls_token_stake(&engine, w1, CLS_TIER_SCOUT, &s1);
    cls_token_stake(&engine, w2, CLS_TIER_OPERATIVE, &s2);
    cls_token_stake(&engine, w3, CLS_TIER_COMMANDER, &s3);
    cls_token_stake(&engine, w4, CLS_TIER_ADMIRAL, &s4);

    printf("    %-12s %-12s %-10s %-8s %s\n", "STAKER", "AMOUNT", "TIER", "APY", "SLOTS");
    printf("    %-12s %-12s %-10s %-8s %s\n", "──────", "──────", "────", "───", "─────");

    for (int i = 1; i <= 4; i++) {
        cls_staker_t *st = cls_token_get_staker(&engine, (uint32_t)i);
        if (!st) continue;
        float apy = cls_token_calculate_apy(&engine, st->staker_id);
        printf("    Staker #%-4u %10llu  ", st->staker_id,
            (unsigned long long)(st->amount_staked / 1000000000ULL));
        print_tier(st->tier);
        printf("%*s %.0f%%     %u\n", (int)(11 - st->tier * 2), "", apy * 100.0f, st->agent_slots);
    }

    printf("    ✓ Total staked: %llu CLAW | %u stakers\n\n",
        (unsigned long long)(engine.staking.total_staked / 1000000000ULL),
        engine.staking.staker_count);

    /* ======== 3. AGENT LICENSING ======== */
    printf("  \033[33m[3/7]\033[0m AGENT LICENSING\n");

    uint32_t lic1, lic2, lic3;
    cls_token_issue_license(&engine, s1, 101, 86400000000ULL, &lic1); /* Scout */
    cls_token_issue_license(&engine, s3, 301, 86400000000ULL, &lic2); /* Commander */
    cls_token_issue_license(&engine, s4, 401, 86400000000ULL, &lic3); /* Admiral */

    for (uint32_t id = lic1; id <= lic3; id++) {
        bool valid; cls_agent_license_t lic;
        cls_token_check_license(&engine, 100 + id * 100 + 1, &valid, &lic);
        if (!valid) { cls_token_check_license(&engine, 101, &valid, &lic); }
        /* Just show what we have */
    }

    printf("    License #%u: Agent 101 | ", lic1);
    print_tier(engine.licenses[0].tier_required);
    printf(" | Fee: %llu/epoch\n", (unsigned long long)(engine.licenses[0].fee_per_epoch / 1000000000ULL));
    printf("      Modules: cognitive=%s planning=%s defi=%s solana=%s\n",
        engine.licenses[0].access_cognitive?"✓":"✗",
        engine.licenses[0].access_planning?"✓":"✗",
        engine.licenses[0].access_defi?"✓":"✗",
        engine.licenses[0].access_solana?"✓":"✗");

    printf("    License #%u: Agent 301 | ", lic2);
    print_tier(engine.licenses[1].tier_required);
    printf(" | Fee: %llu/epoch\n", (unsigned long long)(engine.licenses[1].fee_per_epoch / 1000000000ULL));
    printf("      Modules: cognitive=%s planning=%s defi=%s solana=%s\n",
        engine.licenses[1].access_cognitive?"✓":"✗",
        engine.licenses[1].access_planning?"✓":"✗",
        engine.licenses[1].access_defi?"✓":"✗",
        engine.licenses[1].access_solana?"✓":"✗");

    printf("    License #%u: Agent 401 | ", lic3);
    print_tier(engine.licenses[2].tier_required);
    printf(" | Fee: %llu/epoch (FREE)\n", (unsigned long long)(engine.licenses[2].fee_per_epoch / 1000000000ULL));
    printf("      Modules: ALL UNLOCKED ✓\n\n");

    /* ======== 4. GOVERNANCE ======== */
    printf("  \033[33m[4/7]\033[0m GOVERNANCE\n");

    uint32_t prop_id;
    cls_status_t s = cls_token_propose(&engine, s4, CLS_PROP_FEE_ADJUST,
        "CIP-001: Reduce agent licensing fee by 25%",
        "Proposal to reduce base licensing fee from 100 CLAW to 75 CLAW per epoch",
        3600000000ULL, &prop_id);
    printf("    ✓ Proposal #%u: '%s'\n", prop_id, engine.proposals[0].title);
    printf("    ✓ Status: ACTIVE | Quorum: %llu CLAW\n",
        (unsigned long long)(engine.proposals[0].quorum / 1000000000ULL));

    /* Vote */
    cls_token_vote(&engine, prop_id, s2, true);   /* Operative: YES */
    cls_token_vote(&engine, prop_id, s3, true);   /* Commander: YES */
    cls_token_vote(&engine, prop_id, s4, true);   /* Admiral: YES */
    cls_token_vote(&engine, prop_id, s1, false);   /* Scout: NO */

    printf("    ✓ Votes: FOR=%llu | AGAINST=%llu | Voters=%u\n",
        (unsigned long long)(engine.proposals[0].votes_for / 1000000000ULL),
        (unsigned long long)(engine.proposals[0].votes_against / 1000000000ULL),
        engine.proposals[0].vote_count);

    cls_token_finalize_proposal(&engine, prop_id);
    printf("    ✓ Result: %s\n",
        engine.proposals[0].status == CLS_PROP_PASSED ? "PASSED ✓" : "REJECTED ✗");

    cls_token_execute_proposal(&engine, prop_id);
    printf("    ✓ Executed: %s\n\n",
        engine.proposals[0].status == CLS_PROP_EXECUTED ? "YES" : "NO");

    /* ======== 5. REVENUE ======== */
    printf("  \033[33m[5/7]\033[0m REVENUE DISTRIBUTION\n");

    /* Simulate agent operations generating revenue */
    cls_token_record_revenue(&engine, CLS_REV_AGENT_FEE, 500000000000ULL, 101);
    cls_token_record_revenue(&engine, CLS_REV_SWAP_FEE, 200000000000ULL, 301);
    cls_token_record_revenue(&engine, CLS_REV_TX_FEE, 100000000000ULL, 401);
    cls_token_record_revenue(&engine, CLS_REV_INFERENCE_FEE, 150000000000ULL, 301);

    uint64_t total_rev, dist_rev, pend_rev;
    cls_token_get_revenue_stats(&engine, &total_rev, &dist_rev, &pend_rev);
    printf("    ✓ Revenue collected: %llu CLAW\n",
        (unsigned long long)(total_rev / 1000000000ULL));
    printf("      Agent 101 (fee): 500 CLAW\n");
    printf("      Agent 301 (swap+inference): 350 CLAW\n");
    printf("      Agent 401 (tx fee): 100 CLAW\n");

    /* Distribute */
    cls_token_distribute_revenue(&engine);
    cls_token_get_revenue_stats(&engine, &total_rev, &dist_rev, &pend_rev);
    printf("    ✓ Distributed: %llu CLAW (70%% stakers / 20%% treasury / 10%% burned)\n",
        (unsigned long long)(dist_rev / 1000000000ULL));

    /* Check staker rewards */
    printf("    ✓ Staker rewards earned:\n");
    for (int i = 1; i <= 4; i++) {
        cls_staker_t *st = cls_token_get_staker(&engine, (uint32_t)i);
        if (!st) continue;
        printf("      #%u (", st->staker_id);
        print_tier(st->tier);
        printf("): %llu CLAW\n", (unsigned long long)(st->rewards_earned / 1000000000ULL));
    }

    /* Claim */
    uint64_t claimed;
    cls_token_claim_rewards(&engine, s4, &claimed);
    printf("    ✓ Admiral claimed: %llu CLAW\n\n",
        (unsigned long long)(claimed / 1000000000ULL));

    /* ======== 6. VESTING ======== */
    printf("  \033[33m[6/7]\033[0m VESTING SCHEDULES\n");

    uint8_t team_wallet[32] = {0xAA};
    uint32_t vest_id;
    uint64_t team_alloc = CLS_TOKEN_TOTAL_SUPPLY * CLS_ALLOC_TEAM / 10000;
    cls_token_create_vesting(&engine, team_wallet, team_alloc,
                              CLS_VEST_LINEAR, 0, 86400000000ULL * 365, &vest_id);

    printf("    ✓ Vesting #%u: Team allocation\n", vest_id);
    printf("      Total: %llu CLAW | Type: LINEAR\n",
        (unsigned long long)(team_alloc / 1000000000ULL));
    printf("      Duration: 365 days | Cliff: none\n");

    /* Release what's available */
    uint64_t released;
    cls_token_release_vested(&engine, vest_id, &released);
    uint64_t v_total, v_released, v_avail;
    cls_token_get_vesting_info(&engine, vest_id, &v_total, &v_released, &v_avail);
    printf("    ✓ Released: %llu CLAW | Remaining: %llu CLAW\n\n",
        (unsigned long long)(v_released / 1000000000ULL),
        (unsigned long long)(v_avail / 1000000000ULL));

    /* ======== 7. BURN & FINAL STATE ======== */
    printf("  \033[33m[7/7]\033[0m TOKEN BURNS & FINAL STATE\n");

    /* Manual burn */
    cls_token_burn(&engine, 5000000000000ULL); /* Burn 5000 CLAW */
    printf("    ✓ Manual burn: 5,000 CLAW\n");

    /* Early unstake (with penalty) */
    uint64_t unstake_amount, unstake_penalty;
    cls_token_unstake(&engine, s1, &unstake_amount, &unstake_penalty);
    printf("    ✓ Scout unstaked early: returned %llu, penalty %llu CLAW (burned)\n",
        (unsigned long long)(unstake_amount / 1000000000ULL),
        (unsigned long long)(unstake_penalty / 1000000000ULL));

    /* Final supply state */
    cls_token_get_supply(&engine, &sup);
    printf("\n    ╔═══════════════════════════════╗\n");
    printf("    ║     $CLAW TOKEN STATE         ║\n");
    printf("    ╠═══════════════════════════════╣\n");
    printf("    ║  Supply:      %12llu  ║\n", (unsigned long long)(sup.total_supply / 1000000000ULL));
    printf("    ║  Circulating: %12llu  ║\n", (unsigned long long)(sup.circulating / 1000000000ULL));
    printf("    ║  Staked:      %12llu  ║\n", (unsigned long long)(sup.staked / 1000000000ULL));
    printf("    ║  Treasury:    %12llu  ║\n", (unsigned long long)(sup.treasury / 1000000000ULL));
    printf("    ║  Burned:      %12llu  ║\n", (unsigned long long)(sup.burned / 1000000000ULL));
    printf("    ║  Locked:      %12llu  ║\n", (unsigned long long)(sup.locked_vesting / 1000000000ULL));
    printf("    ║  Price:       $%.4f       ║\n", sup.price_usd);
    printf("    ╚═══════════════════════════════╝\n");

    /* ======== SUMMARY ======== */
    printf("\n  \033[33m╔══════════════════════════════════════════════════╗\033[0m\n");
    printf("  \033[33m║       $CLAW TOKEN INTEGRATION COMPLETE           ║\033[0m\n");
    printf("  \033[33m╠══════════════════════════════════════════════════╣\033[0m\n");
    printf("  \033[33m║\033[0m  ✓ Tokenomics: 1B supply, 6 allocations         \033[33m║\033[0m\n");
    printf("  \033[33m║\033[0m  ✓ Staking: 4 tiers, APY bonuses, epochs        \033[33m║\033[0m\n");
    printf("  \033[33m║\033[0m  ✓ Governance: propose → vote → execute          \033[33m║\033[0m\n");
    printf("  \033[33m║\033[0m  ✓ Revenue: collect → 70/20/10 split             \033[33m║\033[0m\n");
    printf("  \033[33m║\033[0m  ✓ Licensing: tier-gated module access           \033[33m║\033[0m\n");
    printf("  \033[33m║\033[0m  ✓ Vesting: linear/cliff/stepped schedules      \033[33m║\033[0m\n");
    printf("  \033[33m║\033[0m  ✓ Burns: manual + penalty + revenue             \033[33m║\033[0m\n");
    printf("  \033[33m╚══════════════════════════════════════════════════╝\033[0m\n\n");

    cls_token_destroy(&engine);
    cls_comm_destroy(&bus);
    printf("  ✓ Token engine destroyed. All clean.\n\n");
    return 0;
}
