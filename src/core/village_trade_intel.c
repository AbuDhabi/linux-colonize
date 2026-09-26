#include "core/village_trade_intel.h"

#include <stdlib.h>
#include <string.h>

/* Well above the DOS settlement cap; a full table just stops learning. */
#define VILLAGE_TRADE_INTEL_CAP 256
#define VILLAGE_TRADE_INTEL_NATIONS 4

typedef struct VillageTradeIntelKnown {
  unsigned char n; /* 0 = unknown */
  signed char goods[VILLAGE_TRADE_INTEL_GOODS];
} VillageTradeIntelKnown;

typedef struct VillageTradeIntelEntry {
  bool used;
  int x;
  int y;
  VillageTradeIntelKnown buys[VILLAGE_TRADE_INTEL_NATIONS];
  VillageTradeIntelKnown sells[VILLAGE_TRADE_INTEL_NATIONS];
  signed char skill[VILLAGE_TRADE_INTEL_NATIONS]; /* -1 = unknown */
} VillageTradeIntelEntry;

static VillageTradeIntelEntry s_intel[VILLAGE_TRADE_INTEL_CAP];

void village_trade_intel_reset(void) {
  memset(s_intel, 0, sizeof(s_intel));
}

static VillageTradeIntelEntry* village_trade_intel_find(int x, int y, bool create) {
  VillageTradeIntelEntry* free_slot = NULL;
  for (int i = 0; i < VILLAGE_TRADE_INTEL_CAP; ++i) {
    VillageTradeIntelEntry* e = &s_intel[i];
    if (e->used) {
      if (e->x == x && e->y == y) {
        return e;
      }
    } else if (!free_slot) {
      free_slot = e;
    }
  }
  if (!create || !free_slot) {
    return NULL;
  }
  memset(free_slot, 0, sizeof(*free_slot));
  free_slot->used = true;
  free_slot->x = x;
  free_slot->y = y;
  memset(free_slot->skill, -1, sizeof(free_slot->skill));
  return free_slot;
}

static void village_trade_intel_store(VillageTradeIntelKnown* k, const int* goods, int n) {
  unsigned char count = 0;
  for (int i = 0; goods && i < n && count < VILLAGE_TRADE_INTEL_GOODS; ++i) {
    if (goods[i] >= 0 && goods[i] < 16) {
      k->goods[count++] = (signed char)goods[i];
    }
  }
  if (count > 0) {
    k->n = count;
  }
}

void village_trade_intel_note_buys(int euro_nation, int x, int y, const int* goods, int n) {
  if (euro_nation < 0 || euro_nation >= VILLAGE_TRADE_INTEL_NATIONS || x < 0 || y < 0) {
    return;
  }
  VillageTradeIntelEntry* e = village_trade_intel_find(x, y, true);
  if (e) {
    village_trade_intel_store(&e->buys[euro_nation], goods, n);
  }
}

void village_trade_intel_note_sells(int euro_nation, int x, int y, const int* goods, int n) {
  if (euro_nation < 0 || euro_nation >= VILLAGE_TRADE_INTEL_NATIONS || x < 0 || y < 0) {
    return;
  }
  VillageTradeIntelEntry* e = village_trade_intel_find(x, y, true);
  if (e) {
    village_trade_intel_store(&e->sells[euro_nation], goods, n);
  }
}

void village_trade_intel_note_skill(int euro_nation, int x, int y, int skill) {
  if (euro_nation < 0 || euro_nation >= VILLAGE_TRADE_INTEL_NATIONS || x < 0 || y < 0 ||
      skill < 0 || skill > 127) {
    return;
  }
  VillageTradeIntelEntry* e = village_trade_intel_find(x, y, true);
  if (e) {
    e->skill[euro_nation] = (signed char)skill;
  }
}

bool village_trade_intel_get_skill(int euro_nation, int x, int y, int* out_skill) {
  if (out_skill) {
    *out_skill = -1;
  }
  if (euro_nation < 0 || euro_nation >= VILLAGE_TRADE_INTEL_NATIONS) {
    return false;
  }
  const VillageTradeIntelEntry* e = village_trade_intel_find(x, y, false);
  if (!e || e->skill[euro_nation] < 0) {
    return false;
  }
  if (out_skill) {
    *out_skill = e->skill[euro_nation];
  }
  return true;
}

bool village_trade_intel_get(
  int euro_nation, int x, int y, int buys[VILLAGE_TRADE_INTEL_GOODS], int* out_buys_n,
  int sells[VILLAGE_TRADE_INTEL_GOODS], int* out_sells_n
) {
  if (out_buys_n) {
    *out_buys_n = 0;
  }
  if (out_sells_n) {
    *out_sells_n = 0;
  }
  if (euro_nation < 0 || euro_nation >= VILLAGE_TRADE_INTEL_NATIONS) {
    return false;
  }
  const VillageTradeIntelEntry* e = village_trade_intel_find(x, y, false);
  if (!e) {
    return false;
  }
  const VillageTradeIntelKnown* b = &e->buys[euro_nation];
  const VillageTradeIntelKnown* s = &e->sells[euro_nation];
  for (int i = 0; i < b->n; ++i) {
    if (buys) {
      buys[i] = b->goods[i];
    }
  }
  for (int i = 0; i < s->n; ++i) {
    if (sells) {
      sells[i] = s->goods[i];
    }
  }
  if (out_buys_n) {
    *out_buys_n = b->n;
  }
  if (out_sells_n) {
    *out_sells_n = s->n;
  }
  return b->n > 0 || s->n > 0;
}

void village_trade_intel_forget_tile(int x, int y) {
  VillageTradeIntelEntry* e = village_trade_intel_find(x, y, false);
  if (e) {
    memset(e, 0, sizeof(*e));
  }
}

/*
 * ---------------------------------------------------------------------
 * Save serialization ('VTIN' chunk of the port extension block)
 * ---------------------------------------------------------------------
 * Payload, little-endian:
 *   uint16 version (2)
 *   uint16 entry_count
 *   entry_count x { int16 x, int16 y,
 *                   4 x { uint8 buys_n, int8 buys[3] },
 *                   4 x { uint8 sells_n, int8 sells[3] },
 *                   4 x int8 skill (-1 = unknown) }
 * Version 1 ends after sells and remains readable so existing port saves
 * keep their trade rows.
 */
#define VILLAGE_TRADE_INTEL_BLOB_VERSION 2u
#define VILLAGE_TRADE_INTEL_BLOB_HEADER 4u
#define VILLAGE_TRADE_INTEL_BLOB_ENTRY_V1 36u
#define VILLAGE_TRADE_INTEL_BLOB_ENTRY 40u

static void intel_put16(uint8_t* p, int v) {
  p[0] = (uint8_t)((unsigned)v & 0xffu);
  p[1] = (uint8_t)(((unsigned)v >> 8) & 0xffu);
}

static int intel_get16(const uint8_t* p) {
  return (int)(int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint8_t* intel_put_known(uint8_t* p, const VillageTradeIntelKnown* k) {
  *p++ = k->n;
  for (int i = 0; i < VILLAGE_TRADE_INTEL_GOODS; ++i) {
    *p++ = (uint8_t)k->goods[i];
  }
  return p;
}

static const uint8_t* intel_get_known(const uint8_t* p, VillageTradeIntelKnown* k) {
  const unsigned char n = *p++;
  k->n = (n <= VILLAGE_TRADE_INTEL_GOODS) ? n : 0;
  for (int i = 0; i < VILLAGE_TRADE_INTEL_GOODS; ++i) {
    const signed char g = (signed char)*p++;
    k->goods[i] = (g >= 0 && g < 16) ? g : 0;
  }
  return p;
}

uint8_t* village_trade_intel_serialize(size_t* out_size) {
  if (out_size) {
    *out_size = 0;
  }
  unsigned count = 0;
  for (int i = 0; i < VILLAGE_TRADE_INTEL_CAP; ++i) {
    if (s_intel[i].used) {
      ++count;
    }
  }
  if (count == 0) {
    return NULL;
  }
  const size_t size = VILLAGE_TRADE_INTEL_BLOB_HEADER + (size_t)count * VILLAGE_TRADE_INTEL_BLOB_ENTRY;
  uint8_t* buf = malloc(size);
  if (!buf) {
    return NULL;
  }
  intel_put16(buf, (int)VILLAGE_TRADE_INTEL_BLOB_VERSION);
  intel_put16(buf + 2, (int)count);
  uint8_t* p = buf + VILLAGE_TRADE_INTEL_BLOB_HEADER;
  for (int i = 0; i < VILLAGE_TRADE_INTEL_CAP; ++i) {
    const VillageTradeIntelEntry* e = &s_intel[i];
    if (!e->used) {
      continue;
    }
    intel_put16(p, e->x);
    intel_put16(p + 2, e->y);
    p += 4;
    for (int n = 0; n < VILLAGE_TRADE_INTEL_NATIONS; ++n) {
      p = intel_put_known(p, &e->buys[n]);
    }
    for (int n = 0; n < VILLAGE_TRADE_INTEL_NATIONS; ++n) {
      p = intel_put_known(p, &e->sells[n]);
    }
    for (int n = 0; n < VILLAGE_TRADE_INTEL_NATIONS; ++n) {
      *p++ = (uint8_t)e->skill[n];
    }
  }
  if (out_size) {
    *out_size = size;
  }
  return buf;
}

void village_trade_intel_deserialize(const uint8_t* data, size_t size) {
  village_trade_intel_reset();
  if (!data || size < VILLAGE_TRADE_INTEL_BLOB_HEADER) {
    return;
  }
  const unsigned version = (unsigned)intel_get16(data);
  if (version != 1u && version != VILLAGE_TRADE_INTEL_BLOB_VERSION) {
    return; /* a newer port's spelling: ignore rather than misread it */
  }
  const int count = intel_get16(data + 2);
  const size_t entry_size = version == 1u ? VILLAGE_TRADE_INTEL_BLOB_ENTRY_V1
                                          : VILLAGE_TRADE_INTEL_BLOB_ENTRY;
  if (count <= 0 ||
      size < VILLAGE_TRADE_INTEL_BLOB_HEADER + (size_t)count * entry_size) {
    return;
  }
  const uint8_t* p = data + VILLAGE_TRADE_INTEL_BLOB_HEADER;
  for (int i = 0; i < count && i < VILLAGE_TRADE_INTEL_CAP; ++i) {
    const int x = intel_get16(p);
    const int y = intel_get16(p + 2);
    p += 4;
    VillageTradeIntelEntry* e = (x >= 0 && y >= 0) ? village_trade_intel_find(x, y, true) : NULL;
    for (int n = 0; n < VILLAGE_TRADE_INTEL_NATIONS; ++n) {
      VillageTradeIntelKnown scratch;
      p = intel_get_known(p, e ? &e->buys[n] : &scratch);
    }
    for (int n = 0; n < VILLAGE_TRADE_INTEL_NATIONS; ++n) {
      VillageTradeIntelKnown scratch;
      p = intel_get_known(p, e ? &e->sells[n] : &scratch);
    }
    if (version >= 2u) {
      for (int n = 0; n < VILLAGE_TRADE_INTEL_NATIONS; ++n) {
        const signed char skill = (signed char)*p++;
        if (e) {
          e->skill[n] = skill >= 0 ? skill : -1;
        }
      }
    }
  }
}
