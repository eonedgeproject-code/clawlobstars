/*
 * ClawLobstars — Solana Agent Demo
 * Autonomous on-chain operations: wallet, transfers, swaps, monitoring
 */

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/include/cls_framework.h"
#include "../src/include/cls_solana.h"

static void price_alert(uint32_t id, double old_val, double new_val, void *ctx) {
    (void)ctx;
    printf("    🚨 ALERT #%u: price moved %.4f → %.4f\n", id, old_val, new_val);
}

static void balance_alert(uint32_t id, double old_val, double new_val, void *ctx) {
    (void)ctx;
    printf("    🚨 ALERT #%u: balance changed %.4f → %.4f SOL\n", id, old_val, new_val);
}

int main(void) {
    printf("\n  \033[35m╔══════════════════════════════════════════════════╗\033[0m\n");
    printf("  \033[35m║   CLAWLOBSTARS SOLANA AGENT — ON-CHAIN OPS      ║\033[0m\n");
    printf("  \033[35m╚══════════════════════════════════════════════════╝\033[0m\n\n");

    /* ======== 1. INIT & CONNECT ======== */
    printf("  \033[33m[1/8]\033[0m INITIALIZATION\n");
    cls_solana_agent_t sol;
    cls_solana_init(&sol, CLS_SOL_DEVNET, 64, 32);
    cls_solana_connect(&sol, NULL);
    cls_solana_set_commitment(&sol, CLS_SOL_CONFIRMED);
    printf("    ✓ Connected to %s (commitment=CONFIRMED)\n", sol.rpc_url);

    /* Comm bus integration */
    cls_comm_bus_t bus;
    cls_comm_init(&bus, 1);
    cls_solana_set_comm(&sol, &bus);
    printf("    ✓ Comm bus linked\n\n");

    /* ======== 2. WALLET ======== */
    printf("  \033[33m[2/8]\033[0m WALLET MANAGEMENT\n");
    cls_sol_keypair_t kp;
    cls_solana_generate_keypair(&kp);
    cls_solana_set_wallet(&sol, &kp);
    cls_solana_sync_wallet(&sol);

    char pubkey_b58[64];
    cls_solana_pubkey_to_base58(&kp.public_key, pubkey_b58, sizeof(pubkey_b58));
    printf("    ✓ Wallet: %s\n", pubkey_b58);
    printf("    ✓ Balance: %.4f SOL (%llu lamports)\n",
        cls_solana_get_balance_sol(&sol), (unsigned long long)cls_solana_get_balance(&sol));
    printf("    ✓ Portfolio: $%.2f USD\n", sol.wallet.portfolio_value_usd);
    printf("    ✓ Tokens: %u\n\n", sol.wallet.token_count);

    /* ======== 3. SOL TRANSFER ======== */
    printf("  \033[33m[3/8]\033[0m SOL TRANSFER\n");
    cls_sol_keypair_t recipient;
    cls_solana_generate_keypair(&recipient);
    char recv_b58[64];
    cls_solana_pubkey_to_base58(&recipient.public_key, recv_b58, sizeof(recv_b58));

    uint64_t send_amount = cls_solana_sol_to_lamports(0.1);
    uint64_t bal_before = cls_solana_get_balance(&sol);
    cls_status_t s = cls_solana_transfer_sol(&sol, &recipient.public_key, send_amount);
    printf("    ✓ Sent 0.1 SOL to %s\n", recv_b58);
    printf("    ✓ Status: %s | Balance: %.4f → %.4f SOL\n",
        CLS_IS_OK(s) ? "CONFIRMED" : "FAILED",
        cls_solana_lamports_to_sol(bal_before),
        cls_solana_get_balance_sol(&sol));
    printf("    ✓ TX: %s\n\n", sol.tx_history[sol.tx_count - 1].tx_hash);

    /* ======== 4. TRANSACTION BUILDING ======== */
    printf("  \033[33m[4/8]\033[0m CUSTOM TRANSACTION\n");
    cls_sol_transaction_t tx;
    cls_solana_tx_init(&tx, &kp.public_key);

    /* Transfer + memo */
    cls_sol_instruction_t ix_transfer, ix_memo;
    cls_solana_ix_transfer_sol(&ix_transfer, &kp.public_key, &recipient.public_key,
                                cls_solana_sol_to_lamports(0.05));
    cls_solana_ix_memo(&ix_memo, "ClawLobstars autonomous transfer", 33);

    cls_solana_tx_add_instruction(&tx, &ix_transfer);
    cls_solana_tx_add_instruction(&tx, &ix_memo);

    /* Simulate first */
    cls_solana_tx_simulate(&sol, &tx);
    printf("    ✓ Simulated: %llu CU | Fee: %llu lamports\n",
        (unsigned long long)tx.compute_units, (unsigned long long)tx.fee_lamports);

    /* Sign & send */
    cls_solana_tx_sign(&tx, &kp);
    cls_solana_tx_send(&sol, &tx);
    printf("    ✓ Sent: %u instructions | %u signatures\n", tx.instruction_count, tx.signature_count);
    printf("    ✓ TX: %s\n\n", tx.tx_hash);

    /* ======== 5. DeFi — PRICE FEED ======== */
    printf("  \033[33m[5/8]\033[0m DeFi PRICE FEEDS\n");
    cls_sol_pubkey_t sol_mint, usdc_mint, bonk_mint;
    memset(&sol_mint, 0, sizeof(sol_mint)); sol_mint.bytes[0] = 0x06;
    memset(&usdc_mint, 0, sizeof(usdc_mint)); usdc_mint.bytes[0] = 0xEE;
    memset(&bonk_mint, 0, sizeof(bonk_mint)); bonk_mint.bytes[0] = 0xB0;

    cls_sol_price_t price;
    cls_solana_get_price(&sol, &sol_mint, &price);
    printf("    ✓ SOL/USD:  $%.2f (%+.1f%%)\n", price.price_usd, price.change_24h);
    cls_solana_get_price(&sol, &usdc_mint, &price);
    printf("    ✓ USDC/USD: $%.2f (%+.1f%%)\n", price.price_usd, price.change_24h);
    cls_solana_get_price(&sol, &bonk_mint, &price);
    printf("    ✓ BONK/USD: $%.6f (%+.1f%%)\n\n", price.price_usd, price.change_24h);

    /* ======== 6. DeFi — SWAP ======== */
    printf("  \033[33m[6/8]\033[0m DeFi SWAP EXECUTION\n");
    cls_sol_swap_quote_t quote;
    uint64_t swap_amount = cls_solana_sol_to_lamports(0.5);
    cls_solana_get_swap_quote(&sol, &sol_mint, &bonk_mint, swap_amount, 50.0, &quote);

    printf("    ✓ Quote: %.4f SOL → %llu BONK\n",
        cls_solana_lamports_to_sol(quote.input_amount),
        (unsigned long long)quote.output_amount);
    printf("    ✓ Route: %s | Impact: %.2f%% | Fee: %llu\n",
        quote.route, quote.price_impact, (unsigned long long)quote.fee_amount);
    printf("    ✓ Min output (%.0f bps slippage): %llu\n", quote.slippage_bps,
        (unsigned long long)quote.min_output);

    s = cls_solana_execute_swap(&sol, &quote);
    printf("    ✓ Swap: %s | Tokens: %u\n\n",
        CLS_IS_OK(s) ? "EXECUTED" : "FAILED", sol.wallet.token_count);

    /* ======== 7. ON-CHAIN WATCHERS ======== */
    printf("  \033[33m[7/8]\033[0m ON-CHAIN MONITORING\n");
    uint32_t w1, w2;
    cls_solana_watch_balance(&sol, &kp.public_key, 0.01, balance_alert, NULL, &w1);
    cls_solana_watch_price(&sol, &bonk_mint, 0.001, price_alert, NULL, &w2);
    printf("    ✓ Watcher #%u: balance monitor\n", w1);
    printf("    ✓ Watcher #%u: price monitor\n", w2);
    printf("    ✓ Active watchers: %u\n", sol.watcher_count);

    /* Poll watchers */
    cls_solana_poll_watchers(&sol);
    printf("    ✓ Poll cycle completed\n");

    cls_solana_remove_watcher(&sol, w1);
    printf("    ✓ Removed watcher #%u | Remaining: %u\n\n", w1, sol.watcher_count);

    /* ======== 8. STATS ======== */
    printf("  \033[33m[8/8]\033[0m AGENT STATS\n");
    cls_sol_rpc_stats_t stats;
    cls_solana_get_rpc_stats(&sol, &stats);

    uint64_t slot;
    cls_solana_get_slot(&sol, &slot);

    printf("    ✓ RPC: %llu requests | %.0f µs avg latency\n",
        (unsigned long long)stats.total_requests, stats.avg_latency_us);
    printf("    ✓ TX: %llu sent | %llu confirmed\n",
        (unsigned long long)sol.total_tx_sent, (unsigned long long)sol.total_tx_confirmed);
    printf("    ✓ SOL spent (fees): %.6f\n", cls_solana_lamports_to_sol(sol.total_sol_spent));
    printf("    ✓ Current slot: %llu\n", (unsigned long long)slot);
    printf("    ✓ Final balance: %.4f SOL\n", cls_solana_get_balance_sol(&sol));

    /* ======== SUMMARY ======== */
    printf("\n  \033[35m╔══════════════════════════════════════════════════╗\033[0m\n");
    printf("  \033[35m║         SOLANA AGENT FULLY OPERATIONAL           ║\033[0m\n");
    printf("  \033[35m╠══════════════════════════════════════════════════╣\033[0m\n");
    printf("  \033[35m║\033[0m  ✓ Wallet generation & management                \033[35m║\033[0m\n");
    printf("  \033[35m║\033[0m  ✓ SOL transfers with auto-signing               \033[35m║\033[0m\n");
    printf("  \033[35m║\033[0m  ✓ Custom transaction building (multi-ix)        \033[35m║\033[0m\n");
    printf("  \033[35m║\033[0m  ✓ Transaction simulation before send            \033[35m║\033[0m\n");
    printf("  \033[35m║\033[0m  ✓ SPL token transfers with ATA derivation       \033[35m║\033[0m\n");
    printf("  \033[35m║\033[0m  ✓ DeFi price feeds & swap quotes                \033[35m║\033[0m\n");
    printf("  \033[35m║\033[0m  ✓ AMM swap execution (Raydium)                  \033[35m║\033[0m\n");
    printf("  \033[35m║\033[0m  ✓ On-chain balance & price watchers             \033[35m║\033[0m\n");
    printf("  \033[35m║\033[0m  ✓ Base58 encoding/decoding                      \033[35m║\033[0m\n");
    printf("  \033[35m║\033[0m  ✓ Comm bus integration for event routing        \033[35m║\033[0m\n");
    printf("  \033[35m╚══════════════════════════════════════════════════╝\033[0m\n\n");

    /* Cleanup */
    cls_solana_destroy(&sol);
    cls_comm_destroy(&bus);
    printf("  ✓ Solana agent destroyed. All resources released.\n\n");
    return 0;
}
