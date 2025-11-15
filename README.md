# serialise.h — macro-based struct serialization for C

Header-only, zero-alloc-by-default (except for charptr), macro-driven serialization/deserialization for C structs. Generates highly efficient encode/decode functions that write to raw byte buffers in little-endian order.

Works with modern GCC/Clang (C99+). No runtime dependencies.

## Features

- Little-endian packing for fixed-width integers.
- Efficient adapters for: `uint{8,16,32,64}_t`, `int{8,16,32,64}_t`, `size_t` (always 8 bytes), `char *` (length-prefixed), `struct timespec` (compact 8-byte encoding: 34-bit seconds + 30-bit nanoseconds).
- Fixed-size arrays of any supported base type.
- Composable macro to generate complete functions: size, serialize, deserialize.
- Hooks to inject custom code at function boundaries (e.g., tracing, bounds checks).
- Extensible: add new base types via 3 small macros.

## Quick start

1) Drop `include/serialise.h` into your project, and include it:

```
#include "serialise.h"
```

2) Describe what fields to (de)serialize using the `SERIALISE(...)` macro and per-field specs. Example based on your `struct index_record`:

```
// Map custom types as needed
#define SER_TAG_message_guid message_guid
#define TYPE_SIZEOF_message_guid(v) (16u)
#define TYPE_ENC_message_guid(buf, v) do { memcpy((buf), (v).guid, 16); (buf) += 16; } while (0)
#define TYPE_DEC_message_guid(buf, l) do { memcpy((l).guid, (buf), 16); (buf) += 16; } while (0)

SERIALISE(ir, struct index_record,
  SERIALISE_FIELD(uid, uint32_t),
  SERIALISE_FIELD(internaldate, timespec),
  SERIALISE_FIELD(subject, charptr),
  // ... more fields ...
  SERIALISE_FIELD(user_flags, uint32_t, MAX_USER_FLAGS/32),
  SERIALISE_FIELD(guid, message_guid)
)
```

This generates:

```
size_t  serialise_ir_size(struct index_record *r);
char   *serialise_ir(char *buf, struct index_record *r);       // returns end pointer
char   *deserialise_ir(char *buf, struct index_record *r);     // returns end pointer
```

To generate a partial serializer/deserializer (e.g., just flags):

```
SERIALISE(flags, struct index_record,
  SERIALISE_FIELD(system_flags, uint32_t),
  SERIALISE_FIELD(internal_flags, uint32_t),
  SERIALISE_FIELD(user_flags, uint32_t, MAX_USER_FLAGS/32)
)
```

See `examples/index_record_example.c` for a complete runnable sample.

## Field spec syntax

- `SERIALISE_FIELD(name, type)` — scalar field
- `SERIALISE_FIELD(name, type, count)` — fixed-size array field

Where `type` is one of:

- Standard integer tokens: `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`, `int8_t`, `int16_t`, `int32_t`, `int64_t`
- `size_t` — always encoded as 8 bytes (uint64 little-endian)
- `charptr` — writes `uint32_t` length, then that many bytes (no NUL); deserialise allocates `len+1` and NUL-terminates
- `timespec` — compact 8-byte encoding: stores 34-bit signed seconds in the upper bits and 30-bit nanoseconds (0..999,999,999) in the lower bits
- Any custom type you add (see below)

Aliases provided: `bit32 -> uint32_t`, `bit64 -> uint64_t` (via tags).

## Adding custom base types

Define three macros named after a tag of your choice, and map your C type token to that tag for use in field specs:

```
// 1) Map your C type token to a tag
#define SER_TAG_message_guid message_guid

// 2) Implement size/encode/decode for that tag
#define TYPE_SIZEOF_message_guid(v) (16u)
#define TYPE_ENC_message_guid(buf, v) do { memcpy((buf), (v).guid, 16); (buf) += 16; } while (0)
#define TYPE_DEC_message_guid(buf, l) do { memcpy((l).guid, (buf), 16); (buf) += 16; } while (0)

// Usage in fields: SERIALISE_FIELD(guid, message_guid)
```

For more complex types, `TYPE_SIZEOF_<tag>(v)` can compute dynamic size from the value `v` (e.g., nested strings), and `TYPE_ENC/TYPE_DEC` can call other `TYPE_*` helpers.

## Hooks

Override any of these before including `serialise.h` to inject code at function boundaries:

```
#define SERIALISE_HOOK_BEFORE_SIZE(name, T, r)   /* e.g., assert(r) */
#define SERIALISE_HOOK_AFTER_SIZE(name, T, r, sz)
#define SERIALISE_HOOK_BEFORE_ENCODE(name, T, r, buf)
#define SERIALISE_HOOK_AFTER_ENCODE(name, T, r, buf)
#define SERIALISE_HOOK_BEFORE_DECODE(name, T, r, buf)
#define SERIALISE_HOOK_AFTER_DECODE(name, T, r, buf)
```

Also override `SERIAL_ALLOC(sz)` to control memory allocation for `charptr` deserialisation (defaults to `malloc`).

## Encoding details

- All integers use little-endian byte order. 8-bit values are unchanged.
- `size_t` encodes as fixed 8 bytes (uint64 little-endian).
- `charptr`: `uint32_t len` then `len` bytes (no NUL). Deserialiser allocates `len+1`, copies bytes, and appends `\0`.
- `timespec`: compact 8-byte encoding (34-bit signed seconds + 30-bit nanoseconds).

## Performance notes

- Uses `memcpy` to avoid alignment issues; compilers optimize these well.
- Arrays are encoded/decoded with tight loops; constants fold for fixed-size types.

## Limitations

- Type inference from field name is not portable in C and is not attempted. Supply a `type` tag in `SERIALISE_FIELD(...)`. You can add tags for your typedefs via `#define SER_TAG_<yourtype> <tag>`.
- `charptr` arrays are supported (they will loop with per-element dynamic size), but ensure you free allocated strings in your own code.
- If you need 32-bit `size_t` on a 64-bit platform (or vice versa), define a custom tag and adapters instead of using `size_t` directly.

## Build the example

```
make -C examples
./examples/run
```

## License

MIT-like for now; adjust as needed for your project.
