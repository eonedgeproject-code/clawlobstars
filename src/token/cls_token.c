/*
 * ClawLobstars - $CLAW Token Engine Implementation
 * Staking, governance, revenue distribution, licensing, vesting
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "../include/cls_framework.h"
#include "../include/cls_token.h"

static uint64_t tok_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* ============================================================
 * Initialization
 * ============================================================ */

cls_status_t cls_token_init(cls_token_engine_t *engine, uint32_t max_licenses,
                             uint32_t max_vesting) {
    if (!engine) return CLS_ERR_INVALID;
    memset(engine, 0, sizeof(cls_token_engine_t));

    /* Initial supply */
    engine->supply.total_supply = CLS_TOKEN_TOTAL_SUPPLY;
    engine->supply.treasury = CLS_TOKEN_TOTAL_SUPPLY * CLS_ALLOC_TREASURY / 10000;
    engine->supply.locked_vesting = CLS_TOKEN_TOTAL_SUPPLY * CLS_ALLOC_TEAM / 10000;
    engine->supply.circulating = CLS_TOKEN_TOTAL_SUPPLY - engine->supply.treasury
                                  - engine->supply.locked_vesting;
    engine->supply.price_usd = 0.001;

    /* Default staking config */
    engine->staking.base_apy = 0.12f;
    engine->staking.tier_bonus[0] = 0.0f;
    engine->staking.tier_bonus[1] = 0.02f;
    engine->staking.tier_bonus[2] = 0.05f;
    engine->staking.tier_bonus[3] = 0.10f;
    engine->staking.emission_per_epoch = 1000000000000ULL; /* 1000 CLAW/epoch */
    engine->staking.epoch_duration_us = 86400000000ULL;    /* 24 hours */
    engine->staking.min_stake_duration = 604800000000ULL;  /* 7 days */
    engine->staking.early_unstake_penalty = 0.10f;

    engine->next_staker_id = 1;
    engine->next_proposal_id = 1;
    engine->next_license_id = 1;
    engine->epoch_start = tok_time_us();
    engine->current_epoch = 1;

    if (max_licenses > 0) {
        engine->licenses = (cls_agent_license_t *)calloc(max_licenses, sizeof(cls_agent_license_t));
        if (!engine->licenses) return CLS_ERR_NOMEM;
        engine->max_licenses = max_licenses;
    }

    if (max_vesting > 0) {
        engine->vesting = (cls_vesting_t *)calloc(max_vesting, sizeof(cls_vesting_t));
        if (!engine->vesting) { free(engine->licenses); return CLS_ERR_NOMEM; }
        engine->max_vesting = max_vesting;
    }

    return CLS_OK;
}

cls_status_t cls_token_set_comm(cls_token_engine_t *engine, cls_comm_bus_t *bus) {
    if (!engine) return CLS_ERR_INVALID;
    engine->comm_bus = bus;
    return CLS_OK;
}

cls_status_t cls_token_set_price(cls_token_engine_t *engine, double price_usd) {
    if (!engine) return CLS_ERR_INVALID;
    engine->supply.price_usd = price_usd;
    engine->supply.market_cap = (double)engine->supply.circulating / 1e9 * price_usd;
    return CLS_OK;
}

/* ============================================================
 * Supply
 * ============================================================ */

cls_status_t cls_token_get_supply(const cls_token_engine_t *engine, cls_token_supply_t *supply) {
    if (!engine || !supply) return CLS_ERR_INVALID;
    *supply = engine->supply;
    return CLS_OK;
}

cls_status_t cls_token_burn(cls_token_engine_t *engine, uint64_t amount) {
    if (!engine || amount == 0) return CLS_ERR_INVALID;
    if (amount > engine->supply.circulating) return CLS_ERR_NOMEM;

    engine->supply.circulating -= amount;
    engine->supply.burned += amount;
    engine->supply.total_supply -= amount;

    if (engine->comm_bus) {
        cls_comm_broadcast(engine->comm_bus, CLS_MSG_SYSTEM, "BURN", 5);
    }
    return CLS_OK;
}

/* ============================================================
 * Staking
 * ============================================================ */

cls_staking_tier_t cls_token_get_tier(uint64_t staked_amount) {
    if (staked_amount >= CLS_TIER_ADMIRAL)   return CLS_TIER_4_ADMIRAL;
    if (staked_amount >= CLS_TIER_COMMANDER) return CLS_TIER_3_COMMANDER;
    if (staked_amount >= CLS_TIER_OPERATIVE) return CLS_TIER_2_OPERATIVE;
    if (staked_amount >= CLS_TIER_SCOUT)     return CLS_TIER_1_SCOUT;
    return CLS_TIER_NONE;
}

uint32_t cls_token_get_agent_slots(cls_staking_tier_t tier) {
    switch (tier) {
    case CLS_TIER_1_SCOUT:     return 1;
    case CLS_TIER_2_OPERATIVE: return 5;
    case CLS_TIER_3_COMMANDER: return 20;
    case CLS_TIER_4_ADMIRAL:   return 100;
    default:                   return 0;
    }
}

cls_status_t cls_token_configure_staking(cls_token_engine_t *engine,
                                          const cls_staking_config_t *config) {
    if (!engine || !config) return CLS_ERR_INVALID;
    engine->staking = *config;
    return CLS_OK;
}

cls_status_t cls_token_stake(cls_token_engine_t *engine, const uint8_t *wallet,
                              uint64_t amount, uint32_t *out_staker_id) {
    if (!engine || !wallet || amount == 0) return CLS_ERR_INVALID;
    if (engine->staking.staker_count >= CLS_TOKEN_MAX_STAKERS) return CLS_ERR_OVERFLOW;

    /* Check if wallet already staking */
    for (uint32_t i = 0; i < CLS_TOKEN_MAX_STAKERS; i++) {
        if (engine->stakers[i].active &&
            memcmp(engine->stakers[i].wallet, wallet, 32) == 0) {
            /* Add to existing stake */
            engine->stakers[i].amount_staked += amount;
            engine->stakers[i].tier = cls_token_get_tier(engine->stakers[i].amount_staked);
            engine->stakers[i].agent_slots = cls_token_get_agent_slots(engine->stakers[i].tier);
            engine->staking.total_staked += amount;
            engine->supply.staked += amount;
            engine->supply.circulating -= amount;
            if (out_staker_id) *out_staker_id = engine->stakers[i].staker_id;
            return CLS_OK;
        }
    }

    /* New staker */
    for (uint32_t i = 0; i < CLS_TOKEN_MAX_STAKERS; i++) {
        if (!engine->stakers[i].active) {
            cls_staker_t *s = &engine->stakers[i];
            s->staker_id = engine->next_staker_id++;
            memcpy(s->wallet, wallet, 32);
            s->amount_staked = amount;
            s->tier = cls_token_get_tier(amount);
            s->staked_at = tok_time_us();
            s->last_claim = s->staked_at;
            s->agent_slots = cls_token_get_agent_slots(s->tier);
            s->active = true;

            engine->staking.total_staked += amount;
            engine->staking.staker_count++;
            engine->supply.staked += amount;
            engine->supply.circulating -= amount;

            if (out_staker_id) *out_staker_id = s->staker_id;
            return CLS_OK;
        }
    }
    return CLS_ERR_OVERFLOW;
}

cls_status_t cls_token_unstake(cls_token_engine_t *engine, uint32_t staker_id,
                                uint64_t *out_amount, uint64_t *out_penalty) {
    if (!engine) return CLS_ERR_INVALID;

    cls_staker_t *s = cls_token_get_staker(engine, staker_id);
    if (!s) return CLS_ERR_NOT_FOUND;

    uint64_t penalty = 0;
    uint64_t now = tok_time_us();

    /* Early unstake penalty */
    if ((now - s->staked_at) < engine->staking.min_stake_duration) {
        penalty = (uint64_t)((float)s->amount_staked * engine->staking.early_unstake_penalty);
    }

    uint64_t returned = s->amount_staked - penalty;

    engine->staking.total_staked -= s->amount_staked;
    engine->staking.staker_count--;
    engine->supply.staked -= s->amount_staked;
    engine->supply.circulating += returned;
    if (penalty > 0) engine->supply.burned += penalty;

    if (out_amount) *out_amount = returned;
    if (out_penalty) *out_penalty = penalty;

    s->active = false;
    s->amount_staked = 0;
    s->tier = CLS_TIER_NONE;
    return CLS_OK;
}

float cls_token_calculate_apy(const cls_token_engine_t *engine, uint32_t staker_id) {
    if (!engine) return 0.0f;
    for (uint32_t i = 0; i < CLS_TOKEN_MAX_STAKERS; i++) {
        if (engine->stakers[i].staker_id == staker_id && engine->stakers[i].active) {
            int tier_idx = (int)engine->stakers[i].tier - 1;
            if (tier_idx < 0) tier_idx = 0;
            if (tier_idx > 3) tier_idx = 3;
            return engine->staking.base_apy + engine->staking.tier_bonus[tier_idx];
        }
    }
    return 0.0f;
}

cls_status_t cls_token_claim_rewards(cls_token_engine_t *engine, uint32_t staker_id,
                                      uint64_t *out_rewards) {
    if (!engine) return CLS_ERR_INVALID;

    cls_staker_t *s = cls_token_get_staker(engine, staker_id);
    if (!s) return CLS_ERR_NOT_FOUND;

    uint64_t pending = s->rewards_earned - s->rewards_claimed;
    if (pending == 0) {
        if (out_rewards) *out_rewards = 0;
        return CLS_OK;
    }

    s->rewards_claimed += pending;
    s->last_claim = tok_time_us();
    engine->supply.circulating += pending;

    if (out_rewards) *out_rewards = pending;
    return CLS_OK;
}

cls_staker_t *cls_token_get_staker(cls_token_engine_t *engine, uint32_t staker_id) {
    if (!engine) return NULL;
    for (uint32_t i = 0; i < CLS_TOKEN_MAX_STAKERS; i++) {
        if (engine->stakers[i].staker_id == staker_id && engine->stakers[i].active)
            return &engine->stakers[i];
    }
    return NULL;
}

cls_status_t cls_token_process_epoch(cls_token_engine_t *engine) {
    if (!engine) return CLS_ERR_INVALID;

    uint64_t now = tok_time_us();
    if ((now - engine->epoch_start) < engine->staking.epoch_duration_us)
        return CLS_OK; /* Not time yet */

    engine->current_epoch++;
    engine->epoch_start = now;

    if (engine->staking.total_staked == 0) return CLS_OK;

    /* Distribute emission proportionally */
    uint64_t emission = engine->staking.emission_per_epoch;

    for (uint32_t i = 0; i < CLS_TOKEN_MAX_STAKERS; i++) {
        if (!engine->stakers[i].active) continue;
        cls_staker_t *s = &engine->stakers[i];

        float apy = cls_token_calculate_apy(engine, s->staker_id);
        s->apy = apy;

        /* Proportional share of emission */
        double share = (double)s->amount_staked / (double)engine->staking.total_staked;
        uint64_t reward = (uint64_t)(share * (double)emission);

        /* Apply APY bonus */
        reward = (uint64_t)((float)reward * (1.0f + apy));

        s->rewards_earned += reward;
    }

    if (engine->comm_bus) {
        cls_comm_broadcast(engine->comm_bus, CLS_MSG_SYSTEM, "EPOCH", 6);
    }

    return CLS_OK;
}

/* ============================================================
 * Governance
 * ============================================================ */

cls_status_t cls_token_propose(cls_token_engine_t *engine, uint32_t proposer_staker_id,
                                cls_proposal_type_t type, const char *title,
                                const char *description, uint64_t voting_duration_us,
                                uint32_t *out_proposal_id) {
    if (!engine || !title) return CLS_ERR_INVALID;
    if (engine->proposal_count >= CLS_TOKEN_MAX_PROPOSALS) return CLS_ERR_OVERFLOW;

    /* Only ADMIRAL tier can propose */
    cls_staker_t *proposer = cls_token_get_staker(engine, proposer_staker_id);
    if (!proposer || proposer->tier < CLS_TIER_4_ADMIRAL) return CLS_ERR_SECURITY;

    cls_governance_proposal_t *p = &engine->proposals[engine->proposal_count];
    memset(p, 0, sizeof(cls_governance_proposal_t));

    p->proposal_id = engine->next_proposal_id++;
    p->type = type;
    p->status = CLS_PROP_ACTIVE;
    p->proposer_id = proposer_staker_id;
    p->created_at = tok_time_us();
    p->voting_ends = p->created_at + voting_duration_us;
    p->quorum = engine->staking.total_staked / 10; /* 10% quorum */

    strncpy(p->title, title, sizeof(p->title) - 1);
    if (description) strncpy(p->description, description, sizeof(p->description) - 1);

    engine->proposal_count++;
    if (out_proposal_id) *out_proposal_id = p->proposal_id;
    return CLS_OK;
}

cls_status_t cls_token_vote(cls_token_engine_t *engine, uint32_t proposal_id,
                             uint32_t voter_staker_id, bool vote_for) {
    if (!engine) return CLS_ERR_INVALID;

    cls_governance_proposal_t *p = cls_token_get_proposal(engine, proposal_id);
    if (!p || p->status != CLS_PROP_ACTIVE) return CLS_ERR_STATE;

    cls_staker_t *voter = cls_token_get_staker(engine, voter_staker_id);
    if (!voter) return CLS_ERR_NOT_FOUND;

    /* Check if already voted */
    for (uint32_t i = 0; i < p->vote_count; i++) {
        if (p->votes[i].voter_id == voter_staker_id)
            return CLS_ERR_INVALID; /* Already voted */
    }

    if (p->vote_count >= CLS_TOKEN_MAX_VOTERS) return CLS_ERR_OVERFLOW;

    cls_vote_t *v = &p->votes[p->vote_count];
    v->voter_id = voter_staker_id;
    v->vote_weight = voter->amount_staked;
    v->vote_for = vote_for;
    v->voted_at = tok_time_us();

    if (vote_for) p->votes_for += voter->amount_staked;
    else p->votes_against += voter->amount_staked;

    p->vote_count++;
    return CLS_OK;
}

cls_status_t cls_token_finalize_proposal(cls_token_engine_t *engine, uint32_t proposal_id) {
    if (!engine) return CLS_ERR_INVALID;

    cls_governance_proposal_t *p = cls_token_get_proposal(engine, proposal_id);
    if (!p || p->status != CLS_PROP_ACTIVE) return CLS_ERR_STATE;

    uint64_t total_votes = p->votes_for + p->votes_against;

    if (total_votes < p->quorum) {
        p->status = CLS_PROP_REJECTED; /* Didn't meet quorum */
    } else if (p->votes_for > p->votes_against) {
        p->status = CLS_PROP_PASSED;
    } else {
        p->status = CLS_PROP_REJECTED;
    }

    return CLS_OK;
}

cls_status_t cls_token_execute_proposal(cls_token_engine_t *engine, uint32_t proposal_id) {
    if (!engine) return CLS_ERR_INVALID;

    cls_governance_proposal_t *p = cls_token_get_proposal(engine, proposal_id);
    if (!p || p->status != CLS_PROP_PASSED) return CLS_ERR_STATE;

    /* Execute based on type */
    switch (p->type) {
    case CLS_PROP_FEE_ADJUST:
        /* Apply new fee parameter */
        break;
    case CLS_PROP_TREASURY:
        /* Process treasury spending */
        if (p->param_value <= engine->supply.treasury) {
            engine->supply.treasury -= p->param_value;
            engine->supply.circulating += p->param_value;
        }
        break;
    case CLS_PROP_PARAM_CHANGE:
    case CLS_PROP_MODULE_ADD:
    case CLS_PROP_EMERGENCY:
        break;
    }

    p->status = CLS_PROP_EXECUTED;

    if (engine->comm_bus) {
        cls_comm_broadcast(engine->comm_bus, CLS_MSG_SYSTEM, "GOV_EXEC", 9);
    }
    return CLS_OK;
}

cls_governance_proposal_t *cls_token_get_proposal(cls_token_engine_t *engine, uint32_t proposal_id) {
    if (!engine) return NULL;
    for (uint32_t i = 0; i < engine->proposal_count; i++) {
        if (engine->proposals[i].proposal_id == proposal_id)
            return &engine->proposals[i];
    }
    return NULL;
}

/* ============================================================
 * Revenue & Distribution
 * ============================================================ */

cls_status_t cls_token_record_revenue(cls_token_engine_t *engine, cls_revenue_type_t source,
                                       uint64_t amount, uint32_t agent_id) {
    if (!engine || amount == 0) return CLS_ERR_INVALID;

    uint32_t idx = engine->revenue_head;
    cls_revenue_record_t *r = &engine->revenue[idx];
    r->record_id = engine->revenue_count + 1;
    r->source = source;
    r->amount = amount;
    r->timestamp = tok_time_us();
    r->agent_id = agent_id;

    engine->revenue_head = (engine->revenue_head + 1) % CLS_TOKEN_MAX_REVENUE;
    if (engine->revenue_count < CLS_TOKEN_MAX_REVENUE) engine->revenue_count++;
    engine->total_revenue += amount;

    return CLS_OK;
}

cls_status_t cls_token_distribute_revenue(cls_token_engine_t *engine) {
    if (!engine) return CLS_ERR_INVALID;

    uint64_t pending = engine->total_revenue - engine->total_distributed;
    if (pending == 0 || engine->staking.total_staked == 0) return CLS_OK;

    /* 70% to stakers, 20% to treasury, 10% burned */
    uint64_t staker_share = pending * 70 / 100;
    uint64_t treasury_share = pending * 20 / 100;
    uint64_t burn_share = pending - staker_share - treasury_share;

    /* Distribute to stakers proportionally */
    for (uint32_t i = 0; i < CLS_TOKEN_MAX_STAKERS; i++) {
        if (!engine->stakers[i].active) continue;
        double share = (double)engine->stakers[i].amount_staked /
                       (double)engine->staking.total_staked;
        uint64_t reward = (uint64_t)(share * (double)staker_share);
        engine->stakers[i].rewards_earned += reward;
    }

    engine->supply.treasury += treasury_share;
    cls_token_burn(engine, burn_share);

    engine->total_distributed = engine->total_revenue;
    return CLS_OK;
}

cls_status_t cls_token_get_revenue_stats(const cls_token_engine_t *engine,
                                          uint64_t *total, uint64_t *distributed,
                                          uint64_t *pending) {
    if (!engine) return CLS_ERR_INVALID;
    if (total) *total = engine->total_revenue;
    if (distributed) *distributed = engine->total_distributed;
    if (pending) *pending = engine->total_revenue - engine->total_distributed;
    return CLS_OK;
}

/* ============================================================
 * Agent Licensing
 * ============================================================ */

cls_status_t cls_token_issue_license(cls_token_engine_t *engine, uint32_t staker_id,
                                      uint32_t agent_id, uint64_t duration_us,
                                      uint32_t *out_license_id) {
    if (!engine || !engine->licenses) return CLS_ERR_INVALID;
    if (engine->license_count >= engine->max_licenses) return CLS_ERR_OVERFLOW;

    cls_staker_t *s = cls_token_get_staker(engine, staker_id);
    if (!s) return CLS_ERR_NOT_FOUND;
    if (s->tier == CLS_TIER_NONE) return CLS_ERR_SECURITY;

    /* Check agent slot availability */
    uint32_t used_slots = 0;
    for (uint32_t i = 0; i < engine->license_count; i++) {
        if (engine->licenses[i].staker_id == staker_id && engine->licenses[i].active)
            used_slots++;
    }
    if (used_slots >= s->agent_slots) return CLS_ERR_OVERFLOW;

    cls_agent_license_t *lic = &engine->licenses[engine->license_count];
    memset(lic, 0, sizeof(cls_agent_license_t));

    lic->license_id = engine->next_license_id++;
    lic->agent_id = agent_id;
    lic->staker_id = staker_id;
    lic->tier_required = s->tier;
    lic->issued_at = tok_time_us();
    lic->expires_at = lic->issued_at + duration_us;
    lic->active = true;

    /* Module access based on tier */
    lic->access_cognitive = true; /* All tiers */
    lic->access_planning = (s->tier >= CLS_TIER_1_SCOUT);
    lic->access_multiagent = (s->tier >= CLS_TIER_2_OPERATIVE);
    lic->access_training = (s->tier >= CLS_TIER_2_OPERATIVE);
    lic->access_defi = (s->tier >= CLS_TIER_3_COMMANDER);
    lic->access_solana = (s->tier >= CLS_TIER_3_COMMANDER);

    /* Calculate fee */
    lic->fee_per_epoch = 100000000000ULL; /* 100 CLAW base */
    if (s->tier >= CLS_TIER_3_COMMANDER) lic->fee_per_epoch /= 2; /* 50% discount */
    if (s->tier >= CLS_TIER_4_ADMIRAL) lic->fee_per_epoch = 0; /* Free */

    engine->license_count++;
    if (out_license_id) *out_license_id = lic->license_id;
    return CLS_OK;
}

cls_status_t cls_token_revoke_license(cls_token_engine_t *engine, uint32_t license_id) {
    if (!engine) return CLS_ERR_INVALID;
    for (uint32_t i = 0; i < engine->license_count; i++) {
        if (engine->licenses[i].license_id == license_id) {
            engine->licenses[i].active = false;
            return CLS_OK;
        }
    }
    return CLS_ERR_NOT_FOUND;
}

cls_status_t cls_token_check_license(const cls_token_engine_t *engine, uint32_t agent_id,
                                      bool *valid, cls_agent_license_t *license) {
    if (!engine || !valid) return CLS_ERR_INVALID;

    *valid = false;
    uint64_t now = tok_time_us();

    for (uint32_t i = 0; i < engine->license_count; i++) {
        if (engine->licenses[i].agent_id == agent_id &&
            engine->licenses[i].active &&
            now < engine->licenses[i].expires_at) {
            *valid = true;
            if (license) *license = engine->licenses[i];
            return CLS_OK;
        }
    }
    return CLS_OK;
}

cls_status_t cls_token_renew_license(cls_token_engine_t *engine, uint32_t license_id,
                                      uint64_t extend_us) {
    if (!engine) return CLS_ERR_INVALID;
    for (uint32_t i = 0; i < engine->license_count; i++) {
        if (engine->licenses[i].license_id == license_id) {
            engine->licenses[i].expires_at += extend_us;
            engine->licenses[i].active = true;
            return CLS_OK;
        }
    }
    return CLS_ERR_NOT_FOUND;
}

/* ============================================================
 * Vesting
 * ============================================================ */

cls_status_t cls_token_create_vesting(cls_token_engine_t *engine, const uint8_t *beneficiary,
                                       uint64_t total_amount, cls_vest_type_t type,
                                       uint64_t cliff_us, uint64_t total_duration_us,
                                       uint32_t *out_schedule_id) {
    if (!engine || !engine->vesting || !beneficiary || total_amount == 0)
        return CLS_ERR_INVALID;
    if (engine->vesting_count >= engine->max_vesting) return CLS_ERR_OVERFLOW;

    cls_vesting_t *v = &engine->vesting[engine->vesting_count];
    memset(v, 0, sizeof(cls_vesting_t));

    v->schedule_id = engine->vesting_count + 1;
    memcpy(v->beneficiary, beneficiary, 32);
    v->total_amount = total_amount;
    v->type = type;
    v->start_time = tok_time_us();
    v->cliff_time = v->start_time + cliff_us;
    v->end_time = v->start_time + total_duration_us;

    engine->vesting_count++;
    if (out_schedule_id) *out_schedule_id = v->schedule_id;
    return CLS_OK;
}

cls_status_t cls_token_release_vested(cls_token_engine_t *engine, uint32_t schedule_id,
                                       uint64_t *out_released) {
    if (!engine || !engine->vesting) return CLS_ERR_INVALID;

    for (uint32_t i = 0; i < engine->vesting_count; i++) {
        if (engine->vesting[i].schedule_id != schedule_id) continue;
        cls_vesting_t *v = &engine->vesting[i];

        uint64_t now = tok_time_us();

        /* Before cliff: nothing */
        if (now < v->cliff_time) {
            if (out_released) *out_released = 0;
            return CLS_OK;
        }

        /* Calculate vested amount */
        uint64_t vested;
        if (now >= v->end_time) {
            vested = v->total_amount;
        } else {
            double progress = (double)(now - v->start_time) / (double)(v->end_time - v->start_time);
            switch (v->type) {
            case CLS_VEST_LINEAR:
                vested = (uint64_t)((double)v->total_amount * progress);
                break;
            case CLS_VEST_CLIFF:
                vested = (now >= v->cliff_time) ? v->total_amount : 0;
                break;
            case CLS_VEST_STEPPED:
                /* 25% per quarter */
                vested = (uint64_t)((double)v->total_amount * ((int)(progress * 4) / 4.0));
                break;
            default:
                vested = 0;
            }
        }

        uint64_t releasable = vested - v->released;
        v->released += releasable;
        v->last_release = now;

        engine->supply.locked_vesting -= releasable;
        engine->supply.circulating += releasable;

        if (out_released) *out_released = releasable;
        return CLS_OK;
    }
    return CLS_ERR_NOT_FOUND;
}

cls_status_t cls_token_get_vesting_info(const cls_token_engine_t *engine, uint32_t schedule_id,
                                         uint64_t *total, uint64_t *released, uint64_t *available) {
    if (!engine || !engine->vesting) return CLS_ERR_INVALID;
    for (uint32_t i = 0; i < engine->vesting_count; i++) {
        if (engine->vesting[i].schedule_id == schedule_id) {
            if (total) *total = engine->vesting[i].total_amount;
            if (released) *released = engine->vesting[i].released;
            if (available) *available = engine->vesting[i].total_amount - engine->vesting[i].released;
            return CLS_OK;
        }
    }
    return CLS_ERR_NOT_FOUND;
}

/* ============================================================
 * Cleanup
 * ============================================================ */

void cls_token_destroy(cls_token_engine_t *engine) {
    if (!engine) return;
    free(engine->licenses);
    free(engine->vesting);
    memset(engine, 0, sizeof(cls_token_engine_t));
}
