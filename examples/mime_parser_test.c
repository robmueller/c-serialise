// Test for MIME parser

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../parsers/mime_parser.h"

static void test_content_type_simple(void) {
    printf("Test 1: Simple Content-Type parsing...\n");

    struct content_type ct = {0};
    int rc = parse_content_type("text/plain", &ct);

    assert(rc == 0);
    assert(ct.type != NULL);
    assert(strcmp(ct.type, "text") == 0);
    assert(ct.subtype != NULL);
    assert(strcmp(ct.subtype, "plain") == 0);
    assert(ct.num_params == 0);

    printf("  Type: %s/%s\n", ct.type, ct.subtype);
    printf("  ✓ Passed\n\n");

    free_content_type(&ct);
}

static void test_content_type_with_params(void) {
    printf("Test 2: Content-Type with parameters...\n");

    struct content_type ct = {0};
    int rc = parse_content_type("text/plain; charset=utf-8; format=flowed", &ct);

    assert(rc == 0);
    assert(strcmp(ct.type, "text") == 0);
    assert(strcmp(ct.subtype, "plain") == 0);
    assert(ct.num_params == 2);

    printf("  Type: %s/%s\n", ct.type, ct.subtype);
    printf("  Parameters:\n");
    for (size_t i = 0; i < ct.num_params; i++) {
        printf("    %s = %s\n", ct.params[i].name, ct.params[i].value);
        if (i == 0) {
            assert(strcmp(ct.params[i].name, "charset") == 0);
            assert(strcmp(ct.params[i].value, "utf-8") == 0);
        } else if (i == 1) {
            assert(strcmp(ct.params[i].name, "format") == 0);
            assert(strcmp(ct.params[i].value, "flowed") == 0);
        }
    }

    printf("  ✓ Passed\n\n");

    free_content_type(&ct);
}

static void test_content_type_multipart(void) {
    printf("Test 3: Multipart Content-Type with boundary...\n");

    struct content_type ct = {0};
    int rc = parse_content_type("multipart/mixed; boundary=\"----=_Part_12345\"", &ct);

    assert(rc == 0);
    assert(strcmp(ct.type, "multipart") == 0);
    assert(strcmp(ct.subtype, "mixed") == 0);
    assert(ct.num_params == 1);
    assert(strcmp(ct.params[0].name, "boundary") == 0);
    assert(strcmp(ct.params[0].value, "----=_Part_12345") == 0);

    printf("  Type: %s/%s\n", ct.type, ct.subtype);
    printf("  Boundary: %s\n", ct.params[0].value);
    printf("  ✓ Passed\n\n");

    free_content_type(&ct);
}

static void test_message_headers(void) {
    printf("Test 4: Message header parsing...\n");

    const char *headers =
        "From: Alice <alice@example.com>\n"
        "To: Bob <bob@example.com>, Charlie <charlie@example.com>\n"
        "Subject: Test message\n"
        "Message-ID: <12345@example.com>\n"
        "Date: Mon, 1 Jan 2024 12:00:00 +0000\n";

    struct message msg = {0};
    int rc = parse_message_headers(headers, &msg);

    assert(rc == 0);

    printf("  From: ");
    if (msg.from_count > 0) {
        printf("%s <%s>\n", msg.from[0].name ? msg.from[0].name : "(null)", msg.from[0].email);
        assert(strcmp(msg.from[0].email, "alice@example.com") == 0);
    }

    printf("  To: ");
    for (size_t i = 0; i < msg.to_count; i++) {
        if (i > 0) printf(", ");
        printf("%s <%s>", msg.to[i].name ? msg.to[i].name : "(null)", msg.to[i].email);
    }
    printf("\n");
    assert(msg.to_count == 2);
    assert(strcmp(msg.to[0].email, "bob@example.com") == 0);
    assert(strcmp(msg.to[1].email, "charlie@example.com") == 0);

    printf("  Subject: %s\n", msg.subject ? msg.subject : "(null)");
    assert(msg.subject != NULL);
    assert(strcmp(msg.subject, "Test message") == 0);

    printf("  Message-ID: %s\n", msg.message_id_count > 0 ? msg.message_id[0] : "(null)");
    assert(msg.message_id_count == 1);
    assert(strcmp(msg.message_id[0], "<12345@example.com>") == 0);

    printf("  Date: %s\n", msg.date ? msg.date : "(null)");
    assert(msg.date != NULL);

    printf("  ✓ Passed\n\n");

    free_message(&msg);
}

static void test_message_ids(void) {
    printf("Test 5: Message-ID array parsing...\n");

    const char *headers =
        "Message-ID: <msg1@example.com>\n"
        "In-Reply-To: <msg0@example.com>\n"
        "References: <msg-a@example.com> <msg-b@example.com> <msg-c@example.com>\n";

    struct message msg = {0};
    int rc = parse_message_headers(headers, &msg);

    assert(rc == 0);

    printf("  Message-ID: ");
    for (size_t i = 0; i < msg.message_id_count; i++) {
        printf("%s ", msg.message_id[i]);
    }
    printf("\n");
    assert(msg.message_id_count == 1);

    printf("  In-Reply-To: ");
    for (size_t i = 0; i < msg.in_reply_to_count; i++) {
        printf("%s ", msg.in_reply_to[i]);
    }
    printf("\n");
    assert(msg.in_reply_to_count == 1);

    printf("  References: ");
    for (size_t i = 0; i < msg.references_count; i++) {
        printf("%s ", msg.references[i]);
    }
    printf("\n");
    assert(msg.references_count == 3);

    printf("  ✓ Passed\n\n");

    free_message(&msg);
}

static void test_mime_headers(void) {
    printf("Test 6: MIME header parsing...\n");

    const char *headers =
        "Content-Type: text/html; charset=utf-8\n"
        "Content-Transfer-Encoding: quoted-printable\n"
        "Content-Disposition: inline\n"
        "Content-ID: <part1@example.com>\n"
        "X-Custom-Header: custom value\n";

    struct mime_part part = {0};
    int rc = parse_mime_headers(headers, &part);

    assert(rc == 0);

    printf("  Content-Type: %s/%s", part.content_type.type, part.content_type.subtype);
    if (part.content_type.num_params > 0) {
        printf("; %s=%s", part.content_type.params[0].name, part.content_type.params[0].value);
    }
    printf("\n");

    assert(strcmp(part.content_type.type, "text") == 0);
    assert(strcmp(part.content_type.subtype, "html") == 0);
    assert(part.content_type.num_params == 1);
    assert(strcmp(part.content_type.params[0].name, "charset") == 0);
    assert(strcmp(part.content_type.params[0].value, "utf-8") == 0);

    printf("  Content-Transfer-Encoding: %s\n",
           part.content_transfer_encoding ? part.content_transfer_encoding : "(null)");
    assert(part.content_transfer_encoding != NULL);
    assert(strcmp(part.content_transfer_encoding, "quoted-printable") == 0);

    printf("  Content-Disposition: %s\n",
           part.content_disposition ? part.content_disposition : "(null)");
    assert(part.content_disposition != NULL);
    assert(strcmp(part.content_disposition, "inline") == 0);

    printf("  Content-ID: %s\n", part.content_id ? part.content_id : "(null)");
    assert(part.content_id != NULL);
    assert(strcmp(part.content_id, "<part1@example.com>") == 0);

    printf("  Additional headers: %zu\n", part.num_headers);
    assert(part.num_headers == 1);
    assert(strcmp(part.headers[0].name, "X-Custom-Header") == 0);
    assert(strcmp(part.headers[0].value, "custom value") == 0);
    printf("    %s: %s\n", part.headers[0].name, part.headers[0].value);

    printf("  ✓ Passed\n\n");

    free_mime_part(&part);
}

static void test_default_content_type(void) {
    printf("Test 7: Default Content-Type (text/plain)...\n");

    const char *headers = "Subject: Test\n";

    struct mime_part part = {0};
    int rc = parse_mime_headers(headers, &part);

    assert(rc == 0);
    assert(strcmp(part.content_type.type, "text") == 0);
    assert(strcmp(part.content_type.subtype, "plain") == 0);

    printf("  Default Content-Type: %s/%s\n", part.content_type.type, part.content_type.subtype);
    printf("  ✓ Passed\n\n");

    free_mime_part(&part);
}

int main(void) {
    printf("=== MIME Parser Tests ===\n\n");

    test_content_type_simple();
    test_content_type_with_params();
    test_content_type_multipart();
    test_message_headers();
    test_message_ids();
    test_mime_headers();
    test_default_content_type();

    printf("=== All tests passed! ===\n");
    return 0;
}
