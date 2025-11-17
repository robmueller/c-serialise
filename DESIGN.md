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

## Generated Structures and Functions

### Generated Key Structures

For each key definition, a dedicated struct is generated containing only the key fields:

`SERIALISE_PRIMARY_KEY(user_record, pk, SERIALISE_FIELD(user_id, uint64_t))` generates:

```c
struct user_record_pk {
    uint64_t user_id;
};
```

`SERIALISE_SECONDARY_KEY(user_record, by_email, SERIALISE_FIELD(email, charptr))` generates:

```c
struct user_record_by_email_key {
    char *email;  // Note: charptr maps to char*
};
```

**Type Mapping Rules:**
- `charptr` → `char *`
- `timespec` → `struct timespec`
- `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t` → same
- `int8_t`, `int16_t`, `int32_t`, `int64_t` → same
- `size_t` → `size_t`
- Custom types (e.g., `message_guid`) → `struct message_guid`

### Key Buffer for Update Tracking

To handle updates correctly (when key fields change), we use a key buffer:

```c
typedef struct {
    char *buf;      // Buffer holding serialized keys (malloc'd, can be NULL)
    size_t size;    // Current buffer size
} kvstore_key_buf_t;

// Initialize to empty
#define KVSTORE_KEY_BUF_INIT { .buf = NULL, .size = 0 }

// Free the buffer
void kvstore_key_buf_free(kvstore_key_buf_t *kb);
```

**Buffer Layout:**
```
[pk_len:4][pk_data...][sk1_len:4][sk1_data...][sk2_len:4][sk2_data...]...
```

### Primary Key Functions

`SERIALISE_PRIMARY_KEY(record_type, pk, ...)` generates:

```c
// Dedicated primary key struct
struct record_type_pk {
    // Only pk fields
};

// Calculate size needed for primary key
size_t serialise_record_type_pk_size(struct record_type_pk *key);

// Serialize primary key into buffer
char* serialise_record_type_pk(char *buf, struct record_type_pk *key);

// Deserialize primary key from buffer
char* deserialise_record_type_pk(char *buf, struct record_type_pk *key);

// Extract pk fields from full record
void record_type_extract_pk(struct record_type *rec, struct record_type_pk *pk);

// Store record in KV store using primary key
int kvstore_put_record_type(kvstore_txn_t *txn, struct record_type *rec,
                            kvstore_key_buf_t *old_keys);

// Fetch record by primary key
int kvstore_get_record_type(kvstore_txn_t *txn, struct record_type_pk *key,
                            struct record_type *result,
                            kvstore_key_buf_t *key_buf);

// Delete record by primary key
int kvstore_del_record_type(kvstore_txn_t *txn, struct record_type_pk *key);

// Cursor for iterating primary key table
kvstore_cursor_t* kvstore_cursor_record_type_pk(kvstore_txn_t *txn,
                                                 struct record_type_pk *start_key);
```

### Secondary Key Functions

`SERIALISE_SECONDARY_KEY(record_type, index_name, ...)` generates:

```c
// Dedicated secondary key struct
struct record_type_index_name_key {
    // Only index_name fields
};

// Calculate size needed for secondary key
size_t serialise_record_type_index_name_size(struct record_type_index_name_key *key);

// Serialize secondary key into buffer
char* serialise_record_type_index_name(char *buf, struct record_type_index_name_key *key);

// Deserialize secondary key from buffer
char* deserialise_record_type_index_name(char *buf, struct record_type_index_name_key *key);

// Extract sk fields from full record
void record_type_extract_index_name(struct record_type *rec,
                                    struct record_type_index_name_key *sk);

// Lookup by secondary key, returns primary key
int kvstore_lookup_record_type_index_name(kvstore_txn_t *txn,
                                          struct record_type_index_name_key *sec_key,
                                          struct record_type_pk *pri_key_out);

// Cursor for iterating secondary index
kvstore_cursor_t* kvstore_cursor_record_type_index_name(kvstore_txn_t *txn,
                                          struct record_type_index_name_key *start_key);

// Internal: used by kvstore_put_record_type() to maintain indices
int kvstore_put_record_type_index_name_internal(kvstore_txn_t *txn,
                                                struct record_type *rec,
                                                char *old_key_data, size_t old_key_len);
int kvstore_del_record_type_index_name_internal(kvstore_txn_t *txn,
                                                struct record_type_index_name_key *key,
                                                struct record_type_pk *pk);
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

### Update Logic with Key Change Detection

The `kvstore_put_record_type()` function handles three scenarios:

**1. Insert (old_keys is NULL or empty):**
- Store record in primary table
- Add all secondary index entries

**2. Update with no key changes:**
- Update primary table value only
- No secondary index changes needed

**3. Update with key changes:**
- If primary key changed:
  - Delete old primary key entry
  - Delete all old secondary index entries
  - Insert new primary key entry
  - Insert all new secondary index entries
- If primary key unchanged but secondary key(s) changed:
  - Update primary table value
  - For each changed secondary key:
    - Delete old secondary index entry
    - Insert new secondary index entry

**Implementation pseudocode:**

```c
int kvstore_put_user_record(kvstore_txn_t *txn, struct user_record *rec,
                            kvstore_key_buf_t *old_keys) {
    // Serialize new keys
    struct user_record_pk new_pk;
    user_record_extract_pk(rec, &new_pk);
    size_t new_pk_sz = serialise_user_record_pk_size(&new_pk);
    char *new_pk_buf = alloca(new_pk_sz);
    serialise_user_record_pk(new_pk_buf, &new_pk);

    struct user_record_by_email_key new_sk_email;
    user_record_extract_by_email(rec, &new_sk_email);
    size_t new_sk_email_sz = serialise_user_record_by_email_size(&new_sk_email);
    char *new_sk_email_buf = alloca(new_sk_email_sz);
    serialise_user_record_by_email(new_sk_email_buf, &new_sk_email);

    // Similar for by_username...

    // Compare with old keys if provided
    if (old_keys && old_keys->buf) {
        char *p = old_keys->buf;
        uint32_t old_pk_len; memcpy(&old_pk_len, p, 4); p += 4;
        char *old_pk_buf = p; p += old_pk_len;

        uint32_t old_sk_email_len; memcpy(&old_sk_email_len, p, 4); p += 4;
        char *old_sk_email_buf = p; p += old_sk_email_len;

        // Similar for other sks...

        // Check if primary key changed
        int pk_changed = (old_pk_len != new_pk_sz ||
                         memcmp(old_pk_buf, new_pk_buf, new_pk_sz) != 0);

        if (pk_changed) {
            // Delete old primary and all old secondary indices
            kvstore_txn_del(txn, "user_record_pk",
                           &(kvstore_val_t){old_pk_buf, old_pk_len});
            kvstore_del_user_record_by_email_internal(txn, old_sk_email_buf,
                                                      old_pk_buf, old_pk_len);
            // Similar for other sks...

            // Insert new primary and all new secondary indices
            // (fall through to insert logic below)
        } else {
            // PK unchanged, check each SK
            int sk_email_changed = (old_sk_email_len != new_sk_email_sz ||
                                   memcmp(old_sk_email_buf, new_sk_email_buf,
                                          new_sk_email_sz) != 0);
            if (sk_email_changed) {
                kvstore_del_user_record_by_email_internal(txn, old_sk_email_buf,
                                                          old_pk_buf, old_pk_len);
                kvstore_put_user_record_by_email_internal(txn, rec,
                                                          new_sk_email_buf, new_sk_email_sz);
            }
            // Similar for other sks...
        }
    }

    // Store/update primary table
    size_t val_sz = serialise_user_record_size(rec);
    char *val_buf = alloca(val_sz);
    serialise_user_record(val_buf, rec);

    kvstore_val_t key = { new_pk_buf, new_pk_sz };
    kvstore_val_t val = { val_buf, val_sz };
    kvstore_txn_put(txn, "user_record_pk", &key, &val);

    // Add/update secondary indices (if new or pk changed)
    if (!old_keys || !old_keys->buf || pk_changed) {
        kvstore_put_user_record_by_email_internal(txn, rec,
                                                  new_sk_email_buf, new_sk_email_sz);
        // Similar for other sks...
    }

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

// Insert into KV store (NULL key_buf = new insert)
kvstore_put_user_record(txn, &user, NULL);

kvstore_txn_commit(txn);

// --- Later: lookup by primary key ---
txn = kvstore_txn_begin(db, 0);

struct user_record_pk key = { .user_id = 12345 };
struct user_record result = {0};

// Don't need key_buf for read-only lookup
if (kvstore_get_user_record(txn, &key, &result, NULL) == KVSTORE_OK) {
    printf("Found user: %s (%s)\n", result.username, result.email);
}

kvstore_txn_commit(txn);
```

### Example 2: Secondary Index Lookup

```c
txn = kvstore_txn_begin(db, 0);

// Search by email (secondary index)
struct user_record_by_email_key email_key = {
    .email = "alice@example.com"
};
struct user_record_pk pri_key = {0};

// First: lookup secondary index to get primary key
int rc = kvstore_lookup_user_record_by_email(txn, &email_key, &pri_key);

if (rc == KVSTORE_OK) {
    // Second: lookup primary table to get full record
    struct user_record full_record = {0};
    rc = kvstore_get_user_record(txn, &pri_key, &full_record, NULL);

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
struct user_record_pk start_key = { .user_id = 0 };
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

struct message_record_by_mailbox_time_key start = {
    .mailbox_id = 42,
    .received = { .tv_sec = 0, .tv_nsec = 0 }
};

kvstore_cursor_t *cur = kvstore_cursor_message_record_by_mailbox_time(txn, &start);

kvstore_val_t key_val, pk_val;
while (kvstore_cursor_get(cur, &key_val, &pk_val) == KVSTORE_OK) {
    // key_val contains the compound key (mailbox_id + received)
    // pk_val contains the primary key (mailbox_id + uid)

    struct message_record_pk pk = {0};
    deserialise_message_record_pk(pk_val.data, &pk);

    // Now fetch full record
    struct message_record full = {0};
    kvstore_get_message_record(txn, &pk, &full, NULL);

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

## Macro Definitions

### Type Mapping from Serialization Tags to C Types

To generate key structs, we need to map from serialization type tags (e.g., `charptr`, `u64`) back to C types:

```c
// Map serialization tag to C type for struct field declarations
#define SER_CTYPE_u8       uint8_t
#define SER_CTYPE_u16      uint16_t
#define SER_CTYPE_u32      uint32_t
#define SER_CTYPE_u64      uint64_t
#define SER_CTYPE_i8       int8_t
#define SER_CTYPE_i16      int16_t
#define SER_CTYPE_i32      int32_t
#define SER_CTYPE_i64      int64_t
#define SER_CTYPE_size     size_t
#define SER_CTYPE_charptr  char*
#define SER_CTYPE_timespec struct timespec

// For custom types, need to define:
// #define SER_CTYPE_message_guid struct message_guid
```

### Key Struct Generation

```c
// Generate a struct field declaration from a field spec
#define KEY_FIELD_DECL(tuple) KEY_FIELD_DECL_I tuple
#define KEY_FIELD_DECL_I(kind, ...) SER_CAT(KEY_FIELD_DECL_, kind)(__VA_ARGS__)

#define KEY_FIELD_DECL_SCALAR(name, type) \
    SER_CTYPE_##type name;

#define KEY_FIELD_DECL_ARRAY(name, type, count) \
    SER_CTYPE_##type name[count];

// Generate complete key struct
#define GENERATE_KEY_STRUCT(struct_name, ...) \
    struct struct_name { \
        FOR_EACH(KEY_FIELD_DECL, __VA_ARGS__) \
    };
```

### SERIALISE_PRIMARY_KEY

```c
#define SERIALISE_PRIMARY_KEY(name, key_name, ...) \
    /* Generate struct name_pk with only pk fields */ \
    GENERATE_KEY_STRUCT(SER_CAT(name, _pk), __VA_ARGS__) \
    \
    /* Generate serialization functions for the key */ \
    SERIALISE_KEY_FUNCS(name, key_name, __VA_ARGS__) \
    \
    /* Generate extractor: copies fields from full struct to pk struct */ \
    GENERATE_KEY_EXTRACTOR(name, key_name, __VA_ARGS__) \
    \
    /* Generate KV operations */ \
    KVSTORE_PRIMARY_OPS(name, key_name)
```

### SERIALISE_SECONDARY_KEY

```c
#define SERIALISE_SECONDARY_KEY(name, index_name, ...) \
    /* Generate struct name_index_name_key with only sk fields */ \
    GENERATE_KEY_STRUCT(SER_CAT(SER_CAT(name, _), SER_CAT(index_name, _key)), \
                        __VA_ARGS__) \
    \
    /* Generate serialization functions for the key */ \
    SERIALISE_KEY_FUNCS_SK(name, index_name, __VA_ARGS__) \
    \
    /* Generate extractor */ \
    GENERATE_KEY_EXTRACTOR_SK(name, index_name, __VA_ARGS__) \
    \
    /* Generate KV operations */ \
    KVSTORE_SECONDARY_OPS(name, index_name)
```

### Key Extraction Functions

```c
// Generate function to copy fields from full record to key struct
#define KEY_EXTRACT_STMT(tuple) KEY_EXTRACT_STMT_I tuple
#define KEY_EXTRACT_STMT_I(kind, ...) SER_CAT(KEY_EXTRACT_, kind)(__VA_ARGS__)

#define KEY_EXTRACT_SCALAR(name, type) \
    do { key->name = rec->name; } while (0)

#define KEY_EXTRACT_ARRAY(name, type, count) \
    do { memcpy(key->name, rec->name, sizeof(key->name)); } while (0)

#define GENERATE_KEY_EXTRACTOR(name, key_name, ...) \
    static inline void SER_CAT(name, _extract_pk)( \
        struct name *rec, struct SER_CAT(name, _pk) *key) { \
        FOR_EACH(KEY_EXTRACT_STMT, __VA_ARGS__); \
    }
```

### Internal Helper Macros

```c
// Generate size/encode/decode functions for a key (similar to SERIALISE)
#define SERIALISE_KEY_FUNCS(name, key_name, ...) \
    size_t SER_CAT(serialise_, SER_CAT(name, _pk_size))( \
        struct SER_CAT(name, _pk) *key) { \
        size_t _sz = 0; \
        /* Use key-> instead of r-> */ \
        FOR_EACH(ITEM_SIZE_KEY, __VA_ARGS__); \
        return _sz; \
    } \
    /* Similar for encode/decode */

// Generate primary table operations
#define KVSTORE_PRIMARY_OPS(name, key_name) \
    /* kvstore_put_<name>(): handles key change detection */ \
    /* kvstore_get_<name>(): fetches and optionally fills key_buf */ \
    /* kvstore_del_<name>(): simple delete by pk */ \
    /* kvstore_cursor_<name>_pk(): creates cursor */

// Generate secondary table operations
#define KVSTORE_SECONDARY_OPS(name, index_name) \
    /* kvstore_lookup_<name>_<index>(): sk -> pk lookup */ \
    /* kvstore_cursor_<name>_<index>(): iteration by sk */ \
    /* Internal put/del helpers for index maintenance */
```

### Example Macro Expansion

Input:
```c
SERIALISE_PRIMARY_KEY(user_record, pk,
    SERIALISE_FIELD(user_id, uint64_t)
)
```

Expands to:
```c
// 1. Key struct
struct user_record_pk {
    uint64_t user_id;
};

// 2. Serialization functions
size_t serialise_user_record_pk_size(struct user_record_pk *key) {
    size_t _sz = 0;
    _sz += TYPE_SIZEOF(u64, key->user_id);
    return _sz;
}

char* serialise_user_record_pk(char *buf, struct user_record_pk *key) {
    TYPE_ENC(u64, buf, key->user_id);
    return buf;
}

char* deserialise_user_record_pk(char *buf, struct user_record_pk *key) {
    TYPE_DEC(u64, buf, key->user_id);
    return buf;
}

// 3. Extractor
static inline void user_record_extract_pk(struct user_record *rec,
                                          struct user_record_pk *key) {
    key->user_id = rec->user_id;
}

// 4. KV operations
int kvstore_put_user_record(kvstore_txn_t *txn, struct user_record *rec,
                            kvstore_key_buf_t *old_keys) {
    // Implementation with key change detection...
}

int kvstore_get_user_record(kvstore_txn_t *txn, struct user_record_pk *key,
                            struct user_record *result,
                            kvstore_key_buf_t *key_buf) {
    // Implementation...
}

// ... etc
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

## Design Decisions and Rationale

### ✅ Index updates on modify
**Decision:** Auto-detect key changes using `kvstore_key_buf_t`
**Rationale:** The key buffer mechanism elegantly handles all update scenarios (insert, update with/without key changes) without requiring user code to manually track which indices changed.

### ✅ Memory management
**Decision:**
- Key serialization buffers: Stack (`alloca`) - short-lived, no cleanup needed
- Key buffer tracking: Heap (`malloc`) - user-owned, explicit cleanup with `kvstore_key_buf_free()`
- Full record buffers: Stack (`alloca`) for temporary serialization

**Rationale:** Stack allocation avoids heap fragmentation for hot-path operations. Key buffer is heap-allocated because it's optional and persists across function calls.

### ✅ Dedicated key structs
**Decision:** Generate `struct record_type_pk` and `struct record_type_index_name_key`
**Rationale:** Type-safe, minimal memory footprint, clear API semantics.

### Open Questions for Implementation

1. **Unique constraints**: Should secondary indices enforce uniqueness, or allow duplicates?
   - Current design: Allow duplicates (multiple records can have same secondary key value)
   - Alternative: Add `SERIALISE_UNIQUE_KEY` macro variant

2. **Prefix iteration**: Should cursors support prefix matching for efficient range queries?
   - Example: Find all users with email ending in "@example.com"
   - Could add `kvstore_cursor_*_prefix()` variants

3. **Sorting order**: Little-endian serialization provides byte-order sorting
   - For integers: Natural ascending order
   - For timestamps: Chronological order
   - For strings: Lexicographic order
   - Is this desired, or should we support descending/custom comparators?

4. **Error handling**: Return codes (current design) vs exceptions vs callbacks?
   - Current: Return `KVSTORE_OK`, `KVSTORE_NOTFOUND`, `KVSTORE_ERROR`, etc.
   - Could add optional error callback for detailed diagnostics

5. **Transaction isolation**: Should reads in a transaction see uncommitted writes?
   - Affects read-your-own-writes semantics
   - Backend-dependent (LMDB vs RocksDB behavior differs)

6. **Bulk operations**: Should we provide batch insert/delete helpers?
   - `kvstore_put_batch()` for multiple records
   - Useful for bulk loading scenarios

---

## Next Steps

1. Implement core macro infrastructure in `kvstore.h`
2. Create LMDB or twom backend adapter
3. Write comprehensive examples
4. Add unit tests for single/compound keys
5. Document performance characteristics
6. Consider adding helper macros for common patterns (e.g., auto-index maintenance)
