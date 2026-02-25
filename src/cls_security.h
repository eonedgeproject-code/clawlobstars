/*
 * ClawLobstars - Security Layer
 * Input validation, auth, access control, encryption, audit
 */

#ifndef CLS_SECURITY_H
#define CLS_SECURITY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLS_SEC_TOKEN_SIZE   64
#define CLS_SEC_HASH_SIZE    32
#define CLS_SEC_MAX_ROLES    16
#define CLS_SEC_MAX_AUDIT    512
#define CLS_SEC_MAX_RULES    64

/* Authentication result */
typedef enum {
    CLS_AUTH_OK          = 0,
    CLS_AUTH_DENIED      = 1,
    CLS_AUTH_EXPIRED     = 2,
    CLS_AUTH_INVALID     = 3,
    CLS_AUTH_RATE_LIMIT  = 4
} cls_auth_result_t;

/* Access control permission */
typedef enum {
    CLS_PERM_NONE      = 0x00,
    CLS_PERM_READ      = 0x01,
    CLS_PERM_WRITE     = 0x02,
    CLS_PERM_EXECUTE   = 0x04,
    CLS_PERM_ADMIN     = 0x08,
    CLS_PERM_ALL       = 0x0F
} cls_permission_t;

/* Audit event type */
typedef enum {
    CLS_AUDIT_AUTH_SUCCESS  = 0,
    CLS_AUDIT_AUTH_FAILURE  = 1,
    CLS_AUDIT_ACCESS_GRANT  = 2,
    CLS_AUDIT_ACCESS_DENY   = 3,
    CLS_AUDIT_INPUT_REJECT  = 4,
    CLS_AUDIT_CONFIG_CHANGE = 5,
    CLS_AUDIT_ANOMALY       = 6,
    CLS_AUDIT_ENCRYPT       = 7,
    CLS_AUDIT_DECRYPT       = 8
} cls_audit_type_t;

/* Auth token */
typedef struct {
    uint8_t     token[CLS_SEC_TOKEN_SIZE];
    uint32_t    agent_id;
    uint32_t    role_id;
    uint64_t    issued_at;
    uint64_t    expires_at;
    bool        valid;
} cls_auth_token_t;

/* Role for RBAC */
typedef struct {
    uint32_t        role_id;
    char            name[32];
    uint8_t         permissions;    /* Bitmask of cls_permission_t */
} cls_role_t;

/* Input validation rule */
typedef struct {
    uint32_t    rule_id;
    uint16_t    data_type;
    size_t      max_size;
    bool        allow_null;
    bool        (*custom_validator)(const void *data, size_t len);
} cls_validation_rule_t;

/* Audit log entry */
typedef struct {
    uint32_t        entry_id;
    cls_audit_type_t type;
    uint32_t        agent_id;
    uint64_t        timestamp_us;
    char            detail[128];
    uint8_t         severity;       /* 0-5 */
} cls_audit_entry_t;

/* Security context */
struct cls_security_ctx {
    cls_security_level_t    level;
    cls_role_t              roles[CLS_SEC_MAX_ROLES];
    uint32_t                role_count;
    cls_validation_rule_t   rules[CLS_SEC_MAX_RULES];
    uint32_t                rule_count;
    cls_audit_entry_t       audit_log[CLS_SEC_MAX_AUDIT];
    uint32_t                audit_head;
    uint32_t                audit_count;
    uint32_t                next_audit_id;
    uint64_t                auth_attempts;
    uint64_t                auth_failures;
    uint64_t                inputs_rejected;
    uint8_t                 master_key[CLS_SEC_HASH_SIZE];
    bool                    key_initialized;
};

/* ---- API ---- */

cls_status_t cls_security_init(cls_security_ctx_t *sec, cls_security_level_t level);

/* Key management */
cls_status_t cls_security_set_key(cls_security_ctx_t *sec, const uint8_t *key, size_t key_len);

/* Role-based access control */
cls_status_t cls_security_add_role(cls_security_ctx_t *sec, const cls_role_t *role);
cls_status_t cls_security_check_permission(cls_security_ctx_t *sec, uint32_t role_id,
                                            cls_permission_t required);

/* Authentication */
cls_status_t cls_security_auth(cls_security_ctx_t *sec, uint32_t agent_id,
                                const uint8_t *credentials, size_t cred_len,
                                cls_auth_token_t *out_token);
cls_status_t cls_security_validate_token(cls_security_ctx_t *sec, const cls_auth_token_t *token);
cls_status_t cls_security_revoke_token(cls_security_ctx_t *sec, cls_auth_token_t *token);

/* Input validation */
cls_status_t cls_security_add_rule(cls_security_ctx_t *sec, const cls_validation_rule_t *rule);
cls_status_t cls_security_validate_input(cls_security_ctx_t *sec, const cls_frame_t *frame);
cls_status_t cls_security_validate_buffer(cls_security_ctx_t *sec, const void *buf,
                                           size_t len, size_t max_allowed);

/* Encryption (XOR-based stream cipher for embedded; plug real AES for production) */
cls_status_t cls_security_encrypt(cls_security_ctx_t *sec, const void *plaintext,
                                   size_t len, void *ciphertext);
cls_status_t cls_security_decrypt(cls_security_ctx_t *sec, const void *ciphertext,
                                   size_t len, void *plaintext);

/* Hashing (FNV-1a based, use SHA256 for production) */
cls_status_t cls_security_hash(const void *data, size_t len, uint8_t *hash_out);

/* Audit log */
cls_status_t cls_security_audit(cls_security_ctx_t *sec, cls_audit_type_t type,
                                 uint32_t agent_id, const char *detail, uint8_t severity);
cls_status_t cls_security_get_audit(const cls_security_ctx_t *sec, cls_audit_entry_t *entries,
                                     uint32_t *count, uint32_t max_entries);

void cls_security_destroy(cls_security_ctx_t *sec);

#ifdef __cplusplus
}
#endif

#endif /* CLS_SECURITY_H */
