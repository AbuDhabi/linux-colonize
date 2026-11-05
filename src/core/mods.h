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
  /* DOS bug fix (dos_bugs.md DB-75): a human Go To whose goal holds a native unit stops next to
   * it and waits instead of attacking it (units_goto_waits_for_native). */
  MOD_GOTO_NATIVE_WAIT = 1,
  /* Gameplay: every built building's NAMES.TXT min population floors voluntary
   * ejects; the colony screen gets an ABANDON button. */
  MOD_POPULATION_FLOOR = 2,
  /* Gameplay: a colony raid may kill a colonist (@RAIDSCALP). */
  MOD_RAID_SCALPS = 3,
  /* Gameplay: losing a Royal unit cuts the tax by its own table row and shows
   * @KINGMERCY (cut content in DOS). */
  MOD_ROYAL_MERCY = 4,
  /* Gameplay: Founding Father weights by year band from NAMES.TXT @MODFATHERS. */
  MOD_LATE_FATHERS = 5,
  /* New feature: the two Town Hall upgrades DOS refuses, with their wider work radius. */
  MOD_TOWN_HALL_TIERS = 6,
  /* Gameplay: human colonies pay the NAMES.TXT building upkeep (@UPKEEP). */
  MOD_BUILDING_UPKEEP = 7,
  /* New feature: the cut King's audience chamber, from the VIEW menu. */
  MOD_KINGS_AUDIENCE = 8,
  /* Gameplay: chiefs name the cause of their tribe's alarm (@PISS0-5). */
  MOD_TRIBE_MOOD = 9,
  /* Gameplay: computer Scouts spy on human colonies (@LOSTTHEIRSCOUTS). */
  MOD_SCOUT_SPYING = 10,
  /* New feature: an AI cancelling its treaty is announced in a Foreign Affairs
   * advisor popup. */
  MOD_TREATY_CANCEL_POPUP = 11,
  /* New feature: VIEW Plan Pioneer and ORDERS Automate for Pioneers (pioneer_plan.h). */
  MOD_PIONEER_PLAN = 12,
  /* New feature: ORDERS Automate on a Scout explores (explore_auto.h). */
  MOD_SCOUT_AUTOEXPLORE = 13,
  /* New feature: ORDERS Automate on a ship explores the ocean (explore_auto.h). */
  MOD_SHIP_AUTOEXPLORE = 14,
  /* Gameplay: a tougher REF (more waves a turn, seize-landings, nearby garrison counted). */
  MOD_HARD_KING = 15,
  /* New feature: seen tiles outside live unit/colony sight get a fog-blue veil. */
  MOD_CURRENT_VISION = 16,
  MOD_COUNT
} ModId;

/* True when `id` is on for the running campaign. */
bool mods_on(ModId id);

uint32_t mods_campaign_mask(void);
void mods_campaign_set(uint32_t mask);

#endif /* COLONIZE_MODS_H */
