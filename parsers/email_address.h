// Email address parser based on RFC5322 and JMAP EmailAddress specification
// Handles mailbox parsing with display-name and addr-spec extraction

#ifndef EMAIL_ADDRESS_H_
#define EMAIL_ADDRESS_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ------------------------
// Email Address Structure
// ------------------------

/**
 * struct email_address - Represents a parsed email address
 * @name: Display name (can be NULL if no display-name present)
 *        Decoded from quoted-string or comment, with surrounding quotes removed,
 *        quoted-pairs decoded, whitespace unfolded, and leading/trailing space trimmed
 * @email: The addr-spec portion (e.g., "user@example.com")
 *         Should conform to addr-spec form but MAY be invalid to accommodate
 *         malformed messages and semi-complete drafts
 *
 * Based on JMAP EmailAddress specification:
 * - name: The "display-name" of the "mailbox" from RFC5322
 * - email: The "addr-spec" of the "mailbox" from RFC5322
 *
 * Example parsing:
 *   "James Smythe" <james@example.com>
 *   -> { name: "James Smythe", email: "james@example.com" }
 *
 *   jane@example.com
 *   -> { name: NULL, email: "jane@example.com" }
 *
 *   =?UTF-8?Q?John_Sm=C3=AEth?= <john@example.com>
 *   -> { name: "John Smîth", email: "john@example.com" }
 */
struct email_address {
    char *name;   // Display name (NULL if not present)
    char *email;  // Email address (addr-spec)
};

// ------------------------
// Parser Functions
// ------------------------

/**
 * parse_email_address - Parse a single mailbox into email_address struct
 * @input: String containing a single mailbox (e.g., "Name" <addr@example.com>)
 * @addr: Output structure to populate (caller must free name and email)
 *
 * Parses a single RFC5322 mailbox, handling:
 * - Quoted display names with escaped characters
 * - Comments in display name position
 * - RFC2047 encoded words in display name
 * - Angle-bracket delimited addr-spec
 * - Bare addr-spec without display name
 *
 * Best-effort parsing is used to handle malformed input.
 *
 * Returns: 0 on success, -1 on error
 */
int parse_email_address(const char *input, struct email_address *addr);

/**
 * parse_email_address_list - Parse an address-list into array of email_address
 * @input: String containing comma-separated mailboxes or groups
 * @addresses: Output array pointer (caller must free array and each element)
 * @count: Output number of addresses parsed
 *
 * Parses an RFC5322 address-list containing:
 * - Multiple comma-separated mailboxes
 * - Groups (group-name: mailbox-list;)
 *
 * Groups are expanded - the group name is discarded and individual addresses
 * within the group are extracted.
 *
 * Example:
 *   "Alice" <alice@example.com>, Friends: bob@example.com, charlie@example.com;
 *   -> 3 addresses: alice@example.com, bob@example.com, charlie@example.com
 *
 * Returns: 0 on success, -1 on error
 */
int parse_email_address_list(const char *input, struct email_address **addresses, size_t *count);

/**
 * free_email_address - Free memory allocated in email_address struct
 * @addr: Address structure to free (does not free the struct itself)
 */
void free_email_address(struct email_address *addr);

/**
 * free_email_address_list - Free array of email addresses
 * @addresses: Array of addresses to free
 * @count: Number of addresses in array
 */
void free_email_address_list(struct email_address *addresses, size_t count);

#ifdef __cplusplus
}
#endif

#endif // EMAIL_ADDRESS_H_
