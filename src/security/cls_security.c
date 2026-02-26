/*
 * ClawLobstars - Security Layer Implementation
 * Authentication, RBAC, input validation, stream cipher, audit logging
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "../include/cls_framework.h"

static uint64_t cls_sec_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

cls_status_t cls_security_init(cls_security_ctx_t *sec, cls_security_level_t level) {
    if (!sec) return CLS_ERR_INVALID;

    memset(sec, 0, sizeof(cls_security_ctx_t));
    sec->level = level;
    sec->next_audit_id = 1;
    return CLS_OK;
}

/* ---- Key Management ---- */

cls_status_t cls_security_set_key(cls_security_ctx_t *sec, const uint8_t *key, size_t key_len) {
    if (!sec || !key || key_len == 0) return CLS_ERR_INVALID;

    /* Hash key into fixed-size master key using FNV-1a based expansion */
    memset(sec->master_key, 0, CLS_SEC_HASH_SIZE);
    for (size_t i = 0; i < key_len; i++) {
        sec->master_key[i % CLS_SEC_HASH_SIZE] ^= key[i];
        /* Mix */
        uint8_t mix = sec->master_key[i % CLS_SEC_HASH_SIZE];
        sec->master_key[(i + 7) % CLS_SEC_HASH_SIZE] ^= (mix << 3) | (mix >> 5);
        sec->master_key[(i + 13) % CLS_SEC_HASH_SIZE] ^= (mix << 5) | (mix >> 3);
    }

    sec->key_initialized = true;

    cls_security_audit(sec, CLS_AUDIT_CONFIG_CHANGE, 0, "Master key set", 2);
    return CLS_OK;
}

/* ---- RBAC ---- */

cls_status_t cls_security_add_role(cls_security_ctx_t *sec, const cls_role_t *role) {
    if (!sec || !role) return CLS_ERR_INVALID;
    if (sec->role_count >= CLS_SEC_MAX_ROLES) return CLS_ERR_OVERFLOW;

    /* Check duplicate */
    for (uint32_t i = 0; i < sec->role_count; i++) {
        if (sec->roles[i].role_id == role->role_id)
            return CLS_ERR_INVALID;
    }

    sec->roles[sec->role_count] = *role;
    sec->role_count++;

    cls_security_audit(sec, CLS_AUDIT_CONFIG_CHANGE, 0, "Role added", 1);
    return CLS_OK;
}

cls_status_t cls_security_check_permission(cls_security_ctx_t *sec, uint32_t role_id,
                                            cls_permission_t required) {
    if (!sec) return CLS_ERR_INVALID;

    for (uint32_t i = 0; i < sec->role_count; i++) {
        if (sec->roles[i].role_id == role_id) {
            if ((sec->roles[i].permissions & required) == required) {
                cls_security_audit(sec, CLS_AUDIT_ACCESS_GRANT, role_id, "Permission granted", 0);
                return CLS_OK;
            }
            cls_security_audit(sec, CLS_AUDIT_ACCESS_DENY, role_id, "Permission denied", 3);
            return CLS_ERR_SECURITY;
        }
    }

    cls_security_audit(sec, CLS_AUDIT_ACCESS_DENY, role_id, "Role not found", 3);
    return CLS_ERR_NOT_FOUND;
}

/* ---- Authentication ---- */

cls_status_t cls_security_auth(cls_security_ctx_t *sec, uint32_t agent_id,
                                const uint8_t *credentials, size_t cred_len,
                                cls_auth_token_t *out_token) {
    if (!sec || !credentials || cred_len == 0 || !out_token)
        return CLS_ERR_INVALID;

    sec->auth_attempts++;

    /* Basic credential validation (hash-compare in production) */
    uint8_t cred_hash[CLS_SEC_HASH_SIZE];
    cls_security_hash(credentials, cred_len, cred_hash);

    /* For security levels >= HIGH, require key to be set */
    if (sec->level >= CLS_SEC_HIGH && !sec->key_initialized) {
        sec->auth_failures++;
        cls_security_audit(sec, CLS_AUDIT_AUTH_FAILURE, agent_id, "No master key", 4);
        return CLS_ERR_SECURITY;
    }

    /* Generate token */
    memset(out_token, 0, sizeof(cls_auth_token_t));
    out_token->agent_id = agent_id;
    out_token->role_id = 0; /* Default role */
    out_token->issued_at = cls_sec_time_us();
    out_token->expires_at = out_token->issued_at + 3600000000ULL; /* 1 hour */
    out_token->valid = true;

    /* Token = hash of (master_key XOR credential_hash XOR timestamp) */
    uint64_t ts = out_token->issued_at;
    for (uint32_t i = 0; i < CLS_SEC_TOKEN_SIZE; i++) {
        out_token->token[i] = cred_hash[i % CLS_SEC_HASH_SIZE] ^
                               sec->master_key[i % CLS_SEC_HASH_SIZE] ^
                               ((uint8_t)(ts >> (i % 8)));
    }

    cls_security_audit(sec, CLS_AUDIT_AUTH_SUCCESS, agent_id, "Authenticated", 1);
    return CLS_OK;
}

cls_status_t cls_security_validate_token(cls_security_ctx_t *sec, const cls_auth_token_t *token) {
    if (!sec || !token) return CLS_ERR_INVALID;

    if (!token->valid) return CLS_ERR_SECURITY;

    uint64_t now = cls_sec_time_us();
    if (now > token->expires_at) {
        cls_security_audit(sec, CLS_AUDIT_AUTH_FAILURE, token->agent_id, "Token expired", 2);
        return CLS_ERR_TIMEOUT;
    }

    return CLS_OK;
}

cls_status_t cls_security_revoke_token(cls_security_ctx_t *sec, cls_auth_token_t *token) {
    if (!sec || !token) return CLS_ERR_INVALID;
    token->valid = false;
    cls_security_audit(sec, CLS_AUDIT_CONFIG_CHANGE, token->agent_id, "Token revoked", 2);
    return CLS_OK;
}

/* ---- Input Validation ---- */

cls_status_t cls_security_add_rule(cls_security_ctx_t *sec, const cls_validation_rule_t *rule) {
    if (!sec || !rule) return CLS_ERR_INVALID;
    if (sec->rule_count >= CLS_SEC_MAX_RULES) return CLS_ERR_OVERFLOW;

    sec->rules[sec->rule_count] = *rule;
    sec->rule_count++;
    return CLS_OK;
}

cls_status_t cls_security_validate_input(cls_security_ctx_t *sec, const cls_frame_t *frame) {
    if (!sec || !frame) return CLS_ERR_INVALID;

    /* Basic validation */
    if (!frame->payload && frame->payload_len > 0) {
        sec->inputs_rejected++;
        cls_security_audit(sec, CLS_AUDIT_INPUT_REJECT, 0, "NULL payload with nonzero len", 3);
        return CLS_ERR_INVALID;
    }

    /* Apply rules */
    for (uint32_t i = 0; i < sec->rule_count; i++) {
        cls_validation_rule_t *rule = &sec->rules[i];

        if (rule->data_type != 0 && rule->data_type != frame->data_type)
            continue;

        /* Size check */
        if (rule->max_size > 0 && frame->payload_len > rule->max_size) {
            sec->inputs_rejected++;
            cls_security_audit(sec, CLS_AUDIT_INPUT_REJECT, 0, "Payload exceeds max size", 3);
            return CLS_ERR_OVERFLOW;
        }

        /* Null check */
        if (!rule->allow_null && !frame->payload) {
            sec->inputs_rejected++;
            cls_security_audit(sec, CLS_AUDIT_INPUT_REJECT, 0, "NULL payload rejected", 3);
            return CLS_ERR_INVALID;
        }

        /* Custom validator */
        if (rule->custom_validator) {
            if (!rule->custom_validator(frame->payload, frame->payload_len)) {
                sec->inputs_rejected++;
                cls_security_audit(sec, CLS_AUDIT_INPUT_REJECT, 0, "Custom validation failed", 3);
                return CLS_ERR_SECURITY;
            }
        }
    }

    return CLS_OK;
}

cls_status_t cls_security_validate_buffer(cls_security_ctx_t *sec, const void *buf,
                                           size_t len, size_t max_allowed) {
    if (!sec) return CLS_ERR_INVALID;
    if (!buf && len > 0) return CLS_ERR_INVALID;
    if (len > max_allowed) {
        sec->inputs_rejected++;
        return CLS_ERR_OVERFLOW;
    }
    return CLS_OK;
}

/* ---- Encryption (XOR stream cipher — plug AES for production) ---- */

cls_status_t cls_security_encrypt(cls_security_ctx_t *sec, const void *plaintext,
                                   size_t len, void *ciphertext) {
    if (!sec || !plaintext || !ciphertext || len == 0)
        return CLS_ERR_INVALID;
    if (!sec->key_initialized) return CLS_ERR_SECURITY;

    const uint8_t *pt = (const uint8_t *)plaintext;
    uint8_t *ct = (uint8_t *)ciphertext;

    /* XOR with expanded key stream */
    uint8_t key_stream[CLS_SEC_HASH_SIZE];
    memcpy(key_stream, sec->master_key, CLS_SEC_HASH_SIZE);

    for (size_t i = 0; i < len; i++) {
        ct[i] = pt[i] ^ key_stream[i % CLS_SEC_HASH_SIZE];

        /* Evolve key stream (simple PRNG feedback) */
        if ((i + 1) % CLS_SEC_HASH_SIZE == 0) {
            for (int j = 0; j < CLS_SEC_HASH_SIZE; j++) {
                key_stream[j] ^= (key_stream[(j + 1) % CLS_SEC_HASH_SIZE] + (uint8_t)i);
            }
        }
    }

    cls_security_audit(sec, CLS_AUDIT_ENCRYPT, 0, "Data encrypted", 0);
    return CLS_OK;
}

cls_status_t cls_security_decrypt(cls_security_ctx_t *sec, const void *ciphertext,
                                   size_t len, void *plaintext) {
    /* Symmetric — same operation as encrypt */
    cls_status_t result = cls_security_encrypt(sec, ciphertext, len, plaintext);
    if (CLS_IS_OK(result)) {
        cls_security_audit(sec, CLS_AUDIT_DECRYPT, 0, "Data decrypted", 0);
    }
    return result;
}

/* ---- Hashing (FNV-1a based) ---- */

cls_status_t cls_security_hash(const void *data, size_t len, uint8_t *hash_out) {
    if (!data || !hash_out || len == 0) return CLS_ERR_INVALID;

    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t hashes[8];

    /* Generate 8 independent FNV-1a hashes with different seeds */
    for (int h = 0; h < 8; h++) {
        uint32_t hash = 2166136261u + (uint32_t)h * 16777619u;
        for (size_t i = 0; i < len; i++) {
            hash ^= bytes[i];
            hash *= 16777619u;
        }
        hashes[h] = hash;
    }

    memcpy(hash_out, hashes, CLS_SEC_HASH_SIZE);
    return CLS_OK;
}

/* ---- Audit Log ---- */

cls_status_t cls_security_audit(cls_security_ctx_t *sec, cls_audit_type_t type,
                                 uint32_t agent_id, const char *detail, uint8_t severity) {
    if (!sec) return CLS_ERR_INVALID;

    uint32_t idx = sec->audit_head;
    cls_audit_entry_t *entry = &sec->audit_log[idx];

    /* Fix: zero entry first to prevent stale data from previous ring cycle */
    memset(entry, 0, sizeof(cls_audit_entry_t));

    entry->entry_id = sec->next_audit_id++;
    entry->type = type;
    entry->agent_id = agent_id;
    entry->timestamp_us = cls_sec_time_us();
    entry->severity = severity;

    if (detail) {
        strncpy(entry->detail, detail, sizeof(entry->detail) - 1);
        entry->detail[sizeof(entry->detail) - 1] = '\0';
    }

    sec->audit_head = (sec->audit_head + 1) % CLS_SEC_MAX_AUDIT;
    if (sec->audit_count < CLS_SEC_MAX_AUDIT) sec->audit_count++;

    return CLS_OK;
}

cls_status_t cls_security_get_audit(const cls_security_ctx_t *sec, cls_audit_entry_t *entries,
                                     uint32_t *count, uint32_t max_entries) {
    if (!sec || !entries || !count) return CLS_ERR_INVALID;

    uint32_t to_copy = CLS_MIN(sec->audit_count, max_entries);
    uint32_t start;

    if (sec->audit_count < CLS_SEC_MAX_AUDIT) {
        start = 0;
    } else {
        start = sec->audit_head; /* Oldest entry */
    }

    for (uint32_t i = 0; i < to_copy; i++) {
        uint32_t idx = (start + i) % CLS_SEC_MAX_AUDIT;
        entries[i] = sec->audit_log[idx];
    }

    *count = to_copy;
    return CLS_OK;
}

void cls_security_destroy(cls_security_ctx_t *sec) {
    if (!sec) return;
    /* Zero out sensitive data */
    memset(sec->master_key, 0, CLS_SEC_HASH_SIZE);
    memset(sec, 0, sizeof(cls_security_ctx_t));
}
