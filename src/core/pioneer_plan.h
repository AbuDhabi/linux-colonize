#ifndef COLONIZE_CORE_PIONEER_PLAN_H
#define COLONIZE_CORE_PIONEER_PLAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/world.h"

/*
 * Port-only divergence (settings.json divergences.pioneer_plan, docs/pioneer_plan.md):
 * a per-tile plan of terrain improvements the player paints in VIEW Plan Pioneer
 * mode, and the ORDERS Automate order that walks a Pioneer from planned tile to
 * planned tile doing the work with the ordinary Clear/Plow/Road bodies.
 *
 * The plan and the automated units ride in the 'PPLN' port extension chunk; DOS
 * has no field for either and drops both on a re-save.
 */

#define PIONEER_PLAN_CLEAR 0x01u
#define PIONEER_PLAN_PLOW 0x02u
#define PIONEER_PLAN_ROAD 0x04u

/* 'PPLN' (col1_save.h ext chunk tag). */
#define COLONIZE_COL1_EXT_TAG_PIONEER_PLAN 0x4e4c5050u

typedef struct ColonizeUnit ColonizeUnit;

bool pioneer_plan_enabled(void);
void pioneer_plan_reset(void);
uint8_t pioneer_plan_at(int x, int y);

/* Bits `nation` may plan on (x,y) right now: an explored land tile without a
 * foreign settlement; Clear (+Plow after it) on forest, Plow on open unplowed
 * land, Road where there is no road or settlement. Hills/Mountains: Road only. */
uint8_t pioneer_plan_valid_bits(const ColonizeWorld* w, int nation, int x, int y);

/*
 * Plan-mode order on (x,y). PIONEER_PLAN_CLEAR cycles a forest tile through
 * Clear -> Clear+Plow -> nothing; PLOW and ROAD toggle. Returns false when
 * the tile cannot take that plan.
 */
bool pioneer_plan_toggle(const ColonizeWorld* w, int nation, int x, int y, uint8_t bit);

/* Drop the plan bits that are done or impossible on every tile `nation`
 * currently has in sight (unit sight radius, colony radius 1). Tiles out of
 * sight keep their plan until seen again. */
void pioneer_plan_reconcile_seen(const ColonizeWorld* w, int nation);

/* ORDERS Automate: latch the unit (a Pioneer with tools) and take a first step. */
bool pioneer_plan_automate(
  const ColonizeWorld* w, ColonizeUnit* u, AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
);

/*
 * One automation decision for an automated unit that is not mid-job: start the
 * job on its own tile (Road, then Clear, then Plow) through
 * units_pioneer_road_w / units_pioneer_plow_w, or Go To the nearest planned
 * tile on its continent. Out of tools or out of reachable plans cancels the
 * automation. Returns true when a job was started (the unit's turn is spent).
 */
bool pioneer_plan_auto_step(
  const ColonizeWorld* w, ColonizeUnit* u, AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
);

/* 'PPLN' payload; NULL when there is nothing to keep. */
uint8_t* pioneer_plan_serialize(
  const ColonizeWorldMap* map, const ColonizeUnitPool* units, size_t* out_size
);
/* Restores the plan and the automated units; NULL/0 resets. */
void pioneer_plan_deserialize(const uint8_t* data, size_t size, ColonizeUnitPool* units);

#endif
