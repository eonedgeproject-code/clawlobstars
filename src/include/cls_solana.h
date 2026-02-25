/*
 * ClawLobstars - Solana Agent Module
 * On-chain agent: wallet management, RPC, transactions, token ops, DeFi
 *
 * This module enables ClawLobstars agents to operate autonomously
 * on the Solana blockchain — executing trades, managing tokens,
 * monitoring on-chain state, and coordinating via programs.
 */

#ifndef CLS_SOLANA_H
#define CLS_SOLANA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Constants
 * ============================================================ */

#define CLS_SOL_PUBKEY_SIZE     32
#define CLS_SOL_PRIVKEY_SIZE    64
#define CLS_SOL_SIGNATURE_SIZE  64
#define CLS_SOL_HASH_SIZE       32
#define CLS_SOL_MAX_ACCOUNTS    16
#define CLS_SOL_MAX_INSTRUCTIONS 8
#define CLS_SOL_MAX_DATA        1024
#define CLS_SOL_MAX_WATCHERS    32
#define CLS_SOL_RPC_URL_MAX     256

/* Lamports per SOL */
#define CLS_SOL_LAMPORTS_PER_SOL 1000000000ULL

/* Known program IDs (first byte identifier for simulation) */
#define CLS_SOL_SYSTEM_PROGRAM      0x00
#define CLS_SOL_TOKEN_PROGRAM       0x06
#define CLS_SOL_ASSOC_TOKEN_PROGRAM 0x8C
#define CLS_SOL_MEMO_PROGRAM        0x05
#define CLS_SOL_RAYDIUM_AMM         0xAA
#define CLS_SOL_JUPITER_AGG         0xBB

/* ============================================================
 * Types
 * ============================================================ */

/* Network cluster */
typedef enum {
    CLS_SOL_MAINNET   = 0,
    CLS_SOL_DEVNET    = 1,
    CLS_SOL_TESTNET   = 2,
    CLS_SOL_LOCALNET  = 3
} cls_sol_cluster_t;

/* Transaction status */
typedef enum {
    CLS_SOL_TX_PENDING    = 0,
    CLS_SOL_TX_CONFIRMED  = 1,
    CLS_SOL_TX_FINALIZED  = 2,
    CLS_SOL_TX_FAILED     = 3,
    CLS_SOL_TX_DROPPED    = 4,
    CLS_SOL_TX_SIMULATED  = 5
} cls_sol_tx_status_t;

/* Commitment level */
typedef enum {
    CLS_SOL_PROCESSED  = 0,
    CLS_SOL_CONFIRMED  = 1,
    CLS_SOL_FINALIZED  = 2
} cls_sol_commitment_t;

/* Token standard */
typedef enum {
    CLS_SOL_TOKEN_SPL    = 0,
    CLS_SOL_TOKEN_2022   = 1
} cls_sol_token_std_t;

/* DeFi operation type */
typedef enum {
    CLS_DEFI_SWAP        = 0,
    CLS_DEFI_ADD_LIQ     = 1,
    CLS_DEFI_REMOVE_LIQ  = 2,
    CLS_DEFI_STAKE        = 3,
    CLS_DEFI_UNSTAKE      = 4,
    CLS_DEFI_BORROW       = 5,
    CLS_DEFI_REPAY        = 6
} cls_defi_op_t;

/* Watcher event type */
typedef enum {
    CLS_WATCH_BALANCE     = 0,
    CLS_WATCH_TOKEN       = 1,
    CLS_WATCH_ACCOUNT     = 2,
    CLS_WATCH_PROGRAM     = 3,
    CLS_WATCH_PRICE       = 4
} cls_watch_type_t;

/* ============================================================
 * Core Structures
 * ============================================================ */

/* Public key (32 bytes) */
typedef struct {
    uint8_t     bytes[CLS_SOL_PUBKEY_SIZE];
} cls_sol_pubkey_t;

/* Keypair */
typedef struct {
    uint8_t     private_key[CLS_SOL_PRIVKEY_SIZE];
    cls_sol_pubkey_t public_key;
    bool        loaded;
    char        label[32];
} cls_sol_keypair_t;

/* Account info from RPC */
typedef struct {
    cls_sol_pubkey_t owner;
    uint64_t    lamports;
    uint8_t    *data;
    size_t      data_len;
    bool        executable;
    uint64_t    rent_epoch;
} cls_sol_account_info_t;

/* Token account */
typedef struct {
    cls_sol_pubkey_t mint;
    cls_sol_pubkey_t owner;
    uint64_t    amount;
    uint8_t     decimals;
    bool        is_native;
    char        symbol[16];
} cls_sol_token_account_t;

/* Transaction instruction */
typedef struct {
    cls_sol_pubkey_t program_id;
    cls_sol_pubkey_t accounts[CLS_SOL_MAX_ACCOUNTS];
    bool            is_signer[CLS_SOL_MAX_ACCOUNTS];
    bool            is_writable[CLS_SOL_MAX_ACCOUNTS];
    uint8_t         account_count;
    uint8_t         data[CLS_SOL_MAX_DATA];
    size_t          data_len;
} cls_sol_instruction_t;

/* Transaction */
typedef struct {
    cls_sol_instruction_t instructions[CLS_SOL_MAX_INSTRUCTIONS];
    uint8_t             instruction_count;
    uint8_t             blockhash[CLS_SOL_HASH_SIZE];
    cls_sol_pubkey_t    fee_payer;
    uint8_t             signatures[4][CLS_SOL_SIGNATURE_SIZE];
    uint8_t             signature_count;
    cls_sol_tx_status_t status;
    uint64_t            created_at;
    uint64_t            confirmed_at;
    uint64_t            fee_lamports;
    uint64_t            compute_units;
    char                tx_hash[88]; /* Base58 signature */
} cls_sol_transaction_t;

/* Price feed */
typedef struct {
    cls_sol_pubkey_t token_mint;
    char            symbol[16];
    double          price_usd;
    double          price_sol;
    double          change_24h;
    double          volume_24h;
    uint64_t        market_cap;
    uint64_t        updated_at;
} cls_sol_price_t;

/* Swap quote */
typedef struct {
    cls_sol_pubkey_t input_mint;
    cls_sol_pubkey_t output_mint;
    uint64_t        input_amount;
    uint64_t        output_amount;
    uint64_t        min_output;     /* After slippage */
    double          price_impact;
    uint64_t        fee_amount;
    double          slippage_bps;
    char            route[64];      /* AMM used */
} cls_sol_swap_quote_t;

/* On-chain watcher */
typedef struct {
    uint32_t        watcher_id;
    cls_watch_type_t type;
    cls_sol_pubkey_t target;
    double          threshold;
    bool            triggered;
    uint64_t        last_check;
    void          (*callback)(uint32_t watcher_id, double old_val, double new_val, void *ctx);
    void           *user_ctx;
} cls_sol_watcher_t;

/* Agent wallet state */
typedef struct {
    cls_sol_keypair_t       keypair;
    uint64_t                sol_balance;
    cls_sol_token_account_t tokens[32];
    uint32_t                token_count;
    double                  portfolio_value_usd;
    uint64_t                last_sync;
} cls_sol_wallet_t;

/* RPC stats */
typedef struct {
    uint64_t    total_requests;
    uint64_t    successful;
    uint64_t    failed;
    uint64_t    total_latency_us;
    double      avg_latency_us;
    uint64_t    last_slot;
    uint64_t    last_block_time;
} cls_sol_rpc_stats_t;

/* ============================================================
 * Solana Agent Context
 * ============================================================ */

typedef struct cls_solana_agent {
    /* Connection */
    cls_sol_cluster_t   cluster;
    char                rpc_url[CLS_SOL_RPC_URL_MAX];
    cls_sol_commitment_t commitment;
    bool                connected;

    /* Wallet */
    cls_sol_wallet_t    wallet;

    /* Transaction history */
    cls_sol_transaction_t *tx_history;
    uint32_t            tx_count;
    uint32_t            tx_max;

    /* Watchers */
    cls_sol_watcher_t   watchers[CLS_SOL_MAX_WATCHERS];
    uint32_t            watcher_count;
    uint32_t            next_watcher_id;

    /* Price cache */
    cls_sol_price_t    *price_cache;
    uint32_t            price_count;
    uint32_t            price_max;

    /* Stats */
    cls_sol_rpc_stats_t rpc_stats;
    uint64_t            total_tx_sent;
    uint64_t            total_tx_confirmed;
    uint64_t            total_sol_spent;
    uint64_t            total_sol_earned;

    /* Agent integration */
    cls_comm_bus_t     *comm_bus;
} cls_solana_agent_t;

/* ============================================================
 * API — Initialization & Connection
 * ============================================================ */

cls_status_t cls_solana_init(cls_solana_agent_t *agent, cls_sol_cluster_t cluster,
                              uint32_t tx_history_size, uint32_t price_cache_size);

cls_status_t cls_solana_connect(cls_solana_agent_t *agent, const char *rpc_url);
cls_status_t cls_solana_disconnect(cls_solana_agent_t *agent);
cls_status_t cls_solana_set_commitment(cls_solana_agent_t *agent, cls_sol_commitment_t level);

/* Link to comm bus for inter-module messaging */
cls_status_t cls_solana_set_comm(cls_solana_agent_t *agent, cls_comm_bus_t *bus);

/* ============================================================
 * API — Wallet Management
 * ============================================================ */

cls_status_t cls_solana_generate_keypair(cls_sol_keypair_t *kp);
cls_status_t cls_solana_load_keypair(cls_sol_keypair_t *kp, const uint8_t *secret, size_t len);
cls_status_t cls_solana_set_wallet(cls_solana_agent_t *agent, const cls_sol_keypair_t *kp);
cls_status_t cls_solana_sync_wallet(cls_solana_agent_t *agent);

/* Get wallet info */
uint64_t cls_solana_get_balance(const cls_solana_agent_t *agent);
double cls_solana_get_balance_sol(const cls_solana_agent_t *agent);
const cls_sol_pubkey_t *cls_solana_get_pubkey(const cls_solana_agent_t *agent);

/* ============================================================
 * API — RPC Operations (Simulated in offline mode)
 * ============================================================ */

cls_status_t cls_solana_get_account_info(cls_solana_agent_t *agent, const cls_sol_pubkey_t *pubkey,
                                          cls_sol_account_info_t *info);

cls_status_t cls_solana_get_token_accounts(cls_solana_agent_t *agent,
                                            cls_sol_token_account_t *accounts,
                                            uint32_t *count, uint32_t max_accounts);

cls_status_t cls_solana_get_slot(cls_solana_agent_t *agent, uint64_t *slot);
cls_status_t cls_solana_get_block_time(cls_solana_agent_t *agent, uint64_t slot, uint64_t *timestamp);

/* ============================================================
 * API — Transaction Building & Execution
 * ============================================================ */

cls_status_t cls_solana_tx_init(cls_sol_transaction_t *tx, const cls_sol_pubkey_t *fee_payer);

cls_status_t cls_solana_tx_add_instruction(cls_sol_transaction_t *tx,
                                            const cls_sol_instruction_t *ix);

/* Pre-built instructions */
cls_status_t cls_solana_ix_transfer_sol(cls_sol_instruction_t *ix,
                                         const cls_sol_pubkey_t *from,
                                         const cls_sol_pubkey_t *to,
                                         uint64_t lamports);

cls_status_t cls_solana_ix_transfer_token(cls_sol_instruction_t *ix,
                                           const cls_sol_pubkey_t *from_ata,
                                           const cls_sol_pubkey_t *to_ata,
                                           const cls_sol_pubkey_t *authority,
                                           uint64_t amount, uint8_t decimals);

cls_status_t cls_solana_ix_create_ata(cls_sol_instruction_t *ix,
                                       const cls_sol_pubkey_t *payer,
                                       const cls_sol_pubkey_t *wallet,
                                       const cls_sol_pubkey_t *mint);

cls_status_t cls_solana_ix_memo(cls_sol_instruction_t *ix,
                                 const char *memo, size_t memo_len);

/* Sign & send */
cls_status_t cls_solana_tx_sign(cls_sol_transaction_t *tx, const cls_sol_keypair_t *signer);
cls_status_t cls_solana_tx_simulate(cls_solana_agent_t *agent, cls_sol_transaction_t *tx);
cls_status_t cls_solana_tx_send(cls_solana_agent_t *agent, cls_sol_transaction_t *tx);
cls_status_t cls_solana_tx_confirm(cls_solana_agent_t *agent, cls_sol_transaction_t *tx,
                                    uint32_t timeout_ms);

/* ============================================================
 * API — Token Operations
 * ============================================================ */

cls_status_t cls_solana_get_token_balance(cls_solana_agent_t *agent,
                                           const cls_sol_pubkey_t *mint,
                                           uint64_t *amount, uint8_t *decimals);

cls_status_t cls_solana_transfer_sol(cls_solana_agent_t *agent,
                                      const cls_sol_pubkey_t *to, uint64_t lamports);

cls_status_t cls_solana_transfer_token(cls_solana_agent_t *agent,
                                        const cls_sol_pubkey_t *mint,
                                        const cls_sol_pubkey_t *to,
                                        uint64_t amount);

/* ============================================================
 * API — DeFi Operations
 * ============================================================ */

cls_status_t cls_solana_get_price(cls_solana_agent_t *agent,
                                   const cls_sol_pubkey_t *mint,
                                   cls_sol_price_t *price);

cls_status_t cls_solana_get_swap_quote(cls_solana_agent_t *agent,
                                        const cls_sol_pubkey_t *input_mint,
                                        const cls_sol_pubkey_t *output_mint,
                                        uint64_t amount, double slippage_bps,
                                        cls_sol_swap_quote_t *quote);

cls_status_t cls_solana_execute_swap(cls_solana_agent_t *agent,
                                      const cls_sol_swap_quote_t *quote);

/* ============================================================
 * API — On-Chain Monitoring
 * ============================================================ */

cls_status_t cls_solana_watch_balance(cls_solana_agent_t *agent,
                                       const cls_sol_pubkey_t *target,
                                       double threshold,
                                       void (*callback)(uint32_t, double, double, void *),
                                       void *ctx, uint32_t *watcher_id);

cls_status_t cls_solana_watch_token(cls_solana_agent_t *agent,
                                     const cls_sol_pubkey_t *mint,
                                     double threshold,
                                     void (*callback)(uint32_t, double, double, void *),
                                     void *ctx, uint32_t *watcher_id);

cls_status_t cls_solana_watch_price(cls_solana_agent_t *agent,
                                     const cls_sol_pubkey_t *mint,
                                     double threshold,
                                     void (*callback)(uint32_t, double, double, void *),
                                     void *ctx, uint32_t *watcher_id);

cls_status_t cls_solana_remove_watcher(cls_solana_agent_t *agent, uint32_t watcher_id);

/* Process all watchers (call in agent loop) */
cls_status_t cls_solana_poll_watchers(cls_solana_agent_t *agent);

/* ============================================================
 * API — Utilities
 * ============================================================ */

/* Pubkey helpers */
cls_status_t cls_solana_pubkey_from_bytes(cls_sol_pubkey_t *pk, const uint8_t *bytes);
cls_status_t cls_solana_pubkey_from_base58(cls_sol_pubkey_t *pk, const char *base58);
cls_status_t cls_solana_pubkey_to_base58(const cls_sol_pubkey_t *pk, char *out, size_t out_len);
bool cls_solana_pubkey_equal(const cls_sol_pubkey_t *a, const cls_sol_pubkey_t *b);

/* Derive associated token address */
cls_status_t cls_solana_derive_ata(cls_sol_pubkey_t *ata,
                                    const cls_sol_pubkey_t *wallet,
                                    const cls_sol_pubkey_t *mint);

/* SOL <-> lamports conversion */
double cls_solana_lamports_to_sol(uint64_t lamports);
uint64_t cls_solana_sol_to_lamports(double sol);

/* Stats */
void cls_solana_get_rpc_stats(const cls_solana_agent_t *agent, cls_sol_rpc_stats_t *stats);

/* Cleanup */
void cls_solana_destroy(cls_solana_agent_t *agent);

#ifdef __cplusplus
}
#endif

#endif /* CLS_SOLANA_H */
