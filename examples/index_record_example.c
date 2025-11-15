#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "../include/serialise.h"

#ifndef MAX_USER_FLAGS
#define MAX_USER_FLAGS 64
#endif

typedef uint64_t modseq_t;
typedef uint64_t bit64; // alias used in example
typedef uint32_t bit32; // alias used in example

struct message_guid { uint8_t guid[16]; };

// Map C token to tag for use in SERIALISE_FIELD(..., message_guid)
#define SER_TAG_message_guid message_guid

// Provide adapters for message_guid as fixed 16-byte blob
#define TYPE_SIZEOF_message_guid(v) (16u)
#define TYPE_ENC_message_guid(buf, v) do { memcpy((buf), (v).guid, 16); (buf) += 16; } while (0)
#define TYPE_DEC_message_guid(buf, l) do { memcpy((l).guid, (buf), 16); (buf) += 16; } while (0)

struct index_record {
    uint32_t uid;
    struct timespec internaldate;
    char * subject;
    struct timespec sentdate;
    uint64_t size;
    uint32_t header_size;
    struct timespec gmtime;
    size_t cache_offset;
    struct timespec last_updated;
    uint32_t system_flags;
    uint32_t internal_flags;
    uint32_t user_flags[MAX_USER_FLAGS/32];
    struct timespec savedate;
    uint16_t cache_version;
    struct message_guid guid;
    modseq_t modseq;
    modseq_t createdmodseq;
    bit64 cid;
    bit64 basecid;
    bit32 cache_crc;
};

// Build a serialiser for the entire index_record
SERIALISE(ir, struct index_record,
  SERIALISE_FIELD(uid, uint32_t),
  SERIALISE_FIELD(internaldate, timespec),
  SERIALISE_FIELD(subject, charptr),
  SERIALISE_FIELD(sentdate, timespec),
  SERIALISE_FIELD(size, uint64_t),
  SERIALISE_FIELD(header_size, uint32_t),
  SERIALISE_FIELD(gmtime, timespec),
  SERIALISE_FIELD(cache_offset, size_t),
  SERIALISE_FIELD(last_updated, timespec),
  SERIALISE_FIELD(system_flags, uint32_t),
  SERIALISE_FIELD(internal_flags, uint32_t),
  SERIALISE_FIELD(user_flags, uint32_t, MAX_USER_FLAGS/32),
  SERIALISE_FIELD(savedate, timespec),
  SERIALISE_FIELD(cache_version, uint16_t),
  SERIALISE_FIELD(guid, message_guid),
  SERIALISE_FIELD(modseq, uint64_t),
  SERIALISE_FIELD(createdmodseq, uint64_t),
  SERIALISE_FIELD(cid, uint64_t),
  SERIALISE_FIELD(basecid, uint64_t),
  SERIALISE_FIELD(cache_crc, uint32_t)
)

// Build a serialiser just for the flags subset
SERIALISE(flags, struct index_record,
  SERIALISE_FIELD(system_flags, uint32_t),
  SERIALISE_FIELD(internal_flags, uint32_t),
  SERIALISE_FIELD(user_flags, uint32_t, MAX_USER_FLAGS/32)
)

int main(void) {
  struct index_record r = {0};
  r.uid = 123;
  r.internaldate.tv_sec = 1700000000; r.internaldate.tv_nsec = 123456789;
  {
    const char *msg = "Hello, world!";
    r.subject = (char*)malloc(strlen(msg) + 1);
    memcpy(r.subject, msg, strlen(msg) + 1);
  }
  r.sentdate = r.internaldate;
  r.size = 9999;
  r.header_size = 88;
  r.gmtime = r.internaldate;
  r.cache_offset = 42;
  r.last_updated = r.internaldate;
  r.system_flags = 0xA5A5A5A5u;
  r.internal_flags = 0x5A5A5A5Au;
  for (size_t i = 0; i < MAX_USER_FLAGS/32; ++i) r.user_flags[i] = (uint32_t)i;
  r.savedate = r.internaldate;
  r.cache_version = 7;
  memset(&r.guid.guid[0], 0x11, 16);
  r.modseq = 5555;
  r.createdmodseq = 4444;
  r.cid = 7777;
  r.basecid = 6666;
  r.cache_crc = 0xDEADBEEF;

  size_t need = serialise_ir_size(&r);
  char *buf = (char*)malloc(need);
  char *end = serialise_ir(buf, &r);

  struct index_record rr = {0};
  deserialise_ir(buf, &rr);

  // Validate round-trip equality for all fields
  assert(end == buf + need);
  assert(rr.uid == r.uid);
  assert(rr.internaldate.tv_sec == r.internaldate.tv_sec);
  assert(rr.internaldate.tv_nsec == r.internaldate.tv_nsec);
  assert((rr.subject && r.subject && strcmp(rr.subject, r.subject) == 0) || (!rr.subject && !r.subject));
  assert(rr.sentdate.tv_sec == r.sentdate.tv_sec);
  assert(rr.sentdate.tv_nsec == r.sentdate.tv_nsec);
  assert(rr.size == r.size);
  assert(rr.header_size == r.header_size);
  assert(rr.gmtime.tv_sec == r.gmtime.tv_sec);
  assert(rr.gmtime.tv_nsec == r.gmtime.tv_nsec);
  assert(rr.cache_offset == r.cache_offset);
  assert(rr.last_updated.tv_sec == r.last_updated.tv_sec);
  assert(rr.last_updated.tv_nsec == r.last_updated.tv_nsec);
  assert(rr.system_flags == r.system_flags);
  assert(rr.internal_flags == r.internal_flags);
  for (size_t i = 0; i < MAX_USER_FLAGS/32; ++i) assert(rr.user_flags[i] == r.user_flags[i]);
  assert(rr.savedate.tv_sec == r.savedate.tv_sec);
  assert(rr.savedate.tv_nsec == r.savedate.tv_nsec);
  assert(rr.cache_version == r.cache_version);
  assert(memcmp(rr.guid.guid, r.guid.guid, 16) == 0);
  assert(rr.modseq == r.modseq);
  assert(rr.createdmodseq == r.createdmodseq);
  assert(rr.cid == r.cid);
  assert(rr.basecid == r.basecid);
  assert(rr.cache_crc == r.cache_crc);

  // Test the flags-only serializer/deserializer
  size_t flags_need = serialise_flags_size(&r);
  size_t flags_expected = 4u + 4u + (MAX_USER_FLAGS/32u) * 4u;
  assert(flags_need == flags_expected);
  char *fbuf = (char*)malloc(flags_need);
  char *fend = serialise_flags(fbuf, &r);
  assert(fend == fbuf + flags_need);
  struct index_record rr2; memset(&rr2, 0, sizeof(rr2));
  // Initialize to different values to ensure decode overwrites
  rr2.system_flags = 0xFFFFFFFFu;
  rr2.internal_flags = 0xFFFFFFFFu;
  for (size_t i = 0; i < MAX_USER_FLAGS/32; ++i) rr2.user_flags[i] = 0xFFFFFFFFu;
  deserialise_flags(fbuf, &rr2);
  assert(rr2.system_flags == r.system_flags);
  assert(rr2.internal_flags == r.internal_flags);
  for (size_t i = 0; i < MAX_USER_FLAGS/32; ++i) assert(rr2.user_flags[i] == r.user_flags[i]);

  // Additional test: deserialise flags into an all-zero record; all other fields remain zero
  struct index_record rr3; memset(&rr3, 0, sizeof(rr3));
  char *fbuf2 = (char*)malloc(flags_need);
  memset(fbuf2, 0, flags_need);
  memcpy(fbuf2, fbuf, flags_need);
  deserialise_flags(fbuf2, &rr3);
  // Flags match
  assert(rr3.system_flags == r.system_flags);
  assert(rr3.internal_flags == r.internal_flags);
  for (size_t i = 0; i < MAX_USER_FLAGS/32; ++i) assert(rr3.user_flags[i] == r.user_flags[i]);
  // Non-flag fields all zero/NULL
  assert(rr3.uid == 0);
  assert(rr3.internaldate.tv_sec == 0 && rr3.internaldate.tv_nsec == 0);
  assert(rr3.subject == NULL);
  assert(rr3.sentdate.tv_sec == 0 && rr3.sentdate.tv_nsec == 0);
  assert(rr3.size == 0);
  assert(rr3.header_size == 0);
  assert(rr3.gmtime.tv_sec == 0 && rr3.gmtime.tv_nsec == 0);
  assert(rr3.cache_offset == 0);
  assert(rr3.last_updated.tv_sec == 0 && rr3.last_updated.tv_nsec == 0);
  assert(rr3.savedate.tv_sec == 0 && rr3.savedate.tv_nsec == 0);
  assert(rr3.cache_version == 0);
  { uint8_t z[16] = {0}; assert(memcmp(rr3.guid.guid, z, 16) == 0); }
  assert(rr3.modseq == 0);
  assert(rr3.createdmodseq == 0);
  assert(rr3.cid == 0);
  assert(rr3.basecid == 0);
  assert(rr3.cache_crc == 0);

  printf("uid=%u subj=%s size=%llu bytes=%zu end-delta=%zu\n",
         rr.uid, rr.subject ? rr.subject : "(null)",
         (unsigned long long)rr.size, need, (size_t)(end - buf));

  free(rr.subject);
  free(buf);
  free(fbuf);
  free(fbuf2);
  free(r.subject);
  return 0;
}
