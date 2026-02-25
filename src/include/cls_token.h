/*
 * ClawLobstars - $CLAW Token Integration Module
 * Tokenomics engine, staking, governance, revenue distribution, agent licensing
 *
 * The $CLAW token powers the ClawLobstars ecosystem:
 * - Stake to run agents on-chain
 * - Govern protocol parameters via voting
 * - Earn revenue share from agent operations
 * - License agent modules with token-gated access
 */

#ifndef CLS_TOKEN_H
#define CLS_TOKEN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Constants
 * ============================================================ */

#define CLS_TOKEN_SYMBOL        "CLAW"
#define CLS_TOKEN_DECIMALS      9
#define CLS_TOKEN_TOTAL_SUPPLY  1000000000000000000ULL  /* 1B * 10^9 */

#define CLS_TOKEN_MAX_STAKERS   256
#define CLS_TOKEN_MAX_PROPOSALS 64
#define CLS_TOKEN_MAX_VOTERS    128
#define CLS_TOKEN_MAX_TIERS     8
#define CLS_TOKEN_MAX_REVENUE   128

/* Supply allocation (basis points, total = 10000) */
#define CLS_ALLOC_COMMUNITY     3000   /* 30% */
#define CLS_ALLOC_STAKING       2000   /* 20% */
#define CLS_ALLOC_DEVELOPMENT   1500   /* 15% */
#define CLS_ALLOC_LIQUIDITY     1500   /* 15% */
#define CLS_ALLOC_TEAM          1000   /* 10% (vested) */
#define CLS_ALLOC_TREASURY      1000   /* 10% */

/* Staking tiers (minimum CLAW tokens, raw with decimals) */
#define CLS_TIER_SCOUT          1000000000000ULL     /* 1,000 CLAW */
#define CLS_TIER_OPERATIVE      10000000000000ULL    /* 10,000 CLAW */
#define CLS_TIER_COMMANDER      100000000000000ULL   /* 100,000 CLAW */
#define CLS_TIER_ADMIRAL        1000000000000000ULL  /* 1,000,000 CLAW */

/* ============================================================
 * Types
 * ============================================================ */

/* Staking tier */
typedef enum {
    CLS_TIER_NONE       = 0,
    CLS_TIER_1_SCOUT    = 1,   /* Basic agent access */
    CLS_TIER_2_OPERATIVE = 2,  /* + Multi-agent, training */
    CLS_TIER_3_COMMANDER = 3,  /* + DeFi, priority execution */
    CLS_TIER_4_ADMIRAL  = 4    /* + Governance, revenue share, all modules */
} cls_staking_tier_t;

/* Proposal status */
typedef enum {
    CLS_PROP_DRAFT      = 0,
    CLS_PROP_ACTIVE     = 1,
    CLS_PROP_PASSED     = 2,
    CLS_PROP_REJECTED   = 3,
    CLS_PROP_EXECUTED   = 4,
    CLS_PROP_CANCELLED  = 5
} cls_proposal_status_t;

/* Proposal type */
typedef enum {
    CLS_PROP_PARAM_CHANGE  = 0,   /* Change protocol parameter */
    CLS_PROP_MODULE_ADD    = 1,   /* Add new module */
    CLS_PROP_FEE_ADJUST    = 2,   /* Adjust fees */
    CLS_PROP_TREASURY      = 3,   /* Treasury spending */
    CLS_PROP_EMERGENCY     = 4    /* Emergency action */
} cls_proposal_type_t;

/* Revenue source */
typedef enum {
    CLS_REV_AGENT_FEE     = 0,   /* Per-agent licensing fee */
    CLS_REV_TX_FEE        = 1,   /* Transaction fee share */
    CLS_REV_SWAP_FEE      = 2,   /* DeFi swap fee */
    CLS_REV_INFERENCE_FEE = 3,   /* Cognitive inference fee */
    CLS_REV_STAKING_REWARD = 4   /* Staking emission */
} cls_revenue_type_t;

/* Vesting schedule type */
typedef enum {
    CLS_VEST_LINEAR      = 0,
    CLS_VEST_CLIFF       = 1,
    CLS_VEST_STEPPED     = 2
} cls_vest_type_t;

/* ============================================================
 * Structures
 * ============================================================ */

/* Token supply breakdown */
typedef struct {
    uint64_t    total_supply;
    uint64_t    circulating;
    uint64_t    staked;
    uint64_t    treasury;
    uint64_t    burned;
    uint64_t    locked_vesting;
    double      price_usd;
    double      market_cap;
} cls_token_supply_t;

/* Staker record */
typedef struct {
    uint32_t            staker_id;
    uint8_t             wallet[32];     /* Pubkey */
    uint64_t            amount_staked;
    cls_staking_tier_t  tier;
    uint64_t            staked_at;
    uint64_t            last_claim;
    uint64_t            rewards_earned;
    uint64_t            rewards_claimed;
    float               apy;            /* Current APY for this staker */
    uint32_t            agent_slots;    /* How many agents can run */
    bool                active;
} cls_staker_t;

/* Governance vote */
typedef struct {
    uint32_t    voter_id;       /* staker_id */
    uint64_t    vote_weight;    /* = staked amount */
    bool        vote_for;       /* true = yes, false = no */
    uint64_t    voted_at;
} cls_vote_t;

/* Governance proposal */
typedef struct {
    uint32_t                proposal_id;
    cls_proposal_type_t     type;
    cls_proposal_status_t   status;
    char                    title[128];
    char                    description[512];
    uint32_t                proposer_id;
    uint64_t                created_at;
    uint64_t                voting_ends;
    uint64_t                quorum;         /* Min votes needed */
    uint64_t                votes_for;
    uint64_t                votes_against;
    cls_vote_t              votes[CLS_TOKEN_MAX_VOTERS];
    uint32_t                vote_count;
    /* Execution payload */
    uint32_t                param_id;
    uint64_t                param_value;
} cls_governance_proposal_t;

/* Revenue record */
typedef struct {
    uint32_t            record_id;
    cls_revenue_type_t  source;
    uint64_t            amount;
    uint64_t            timestamp;
    uint32_t            agent_id;
} cls_revenue_record_t;

/* Agent license */
typedef struct {
    uint32_t            license_id;
    uint32_t            agent_id;
    uint32_t            staker_id;
    cls_staking_tier_t  tier_required;
    uint64_t            fee_per_epoch;   /* CLAW per epoch */
    uint64_t            issued_at;
    uint64_t            expires_at;
    bool                active;
    /* Module access flags */
    bool                access_cognitive;
    bool                access_planning;
    bool                access_defi;
    bool                access_multiagent;
    bool                access_training;
    bool                access_solana;
} cls_agent_license_t;

/* Vesting schedule */
typedef struct {
    uint32_t        schedule_id;
    uint8_t         beneficiary[32];
    uint64_t        total_amount;
    uint64_t        released;
    cls_vest_type_t type;
    uint64_t        start_time;
    uint64_t        cliff_time;
    uint64_t        end_time;
    uint64_t        last_release;
} cls_vesting_t;

/* Staking pool config */
typedef struct {
    float       base_apy;           /* Base APY (e.g. 0.12 = 12%) */
    float       tier_bonus[4];      /* Bonus per tier */
    uint64_t    emission_per_epoch; /* CLAW emitted per epoch */
    uint64_t    epoch_duration_us;  /* Epoch length in µs */
    uint64_t    min_stake_duration; /* Min lock time in µs */
    float       early_unstake_penalty; /* Penalty for early exit (0.0-1.0) */
    uint64_t    total_staked;
    uint32_t    staker_count;
} cls_staking_config_t;

/* ============================================================
 * Token Engine Context
 * ============================================================ */

typedef struct cls_token_engine {
    /* Supply */
    cls_token_supply_t  supply;

    /* Staking */
    cls_staking_config_t staking;
    cls_staker_t        stakers[CLS_TOKEN_MAX_STAKERS];
    uint32_t            next_staker_id;

    /* Governance */
    cls_governance_proposal_t proposals[CLS_TOKEN_MAX_PROPOSALS];
    uint32_t            proposal_count;
    uint32_t            next_proposal_id;

    /* Revenue */
    cls_revenue_record_t revenue[CLS_TOKEN_MAX_REVENUE];
    uint32_t            revenue_count;
    uint32_t            revenue_head;
    uint64_t            total_revenue;
    uint64_t            total_distributed;

    /* Licensing */
    cls_agent_license_t *licenses;
    uint32_t            license_count;
    uint32_t            max_licenses;
    uint32_t            next_license_id;

    /* Vesting */
    cls_vesting_t       *vesting;
    uint32_t            vesting_count;
    uint32_t            max_vesting;

    /* Epoch tracking */
    uint64_t            current_epoch;
    uint64_t            epoch_start;

    /* Integration */
    cls_comm_bus_t      *comm_bus;
} cls_token_engine_t;

/* ============================================================
 * API — Initialization
 * ============================================================ */

cls_status_t cls_token_init(cls_token_engine_t *engine, uint32_t max_licenses,
                             uint32_t max_vesting);
cls_status_t cls_token_set_comm(cls_token_engine_t *engine, cls_comm_bus_t *bus);
cls_status_t cls_token_set_price(cls_token_engine_t *engine, double price_usd);

/* ============================================================
 * API — Supply & Burns
 * ============================================================ */

cls_status_t cls_token_get_supply(const cls_token_engine_t *engine, cls_token_supply_t *supply);
cls_status_t cls_token_burn(cls_token_engine_t *engine, uint64_t amount);

/* ============================================================
 * API — Staking
 * ============================================================ */

cls_status_t cls_token_configure_staking(cls_token_engine_t *engine,
                                          const cls_staking_config_t *config);

cls_status_t cls_token_stake(cls_token_engine_t *engine, const uint8_t *wallet,
                              uint64_t amount, uint32_t *out_staker_id);

cls_status_t cls_token_unstake(cls_token_engine_t *engine, uint32_t staker_id,
                                uint64_t *out_amount, uint64_t *out_penalty);

cls_status_t cls_token_claim_rewards(cls_token_engine_t *engine, uint32_t staker_id,
                                      uint64_t *out_rewards);

cls_staker_t *cls_token_get_staker(cls_token_engine_t *engine, uint32_t staker_id);
cls_staking_tier_t cls_token_get_tier(uint64_t staked_amount);
uint32_t cls_token_get_agent_slots(cls_staking_tier_t tier);
float cls_token_calculate_apy(const cls_token_engine_t *engine, uint32_t staker_id);

/* Process epoch: distribute rewards */
cls_status_t cls_token_process_epoch(cls_token_engine_t *engine);

/* ============================================================
 * API — Governance
 * ============================================================ */

cls_status_t cls_token_propose(cls_token_engine_t *engine, uint32_t proposer_staker_id,
                                cls_proposal_type_t type, const char *title,
                                const char *description, uint64_t voting_duration_us,
                                uint32_t *out_proposal_id);

cls_status_t cls_token_vote(cls_token_engine_t *engine, uint32_t proposal_id,
                             uint32_t voter_staker_id, bool vote_for);

cls_status_t cls_token_finalize_proposal(cls_token_engine_t *engine, uint32_t proposal_id);
cls_status_t cls_token_execute_proposal(cls_token_engine_t *engine, uint32_t proposal_id);

cls_governance_proposal_t *cls_token_get_proposal(cls_token_engine_t *engine, uint32_t proposal_id);

/* ============================================================
 * API — Revenue & Distribution
 * ============================================================ */

cls_status_t cls_token_record_revenue(cls_token_engine_t *engine, cls_revenue_type_t source,
                                       uint64_t amount, uint32_t agent_id);

cls_status_t cls_token_distribute_revenue(cls_token_engine_t *engine);

cls_status_t cls_token_get_revenue_stats(const cls_token_engine_t *engine,
                                          uint64_t *total, uint64_t *distributed,
                                          uint64_t *pending);

/* ============================================================
 * API — Agent Licensing
 * ============================================================ */

cls_status_t cls_token_issue_license(cls_token_engine_t *engine, uint32_t staker_id,
                                      uint32_t agent_id, uint64_t duration_us,
                                      uint32_t *out_license_id);

cls_status_t cls_token_revoke_license(cls_token_engine_t *engine, uint32_t license_id);
cls_status_t cls_token_check_license(const cls_token_engine_t *engine, uint32_t agent_id,
                                      bool *valid, cls_agent_license_t *license);

cls_status_t cls_token_renew_license(cls_token_engine_t *engine, uint32_t license_id,
                                      uint64_t extend_us);

/* ============================================================
 * API — Vesting
 * ============================================================ */

cls_status_t cls_token_create_vesting(cls_token_engine_t *engine, const uint8_t *beneficiary,
                                       uint64_t total_amount, cls_vest_type_t type,
                                       uint64_t cliff_us, uint64_t total_duration_us,
                                       uint32_t *out_schedule_id);

cls_status_t cls_token_release_vested(cls_token_engine_t *engine, uint32_t schedule_id,
                                       uint64_t *out_released);

cls_status_t cls_token_get_vesting_info(const cls_token_engine_t *engine, uint32_t schedule_id,
                                         uint64_t *total, uint64_t *released, uint64_t *available);

/* ============================================================
 * API — Cleanup
 * ============================================================ */

void cls_token_destroy(cls_token_engine_t *engine);

#ifdef __cplusplus
}
#endif

#endif /* CLS_TOKEN_H */
