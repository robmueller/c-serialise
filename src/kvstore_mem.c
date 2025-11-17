// Simple in-memory KV store backend for testing
// Not production-ready - uses simple sorted arrays

#define _POSIX_C_SOURCE 200809L
#include "../include/kvstore_backend.h"
#include <string.h>
#include <sys/types.h>

// ------------------------
// Data structures
// ------------------------

typedef struct {
    void *key;
    size_t key_size;
    void *val;
    size_t val_size;
} kv_pair_t;

typedef struct {
    char *name;
    kv_pair_t *pairs;
    size_t count;
    size_t capacity;
} kv_table_t;

typedef struct {
    kv_table_t *tables;
    size_t table_count;
    size_t table_capacity;
} mem_db_t;

typedef struct {
    mem_db_t *db;
    bool committed;
    // Simple implementation: changes go directly to DB
    // Real implementation would buffer changes
} mem_txn_t;

typedef struct {
    kv_table_t *table;
    size_t index;
} mem_cursor_t;

// ------------------------
// Helper functions
// ------------------------

static int compare_keys(const void *k1, size_t s1, const void *k2, size_t s2) {
    size_t min_size = s1 < s2 ? s1 : s2;
    int cmp = memcmp(k1, k2, min_size);
    if (cmp != 0) return cmp;
    if (s1 < s2) return -1;
    if (s1 > s2) return 1;
    return 0;
}

static kv_table_t* find_table(mem_db_t *db, const char *name) {
    for (size_t i = 0; i < db->table_count; i++) {
        if (strcmp(db->tables[i].name, name) == 0) {
            return &db->tables[i];
        }
    }
    return NULL;
}

static kv_table_t* get_or_create_table(mem_db_t *db, const char *name) {
    kv_table_t *table = find_table(db, name);
    if (table) return table;

    // Create new table
    if (db->table_count >= db->table_capacity) {
        db->table_capacity = db->table_capacity ? db->table_capacity * 2 : 8;
        db->tables = (kv_table_t*)realloc(db->tables,
                                          db->table_capacity * sizeof(kv_table_t));
    }

    table = &db->tables[db->table_count++];
    table->name = strdup(name);
    table->pairs = NULL;
    table->count = 0;
    table->capacity = 0;

    return table;
}

static ssize_t find_key_index(kv_table_t *table, const void *key, size_t key_size) {
    // Binary search
    ssize_t left = 0;
    ssize_t right = (ssize_t)table->count - 1;

    while (left <= right) {
        ssize_t mid = left + (right - left) / 2;
        int cmp = compare_keys(key, key_size,
                              table->pairs[mid].key, table->pairs[mid].key_size);

        if (cmp == 0) return mid;
        if (cmp < 0) right = mid - 1;
        else left = mid + 1;
    }

    return -1;  // Not found
}

static ssize_t find_insert_pos(kv_table_t *table, const void *key, size_t key_size) {
    // Binary search for insertion position
    ssize_t left = 0;
    ssize_t right = (ssize_t)table->count;

    while (left < right) {
        ssize_t mid = left + (right - left) / 2;
        int cmp = compare_keys(key, key_size,
                              table->pairs[mid].key, table->pairs[mid].key_size);

        if (cmp < 0) right = mid;
        else if (cmp > 0) left = mid + 1;
        else return mid;  // Exact match
    }

    return left;
}

// ------------------------
// Backend operations
// ------------------------

static int mem_open(kvstore_t *db, const char *path) {
    (void)path;  // In-memory: ignore path

    mem_db_t *mdb = (mem_db_t*)calloc(1, sizeof(mem_db_t));
    if (!mdb) return KVSTORE_ERROR;

    db->backend_handle = mdb;
    return KVSTORE_OK;
}

static void mem_close(kvstore_t *db) {
    mem_db_t *mdb = (mem_db_t*)db->backend_handle;
    if (!mdb) return;

    // Free all tables
    for (size_t i = 0; i < mdb->table_count; i++) {
        kv_table_t *table = &mdb->tables[i];
        for (size_t j = 0; j < table->count; j++) {
            free(table->pairs[j].key);
            free(table->pairs[j].val);
        }
        free(table->pairs);
        free(table->name);
    }
    free(mdb->tables);
    free(mdb);

    db->backend_handle = NULL;
}

static int mem_txn_begin(kvstore_t *db, kvstore_txn_t *txn, bool read_only) {
    mem_txn_t *mtxn = (mem_txn_t*)calloc(1, sizeof(mem_txn_t));
    if (!mtxn) return KVSTORE_ERROR;

    mtxn->db = (mem_db_t*)db->backend_handle;
    mtxn->committed = false;

    txn->backend_txn = mtxn;
    txn->read_only = read_only;

    return KVSTORE_OK;
}

static int mem_txn_commit(kvstore_txn_t *txn) {
    mem_txn_t *mtxn = (mem_txn_t*)txn->backend_txn;
    if (!mtxn) return KVSTORE_ERROR;

    mtxn->committed = true;
    free(mtxn);
    txn->backend_txn = NULL;

    return KVSTORE_OK;
}

static void mem_txn_abort(kvstore_txn_t *txn) {
    mem_txn_t *mtxn = (mem_txn_t*)txn->backend_txn;
    if (!mtxn) return;

    // Simple implementation: can't rollback (changes already applied)
    // Real implementation would buffer changes

    free(mtxn);
    txn->backend_txn = NULL;
}

static int mem_put(kvstore_txn_t *txn, const char *table_name,
                   kvstore_val_t *key, kvstore_val_t *val) {
    mem_txn_t *mtxn = (mem_txn_t*)txn->backend_txn;
    if (!mtxn) return KVSTORE_ERROR;

    kv_table_t *table = get_or_create_table(mtxn->db, table_name);

    // Find existing or insertion point
    ssize_t idx = find_insert_pos(table, key->data, key->size);
    bool exists = (idx >= 0 && idx < (ssize_t)table->count &&
                   compare_keys(key->data, key->size,
                               table->pairs[idx].key, table->pairs[idx].key_size) == 0);

    if (exists) {
        // Update existing
        free(table->pairs[idx].val);
        table->pairs[idx].val = malloc(val->size);
        memcpy(table->pairs[idx].val, val->data, val->size);
        table->pairs[idx].val_size = val->size;
    } else {
        // Insert new
        if (table->count >= table->capacity) {
            table->capacity = table->capacity ? table->capacity * 2 : 16;
            table->pairs = (kv_pair_t*)realloc(table->pairs,
                                               table->capacity * sizeof(kv_pair_t));
        }

        // Shift elements
        for (ssize_t i = table->count; i > idx; i--) {
            table->pairs[i] = table->pairs[i-1];
        }

        // Insert at position
        table->pairs[idx].key = malloc(key->size);
        memcpy(table->pairs[idx].key, key->data, key->size);
        table->pairs[idx].key_size = key->size;

        table->pairs[idx].val = malloc(val->size);
        memcpy(table->pairs[idx].val, val->data, val->size);
        table->pairs[idx].val_size = val->size;

        table->count++;
    }

    return KVSTORE_OK;
}

static int mem_get(kvstore_txn_t *txn, const char *table_name,
                   kvstore_val_t *key, kvstore_val_t *val_out) {
    mem_txn_t *mtxn = (mem_txn_t*)txn->backend_txn;
    if (!mtxn) return KVSTORE_ERROR;

    kv_table_t *table = find_table(mtxn->db, table_name);
    if (!table) return KVSTORE_NOTFOUND;

    ssize_t idx = find_key_index(table, key->data, key->size);
    if (idx < 0) return KVSTORE_NOTFOUND;

    val_out->data = table->pairs[idx].val;
    val_out->size = table->pairs[idx].val_size;

    return KVSTORE_OK;
}

static int mem_del(kvstore_txn_t *txn, const char *table_name,
                   kvstore_val_t *key) {
    mem_txn_t *mtxn = (mem_txn_t*)txn->backend_txn;
    if (!mtxn) return KVSTORE_ERROR;

    kv_table_t *table = find_table(mtxn->db, table_name);
    if (!table) return KVSTORE_NOTFOUND;

    ssize_t idx = find_key_index(table, key->data, key->size);
    if (idx < 0) return KVSTORE_NOTFOUND;

    // Free memory
    free(table->pairs[idx].key);
    free(table->pairs[idx].val);

    // Shift elements
    for (size_t i = idx; i < table->count - 1; i++) {
        table->pairs[i] = table->pairs[i+1];
    }

    table->count--;

    return KVSTORE_OK;
}

static int mem_cursor_open(kvstore_txn_t *txn, kvstore_cursor_t *cur,
                           const char *table_name, kvstore_val_t *start_key) {
    mem_txn_t *mtxn = (mem_txn_t*)txn->backend_txn;
    if (!mtxn) return KVSTORE_ERROR;

    kv_table_t *table = find_table(mtxn->db, table_name);
    if (!table) return KVSTORE_NOTFOUND;

    mem_cursor_t *mcur = (mem_cursor_t*)calloc(1, sizeof(mem_cursor_t));
    mcur->table = table;

    if (start_key) {
        // Find first key >= start_key
        mcur->index = (size_t)find_insert_pos(table, start_key->data, start_key->size);
    } else {
        mcur->index = 0;
    }

    cur->backend_cursor = mcur;
    cur->table = strdup(table_name);
    cur->valid = (mcur->index < table->count);

    return KVSTORE_OK;
}

static int mem_cursor_get(kvstore_cursor_t *cur,
                          kvstore_val_t *key_out, kvstore_val_t *val_out) {
    mem_cursor_t *mcur = (mem_cursor_t*)cur->backend_cursor;
    if (!mcur || !cur->valid) return KVSTORE_ERROR;

    if (mcur->index >= mcur->table->count) return KVSTORE_NOTFOUND;

    kv_pair_t *pair = &mcur->table->pairs[mcur->index];

    if (key_out) {
        key_out->data = pair->key;
        key_out->size = pair->key_size;
    }

    if (val_out) {
        val_out->data = pair->val;
        val_out->size = pair->val_size;
    }

    return KVSTORE_OK;
}

static int mem_cursor_next(kvstore_cursor_t *cur) {
    mem_cursor_t *mcur = (mem_cursor_t*)cur->backend_cursor;
    if (!mcur) return KVSTORE_ERROR;

    mcur->index++;
    cur->valid = (mcur->index < mcur->table->count);

    return cur->valid ? KVSTORE_OK : KVSTORE_NOTFOUND;
}

static void mem_cursor_close(kvstore_cursor_t *cur) {
    if (cur->backend_cursor) {
        free(cur->backend_cursor);
        cur->backend_cursor = NULL;
    }
    if (cur->table) {
        free(cur->table);
        cur->table = NULL;
    }
    cur->valid = false;
}

// ------------------------
// Ops vtable
// ------------------------

static const struct kvstore_ops mem_ops = {
    .open = mem_open,
    .close = mem_close,
    .txn_begin = mem_txn_begin,
    .txn_commit = mem_txn_commit,
    .txn_abort = mem_txn_abort,
    .put = mem_put,
    .get = mem_get,
    .del = mem_del,
    .cursor_open = mem_cursor_open,
    .cursor_get = mem_cursor_get,
    .cursor_next = mem_cursor_next,
    .cursor_close = mem_cursor_close,
};

const struct kvstore_ops* kvstore_mem_ops(void) {
    return &mem_ops;
}
