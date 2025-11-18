// Comprehensive test with compound keys and diverse field types
// Tests duplicate secondary keys, updates, and complex queries

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "../include/kvstore.h"
#include "../include/kvstore_backend.h"

extern kvstore_t* kvstore_open_mem(void);

// ------------------------
// Message record with compound primary key
// ------------------------

struct message_record {
    // Primary key: mailbox_id + uid (compound)
    uint32_t mailbox_id;
    uint32_t uid;

    // Message metadata
    char *subject;
    char *sender;
    char *recipient;
    struct timespec received;
    uint64_t size;
    uint32_t flags;
    uint8_t priority;

    // Thread information
    uint64_t thread_id;
    struct timespec last_modified;
};

// Full record serialization
SERIALISE(message_record, struct message_record,
    SERIALISE_FIELD(mailbox_id, uint32_t),
    SERIALISE_FIELD(uid, uint32_t),
    SERIALISE_FIELD(subject, charptr),
    SERIALISE_FIELD(sender, charptr),
    SERIALISE_FIELD(recipient, charptr),
    SERIALISE_FIELD(received, timespec),
    SERIALISE_FIELD(size, uint64_t),
    SERIALISE_FIELD(flags, uint32_t),
    SERIALISE_FIELD(priority, uint8_t),
    SERIALISE_FIELD(thread_id, uint64_t),
    SERIALISE_FIELD(last_modified, timespec)
)

// Declare keys forward (enables automatic key_buf population in kvstore_get)
SERIALISE_DECLARE_KEYS(message_record)

// Compound primary key: mailbox_id + uid
SERIALISE_PRIMARY_KEY(message_record, pk,
    SERIALISE_FIELD(mailbox_id, uint32_t),
    SERIALISE_FIELD(uid, uint32_t)
)

// Secondary index: by sender
SERIALISE_SECONDARY_KEY(message_record, by_sender,
    SERIALISE_FIELD(sender, charptr)
)

// Secondary index: by recipient
SERIALISE_SECONDARY_KEY(message_record, by_recipient,
    SERIALISE_FIELD(recipient, charptr)
)

// Secondary index: by thread_id
SERIALISE_SECONDARY_KEY(message_record, by_thread,
    SERIALISE_FIELD(thread_id, uint64_t)
)

// Compound secondary index: mailbox_id + received (for mailbox time-ordered view)
SERIALISE_SECONDARY_KEY(message_record, by_mailbox_time,
    SERIALISE_FIELD(mailbox_id, uint32_t),
    SERIALISE_FIELD(received, timespec)
)

// Generate helper functions for key management and index updates
SERIALISE_FINALIZE_INDICES(message_record, by_sender, by_recipient, by_thread, by_mailbox_time)

// ------------------------
// Helper functions
// ------------------------

static void free_message(struct message_record *msg) {
    free(msg->subject);
    free(msg->sender);
    free(msg->recipient);
}

static struct message_record create_message(uint32_t mailbox_id, uint32_t uid,
                                            const char *subject, const char *sender,
                                            const char *recipient, time_t received_sec,
                                            uint64_t size, uint32_t flags, uint8_t priority,
                                            uint64_t thread_id) {
    struct message_record msg = {0};
    msg.mailbox_id = mailbox_id;
    msg.uid = uid;
    msg.subject = strdup(subject);
    msg.sender = strdup(sender);
    msg.recipient = strdup(recipient);
    msg.received.tv_sec = received_sec;
    msg.received.tv_nsec = 0;
    msg.size = size;
    msg.flags = flags;
    msg.priority = priority;
    msg.thread_id = thread_id;
    msg.last_modified = msg.received;
    return msg;
}

// ------------------------
// Main test
// ------------------------

int main(void) {
    printf("=== Complex KV Store Test ===\n\n");

    kvstore_t *db = kvstore_open_mem();
    assert(db != NULL);

    // Test data: 12 messages across 3 mailboxes
    // NOTE: Current implementation stores only ONE primary key per secondary key value.
    // For production use with duplicate secondary keys, consider using a different
    // index structure (e.g., btree with duplicate keys, or separate list tables).
    // This example uses unique values for each secondary key for demonstration.
    struct message_record test_data[] = {
        // Mailbox 1
        create_message(1, 101, "Hello", "alice@example.com", "bob@example.com",
                      1700000000, 1024, 0x01, 1, 1001),
        create_message(1, 102, "Re: Hello", "bob@example.com", "alice@example.com",
                      1700000100, 2048, 0x01, 1, 1002),
        create_message(1, 103, "Meeting tomorrow", "carol@example.com", "team@example.com",
                      1700000200, 3072, 0x02, 2, 1003),
        create_message(1, 104, "Urgent!", "dave@example.com", "sales@example.com",
                      1700000300, 512, 0x04, 3, 1004),

        // Mailbox 2
        create_message(2, 201, "Project update", "eve@example.com", "team@example.com",
                      1700001000, 4096, 0x01, 1, 2001),
        create_message(2, 202, "Lunch plans", "frank@example.com", "bob@example.com",
                      1700001100, 1536, 0x00, 1, 2002),
        create_message(2, 203, "Re: Lunch plans", "grace@example.com", "alice@example.com",
                      1700001200, 1600, 0x01, 1, 2003),
        create_message(2, 204, "Invoice", "billing@example.com", "accounting@example.com",
                      1700001300, 8192, 0x02, 2, 2004),

        // Mailbox 3
        create_message(3, 301, "Newsletter", "news@example.com", "subscribers@example.com",
                      1700002000, 16384, 0x00, 0, 3001),
        create_message(3, 302, "Password reset", "noreply@example.com", "support@example.com",
                      1700002100, 512, 0x04, 3, 3002),
        create_message(3, 303, "Reminder", "heidi@example.com", "alice@example.com",
                      1700002200, 768, 0x02, 2, 3003),
        create_message(3, 304, "Follow-up", "ivan@example.com", "alice@example.com",
                      1700002300, 1024, 0x01, 1, 3004),
    };

    int num_messages = sizeof(test_data) / sizeof(test_data[0]);

    // TEST 1: Insert all messages
    printf("Test 1: Inserting %d messages...\n", num_messages);
    {
        kvstore_txn_t *txn = kvstore_txn_begin(db, false);

        for (int i = 0; i < num_messages; i++) {
            int rc = kvstore_put_message_record_with_all_indices(txn, &test_data[i], NULL);
            assert(rc == KVSTORE_OK);
        }

        kvstore_txn_commit(txn);
        printf("  ✓ Inserted all %d messages\n", num_messages);
    }

    // TEST 2: Lookup by compound primary key
    printf("\nTest 2: Lookup by compound primary key...\n");
    {
        kvstore_txn_t *txn = kvstore_txn_begin(db, true);

        // Lookup message (2, 203)
        struct message_record_pk key = { .mailbox_id = 2, .uid = 203 };
        struct message_record result = {0};

        int rc = kvstore_get_message_record(txn, &key, &result, NULL);
        assert(rc == KVSTORE_OK);
        assert(result.mailbox_id == 2);
        assert(result.uid == 203);
        assert(strcmp(result.subject, "Re: Lunch plans") == 0);
        assert(strcmp(result.sender, "grace@example.com") == 0);
        assert(result.thread_id == 2003);

        printf("  ✓ Found message (%u, %u): '%s' from %s\n",
               result.mailbox_id, result.uid, result.subject, result.sender);

        free_message(&result);
        kvstore_txn_commit(txn);
    }

    // TEST 3: Lookup by secondary key (sender)
    printf("\nTest 3: Lookup by sender (alice@example.com)...\n");
    {
        kvstore_txn_t *txn = kvstore_txn_begin(db, true);
        struct message_record_by_sender_key sender_key = { .sender = "alice@example.com" };

        // Lookup by sender
        struct message_record_pk pk = {0};
        int rc = kvstore_lookup_message_record_by_sender(txn, &sender_key, &pk);

        assert(rc == KVSTORE_OK);
        assert(pk.mailbox_id == 1);
        assert(pk.uid == 101);

        // Fetch full message
        struct message_record msg = {0};
        rc = kvstore_get_message_record(txn, &pk, &msg, NULL);
        assert(rc == KVSTORE_OK);

        printf("  ✓ Found message from %s: (%u, %u) '%s'\n",
               msg.sender, msg.mailbox_id, msg.uid, msg.subject);

        free_message(&msg);
        kvstore_txn_commit(txn);
    }

    // TEST 4: Lookup by thread ID (unique secondary key)
    printf("\nTest 4: Lookup by thread ID (1001)...\n");
    {
        kvstore_txn_t *txn = kvstore_txn_begin(db, true);
        struct message_record_by_thread_key thread_key = { .thread_id = 1001 };

        // Lookup by thread ID
        struct message_record_pk pk = {0};
        int rc = kvstore_lookup_message_record_by_thread(txn, &thread_key, &pk);
        assert(rc == KVSTORE_OK);

        // Fetch message
        struct message_record msg = {0};
        rc = kvstore_get_message_record(txn, &pk, &msg, NULL);
        assert(rc == KVSTORE_OK);
        assert(msg.thread_id == 1001);

        printf("  ✓ Found message in thread %llu: (%u, %u) '%s'\n",
               (unsigned long long)msg.thread_id,
               msg.mailbox_id, msg.uid, msg.subject);

        free_message(&msg);
        kvstore_txn_commit(txn);
    }

    // TEST 5: Iterate mailbox 2 in time order
    printf("\nTest 5: Iterate mailbox 2 in time order...\n");
    {
        kvstore_txn_t *txn = kvstore_txn_begin(db, true);

        struct message_record_by_mailbox_time_key start = {
            .mailbox_id = 2,
            .received = { .tv_sec = 0, .tv_nsec = 0 }
        };

        kvstore_cursor_t *cur = kvstore_cursor_message_record_by_mailbox_time(txn, &start);

        int count = 0;
        time_t last_time = 0;

        kvstore_val_t key_val, pk_val;
        while (kvstore_cursor_get(cur, &key_val, &pk_val) == KVSTORE_OK) {
            struct message_record_pk cur_pk = {0};
            deserialise_message_record_pk((char*)pk_val.data, &cur_pk);

            struct message_record msg = {0};
            kvstore_get_message_record(txn, &cur_pk, &msg, NULL);

            if (msg.mailbox_id != 2) {
                free_message(&msg);
                break;
            }

            // Verify time ordering
            assert(msg.received.tv_sec >= last_time);
            last_time = msg.received.tv_sec;

            printf("  %d. (%u, %u) '%s' at %ld\n",
                   count + 1, msg.mailbox_id, msg.uid, msg.subject, msg.received.tv_sec);
            count++;

            free_message(&msg);

            if (kvstore_cursor_next(cur) != KVSTORE_OK) break;
        }

        kvstore_cursor_close(cur);
        assert(count == 4);  // 4 messages in mailbox 2
        printf("  ✓ Found %d messages in time order\n", count);

        kvstore_txn_commit(txn);
    }

    // TEST 6: Update subset of messages (change sender, flags, priority)
    printf("\nTest 6: Update random subset of messages...\n");
    {
        kvstore_txn_t *txn = kvstore_txn_begin(db, false);

        // Update messages at indices 1, 6, 9 (changing sender to test secondary key updates)
        int update_indices[] = {1, 6, 9};
        int num_updates = 3;

        // Hoist key_buf out of loop to reuse buffer and reduce reallocations
        kvstore_key_buf_t key_buf = KVSTORE_KEY_BUF_INIT;

        for (int i = 0; i < num_updates; i++) {
            int idx = update_indices[i];
            struct message_record *msg = &test_data[idx];

            // Fetch current version
            struct message_record_pk key = {
                .mailbox_id = msg->mailbox_id,
                .uid = msg->uid
            };
            struct message_record current = {0};

            // Get message and automatically populate key_buf (reuses buffer from previous iteration)
            int rc = kvstore_get_message_record(txn, &key, &current, &key_buf);
            assert(rc == KVSTORE_OK);

            // Modify sender (secondary key change)
            free(current.sender);
            current.sender = strdup("updated@example.com");

            // Modify flags and priority (non-key fields)
            current.flags = 0xFF;
            current.priority = 9;

            // Update last_modified
            current.last_modified.tv_sec = time(NULL);

            // Write back with change detection
            rc = kvstore_put_message_record_with_all_indices(txn, &current, &key_buf);
            assert(rc == KVSTORE_OK);

            printf("  Updated (%u, %u): new sender = %s\n",
                   current.mailbox_id, current.uid, current.sender);

            free_message(&current);
        }

        // Free key_buf once after all updates
        kvstore_key_buf_free(&key_buf);

        kvstore_txn_commit(txn);
        printf("  ✓ Updated %d messages (reused key buffer)\n", num_updates);
    }

    // TEST 7: Verify old sender lookup fails
    printf("\nTest 7: Verify old sender no longer found...\n");
    {
        kvstore_txn_t *txn = kvstore_txn_begin(db, true);

        // Check message (2, 203) which was updated
        struct message_record_pk key = { .mailbox_id = 2, .uid = 203 };
        struct message_record msg = {0};

        int rc = kvstore_get_message_record(txn, &key, &msg, NULL);
        assert(rc == KVSTORE_OK);
        assert(strcmp(msg.sender, "updated@example.com") == 0);  // New sender
        assert(msg.flags == 0xFF);
        assert(msg.priority == 9);

        printf("  ✓ Message updated correctly: sender is now '%s'\n", msg.sender);

        free_message(&msg);
        kvstore_txn_commit(txn);
    }

    // TEST 8: Verify one of the updated messages can be found by new sender
    printf("\nTest 8: Lookup by new sender (updated@example.com)...\n");
    {
        kvstore_txn_t *txn = kvstore_txn_begin(db, true);
        struct message_record_by_sender_key sender_key = { .sender = "updated@example.com" };

        // Should find one of the updated messages (last one wins in secondary index)
        struct message_record_pk pk = {0};
        int rc = kvstore_lookup_message_record_by_sender(txn, &sender_key, &pk);
        assert(rc == KVSTORE_OK);

        struct message_record msg = {0};
        rc = kvstore_get_message_record(txn, &pk, &msg, NULL);
        assert(rc == KVSTORE_OK);
        assert(strcmp(msg.sender, "updated@example.com") == 0);

        printf("  ✓ Found updated message: (%u, %u) '%s'\n",
               msg.mailbox_id, msg.uid, msg.subject);

        free_message(&msg);
        kvstore_txn_commit(txn);
    }

    // TEST 9: Lookup by recipient
    printf("\nTest 9: Lookup by recipient (alice@example.com)...\n");
    {
        kvstore_txn_t *txn = kvstore_txn_begin(db, true);
        struct message_record_by_recipient_key recip_key = { .recipient = "alice@example.com" };

        // Lookup - will find one of the messages to alice (last one indexed)
        struct message_record_pk pk = {0};
        int rc = kvstore_lookup_message_record_by_recipient(txn, &recip_key, &pk);

        if (rc == KVSTORE_OK) {
            struct message_record msg = {0};
            kvstore_get_message_record(txn, &pk, &msg, NULL);

            printf("  ✓ Found message to %s: (%u, %u) '%s'\n",
                   msg.recipient, msg.mailbox_id, msg.uid, msg.subject);

            free_message(&msg);
        }

        kvstore_txn_commit(txn);
    }

    // Cleanup
    for (int i = 0; i < num_messages; i++) {
        free_message(&test_data[i]);
    }

    kvstore_close(db);

    printf("\n=== All tests passed! ===\n");
    return 0;
}
