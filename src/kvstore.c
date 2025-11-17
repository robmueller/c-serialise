// Generic KV store implementation (calls through vtable)

#include "../include/kvstore_backend.h"
#include <stdlib.h>

// ------------------------
// Database lifecycle
// ------------------------

kvstore_t* kvstore_open(const char *path, const struct kvstore_ops *ops) {
    kvstore_t *db = (kvstore_t*)calloc(1, sizeof(kvstore_t));
    if (!db) return NULL;

    db->ops = ops;

    if (ops->open(db, path) != KVSTORE_OK) {
        free(db);
        return NULL;
    }

    return db;
}

void kvstore_close(kvstore_t *db) {
    if (!db) return;

    if (db->ops->close) {
        db->ops->close(db);
    }

    free(db);
}

// ------------------------
// Transaction management
// ------------------------

kvstore_txn_t* kvstore_txn_begin(kvstore_t *db, bool read_only) {
    if (!db) return NULL;

    kvstore_txn_t *txn = (kvstore_txn_t*)calloc(1, sizeof(kvstore_txn_t));
    if (!txn) return NULL;

    txn->db = db;
    txn->read_only = read_only;

    if (db->ops->txn_begin(db, txn, read_only) != KVSTORE_OK) {
        free(txn);
        return NULL;
    }

    return txn;
}

int kvstore_txn_commit(kvstore_txn_t *txn) {
    if (!txn || !txn->db) return KVSTORE_ERROR;

    int rc = txn->db->ops->txn_commit(txn);
    free(txn);

    return rc;
}

void kvstore_txn_abort(kvstore_txn_t *txn) {
    if (!txn || !txn->db) return;

    txn->db->ops->txn_abort(txn);
    free(txn);
}

// ------------------------
// KV operations
// ------------------------

int kvstore_txn_put(kvstore_txn_t *txn, const char *table,
                    kvstore_val_t *key, kvstore_val_t *val) {
    if (!txn || !txn->db || !txn->db->ops->put) return KVSTORE_ERROR;
    return txn->db->ops->put(txn, table, key, val);
}

int kvstore_txn_get(kvstore_txn_t *txn, const char *table,
                    kvstore_val_t *key, kvstore_val_t *val_out) {
    if (!txn || !txn->db || !txn->db->ops->get) return KVSTORE_ERROR;
    return txn->db->ops->get(txn, table, key, val_out);
}

int kvstore_txn_del(kvstore_txn_t *txn, const char *table,
                    kvstore_val_t *key) {
    if (!txn || !txn->db || !txn->db->ops->del) return KVSTORE_ERROR;
    return txn->db->ops->del(txn, table, key);
}

// ------------------------
// Cursor operations
// ------------------------

kvstore_cursor_t* kvstore_cursor_open(kvstore_txn_t *txn, const char *table,
                                      kvstore_val_t *start_key) {
    if (!txn || !txn->db || !txn->db->ops->cursor_open) return NULL;

    kvstore_cursor_t *cur = (kvstore_cursor_t*)calloc(1, sizeof(kvstore_cursor_t));
    if (!cur) return NULL;

    cur->txn = txn;

    if (txn->db->ops->cursor_open(txn, cur, table, start_key) != KVSTORE_OK) {
        free(cur);
        return NULL;
    }

    return cur;
}

int kvstore_cursor_get(kvstore_cursor_t *cur, kvstore_val_t *key_out,
                       kvstore_val_t *val_out) {
    if (!cur || !cur->txn || !cur->txn->db) return KVSTORE_ERROR;
    return cur->txn->db->ops->cursor_get(cur, key_out, val_out);
}

int kvstore_cursor_next(kvstore_cursor_t *cur) {
    if (!cur || !cur->txn || !cur->txn->db) return KVSTORE_ERROR;
    return cur->txn->db->ops->cursor_next(cur);
}

void kvstore_cursor_close(kvstore_cursor_t *cur) {
    if (!cur) return;

    if (cur->txn && cur->txn->db && cur->txn->db->ops->cursor_close) {
        cur->txn->db->ops->cursor_close(cur);
    }

    free(cur);
}

// Forward declaration from kvstore_mem.c
extern const struct kvstore_ops* kvstore_mem_ops(void);

// Helper to create in-memory database
kvstore_t* kvstore_open_mem(void) {
    return kvstore_open(":memory:", kvstore_mem_ops());
}
