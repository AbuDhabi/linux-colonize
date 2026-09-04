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
   * FUN_157e_004a: type 0x0b + damaged bit7 → −2. DOS type 0x0b is
   * Artillery (bugs.md: Damaged Artillery is the weaker unit) — the old
   * Privateer-only peel missed it; keep Privateer for naval-resolve parity.
   */
  if ((combat_type_is_privateer(t) || combat_type_is_artillery_name(t->name)) &&
      (u->col1_unknown15 & 0x80u) != 0) {
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

static int combat_woi_active(const ColonizeCol1Save* col1) {
  if (!col1) {
    return 0;
  }
  /*
   * game_options.woi (0x5382&1) alone — unknown46[0] aliases DOS
   * price_group_state word 0 (col1_save.h) and is near-always nonzero on a
   * real DOS-authored save, which made this OR misfire "at war" on every
   * such save regardless of actual WoI state. See ai_king_independence_declared.
   */
  return col1->head.game_options.woi != 0;
}

static int combat_ref_present(const ColonizeCol1Save* col1) {
  if (!col1) {
    return 0;
  }
  /* Same unknown46[1]/price_group_state-word-0 collision as combat_woi_active. */
  return col1->head.game_options.ref_present != 0;
}

/*
 * Crown / REF nation id (DOS DS:0x53d2). Match ai_king_crown_nation: peer of
 * the human Euro slot (0↔1). Cite: king_ref.md; FUN_43f7_0218.
 */
static int combat_crown_nation(const ColonizeCol1Save* col1) {
  if (!col1) {
    return 1;
  }
  for (int i = 0; i < 4; ++i) {
    if (col1->player[i].control == 0) {
      return (i == 0) ? 1 : 0;
    }
  }
  return 1;
}

static int combat_unit_on_colony(
  const ColonizeCombatStrengthCtx* ctx,
  const ColonizeUnit* u
) {
  if (!ctx || !ctx->colonies || !u) {
    return 0;
  }
  return colonies_id_at(ctx->colonies, u->x, u->y) >= 0;
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

  /* A. Own-nation Euro colony on unit tile. */
  if (ctx->colonies && u->nation_id >= 0 && u->nation_id <= 3) {
    const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
    const ColonizeColony* col = colonies_get(ctx->colonies, cid);
    if (col && col->active && col->nation_id == u->nation_id) {
      local_1a = combat_colony_local_1a(ctx->colonies, col, out_flags);
      goto fortify;
    }
  }

  /*
   * B. Native village — DOS FUN_157e_015e village arm (viceroy 8989-9002):
   * local_1a = 2; tribe tech > 1 (Advanced/Civilized) → 4 (8d02|0x10);
   * capital (tribe record +3 bit4) → local_1a <<= 1 (8d02|0x20). The old
   * (probe+1)*2 formula here belonged to the COLONY arm (FUN_157e_0008
   * probes the bound colony's fort tier) — it mislabeled an Aztec village
   * +150% (bugs: "Aztec bonus").
   */
  if (ctx->col1) {
    const ColonizeCol1Tribe* tribe = combat_tribe_at(ctx->col1, u->x, u->y);
    if (tribe) {
      const int nation = (int)tribe->nation_id;
      int tech = 0;
      if (nation >= 4 && nation <= 11) {
        tech = (int)ctx->col1->indian[nation - 4].tech;
      }
      local_1a = 2;
      if (tech > 1) {
        local_1a = 4;
      }
      if (tribe->state.capital) {
        local_1a <<= 1;
      }
      if (out_flags) {
        out_flags->flags |= COMBAT_FLAG_VILLAGE;
        out_flags->village_n = tech;
        if (tribe->state.capital) {
          out_flags->flags2 |= COMBAT_FLAG_VILLAGE_CAPITAL;
        }
      }
      goto fortify;
    }
  }

  /* C. Open terrain (FUN_157e_015e gates → local_1a or 0x8d04 stash). */
  if (ctx->map) {
    const int terr_class = map_dos_terr_class_at(ctx->map, u->x, u->y);
    const int terr_byte = map_dos_terr_found_score_byte(terr_class);
    const int unit_nat = u->nation_id & 0xf;
    const int foe_nat = foe ? (foe->nation_id & 0xf) : 0xf;
    const int woi = combat_woi_active(ctx->col1);

    /*
     * Immediate apply (8d02|0x80): native defender, or Euro foe that is AI /
     * pre-WoI (so normal defender terrain). Cite: viceroy 9015–9021.
     */
    if (unit_nat > 3 ||
        (foe_nat < 4 && (!woi || combat_nation_is_ai(ctx->col1, foe_nat)))) {
      local_1a = terr_byte;
      if (out_flags) {
        out_flags->flags |= COMBAT_FLAG_TERRAIN;
        out_flags->terrain_byte = terr_byte;
      }
      goto fortify;
    }

    /*
     * Else: Euro defender vs native, or vs human Euro under WoI.
     * Village on either tile → still apply to defender. Fortified → deny both
     * (no stash). Otherwise stash into 0x8d04 for attacker formula (ambush).
     */
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
      /*
       * bugs.md: the Rebels-vs-Tories ambush terrain (stash → attacker
       * ×(stash+4)/4) applies ONLY when BOTH units stand outside colonies —
       * attacking from a non-colony tile into a non-colony tile. With a
       * colony on either tile the defender keeps its terrain normally and
       * the attacker gets no ambush (DOS 1b0e gates the WoI tail on the
       * colony lookup, iVar18 < 0).
       */
      if (combat_woi_active(ctx->col1) && ctx->colonies) {
        const int colony_here = colonies_id_at(ctx->colonies, u->x, u->y) >= 0;
        const int colony_foe =
          foe && colonies_id_at(ctx->colonies, foe->x, foe->y) >= 0;
        if (colony_here || colony_foe) {
          apply_now = 1;
          skip_stash = 1;
        }
      }
    } else if (u->orders == UNITS_ORDER_FORTIFIED || u->orders == UNITS_ORDER_FORTIFY) {
      skip_stash = 1;
    }

    if (apply_now) {
      local_1a = terr_byte;
      if (out_flags) {
        out_flags->flags |= COMBAT_FLAG_TERRAIN;
        out_flags->terrain_byte = terr_byte;
      }
    } else if (!skip_stash) {
      /*
       * Stash only — do not set COMBAT_FLAG_TERRAIN on defender (DOS writes
       * 8d00|0x80 for the attacker analysis column). land_engage applies stash.
       */
      if (out_flags) {
        out_flags->terrain_stash = terr_byte;
        out_flags->terrain_byte = terr_byte;
      }
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

int combat_type_is_artillery_name(const char* name) {
  return name && name[0] &&
         (strstr(name, "Artillery") != NULL || strstr(name, "Cannon") != NULL);
}

int combat_type_is_scout_name(const char* name) {
  return name && name[0] && strstr(name, "Scout") != NULL;
}

int combat_unit_is_combat_role(const ColonizeUnitPool* pool, int unit_id) {
  if (!pool) {
    return 0;
  }
  const ColonizeUnitType* t = NULL;
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u || !u->active) {
    return 0;
  }
  t = units_type(pool, u->type_index);
  if (!t) {
    return 0;
  }
  /* FUN_5fef_0000: skip when type.attack (5236) == 0. */
  return t->attack > 0;
}

static int combat_colony_sol_at(
  const ColonizeCombatStrengthCtx* ctx,
  int x,
  int y
) {
  if (!ctx || !ctx->colonies || !ctx->col1) {
    return 0;
  }
  const int cid = colonies_id_at(ctx->colonies, x, y);
  if (cid < 0) {
    return 0;
  }
  const ColonizeColony* c = colonies_get(ctx->colonies, cid);
  if (!c) {
    return 0;
  }
  /* Inline colony_prod_sol_percent to avoid linking colony_production into smokes. */
  if (ctx->col1->colony) {
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* cc = &ctx->col1->colony[i];
      if ((int)cc->x != c->x || (int)cc->y != c->y) {
        continue;
      }
      if (cc->rebel_divisor == 0) {
        break;
      }
      int sol = (int)((cc->rebel_dividend * 100u) / cc->rebel_divisor);
      if (sol < 0) {
        sol = 0;
      }
      sol += founding_fathers_bolivar_sol_bonus(ctx->col1, c->nation_id);
      if (sol > 100) {
        sol = 100;
      }
      return sol;
    }
  }
  if (c->nation_id >= 0 && c->nation_id < 4) {
    int sol = (int)ctx->col1->nation[c->nation_id].liberty_bells_total / 4;
    sol += founding_fathers_bolivar_sol_bonus(ctx->col1, c->nation_id);
    if (sol > 100) {
      sol = 100;
    }
    if (sol < 0) {
      sol = 0;
    }
    return sol;
  }
  return 0;
}

/*
 * FUN_5fef_1b0e peels after 157e base strengths.
 * Cite: viceroy_unpacked.c ~100459–100576 (artillery/ambush/SoL/diff/Scout).
 */
void combat_apply_1b0e_peels(
  const ColonizeCombatStrengthCtx* ctx,
  int attacker_id,
  int defender_id,
  ColonizeCombatEngageResult* io
) {
  if (!ctx || !ctx->units || !io) {
    return;
  }
  const ColonizeUnit* atk = units_get_const(ctx->units, attacker_id);
  const ColonizeUnit* def = units_get_const(ctx->units, defender_id);
  if (!atk || !def) {
    return;
  }
  const ColonizeUnitType* at = units_type(ctx->units, atk->type_index);
  const ColonizeUnitType* dt = units_type(ctx->units, def->type_index);
  if (!at || !dt) {
    return;
  }

  const int atk_nat = atk->nation_id;
  const int def_nat = def->nation_id;
  const int atk_ship = combat_type_is_ship(ctx->units, attacker_id);
  const int def_ship = combat_type_is_ship(ctx->units, defender_id);
  const int on_colony = combat_unit_on_colony(ctx, def);
  /*
   * DOS 1b0e keys the artillery + Spanish-ambush clauses on
   * FUN_281f_06be(dest) = tile settlement bit (layer2&2 — set on Euro colony
   * AND Indian village tiles; euro_unit_act.md road-pair rule "colony 0x02"),
   * NOT on the colony lookup (07be) the WoI clauses use. So artillery
   * attacking a village is NOT "in the open" (bugs: −75% wrongly applied),
   * and the Spanish +50% fires on village tiles.
   */
  const int on_settlement =
    on_colony || (ctx->col1 && combat_tribe_at(ctx->col1, def->x, def->y) != NULL);
  const int land = !atk_ship && !def_ship;

  /* Difficulty: human Euro side strength -= (difficulty - 4). */
  if (ctx->col1) {
    const int diff = (int)ctx->col1->head.difficulty;
    const int adj = diff - 4;
    if (atk_nat >= 0 && atk_nat <= 3 && !combat_nation_is_ai(ctx->col1, atk_nat)) {
      io->atk_strength -= adj;
    }
    if (def_nat >= 0 && def_nat <= 3 && !combat_nation_is_ai(ctx->col1, def_nat)) {
      io->def_strength -= adj;
    }
  }

  /*
   * DOS 1b0e 1f0b (unported until 2026-09-03): land vs land, attacker type
   * attack (5236) > 1 and defender type defense (5235) < 2 → defender
   * strength halved. No 8d00 flag — DOS 636c has no row for it.
   */
  if (land && at->attack > 1 && dt->defense < 2) {
    io->def_strength >>= 1;
  }

  /* Artillery open-field >>2 when the combat tile has no settlement. */
  if (land) {
    const int atk_arty = combat_type_is_artillery_name(at->name);
    const int def_arty = combat_type_is_artillery_name(dt->name);
    /*
     * DOS 1f4b: BOTH clauses read the DEFENDER's orders (asm IMUL of the
     * defender slot in each) — penalty unless (defender Fortify/Fortified
     * AND defender is Euro). The old port tested each side's own orders.
     */
    const int def_fort =
      def->orders == UNITS_ORDER_FORTIFIED || def->orders == UNITS_ORDER_FORTIFY;
    const int def_shielded = def_fort && def_nat <= 3;
    if (!on_settlement) {
      if (atk_arty && !def_shielded) {
        io->atk_strength >>= 2;
        io->atk_flags.flags |= COMBAT_FLAG_ARTILLERY;
        io->atk_flags.flags_hi |= COMBAT_FLAG_ARTILLERY;
      }
      if (def_arty && !def_shielded) {
        io->def_strength >>= 2;
        io->def_flags.flags |= COMBAT_FLAG_ARTILLERY;
        io->def_flags.flags_hi |= COMBAT_FLAG_ARTILLERY;
      }
    } else if (def_arty && atk_nat > 3) {
      /* Artillery defender vs native attacker on settlement → ×2. */
      io->def_strength <<= 1;
      io->def_flags.flags2 |= COMBAT_FLAG_ARTY_COLONY;
    }
    /* Spanish ambush +50% vs Indians on a settlement tile (villages). */
    if (atk_nat == 2 && def_nat > 3 && on_settlement) {
      io->atk_strength += io->atk_strength >> 1;
      io->atk_flags.flags |= COMBAT_FLAG_AMBUSH;
      io->atk_flags.flags_hi |= COMBAT_FLAG_AMBUSH;
    }
  }

  /*
   * WoI popular-support peels (FUN_5fef_1b0e ~100494–100527).
   * Gate: WoI + Euro attacker. Colony tile vs open-field branches differ.
   * Cite: viceroy_unpacked.c; king_ref.md (0x53d2 = crown).
   */
  if (ctx->col1 && combat_woi_active(ctx->col1) && atk_nat >= 0 && atk_nat <= 3 && land) {
    const int crown = combat_crown_nation(ctx->col1);
    const int atk_is_crown = (atk_nat == crown);
    if (!on_colony) {
      /*
       * Open field: crown attacker on land (not ocean) →
       * atk += difficulty * atk / 20.
       */
      if (atk_is_crown && ctx->map && map_tile_is_land(ctx->map, def->x, def->y)) {
        const int diff = (int)ctx->col1->head.difficulty;
        io->atk_strength += (diff * io->atk_strength) / 20;
      }
    } else {
      /* On colony: +50% if crown attacker OR ref_present (0x8d01|0x80). */
      if (atk_is_crown || combat_ref_present(ctx->col1)) {
        io->atk_strength += io->atk_strength >> 1;
        io->atk_flags.flags |= COMBAT_FLAG_REF;
        io->atk_flags.flags_hi |= COMBAT_FLAG_REF;
      }
      int sol = combat_colony_sol_at(ctx, def->x, def->y);
      if (sol < 0) {
        sol = 0;
      }
      if (sol > 100) {
        sol = 100;
      }
      int support = sol;
      if (atk_is_crown) {
        support = 100 - sol; /* Tory share for crown/REF */
        if (support > 0) {
          io->atk_flags.flags2 |= COMBAT_FLAG_TORIES;
        }
      } else if (support > 0) {
        io->atk_flags.flags2 |= COMBAT_FLAG_REBELS;
      }
      if (support > 0) {
        const int add = (support * io->atk_strength) / 100;
        io->atk_strength += add;
        io->atk_flags.sol_percent = support;
      }
    }
  }

  /* Discoverer damper: human attacker vs AI Euro, difficulty 0 → −25%. */
  if (ctx->col1 && ctx->col1->head.difficulty == 0 && atk_nat >= 0 && atk_nat <= 3 &&
      !combat_nation_is_ai(ctx->col1, atk_nat) && def_nat >= 0 && def_nat <= 3 &&
      combat_nation_is_ai(ctx->col1, def_nat)) {
    io->atk_strength -= io->atk_strength >> 2;
  }

  /* Scout vs Artillery: force defender win (Indian scout / human arty thin). */
  if (land && combat_type_is_scout_name(at->name) &&
      combat_type_is_artillery_name(dt->name)) {
    io->force_defender_wins = true;
  }

  if (io->atk_strength < 0) {
    io->atk_strength = 0;
  }
  if (io->def_strength < 0) {
    io->def_strength = 0;
  }
}

void combat_land_engage(
  const ColonizeCombatStrengthCtx* ctx,
  int attacker_id,
  int defender_id,
  ColonizeCombatEngageResult* out
) {
  if (!out) {
    return;
  }
  memset(out, 0, sizeof(*out));
  combat_side_flags_clear(&out->atk_flags);
  combat_side_flags_clear(&out->def_flags);
  out->atk_strength = combat_unit_base_x8(ctx, attacker_id, 1, &out->atk_flags);
  out->def_strength =
    combat_engagement_strength(ctx, defender_id, attacker_id, &out->def_flags);
  /*
   * FUN_5fef_1b0e attacker scale: ((0x8d04 + 4) * 004a >> 2) * 3 >> 1.
   * 8d04 is 015e terrain stash (0 when defender absorbed terrain / colony /
   * village). Always includes the ×3/2 attack factor; stash adds ambush terrain
   * for Indian→Euro and human→AI-Euro under WoI (REF).
   */
  {
    const int stash = out->def_flags.terrain_stash;
    out->atk_strength = ((stash + 4) * out->atk_strength >> 2) * 3 >> 1;
    if (stash > 0) {
      out->atk_flags.flags |= COMBAT_FLAG_TERRAIN;
      out->atk_flags.terrain_byte = stash;
      out->atk_flags.terrain_stash = stash;
    }
  }
  if (out->atk_strength < 0) {
    out->atk_strength = 0;
  }
  if (out->def_strength < 0) {
    out->def_strength = 0;
  }
  combat_apply_1b0e_peels(ctx, attacker_id, defender_id, out);
}

void combat_naval_engage(
  const ColonizeCombatStrengthCtx* ctx,
  int attacker_id,
  int defender_id,
  ColonizeCombatEngageResult* out
) {
  if (!out) {
    return;
  }
  memset(out, 0, sizeof(*out));
  combat_side_flags_clear(&out->atk_flags);
  combat_side_flags_clear(&out->def_flags);
  out->atk_strength = combat_unit_base_x8(ctx, attacker_id, 1, &out->atk_flags);
  out->def_strength = combat_unit_base_x8(ctx, defender_id, 0, &out->def_flags);
  /* bugs.md: attacking ships get the same +50% attack factor land attackers
   * do (the land formula's ×3/2 tail was never applied at sea). */
  out->atk_strength += out->atk_strength >> 1;
  if (out->atk_strength < 0) {
    out->atk_strength = 0;
  }
  if (out->def_strength < 0) {
    out->def_strength = 0;
  }
  /* Naval: difficulty peel only (artillery/ambush/SoL are land). */
  combat_apply_1b0e_peels(ctx, attacker_id, defender_id, out);
}
