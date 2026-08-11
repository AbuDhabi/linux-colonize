#include "core/combat_strength.h"

#include <string.h>

#include "core/founding_fathers.h"

void combat_side_flags_clear(ColonizeCombatSideFlags* f) {
  if (!f) {
    return;
  }
  memset(f, 0, sizeof(*f));
}

static int combat_ship_holds_occupied(const ColonizeUnit* u) {
  if (!u) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    if (u->hold_goods_amount[i] > 0 && u->hold_goods_amount[i] < 255) {
      ++n;
    }
  }
  return n;
}

static int combat_type_is_soldier_or_dragoon(const ColonizeUnitType* t) {
  if (!t || !t->name[0]) {
    return 0;
  }
  return strstr(t->name, "Soldier") != NULL || strstr(t->name, "Dragoon") != NULL ||
         strstr(t->name, "Continental") != NULL || strstr(t->name, "Cont. Army") != NULL;
}

static int combat_type_is_privateer(const ColonizeUnitType* t) {
  return t && t->name[0] && strstr(t->name, "Privateer") != NULL;
}

static int combat_type_is_ship(const ColonizeUnitPool* pool, int unit_id) {
  return pool && units_is_sea(pool, unit_id);
}

static int combat_nation_is_ai(const ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 0 || nation_id > 3) {
    return 1; /* natives / unknown → treat as AI for terrain gates */
  }
  return col1->player[nation_id].control != 0;
}

static const ColonizeCol1Tribe* combat_tribe_at(const ColonizeCol1Save* col1, int x, int y) {
  if (!col1 || !col1->tribe) {
    return NULL;
  }
  for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
    if ((int)col1->tribe[i].x == x && (int)col1->tribe[i].y == y) {
      return &col1->tribe[i];
    }
  }
  return NULL;
}

/*
 * Thin FUN_157e_0008: count 0..3 settlement defense probes from Indian tech.
 * Cite: FUN_15eb_038e(0..2); indian[].tech.
 */
static int combat_village_probe_count(const ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 4 || nation_id > 11) {
    return 0;
  }
  const int tech = (int)col1->indian[nation_id - 4].tech;
  int n = 0;
  if (tech >= 1) {
    ++n;
  }
  if (tech >= 2) {
    ++n;
  }
  if (tech >= 3) {
    ++n;
  }
  return n;
}

static int combat_colony_local_1a(
  const ColonizeColonyPool* colonies,
  const ColonizeColony* col,
  ColonizeCombatSideFlags* flags
) {
  /* Bare colony → 2; Stockade+ → 4; Fortress doubles (→8). */
  int local_1a = 2;
  if (flags) {
    flags->flags |= COMBAT_FLAG_COLONY;
  }
  const int stockade = colonies_find_building(colonies, "Stockade");
  const int fort = colonies_find_building(colonies, "Fort");
  const int fortress = colonies_find_building(colonies, "Fortress");
  const int has_stockade =
    stockade >= 0 && stockade < COLONIZE_BUILDING_TYPES_MAX && col->has_building[stockade];
  const int has_fort = fort >= 0 && fort < COLONIZE_BUILDING_TYPES_MAX && col->has_building[fort];
  const int has_fortress =
    fortress >= 0 && fortress < COLONIZE_BUILDING_TYPES_MAX && col->has_building[fortress];
  if (has_stockade || has_fort || has_fortress) {
    local_1a = 4;
    if (flags) {
      flags->flags |= COMBAT_FLAG_STOCKADE;
    }
  }
  if (has_fortress) {
    local_1a <<= 1;
    if (flags) {
      flags->flags |= COMBAT_FLAG_FORTRESS;
    }
  }
  return local_1a;
}

int combat_unit_base_x8(
  const ColonizeCombatStrengthCtx* ctx,
  int unit_id,
  int mode,
  ColonizeCombatSideFlags* out_flags
) {
  if (out_flags) {
    combat_side_flags_clear(out_flags);
  }
  if (!ctx || !ctx->units) {
    return 0;
  }
  const ColonizeUnit* u = units_get_const(ctx->units, unit_id);
  if (!u || !u->active) {
    return 0;
  }
  const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
  if (!t) {
    return 0;
  }

  if (out_flags && mode != 0) {
    out_flags->flags |= COMBAT_FLAG_MODE_ATK;
  }

  int local_8 = (mode == 0) ? t->defense : t->attack;
  if (local_8 < 0) {
    local_8 = 0;
  }
  /*
   * FUN_157e_004a: type 0x0b + damaged bit7 → −2. Linux peels Privateer by
   * name (same as prior naval resolve / ai_euro).
   */
  if (combat_type_is_privateer(t) && (u->col1_unknown15 & 0x80u) != 0) {
    local_8 -= 2;
    if (local_8 < 0) {
      local_8 = 0;
    }
  }
  if (out_flags) {
    out_flags->base_combat = local_8;
  }

  int local_4 = local_8 * 8;

  /* Veteran Soldier/Dragoon: profession 0x15 → +50%. */
  if (combat_type_is_soldier_or_dragoon(t) && u->profession == UNITS_JOB_SOLDIER) {
    local_4 = local_4 + (local_4 >> 1);
    if (out_flags) {
      out_flags->flags |= COMBAT_FLAG_VETERAN;
    }
  }

  /* Drake Privateer → +50%. */
  if (combat_type_is_privateer(t) && ctx->col1 &&
      founding_fathers_nation_has(ctx->col1, u->nation_id, FF_FRANCIS_DRAKE)) {
    local_4 = local_4 + (local_4 >> 1);
    if (out_flags) {
      out_flags->flags_hi |= COMBAT_FLAG_DRAKE;
    }
  }

  /* Ship holds occupied subtract after ×8. */
  if (combat_type_is_ship(ctx->units, unit_id)) {
    const int holds = combat_ship_holds_occupied(u);
    if (holds > 0) {
      local_4 -= holds;
      if (out_flags) {
        out_flags->flags |= COMBAT_FLAG_HOLDS;
        out_flags->holds_occupied = holds;
      }
    }
  }

  if (local_4 < 0) {
    local_4 = 0;
  }
  return local_4;
}

int combat_engagement_strength(
  const ColonizeCombatStrengthCtx* ctx,
  int unit_id,
  int foe_id,
  ColonizeCombatSideFlags* out_flags
) {
  if (out_flags) {
    combat_side_flags_clear(out_flags);
  }
  if (!ctx || !ctx->units) {
    return 0;
  }
  const ColonizeUnit* u = units_get_const(ctx->units, unit_id);
  const ColonizeUnit* foe = units_get_const(ctx->units, foe_id);
  if (!u || !u->active) {
    return 0;
  }

  const int base = combat_unit_base_x8(ctx, unit_id, 0, out_flags);
  int local_1a = 0;
  int terr_stashed = 0;

  /* A. Own-nation Euro colony on unit tile. */
  if (ctx->colonies && u->nation_id >= 0 && u->nation_id <= 3) {
    const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
    const ColonizeColony* col = colonies_get(ctx->colonies, cid);
    if (col && col->active && col->nation_id == u->nation_id) {
      local_1a = combat_colony_local_1a(ctx->colonies, col, out_flags);
      goto fortify;
    }
  }

  /* B. Native village. */
  if (ctx->col1) {
    const ColonizeCol1Tribe* tribe = combat_tribe_at(ctx->col1, u->x, u->y);
    if (tribe) {
      const int nation = (int)tribe->nation_id;
      const int n = combat_village_probe_count(ctx->col1, nation >= 4 ? nation : u->nation_id);
      local_1a = (n + 1) * 2;
      if (out_flags) {
        out_flags->flags |= COMBAT_FLAG_VILLAGE;
        out_flags->village_n = n;
      }
      goto fortify;
    }
  }

  /* C. Open terrain (FUN_157e_015e gates). */
  if (ctx->map) {
    const int terr_class = map_dos_terr_class_at(ctx->map, u->x, u->y);
    const int terr_byte = map_dos_terr_found_score_byte(terr_class);
    const int unit_nat = u->nation_id & 0xf;
    const int foe_nat = foe ? (foe->nation_id & 0xf) : 0xf;
    const int woi = (ctx->col1 && ctx->col1->head.game_options.woi) ? 1 : 0;

    if (unit_nat > 3 ||
        (foe_nat < 4 && (!woi || foe_nat > 3 || combat_nation_is_ai(ctx->col1, foe_nat)))) {
      local_1a = terr_byte;
      if (out_flags) {
        out_flags->flags |= COMBAT_FLAG_TERRAIN;
        out_flags->terrain_byte = terr_byte;
      }
      goto fortify;
    }

    int apply_now = 0;
    int skip_stash = 0;
    if (foe_nat < 4 && !combat_nation_is_ai(ctx->col1, foe_nat)) {
      const int village_here = ctx->col1 && combat_tribe_at(ctx->col1, u->x, u->y) != NULL;
      const int village_foe =
        foe && ctx->col1 && combat_tribe_at(ctx->col1, foe->x, foe->y) != NULL;
      if (village_here || village_foe) {
        apply_now = 1;
        skip_stash = 1;
      }
    } else if (u->orders == UNITS_ORDER_FORTIFIED) {
      skip_stash = 1;
    }

    if (apply_now) {
      local_1a = terr_byte;
      if (out_flags) {
        out_flags->flags |= COMBAT_FLAG_TERRAIN;
        out_flags->terrain_byte = terr_byte;
      }
    } else if (!skip_stash) {
      terr_stashed = terr_byte;
      if (out_flags) {
        out_flags->terrain_byte = terr_byte;
        /* Attacker-side stash bit lives on 0x8d00; expose on flags for UI. */
        out_flags->flags |= COMBAT_FLAG_TERRAIN;
      }
      (void)terr_stashed;
    }
  }

fortify:
  /* D. Fortify: orders==6, land unit, local_1a < 5 → +2. */
  if ((u->orders == UNITS_ORDER_FORTIFIED || u->orders == UNITS_ORDER_FORTIFY) &&
      !combat_type_is_ship(ctx->units, unit_id) && local_1a < 5) {
    local_1a += 2;
    if (out_flags) {
      out_flags->flags_hi |= 0x20u;
      out_flags->flags |= COMBAT_FLAG_FORTIFY;
    }
  }

  if (out_flags) {
    out_flags->local_1a = local_1a;
  }
  return (int)(((local_1a + 4) * base) >> 2);
}

int combat_unit_toughness(
  const ColonizeCombatStrengthCtx* ctx,
  int unit_id,
  int foe_id
) {
  /* Always use 015e so colony/village/fortify apply; foe_id may be -1. */
  return combat_engagement_strength(ctx, unit_id, foe_id, NULL);
}
