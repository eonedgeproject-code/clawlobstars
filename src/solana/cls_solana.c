/*
 * ClawLobstars - Solana Agent Implementation
 * Wallet, transactions, token ops, DeFi, on-chain monitoring
 *
 * NOTE: RPC calls are simulated for offline/edge operation.
 * Plug real HTTP client (libcurl) for mainnet connectivity.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include "../include/cls_framework.h"
#include "../include/cls_solana.h"

/* ============================================================
 * Internal Helpers
 * ============================================================ */

static uint64_t sol_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* Simple xorshift PRNG */
static uint64_t sol_rng_state = 0;
static uint64_t sol_rand64(void) {
    if (sol_rng_state == 0) sol_rng_state = sol_time_us();
    sol_rng_state ^= sol_rng_state << 13;
    sol_rng_state ^= sol_rng_state >> 7;
    sol_rng_state ^= sol_rng_state << 17;
    return sol_rng_state;
}

/* Base58 alphabet */
static const char B58_ALPHABET[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

static void rpc_record(cls_solana_agent_t *agent, bool success, uint64_t latency) {
    agent->rpc_stats.total_requests++;
    if (success) agent->rpc_stats.successful++;
    else agent->rpc_stats.failed++;
    agent->rpc_stats.total_latency_us += latency;
    if (agent->rpc_stats.total_requests > 0) {
        agent->rpc_stats.avg_latency_us = (double)agent->rpc_stats.total_latency_us /
                                            (double)agent->rpc_stats.total_requests;
    }
}

/* ============================================================
 * Initialization & Connection
 * ============================================================ */

cls_status_t cls_solana_init(cls_solana_agent_t *agent, cls_sol_cluster_t cluster,
                              uint32_t tx_history_size, uint32_t price_cache_size) {
    if (!agent) return CLS_ERR_INVALID;

    memset(agent, 0, sizeof(cls_solana_agent_t));
    agent->cluster = cluster;
    agent->commitment = CLS_SOL_CONFIRMED;
    agent->next_watcher_id = 1;

    /* Set default RPC URL */
    switch (cluster) {
    case CLS_SOL_MAINNET:
        strncpy(agent->rpc_url, "https://api.mainnet-beta.solana.com", CLS_SOL_RPC_URL_MAX);
        break;
    case CLS_SOL_DEVNET:
        strncpy(agent->rpc_url, "https://api.devnet.solana.com", CLS_SOL_RPC_URL_MAX);
        break;
    case CLS_SOL_TESTNET:
        strncpy(agent->rpc_url, "https://api.testnet.solana.com", CLS_SOL_RPC_URL_MAX);
        break;
    case CLS_SOL_LOCALNET:
        strncpy(agent->rpc_url, "http://127.0.0.1:8899", CLS_SOL_RPC_URL_MAX);
        break;
    }

    if (tx_history_size > 0) {
        agent->tx_history = (cls_sol_transaction_t *)calloc(tx_history_size, sizeof(cls_sol_transaction_t));
        if (!agent->tx_history) return CLS_ERR_NOMEM;
        agent->tx_max = tx_history_size;
    }

    if (price_cache_size > 0) {
        agent->price_cache = (cls_sol_price_t *)calloc(price_cache_size, sizeof(cls_sol_price_t));
        if (!agent->price_cache) {
            free(agent->tx_history);
            return CLS_ERR_NOMEM;
        }
        agent->price_max = price_cache_size;
    }

    return CLS_OK;
}

cls_status_t cls_solana_connect(cls_solana_agent_t *agent, const char *rpc_url) {
    if (!agent) return CLS_ERR_INVALID;
    if (rpc_url) {
        strncpy(agent->rpc_url, rpc_url, CLS_SOL_RPC_URL_MAX - 1);
    }
    agent->connected = true;
    agent->rpc_stats.last_slot = 250000000ULL + (sol_rand64() % 1000000);
    agent->rpc_stats.last_block_time = (uint64_t)time(NULL);
    rpc_record(agent, true, 50);
    return CLS_OK;
}

cls_status_t cls_solana_disconnect(cls_solana_agent_t *agent) {
    if (!agent) return CLS_ERR_INVALID;
    agent->connected = false;
    return CLS_OK;
}

cls_status_t cls_solana_set_commitment(cls_solana_agent_t *agent, cls_sol_commitment_t level) {
    if (!agent) return CLS_ERR_INVALID;
    agent->commitment = level;
    return CLS_OK;
}

cls_status_t cls_solana_set_comm(cls_solana_agent_t *agent, cls_comm_bus_t *bus) {
    if (!agent) return CLS_ERR_INVALID;
    agent->comm_bus = bus;
    return CLS_OK;
}

/* ============================================================
 * Wallet Management
 * ============================================================ */

cls_status_t cls_solana_generate_keypair(cls_sol_keypair_t *kp) {
    if (!kp) return CLS_ERR_INVALID;

    memset(kp, 0, sizeof(cls_sol_keypair_t));

    /* Generate pseudo-random keypair */
    for (int i = 0; i < CLS_SOL_PRIVKEY_SIZE; i++) {
        kp->private_key[i] = (uint8_t)(sol_rand64() & 0xFF);
    }

    /* Public key = hash of private key (simplified Ed25519) */
    uint32_t hash = 2166136261u;
    for (int i = 0; i < CLS_SOL_PRIVKEY_SIZE; i++) {
        hash ^= kp->private_key[i];
        hash *= 16777619u;
    }
    for (int i = 0; i < CLS_SOL_PUBKEY_SIZE; i++) {
        kp->public_key.bytes[i] = (uint8_t)((hash >> (i % 4 * 8)) ^ kp->private_key[i]);
        hash = hash * 16777619u + (uint32_t)i;
    }

    kp->loaded = true;
    snprintf(kp->label, sizeof(kp->label), "wallet-%04x", (uint16_t)(hash & 0xFFFF));
    return CLS_OK;
}

cls_status_t cls_solana_load_keypair(cls_sol_keypair_t *kp, const uint8_t *secret, size_t len) {
    if (!kp || !secret) return CLS_ERR_INVALID;
    if (len < CLS_SOL_PRIVKEY_SIZE) return CLS_ERR_INVALID;

    memset(kp, 0, sizeof(cls_sol_keypair_t));
    memcpy(kp->private_key, secret, CLS_SOL_PRIVKEY_SIZE);

    /* Derive public key */
    if (len >= CLS_SOL_PRIVKEY_SIZE + CLS_SOL_PUBKEY_SIZE) {
        memcpy(kp->public_key.bytes, secret + CLS_SOL_PRIVKEY_SIZE, CLS_SOL_PUBKEY_SIZE);
    } else {
        uint32_t hash = 2166136261u;
        for (int i = 0; i < CLS_SOL_PRIVKEY_SIZE; i++) {
            hash ^= kp->private_key[i];
            hash *= 16777619u;
        }
        for (int i = 0; i < CLS_SOL_PUBKEY_SIZE; i++) {
            kp->public_key.bytes[i] = (uint8_t)((hash >> (i % 4 * 8)) ^ kp->private_key[i]);
            hash = hash * 16777619u + (uint32_t)i;
        }
    }

    kp->loaded = true;
    return CLS_OK;
}

cls_status_t cls_solana_set_wallet(cls_solana_agent_t *agent, const cls_sol_keypair_t *kp) {
    if (!agent || !kp || !kp->loaded) return CLS_ERR_INVALID;
    agent->wallet.keypair = *kp;
    agent->wallet.last_sync = 0;
    return CLS_OK;
}

cls_status_t cls_solana_sync_wallet(cls_solana_agent_t *agent) {
    if (!agent || !agent->wallet.keypair.loaded) return CLS_ERR_STATE;

    uint64_t start = sol_time_us();

    /* Simulate balance fetch */
    if (agent->wallet.sol_balance == 0) {
        agent->wallet.sol_balance = (sol_rand64() % 50) * CLS_SOL_LAMPORTS_PER_SOL / 10;
    }

    /* Simulate token discovery */
    if (agent->wallet.token_count == 0) {
        /* Add SOL native */
        cls_sol_token_account_t *wsol = &agent->wallet.tokens[0];
        memset(wsol->mint.bytes, 0, CLS_SOL_PUBKEY_SIZE);
        wsol->mint.bytes[0] = 0x06; /* Wrapped SOL identifier */
        wsol->owner = agent->wallet.keypair.public_key;
        wsol->amount = agent->wallet.sol_balance;
        wsol->decimals = 9;
        wsol->is_native = true;
        strncpy(wsol->symbol, "SOL", sizeof(wsol->symbol));
        agent->wallet.token_count = 1;
    }

    agent->wallet.portfolio_value_usd =
        cls_solana_lamports_to_sol(agent->wallet.sol_balance) * 150.0; /* Simulated price */

    agent->wallet.last_sync = sol_time_us();
    rpc_record(agent, true, sol_time_us() - start);
    return CLS_OK;
}

uint64_t cls_solana_get_balance(const cls_solana_agent_t *agent) {
    return agent ? agent->wallet.sol_balance : 0;
}

double cls_solana_get_balance_sol(const cls_solana_agent_t *agent) {
    return agent ? cls_solana_lamports_to_sol(agent->wallet.sol_balance) : 0.0;
}

const cls_sol_pubkey_t *cls_solana_get_pubkey(const cls_solana_agent_t *agent) {
    return agent ? &agent->wallet.keypair.public_key : NULL;
}

/* ============================================================
 * RPC Operations
 * ============================================================ */

cls_status_t cls_solana_get_account_info(cls_solana_agent_t *agent, const cls_sol_pubkey_t *pubkey,
                                          cls_sol_account_info_t *info) {
    if (!agent || !pubkey || !info) return CLS_ERR_INVALID;
    uint64_t start = sol_time_us();

    memset(info, 0, sizeof(cls_sol_account_info_t));
    info->lamports = (sol_rand64() % 100) * CLS_SOL_LAMPORTS_PER_SOL / 10;
    info->executable = false;
    info->rent_epoch = 0;

    rpc_record(agent, true, sol_time_us() - start);
    return CLS_OK;
}

cls_status_t cls_solana_get_token_accounts(cls_solana_agent_t *agent,
                                            cls_sol_token_account_t *accounts,
                                            uint32_t *count, uint32_t max_accounts) {
    if (!agent || !accounts || !count) return CLS_ERR_INVALID;

    uint32_t to_copy = (agent->wallet.token_count < max_accounts) ?
                         agent->wallet.token_count : max_accounts;
    memcpy(accounts, agent->wallet.tokens, to_copy * sizeof(cls_sol_token_account_t));
    *count = to_copy;
    return CLS_OK;
}

cls_status_t cls_solana_get_slot(cls_solana_agent_t *agent, uint64_t *slot) {
    if (!agent || !slot) return CLS_ERR_INVALID;
    agent->rpc_stats.last_slot++;
    *slot = agent->rpc_stats.last_slot;
    rpc_record(agent, true, 5);
    return CLS_OK;
}

cls_status_t cls_solana_get_block_time(cls_solana_agent_t *agent, uint64_t slot, uint64_t *timestamp) {
    if (!agent || !timestamp) return CLS_ERR_INVALID;
    CLS_UNUSED(slot);
    *timestamp = (uint64_t)time(NULL);
    return CLS_OK;
}

/* ============================================================
 * Transaction Building
 * ============================================================ */

cls_status_t cls_solana_tx_init(cls_sol_transaction_t *tx, const cls_sol_pubkey_t *fee_payer) {
    if (!tx || !fee_payer) return CLS_ERR_INVALID;
    memset(tx, 0, sizeof(cls_sol_transaction_t));
    tx->fee_payer = *fee_payer;
    tx->status = CLS_SOL_TX_PENDING;
    tx->created_at = sol_time_us();

    /* Simulated recent blockhash */
    for (int i = 0; i < CLS_SOL_HASH_SIZE; i++) {
        tx->blockhash[i] = (uint8_t)(sol_rand64() & 0xFF);
    }

    return CLS_OK;
}

cls_status_t cls_solana_tx_add_instruction(cls_sol_transaction_t *tx,
                                            const cls_sol_instruction_t *ix) {
    if (!tx || !ix) return CLS_ERR_INVALID;
    if (tx->instruction_count >= CLS_SOL_MAX_INSTRUCTIONS) return CLS_ERR_OVERFLOW;

    tx->instructions[tx->instruction_count] = *ix;
    tx->instruction_count++;
    return CLS_OK;
}

cls_status_t cls_solana_ix_transfer_sol(cls_sol_instruction_t *ix,
                                         const cls_sol_pubkey_t *from,
                                         const cls_sol_pubkey_t *to,
                                         uint64_t lamports) {
    if (!ix || !from || !to || lamports == 0) return CLS_ERR_INVALID;

    memset(ix, 0, sizeof(cls_sol_instruction_t));

    /* System Program transfer instruction */
    memset(ix->program_id.bytes, 0, CLS_SOL_PUBKEY_SIZE);
    ix->program_id.bytes[0] = CLS_SOL_SYSTEM_PROGRAM;

    ix->accounts[0] = *from;
    ix->is_signer[0] = true;
    ix->is_writable[0] = true;
    ix->accounts[1] = *to;
    ix->is_signer[1] = false;
    ix->is_writable[1] = true;
    ix->account_count = 2;

    /* Instruction data: [2,0,0,0] + lamports (LE u64) */
    ix->data[0] = 2; ix->data[1] = 0; ix->data[2] = 0; ix->data[3] = 0;
    memcpy(&ix->data[4], &lamports, sizeof(uint64_t));
    ix->data_len = 12;

    return CLS_OK;
}

cls_status_t cls_solana_ix_transfer_token(cls_sol_instruction_t *ix,
                                           const cls_sol_pubkey_t *from_ata,
                                           const cls_sol_pubkey_t *to_ata,
                                           const cls_sol_pubkey_t *authority,
                                           uint64_t amount, uint8_t decimals) {
    if (!ix || !from_ata || !to_ata || !authority) return CLS_ERR_INVALID;

    memset(ix, 0, sizeof(cls_sol_instruction_t));

    /* Token Program transfer checked */
    memset(ix->program_id.bytes, 0, CLS_SOL_PUBKEY_SIZE);
    ix->program_id.bytes[0] = CLS_SOL_TOKEN_PROGRAM;

    ix->accounts[0] = *from_ata;  ix->is_writable[0] = true;
    ix->accounts[1] = *to_ata;    ix->is_writable[1] = true;
    ix->accounts[2] = *authority;  ix->is_signer[2] = true;
    ix->account_count = 3;

    /* TransferChecked instruction: type(1) + amount(8) + decimals(1) */
    ix->data[0] = 12; /* TransferChecked */
    memcpy(&ix->data[1], &amount, sizeof(uint64_t));
    ix->data[9] = decimals;
    ix->data_len = 10;

    return CLS_OK;
}

cls_status_t cls_solana_ix_create_ata(cls_sol_instruction_t *ix,
                                       const cls_sol_pubkey_t *payer,
                                       const cls_sol_pubkey_t *wallet,
                                       const cls_sol_pubkey_t *mint) {
    if (!ix || !payer || !wallet || !mint) return CLS_ERR_INVALID;

    memset(ix, 0, sizeof(cls_sol_instruction_t));
    ix->program_id.bytes[0] = CLS_SOL_ASSOC_TOKEN_PROGRAM;

    ix->accounts[0] = *payer;   ix->is_signer[0] = true;  ix->is_writable[0] = true;
    ix->accounts[1] = *wallet;  /* ATA address (derived) */
    ix->accounts[2] = *wallet;  /* Wallet owner */
    ix->accounts[3] = *mint;    /* Token mint */
    ix->account_count = 4;
    ix->data_len = 0; /* No instruction data needed */

    return CLS_OK;
}

cls_status_t cls_solana_ix_memo(cls_sol_instruction_t *ix, const char *memo, size_t memo_len) {
    if (!ix || !memo || memo_len == 0) return CLS_ERR_INVALID;
    if (memo_len > CLS_SOL_MAX_DATA) return CLS_ERR_OVERFLOW;

    memset(ix, 0, sizeof(cls_sol_instruction_t));
    ix->program_id.bytes[0] = CLS_SOL_MEMO_PROGRAM;
    ix->account_count = 0;
    memcpy(ix->data, memo, memo_len);
    ix->data_len = memo_len;

    return CLS_OK;
}

cls_status_t cls_solana_tx_sign(cls_sol_transaction_t *tx, const cls_sol_keypair_t *signer) {
    if (!tx || !signer || !signer->loaded) return CLS_ERR_INVALID;
    if (tx->signature_count >= 4) return CLS_ERR_OVERFLOW;

    /* Simulated Ed25519 signature */
    uint32_t hash = 2166136261u;
    for (int i = 0; i < CLS_SOL_HASH_SIZE; i++) {
        hash ^= tx->blockhash[i];
        hash *= 16777619u;
    }
    for (int i = 0; i < CLS_SOL_PRIVKEY_SIZE; i++) {
        hash ^= signer->private_key[i];
        hash *= 16777619u;
    }

    uint8_t *sig = tx->signatures[tx->signature_count];
    for (int i = 0; i < CLS_SOL_SIGNATURE_SIZE; i++) {
        sig[i] = (uint8_t)(hash & 0xFF);
        hash = hash * 16777619u + (uint32_t)i;
    }

    /* Generate base58 tx hash from first signature */
    if (tx->signature_count == 0) {
        for (int i = 0; i < 43 && i < 87; i++) {
            tx->tx_hash[i] = B58_ALPHABET[sig[i % CLS_SOL_SIGNATURE_SIZE] % 58];
        }
        tx->tx_hash[43] = '\0';
    }

    tx->signature_count++;
    return CLS_OK;
}

cls_status_t cls_solana_tx_simulate(cls_solana_agent_t *agent, cls_sol_transaction_t *tx) {
    if (!agent || !tx) return CLS_ERR_INVALID;
    uint64_t start = sol_time_us();

    /* Simulated execution */
    tx->compute_units = 200000 + (sol_rand64() % 100000);
    tx->fee_lamports = 5000 + tx->compute_units / 1000;
    tx->status = CLS_SOL_TX_SIMULATED;

    rpc_record(agent, true, sol_time_us() - start);
    return CLS_OK;
}

cls_status_t cls_solana_tx_send(cls_solana_agent_t *agent, cls_sol_transaction_t *tx) {
    if (!agent || !tx) return CLS_ERR_INVALID;
    if (tx->signature_count == 0) return CLS_ERR_STATE;

    uint64_t start = sol_time_us();

    /* Simulate send */
    tx->status = CLS_SOL_TX_CONFIRMED;
    tx->confirmed_at = sol_time_us();
    tx->compute_units = 200000 + (sol_rand64() % 100000);
    tx->fee_lamports = 5000;

    /* Record in history */
    if (agent->tx_count < agent->tx_max) {
        agent->tx_history[agent->tx_count] = *tx;
        agent->tx_count++;
    }

    agent->total_tx_sent++;
    agent->total_tx_confirmed++;
    agent->total_sol_spent += tx->fee_lamports;

    rpc_record(agent, true, sol_time_us() - start);

    /* Notify via comm bus */
    if (agent->comm_bus) {
        cls_comm_broadcast(agent->comm_bus, CLS_MSG_ACTION, tx->tx_hash, strlen(tx->tx_hash));
    }

    return CLS_OK;
}

cls_status_t cls_solana_tx_confirm(cls_solana_agent_t *agent, cls_sol_transaction_t *tx,
                                    uint32_t timeout_ms) {
    if (!agent || !tx) return CLS_ERR_INVALID;
    CLS_UNUSED(timeout_ms);

    if (tx->status == CLS_SOL_TX_CONFIRMED || tx->status == CLS_SOL_TX_FINALIZED) {
        return CLS_OK;
    }
    return CLS_ERR_TIMEOUT;
}

/* ============================================================
 * High-Level Token Operations
 * ============================================================ */

cls_status_t cls_solana_get_token_balance(cls_solana_agent_t *agent,
                                           const cls_sol_pubkey_t *mint,
                                           uint64_t *amount, uint8_t *decimals) {
    if (!agent || !mint || !amount) return CLS_ERR_INVALID;

    for (uint32_t i = 0; i < agent->wallet.token_count; i++) {
        if (cls_solana_pubkey_equal(&agent->wallet.tokens[i].mint, mint)) {
            *amount = agent->wallet.tokens[i].amount;
            if (decimals) *decimals = agent->wallet.tokens[i].decimals;
            return CLS_OK;
        }
    }

    *amount = 0;
    return CLS_ERR_NOT_FOUND;
}

cls_status_t cls_solana_transfer_sol(cls_solana_agent_t *agent,
                                      const cls_sol_pubkey_t *to, uint64_t lamports) {
    if (!agent || !to || lamports == 0) return CLS_ERR_INVALID;
    if (agent->wallet.sol_balance < lamports + 5000) return CLS_ERR_NOMEM; /* Insufficient */

    cls_sol_transaction_t tx;
    cls_solana_tx_init(&tx, &agent->wallet.keypair.public_key);

    cls_sol_instruction_t ix;
    cls_solana_ix_transfer_sol(&ix, &agent->wallet.keypair.public_key, to, lamports);
    cls_solana_tx_add_instruction(&tx, &ix);

    cls_solana_tx_sign(&tx, &agent->wallet.keypair);
    cls_status_t result = cls_solana_tx_send(agent, &tx);

    if (CLS_IS_OK(result)) {
        agent->wallet.sol_balance -= (lamports + tx.fee_lamports);
    }

    return result;
}

cls_status_t cls_solana_transfer_token(cls_solana_agent_t *agent,
                                        const cls_sol_pubkey_t *mint,
                                        const cls_sol_pubkey_t *to,
                                        uint64_t amount) {
    if (!agent || !mint || !to || amount == 0) return CLS_ERR_INVALID;

    /* Find token in wallet */
    cls_sol_token_account_t *token = NULL;
    for (uint32_t i = 0; i < agent->wallet.token_count; i++) {
        if (cls_solana_pubkey_equal(&agent->wallet.tokens[i].mint, mint)) {
            token = &agent->wallet.tokens[i];
            break;
        }
    }
    if (!token || token->amount < amount) return CLS_ERR_NOMEM;

    /* Derive ATAs */
    cls_sol_pubkey_t from_ata, to_ata;
    cls_solana_derive_ata(&from_ata, &agent->wallet.keypair.public_key, mint);
    cls_solana_derive_ata(&to_ata, to, mint);

    cls_sol_transaction_t tx;
    cls_solana_tx_init(&tx, &agent->wallet.keypair.public_key);

    cls_sol_instruction_t ix;
    cls_solana_ix_transfer_token(&ix, &from_ata, &to_ata,
                                  &agent->wallet.keypair.public_key,
                                  amount, token->decimals);
    cls_solana_tx_add_instruction(&tx, &ix);

    cls_solana_tx_sign(&tx, &agent->wallet.keypair);
    cls_status_t result = cls_solana_tx_send(agent, &tx);

    if (CLS_IS_OK(result)) {
        token->amount -= amount;
        agent->wallet.sol_balance -= tx.fee_lamports;
    }

    return result;
}

/* ============================================================
 * DeFi Operations
 * ============================================================ */

cls_status_t cls_solana_get_price(cls_solana_agent_t *agent,
                                   const cls_sol_pubkey_t *mint,
                                   cls_sol_price_t *price) {
    if (!agent || !mint || !price) return CLS_ERR_INVALID;

    /* Check cache */
    for (uint32_t i = 0; i < agent->price_count; i++) {
        if (cls_solana_pubkey_equal(&agent->price_cache[i].token_mint, mint)) {
            *price = agent->price_cache[i];
            return CLS_OK;
        }
    }

    /* Simulate price fetch */
    memset(price, 0, sizeof(cls_sol_price_t));
    price->token_mint = *mint;
    price->price_usd = 0.001 + (double)(sol_rand64() % 10000) / 100.0;
    price->price_sol = price->price_usd / 150.0;
    price->change_24h = ((double)(sol_rand64() % 200) - 100.0) / 10.0;
    price->volume_24h = (sol_rand64() % 10000000);
    price->market_cap = (sol_rand64() % 100000000);
    price->updated_at = sol_time_us();

    /* Cache it */
    if (agent->price_count < agent->price_max) {
        agent->price_cache[agent->price_count] = *price;
        agent->price_count++;
    }

    rpc_record(agent, true, 10);
    return CLS_OK;
}

cls_status_t cls_solana_get_swap_quote(cls_solana_agent_t *agent,
                                        const cls_sol_pubkey_t *input_mint,
                                        const cls_sol_pubkey_t *output_mint,
                                        uint64_t amount, double slippage_bps,
                                        cls_sol_swap_quote_t *quote) {
    if (!agent || !input_mint || !output_mint || !quote || amount == 0)
        return CLS_ERR_INVALID;

    memset(quote, 0, sizeof(cls_sol_swap_quote_t));
    quote->input_mint = *input_mint;
    quote->output_mint = *output_mint;
    quote->input_amount = amount;
    quote->slippage_bps = slippage_bps;

    /* Simulate swap pricing */
    cls_sol_price_t in_price, out_price;
    cls_solana_get_price(agent, input_mint, &in_price);
    cls_solana_get_price(agent, output_mint, &out_price);

    if (out_price.price_usd > 0.0001) {
        double value_usd = (double)amount * in_price.price_usd / 1e9;
        quote->output_amount = (uint64_t)(value_usd / out_price.price_usd * 1e9);
    } else {
        quote->output_amount = amount; /* 1:1 fallback */
    }

    quote->price_impact = 0.1 + (double)(sol_rand64() % 100) / 1000.0;
    quote->fee_amount = amount / 333; /* ~0.3% fee */
    quote->min_output = (uint64_t)((double)quote->output_amount * (1.0 - slippage_bps / 10000.0));
    strncpy(quote->route, "Raydium CLMM", sizeof(quote->route));

    rpc_record(agent, true, 25);
    return CLS_OK;
}

cls_status_t cls_solana_execute_swap(cls_solana_agent_t *agent,
                                      const cls_sol_swap_quote_t *quote) {
    if (!agent || !quote) return CLS_ERR_INVALID;

    cls_sol_transaction_t tx;
    cls_solana_tx_init(&tx, &agent->wallet.keypair.public_key);

    /* Build swap instruction */
    cls_sol_instruction_t ix;
    memset(&ix, 0, sizeof(ix));
    ix.program_id.bytes[0] = CLS_SOL_RAYDIUM_AMM;
    ix.accounts[0] = agent->wallet.keypair.public_key;
    ix.is_signer[0] = true;
    ix.is_writable[0] = true;
    ix.account_count = 1;

    /* Swap data: op(1) + amount_in(8) + min_out(8) */
    ix.data[0] = 0x09; /* Swap instruction */
    memcpy(&ix.data[1], &quote->input_amount, sizeof(uint64_t));
    memcpy(&ix.data[9], &quote->min_output, sizeof(uint64_t));
    ix.data_len = 17;

    cls_solana_tx_add_instruction(&tx, &ix);

    /* Add memo */
    cls_sol_instruction_t memo_ix;
    cls_solana_ix_memo(&memo_ix, "CLS-SWAP", 8);
    cls_solana_tx_add_instruction(&tx, &memo_ix);

    cls_solana_tx_sign(&tx, &agent->wallet.keypair);
    cls_status_t result = cls_solana_tx_send(agent, &tx);

    if (CLS_IS_OK(result)) {
        agent->wallet.sol_balance -= tx.fee_lamports;

        /* Add output token to wallet if new */
        bool found = false;
        for (uint32_t i = 0; i < agent->wallet.token_count; i++) {
            if (cls_solana_pubkey_equal(&agent->wallet.tokens[i].mint, &quote->output_mint)) {
                agent->wallet.tokens[i].amount += quote->output_amount;
                found = true;
                break;
            }
        }
        if (!found && agent->wallet.token_count < 32) {
            cls_sol_token_account_t *t = &agent->wallet.tokens[agent->wallet.token_count];
            t->mint = quote->output_mint;
            t->owner = agent->wallet.keypair.public_key;
            t->amount = quote->output_amount;
            t->decimals = 9;
            strncpy(t->symbol, "???", sizeof(t->symbol));
            agent->wallet.token_count++;
        }
    }

    return result;
}

/* ============================================================
 * On-Chain Monitoring
 * ============================================================ */

cls_status_t cls_solana_watch_balance(cls_solana_agent_t *agent,
                                       const cls_sol_pubkey_t *target,
                                       double threshold,
                                       void (*callback)(uint32_t, double, double, void *),
                                       void *ctx, uint32_t *watcher_id) {
    if (!agent || !target || !callback) return CLS_ERR_INVALID;
    if (agent->watcher_count >= CLS_SOL_MAX_WATCHERS) return CLS_ERR_OVERFLOW;

    cls_sol_watcher_t *w = &agent->watchers[agent->watcher_count];
    w->watcher_id = agent->next_watcher_id++;
    w->type = CLS_WATCH_BALANCE;
    w->target = *target;
    w->threshold = threshold;
    w->callback = callback;
    w->user_ctx = ctx;
    w->triggered = false;
    w->last_check = sol_time_us();

    if (watcher_id) *watcher_id = w->watcher_id;
    agent->watcher_count++;
    return CLS_OK;
}

cls_status_t cls_solana_watch_token(cls_solana_agent_t *agent,
                                     const cls_sol_pubkey_t *mint,
                                     double threshold,
                                     void (*callback)(uint32_t, double, double, void *),
                                     void *ctx, uint32_t *watcher_id) {
    if (!agent || !mint || !callback) return CLS_ERR_INVALID;
    if (agent->watcher_count >= CLS_SOL_MAX_WATCHERS) return CLS_ERR_OVERFLOW;

    cls_sol_watcher_t *w = &agent->watchers[agent->watcher_count];
    w->watcher_id = agent->next_watcher_id++;
    w->type = CLS_WATCH_TOKEN;
    w->target = *mint;
    w->threshold = threshold;
    w->callback = callback;
    w->user_ctx = ctx;
    w->triggered = false;
    w->last_check = sol_time_us();

    if (watcher_id) *watcher_id = w->watcher_id;
    agent->watcher_count++;
    return CLS_OK;
}

cls_status_t cls_solana_watch_price(cls_solana_agent_t *agent,
                                     const cls_sol_pubkey_t *mint,
                                     double threshold,
                                     void (*callback)(uint32_t, double, double, void *),
                                     void *ctx, uint32_t *watcher_id) {
    if (!agent || !mint || !callback) return CLS_ERR_INVALID;
    if (agent->watcher_count >= CLS_SOL_MAX_WATCHERS) return CLS_ERR_OVERFLOW;

    cls_sol_watcher_t *w = &agent->watchers[agent->watcher_count];
    w->watcher_id = agent->next_watcher_id++;
    w->type = CLS_WATCH_PRICE;
    w->target = *mint;
    w->threshold = threshold;
    w->callback = callback;
    w->user_ctx = ctx;
    w->triggered = false;
    w->last_check = sol_time_us();

    if (watcher_id) *watcher_id = w->watcher_id;
    agent->watcher_count++;
    return CLS_OK;
}

cls_status_t cls_solana_remove_watcher(cls_solana_agent_t *agent, uint32_t watcher_id) {
    if (!agent) return CLS_ERR_INVALID;
    for (uint32_t i = 0; i < agent->watcher_count; i++) {
        if (agent->watchers[i].watcher_id == watcher_id) {
            memmove(&agent->watchers[i], &agent->watchers[i + 1],
                    (agent->watcher_count - i - 1) * sizeof(cls_sol_watcher_t));
            agent->watcher_count--;
            return CLS_OK;
        }
    }
    return CLS_ERR_NOT_FOUND;
}

cls_status_t cls_solana_poll_watchers(cls_solana_agent_t *agent) {
    if (!agent) return CLS_ERR_INVALID;

    for (uint32_t i = 0; i < agent->watcher_count; i++) {
        cls_sol_watcher_t *w = &agent->watchers[i];
        double old_val = 0.0, new_val = 0.0;

        switch (w->type) {
        case CLS_WATCH_BALANCE: {
            cls_sol_account_info_t info;
            cls_solana_get_account_info(agent, &w->target, &info);
            new_val = cls_solana_lamports_to_sol(info.lamports);
            break;
        }
        case CLS_WATCH_PRICE: {
            cls_sol_price_t price;
            cls_solana_get_price(agent, &w->target, &price);
            new_val = price.price_usd;
            break;
        }
        case CLS_WATCH_TOKEN:
        case CLS_WATCH_ACCOUNT:
        case CLS_WATCH_PROGRAM:
            new_val = (double)(sol_rand64() % 1000) / 10.0;
            break;
        }

        if (fabs(new_val - old_val) > w->threshold && !w->triggered) {
            w->triggered = true;
            if (w->callback) w->callback(w->watcher_id, old_val, new_val, w->user_ctx);
        }
        w->last_check = sol_time_us();
    }

    return CLS_OK;
}

/* ============================================================
 * Utilities
 * ============================================================ */

cls_status_t cls_solana_pubkey_from_bytes(cls_sol_pubkey_t *pk, const uint8_t *bytes) {
    if (!pk || !bytes) return CLS_ERR_INVALID;
    memcpy(pk->bytes, bytes, CLS_SOL_PUBKEY_SIZE);
    return CLS_OK;
}

cls_status_t cls_solana_pubkey_from_base58(cls_sol_pubkey_t *pk, const char *base58) {
    if (!pk || !base58) return CLS_ERR_INVALID;

    /* Simple base58 decode for 32-byte pubkey */
    memset(pk->bytes, 0, CLS_SOL_PUBKEY_SIZE);
    size_t len = strlen(base58);

    for (size_t i = 0; i < len; i++) {
        const char *p = strchr(B58_ALPHABET, base58[i]);
        if (!p) return CLS_ERR_INVALID;
        uint32_t val = (uint32_t)(p - B58_ALPHABET);

        /* Multiply existing bytes by 58 and add value */
        uint32_t carry = val;
        for (int j = CLS_SOL_PUBKEY_SIZE - 1; j >= 0; j--) {
            carry += (uint32_t)pk->bytes[j] * 58;
            pk->bytes[j] = (uint8_t)(carry & 0xFF);
            carry >>= 8;
        }
    }

    return CLS_OK;
}

cls_status_t cls_solana_pubkey_to_base58(const cls_sol_pubkey_t *pk, char *out, size_t out_len) {
    if (!pk || !out || out_len < 45) return CLS_ERR_INVALID;

    uint8_t temp[CLS_SOL_PUBKEY_SIZE];
    memcpy(temp, pk->bytes, CLS_SOL_PUBKEY_SIZE);

    char encoded[64];
    int idx = sizeof(encoded) - 1;
    encoded[idx] = '\0';

    while (idx > 0) {
        uint32_t remainder = 0;
        bool all_zero = true;
        for (int i = 0; i < CLS_SOL_PUBKEY_SIZE; i++) {
            uint32_t val = remainder * 256 + temp[i];
            temp[i] = (uint8_t)(val / 58);
            remainder = val % 58;
            if (temp[i] != 0) all_zero = false;
        }
        encoded[--idx] = B58_ALPHABET[remainder];
        if (all_zero) break;
    }

    /* Add leading 1s for leading zero bytes */
    for (int i = 0; i < CLS_SOL_PUBKEY_SIZE && pk->bytes[i] == 0; i++) {
        encoded[--idx] = '1';
    }

    size_t result_len = sizeof(encoded) - 1 - (size_t)idx;
    if (result_len >= out_len) return CLS_ERR_OVERFLOW;

    memcpy(out, &encoded[idx], result_len + 1);
    return CLS_OK;
}

bool cls_solana_pubkey_equal(const cls_sol_pubkey_t *a, const cls_sol_pubkey_t *b) {
    if (!a || !b) return false;
    return memcmp(a->bytes, b->bytes, CLS_SOL_PUBKEY_SIZE) == 0;
}

cls_status_t cls_solana_derive_ata(cls_sol_pubkey_t *ata,
                                    const cls_sol_pubkey_t *wallet,
                                    const cls_sol_pubkey_t *mint) {
    if (!ata || !wallet || !mint) return CLS_ERR_INVALID;

    /* Simplified PDA derivation: hash(wallet || TOKEN_PROGRAM || mint) */
    uint32_t hash = 2166136261u;
    for (int i = 0; i < CLS_SOL_PUBKEY_SIZE; i++) {
        hash ^= wallet->bytes[i]; hash *= 16777619u;
    }
    hash ^= CLS_SOL_TOKEN_PROGRAM; hash *= 16777619u;
    for (int i = 0; i < CLS_SOL_PUBKEY_SIZE; i++) {
        hash ^= mint->bytes[i]; hash *= 16777619u;
    }

    for (int i = 0; i < CLS_SOL_PUBKEY_SIZE; i++) {
        ata->bytes[i] = (uint8_t)(hash & 0xFF);
        hash = hash * 16777619u + (uint32_t)i;
    }

    return CLS_OK;
}

double cls_solana_lamports_to_sol(uint64_t lamports) {
    return (double)lamports / (double)CLS_SOL_LAMPORTS_PER_SOL;
}

uint64_t cls_solana_sol_to_lamports(double sol) {
    return (uint64_t)(sol * (double)CLS_SOL_LAMPORTS_PER_SOL);
}

void cls_solana_get_rpc_stats(const cls_solana_agent_t *agent, cls_sol_rpc_stats_t *stats) {
    if (!agent || !stats) return;
    *stats = agent->rpc_stats;
}

void cls_solana_destroy(cls_solana_agent_t *agent) {
    if (!agent) return;
    free(agent->tx_history);
    free(agent->price_cache);
    memset(agent, 0, sizeof(cls_solana_agent_t));
}
