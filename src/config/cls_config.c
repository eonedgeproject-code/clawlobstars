/*
 * ClawLobstars - Configuration Subsystem Implementation
 * INI-style key-value store with sections and type validation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../include/cls_framework.h"
#include "../include/cls_config.h"

/* ============================================================
 * Helpers
 * ============================================================ */

static cls_cfg_entry_t *cfg_find(cls_config_store_t *store, const char *section,
                                  const char *key) {
    for (uint32_t i = 0; i < store->count; i++) {
        if (!store->entries[i].active) continue;
        if (strcmp(store->entries[i].section, section) == 0 &&
            strcmp(store->entries[i].key, key) == 0) {
            return &store->entries[i];
        }
    }
    return NULL;
}

static const cls_cfg_entry_t *cfg_find_const(const cls_config_store_t *store,
                                              const char *section, const char *key) {
    for (uint32_t i = 0; i < store->count; i++) {
        if (!store->entries[i].active) continue;
        if (strcmp(store->entries[i].section, section) == 0 &&
            strcmp(store->entries[i].key, key) == 0) {
            return &store->entries[i];
        }
    }
    return NULL;
}

static cls_cfg_entry_t *cfg_alloc(cls_config_store_t *store) {
    if (store->count >= CLS_CFG_MAX_ENTRIES) return NULL;

    /* Reuse inactive slot */
    for (uint32_t i = 0; i < store->count; i++) {
        if (!store->entries[i].active) return &store->entries[i];
    }

    return &store->entries[store->count++];
}

/* ============================================================
 * Init
 * ============================================================ */

cls_status_t cls_cfg_init(cls_config_store_t *store) {
    if (!store) return CLS_ERR_INVALID;
    memset(store, 0, sizeof(cls_config_store_t));
    store->initialized = true;
    store->version = 1;
    return CLS_OK;
}

/* ============================================================
 * Setters
 * ============================================================ */

cls_status_t cls_cfg_set_str(cls_config_store_t *store, const char *section,
                              const char *key, const char *value) {
    if (!store || !store->initialized || !section || !key || !value)
        return CLS_ERR_INVALID;
    if (store->frozen) return CLS_ERR_STATE;

    cls_cfg_entry_t *e = cfg_find(store, section, key);
    if (e) {
        if (e->locked) return CLS_ERR_SECURITY;
        strncpy(e->value, value, CLS_CFG_VAL_LEN - 1);
        e->type = CLS_CFG_STRING;
    } else {
        e = cfg_alloc(store);
        if (!e) return CLS_ERR_OVERFLOW;
        memset(e, 0, sizeof(cls_cfg_entry_t));
        strncpy(e->section, section, CLS_CFG_SEC_LEN - 1);
        strncpy(e->key, key, CLS_CFG_KEY_LEN - 1);
        strncpy(e->value, value, CLS_CFG_VAL_LEN - 1);
        e->type = CLS_CFG_STRING;
        e->active = true;
    }

    store->version++;
    return CLS_OK;
}

cls_status_t cls_cfg_set_int(cls_config_store_t *store, const char *section,
                              const char *key, int64_t value) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%ld", (long)value);

    cls_status_t rc = cls_cfg_set_str(store, section, key, buf);
    if (rc != CLS_OK) return rc;

    cls_cfg_entry_t *e = cfg_find(store, section, key);
    if (e) e->type = CLS_CFG_INT;
    return CLS_OK;
}

cls_status_t cls_cfg_set_float(cls_config_store_t *store, const char *section,
                                const char *key, double value) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.6f", value);

    cls_status_t rc = cls_cfg_set_str(store, section, key, buf);
    if (rc != CLS_OK) return rc;

    cls_cfg_entry_t *e = cfg_find(store, section, key);
    if (e) e->type = CLS_CFG_FLOAT;
    return CLS_OK;
}

cls_status_t cls_cfg_set_bool(cls_config_store_t *store, const char *section,
                               const char *key, bool value) {
    cls_status_t rc = cls_cfg_set_str(store, section, key, value ? "true" : "false");
    if (rc != CLS_OK) return rc;

    cls_cfg_entry_t *e = cfg_find(store, section, key);
    if (e) e->type = CLS_CFG_BOOL;
    return CLS_OK;
}

/* ============================================================
 * Getters
 * ============================================================ */

cls_status_t cls_cfg_get_str(const cls_config_store_t *store, const char *section,
                              const char *key, const char **out) {
    if (!store || !store->initialized || !section || !key || !out)
        return CLS_ERR_INVALID;

    const cls_cfg_entry_t *e = cfg_find_const(store, section, key);
    if (!e) return CLS_ERR_NOT_FOUND;

    *out = e->value;
    return CLS_OK;
}

cls_status_t cls_cfg_get_int(const cls_config_store_t *store, const char *section,
                              const char *key, int64_t *out) {
    if (!out) return CLS_ERR_INVALID;

    const char *val;
    cls_status_t rc = cls_cfg_get_str(store, section, key, &val);
    if (rc != CLS_OK) return rc;

    char *endp;
    *out = strtol(val, &endp, 10);
    if (*endp != '\0') return CLS_ERR_INVALID;
    return CLS_OK;
}

cls_status_t cls_cfg_get_float(const cls_config_store_t *store, const char *section,
                                const char *key, double *out) {
    if (!out) return CLS_ERR_INVALID;

    const char *val;
    cls_status_t rc = cls_cfg_get_str(store, section, key, &val);
    if (rc != CLS_OK) return rc;

    char *endp;
    *out = strtod(val, &endp);
    if (*endp != '\0') return CLS_ERR_INVALID;
    return CLS_OK;
}

cls_status_t cls_cfg_get_bool(const cls_config_store_t *store, const char *section,
                               const char *key, bool *out) {
    if (!out) return CLS_ERR_INVALID;

    const char *val;
    cls_status_t rc = cls_cfg_get_str(store, section, key, &val);
    if (rc != CLS_OK) return rc;

    if (strcmp(val, "true") == 0 || strcmp(val, "1") == 0 || strcmp(val, "yes") == 0) {
        *out = true;
    } else if (strcmp(val, "false") == 0 || strcmp(val, "0") == 0 || strcmp(val, "no") == 0) {
        *out = false;
    } else {
        return CLS_ERR_INVALID;
    }
    return CLS_OK;
}

/* ============================================================
 * Defaults
 * ============================================================ */

cls_status_t cls_cfg_default_str(cls_config_store_t *store, const char *section,
                                  const char *key, const char *value) {
    if (!store || !store->initialized) return CLS_ERR_INVALID;
    if (cfg_find(store, section, key)) return CLS_OK; /* already exists */
    return cls_cfg_set_str(store, section, key, value);
}

cls_status_t cls_cfg_default_int(cls_config_store_t *store, const char *section,
                                  const char *key, int64_t value) {
    if (!store || !store->initialized) return CLS_ERR_INVALID;
    if (cfg_find(store, section, key)) return CLS_OK;
    return cls_cfg_set_int(store, section, key, value);
}

/* ============================================================
 * Management
 * ============================================================ */

cls_status_t cls_cfg_lock(cls_config_store_t *store, const char *section,
                           const char *key) {
    if (!store || !store->initialized) return CLS_ERR_INVALID;
    cls_cfg_entry_t *e = cfg_find(store, section, key);
    if (!e) return CLS_ERR_NOT_FOUND;
    e->locked = true;
    return CLS_OK;
}

cls_status_t cls_cfg_freeze(cls_config_store_t *store) {
    if (!store || !store->initialized) return CLS_ERR_INVALID;
    store->frozen = true;
    return CLS_OK;
}

cls_status_t cls_cfg_delete(cls_config_store_t *store, const char *section,
                             const char *key) {
    if (!store || !store->initialized) return CLS_ERR_INVALID;
    if (store->frozen) return CLS_ERR_STATE;

    cls_cfg_entry_t *e = cfg_find(store, section, key);
    if (!e) return CLS_ERR_NOT_FOUND;
    if (e->locked) return CLS_ERR_SECURITY;

    e->active = false;
    store->version++;
    return CLS_OK;
}

uint32_t cls_cfg_count(const cls_config_store_t *store) {
    if (!store) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < store->count; i++) {
        if (store->entries[i].active) n++;
    }
    return n;
}

cls_status_t cls_cfg_dump(const cls_config_store_t *store, FILE *fp) {
    if (!store || !fp) return CLS_ERR_INVALID;

    char last_section[CLS_CFG_SEC_LEN] = {0};

    for (uint32_t i = 0; i < store->count; i++) {
        if (!store->entries[i].active) continue;

        if (strcmp(store->entries[i].section, last_section) != 0) {
            fprintf(fp, "\n[%s]\n", store->entries[i].section);
            strncpy(last_section, store->entries[i].section, CLS_CFG_SEC_LEN - 1);
        }

        fprintf(fp, "%s = %s%s\n",
                store->entries[i].key,
                store->entries[i].value,
                store->entries[i].locked ? " # locked" : "");
    }
    return CLS_OK;
}

void cls_cfg_destroy(cls_config_store_t *store) {
    if (!store) return;
    memset(store, 0, sizeof(cls_config_store_t));
}
