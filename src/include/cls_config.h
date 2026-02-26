/*
 * ClawLobstars - Configuration Subsystem
 * INI-style key-value config with sections, defaults, and validation
 */

#ifndef CLS_CONFIG_H
#define CLS_CONFIG_H

#include "cls_framework.h"

/* ============================================================
 * Constants
 * ============================================================ */
#define CLS_CFG_MAX_ENTRIES   128
#define CLS_CFG_MAX_SECTIONS   16
#define CLS_CFG_KEY_LEN        64
#define CLS_CFG_VAL_LEN       256
#define CLS_CFG_SEC_LEN        32

/* ============================================================
 * Types
 * ============================================================ */
typedef enum {
    CLS_CFG_STRING = 0,
    CLS_CFG_INT,
    CLS_CFG_FLOAT,
    CLS_CFG_BOOL
} cls_cfg_type_t;

typedef struct {
    char            section[CLS_CFG_SEC_LEN];
    char            key[CLS_CFG_KEY_LEN];
    char            value[CLS_CFG_VAL_LEN];
    cls_cfg_type_t  type;
    bool            locked;      /* read-only after set */
    bool            active;
} cls_cfg_entry_t;

typedef struct {
    cls_cfg_entry_t entries[CLS_CFG_MAX_ENTRIES];
    uint32_t        count;
    uint32_t        version;     /* bumped on every write */
    bool            frozen;      /* no more writes allowed */
    bool            initialized;
} cls_config_store_t;

/* ============================================================
 * API
 * ============================================================ */
cls_status_t cls_cfg_init(cls_config_store_t *store);

/* Setters */
cls_status_t cls_cfg_set_str(cls_config_store_t *store, const char *section,
                              const char *key, const char *value);
cls_status_t cls_cfg_set_int(cls_config_store_t *store, const char *section,
                              const char *key, int64_t value);
cls_status_t cls_cfg_set_float(cls_config_store_t *store, const char *section,
                                const char *key, double value);
cls_status_t cls_cfg_set_bool(cls_config_store_t *store, const char *section,
                               const char *key, bool value);

/* Getters */
cls_status_t cls_cfg_get_str(const cls_config_store_t *store, const char *section,
                              const char *key, const char **out);
cls_status_t cls_cfg_get_int(const cls_config_store_t *store, const char *section,
                              const char *key, int64_t *out);
cls_status_t cls_cfg_get_float(const cls_config_store_t *store, const char *section,
                                const char *key, double *out);
cls_status_t cls_cfg_get_bool(const cls_config_store_t *store, const char *section,
                               const char *key, bool *out);

/* Defaults (set only if key doesn't exist) */
cls_status_t cls_cfg_default_str(cls_config_store_t *store, const char *section,
                                  const char *key, const char *value);
cls_status_t cls_cfg_default_int(cls_config_store_t *store, const char *section,
                                  const char *key, int64_t value);

/* Management */
cls_status_t cls_cfg_lock(cls_config_store_t *store, const char *section, const char *key);
cls_status_t cls_cfg_freeze(cls_config_store_t *store);
cls_status_t cls_cfg_delete(cls_config_store_t *store, const char *section, const char *key);
uint32_t     cls_cfg_count(const cls_config_store_t *store);
cls_status_t cls_cfg_dump(const cls_config_store_t *store, FILE *fp);
void         cls_cfg_destroy(cls_config_store_t *store);

#endif /* CLS_CONFIG_H */
