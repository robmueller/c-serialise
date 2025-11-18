// KV Store backend interface
// Implement this interface to support different KV backends (LMDB, RocksDB, etc.)

#ifndef KVSTORE_BACKEND_H_
#define KVSTORE_BACKEND_H_

#include "kvstore.h"

#ifdef __cplusplus
extern "C" {
#endif

// ------------------------
// Backend interface
// ------------------------

// Database handle
struct kvstore {
    void *backend_handle;
    const struct kvstore_ops *ops;
};

// Transaction handle
struct kvstore_txn {
    kvstore_t *db;
    void *backend_txn;
    bool read_only;
};

// Cursor handle
struct kvstore_cursor {
    kvstore_txn_t *txn;
    void *backend_cursor;
    char *table;
    bool valid;
};

// Backend operations vtable
struct kvstore_ops {
    // Database lifecycle
    int (*open)(kvstore_t *db, const char *path);
    void (*close)(kvstore_t *db);

    // Transaction management
    int (*txn_begin)(kvstore_t *db, kvstore_txn_t *txn, bool read_only);
    int (*txn_commit)(kvstore_txn_t *txn);
    void (*txn_abort)(kvstore_txn_t *txn);

    // KV operations
    int (*put)(kvstore_txn_t *txn, const char *table,
               kvstore_val_t *key, kvstore_val_t *val);
    int (*get)(kvstore_txn_t *txn, const char *table,
               kvstore_val_t *key, kvstore_val_t *val_out);
    int (*del)(kvstore_txn_t *txn, const char *table,
               kvstore_val_t *key);

    // Cursor operations
    int (*cursor_open)(kvstore_txn_t *txn, kvstore_cursor_t *cur,
                       const char *table, kvstore_val_t *start_key);
    int (*cursor_get)(kvstore_cursor_t *cur,
                      kvstore_val_t *key_out, kvstore_val_t *val_out);
    int (*cursor_next)(kvstore_cursor_t *cur);
    void (*cursor_close)(kvstore_cursor_t *cur);
};

// ------------------------
// Generic implementation
// ------------------------

// Open database (allocates kvstore_t)
kvstore_t* kvstore_open(const char *path, const struct kvstore_ops *ops);

// Close database
void kvstore_close(kvstore_t *db);

// Transaction management
kvstore_txn_t* kvstore_txn_begin(kvstore_t *db, bool read_only);
int kvstore_txn_commit(kvstore_txn_t *txn);
void kvstore_txn_abort(kvstore_txn_t *txn);

// KV operations (implementations call through vtable)
int kvstore_txn_put(kvstore_txn_t *txn, const char *table,
                    kvstore_val_t *key, kvstore_val_t *val);
int kvstore_txn_get(kvstore_txn_t *txn, const char *table,
                    kvstore_val_t *key, kvstore_val_t *val_out);
int kvstore_txn_del(kvstore_txn_t *txn, const char *table,
                    kvstore_val_t *key);

// Cursor operations
kvstore_cursor_t* kvstore_cursor_open(kvstore_txn_t *txn, const char *table,
                                      kvstore_val_t *start_key);
int kvstore_cursor_get(kvstore_cursor_t *cur, kvstore_val_t *key_out,
                       kvstore_val_t *val_out);
int kvstore_cursor_next(kvstore_cursor_t *cur);
void kvstore_cursor_close(kvstore_cursor_t *cur);

#ifdef __cplusplus
}
#endif

#endif // KVSTORE_BACKEND_H_
