#ifndef COLONIZE_CORE_EXPLORE_AUTO_H
#define COLONIZE_CORE_EXPLORE_AUTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/world.h"

/*
 * Port-only divergences (settings.json divergences.scout_autoexplore /
 * ship_autoexplore, docs/explore_auto.md): ORDERS Automate on a Scout or a ship
 * explores on its own. Each decision Goes To the best target: for a Scout, on
 * its continent, a seen Lost City Rumour or a native settlement the nation has
 * not visited, else the nearest seen tile next to unexplored ground; for a
 * ship, the nearest seen ocean tile (not lake) next to unexplored ocean.
 * Never a tile holding a foreign unit or colony, nor one next to a unit of a
 * nation at war (natives: the 153e hostile tier), so the order never turns
 * into an attack. Nothing left to explore ends the automation.
 *
 * The automated flag (ColonizeUnit.automated, shared with Pioneer Automate)
 * and the visited-settlement set ride in the 'SAUT' port extension chunk; DOS
 * ignores it and keeps only the unit's real order byte.
 */

/* 'SAUT' (col1_save.h ext chunk tag). */
#define COLONIZE_COL1_EXT_TAG_EXPLORE_AUTO 0x54554153u

typedef struct ColonizeUnit ColonizeUnit;

/* A Scout or a ship: the units this module automates (setting aside). */
bool explore_auto_is_explorer(const ColonizeUnitPool* pool, const ColonizeUnit* u);
/* An explorer whose divergence is on: ORDERS Automate explores with it. */
bool explore_auto_allowed(const ColonizeUnitPool* pool, const ColonizeUnit* u);
void explore_auto_reset(void);

/* An automated unit's Go To raised the settlement entry at (x,y): do not aim there again. */
void explore_auto_note_village(int nation, int x, int y);

/* ORDERS Automate on a Scout or ship: latch it and set the first Go To. False when
 * there is nothing left to explore. */
bool explore_auto_automate(const ColonizeWorld* w, ColonizeUnit* u);

/* One decision for an automated explorer. Nothing left to explore cancels the
 * automation. No-op for a unit that is not an explorer. */
void explore_auto_step(const ColonizeWorld* w, ColonizeUnit* u);

/* 'SAUT' payload; NULL when there is nothing to keep. */
uint8_t* explore_auto_serialize(const ColonizeUnitPool* units, size_t* out_size);
/* Restores the automated explorers and the visited set; NULL/0 resets. */
void explore_auto_deserialize(const uint8_t* data, size_t size, ColonizeUnitPool* units);

#endif
