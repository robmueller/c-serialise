// Comprehensive KV store example with user records
// Demonstrates primary keys, secondary keys, updates, and iteration

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "../include/kvstore.h"
#include "../include/kvstore_backend.h"

// Forward declaration
extern kvstore_t* kvstore_open_mem(void);

// ------------------------
// Define user record struct
// ------------------------

struct user_record {
    uint64_t user_id;
    char *email;
    char *username;
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

// Define primary key (user_id)
SERIALISE_PRIMARY_KEY(user_record, pk,
    SERIALISE_FIELD(user_id, uint64_t)
)

// Define secondary index by email
SERIALISE_SECONDARY_KEY(user_record, by_email,
    SERIALISE_FIELD(email, charptr)
)

// Define secondary index by username
SERIALISE_SECONDARY_KEY(user_record, by_username,
    SERIALISE_FIELD(username, charptr)
)

// Generate helper functions for key management and index updates
SERIALISE_FINALIZE_INDICES(user_record, by_email, by_username)

// ------------------------
// Helper to create user
// ------------------------

static struct user_record create_user(uint64_t id, const char *email,
                                     const char *username, uint32_t age,
                                     uint64_t balance) {
    struct user_record user = {0};
    user.user_id = id;
    user.email = strdup(email);
    user.username = strdup(username);
    user.age = age;
    clock_gettime(CLOCK_REALTIME, &user.created);
    user.account_balance = balance;
    return user;
}

static void free_user(struct user_record *user) {
    free(user->email);
    free(user->username);
}

// ------------------------
// Main example
// ------------------------

int main(void) {
    printf("=== KV Store Example ===\n\n");

    // Open in-memory database
    kvstore_t *db = kvstore_open_mem();
    assert(db != NULL);

    // Test 1: Insert users
    printf("Test 1: Inserting users...\n");
    {
        kvstore_txn_t *txn = kvstore_txn_begin(db, false);
        assert(txn != NULL);

        struct user_record alice = create_user(1001, "alice@example.com",
                                              "alice", 30, 100000);
        struct user_record bob = create_user(1002, "bob@example.com",
                                            "bob", 25, 50000);
        struct user_record charlie = create_user(1003, "charlie@example.com",
                                                "charlie", 35, 75000);

        assert(kvstore_put_user_record_with_all_indices(txn, &alice, NULL) == KVSTORE_OK);
        assert(kvstore_put_user_record_with_all_indices(txn, &bob, NULL) == KVSTORE_OK);
        assert(kvstore_put_user_record_with_all_indices(txn, &charlie, NULL) == KVSTORE_OK);

        kvstore_txn_commit(txn);

        free_user(&alice);
        free_user(&bob);
        free_user(&charlie);

        printf("  ✓ Inserted 3 users\n");
    }

    // Test 2: Lookup by primary key
    printf("\nTest 2: Lookup by primary key...\n");
    {
        kvstore_txn_t *txn = kvstore_txn_begin(db, true);
        struct user_record_pk key = { .user_id = 1002 };
        struct user_record result = {0};

        int rc = kvstore_get_user_record(txn, &key, &result, NULL);
        assert(rc == KVSTORE_OK);
        assert(result.user_id == 1002);
        assert(strcmp(result.username, "bob") == 0);
        assert(strcmp(result.email, "bob@example.com") == 0);
        assert(result.age == 25);
        assert(result.account_balance == 50000);

        printf("  ✓ Found user %llu: %s (%s), age %u, balance %llu\n",
               (unsigned long long)result.user_id, result.username, result.email,
               result.age, (unsigned long long)result.account_balance);

        free_user(&result);
        kvstore_txn_commit(txn);
    }

    // Test 3: Lookup by secondary key (email)
    printf("\nTest 3: Lookup by secondary key (email)...\n");
    {
        kvstore_txn_t *txn = kvstore_txn_begin(db, true);
        struct user_record_by_email_key email_key = { .email = "alice@example.com" };
        struct user_record_pk pri_key = {0};

        // First: lookup secondary index to get primary key
        int rc = kvstore_lookup_user_record_by_email(txn, &email_key, &pri_key);
        assert(rc == KVSTORE_OK);
        assert(pri_key.user_id == 1001);

        // Second: lookup primary table to get full record
        struct user_record result = {0};
        rc = kvstore_get_user_record(txn, &pri_key, &result, NULL);
        assert(rc == KVSTORE_OK);
        assert(strcmp(result.username, "alice") == 0);

        printf("  ✓ Found user by email: %llu (%s)\n",
               (unsigned long long)result.user_id, result.username);

        free_user(&result);
        kvstore_txn_commit(txn);
    }

    // Test 4: Iterate all users (primary key order)
    printf("\nTest 4: Iterate all users (primary key order)...\n");
    {
        kvstore_txn_t *txn = kvstore_txn_begin(db, true);
        struct user_record_pk start_key = { .user_id = 0 };
        kvstore_cursor_t *cur = kvstore_cursor_user_record_pk(txn, &start_key);
        assert(cur != NULL);

        int count = 0;
        kvstore_val_t key_val, rec_val;
        while (kvstore_cursor_get(cur, &key_val, &rec_val) == KVSTORE_OK) {
            struct user_record rec = {0};
            deserialise_user_record((char*)rec_val.data, &rec);

            printf("  %d. User %llu: %s (%s)\n",
                   ++count, (unsigned long long)rec.user_id,
                   rec.username, rec.email);

            free_user(&rec);

            if (kvstore_cursor_next(cur) != KVSTORE_OK) break;
        }

        assert(count == 3);
        printf("  ✓ Iterated %d users\n", count);

        kvstore_cursor_close(cur);
        kvstore_txn_commit(txn);
    }

    // Test 5: Update with key change detection
    printf("\nTest 5: Update user (change email)...\n");
    {
        // Get existing user
        kvstore_txn_t *txn = kvstore_txn_begin(db, false);
        struct user_record_pk key = { .user_id = 1002 };
        struct user_record user = {0};
        kvstore_key_buf_t key_buf = KVSTORE_KEY_BUF_INIT;

        int rc = kvstore_get_user_record(txn, &key, &user, NULL);
        
        // Populate key_buf with all current keys
        populate_key_buf_user_record(&user, &key_buf);
        assert(rc == KVSTORE_OK);

        printf("  Before: %s (%s)\n", user.username, user.email);

        // Modify email (secondary key)
        free(user.email);
        user.email = strdup("bob_new@example.com");

        // Put will detect email change and update index
        rc = kvstore_put_user_record_with_all_indices(txn, &user, &key_buf);
        assert(rc == KVSTORE_OK);

        printf("  After:  %s (%s)\n", user.username, user.email);

        kvstore_key_buf_free(&key_buf);
        free_user(&user);
        kvstore_txn_commit(txn);

        printf("  ✓ Updated email\n");
    }

    // Test 6: Verify old email no longer works
    printf("\nTest 6: Verify old email lookup fails...\n");
    {
        kvstore_txn_t *txn = kvstore_txn_begin(db, true);
        struct user_record_by_email_key old_email = { .email = "bob@example.com" };
        struct user_record_pk pri_key = {0};

        int rc = kvstore_lookup_user_record_by_email(txn, &old_email, &pri_key);
        assert(rc == KVSTORE_NOTFOUND);

        printf("  ✓ Old email not found (as expected)\n");

        kvstore_txn_commit(txn);
    }

    // Test 7: Verify new email works
    printf("\nTest 7: Verify new email lookup works...\n");
    {
        kvstore_txn_t *txn = kvstore_txn_begin(db, true);
        struct user_record_by_email_key new_email = { .email = "bob_new@example.com" };
        struct user_record_pk pri_key = {0};

        int rc = kvstore_lookup_user_record_by_email(txn, &new_email, &pri_key);
        assert(rc == KVSTORE_OK);
        assert(pri_key.user_id == 1002);

        printf("  ✓ New email found user %llu\n",
               (unsigned long long)pri_key.user_id);

        kvstore_txn_commit(txn);
    }

    // Test 8: Delete user
    printf("\nTest 8: Delete user...\n");
    {
        kvstore_txn_t *txn = kvstore_txn_begin(db, false);
        struct user_record_pk key = { .user_id = 1003 };

        int rc = kvstore_del_user_record(txn, &key);
        assert(rc == KVSTORE_OK);

        kvstore_txn_commit(txn);

        printf("  ✓ Deleted user 1003\n");
    }

    // Test 9: Verify deletion
    printf("\nTest 9: Verify user deleted...\n");
    {
        kvstore_txn_t *txn = kvstore_txn_begin(db, true);
        struct user_record_pk key = { .user_id = 1003 };
        struct user_record result = {0};

        int rc = kvstore_get_user_record(txn, &key, &result, NULL);
        assert(rc == KVSTORE_NOTFOUND);

        printf("  ✓ User 1003 not found (as expected)\n");

        kvstore_txn_commit(txn);
    }

    // Cleanup
    kvstore_close(db);

    printf("\n=== All tests passed! ===\n");
    return 0;
}
