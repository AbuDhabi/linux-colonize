#include "core/mods.h"

static uint32_t s_campaign_mask;

bool mods_on(ModId id) {
  return (unsigned)id < (unsigned)MOD_COUNT && (s_campaign_mask >> id & 1u) != 0;
}

uint32_t mods_campaign_mask(void) {
  return s_campaign_mask;
}

void mods_campaign_set(uint32_t mask) {
  s_campaign_mask = mask & ((1u << MOD_COUNT) - 1u);
}
