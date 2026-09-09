#include "core/colony_craft.h"

#include <stdio.h>
#include <string.h>

#include "core/colony_production.h"

typedef struct ColonyCraftRecipe {
  const char* needle;
  int in_cargo;
  int out_cargo;
  int craft_profession;
} ColonyCraftRecipe;

/*
 * Recipe table order = the DOS conversion-ledger emission order.
 *
 * DOS FUN_15eb_1f72 (viceroy 12682-12689; asm 15eb:2385-15eb:23e8) closes the
 * per-colony EOT production pass with exactly eight ledger rows, in this order:
 *
 *     FUN_15eb_0b96(0,    food_needed)   Food      (pop*2 + horses)
 *     FUN_15eb_0b96(5,    prod[16])      Lumber    <- hammers (see turn.c)
 *     FUN_15eb_0bd4(6,    0xe)           Ore       -> Tools
 *     FUN_15eb_0bd4(2,    0xa)           Tobacco   -> Cigars
 *     FUN_15eb_0bd4(3,    0xb)           Cotton    -> Cloth
 *     FUN_15eb_0bd4(4,    0xc)           Furs      -> Coats
 *     FUN_15eb_0bd4(1,    9)             Sugar     -> Rum
 *     FUN_15eb_0b96(0xe,  prod[15])      Tools     <- Muskets
 *
 * (The four DS scratch arrays those helpers index are DS-relative: -0x7238 =
 * DS:0x8dc8 gross production, -0x71f6 = DS:0x8e0a demand, -0x71ce = DS:0x8e32
 * production shortfall, -0x71a6 = DS:0x8e5a unmet-after-stock. Each holds 20
 * entries -- the 16 cargos plus hammers(16)/crosses(17)/bells(18), which is why
 * FUN_15eb_1f72's crosses/bells accumulators land on DS:0x8dea/0x8dec and the
 * two literal operands above resolve to prod[muskets]=DS:0x8de6 and
 * prod[hammers]=DS:0x8de8.)
 *
 * Ore->Tools FIRST and Tools->Muskets LAST is load-bearing, not cosmetic:
 * FUN_15eb_0bd4 (viceroy 10159-10182) rescales the raw good's unmet word by 3/2
 * when the factory tier discount applied, and FUN_15eb_0b96 (viceroy 10143-
 * 10156) then does `if (cargo == 0xe && unmet[ore] != 0) tools_prod -=
 * unmet[ore]` -- i.e. the ore shortfall computed by the (6,0xe) row is
 * propagated into the tools supply seen by the (0xe, muskets) row. The three
 * rows between them touch disjoint cargo pairs and are order-free.
 *
 * REFUTED (smell #22): DOS does NOT hold muskets back to last turn's tools.
 * FUN_15eb_0b52 (viceroy 10122-10141) records the tools row as
 * `stock[0xe] + prod[0xe] < demand[0xe]`, with param_4 = colony +0x9a +
 * 0xe*2 (the stored tools) and param_2 = this tick's gross tools production
 * (less the ore shortfall above). Freshly smelted tools ARE spendable by the
 * gunsmith in the same tick, so the ore -> tools -> muskets cascade below is
 * DOS-correct. Same conclusion as the Lumberjack+Carpenter same-turn lumber
 * finding in turn.c's hammers block.
 */
static const ColonyCraftRecipe k_recipes[] = {
  {"Blacksmith", COLONIZE_CARGO_ORE, COLONIZE_CARGO_TOOLS, COLONIZE_PROF_BLACKSMITH},
  {"Iron Works", COLONIZE_CARGO_ORE, COLONIZE_CARGO_TOOLS, COLONIZE_PROF_BLACKSMITH},
  {"Tobacconist", COLONIZE_CARGO_TOBACCO, COLONIZE_CARGO_CIGARS, COLONIZE_PROF_TOBACCONIST},
  {"Cigar Factory", COLONIZE_CARGO_TOBACCO, COLONIZE_CARGO_CIGARS, COLONIZE_PROF_TOBACCONIST},
  {"Weaver", COLONIZE_CARGO_COTTON, COLONIZE_CARGO_CLOTH, COLONIZE_PROF_WEAVER},
  {"Textile", COLONIZE_CARGO_COTTON, COLONIZE_CARGO_CLOTH, COLONIZE_PROF_WEAVER},
  {"Fur Trad", COLONIZE_CARGO_FURS, COLONIZE_CARGO_COATS, COLONIZE_PROF_FUR_TRADER},
  {"Fur Fact", COLONIZE_CARGO_FURS, COLONIZE_CARGO_COATS, COLONIZE_PROF_FUR_TRADER},
  {"Rum Distill", COLONIZE_CARGO_SUGAR, COLONIZE_CARGO_RUM, COLONIZE_PROF_DISTILLER},
  {"Rum Factory", COLONIZE_CARGO_SUGAR, COLONIZE_CARGO_RUM, COLONIZE_PROF_DISTILLER},
  {"Armory", COLONIZE_CARGO_TOOLS, COLONIZE_CARGO_MUSKETS, COLONIZE_PROF_GUNSMITH},
  {"Magazine", COLONIZE_CARGO_TOOLS, COLONIZE_CARGO_MUSKETS, COLONIZE_PROF_GUNSMITH},
  {"Arsenal", COLONIZE_CARGO_TOOLS, COLONIZE_CARGO_MUSKETS, COLONIZE_PROF_GUNSMITH},
};

static int colony_craft_clamp(int v) {
  if (v < 0) {
    return 0;
  }
  if (v > 65535) {
    return 65535;
  }
  return v;
}

static bool colony_craft_name_matches(const char* name, const char* needle) {
  return name && needle && strstr(name, needle) != NULL;
}

static void colony_craft_pair_totals(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonyCraftRecipe* rec,
  int sol_bonus,
  int* out_total_out,
  int* out_total_in
) {
  int total_out = 0;
  int total_in = 0;
  if (!pool || !colony || !rec) {
    if (out_total_out) {
      *out_total_out = 0;
    }
    if (out_total_in) {
      *out_total_in = 0;
    }
    return;
  }
  for (int i = 0; i < colony->colonist_count; ++i) {
    const ColonizeColonist* c = &colony->colonists[i];
    if (!c->active || c->building_type < 0 || c->building_type >= pool->building_type_count) {
      continue;
    }
    const char* bname = pool->building_types[c->building_type].name;
    if (!colony_craft_name_matches(bname, rec->needle)) {
      continue;
    }
    /* sol_bonus folds in before tier/skill math (matches DOS FUN_15eb_1d4c) —
     * pass the signed value through, not just a positive-only bump; a Tory
     * penalty (negative) reduces output here exactly like it already does
     * for field yields in turn.c, not just SoL bonuses increasing it. */
    total_out +=
      colony_prod_manufacturing_output(bname, c->profession, rec->craft_profession, sol_bonus);
    /* sol_bonus folds into input the same way it folds into output —
     * player-confirmed 2026-08-15 (Viceroy): factory tier discount tracks
     * the *actual* SoL-adjusted output, not the flat base rate. See
     * colony_prod_manufacturing_input's header comment. */
    total_in +=
      colony_prod_manufacturing_input(bname, c->profession, rec->craft_profession, sol_bonus);
  }
  if (out_total_out) {
    *out_total_out = total_out;
  }
  if (out_total_in) {
    *out_total_in = total_in;
  }
}

void colony_craft_one_colony(
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  ColonizeColonyProdDelta* delta,
  int sol_bonus
) {
  if (!pool || !colony || !colony->active) {
    return;
  }

  bool done_pair[COLONIZE_CARGO_COUNT][COLONIZE_CARGO_COUNT];
  memset(done_pair, 0, sizeof(done_pair));

  for (size_t r = 0; r < sizeof(k_recipes) / sizeof(k_recipes[0]); ++r) {
    const ColonyCraftRecipe* rec = &k_recipes[r];
    if (rec->in_cargo < 0 || rec->in_cargo >= COLONIZE_CARGO_COUNT || rec->out_cargo < 0 ||
        rec->out_cargo >= COLONIZE_CARGO_COUNT) {
      continue;
    }
    if (done_pair[rec->in_cargo][rec->out_cargo]) {
      continue;
    }

    int total_out = 0;
    int total_in = 0;
    for (size_t r2 = 0; r2 < sizeof(k_recipes) / sizeof(k_recipes[0]); ++r2) {
      const ColonyCraftRecipe* rec2 = &k_recipes[r2];
      if (rec2->in_cargo != rec->in_cargo || rec2->out_cargo != rec->out_cargo) {
        continue;
      }
      int pair_out = 0;
      int pair_in = 0;
      colony_craft_pair_totals(pool, colony, rec2, sol_bonus, &pair_out, &pair_in);
      total_out += pair_out;
      total_in += pair_in;
    }
    done_pair[rec->in_cargo][rec->out_cargo] = true;

    if (total_out <= 0 || total_in <= 0) {
      continue;
    }

    int actual_in = colony->stock[rec->in_cargo];
    if (actual_in > total_in) {
      actual_in = total_in;
    }
    if (actual_in <= 0) {
      continue;
    }

    const int actual_out = total_out * actual_in / total_in;
    colony->stock[rec->in_cargo] -= actual_in;
    colony->stock[rec->out_cargo] =
      colony_craft_clamp(colony->stock[rec->out_cargo] + actual_out);
    if (delta) {
      delta->goods[rec->in_cargo] -= actual_in;
      delta->goods[rec->out_cargo] += actual_out;
    }
  }
}

/* See header: demand[in_cargo] = someone staffed produced a positive
 * tier-scaled input requirement for that recipe this tick (stock not read). */
void colony_craft_demand_mask(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int sol_bonus,
  bool demand[COLONIZE_CARGO_COUNT]
) {
  if (!demand) {
    return;
  }
  memset(demand, 0, sizeof(bool) * COLONIZE_CARGO_COUNT);
  if (!pool || !colony || !colony->active) {
    return;
  }

  bool done_pair[COLONIZE_CARGO_COUNT][COLONIZE_CARGO_COUNT];
  memset(done_pair, 0, sizeof(done_pair));

  for (size_t r = 0; r < sizeof(k_recipes) / sizeof(k_recipes[0]); ++r) {
    const ColonyCraftRecipe* rec = &k_recipes[r];
    if (rec->in_cargo < 0 || rec->in_cargo >= COLONIZE_CARGO_COUNT || rec->out_cargo < 0 ||
        rec->out_cargo >= COLONIZE_CARGO_COUNT) {
      continue;
    }
    if (done_pair[rec->in_cargo][rec->out_cargo]) {
      continue;
    }

    int total_in = 0;
    for (size_t r2 = 0; r2 < sizeof(k_recipes) / sizeof(k_recipes[0]); ++r2) {
      const ColonyCraftRecipe* rec2 = &k_recipes[r2];
      if (rec2->in_cargo != rec->in_cargo || rec2->out_cargo != rec->out_cargo) {
        continue;
      }
      int pair_out = 0;
      int pair_in = 0;
      colony_craft_pair_totals(pool, colony, rec2, sol_bonus, &pair_out, &pair_in);
      total_in += pair_in;
    }
    done_pair[rec->in_cargo][rec->out_cargo] = true;

    if (total_in > 0) {
      demand[rec->in_cargo] = true;
    }
  }
}

/*
 * Preview helper: same recipe pass as colony_craft_one_colony but records shortfalls
 * and does not require mutating the live colony (operates on scratch stock).
 */
void colony_craft_preview(
  const ColonizeColonyPool* pool,
  ColonizeColony* scratch,
  int shortfall[COLONIZE_CARGO_COUNT],
  ColonizeColonyProdDelta* delta,
  int sol_bonus,
  int gross_out[COLONIZE_CARGO_COUNT],
  int capacity_out[COLONIZE_CARGO_COUNT]
) {
  if (!pool || !scratch || !scratch->active) {
    return;
  }
  if (shortfall) {
    memset(shortfall, 0, sizeof(int) * COLONIZE_CARGO_COUNT);
  }
  if (delta) {
    memset(delta, 0, sizeof(*delta));
  }
  if (gross_out) {
    memset(gross_out, 0, sizeof(int) * COLONIZE_CARGO_COUNT);
  }
  if (capacity_out) {
    memset(capacity_out, 0, sizeof(int) * COLONIZE_CARGO_COUNT);
  }

  bool done_pair[COLONIZE_CARGO_COUNT][COLONIZE_CARGO_COUNT];
  memset(done_pair, 0, sizeof(done_pair));

  for (size_t r = 0; r < sizeof(k_recipes) / sizeof(k_recipes[0]); ++r) {
    const ColonyCraftRecipe* rec = &k_recipes[r];
    if (rec->in_cargo < 0 || rec->in_cargo >= COLONIZE_CARGO_COUNT || rec->out_cargo < 0 ||
        rec->out_cargo >= COLONIZE_CARGO_COUNT) {
      continue;
    }
    if (done_pair[rec->in_cargo][rec->out_cargo]) {
      continue;
    }

    int total_out = 0;
    int total_in = 0;
    for (size_t r2 = 0; r2 < sizeof(k_recipes) / sizeof(k_recipes[0]); ++r2) {
      const ColonyCraftRecipe* rec2 = &k_recipes[r2];
      if (rec2->in_cargo != rec->in_cargo || rec2->out_cargo != rec->out_cargo) {
        continue;
      }
      int pair_out = 0;
      int pair_in = 0;
      colony_craft_pair_totals(pool, scratch, rec2, sol_bonus, &pair_out, &pair_in);
      total_out += pair_out;
      total_in += pair_in;
    }
    done_pair[rec->in_cargo][rec->out_cargo] = true;

    if (total_out <= 0 || total_in <= 0) {
      continue;
    }
    if (capacity_out) {
      /* Full worker capacity, uncapped by available stock — see the header
       * comment above. */
      capacity_out[rec->out_cargo] += total_out;
    }

    int actual_in = scratch->stock[rec->in_cargo];
    if (actual_in > total_in) {
      actual_in = total_in;
    }
    if (actual_in <= 0) {
      if (shortfall) {
        shortfall[rec->out_cargo] += total_out;
        /* Symmetric input-side shortfall: the raw good's own row shows the
         * same "wanted but didn't have" indicator, not just the output. */
        shortfall[rec->in_cargo] += total_in;
      }
      continue;
    }

    const int actual_out = total_out * actual_in / total_in;
    if (shortfall && actual_out < total_out) {
      shortfall[rec->out_cargo] += total_out - actual_out;
    }
    if (shortfall && actual_in < total_in) {
      shortfall[rec->in_cargo] += total_in - actual_in;
    }
    scratch->stock[rec->in_cargo] -= actual_in;
    scratch->stock[rec->out_cargo] += actual_out;
    if (delta) {
      delta->goods[rec->in_cargo] -= actual_in;
      delta->goods[rec->out_cargo] += actual_out;
    }
    if (gross_out) {
      gross_out[rec->out_cargo] += actual_out;
    }
  }
}
