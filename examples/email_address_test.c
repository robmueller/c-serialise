// Test for email address parser

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../parsers/email_address.h"

static void test_single_address(void) {
    printf("Test 1: Single address with display name...\n");

    struct email_address addr = {0};
    int rc = parse_email_address("\"James Smythe\" <james@example.com>", &addr);

    assert(rc == 0);
    assert(addr.name != NULL);
    assert(strcmp(addr.name, "James Smythe") == 0);
    assert(addr.email != NULL);
    assert(strcmp(addr.email, "james@example.com") == 0);

    printf("  Name: %s\n", addr.name ? addr.name : "(null)");
    printf("  Email: %s\n", addr.email);
    printf("  ✓ Passed\n\n");

    free_email_address(&addr);
}

static void test_bare_address(void) {
    printf("Test 2: Bare email address...\n");

    struct email_address addr = {0};
    int rc = parse_email_address("jane@example.com", &addr);

    assert(rc == 0);
    assert(addr.name == NULL);
    assert(addr.email != NULL);
    assert(strcmp(addr.email, "jane@example.com") == 0);

    printf("  Name: %s\n", addr.name ? addr.name : "(null)");
    printf("  Email: %s\n", addr.email);
    printf("  ✓ Passed\n\n");

    free_email_address(&addr);
}

static void test_address_with_spaces(void) {
    printf("Test 3: Address with leading/trailing spaces...\n");

    struct email_address addr = {0};
    int rc = parse_email_address("  \"James Smythe\"   <james@example.com>  ", &addr);

    assert(rc == 0);
    assert(addr.name != NULL);
    assert(strcmp(addr.name, "James Smythe") == 0);
    assert(addr.email != NULL);
    assert(strcmp(addr.email, "james@example.com") == 0);

    printf("  Name: %s\n", addr.name ? addr.name : "(null)");
    printf("  Email: %s\n", addr.email);
    printf("  ✓ Passed\n\n");

    free_email_address(&addr);
}

static void test_unquoted_display_name(void) {
    printf("Test 4: Unquoted display name (single atom)...\n");

    struct email_address addr = {0};
    // Note: Multi-word unquoted display names need phrase parsing (sequence of atoms)
    // For now, testing single-atom display name
    int rc = parse_email_address("John <john@example.com>", &addr);

    assert(rc == 0);
    assert(addr.name != NULL);
    assert(strcmp(addr.name, "John") == 0);
    assert(addr.email != NULL);
    assert(strcmp(addr.email, "john@example.com") == 0);

    printf("  Name: %s\n", addr.name ? addr.name : "(null)");
    printf("  Email: %s\n", addr.email);
    printf("  ✓ Passed\n\n");

    free_email_address(&addr);
}

static void test_address_list(void) {
    printf("Test 5: Address list parsing...\n");

    const char *input = "\"James Smythe\" <james@example.com>, "
                       "jane@example.com, "
                       "John <john@example.com>";

    struct email_address *addrs = NULL;
    size_t count = 0;

    int rc = parse_email_address_list(input, &addrs, &count);

    assert(rc == 0);
    assert(count == 3);

    printf("  Parsed %zu addresses:\n", count);

    // First address
    assert(addrs[0].name != NULL);
    assert(strcmp(addrs[0].name, "James Smythe") == 0);
    assert(strcmp(addrs[0].email, "james@example.com") == 0);
    printf("    1. Name: %s, Email: %s\n",
           addrs[0].name ? addrs[0].name : "(null)", addrs[0].email);

    // Second address
    assert(addrs[1].name == NULL);
    assert(strcmp(addrs[1].email, "jane@example.com") == 0);
    printf("    2. Name: %s, Email: %s\n",
           addrs[1].name ? addrs[1].name : "(null)", addrs[1].email);

    // Third address
    assert(addrs[2].name != NULL);
    assert(strcmp(addrs[2].name, "John") == 0);
    assert(strcmp(addrs[2].email, "john@example.com") == 0);
    printf("    3. Name: %s, Email: %s\n",
           addrs[2].name ? addrs[2].name : "(null)", addrs[2].email);

    printf("  ✓ Passed\n\n");

    free_email_address_list(addrs, count);
}

static void test_group_syntax(void) {
    printf("Test 6: Group syntax...\n");

    const char *input = "Friends: jane@example.com, bob@example.com;";

    struct email_address *addrs = NULL;
    size_t count = 0;

    int rc = parse_email_address_list(input, &addrs, &count);

    assert(rc == 0);
    assert(count == 2);

    printf("  Parsed %zu addresses from group:\n", count);

    assert(addrs[0].name == NULL);
    assert(strcmp(addrs[0].email, "jane@example.com") == 0);
    printf("    1. Name: %s, Email: %s\n",
           addrs[0].name ? addrs[0].name : "(null)", addrs[0].email);

    assert(addrs[1].name == NULL);
    assert(strcmp(addrs[1].email, "bob@example.com") == 0);
    printf("    2. Name: %s, Email: %s\n",
           addrs[1].name ? addrs[1].name : "(null)", addrs[1].email);

    printf("  ✓ Passed\n\n");

    free_email_address_list(addrs, count);
}

static void test_mixed_list(void) {
    printf("Test 7: Mixed list with group (simplified spec example)...\n");

    // Simplified version without RFC2047 encoding and using quoted names
    const char *input = "\"James Smythe\" <james@example.com>, "
                       "Friends: jane@example.com, John <john@example.com>;";

    struct email_address *addrs = NULL;
    size_t count = 0;

    int rc = parse_email_address_list(input, &addrs, &count);

    assert(rc == 0);
    assert(count == 3);

    printf("  Parsed %zu addresses:\n", count);

    for (size_t i = 0; i < count; i++) {
        printf("    %zu. Name: %s, Email: %s\n",
               i + 1,
               addrs[i].name ? addrs[i].name : "(null)",
               addrs[i].email);
    }

    // Verify expected values
    assert(strcmp(addrs[0].name, "James Smythe") == 0);
    assert(strcmp(addrs[0].email, "james@example.com") == 0);

    assert(addrs[1].name == NULL);
    assert(strcmp(addrs[1].email, "jane@example.com") == 0);

    assert(strcmp(addrs[2].name, "John") == 0);
    assert(strcmp(addrs[2].email, "john@example.com") == 0);

    printf("  ✓ Passed\n\n");

    free_email_address_list(addrs, count);
}

static void test_quoted_pairs(void) {
    printf("Test 8: Quoted-pairs in display name...\n");

    struct email_address addr = {0};
    int rc = parse_email_address("\"John \\\"The Boss\\\" Doe\" <john@example.com>", &addr);

    assert(rc == 0);
    assert(addr.name != NULL);
    assert(strcmp(addr.name, "John \"The Boss\" Doe") == 0);
    assert(addr.email != NULL);
    assert(strcmp(addr.email, "john@example.com") == 0);

    printf("  Name: %s\n", addr.name);
    printf("  Email: %s\n", addr.email);
    printf("  ✓ Passed\n\n");

    free_email_address(&addr);
}

int main(void) {
    printf("=== Email Address Parser Tests ===\n\n");

    test_single_address();
    test_bare_address();
    test_address_with_spaces();
    test_unquoted_display_name();
    test_address_list();
    test_group_syntax();
    test_mixed_list();
    test_quoted_pairs();

    printf("=== All tests passed! ===\n");
    return 0;
}
