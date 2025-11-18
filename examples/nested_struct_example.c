// Example demonstrating SERIALISE_FIELD_PTR for arrays of nested structs

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "../include/serialise.h"

// ------------------------
// User record (nested struct)
// ------------------------

struct user_record {
    uint64_t user_id;
    char *username;
    uint32_t age;
    struct timespec created;
};

SERIALISE(user_record,
    SERIALISE_FIELD(user_id, uint64_t),
    SERIALISE_FIELD(username, charptr),
    SERIALISE_FIELD(age, uint32_t),
    SERIALISE_FIELD(created, timespec)
)

// ------------------------
// Customer record (contains array of users)
// ------------------------

struct customer_record {
    uint64_t customer_id;
    char *customer_name;
    uint32_t num_users;
    struct user_record *users;
};

SERIALISE(customer_record,
    SERIALISE_FIELD(customer_id, uint64_t),
    SERIALISE_FIELD(customer_name, charptr),
    SERIALISE_FIELD(num_users, uint32_t),
    SERIALISE_FIELD_PTR(users, user_record, num_users)
)

// ------------------------
// Helper functions
// ------------------------

static void free_user(struct user_record *user) {
    free(user->username);
}

static void free_customer(struct customer_record *customer) {
    free(customer->customer_name);
    if (customer->users) {
        for (uint32_t i = 0; i < customer->num_users; i++) {
            free_user(&customer->users[i]);
        }
        free(customer->users);
    }
}

static struct user_record create_user(uint64_t user_id, const char *username, uint32_t age, time_t created_sec) {
    struct user_record user = {0};
    user.user_id = user_id;
    user.username = strdup(username);
    user.age = age;
    user.created.tv_sec = created_sec;
    user.created.tv_nsec = 0;
    return user;
}

// ------------------------
// Main test
// ------------------------

int main(void) {
    printf("=== Nested Struct Serialization Test ===\n\n");

    // Test 1: Create customer with 3 users
    printf("Test 1: Creating customer with 3 users...\n");
    struct customer_record customer = {0};
    customer.customer_id = 1001;
    customer.customer_name = strdup("Acme Corp");
    customer.num_users = 3;
    customer.users = (struct user_record *)malloc(sizeof(struct user_record) * customer.num_users);

    customer.users[0] = create_user(1, "alice", 30, 1700000000);
    customer.users[1] = create_user(2, "bob", 25, 1700000100);
    customer.users[2] = create_user(3, "charlie", 35, 1700000200);

    printf("  Customer ID: %llu\n", (unsigned long long)customer.customer_id);
    printf("  Customer Name: %s\n", customer.customer_name);
    printf("  Number of users: %u\n", customer.num_users);
    for (uint32_t i = 0; i < customer.num_users; i++) {
        printf("    User %u: %s (age %u, id %llu)\n",
               i + 1, customer.users[i].username, customer.users[i].age,
               (unsigned long long)customer.users[i].user_id);
    }

    // Test 2: Serialize customer
    printf("\nTest 2: Serializing customer...\n");
    size_t serialized_size = serialise_customer_record_size(&customer);
    printf("  Serialized size: %zu bytes\n", serialized_size);

    char *buffer = (char *)malloc(serialized_size);
    char *end = serialise_customer_record(buffer, &customer);
    assert(end == buffer + serialized_size);
    printf("  ✓ Serialization complete\n");

    // Test 3: Deserialize customer
    printf("\nTest 3: Deserializing customer...\n");
    struct customer_record customer2 = {0};
    deserialise_customer_record(buffer, &customer2);

    printf("  Customer ID: %llu\n", (unsigned long long)customer2.customer_id);
    printf("  Customer Name: %s\n", customer2.customer_name);
    printf("  Number of users: %u\n", customer2.num_users);

    // Verify correctness
    assert(customer2.customer_id == customer.customer_id);
    assert(strcmp(customer2.customer_name, customer.customer_name) == 0);
    assert(customer2.num_users == customer.num_users);

    for (uint32_t i = 0; i < customer2.num_users; i++) {
        printf("    User %u: %s (age %u, id %llu)\n",
               i + 1, customer2.users[i].username, customer2.users[i].age,
               (unsigned long long)customer2.users[i].user_id);

        assert(customer2.users[i].user_id == customer.users[i].user_id);
        assert(strcmp(customer2.users[i].username, customer.users[i].username) == 0);
        assert(customer2.users[i].age == customer.users[i].age);
        assert(customer2.users[i].created.tv_sec == customer.users[i].created.tv_sec);
    }
    printf("  ✓ Deserialization verified\n");

    // Test 4: Empty array (0 users)
    printf("\nTest 4: Testing empty user array...\n");
    struct customer_record customer3 = {0};
    customer3.customer_id = 1002;
    customer3.customer_name = strdup("Empty Inc");
    customer3.num_users = 0;
    customer3.users = NULL;

    size_t size3 = serialise_customer_record_size(&customer3);
    char *buf3 = (char *)malloc(size3);
    serialise_customer_record(buf3, &customer3);

    struct customer_record customer3_restored = {0};
    deserialise_customer_record(buf3, &customer3_restored);

    assert(customer3_restored.customer_id == customer3.customer_id);
    assert(strcmp(customer3_restored.customer_name, customer3.customer_name) == 0);
    assert(customer3_restored.num_users == 0);
    assert(customer3_restored.users == NULL);
    printf("  ✓ Empty array handled correctly\n");

    // Test 5: Single user
    printf("\nTest 5: Testing single user...\n");
    struct customer_record customer4 = {0};
    customer4.customer_id = 1003;
    customer4.customer_name = strdup("Solo Ltd");
    customer4.num_users = 1;
    customer4.users = (struct user_record *)malloc(sizeof(struct user_record));
    customer4.users[0] = create_user(100, "david", 40, 1700000300);

    size_t size4 = serialise_customer_record_size(&customer4);
    char *buf4 = (char *)malloc(size4);
    serialise_customer_record(buf4, &customer4);

    struct customer_record customer4_restored = {0};
    deserialise_customer_record(buf4, &customer4_restored);

    assert(customer4_restored.customer_id == customer4.customer_id);
    assert(strcmp(customer4_restored.customer_name, customer4.customer_name) == 0);
    assert(customer4_restored.num_users == 1);
    assert(customer4_restored.users[0].user_id == 100);
    assert(strcmp(customer4_restored.users[0].username, "david") == 0);
    printf("  ✓ Single user handled correctly\n");

    // Cleanup
    free(buffer);
    free_customer(&customer);
    free_customer(&customer2);
    free(buf3);
    free_customer(&customer3);
    free_customer(&customer3_restored);
    free(buf4);
    free_customer(&customer4);
    free_customer(&customer4_restored);

    printf("\n=== All tests passed! ===\n");
    return 0;
}
