// Email address parser implementation

#define _POSIX_C_SOURCE 200809L
#include "email_address.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

// ------------------------
// Helper Functions
// ------------------------

/**
 * skip_whitespace - Skip whitespace and comments
 * @s: Pointer to string pointer (updated to point after whitespace)
 *
 * Skips:
 * - Linear whitespace (space, tab, CR, LF)
 * - Comments in parentheses (with nesting support)
 */
static void skip_whitespace(const char **s) {
    while (**s) {
        if (isspace(**s)) {
            (*s)++;
        } else if (**s == '(') {
            // Skip comment (with nesting)
            (*s)++;
            int depth = 1;
            while (**s && depth > 0) {
                if (**s == '\\' && *(*s + 1)) {
                    (*s) += 2; // Skip quoted-pair
                } else if (**s == '(') {
                    depth++;
                    (*s)++;
                } else if (**s == ')') {
                    depth--;
                    (*s)++;
                } else {
                    (*s)++;
                }
            }
        } else {
            break;
        }
    }
}

/**
 * extract_quoted_string - Extract and decode a quoted-string
 * @s: Pointer to string pointer (should point to opening DQUOTE)
 *
 * Returns: Newly allocated string with quotes removed and quoted-pairs decoded
 *          Caller must free. Returns NULL on error.
 */
static char *extract_quoted_string(const char **s) {
    if (**s != '"') return NULL;

    (*s)++; // Skip opening quote

    char *result = malloc(strlen(*s) + 1);
    if (!result) return NULL;

    char *dst = result;
    while (**s && **s != '"') {
        if (**s == '\\' && *(*s + 1)) {
            // Quoted-pair: skip backslash, copy next char
            (*s)++;
            *dst++ = **s;
            (*s)++;
        } else {
            *dst++ = **s;
            (*s)++;
        }
    }
    *dst = '\0';

    if (**s == '"') {
        (*s)++; // Skip closing quote
    }

    // Trim leading and trailing whitespace
    char *start = result;
    while (*start && isspace(*start)) start++;

    char *end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) *end-- = '\0';

    if (start != result) {
        memmove(result, start, strlen(start) + 1);
    }

    return result;
}

/**
 * extract_atom - Extract an atom (unquoted word)
 * @s: Pointer to string pointer
 *
 * Returns: Newly allocated string containing the atom
 *          Caller must free. Returns NULL if no atom found.
 */
static char *extract_atom(const char **s) {
    const char *start = *s;

    // Atom chars: printable ASCII except specials
    while (**s && !isspace(**s) &&
           **s != '(' && **s != ')' && **s != '<' && **s != '>' &&
           **s != '@' && **s != ',' && **s != ';' && **s != ':' &&
           **s != '\\' && **s != '"' && **s != '[' && **s != ']') {
        (*s)++;
    }

    if (*s == start) return NULL;

    size_t len = *s - start;
    char *result = malloc(len + 1);
    if (!result) return NULL;

    memcpy(result, start, len);
    result[len] = '\0';

    return result;
}

/**
 * extract_angle_addr - Extract addr-spec from angle brackets
 * @s: Pointer to string pointer (should point to '<')
 *
 * Returns: Newly allocated string with the addr-spec
 *          Caller must free. Returns NULL on error.
 */
static char *extract_angle_addr(const char **s) {
    if (**s != '<') return NULL;

    (*s)++; // Skip '<'
    skip_whitespace(s);

    const char *start = *s;

    // Find closing '>'
    while (**s && **s != '>') {
        if (**s == '\\' && *(*s + 1)) {
            (*s) += 2; // Skip quoted-pair
        } else {
            (*s)++;
        }
    }

    const char *end = *s;

    if (**s == '>') {
        (*s)++; // Skip '>'
    }

    // Trim whitespace
    while (end > start && isspace(*(end - 1))) end--;

    size_t len = end - start;
    char *result = malloc(len + 1);
    if (!result) return NULL;

    memcpy(result, start, len);
    result[len] = '\0';

    return result;
}

/**
 * extract_addr_spec - Extract a bare addr-spec (without angle brackets)
 * @s: Pointer to string pointer
 *
 * Returns: Newly allocated string with the addr-spec
 *          Caller must free. Returns NULL on error.
 */
static char *extract_addr_spec(const char **s) {
    const char *start = *s;

    // Simple extraction: read until comma, semicolon, or end
    while (**s && **s != ',' && **s != ';' && **s != '<' && **s != '>') {
        (*s)++;
    }

    const char *end = *s;

    // Trim whitespace
    while (end > start && isspace(*(end - 1))) end--;

    size_t len = end - start;
    char *result = malloc(len + 1);
    if (!result) return NULL;

    memcpy(result, start, len);
    result[len] = '\0';

    return result;
}

// ------------------------
// Public API
// ------------------------

void free_email_address(struct email_address *addr) {
    if (!addr) return;
    free(addr->name);
    free(addr->email);
    addr->name = NULL;
    addr->email = NULL;
}

void free_email_address_list(struct email_address *addresses, size_t count) {
    if (!addresses) return;
    for (size_t i = 0; i < count; i++) {
        free_email_address(&addresses[i]);
    }
    free(addresses);
}

int parse_email_address(const char *input, struct email_address *addr) {
    if (!input || !addr) return -1;

    memset(addr, 0, sizeof(*addr));

    const char *s = input;
    skip_whitespace(&s);

    if (!*s) return -1;

    char *name = NULL;
    char *email = NULL;

    // Check if we have a display name
    if (*s == '"') {
        // Quoted display name
        name = extract_quoted_string(&s);
        skip_whitespace(&s);
    } else if (*s != '<') {
        // Might be an atom or phrase (sequence of atoms)
        const char *checkpoint = s;
        char *atom = extract_atom(&s);
        skip_whitespace(&s);

        if (*s == '<') {
            // It was a display name
            name = atom;
        } else {
            // It was the start of a bare addr-spec, restore position
            free(atom);
            s = checkpoint;
        }
    }

    // Extract email address
    skip_whitespace(&s);
    if (*s == '<') {
        email = extract_angle_addr(&s);
    } else {
        email = extract_addr_spec(&s);
    }

    if (!email) {
        free(name);
        return -1;
    }

    addr->name = name;
    addr->email = email;

    return 0;
}

int parse_email_address_list(const char *input, struct email_address **addresses, size_t *count) {
    if (!input || !addresses || !count) return -1;

    *addresses = NULL;
    *count = 0;

    // First pass: count addresses and allocate
    const char *s = input;
    size_t capacity = 4;
    struct email_address *addrs = malloc(sizeof(struct email_address) * capacity);
    if (!addrs) return -1;

    size_t n = 0;

    while (*s) {
        skip_whitespace(&s);
        if (!*s) break;

        // Check for group syntax: name ':'
        int is_group = 0;

        // Simple group detection: look for ':' before '<' or '@'
        const char *peek = s;
        while (*peek && *peek != ',' && *peek != ';') {
            if (*peek == '<' || *peek == '@') break;
            if (*peek == ':') {
                is_group = 1;
                break;
            }
            peek++;
        }

        if (is_group) {
            // Skip group name and colon
            while (*s && *s != ':') s++;
            if (*s == ':') s++;
            skip_whitespace(&s);

            // Parse addresses in group until ';'
            while (*s && *s != ';') {
                skip_whitespace(&s);
                if (!*s || *s == ';') break;

                if (n >= capacity) {
                    capacity *= 2;
                    struct email_address *new_addrs = realloc(addrs, sizeof(struct email_address) * capacity);
                    if (!new_addrs) {
                        free_email_address_list(addrs, n);
                        return -1;
                    }
                    addrs = new_addrs;
                }

                if (parse_email_address(s, &addrs[n]) == 0) {
                    n++;

                    // Skip to next comma or semicolon
                    while (*s && *s != ',' && *s != ';') s++;
                    if (*s == ',') s++;
                } else {
                    // Skip to next comma or semicolon
                    while (*s && *s != ',' && *s != ';') s++;
                    if (*s == ',') s++;
                }
            }

            if (*s == ';') s++;
        } else {
            // Parse single address
            if (n >= capacity) {
                capacity *= 2;
                struct email_address *new_addrs = realloc(addrs, sizeof(struct email_address) * capacity);
                if (!new_addrs) {
                    free_email_address_list(addrs, n);
                    return -1;
                }
                addrs = new_addrs;
            }

            // Extract just this address (up to comma)
            const char *addr_start = s;
            int depth = 0;
            while (*s && (*s != ',' || depth > 0)) {
                if (*s == '<') depth++;
                else if (*s == '>') depth--;
                else if (*s == '"') {
                    s++;
                    while (*s && *s != '"') {
                        if (*s == '\\' && *(s + 1)) s++;
                        s++;
                    }
                    if (*s == '"') s++;
                    continue;
                }
                s++;
            }

            size_t addr_len = s - addr_start;
            char *addr_str = malloc(addr_len + 1);
            if (!addr_str) {
                free_email_address_list(addrs, n);
                return -1;
            }
            memcpy(addr_str, addr_start, addr_len);
            addr_str[addr_len] = '\0';

            if (parse_email_address(addr_str, &addrs[n]) == 0) {
                n++;
            }

            free(addr_str);
        }

        skip_whitespace(&s);
        if (*s == ',') {
            s++;
        }
    }

    *addresses = addrs;
    *count = n;

    return 0;
}
