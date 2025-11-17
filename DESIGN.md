# C Struct KV Store Design

## Overview

This design extends the existing serialization library to support storing C structures in a key-value store with primary and secondary indices. The system enables:

- **Primary key storage**: Lookup and iteration by primary key
- **Secondary indices**: Alternative access paths that reference the primary key
- **Transaction integration**: Compatible with twom_txn* style transaction APIs
- **Type-safe macros**: Declarative key specification similar to existing SERIALISE macro

## Design Principles

1. **Build on existing serialization**: Reuse the SERIALISE macro infrastructure
2. **Zero runtime overhead**: Macro-generated code with no vtables or indirection
3. **Compile-time validation**: Type checking through macro expansion
4. **Flexible key composition**: Support single or multi-field keys
5. **KV-agnostic**: Abstract interface adaptable to any KV backend

---

## User Interface

### 1. Declaring a Record with Keys

```c
// Define your struct
struct user_record {
    uint64_t user_id;        // Will be primary key
    char *email;             // Will be secondary key
    char *username;          // Will be secondary key
    uint32_t age;
    struct timespec created;
    uint64_t account_balance;
};

// Define serialization for the full struct
SERIALISE(user_record, struct user_record,
    SERIALISE_FIELD(user_id, uint64_t),
    SERIALISE_FIELD(email, charptr),
    SERIALISE_FIELD(username, charptr),
    SERIALISE_FIELD(age, uint32_t),
    SERIALISE_FIELD(created, timespec),
    SERIALISE_FIELD(account_balance, uint64_t)
)

// Define the primary key (user_id)
SERIALISE_PRIMARY_KEY(user_record, pk,
    SERIALISE_FIELD(user_id, uint64_t)
)

// Define secondary indices
SERIALISE_SECONDARY_KEY(user_record, by_email,
    SERIALISE_FIELD(email, charptr)
)

SERIALISE_SECONDARY_KEY(user_record, by_username,
    SERIALISE_FIELD(username, charptr)
)
```

### 2. Multi-field Compound Keys

```c
struct message_record {
    uint32_t mailbox_id;
    uint32_t uid;
    struct timespec received;
    char *subject;
    uint64_t size;
};

SERIALISE(message_record, struct message_record,
    SERIALISE_FIELD(mailbox_id, uint32_t),
    SERIALISE_FIELD(uid, uint32_t),
    SERIALISE_FIELD(received, timespec),
    SERIALISE_FIELD(subject, charptr),
    SERIALISE_FIELD(size, uint64_t)
)

// Compound primary key: mailbox_id + uid
SERIALISE_PRIMARY_KEY(message_record, pk,
    SERIALISE_FIELD(mailbox_id, uint32_t),
    SERIALISE_FIELD(uid, uint32_t)
)

// Secondary index by timestamp
SERIALISE_SECONDARY_KEY(message_record, by_received,
    SERIALISE_FIELD(received, timespec)
)

// Compound secondary key: mailbox_id + received
SERIALISE_SECONDARY_KEY(message_record, by_mailbox_time,
    SERIALISE_FIELD(mailbox_id, uint32_t),
    SERIALISE_FIELD(received, timespec)
)
```

---

## Generated Functions

For each declaration, the macros generate a set of functions:

### Primary Key Functions

`SERIALISE_PRIMARY_KEY(record_type, pk, ...)` generates:

```c
// Calculate size needed for primary key
size_t serialise_record_type_pk_size(struct record_type *r);

// Serialize primary key into buffer
char* serialise_record_type_pk(char *buf, struct record_type *r);

// Deserialize primary key from buffer
char* deserialise_record_type_pk(char *buf, struct record_type *r);

// Store record in KV store using primary key
int kvstore_put_record_type(kvstore_txn_t *txn, struct record_type *r);

// Fetch record by primary key
int kvstore_get_record_type(kvstore_txn_t *txn, struct record_type *key,
                             struct record_type *result);

// Delete record by primary key
int kvstore_del_record_type(kvstore_txn_t *txn, struct record_type *key);

// Cursor for iterating primary key table
kvstore_cursor_t* kvstore_cursor_record_type_pk(kvstore_txn_t *txn,
                                                 struct record_type *start_key);
```

### Secondary Key Functions

`SERIALISE_SECONDARY_KEY(record_type, index_name, ...)` generates:

```c
// Calculate size needed for secondary key
size_t serialise_record_type_index_name_size(struct record_type *r);

// Serialize secondary key into buffer
char* serialise_record_type_index_name(char *buf, struct record_type *r);

// Deserialize secondary key from buffer
char* deserialise_record_type_index_name(char *buf, struct record_type *r);

// Add/update secondary index entry
int kvstore_put_record_type_index_name(kvstore_txn_t *txn, struct record_type *r);

// Delete secondary index entry
int kvstore_del_record_type_index_name(kvstore_txn_t *txn, struct record_type *r);

// Lookup by secondary key, returns primary key
int kvstore_lookup_record_type_index_name(kvstore_txn_t *txn,
                                          struct record_type *sec_key,
                                          struct record_type *pri_key_out);

// Cursor for iterating secondary index
kvstore_cursor_t* kvstore_cursor_record_type_index_name(kvstore_txn_t *txn,
                                                         struct record_type *start_key);
```

---

## KV Store Abstraction Layer

### Core Types

```c
// Transaction handle (opaque)
typedef struct kvstore_txn kvstore_txn_t;

// Cursor handle (opaque)
typedef struct kvstore_cursor kvstore_cursor_t;

// Key-value pair for generic operations
typedef struct {
    void *data;
    size_t size;
} kvstore_val_t;

// Return codes
#define KVSTORE_OK        0
#define KVSTORE_NOTFOUND  1
#define KVSTORE_EXISTS    2
#define KVSTORE_ERROR    -1
```

### Transaction API

```c
// Begin a transaction (read-write by default)
kvstore_txn_t* kvstore_txn_begin(kvstore_t *db, int flags);

// Commit transaction
int kvstore_txn_commit(kvstore_txn_t *txn);

// Abort transaction
void kvstore_txn_abort(kvstore_txn_t *txn);

// Raw KV operations
int kvstore_txn_put(kvstore_txn_t *txn, const char *table,
                    kvstore_val_t *key, kvstore_val_t *val);

int kvstore_txn_get(kvstore_txn_t *txn, const char *table,
                    kvstore_val_t *key, kvstore_val_t *val_out);

int kvstore_txn_del(kvstore_txn_t *txn, const char *table,
                    kvstore_val_t *key);
```

### Cursor API

```c
// Create cursor positioned at key (or first key >= start_key if not exact match)
kvstore_cursor_t* kvstore_cursor_begin(kvstore_txn_t *txn, const char *table,
                                       kvstore_val_t *start_key);

// Advance to next entry
int kvstore_cursor_next(kvstore_cursor_t *cur);

// Get current key and value
int kvstore_cursor_get(kvstore_cursor_t *cur, kvstore_val_t *key_out,
                       kvstore_val_t *val_out);

// Close cursor
void kvstore_cursor_close(kvstore_cursor_t *cur);
```

---

## Implementation Details

### Table Naming Convention

Each record type uses multiple tables:

- **Primary table**: `<record_type>_pk` - Maps primary key → full record
- **Secondary tables**: `<record_type>_<index_name>` - Maps secondary key → primary key

Example for `user_record`:
- Primary: `user_record_pk`
- Secondary: `user_record_by_email`, `user_record_by_username`

### Storage Layout

**Primary key table:**
```
Key:   [serialized primary key fields]
Value: [serialized full struct]
```

**Secondary index table:**
```
Key:   [serialized secondary key fields]
Value: [serialized primary key fields]
```

### Macro Expansion Strategy

The macros will generate code following this pattern:

```c
SERIALISE_PRIMARY_KEY(user_record, pk,
    SERIALISE_FIELD(user_id, uint64_t)
)

// Expands to:
size_t serialise_user_record_pk_size(struct user_record *r) {
    size_t _sz = 0;
    _sz += TYPE_SIZEOF(u64, r->user_id);
    return _sz;
}

char* serialise_user_record_pk(char *buf, struct user_record *r) {
    TYPE_ENC(u64, buf, r->user_id);
    return buf;
}

char* deserialise_user_record_pk(char *buf, struct user_record *r) {
    TYPE_DEC(u64, buf, r->user_id);
    return buf;
}

int kvstore_put_user_record(kvstore_txn_t *txn, struct user_record *r) {
    // Serialize primary key
    size_t key_sz = serialise_user_record_pk_size(r);
    char *key_buf = alloca(key_sz);
    serialise_user_record_pk(key_buf, r);

    // Serialize full record
    size_t val_sz = serialise_user_record_size(r);
    char *val_buf = alloca(val_sz);
    serialise_user_record(val_buf, r);

    kvstore_val_t key = { key_buf, key_sz };
    kvstore_val_t val = { val_buf, val_sz };

    return kvstore_txn_put(txn, "user_record_pk", &key, &val);
}
```

### Secondary Index Maintenance

When putting/deleting records, secondary indices must be updated:

```c
// User calls this high-level function
int kvstore_put_user_record_all(kvstore_txn_t *txn, struct user_record *r) {
    int rc;

    // Store in primary table
    rc = kvstore_put_user_record(txn, r);
    if (rc != KVSTORE_OK) return rc;

    // Update all secondary indices
    rc = kvstore_put_user_record_by_email(txn, r);
    if (rc != KVSTORE_OK) return rc;

    rc = kvstore_put_user_record_by_username(txn, r);
    if (rc != KVSTORE_OK) return rc;

    return KVSTORE_OK;
}
```

---

## Usage Examples

### Example 1: Simple Insert and Lookup

```c
kvstore_t *db = kvstore_open("mydb.kv");
kvstore_txn_t *txn = kvstore_txn_begin(db, 0);

// Create a record
struct user_record user = {
    .user_id = 12345,
    .email = strdup("alice@example.com"),
    .username = strdup("alice"),
    .age = 30,
    .account_balance = 100000
};
clock_gettime(CLOCK_REALTIME, &user.created);

// Insert into KV store (primary + all secondary indices)
kvstore_put_user_record_all(txn, &user);

// Commit
kvstore_txn_commit(txn);

// --- Later: lookup by primary key ---
txn = kvstore_txn_begin(db, 0);

struct user_record key = { .user_id = 12345 };
struct user_record result = {0};

if (kvstore_get_user_record(txn, &key, &result) == KVSTORE_OK) {
    printf("Found user: %s (%s)\n", result.username, result.email);
}

kvstore_txn_commit(txn);
```

### Example 2: Secondary Index Lookup

```c
txn = kvstore_txn_begin(db, 0);

// Search by email (secondary index)
struct user_record email_key = {
    .email = "alice@example.com"
};
struct user_record pri_key = {0};

// First: lookup secondary index to get primary key
int rc = kvstore_lookup_user_record_by_email(txn, &email_key, &pri_key);

if (rc == KVSTORE_OK) {
    // Second: lookup primary table to get full record
    struct user_record full_record = {0};
    rc = kvstore_get_user_record(txn, &pri_key, &full_record);

    if (rc == KVSTORE_OK) {
        printf("User ID: %llu, Balance: %llu\n",
               full_record.user_id, full_record.account_balance);
    }
}

kvstore_txn_commit(txn);
```

### Example 3: Range Iteration

```c
txn = kvstore_txn_begin(db, 0);

// Iterate all users in primary key order
struct user_record start_key = { .user_id = 0 };
kvstore_cursor_t *cur = kvstore_cursor_user_record_pk(txn, &start_key);

kvstore_val_t key_val, rec_val;
while (kvstore_cursor_get(cur, &key_val, &rec_val) == KVSTORE_OK) {
    struct user_record rec = {0};
    deserialise_user_record(rec_val.data, &rec);

    printf("User %llu: %s\n", rec.user_id, rec.username);

    if (kvstore_cursor_next(cur) != KVSTORE_OK)
        break;
}

kvstore_cursor_close(cur);
kvstore_txn_commit(txn);
```

### Example 4: Compound Key Range Scan

```c
// Iterate messages in mailbox 42, ordered by received time
txn = kvstore_txn_begin(db, 0);

struct message_record start = {
    .mailbox_id = 42,
    .received = { .tv_sec = 0, .tv_nsec = 0 }
};

kvstore_cursor_t *cur = kvstore_cursor_message_record_by_mailbox_time(txn, &start);

kvstore_val_t key_val, pk_val;
while (kvstore_cursor_get(cur, &key_val, &pk_val) == KVSTORE_OK) {
    // key_val contains the compound key (mailbox_id + received)
    // pk_val contains the primary key (mailbox_id + uid)

    struct message_record pk = {0};
    deserialise_message_record_pk(pk_val.data, &pk);

    // Now fetch full record
    struct message_record full = {0};
    kvstore_get_message_record(txn, &pk, &full);

    // Check if still in same mailbox
    if (full.mailbox_id != 42)
        break;

    printf("Message %u: %s (size: %llu)\n",
           full.uid, full.subject, full.size);

    if (kvstore_cursor_next(cur) != KVSTORE_OK)
        break;
}

kvstore_cursor_close(cur);
kvstore_txn_commit(txn);
```

---

## Macro Definitions (Outline)

### SERIALISE_PRIMARY_KEY

```c
#define SERIALISE_PRIMARY_KEY(name, key_name, ...) \
    /* Generate key serialization functions */ \
    SERIALISE_KEY_FUNCS(name, key_name, __VA_ARGS__) \
    /* Generate KV put/get/del for primary table */ \
    KVSTORE_PRIMARY_OPS(name, key_name)
```

### SERIALISE_SECONDARY_KEY

```c
#define SERIALISE_SECONDARY_KEY(name, index_name, ...) \
    /* Generate secondary key serialization functions */ \
    SERIALISE_KEY_FUNCS(name, index_name, __VA_ARGS__) \
    /* Generate KV put/del/lookup for secondary table */ \
    KVSTORE_SECONDARY_OPS(name, index_name)
```

### Internal Helper Macros

```c
// Generate size/encode/decode functions for a key
#define SERIALISE_KEY_FUNCS(name, key_name, ...) \
    /* Similar to SERIALISE macro but for subset of fields */

// Generate primary table operations
#define KVSTORE_PRIMARY_OPS(name, key_name) \
    /* kvstore_put_<name>, kvstore_get_<name>, kvstore_del_<name> */

// Generate secondary table operations
#define KVSTORE_SECONDARY_OPS(name, index_name) \
    /* kvstore_put_<name>_<index>, kvstore_del_<name>_<index>, */
    /* kvstore_lookup_<name>_<index> */
```

---

## Backend Adapter Interface

To support different KV stores (LMDB, RocksDB, twom, etc.), implement:

```c
// In kvstore_lmdb.c or kvstore_twom.c:
struct kvstore {
    void *backend_handle;
    // Backend-specific fields
};

struct kvstore_txn {
    kvstore_t *db;
    void *backend_txn;
};

struct kvstore_cursor {
    kvstore_txn_t *txn;
    void *backend_cursor;
    char *table;
};

// Implement the kvstore_* functions for specific backend
```

---

## File Structure

```
c-serialise/
├── include/
│   ├── serialise.h           # Existing serialization macros
│   ├── kvstore.h             # New: KV store abstraction + key macros
│   └── kvstore_backend.h     # Backend interface specification
├── src/
│   ├── kvstore_lmdb.c        # LMDB backend implementation
│   ├── kvstore_twom.c        # twom backend implementation
│   └── kvstore_rocksdb.c     # RocksDB backend (optional)
├── examples/
│   ├── kvstore_user.c        # Example: user records with indices
│   └── kvstore_messages.c    # Example: compound keys
└── tests/
    └── test_kvstore.c        # Unit tests
```

---

## Benefits of This Design

1. **Declarative**: Keys specified the same way as serialization fields
2. **Type-safe**: Compile-time checking of field types and names
3. **Efficient**: Zero abstraction overhead, direct memory operations
4. **Familiar**: Matches existing SERIALISE macro patterns
5. **Flexible**: Supports single/compound keys, multiple indices
6. **Portable**: KV backend can be swapped via adapter interface
7. **Transaction-safe**: Integrates with existing transaction patterns

---

## Open Questions for Refinement

1. **Unique constraints**: Should secondary indices enforce uniqueness?
2. **Index updates on modify**: Auto-update indices when fields change?
3. **Prefix iteration**: Should cursors support prefix matching?
4. **Sorting order**: Little-endian serialization provides byte-order sorting - is this desired?
5. **Memory management**: Who owns buffers? Stack (alloca) vs heap (malloc)?
6. **Error handling**: Return codes vs exceptions vs callbacks?

---

## Next Steps

1. Implement core macro infrastructure in `kvstore.h`
2. Create LMDB or twom backend adapter
3. Write comprehensive examples
4. Add unit tests for single/compound keys
5. Document performance characteristics
6. Consider adding helper macros for common patterns (e.g., auto-index maintenance)
