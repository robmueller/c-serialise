// KV Store abstraction layer for C struct serialization
// Provides primary and secondary key indexing with automatic change tracking
// See DESIGN.md for full documentation

#ifndef KVSTORE_H_
#define KVSTORE_H_

#include "serialise.h"
#include <stdbool.h>
#include <assert.h>
#include <alloca.h>

#ifdef __cplusplus
extern "C" {
#endif

// ------------------------
// Forward declarations
// ------------------------

typedef struct kvstore kvstore_t;
typedef struct kvstore_txn kvstore_txn_t;
typedef struct kvstore_cursor kvstore_cursor_t;

// ------------------------
// Core types
// ------------------------

// Key-value pair for generic operations
typedef struct {
    void *data;
    size_t size;
} kvstore_val_t;

// Key buffer for tracking old keys during updates
typedef struct {
    char *buf;      // Buffer holding serialized keys (malloc'd, can be NULL)
    size_t size;    // Current buffer size
} kvstore_key_buf_t;

// Initialize to empty
#define KVSTORE_KEY_BUF_INIT { .buf = NULL, .size = 0 }

// Return codes
#define KVSTORE_OK        0
#define KVSTORE_NOTFOUND  1
#define KVSTORE_EXISTS    2
#define KVSTORE_ERROR    -1

// Free key buffer
static inline void kvstore_key_buf_free(kvstore_key_buf_t *kb) {
    if (kb && kb->buf) {
        free(kb->buf);
        kb->buf = NULL;
        kb->size = 0;
    }
}

// ------------------------
// Type mapping: serialization tags to C types
// ------------------------

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

// For custom types, define: #define SER_CTYPE_<tag> <type>

// ------------------------
// Key struct generation
// ------------------------

// Generate a struct field declaration from a field spec
#define KV_FIELD_DECL(t) KV_FIELD_DECL_I t
#define KV_FIELD_DECL_I(kind, ...) SER_CAT(KV_FIELD_DECL_, kind)(__VA_ARGS__)

#define KV_FIELD_DECL_SCALAR(name, type) \
    SER_CAT(SER_CTYPE_, SER_MAP(type)) name;

#define KV_FIELD_DECL_ARRAY(name, type, count) \
    SER_CAT(SER_CTYPE_, SER_MAP(type)) name[count];

// Generate complete key struct
#define KV_GENERATE_STRUCT(struct_name, ...) \
    struct struct_name { \
        FOR_EACH(KV_FIELD_DECL, __VA_ARGS__) \
    };

// ------------------------
// Key extraction functions
// ------------------------

// Generate field extraction statements
#define KV_EXTRACT_STMT(t) KV_EXTRACT_STMT_I t
#define KV_EXTRACT_STMT_I(kind, ...) SER_CAT(KV_EXTRACT_, kind)(__VA_ARGS__)

#define KV_EXTRACT_SCALAR(name, type) \
    do { key->name = rec->name; } while (0)

#define KV_EXTRACT_ARRAY(name, type, count) \
    do { memcpy(key->name, rec->name, sizeof(key->name)); } while (0)

// Generate key extraction function
#define KV_GENERATE_EXTRACTOR(rec_type, key_type, ...) \
    static inline void SER_CAT(rec_type, SER_CAT(_extract_, key_type))( \
        struct rec_type *rec, struct SER_CAT(rec_type, SER_CAT(_, key_type)) *key) { \
        FOR_EACH(KV_EXTRACT_STMT, __VA_ARGS__); \
    }

// ------------------------
// Key serialization functions (size/encode/decode)
// ------------------------

// Similar to SERIALISE macro but works on key struct pointer
#define KV_SERIALISE_KEY(rec_type, key_suffix, key_type, ...) \
size_t SER_CAT(serialise_, SER_CAT(rec_type, SER_CAT(_, SER_CAT(key_suffix, _size))))( \
    struct SER_CAT(rec_type, SER_CAT(_, key_type)) *key) { \
    size_t _sz = 0; \
    FOR_EACH(KV_ITEM_SIZE, __VA_ARGS__); \
    return _sz; \
} \
char* SER_CAT(serialise_, SER_CAT(rec_type, SER_CAT(_, key_suffix)))( \
    char *buf, struct SER_CAT(rec_type, SER_CAT(_, key_type)) *key) { \
    FOR_EACH(KV_ITEM_ENC, __VA_ARGS__); \
    return buf; \
} \
char* SER_CAT(deserialise_, SER_CAT(rec_type, SER_CAT(_, key_suffix)))( \
    char *buf, struct SER_CAT(rec_type, SER_CAT(_, key_type)) *key) { \
    FOR_EACH(KV_ITEM_DEC, __VA_ARGS__); \
    return buf; \
}

// Item handlers for keys (use key-> instead of r->)
#define KV_ITEM_SIZE(t) KV_ITEM_SIZE_I t
#define KV_ITEM_ENC(t)  KV_ITEM_ENC_I t
#define KV_ITEM_DEC(t)  KV_ITEM_DEC_I t

#define KV_ITEM_SIZE_I(kind, ...) SER_CAT(KV_ITEM_SIZE_, kind)(__VA_ARGS__)
#define KV_ITEM_ENC_I(kind, ...)  SER_CAT(KV_ITEM_ENC_,  kind)(__VA_ARGS__)
#define KV_ITEM_DEC_I(kind, ...)  SER_CAT(KV_ITEM_DEC_,  kind)(__VA_ARGS__)

// SCALAR handlers
#define KV_ITEM_SIZE_SCALAR(name, type) do { \
    _sz += TYPE_SIZEOF(SER_MAP(type), key->name); \
} while (0)

#define KV_ITEM_ENC_SCALAR(name, type) do { \
    TYPE_ENC(SER_MAP(type), buf, key->name); \
} while (0)

#define KV_ITEM_DEC_SCALAR(name, type) do { \
    TYPE_DEC(SER_MAP(type), buf, key->name); \
} while (0)

// ARRAY handlers
#define KV_ITEM_SIZE_ARRAY(name, type, count) do { \
    for (size_t _i = 0; _i < (size_t)(count); ++_i) { \
        _sz += TYPE_SIZEOF(SER_MAP(type), key->name[_i]); \
    } \
} while (0)

#define KV_ITEM_ENC_ARRAY(name, type, count) do { \
    for (size_t _i = 0; _i < (size_t)(count); ++_i) { \
        TYPE_ENC(SER_MAP(type), buf, key->name[_i]); \
    } \
} while (0)

#define KV_ITEM_DEC_ARRAY(name, type, count) do { \
    for (size_t _i = 0; _i < (size_t)(count); ++_i) { \
        TYPE_DEC(SER_MAP(type), buf, key->name[_i]); \
    } \
} while (0)

// ------------------------
// Backend interface (see kvstore_backend.h)
// ------------------------

// Raw KV operations
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

// ------------------------
// Primary key macro
// ------------------------

#define SERIALISE_PRIMARY_KEY(rec_type, key_name, ...) \
    /* Generate struct rec_type_pk */ \
    KV_GENERATE_STRUCT(SER_CAT(rec_type, _pk), __VA_ARGS__) \
    \
    /* Generate serialization functions */ \
    KV_SERIALISE_KEY(rec_type, pk, pk, __VA_ARGS__) \
    \
    /* Generate extractor function */ \
    KV_GENERATE_EXTRACTOR(rec_type, pk, __VA_ARGS__) \
    \
    /* Generate KV operations */ \
    KV_PRIMARY_OPS(rec_type, key_name, __VA_ARGS__)

// Helper to count secondary keys (used to know how many to serialize in key_buf)
#define KV_SK_COUNT_GLOBAL 0
#define KV_INCREMENT_SK_COUNT /* Incremented by each SERIALISE_SECONDARY_KEY */

// Generate primary table operations
#define KV_PRIMARY_OPS(rec_type, key_name, ...) \
\
/* PUT: Store record with key change detection */ \
static inline int SER_CAT(kvstore_put_, rec_type)( \
    kvstore_txn_t *txn, struct rec_type *rec, kvstore_key_buf_t *old_keys) { \
    \
    /* Extract and serialize new primary key */ \
    struct SER_CAT(rec_type, _pk) new_pk; \
    SER_CAT(rec_type, _extract_pk)(rec, &new_pk); \
    size_t new_pk_sz = SER_CAT(serialise_, SER_CAT(rec_type, _pk_size))(&new_pk); \
    char *new_pk_buf = (char*)alloca(new_pk_sz); \
    SER_CAT(serialise_, SER_CAT(rec_type, _pk))(new_pk_buf, &new_pk); \
    \
    bool is_update = (old_keys && old_keys->buf); \
    bool pk_changed = false; \
    \
    if (is_update) { \
        /* Parse old primary key from buffer */ \
        char *p = old_keys->buf; \
        uint32_t old_pk_len; \
        memcpy(&old_pk_len, p, 4); \
        p += 4; \
        char *old_pk_buf = p; \
        \
        /* Check if primary key changed */ \
        pk_changed = (old_pk_len != new_pk_sz || \
                     memcmp(old_pk_buf, new_pk_buf, new_pk_sz) != 0); \
        \
        if (pk_changed) { \
            /* Delete old primary entry */ \
            kvstore_val_t old_key = { old_pk_buf, old_pk_len }; \
            kvstore_txn_del(txn, #rec_type "_pk", &old_key); \
        } \
    } \
    \
    /* Serialize full record */ \
    size_t val_sz = SER_CAT(serialise_, SER_CAT(rec_type, _size))(rec); \
    char *val_buf = (char*)alloca(val_sz); \
    SER_CAT(serialise_, rec_type)(val_buf, rec); \
    \
    /* Store in primary table */ \
    kvstore_val_t key = { new_pk_buf, new_pk_sz }; \
    kvstore_val_t val = { val_buf, val_sz }; \
    int rc = kvstore_txn_put(txn, #rec_type "_pk", &key, &val); \
    if (rc != KVSTORE_OK) return rc; \
    \
    /* Note: Secondary index updates handled by their _internal functions */ \
    /* Called from user code or generated helper */ \
    \
    return KVSTORE_OK; \
} \
\
/* GET: Fetch record by primary key */ \
static inline int SER_CAT(kvstore_get_, rec_type)( \
    kvstore_txn_t *txn, struct SER_CAT(rec_type, _pk) *key, \
    struct rec_type *result, kvstore_key_buf_t *key_buf) { \
    \
    /* Serialize lookup key */ \
    size_t key_sz = SER_CAT(serialise_, SER_CAT(rec_type, _pk_size))(key); \
    char *key_buf_tmp = (char*)alloca(key_sz); \
    SER_CAT(serialise_, SER_CAT(rec_type, _pk))(key_buf_tmp, key); \
    \
    /* Fetch from KV store */ \
    kvstore_val_t k = { key_buf_tmp, key_sz }; \
    kvstore_val_t v = {0}; \
    int rc = kvstore_txn_get(txn, #rec_type "_pk", &k, &v); \
    if (rc != KVSTORE_OK) return rc; \
    \
    /* Deserialize result */ \
    SER_CAT(deserialise_, rec_type)((char*)v.data, result); \
    \
    /* If key_buf provided, populate all keys for change detection */ \
    /* NOTE: Requires SERIALISE_FINALIZE_INDICES to be called to define populate_key_buf_* */ \
    if (key_buf) { \
        SER_CAT(populate_key_buf_, rec_type)(result, key_buf); \
    } \
    \
    return KVSTORE_OK; \
} \
\
/* DELETE: Remove record by primary key */ \
static inline int SER_CAT(kvstore_del_, rec_type)( \
    kvstore_txn_t *txn, struct SER_CAT(rec_type, _pk) *key) { \
    \
    /* Serialize key */ \
    size_t key_sz = SER_CAT(serialise_, SER_CAT(rec_type, _pk_size))(key); \
    char *key_buf = (char*)alloca(key_sz); \
    SER_CAT(serialise_, SER_CAT(rec_type, _pk))(key_buf, key); \
    \
    kvstore_val_t k = { key_buf, key_sz }; \
    return kvstore_txn_del(txn, #rec_type "_pk", &k); \
} \
\
/* CURSOR: Iterate primary key table */ \
static inline kvstore_cursor_t* SER_CAT(kvstore_cursor_, SER_CAT(rec_type, _pk))( \
    kvstore_txn_t *txn, struct SER_CAT(rec_type, _pk) *start_key) { \
    \
    kvstore_val_t start = {0}; \
    if (start_key) { \
        size_t key_sz = SER_CAT(serialise_, SER_CAT(rec_type, _pk_size))(start_key); \
        char *key_buf = (char*)alloca(key_sz); \
        SER_CAT(serialise_, SER_CAT(rec_type, _pk))(key_buf, start_key); \
        start.data = key_buf; \
        start.size = key_sz; \
    } \
    \
    return kvstore_cursor_open(txn, #rec_type "_pk", start_key ? &start : NULL); \
}

// ------------------------
// Secondary key macro
// ------------------------

#define SERIALISE_SECONDARY_KEY(rec_type, index_name, ...) \
    /* Generate struct rec_type_index_name_key */ \
    KV_GENERATE_STRUCT(SER_CAT(SER_CAT(rec_type, _), SER_CAT(index_name, _key)), \
                       __VA_ARGS__) \
    \
    /* Generate serialization functions */ \
    KV_SERIALISE_KEY(rec_type, index_name, SER_CAT(index_name, _key), __VA_ARGS__) \
    \
    /* Generate extractor */ \
    KV_GENERATE_EXTRACTOR_SK(rec_type, index_name, __VA_ARGS__) \
    \
    /* Generate KV operations */ \
    KV_SECONDARY_OPS(rec_type, index_name, __VA_ARGS__)

// Generate extractor for secondary key
#define KV_GENERATE_EXTRACTOR_SK(rec_type, index_name, ...) \
    static inline void SER_CAT(rec_type, SER_CAT(_extract_, index_name))( \
        struct rec_type *rec, \
        struct SER_CAT(SER_CAT(rec_type, _), SER_CAT(index_name, _key)) *key) { \
        FOR_EACH(KV_EXTRACT_STMT, __VA_ARGS__); \
    }

// Generate secondary table operations
#define KV_SECONDARY_OPS(rec_type, index_name, ...) \
\
/* LOOKUP: Secondary key -> Primary key */ \
static inline int SER_CAT(kvstore_lookup_, SER_CAT(rec_type, SER_CAT(_, index_name)))( \
    kvstore_txn_t *txn, \
    struct SER_CAT(SER_CAT(rec_type, _), SER_CAT(index_name, _key)) *sec_key, \
    struct SER_CAT(rec_type, _pk) *pri_key_out) { \
    \
    /* Serialize secondary key */ \
    size_t sk_sz = SER_CAT(serialise_, SER_CAT(rec_type, SER_CAT(_, SER_CAT(index_name, _size))))(sec_key); \
    char *sk_buf = (char*)alloca(sk_sz); \
    SER_CAT(serialise_, SER_CAT(rec_type, SER_CAT(_, index_name)))(sk_buf, sec_key); \
    \
    /* Lookup in secondary index table */ \
    kvstore_val_t k = { sk_buf, sk_sz }; \
    kvstore_val_t v = {0}; \
    int rc = kvstore_txn_get(txn, #rec_type "_" #index_name, &k, &v); \
    if (rc != KVSTORE_OK) return rc; \
    \
    /* Deserialize primary key from value */ \
    SER_CAT(deserialise_, SER_CAT(rec_type, _pk))((char*)v.data, pri_key_out); \
    \
    return KVSTORE_OK; \
} \
\
/* CURSOR: Iterate secondary index */ \
static inline kvstore_cursor_t* SER_CAT(kvstore_cursor_, SER_CAT(rec_type, SER_CAT(_, index_name)))( \
    kvstore_txn_t *txn, \
    struct SER_CAT(SER_CAT(rec_type, _), SER_CAT(index_name, _key)) *start_key) { \
    \
    kvstore_val_t start = {0}; \
    if (start_key) { \
        size_t key_sz = SER_CAT(serialise_, SER_CAT(rec_type, SER_CAT(_, SER_CAT(index_name, _size))))(start_key); \
        char *key_buf = (char*)alloca(key_sz); \
        SER_CAT(serialise_, SER_CAT(rec_type, SER_CAT(_, index_name)))(key_buf, start_key); \
        start.data = key_buf; \
        start.size = key_sz; \
    } \
    \
    return kvstore_cursor_open(txn, #rec_type "_" #index_name, start_key ? &start : NULL); \
} \
\
/* INTERNAL PUT: Add/update secondary index entry */ \
static inline int SER_CAT(kvstore_put_, SER_CAT(rec_type, SER_CAT(_, SER_CAT(index_name, _internal))))( \
    kvstore_txn_t *txn, struct rec_type *rec, char *pk_buf, size_t pk_sz) { \
    \
    /* Extract and serialize secondary key */ \
    struct SER_CAT(SER_CAT(rec_type, _), SER_CAT(index_name, _key)) sk; \
    SER_CAT(rec_type, SER_CAT(_extract_, index_name))(rec, &sk); \
    size_t sk_sz = SER_CAT(serialise_, SER_CAT(rec_type, SER_CAT(_, SER_CAT(index_name, _size))))(&sk); \
    char *sk_buf = (char*)alloca(sk_sz); \
    SER_CAT(serialise_, SER_CAT(rec_type, SER_CAT(_, index_name)))(sk_buf, &sk); \
    \
    /* Store: secondary_key -> primary_key */ \
    kvstore_val_t k = { sk_buf, sk_sz }; \
    kvstore_val_t v = { pk_buf, pk_sz }; \
    \
    return kvstore_txn_put(txn, #rec_type "_" #index_name, &k, &v); \
} \
\
/* INTERNAL DELETE: Remove secondary index entry */ \
static inline int SER_CAT(kvstore_del_, SER_CAT(rec_type, SER_CAT(_, SER_CAT(index_name, _internal))))( \
    kvstore_txn_t *txn, char *sk_buf, size_t sk_sz) { \
    \
    kvstore_val_t k = { sk_buf, sk_sz }; \
    return kvstore_txn_del(txn, #rec_type "_" #index_name, &k); \
}

// ------------------------
// Index finalization macro - generates helper functions
// ------------------------

// Helper macros for iterating over secondary keys
#define KV_POPULATE_SK_SIZE(rec_type, sk_name) \
    struct SER_CAT(SER_CAT(rec_type, _), SER_CAT(sk_name, _key)) SER_CAT(sk_, sk_name); \
    SER_CAT(rec_type, SER_CAT(_extract_, sk_name))(rec, &SER_CAT(sk_, sk_name)); \
    size_t SER_CAT(sk_, SER_CAT(sk_name, _sz)) = \
        SER_CAT(serialise_, SER_CAT(rec_type, SER_CAT(_, SER_CAT(sk_name, _size))))(&SER_CAT(sk_, sk_name)); \
    total += 4 + SER_CAT(sk_, SER_CAT(sk_name, _sz));

#define KV_POPULATE_SK_DATA(rec_type, sk_name) \
    len = (uint32_t)SER_CAT(sk_, SER_CAT(sk_name, _sz)); \
    memcpy(p, &len, 4); p += 4; \
    SER_CAT(serialise_, SER_CAT(rec_type, SER_CAT(_, sk_name)))(p, &SER_CAT(sk_, sk_name)); \
    p += SER_CAT(sk_, SER_CAT(sk_name, _sz));

#define KV_PUT_SK_WITH_CHANGE_DETECT(rec_type, sk_name, sk_idx) \
    char *SER_CAT(new_sk_, SER_CAT(sk_name, _buf)) = (char*)alloca(SER_CAT(sk_, SER_CAT(sk_name, _sz))); \
    SER_CAT(serialise_, SER_CAT(rec_type, SER_CAT(_, sk_name)))(SER_CAT(new_sk_, SER_CAT(sk_name, _buf)), \
                                                                  &SER_CAT(sk_, sk_name)); \
    \
    if (old_keys && old_keys->buf) { \
        bool SER_CAT(sk_, SER_CAT(sk_name, _changed)) = \
            (old_sk_lens[sk_idx] != SER_CAT(sk_, SER_CAT(sk_name, _sz)) || \
             memcmp(old_sk_bufs[sk_idx], SER_CAT(new_sk_, SER_CAT(sk_name, _buf)), \
                    SER_CAT(sk_, SER_CAT(sk_name, _sz))) != 0); \
        if (SER_CAT(sk_, SER_CAT(sk_name, _changed))) { \
            SER_CAT(kvstore_del_, SER_CAT(rec_type, SER_CAT(_, SER_CAT(sk_name, _internal))))( \
                txn, old_sk_bufs[sk_idx], old_sk_lens[sk_idx]); \
        } \
    } \
    \
    rc = SER_CAT(kvstore_put_, SER_CAT(rec_type, SER_CAT(_, SER_CAT(sk_name, _internal))))( \
        txn, rec, pk_buf, pk_sz); \
    if (rc != KVSTORE_OK) return rc;

// Forward declaration for populate_key_buf function
// Must be called BEFORE SERIALISE_PRIMARY_KEY to enable automatic key_buf population
#define SERIALISE_DECLARE_KEYS(rec_type) \
    static inline void SER_CAT(populate_key_buf_, rec_type)(struct rec_type *rec, kvstore_key_buf_t *key_buf);

// Generate populate_key_buf and put_with_all_indices functions
// Usage: SERIALISE_FINALIZE_INDICES(record_type, sk1, sk2, sk3, ...)
#define SERIALISE_FINALIZE_INDICES(rec_type, ...) \
\
/* Generate populate_key_buf function */ \
static inline void SER_CAT(populate_key_buf_, rec_type)( \
    struct rec_type *rec, kvstore_key_buf_t *key_buf) { \
    \
    /* Extract primary key */ \
    struct SER_CAT(rec_type, _pk) pk; \
    SER_CAT(rec_type, _extract_pk)(rec, &pk); \
    size_t pk_sz = SER_CAT(serialise_, SER_CAT(rec_type, _pk_size))(&pk); \
    \
    /* Extract all secondary keys and calculate total size */ \
    size_t total = 4 + pk_sz; \
    KV_FINALIZE_FOR_EACH(KV_POPULATE_SK_SIZE, rec_type, __VA_ARGS__) \
    \
    /* Allocate buffer */ \
    if (!key_buf->buf || key_buf->size < total) { \
        key_buf->buf = (char*)realloc(key_buf->buf, total); \
        key_buf->size = total; \
    } \
    \
    /* Serialize all keys into buffer */ \
    char *p = key_buf->buf; \
    uint32_t len; \
    \
    /* Primary key */ \
    len = (uint32_t)pk_sz; \
    memcpy(p, &len, 4); p += 4; \
    SER_CAT(serialise_, SER_CAT(rec_type, _pk))(p, &pk); p += pk_sz; \
    \
    /* Secondary keys */ \
    KV_FINALIZE_FOR_EACH(KV_POPULATE_SK_DATA, rec_type, __VA_ARGS__) \
} \
\
/* Generate put_with_all_indices function */ \
static inline int SER_CAT(kvstore_put_, SER_CAT(rec_type, _with_all_indices))( \
    kvstore_txn_t *txn, struct rec_type *rec, kvstore_key_buf_t *old_keys) { \
    \
    int rc; \
    \
    /* Put to primary table */ \
    rc = SER_CAT(kvstore_put_, rec_type)(txn, rec, old_keys); \
    if (rc != KVSTORE_OK) return rc; \
    \
    /* Extract and serialize primary key */ \
    struct SER_CAT(rec_type, _pk) pk; \
    SER_CAT(rec_type, _extract_pk)(rec, &pk); \
    size_t pk_sz = SER_CAT(serialise_, SER_CAT(rec_type, _pk_size))(&pk); \
    char *pk_buf = (char*)alloca(pk_sz); \
    SER_CAT(serialise_, SER_CAT(rec_type, _pk))(pk_buf, &pk); \
    \
    /* Extract all secondary keys */ \
    size_t total = 0; \
    KV_FINALIZE_FOR_EACH(KV_POPULATE_SK_SIZE, rec_type, __VA_ARGS__) \
    \
    /* Parse old keys if updating */ \
    char *old_sk_bufs[KV_COUNT_ARGS(__VA_ARGS__)]; \
    uint32_t old_sk_lens[KV_COUNT_ARGS(__VA_ARGS__)]; \
    \
    if (old_keys && old_keys->buf) { \
        char *p = old_keys->buf; \
        \
        /* Skip primary key */ \
        uint32_t old_pk_len; \
        memcpy(&old_pk_len, p, 4); \
        p += 4 + old_pk_len; \
        \
        /* Extract old secondary keys */ \
        for (size_t i = 0; i < KV_COUNT_ARGS(__VA_ARGS__); i++) { \
            memcpy(&old_sk_lens[i], p, 4); \
            p += 4; \
            old_sk_bufs[i] = p; \
            p += old_sk_lens[i]; \
        } \
    } \
    \
    /* Update each secondary index with change detection */ \
    KV_FINALIZE_INDEXED_FOR_EACH(KV_PUT_SK_WITH_CHANGE_DETECT, rec_type, __VA_ARGS__) \
    \
    return KVSTORE_OK; \
}

// Helper to count arguments at preprocessing time
#define KV_COUNT_ARGS(...) \
    KV_COUNT_IMPL(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define KV_COUNT_IMPL(_1,_2,_3,_4,_5,_6,_7,_8, N, ...) N

// For-each with record type parameter
#define KV_FINALIZE_FOR_EACH(M, rec_type, ...) \
    KV_FINALIZE_FOR_EACH_IMPL(M, rec_type, ##__VA_ARGS__)

#define KV_FINALIZE_FOR_EACH_IMPL(M, rec_type, ...) \
    KV_FINALIZE_GET_MACRO(__VA_ARGS__, \
        KV_FE_8, KV_FE_7, KV_FE_6, KV_FE_5, KV_FE_4, KV_FE_3, KV_FE_2, KV_FE_1, KV_FE_0) \
    (M, rec_type, ##__VA_ARGS__)

// Indexed for-each (passes index as third parameter)
#define KV_FINALIZE_INDEXED_FOR_EACH(M, rec_type, ...) \
    KV_FINALIZE_INDEXED_IMPL(M, rec_type, 0, ##__VA_ARGS__)

#define KV_FINALIZE_INDEXED_IMPL(M, rec_type, idx, ...) \
    KV_FINALIZE_GET_MACRO(__VA_ARGS__, \
        KV_IFE_8, KV_IFE_7, KV_IFE_6, KV_IFE_5, KV_IFE_4, KV_IFE_3, KV_IFE_2, KV_IFE_1, KV_IFE_0) \
    (M, rec_type, idx, ##__VA_ARGS__)

#define KV_FINALIZE_GET_MACRO(_1,_2,_3,_4,_5,_6,_7,_8, NAME, ...) NAME

#define KV_FE_0(M, rt)
#define KV_FE_1(M, rt, X) M(rt, X)
#define KV_FE_2(M, rt, X, ...) M(rt, X) KV_FE_1(M, rt, __VA_ARGS__)
#define KV_FE_3(M, rt, X, ...) M(rt, X) KV_FE_2(M, rt, __VA_ARGS__)
#define KV_FE_4(M, rt, X, ...) M(rt, X) KV_FE_3(M, rt, __VA_ARGS__)
#define KV_FE_5(M, rt, X, ...) M(rt, X) KV_FE_4(M, rt, __VA_ARGS__)
#define KV_FE_6(M, rt, X, ...) M(rt, X) KV_FE_5(M, rt, __VA_ARGS__)
#define KV_FE_7(M, rt, X, ...) M(rt, X) KV_FE_6(M, rt, __VA_ARGS__)
#define KV_FE_8(M, rt, X, ...) M(rt, X) KV_FE_7(M, rt, __VA_ARGS__)

#define KV_IFE_0(M, rt, idx)
#define KV_IFE_1(M, rt, idx, X) M(rt, X, idx)
#define KV_IFE_2(M, rt, idx, X, ...) M(rt, X, idx) KV_IFE_1(M, rt, idx+1, __VA_ARGS__)
#define KV_IFE_3(M, rt, idx, X, ...) M(rt, X, idx) KV_IFE_2(M, rt, idx+1, __VA_ARGS__)
#define KV_IFE_4(M, rt, idx, X, ...) M(rt, X, idx) KV_IFE_3(M, rt, idx+1, __VA_ARGS__)
#define KV_IFE_5(M, rt, idx, X, ...) M(rt, X, idx) KV_IFE_4(M, rt, idx+1, __VA_ARGS__)
#define KV_IFE_6(M, rt, idx, X, ...) M(rt, X, idx) KV_IFE_5(M, rt, idx+1, __VA_ARGS__)
#define KV_IFE_7(M, rt, idx, X, ...) M(rt, X, idx) KV_IFE_6(M, rt, idx+1, __VA_ARGS__)
#define KV_IFE_8(M, rt, idx, X, ...) M(rt, X, idx) KV_IFE_7(M, rt, idx+1, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // KVSTORE_H_
