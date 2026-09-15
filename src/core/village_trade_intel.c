#include "core/village_trade_intel.h"

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
