#ifndef MIME_SERIALISE_H
#define MIME_SERIALISE_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "serialise.h"
#include "email_address.h"
#include "mime_parser.h"
#include <endian.h>

// Serializers for MIME structures

// Custom type for nested struct (by value, not pointer)
#define ITEM_SIZE_STRUCT(name, struct_type) do { \
    _sz += SER_CAT(serialise_, SER_CAT(struct_type, _size))(&(r->name)); \
} while (0)

#define ITEM_ENC_STRUCT(name, struct_type) do { \
    buf = SER_CAT(serialise_, struct_type)(buf, &(r->name)); \
} while (0)

#define ITEM_DEC_STRUCT(name, struct_type) do { \
    buf = SER_CAT(deserialise_, struct_type)(buf, &(r->name)); \
} while (0)

#define SERIALISE_FIELD_STRUCT(name, struct_type) SERIAL_TUPLE(STRUCT, name, struct_type)

// email_address serializer
SERIALISE(email_address,
    SERIALISE_FIELD(name, charptr),
    SERIALISE_FIELD(email, charptr)
)

// mime_param serializer
SERIALISE(mime_param,
    SERIALISE_FIELD(name, charptr),
    SERIALISE_FIELD(value, charptr)
)

// content_type serializer
SERIALISE(content_type,
    SERIALISE_FIELD(type, charptr),
    SERIALISE_FIELD(subtype, charptr),
    SERIALISE_FIELD(num_params, uint64_t),
    SERIALISE_FIELD_PTR(params, mime_param, num_params)
)

// header serializer
SERIALISE(header,
    SERIALISE_FIELD(name, charptr),
    SERIALISE_FIELD(value, charptr)
)

// For string arrays (message_id, in_reply_to, references), we need a custom type
// Let's define a charptr_array type
#define ITEM_SIZE_CHARPTR_ARRAY(name, count_field) do { \
    for (uint64_t __i = 0; __i < r->count_field; __i++) { \
        size_t __len = r->name[__i] ? strlen(r->name[__i]) : 0; \
        _sz += sizeof(uint64_t) + __len; \
    } \
} while (0)

#define ITEM_ENC_CHARPTR_ARRAY(name, count_field) do { \
    for (uint64_t __i = 0; __i < r->count_field; __i++) { \
        size_t __len = r->name[__i] ? strlen(r->name[__i]) : 0; \
        uint64_t __len_be = SER_BE64(__len); \
        memcpy(buf, &__len_be, sizeof(uint64_t)); \
        buf += sizeof(uint64_t); \
        if (__len > 0) { \
            memcpy(buf, r->name[__i], __len); \
            buf += __len; \
        } \
    } \
} while (0)

#define ITEM_DEC_CHARPTR_ARRAY(name, count_field) do { \
    if (r->count_field > 0) { \
        r->name = (char**)SERIAL_ALLOC(sizeof(char*) * r->count_field); \
        for (uint64_t __i = 0; __i < r->count_field; __i++) { \
            uint64_t __len_be; \
            memcpy(&__len_be, buf, sizeof(uint64_t)); \
            uint64_t __len = SER_BE64(__len_be); \
            buf += sizeof(uint64_t); \
            if (__len > 0) { \
                r->name[__i] = (char*)SERIAL_ALLOC(__len + 1); \
                memcpy(r->name[__i], buf, __len); \
                r->name[__i][__len] = '\0'; \
                buf += __len; \
            } else { \
                r->name[__i] = NULL; \
            } \
        } \
    } else { \
        r->name = NULL; \
    } \
} while (0)

#define SERIALISE_FIELD_CHARPTR_ARRAY(name, count_field) SERIAL_TUPLE(CHARPTR_ARRAY, name, count_field)

// message serializer
SERIALISE(message,
    SERIALISE_FIELD(message_id_count, uint64_t),
    SERIALISE_FIELD_CHARPTR_ARRAY(message_id, message_id_count),
    SERIALISE_FIELD(in_reply_to_count, uint64_t),
    SERIALISE_FIELD_CHARPTR_ARRAY(in_reply_to, in_reply_to_count),
    SERIALISE_FIELD(references_count, uint64_t),
    SERIALISE_FIELD_CHARPTR_ARRAY(references, references_count),
    SERIALISE_FIELD(sender_count, uint64_t),
    SERIALISE_FIELD_PTR(sender, email_address, sender_count),
    SERIALISE_FIELD(from_count, uint64_t),
    SERIALISE_FIELD_PTR(from, email_address, from_count),
    SERIALISE_FIELD(to_count, uint64_t),
    SERIALISE_FIELD_PTR(to, email_address, to_count),
    SERIALISE_FIELD(cc_count, uint64_t),
    SERIALISE_FIELD_PTR(cc, email_address, cc_count),
    SERIALISE_FIELD(bcc_count, uint64_t),
    SERIALISE_FIELD_PTR(bcc, email_address, bcc_count),
    SERIALISE_FIELD(reply_to_count, uint64_t),
    SERIALISE_FIELD_PTR(reply_to, email_address, reply_to_count),
    SERIALISE_FIELD(subject, charptr),
    SERIALISE_FIELD(date, charptr)
)

// Forward declarations for mime_part recursion
size_t serialise_mime_part_size(struct mime_part *r);
char* serialise_mime_part(char *buf, struct mime_part *r);
char* deserialise_mime_part(char *buf, struct mime_part *r);

// mime_part serializer (recursive structure)
// We need custom handling for the nested parts array
#define ITEM_SIZE_MIME_PART_PTR(name, count_field) do { \
    for (uint64_t __i = 0; __i < r->count_field; __i++) { \
        _sz += serialise_mime_part_size(r->name[__i]); \
    } \
} while (0)

#define ITEM_ENC_MIME_PART_PTR(name, count_field) do { \
    for (uint64_t __i = 0; __i < r->count_field; __i++) { \
        buf = serialise_mime_part(buf, r->name[__i]); \
    } \
} while (0)

#define ITEM_DEC_MIME_PART_PTR(name, count_field) do { \
    if (r->count_field > 0) { \
        r->name = (struct mime_part**)SERIAL_ALLOC(sizeof(struct mime_part*) * r->count_field); \
        for (uint64_t __i = 0; __i < r->count_field; __i++) { \
            r->name[__i] = (struct mime_part*)SERIAL_ALLOC(sizeof(struct mime_part)); \
            buf = deserialise_mime_part(buf, r->name[__i]); \
        } \
    } else { \
        r->name = NULL; \
    } \
} while (0)

#define SERIALISE_FIELD_MIME_PART_PTR(name, count_field) SERIAL_TUPLE(MIME_PART_PTR, name, count_field)

// Custom handling for optional message pointer
#define ITEM_SIZE_MESSAGE_PTR(name) do { \
    if (r->name) { \
        _sz += sizeof(uint8_t) + serialise_message_size(r->name); \
    } else { \
        _sz += sizeof(uint8_t); \
    } \
} while (0)

#define ITEM_ENC_MESSAGE_PTR(name) do { \
    if (r->name) { \
        *buf++ = 1; \
        buf = serialise_message(buf, r->name); \
    } else { \
        *buf++ = 0; \
    } \
} while (0)

#define ITEM_DEC_MESSAGE_PTR(name) do { \
    uint8_t __present = *buf++; \
    if (__present) { \
        r->name = (struct message*)SERIAL_ALLOC(sizeof(struct message)); \
        buf = deserialise_message(buf, r->name); \
    } else { \
        r->name = NULL; \
    } \
} while (0)

#define SERIALISE_FIELD_MESSAGE_PTR(name) SERIAL_TUPLE(MESSAGE_PTR, name)

// Custom type for fixed-size char array (guid)
#define ITEM_SIZE_GUID(name) do { \
    _sz += 65; \
} while (0)

#define ITEM_ENC_GUID(name) do { \
    memcpy(buf, r->name, 65); \
    buf += 65; \
} while (0)

#define ITEM_DEC_GUID(name) do { \
    memcpy(r->name, buf, 65); \
    buf += 65; \
} while (0)

#define SERIALISE_FIELD_GUID(name) SERIAL_TUPLE(GUID, name)

// Custom type for raw body data (can contain nulls)
#define ITEM_SIZE_BODY_DATA(body_field, length_field) do { \
    _sz += r->length_field; \
} while (0)

#define ITEM_ENC_BODY_DATA(body_field, length_field) do { \
    if (r->length_field > 0 && r->body_field) { \
        memcpy(buf, r->body_field, r->length_field); \
        buf += r->length_field; \
    } \
} while (0)

#define ITEM_DEC_BODY_DATA(body_field, length_field) do { \
    if (r->length_field > 0) { \
        r->body_field = (char*)SERIAL_ALLOC(r->length_field + 1); \
        memcpy(r->body_field, buf, r->length_field); \
        r->body_field[r->length_field] = '\0'; \
        buf += r->length_field; \
    } else { \
        r->body_field = NULL; \
    } \
} while (0)

#define SERIALISE_FIELD_BODY_DATA(body_field, length_field) SERIAL_TUPLE(BODY_DATA, body_field, length_field)

// Now define the mime_part serializer
SERIALISE(mime_part,
    SERIALISE_FIELD_GUID(guid),
    SERIALISE_FIELD_STRUCT(content_type, content_type),
    SERIALISE_FIELD(content_transfer_encoding, charptr),
    SERIALISE_FIELD(content_disposition, charptr),
    SERIALISE_FIELD(num_disposition_params, uint64_t),
    SERIALISE_FIELD_PTR(content_disposition_params, mime_param, num_disposition_params),
    SERIALISE_FIELD(content_id, charptr),
    SERIALISE_FIELD(num_headers, uint64_t),
    SERIALISE_FIELD_PTR(headers, header, num_headers),
    SERIALISE_FIELD(body_length, uint64_t),
    SERIALISE_FIELD_BODY_DATA(body, body_length),
    SERIALISE_FIELD_MESSAGE_PTR(message),
    SERIALISE_FIELD(num_parts, uint64_t),
    SERIALISE_FIELD_MIME_PART_PTR(parts, num_parts)
)

#endif // MIME_SERIALISE_H
