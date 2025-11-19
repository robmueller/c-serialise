// MIME parser for RFC2045 message structure
// Parses MIME parts with content-type, headers, and nested message structures

#ifndef MIME_PARSER_H_
#define MIME_PARSER_H_

#include <stddef.h>
#include <stdint.h>
#include "email_address.h"

#ifdef __cplusplus
extern "C" {
#endif

// ------------------------
// MIME Structures
// ------------------------

/**
 * struct mime_param - Content-Type or Content-Disposition parameter
 * @name: Parameter name (e.g., "charset", "boundary")
 * @value: Parameter value (e.g., "utf-8", "----boundary----")
 */
struct mime_param {
    char *name;
    char *value;
};

/**
 * struct content_type - Parsed Content-Type header
 * @type: Main type (e.g., "text", "multipart", "message")
 * @subtype: Subtype (e.g., "plain", "html", "mixed", "rfc822")
 * @params: Array of parameters (e.g., charset, boundary)
 * @num_params: Number of parameters
 */
struct content_type {
    char *type;
    char *subtype;
    struct mime_param *params;
    size_t num_params;
};

/**
 * struct header - Generic header name/value pair
 * @name: Header name (e.g., "X-Mailer", "Date")
 * @value: Header value (unfolded, but not decoded)
 */
struct header {
    char *name;
    char *value;
};

/**
 * struct message - Standard email message headers (for message/rfc822 MIME types)
 * @message_id: Array of Message-ID strings (typically 1 element)
 * @message_id_count: Number of Message-IDs
 * @in_reply_to: Array of In-Reply-To message IDs
 * @in_reply_to_count: Number of In-Reply-To IDs
 * @references: Array of References message IDs
 * @references_count: Number of References
 * @sender: Sender addresses (typically 1)
 * @sender_count: Number of senders
 * @from: From addresses
 * @from_count: Number of from addresses
 * @to: To addresses
 * @to_count: Number of to addresses
 * @cc: CC addresses
 * @cc_count: Number of CC addresses
 * @bcc: BCC addresses
 * @bcc_count: Number of BCC addresses
 * @reply_to: Reply-To addresses
 * @reply_to_count: Number of reply-to addresses
 * @subject: Subject line (can be NULL)
 * @date: Date header (can be NULL)
 */
struct message {
    char **message_id;
    size_t message_id_count;

    char **in_reply_to;
    size_t in_reply_to_count;

    char **references;
    size_t references_count;

    struct email_address *sender;
    size_t sender_count;

    struct email_address *from;
    size_t from_count;

    struct email_address *to;
    size_t to_count;

    struct email_address *cc;
    size_t cc_count;

    struct email_address *bcc;
    size_t bcc_count;

    struct email_address *reply_to;
    size_t reply_to_count;

    char *subject;
    char *date;
};

/**
 * struct mime_part - A MIME part with headers and content
 * @guid: SHA256 hash of the original email content (hex string, 65 bytes)
 * @content_type: Parsed Content-Type header
 * @content_transfer_encoding: Transfer encoding (e.g., "base64", "quoted-printable")
 * @content_disposition: Content-Disposition value (e.g., "inline", "attachment")
 * @content_disposition_params: Parameters for Content-Disposition
 * @num_disposition_params: Number of disposition parameters
 * @content_id: Content-ID header value
 * @headers: Additional headers not in standard MIME/message headers
 * @num_headers: Number of additional headers
 * @body: Raw body content (not decoded)
 * @body_length: Length of body in bytes
 * @message: Message structure (if Content-Type is message/rfc822)
 * @parts: Array of sub-parts (if Content-Type is multipart/mixed, etc.)
 * @num_parts: Number of sub-parts
 */
struct mime_part {
    char guid[65];  // SHA256 hash as hex string (64 chars + null terminator)

    struct content_type content_type;

    char *content_transfer_encoding;

    char *content_disposition;
    struct mime_param *content_disposition_params;
    size_t num_disposition_params;

    char *content_id;

    // Additional headers
    struct header *headers;
    size_t num_headers;

    // Body content
    char *body;
    size_t body_length;

    // For message/* types
    struct message *message;

    // For multipart/* types
    struct mime_part **parts;
    size_t num_parts;
};

// ------------------------
// Parser Functions
// ------------------------

/**
 * parse_content_type - Parse a Content-Type header value
 * @value: Content-Type header value (e.g., "text/plain; charset=utf-8")
 * @ct: Output content_type structure
 *
 * Returns: 0 on success, -1 on error
 */
int parse_content_type(const char *value, struct content_type *ct);

/**
 * parse_mime_headers - Parse MIME headers into a mime_part structure
 * @headers: Raw header text (headers only, before body)
 * @part: Output mime_part structure (initialized)
 *
 * Parses standard MIME headers (Content-Type, Content-Transfer-Encoding, etc.)
 * and stores additional headers in the headers array.
 *
 * Returns: 0 on success, -1 on error
 */
int parse_mime_headers(const char *headers, struct mime_part *part);

/**
 * parse_message_headers - Parse email message headers
 * @headers: Raw header text
 * @msg: Output message structure
 *
 * Parses standard email headers (From, To, Subject, etc.)
 *
 * Returns: 0 on success, -1 on error
 */
int parse_message_headers(const char *headers, struct message *msg);

/**
 * parse_mime_part - Parse a complete MIME part (headers + body)
 * @input: Full MIME part text (headers + blank line + body)
 * @part: Output mime_part structure
 *
 * Parses headers, extracts body, and handles multipart boundaries.
 * For message/rfc822 types, recursively parses the embedded message.
 *
 * Returns: 0 on success, -1 on error
 */
int parse_mime_part(const char *input, struct mime_part *part);

/**
 * parse_multipart - Parse multipart MIME body into sub-parts
 * @body: Multipart body content
 * @boundary: Boundary string from Content-Type
 * @parts: Output array of mime_part pointers
 * @num_parts: Output number of parts
 *
 * Returns: 0 on success, -1 on error
 */
int parse_multipart(const char *body, const char *boundary,
                    struct mime_part ***parts, size_t *num_parts);

// ------------------------
// Memory Management
// ------------------------

/**
 * free_mime_param - Free a mime_param structure
 */
void free_mime_param(struct mime_param *param);

/**
 * free_content_type - Free a content_type structure
 */
void free_content_type(struct content_type *ct);

/**
 * free_header - Free a header structure
 */
void free_header(struct header *hdr);

/**
 * free_message - Free a message structure
 */
void free_message(struct message *msg);

/**
 * free_mime_part - Free a mime_part structure (recursive)
 */
void free_mime_part(struct mime_part *part);

#ifdef __cplusplus
}
#endif

#endif // MIME_PARSER_H_
