#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "kvstore.h"
#include "kvstore_backend.h"
#include "parsers/mime_parser.h"
#include "parsers/mime_serialise.h"

// Forward declaration for in-memory KV store
extern kvstore_t* kvstore_open_mem(void);

// Example email for testing
static const char *sample_email =
    "From: \"Alice Smith\" <alice@example.com>\n"
    "To: \"Bob Jones\" <bob@example.com>\n"
    "Subject: Test Email\n"
    "Date: Mon, 1 Jan 2024 12:00:00 +0000\n"
    "Message-ID: <test123@example.com>\n"
    "Content-Type: multipart/mixed; boundary=\"boundary123\"\n"
    "\n"
    "--boundary123\n"
    "Content-Type: text/plain\n"
    "\n"
    "This is the plain text body.\n"
    "\n"
    "--boundary123\n"
    "Content-Type: text/html\n"
    "\n"
    "<html><body>This is HTML</body></html>\n"
    "\n"
    "--boundary123--\n";

// Define a simple email record that stores the mime_part
struct email_record {
    char *guid;         // SHA256 hash (primary key)
    char *date;         // Date for secondary key
    struct mime_part *part;  // The parsed email
};

// Serializer for email_record
// Note: We only serialize guid and date, the mime_part is stored separately
SERIALISE(email_record,
    SERIALISE_FIELD(guid, charptr),
    SERIALISE_FIELD(date, charptr)
)

// Declare keys
SERIALISE_DECLARE_KEYS(email_record)

// Primary key: guid
SERIALISE_PRIMARY_KEY(email_record, "email:",
    SERIALISE_FIELD(guid, charptr)
)

// Secondary key: date + guid for sorting by date
SERIALISE_SECONDARY_KEY(email_record, "email_date:", by_date,
    SERIALISE_FIELD(date, charptr),
    SERIALISE_FIELD(guid, charptr)
)

// Finalize indices
SERIALISE_FINALIZE_INDICES(email_record,
    by_date, "email_date:"
)

int main(void) {
    printf("=== MIME KV Store Example ===\n\n");

    // Open in-memory KV store
    kvstore_t *kv = kvstore_open_mem();
    if (!kv) {
        fprintf(stderr, "Failed to open KV store\n");
        return 1;
    }

    // Parse the email
    printf("1. Parsing email...\n");
    struct mime_part part = {0};
    int rc = parse_mime_part(sample_email, &part);
    if (rc != 0) {
        fprintf(stderr, "Failed to parse email\n");
        kvstore_close(kv);
        return 1;
    }

    printf("   GUID: %s\n", part.guid);
    printf("   Content-Type: %s/%s\n", part.content_type.type, part.content_type.subtype);
    if (part.message) {
        printf("   From: %s\n", part.message->from_count > 0 ? part.message->from[0].email : "N/A");
        printf("   Date: %s\n", part.message->date ? part.message->date : "N/A");
    }
    printf("   Parts: %zu\n\n", part.num_parts);

    // Create email record
    struct email_record email = {0};
    email.guid = strdup(part.guid);
    email.date = part.message && part.message->date ? strdup(part.message->date) : strdup("unknown");
    email.part = &part;

    // Serialize the mime_part
    printf("2. Serializing email...\n");
    size_t serialized_size = serialise_mime_part_size(&part);
    printf("   Serialized size: %zu bytes\n", serialized_size);

    char *buffer = malloc(serialized_size);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate buffer\n");
        free(email.date);
        free_mime_part(&part);
        kvstore_close(kv);
        return 1;
    }

    serialise_mime_part(buffer, &part);
    printf("   Serialization complete\n\n");

    // Start transaction and store
    printf("3. Storing in KV store...\n");
    kvstore_txn_t *txn = kvstore_txn_begin(kv, false);  // false = write transaction
    if (!txn) {
        fprintf(stderr, "Failed to begin transaction\n");
        free(buffer);
        free(email.date);
        free_mime_part(&part);
        kvstore_close(kv);
        return 1;
    }

    // Store the serialized mime_part using guid as key
    // We'll manually store it since we want to store the mime_part directly
    kvstore_val_t key = { part.guid, strlen(part.guid) };
    kvstore_val_t val = { buffer, serialized_size };
    rc = kvstore_txn_put(txn, "emails", &key, &val);
    if (rc != KVSTORE_OK) {
        fprintf(stderr, "Failed to store email\n");
        kvstore_txn_abort(txn);
        free(buffer);
        free(email.date);
        free_mime_part(&part);
        kvstore_close(kv);
        return 1;
    }
    printf("   Stored with key: %s\n", part.guid);

    // Create secondary index on date
    if (part.message && part.message->date) {
        char sk_key[256];
        snprintf(sk_key, sizeof(sk_key), "date:%s:%s", part.message->date, part.guid);
        kvstore_val_t sk_k = { sk_key, strlen(sk_key) };
        kvstore_val_t sk_v = { part.guid, strlen(part.guid) };
        rc = kvstore_txn_put(txn, "emails_by_date", &sk_k, &sk_v);
        if (rc != KVSTORE_OK) {
            fprintf(stderr, "Failed to create secondary key\n");
            kvstore_txn_abort(txn);
            free(buffer);
            free(email.date);
            free_mime_part(&part);
            kvstore_close(kv);
            return 1;
        }
        printf("   Secondary key: %s\n\n", sk_key);
    }

    // Commit transaction
    rc = kvstore_txn_commit(txn);
    if (rc != KVSTORE_OK) {
        fprintf(stderr, "Failed to commit transaction\n");
        free(buffer);
        free(email.date);
        free_mime_part(&part);
        kvstore_close(kv);
        return 1;
    }

    // Retrieve from KV store
    printf("4. Retrieving from KV store...\n");
    txn = kvstore_txn_begin(kv, true);  // true = read-only transaction
    if (!txn) {
        fprintf(stderr, "Failed to begin read transaction\n");
        free(buffer);
        free(email.date);
        free_mime_part(&part);
        kvstore_close(kv);
        return 1;
    }

    kvstore_val_t retrieved_val = {0};
    rc = kvstore_txn_get(txn, "emails", &key, &retrieved_val);
    if (rc != KVSTORE_OK) {
        fprintf(stderr, "Failed to retrieve from KV store\n");
        kvstore_txn_abort(txn);
        free(buffer);
        free(email.date);
        free_mime_part(&part);
        kvstore_close(kv);
        return 1;
    }
    printf("   Retrieved %zu bytes\n", retrieved_val.size);

    // Verify the size matches
    assert(retrieved_val.size == serialized_size);
    assert(memcmp(buffer, retrieved_val.data, serialized_size) == 0);
    printf("   Data matches original\n\n");

    // Deserialize the retrieved data
    printf("5. Deserializing retrieved email...\n");
    struct mime_part retrieved_part = {0};
    deserialise_mime_part((char*)retrieved_val.data, &retrieved_part);

    printf("   GUID: %s\n", retrieved_part.guid);
    printf("   Content-Type: %s/%s\n",
           retrieved_part.content_type.type,
           retrieved_part.content_type.subtype);
    printf("   Parts: %zu\n", retrieved_part.num_parts);

    // Verify the data
    assert(strcmp(part.guid, retrieved_part.guid) == 0);
    assert(strcmp(part.content_type.type, retrieved_part.content_type.type) == 0);
    assert(strcmp(part.content_type.subtype, retrieved_part.content_type.subtype) == 0);
    assert(part.num_parts == retrieved_part.num_parts);

    if (part.message && retrieved_part.message) {
        printf("   From: %s\n",
               retrieved_part.message->from_count > 0 ?
               retrieved_part.message->from[0].email : "N/A");
        printf("   Date: %s\n",
               retrieved_part.message->date ? retrieved_part.message->date : "N/A");

        assert(strcmp(part.message->from[0].email,
                     retrieved_part.message->from[0].email) == 0);
        if (part.message->date && retrieved_part.message->date) {
            assert(strcmp(part.message->date, retrieved_part.message->date) == 0);
        }
    }

    printf("\n=== All tests passed! ===\n");

    // Cleanup
    kvstore_txn_commit(txn);
    free(buffer);
    free(email.guid);
    free(email.date);
    free_mime_part(&part);
    free_mime_part(&retrieved_part);
    kvstore_close(kv);

    return 0;
}
