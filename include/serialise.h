// Header-only macro-based serialization/deserialization library
// See README for usage. Designed for GCC/Clang with macro extensions.

#ifndef SERIALISE_H_
#define SERIALISE_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// ------------------------
// Configuration & Hooks
// ------------------------

// Allocation used by charptr deserialisation (len is payload bytes; impl should add NUL)
#ifndef SERIAL_ALLOC
#define SERIAL_ALLOC(sz) malloc(sz)
#endif

// No-op default hooks; users can override before including this header.
#ifndef SERIALISE_HOOK_BEFORE_SIZE
#define SERIALISE_HOOK_BEFORE_SIZE(name, T, r) do { (void)(r); } while (0)
#endif
#ifndef SERIALISE_HOOK_AFTER_SIZE
#define SERIALISE_HOOK_AFTER_SIZE(name, T, r, sz) do { (void)(r); (void)(sz); } while (0)
#endif
#ifndef SERIALISE_HOOK_BEFORE_ENCODE
#define SERIALISE_HOOK_BEFORE_ENCODE(name, T, r, buf) do { (void)(r); (void)(buf); } while (0)
#endif
#ifndef SERIALISE_HOOK_AFTER_ENCODE
#define SERIALISE_HOOK_AFTER_ENCODE(name, T, r, buf) do { (void)(r); (void)(buf); } while (0)
#endif
#ifndef SERIALISE_HOOK_BEFORE_DECODE
#define SERIALISE_HOOK_BEFORE_DECODE(name, T, r, buf) do { (void)(r); (void)(buf); } while (0)
#endif
#ifndef SERIALISE_HOOK_AFTER_DECODE
#define SERIALISE_HOOK_AFTER_DECODE(name, T, r, buf) do { (void)(r); (void)(buf); } while (0)
#endif

// ------------------------
// Endian helpers (Big-Endian for sortable keys)
// ------------------------

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

// Big-endian conversion functions (for sortable numeric keys in KV stores)
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#  define SER_BE16(x) (uint16_t)(x)
#  define SER_BE32(x) (uint32_t)(x)
#  define SER_BE64(x) (uint64_t)(x)
#else
#  if __has_builtin(__builtin_bswap16) || (defined(__GNUC__) && (__GNUC__ >= 4))
#    define SER_BE16(x) __builtin_bswap16((uint16_t)(x))
#  else
static inline uint16_t SER_BE16(uint16_t x) { return (uint16_t)((x<<8)|(x>>8)); }
#  endif
#  if __has_builtin(__builtin_bswap32) || (defined(__GNUC__) && (__GNUC__ >= 4))
#    define SER_BE32(x) __builtin_bswap32((uint32_t)(x))
#  else
static inline uint32_t SER_BE32(uint32_t x) {
  return ((x & 0x000000FFu) << 24) |
         ((x & 0x0000FF00u) << 8)  |
         ((x & 0x00FF0000u) >> 8)  |
         ((x & 0xFF000000u) >> 24);
}
#  endif
#  if __has_builtin(__builtin_bswap64) || (defined(__GNUC__) && (__GNUC__ >= 4))
#    define SER_BE64(x) __builtin_bswap64((uint64_t)(x))
#  else
static inline uint64_t SER_BE64(uint64_t x) {
  x = ((x & 0x00000000FFFFFFFFull) << 32) | ((x & 0xFFFFFFFF00000000ull) >> 32);
  x = ((x & 0x0000FFFF0000FFFFull) << 16) | ((x & 0xFFFF0000FFFF0000ull) >> 16);
  x = ((x & 0x00FF00FF00FF00FFull) << 8)  | ((x & 0xFF00FF00FF00FF00ull) >> 8);
  return x;
}
#  endif
#endif

// Copy helpers to avoid alignment issues (Big-Endian for sortable keys)
#define SER_WRITE_U8(buf, v)   do { *(uint8_t*)(buf) = (uint8_t)(v); (buf) += 1; } while (0)
#define SER_READ_U8(buf, out)  do { (out) = *(const uint8_t*)(buf); (buf) += 1; } while (0)

#define SER_WRITE_U16(buf, v)  do { uint16_t __ser_u16 = SER_BE16((uint16_t)(v)); memcpy((buf), &__ser_u16, 2); (buf) += 2; } while (0)
#define SER_READ_U16(buf, out) do { uint16_t __ser_u16; memcpy(&__ser_u16, (buf), 2); (out) = SER_BE16(__ser_u16); (buf) += 2; } while (0)

#define SER_WRITE_U32(buf, v)  do { uint32_t __ser_u32 = SER_BE32((uint32_t)(v)); memcpy((buf), &__ser_u32, 4); (buf) += 4; } while (0)
#define SER_READ_U32(buf, out) do { uint32_t __ser_u32; memcpy(&__ser_u32, (buf), 4); (out) = SER_BE32(__ser_u32); (buf) += 4; } while (0)

#define SER_WRITE_U64(buf, v)  do { uint64_t __ser_u64 = SER_BE64((uint64_t)(v)); memcpy((buf), &__ser_u64, 8); (buf) += 8; } while (0)
#define SER_READ_U64(buf, out) do { uint64_t __ser_u64; memcpy(&__ser_u64, (buf), 8); (out) = SER_BE64(__ser_u64); (buf) += 8; } while (0)

// Signed integers: flip sign bit for correct sort order (negative < positive)
// This ensures byte-wise comparison matches numeric comparison
#define SER_WRITE_I8(buf, v)   do { uint8_t __ser_i8 = (uint8_t)(v) ^ 0x80; SER_WRITE_U8(buf, __ser_i8); } while (0)
#define SER_READ_I8(buf, out)  do { uint8_t __ser_i8; SER_READ_U8(buf, __ser_i8); (out) = (int8_t)(__ser_i8 ^ 0x80); } while (0)
#define SER_WRITE_I16(buf, v)  do { uint16_t __ser_i16 = (uint16_t)(v) ^ 0x8000; SER_WRITE_U16(buf, __ser_i16); } while (0)
#define SER_READ_I16(buf, out) do { uint16_t __ser_i16; SER_READ_U16(buf, __ser_i16); (out) = (int16_t)(__ser_i16 ^ 0x8000); } while (0)
#define SER_WRITE_I32(buf, v)  do { uint32_t __ser_i32 = (uint32_t)(v) ^ 0x80000000u; SER_WRITE_U32(buf, __ser_i32); } while (0)
#define SER_READ_I32(buf, out) do { uint32_t __ser_i32; SER_READ_U32(buf, __ser_i32); (out) = (int32_t)(__ser_i32 ^ 0x80000000u); } while (0)
#define SER_WRITE_I64(buf, v)  do { uint64_t __ser_i64 = (uint64_t)(v) ^ 0x8000000000000000ull; SER_WRITE_U64(buf, __ser_i64); } while (0)
#define SER_READ_I64(buf, out) do { uint64_t __ser_i64; SER_READ_U64(buf, __ser_i64); (out) = (int64_t)(__ser_i64 ^ 0x8000000000000000ull); } while (0)

// ------------------------
// Type tags and adapters
// ------------------------

// Map common C types to compact tags (users may extend by defining SER_TAG_<type>)
#define SER_TAG_uint8_t   u8
#define SER_TAG_uint16_t  u16
#define SER_TAG_uint32_t  u32
#define SER_TAG_uint64_t  u64
#define SER_TAG_int8_t    i8
#define SER_TAG_int16_t   i16
#define SER_TAG_int32_t   i32
#define SER_TAG_int64_t   i64
#define SER_TAG_size_t    size
#define SER_TAG_charptr   charptr
#define SER_TAG_timespec  timespec

// Common aliases seen in legacy code
#define SER_TAG_bit32     u32
#define SER_TAG_bit64     u64

// Helper to map a provided type token to a tag
#define SER_MAP(type) SER_CAT(SER_TAG_, type)

// Token pasting helpers
#define SER_CAT_(a,b) a##b
#define SER_CAT(a,b) SER_CAT_(a,b)

// Primitive type size expressions
#define TYPE_SIZEOF_u8(v)   (1u)
#define TYPE_SIZEOF_u16(v)  (2u)
#define TYPE_SIZEOF_u32(v)  (4u)
#define TYPE_SIZEOF_u64(v)  (8u)
#define TYPE_SIZEOF_i8(v)   (1u)
#define TYPE_SIZEOF_i16(v)  (2u)
#define TYPE_SIZEOF_i32(v)  (4u)
#define TYPE_SIZEOF_i64(v)  (8u)

// size_t encodes as fixed 8 bytes (uint64 little-endian)
#define TYPE_SIZEOF_size(v) (8u)

// charptr encodes as: uint32 length (bytes, not including NUL) + bytes
#define TYPE_SIZEOF_charptr(v) (4u + (uint32_t)((v) ? strlen(v) : 0u))

/* timespec sizing defined with its compact encoding below */

// Encoding/Decoding primitives
#define TYPE_ENC_u8(buf, v)   SER_WRITE_U8(buf, (v))
#define TYPE_DEC_u8(buf, l)   SER_READ_U8(buf, (l))
#define TYPE_ENC_u16(buf, v)  SER_WRITE_U16(buf, (v))
#define TYPE_DEC_u16(buf, l)  SER_READ_U16(buf, (l))
#define TYPE_ENC_u32(buf, v)  SER_WRITE_U32(buf, (v))
#define TYPE_DEC_u32(buf, l)  SER_READ_U32(buf, (l))
#define TYPE_ENC_u64(buf, v)  SER_WRITE_U64(buf, (v))
#define TYPE_DEC_u64(buf, l)  SER_READ_U64(buf, (l))

#define TYPE_ENC_i8(buf, v)   SER_WRITE_I8(buf, (v))
#define TYPE_DEC_i8(buf, l)   SER_READ_I8(buf, (l))
#define TYPE_ENC_i16(buf, v)  SER_WRITE_I16(buf, (v))
#define TYPE_DEC_i16(buf, l)  SER_READ_I16(buf, (l))
#define TYPE_ENC_i32(buf, v)  SER_WRITE_I32(buf, (v))
#define TYPE_DEC_i32(buf, l)  SER_READ_I32(buf, (l))
#define TYPE_ENC_i64(buf, v)  SER_WRITE_I64(buf, (v))
#define TYPE_DEC_i64(buf, l)  SER_READ_I64(buf, (l))

// size_t: always encode/decode as 8 bytes for portability
#define TYPE_ENC_size(buf, v) do { \
  uint64_t __ser_tmp64 = (uint64_t)(v); \
  TYPE_ENC_u64(buf, __ser_tmp64); \
} while (0)

#define TYPE_DEC_size(buf, l) do { \
  uint64_t __ser_tmp64; \
  TYPE_DEC_u64(buf, __ser_tmp64); \
  (l) = (size_t)__ser_tmp64; \
} while (0)

// char * (charptr)
#define TYPE_ENC_charptr(buf, v) do { \
  uint32_t __ser_len = (uint32_t)((v) ? strlen(v) : 0u); \
  TYPE_ENC_u32(buf, __ser_len); \
  if (__ser_len) { memcpy((buf), (const void*)(v), __ser_len); (buf) += __ser_len; } \
} while (0)

#define TYPE_DEC_charptr(buf, l) do { \
  uint32_t __ser_len = 0; TYPE_DEC_u32(buf, __ser_len); \
  char *__ser_s = (char*)SERIAL_ALLOC((size_t)__ser_len + 1u); \
  if (__ser_len) { memcpy(__ser_s, (buf), __ser_len); (buf) += __ser_len; } \
  __ser_s[__ser_len] = '\0'; \
  (l) = __ser_s; \
} while (0)

// timespec encoded compactly into 8 bytes (sortable by time):
// Encoded as: tv_sec (signed 64-bit with sign-flip) in high bits, tv_nsec in low bits
// This ensures chronological ordering when used as keys
#define TYPE_SIZEOF_timespec(v) (8u)

#define TYPE_ENC_timespec(buf, v) do { \
  int64_t __ser_sec = (int64_t)((v).tv_sec); \
  uint64_t __ser_nsec = (uint64_t)((v).tv_nsec) & ((1ULL<<30) - 1ULL); \
  uint64_t __ser_sec34 = ((uint64_t)__ser_sec) & ((1ULL<<34) - 1ULL); \
  uint64_t __ser_packed = (__ser_sec34 << 30) | __ser_nsec; \
  /* Flip sign bit of entire packed value for sortable encoding */ \
  __ser_packed ^= 0x8000000000000000ull; \
  TYPE_ENC_u64(buf, __ser_packed); \
} while (0)

#define TYPE_DEC_timespec(buf, l) do { \
  uint64_t __ser_packed; \
  TYPE_DEC_u64(buf, __ser_packed); \
  /* Flip sign bit back */ \
  __ser_packed ^= 0x8000000000000000ull; \
  uint64_t __ser_nsec = __ser_packed & ((1ULL<<30) - 1ULL); \
  uint64_t __ser_sec34 = (__ser_packed >> 30) & ((1ULL<<34) - 1ULL); \
  int64_t __ser_sec = ( (__ser_sec34 & (1ULL<<33)) \
                        ? (int64_t)(__ser_sec34 | (~((1ULL<<34) - 1ULL))) \
                        : (int64_t)__ser_sec34 ); \
  (l).tv_sec = (time_t)__ser_sec; \
  (l).tv_nsec = (long)__ser_nsec; \
} while (0)

// Wrapper to call size/enc/dec by tag
#define TYPE_SIZEOF(tag, v) SER_CAT(TYPE_SIZEOF_, tag)(v)
#define TYPE_ENC(tag, buf, v) SER_CAT(TYPE_ENC_, tag)(buf, v)
#define TYPE_DEC(tag, buf, l) SER_CAT(TYPE_DEC_, tag)(buf, l)

// ------------------------
// Field list expansion machinery
// ------------------------

// Turn a field spec into a normalized tuple
#define SERIAL_TUPLE(...) (__VA_ARGS__)

#define SERIAL_FIELD_SELECT(_1,_2,_3, NAME, ...) NAME
#define SERIAL_FIELD2(name, type)            SERIAL_TUPLE(SCALAR, name, type)
#define SERIAL_FIELD3(name, type, count)     SERIAL_TUPLE(ARRAY,  name, type, count)
#define SERIALISE_FIELD(...) SERIAL_FIELD_SELECT(__VA_ARGS__, SERIAL_FIELD3, SERIAL_FIELD2)(__VA_ARGS__)

// for-each implementation up to 32 items
#define FE_1(M, X) M(X);
#define FE_2(M, X, ...) M(X); FE_1(M, __VA_ARGS__)
#define FE_3(M, X, ...) M(X); FE_2(M, __VA_ARGS__)
#define FE_4(M, X, ...) M(X); FE_3(M, __VA_ARGS__)
#define FE_5(M, X, ...) M(X); FE_4(M, __VA_ARGS__)
#define FE_6(M, X, ...) M(X); FE_5(M, __VA_ARGS__)
#define FE_7(M, X, ...) M(X); FE_6(M, __VA_ARGS__)
#define FE_8(M, X, ...) M(X); FE_7(M, __VA_ARGS__)
#define FE_9(M, X, ...) M(X); FE_8(M, __VA_ARGS__)
#define FE_10(M, X, ...) M(X); FE_9(M, __VA_ARGS__)
#define FE_11(M, X, ...) M(X); FE_10(M, __VA_ARGS__)
#define FE_12(M, X, ...) M(X); FE_11(M, __VA_ARGS__)
#define FE_13(M, X, ...) M(X); FE_12(M, __VA_ARGS__)
#define FE_14(M, X, ...) M(X); FE_13(M, __VA_ARGS__)
#define FE_15(M, X, ...) M(X); FE_14(M, __VA_ARGS__)
#define FE_16(M, X, ...) M(X); FE_15(M, __VA_ARGS__)
#define FE_17(M, X, ...) M(X); FE_16(M, __VA_ARGS__)
#define FE_18(M, X, ...) M(X); FE_17(M, __VA_ARGS__)
#define FE_19(M, X, ...) M(X); FE_18(M, __VA_ARGS__)
#define FE_20(M, X, ...) M(X); FE_19(M, __VA_ARGS__)
#define FE_21(M, X, ...) M(X); FE_20(M, __VA_ARGS__)
#define FE_22(M, X, ...) M(X); FE_21(M, __VA_ARGS__)
#define FE_23(M, X, ...) M(X); FE_22(M, __VA_ARGS__)
#define FE_24(M, X, ...) M(X); FE_23(M, __VA_ARGS__)
#define FE_25(M, X, ...) M(X); FE_24(M, __VA_ARGS__)
#define FE_26(M, X, ...) M(X); FE_25(M, __VA_ARGS__)
#define FE_27(M, X, ...) M(X); FE_26(M, __VA_ARGS__)
#define FE_28(M, X, ...) M(X); FE_27(M, __VA_ARGS__)
#define FE_29(M, X, ...) M(X); FE_28(M, __VA_ARGS__)
#define FE_30(M, X, ...) M(X); FE_29(M, __VA_ARGS__)
#define FE_31(M, X, ...) M(X); FE_30(M, __VA_ARGS__)
#define FE_32(M, X, ...) M(X); FE_31(M, __VA_ARGS__)

#define GET_FE_MACRO( \
 _1,_2,_3,_4,_5,_6,_7,_8,_9,_10, \
 _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
 _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
 _31,_32, NAME, ...) NAME

#define FOR_EACH(M, ...) GET_FE_MACRO(__VA_ARGS__, \
  FE_32,FE_31,FE_30,FE_29,FE_28,FE_27,FE_26,FE_25,FE_24,FE_23, \
  FE_22,FE_21,FE_20,FE_19,FE_18,FE_17,FE_16,FE_15,FE_14,FE_13, \
  FE_12,FE_11,FE_10,FE_9, FE_8, FE_7, FE_6, FE_5, FE_4, FE_3, FE_2, FE_1)(M, __VA_ARGS__)

// Item dispatch helpers
#define ITEM_SIZE(t) ITEM_SIZE_I t
#define ITEM_ENC(t)  ITEM_ENC_I t
#define ITEM_DEC(t)  ITEM_DEC_I t

#define ITEM_SIZE_I(kind, ...) SER_CAT(ITEM_SIZE_, kind)(__VA_ARGS__)
#define ITEM_ENC_I(kind, ...)  SER_CAT(ITEM_ENC_,  kind)(__VA_ARGS__)
#define ITEM_DEC_I(kind, ...)  SER_CAT(ITEM_DEC_,  kind)(__VA_ARGS__)

// SCALAR handlers: name, type
#define ITEM_SIZE_SCALAR(name, type) do { \
  (void)0; _sz += TYPE_SIZEOF(SER_MAP(type), r->name); \
} while (0)

#define ITEM_ENC_SCALAR(name, type) do { \
  TYPE_ENC(SER_MAP(type), buf, r->name); \
} while (0)

#define ITEM_DEC_SCALAR(name, type) do { \
  TYPE_DEC(SER_MAP(type), buf, r->name); \
} while (0)

// ARRAY handlers: name, type, count
#define ITEM_SIZE_ARRAY(name, type, count) do { \
  for (size_t _i = 0; _i < (size_t)(count); ++_i) { \
    _sz += TYPE_SIZEOF(SER_MAP(type), r->name[_i]); \
  } \
} while (0)

#define ITEM_ENC_ARRAY(name, type, count) do { \
  for (size_t _i = 0; _i < (size_t)(count); ++_i) { \
    TYPE_ENC(SER_MAP(type), buf, r->name[_i]); \
  } \
} while (0)

#define ITEM_DEC_ARRAY(name, type, count) do { \
  for (size_t _i = 0; _i < (size_t)(count); ++_i) { \
    TYPE_DEC(SER_MAP(type), buf, r->name[_i]); \
  } \
} while (0)

// ------------------------
// Codegen macro
// ------------------------

#define SERIALISE(name, ...) \
size_t SER_CAT(serialise_, SER_CAT(name, _size))(struct name *r) { \
  size_t _sz = 0; \
  SERIALISE_HOOK_BEFORE_SIZE(name, struct name, r); \
  FOR_EACH(ITEM_SIZE, __VA_ARGS__); \
  SERIALISE_HOOK_AFTER_SIZE(name, struct name, r, _sz); \
  return _sz; \
} \
char* SER_CAT(serialise_, name)(char *buf, struct name *r) { \
  SERIALISE_HOOK_BEFORE_ENCODE(name, struct name, r, buf); \
  FOR_EACH(ITEM_ENC, __VA_ARGS__); \
  SERIALISE_HOOK_AFTER_ENCODE(name, struct name, r, buf); \
  return buf; \
} \
char* SER_CAT(deserialise_, name)(char *buf, struct name *r) { \
  SERIALISE_HOOK_BEFORE_DECODE(name, struct name, r, buf); \
  FOR_EACH(ITEM_DEC, __VA_ARGS__); \
  SERIALISE_HOOK_AFTER_DECODE(name, struct name, r, buf); \
  return buf; \
}

// ------------------------
// Extending with custom types
// ------------------------
// To add a custom base type identified by token <tag>:
//   1) Define SER_TAG_<your-c-type> to expand to <tag> so field specs can use it.
//   2) Provide three macros:
//        #define TYPE_SIZEOF_<tag>(v)    /* returns size in bytes for value 'v' */
//        #define TYPE_ENC_<tag>(buf, v)  /* writes v to buf (advances buf) */
//        #define TYPE_DEC_<tag>(buf, l)  /* reads from buf into l (advances buf) */
//      See README for examples.

#ifdef __cplusplus
}
#endif

#endif // SERIALISE_H_
