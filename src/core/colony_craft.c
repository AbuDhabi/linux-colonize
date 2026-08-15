#include "core/colony_craft.h"

#include <string.h>

#include "core/colony_production.h"

typedef struct ColonyCraftRecipe {
  const char* needle;
  int in_cargo;
  int out_cargo;
  int craft_profession;
} ColonyCraftRecipe;

static const ColonyCraftRecipe k_recipes[] = {
  {"Rum Distill", COLONIZE_CARGO_SUGAR, COLONIZE_CARGO_RUM, COLONIZE_PROF_DISTILLER},
  {"Tobacconist", COLONIZE_CARGO_TOBACCO, COLONIZE_CARGO_CIGARS, COLONIZE_PROF_TOBACCONIST},
  {"Weaver", COLONIZE_CARGO_COTTON, COLONIZE_CARGO_CLOTH, COLONIZE_PROF_WEAVER},
  {"Textile", COLONIZE_CARGO_COTTON, COLONIZE_CARGO_CLOTH, COLONIZE_PROF_WEAVER},
  {"Fur Trad", COLONIZE_CARGO_FURS, COLONIZE_CARGO_COATS, COLONIZE_PROF_FUR_TRADER},
  {"Fur Fact", COLONIZE_CARGO_FURS, COLONIZE_CARGO_COATS, COLONIZE_PROF_FUR_TRADER},
  {"Blacksmith", COLONIZE_CARGO_ORE, COLONIZE_CARGO_TOOLS, COLONIZE_PROF_BLACKSMITH},
  {"Iron Works", COLONIZE_CARGO_ORE, COLONIZE_CARGO_TOOLS, COLONIZE_PROF_BLACKSMITH},
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
    total_in += colony_prod_manufacturing_input(bname, c->profession, rec->craft_profession);
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
    if (actual_out > 0) {
      colony->cargo_produced_mask |= (uint16_t)(1u << rec->out_cargo);
    }
    if (delta) {
      delta->goods[rec->in_cargo] -= actual_in;
      delta->goods[rec->out_cargo] += actual_out;
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
  int sol_bonus
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

    int actual_in = scratch->stock[rec->in_cargo];
    if (actual_in > total_in) {
      actual_in = total_in;
    }
    if (actual_in <= 0) {
      if (shortfall) {
        shortfall[rec->out_cargo] += total_out;
      }
      continue;
    }

    const int actual_out = total_out * actual_in / total_in;
    if (shortfall && actual_out < total_out) {
      shortfall[rec->out_cargo] += total_out - actual_out;
    }
    scratch->stock[rec->in_cargo] -= actual_in;
    scratch->stock[rec->out_cargo] += actual_out;
    if (delta) {
      delta->goods[rec->in_cargo] -= actual_in;
      delta->goods[rec->out_cargo] += actual_out;
    }
  }
}
