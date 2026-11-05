#ifndef COLONIZE_MODS_H
#define COLONIZE_MODS_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Per-campaign gameplay toggles (docs/settings.md "mods"). A new game copies
 * the settings.json defaults into the campaign mask (settings_mods_new_campaign
 * _mask); the save carries it in RTST (runtime_ext.c); a save without that
 * record loads with every toggle off. Nothing changes the mask mid-campaign.
 *
 * Ids are bit numbers in the save: stable and append-only. Never renumber or
 * reuse one; a retired toggle keeps its slot.
 */
typedef enum ModId {
  /* Gameplay: natives beating a loaded wagon train may take its cargo
   * instead of destroying it (units_apply_land_loss_outcome). */
  MOD_WAGON_LOOT = 0,
  /* DOS bug fix: a human Go To whose goal holds a native unit stops next to
   * it and waits instead of attacking it (units_goto_waits_for_native). */
  MOD_GOTO_NATIVE_WAIT = 1,
  MOD_COUNT
} ModId;

/* True when `id` is on for the running campaign. */
bool mods_on(ModId id);

uint32_t mods_campaign_mask(void);
void mods_campaign_set(uint32_t mask);

#endif /* COLONIZE_MODS_H */
