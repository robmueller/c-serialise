// MIME parser implementation

#define _POSIX_C_SOURCE 200809L
#include "mime_parser.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>

// ------------------------
// Helper Functions
// ------------------------

static char *trim_whitespace(char *str) {
    if (!str) return NULL;

    // Trim leading
    while (*str && isspace(*str)) str++;

    if (!*str) return str;

    // Trim trailing
    char *end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) *end-- = '\0';

    return str;
}

static char *unfold_header(const char *value) {
    if (!value) return NULL;

    char *result = strdup(value);
    if (!result) return NULL;

    // Unfold: replace CRLF followed by whitespace with single space
    char *dst = result;
    const char *src = value;

    while (*src) {
        if (*src == '\r' && *(src + 1) == '\n' && isspace(*(src + 2))) {
            *dst++ = ' ';
            src += 2;
            while (*src && isspace(*src)) src++;
        } else if (*src == '\n' && isspace(*(src + 1))) {
            *dst++ = ' ';
            src++;
            while (*src && isspace(*src)) src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    return result;
}

// ------------------------
// Content-Type Parsing
// ------------------------

int parse_content_type(const char *value, struct content_type *ct) {
    if (!value || !ct) return -1;

    memset(ct, 0, sizeof(*ct));

    char *input = unfold_header(value);
    if (!input) return -1;

    char *s = trim_whitespace(input);

    // Parse type/subtype
    char *slash = strchr(s, '/');
    if (!slash) {
        // Default to text/plain if malformed
        ct->type = strdup("text");
        ct->subtype = strdup("plain");
        free(input);
        return 0;
    }

    *slash = '\0';
    ct->type = strdup(trim_whitespace(s));
    s = slash + 1;

    // Find end of subtype (semicolon or end)
    char *semi = strchr(s, ';');
    if (semi) {
        *semi = '\0';
        ct->subtype = strdup(trim_whitespace(s));
        s = semi + 1;

        // Parse parameters
        ct->params = NULL;
        ct->num_params = 0;
        size_t capacity = 4;
        ct->params = malloc(sizeof(struct mime_param) * capacity);
        if (!ct->params) {
            free(input);
            return -1;
        }

        while (*s) {
            s = trim_whitespace(s);
            if (!*s) break;

            // Find '='
            char *eq = strchr(s, '=');
            if (!eq) break;

            *eq = '\0';
            char *name = trim_whitespace(s);
            char *val = eq + 1;
            val = trim_whitespace(val);

            // Remove quotes from value
            if (*val == '"') {
                val++;
                char *end_quote = strchr(val, '"');
                if (end_quote) *end_quote = '\0';
            }

            // Find next semicolon
            char *next_semi = strchr(val, ';');
            if (next_semi) {
                *next_semi = '\0';
                s = next_semi + 1;
            } else {
                s = val + strlen(val);
            }

            // Add parameter
            if (ct->num_params >= capacity) {
                capacity *= 2;
                struct mime_param *new_params = realloc(ct->params,
                    sizeof(struct mime_param) * capacity);
                if (!new_params) {
                    free(input);
                    return -1;
                }
                ct->params = new_params;
            }

            ct->params[ct->num_params].name = strdup(name);
            ct->params[ct->num_params].value = strdup(val);
            ct->num_params++;
        }
    } else {
        ct->subtype = strdup(trim_whitespace(s));
    }

    free(input);
    return 0;
}

// ------------------------
// Message ID Parsing
// ------------------------

static int parse_message_ids(const char *value, char ***ids, size_t *count) {
    if (!value) {
        *ids = NULL;
        *count = 0;
        return 0;
    }

    char *input = unfold_header(value);
    if (!input) return -1;

    size_t capacity = 4;
    char **result = malloc(sizeof(char*) * capacity);
    if (!result) {
        free(input);
        return -1;
    }

    size_t n = 0;
    const char *s = input;

    while (*s) {
        // Skip whitespace
        while (*s && isspace(*s)) s++;
        if (!*s) break;

        // Find message ID (between '<' and '>')
        if (*s == '<') {
            s++;
            const char *start = s;
            while (*s && *s != '>') s++;

            if (*s == '>') {
                if (n >= capacity) {
                    capacity *= 2;
                    char **new_result = realloc(result, sizeof(char*) * capacity);
                    if (!new_result) {
                        for (size_t i = 0; i < n; i++) free(result[i]);
                        free(result);
                        free(input);
                        return -1;
                    }
                    result = new_result;
                }

                size_t len = s - start;
                result[n] = malloc(len + 3); // '<' + content + '>' + '\0'
                if (!result[n]) {
                    for (size_t i = 0; i < n; i++) free(result[i]);
                    free(result);
                    free(input);
                    return -1;
                }
                sprintf(result[n], "<%.*s>", (int)len, start);
                n++;
                s++;
            }
        } else {
            s++;
        }
    }

    free(input);
    *ids = result;
    *count = n;
    return 0;
}

// ------------------------
// Message Header Parsing
// ------------------------

int parse_message_headers(const char *headers, struct message *msg) {
    if (!headers || !msg) return -1;

    memset(msg, 0, sizeof(*msg));

    // Split headers into lines
    char *input = strdup(headers);
    if (!input) return -1;

    char *line = input;
    char *next_line;

    while (line && *line) {
        // Find next line
        next_line = strstr(line, "\n");
        if (next_line) {
            *next_line = '\0';
            next_line++;

            // Handle line folding (next line starts with whitespace)
            while (*next_line && isspace(*next_line) && *next_line != '\n') {
                // This is a continuation, skip for now
                // (unfold_header handles this)
                char *fold_end = strstr(next_line, "\n");
                if (fold_end) {
                    next_line = fold_end + 1;
                } else {
                    next_line = NULL;
                    break;
                }
            }
        }

        // Parse header
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char *name = trim_whitespace(line);
            char *value_unfolded = unfold_header(colon + 1);
            if (!value_unfolded) continue;

            char *value = trim_whitespace(value_unfolded);

            if (strcasecmp(name, "message-id") == 0) {
                parse_message_ids(value, &msg->message_id, &msg->message_id_count);
            } else if (strcasecmp(name, "in-reply-to") == 0) {
                parse_message_ids(value, &msg->in_reply_to, &msg->in_reply_to_count);
            } else if (strcasecmp(name, "references") == 0) {
                parse_message_ids(value, &msg->references, &msg->references_count);
            } else if (strcasecmp(name, "from") == 0) {
                parse_email_address_list(value, &msg->from, &msg->from_count);
            } else if (strcasecmp(name, "sender") == 0) {
                parse_email_address_list(value, &msg->sender, &msg->sender_count);
            } else if (strcasecmp(name, "to") == 0) {
                parse_email_address_list(value, &msg->to, &msg->to_count);
            } else if (strcasecmp(name, "cc") == 0) {
                parse_email_address_list(value, &msg->cc, &msg->cc_count);
            } else if (strcasecmp(name, "bcc") == 0) {
                parse_email_address_list(value, &msg->bcc, &msg->bcc_count);
            } else if (strcasecmp(name, "reply-to") == 0) {
                parse_email_address_list(value, &msg->reply_to, &msg->reply_to_count);
            } else if (strcasecmp(name, "subject") == 0) {
                msg->subject = strdup(value);
            } else if (strcasecmp(name, "date") == 0) {
                msg->date = strdup(value);
            }

            free(value_unfolded);
        }

        line = next_line;
    }

    free(input);
    return 0;
}

// ------------------------
// MIME Header Parsing
// ------------------------

int parse_mime_headers(const char *headers, struct mime_part *part) {
    if (!headers || !part) return -1;

    memset(part, 0, sizeof(*part));

    // Default Content-Type is text/plain
    part->content_type.type = strdup("text");
    part->content_type.subtype = strdup("plain");

    // Parse headers
    char *input = strdup(headers);
    if (!input) return -1;

    // Initialize additional headers array
    size_t hdr_capacity = 8;
    part->headers = malloc(sizeof(struct header) * hdr_capacity);
    if (!part->headers) {
        free(input);
        return -1;
    }

    char *line = input;
    char *next_line;

    while (line && *line) {
        next_line = strstr(line, "\n");
        if (next_line) {
            *next_line = '\0';
            next_line++;
        }

        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char *name = trim_whitespace(line);
            char *value_unfolded = unfold_header(colon + 1);
            if (!value_unfolded) continue;

            char *value = trim_whitespace(value_unfolded);

            if (strcasecmp(name, "content-type") == 0) {
                free(part->content_type.type);
                free(part->content_type.subtype);
                parse_content_type(value, &part->content_type);
            } else if (strcasecmp(name, "content-transfer-encoding") == 0) {
                part->content_transfer_encoding = strdup(value);
            } else if (strcasecmp(name, "content-disposition") == 0) {
                part->content_disposition = strdup(value);
                // TODO: Parse disposition parameters
            } else if (strcasecmp(name, "content-id") == 0) {
                part->content_id = strdup(value);
            } else {
                // Store as additional header
                if (part->num_headers >= hdr_capacity) {
                    hdr_capacity *= 2;
                    struct header *new_hdrs = realloc(part->headers,
                        sizeof(struct header) * hdr_capacity);
                    if (!new_hdrs) {
                        free(value);
                        free(input);
                        return -1;
                    }
                    part->headers = new_hdrs;
                }

                part->headers[part->num_headers].name = strdup(name);
                part->headers[part->num_headers].value = strdup(value);
                part->num_headers++;
            }

            free(value_unfolded);
        }

        line = next_line;
    }

    free(input);
    return 0;
}

// ------------------------
// Memory Management
// ------------------------

void free_mime_param(struct mime_param *param) {
    if (!param) return;
    free(param->name);
    free(param->value);
}

void free_content_type(struct content_type *ct) {
    if (!ct) return;
    free(ct->type);
    free(ct->subtype);
    if (ct->params) {
        for (size_t i = 0; i < ct->num_params; i++) {
            free_mime_param(&ct->params[i]);
        }
        free(ct->params);
    }
}

void free_header(struct header *hdr) {
    if (!hdr) return;
    free(hdr->name);
    free(hdr->value);
}

void free_message(struct message *msg) {
    if (!msg) return;

    if (msg->message_id) {
        for (size_t i = 0; i < msg->message_id_count; i++) {
            free(msg->message_id[i]);
        }
        free(msg->message_id);
    }

    if (msg->in_reply_to) {
        for (size_t i = 0; i < msg->in_reply_to_count; i++) {
            free(msg->in_reply_to[i]);
        }
        free(msg->in_reply_to);
    }

    if (msg->references) {
        for (size_t i = 0; i < msg->references_count; i++) {
            free(msg->references[i]);
        }
        free(msg->references);
    }

    free_email_address_list(msg->sender, msg->sender_count);
    free_email_address_list(msg->from, msg->from_count);
    free_email_address_list(msg->to, msg->to_count);
    free_email_address_list(msg->cc, msg->cc_count);
    free_email_address_list(msg->bcc, msg->bcc_count);
    free_email_address_list(msg->reply_to, msg->reply_to_count);

    free(msg->subject);
    free(msg->date);
}

void free_mime_part(struct mime_part *part) {
    if (!part) return;

    free_content_type(&part->content_type);
    free(part->content_transfer_encoding);
    free(part->content_disposition);

    if (part->content_disposition_params) {
        for (size_t i = 0; i < part->num_disposition_params; i++) {
            free_mime_param(&part->content_disposition_params[i]);
        }
        free(part->content_disposition_params);
    }

    free(part->content_id);

    if (part->headers) {
        for (size_t i = 0; i < part->num_headers; i++) {
            free_header(&part->headers[i]);
        }
        free(part->headers);
    }

    free(part->body);

    if (part->message) {
        free_message(part->message);
        free(part->message);
    }

    if (part->parts) {
        for (size_t i = 0; i < part->num_parts; i++) {
            free_mime_part(part->parts[i]);
            free(part->parts[i]);
        }
        free(part->parts);
    }
}

// TODO: Implement parse_mime_part and parse_multipart
// These are more complex and require recursive parsing

int parse_mime_part(const char *input, struct mime_part *part) {
    (void)input;
    (void)part;
    // TODO: Implement full MIME part parsing
    return -1;
}

int parse_multipart(const char *body, const char *boundary,
                    struct mime_part ***parts, size_t *num_parts) {
    (void)body;
    (void)boundary;
    (void)parts;
    (void)num_parts;
    // TODO: Implement multipart parsing
    return -1;
}
