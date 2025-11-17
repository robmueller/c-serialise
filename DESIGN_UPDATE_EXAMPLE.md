# Example: Update with Key Change Detection

This example demonstrates how to update a record when key fields might have changed.

## Scenario: Change User's Email Address

```c
kvstore_t *db = kvstore_open("mydb.kv");
kvstore_txn_t *txn = kvstore_txn_begin(db, 0);

// Step 1: Fetch existing record
struct user_record_pk lookup_key = { .user_id = 12345 };
struct user_record user = {0};
kvstore_key_buf_t key_buf = KVSTORE_KEY_BUF_INIT;

int rc = kvstore_get_user_record(txn, &lookup_key, &user, &key_buf);
if (rc != KVSTORE_OK) {
    fprintf(stderr, "User not found\n");
    kvstore_txn_abort(txn);
    return -1;
}

// At this point:
// - user contains the full record
// - key_buf.buf contains serialized old keys:
//     [pk_len:4][pk_data][by_email_len:4][by_email_data][by_username_len:4][by_username_data]

// Step 2: Modify the record
free(user.email);
user.email = strdup("alice_new@example.com");  // Email changed (secondary key)
user.age = 31;  // Non-key field changed

// Step 3: Put updated record back
// The put function will:
// - Detect that by_email key changed
// - Delete old secondary index entry (alice@example.com -> user_id:12345)
// - Add new secondary index entry (alice_new@example.com -> user_id:12345)
// - Update primary table with new record data
rc = kvstore_put_user_record(txn, &user, &key_buf);
if (rc != KVSTORE_OK) {
    fprintf(stderr, "Update failed\n");
    kvstore_key_buf_free(&key_buf);
    kvstore_txn_abort(txn);
    return -1;
}

kvstore_key_buf_free(&key_buf);
kvstore_txn_commit(txn);
```

## What Happens Internally

### When `kvstore_get_user_record()` is called with non-NULL `key_buf`:

1. Fetch record from primary table
2. Deserialize full record into `user`
3. Serialize all keys (pk + all sks) into `key_buf->buf`:
   ```
   Offset  Length  Content
   0       4       pk_len = 8
   4       8       user_id = 12345 (serialized)
   12      4       by_email_len = 21 (4 bytes len + "alice@example.com")
   16      21      "alice@example.com" (4-byte len prefix + 17 bytes)
   37      4       by_username_len = 9 (4 bytes len + "alice")
   41      9       "alice" (4-byte len prefix + 5 bytes)
   ```

### When `kvstore_put_user_record()` is called with non-NULL `key_buf`:

1. **Serialize new keys:**
   - `new_pk = {user_id: 12345}`
   - `new_by_email = {email: "alice_new@example.com"}`
   - `new_by_username = {username: "alice"}`

2. **Parse old keys from buffer:**
   - `old_pk = {user_id: 12345}`
   - `old_by_email = {email: "alice@example.com"}`
   - `old_by_username = {username: "alice"}`

3. **Compare keys:**
   - PK unchanged: `12345 == 12345` ✓
   - by_email changed: `"alice@example.com" != "alice_new@example.com"` ✗
   - by_username unchanged: `"alice" == "alice"` ✓

4. **Execute updates:**
   - **Update primary table:**
     - Key: `user_id=12345`
     - Value: Full serialized record (with new email, new age)

   - **Update by_email index:**
     - Delete: Key=`"alice@example.com"`, Value=`user_id=12345`
     - Insert: Key=`"alice_new@example.com"`, Value=`user_id=12345`

   - **by_username index:** No change needed

## Scenario: Change Primary Key

```c
// Continuing from previous example...
txn = kvstore_txn_begin(db, 0);

// Fetch user
struct user_record_pk lookup = { .user_id = 12345 };
struct user_record user = {0};
kvstore_key_buf_t key_buf = KVSTORE_KEY_BUF_INIT;

kvstore_get_user_record(txn, &lookup, &user, &key_buf);

// Change primary key (rare but possible)
user.user_id = 99999;

// Put will detect PK change and do full reindex:
// 1. Delete old PK entry (12345)
// 2. Delete all old SK entries pointing to old PK (12345)
// 3. Insert new PK entry (99999)
// 4. Insert all new SK entries pointing to new PK (99999)
kvstore_put_user_record(txn, &user, &key_buf);

kvstore_key_buf_free(&key_buf);
kvstore_txn_commit(txn);
```

## Performance Considerations

### Insert (key_buf = NULL)
- 1 primary table write
- N secondary index writes (where N = number of secondary indices)

### Update without key changes
- 1 primary table write (value updated)
- 0 secondary index operations

### Update with M secondary keys changed (PK unchanged)
- 1 primary table write
- 2M secondary index operations (M deletes + M inserts)

### Update with PK changed
- 2 primary table operations (delete old + insert new)
- 2N secondary index operations (delete all old + insert all new)

## Memory Management

The `kvstore_key_buf_t` buffer is:
- Allocated/reallocated by `kvstore_get_*()` if size is insufficient
- Owned by the caller
- Must be freed with `kvstore_key_buf_free()` when done
- Can be reused across multiple get/put cycles

## Pattern: Read-Modify-Write Loop

```c
kvstore_key_buf_t key_buf = KVSTORE_KEY_BUF_INIT;

for (int i = 0; i < num_updates; i++) {
    txn = kvstore_txn_begin(db, 0);

    struct user_record_pk pk = { .user_id = user_ids[i] };
    struct user_record user = {0};

    // Reuse key_buf across iterations (avoids realloc if size sufficient)
    if (kvstore_get_user_record(txn, &pk, &user, &key_buf) == KVSTORE_OK) {
        // Modify...
        user.age++;

        // Put with change detection
        kvstore_put_user_record(txn, &user, &key_buf);
    }

    kvstore_txn_commit(txn);
}

// Clean up once at the end
kvstore_key_buf_free(&key_buf);
```
