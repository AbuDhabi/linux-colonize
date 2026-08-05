#include "core/colony_yield.h"

#include <stddef.h>
#include <string.h>

/*
 * Yield columns match NAMES.TXT @JOB Farmer…Fisherman.
 * Rows match pedia terrain indices (0–7 unforesed, 8–15 forest type,
 * 16–23 forest via &7, 24–28 arctic/ocean/sea/mountains/hills).
 *
 * Town commons (center) dual-produce is calibrated to live Col1 observations;
 * see docs/terrain_yields.md.
 */
static const int k_unforesed[8][COLONIZE_FIELD_JOB_COUNT] = {
  /* Tundra */ {2, 0, 0, 0, 0, 0, 2, 0, 0},
  /* Desert */ {1, 0, 0, 1, 0, 0, 2, 0, 0},
  /* Plains */ {4, 0, 0, 2, 0, 0, 1, 0, 0},
  /* Prairie */ {2, 0, 0, 3, 0, 0, 0, 0, 0},
  /* Grassland */ {2, 0, 3, 0, 0, 0, 0, 0, 0},
  /* Savannah */ {3, 3, 0, 0, 0, 0, 0, 0, 0},
  /* Marsh */ {2, 0, 2, 0, 0, 0, 2, 0, 0},
  /* Swamp */ {2, 2, 0, 0, 0, 0, 2, 0, 0},
};

static const int k_forested[8][COLONIZE_FIELD_JOB_COUNT] = {
  /* Boreal */ {1, 0, 0, 0, 3, 2, 1, 0, 0},
  /* Scrub */ {1, 0, 0, 1, 2, 1, 1, 0, 0},
  /* Mixed */ {2, 0, 0, 1, 3, 3, 0, 0, 0},
  /* Broadleaf */ {1, 0, 0, 1, 2, 2, 0, 0, 0},
  /* Conifer */ {1, 0, 1, 0, 2, 3, 0, 0, 0},
  /* Tropical */ {2, 1, 0, 0, 2, 2, 0, 0, 0},
  /* Wetland */ {1, 0, 1, 0, 2, 2, 1, 0, 0},
  /* Rain */ {1, 1, 0, 0, 1, 2, 1, 0, 0},
};

/* Arctic, Ocean, Sea Lane, Mountains, Hills.
 * Hills Farmer is 2 (Terrain Chart / FreeCol / live Col1); NAMES.TXT lists 1. */
static const int k_other[5][COLONIZE_FIELD_JOB_COUNT] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 3},
  {0, 0, 0, 0, 0, 0, 0, 0, 3},
  {0, 0, 0, 0, 0, 0, 4, 1, 0},
  {2, 0, 0, 0, 0, 0, 4, 0, 0},
};

static const char* k_job_names[COLONIZE_FIELD_JOB_COUNT] = {
  "Farmer",
  "Sugar Planter",
  "Tobacco Planter",
  "Cotton Planter",
  "Fur Trapper",
  "Lumberjack",
  "Ore Miner",
  "Silver Miner",
  "Fisherman",
};

int colony_yield_job_cargo(int field_job) {
  switch (field_job) {
  case COLONIZE_JOB_FARMER:
  case COLONIZE_JOB_FISHERMAN:
    return COLONIZE_CARGO_FOOD;
  case COLONIZE_JOB_SUGAR_PLANTER:
    return COLONIZE_CARGO_SUGAR;
  case COLONIZE_JOB_TOBACCO_PLANTER:
    return COLONIZE_CARGO_TOBACCO;
  case COLONIZE_JOB_COTTON_PLANTER:
    return COLONIZE_CARGO_COTTON;
  case COLONIZE_JOB_FUR_TRAPPER:
    return COLONIZE_CARGO_FURS;
  case COLONIZE_JOB_LUMBERJACK:
    return COLONIZE_CARGO_LUMBER;
  case COLONIZE_JOB_ORE_MINER:
    return COLONIZE_CARGO_ORE;
  case COLONIZE_JOB_SILVER_MINER:
    return COLONIZE_CARGO_SILVER;
  default:
    return -1;
  }
}

const char* colony_yield_job_name(int field_job) {
  if (field_job < 0 || field_job >= COLONIZE_FIELD_JOB_COUNT) {
    return "?";
  }
  return k_job_names[field_job];
}

static int colony_yield_base_for_pedia(int pedia, int field_job) {
  if (field_job < 0 || field_job >= COLONIZE_FIELD_JOB_COUNT) {
    return 0;
  }
  if (pedia >= 0 && pedia <= 7) {
    return k_unforesed[pedia][field_job];
  }
  if (pedia >= 8 && pedia <= 23) {
    return k_forested[pedia & 7][field_job];
  }
  if (pedia >= 24 && pedia <= 28) {
    return k_other[pedia - 24][field_job];
  }
  return 0;
}

/* @RESOURCE index → preferred field job; -1 none. */
static int colony_yield_resource_job(int resource) {
  switch (resource) {
  case 1: /* Oasis */
  case 2: /* Wheat */
    return COLONIZE_JOB_FARMER;
  case 3:
    return COLONIZE_JOB_COTTON_PLANTER;
  case 4:
    return COLONIZE_JOB_TOBACCO_PLANTER;
  case 5:
    return COLONIZE_JOB_SUGAR_PLANTER;
  case 6: /* Minerals */
  case 13: /* Ore Deposit */
    return COLONIZE_JOB_ORE_MINER;
  case 7: /* Fishery */
    return COLONIZE_JOB_FISHERMAN;
  case 8: /* Beaver */
  case 9: /* Game */
    return COLONIZE_JOB_FUR_TRAPPER;
  case 10: /* Prime Timber */
  case 11:
    return COLONIZE_JOB_LUMBERJACK;
  case 12: /* Silver Deposit */
    return COLONIZE_JOB_SILVER_MINER;
  default:
    return -1;
  }
}

static int colony_yield_resource_bonus(int resource) {
  /* NAMES.TXT @RESOURCE values — used as flat yield when job matches on fields. */
  static const int k_bonus[] = {0, 3, 4, 6, 6, 7, 4, 5, 6, 6, 6, 6, 12, 6};
  if (resource < 0 || resource >= (int)(sizeof(k_bonus) / sizeof(k_bonus[0]))) {
    return 0;
  }
  return k_bonus[resource];
}

static bool colony_yield_is_crop_job(int field_job) {
  return field_job == COLONIZE_JOB_FARMER || field_job == COLONIZE_JOB_SUGAR_PLANTER ||
         field_job == COLONIZE_JOB_TOBACCO_PLANTER || field_job == COLONIZE_JOB_COTTON_PLANTER;
}

static bool colony_yield_is_road_job(int field_job) {
  return field_job == COLONIZE_JOB_FUR_TRAPPER || field_job == COLONIZE_JOB_LUMBERJACK ||
         field_job == COLONIZE_JOB_ORE_MINER || field_job == COLONIZE_JOB_SILVER_MINER;
}

/*
 * Minor-river bonus for a field job; major = 2×.
 * Food/crops +1, furs/lumber +2, ore/silver +1 (FreeCol classic / Col1).
 */
static int colony_yield_river_bonus(int field_job, bool major) {
  int base = 0;
  switch (field_job) {
  case COLONIZE_JOB_FARMER:
  case COLONIZE_JOB_SUGAR_PLANTER:
  case COLONIZE_JOB_TOBACCO_PLANTER:
  case COLONIZE_JOB_COTTON_PLANTER:
  case COLONIZE_JOB_ORE_MINER:
  case COLONIZE_JOB_SILVER_MINER:
    base = 1;
    break;
  case COLONIZE_JOB_FUR_TRAPPER:
  case COLONIZE_JOB_LUMBERJACK:
    base = 2;
    break;
  default:
    return 0;
  }
  return major ? (base * 2) : base;
}

static int colony_yield_road_bonus(int field_job) {
  return colony_yield_is_road_job(field_job) ? 1 : 0;
}

/* Road and river do not stack — apply the larger bonus once. */
static int colony_yield_road_or_river_bonus(
  const ColonizeWorldMap* map,
  int x,
  int y,
  int field_job
) {
  const int road = map_tile_has_road(map, x, y) ? colony_yield_road_bonus(field_job) : 0;
  int river = 0;
  if (map_tile_has_river(map, x, y)) {
    river = colony_yield_river_bonus(field_job, map_tile_has_major_river(map, x, y));
  }
  return road > river ? road : river;
}

int colony_yield_for_tile(const ColonizeWorldMap* map, int x, int y, int field_job) {
  if (!map || field_job < 0 || field_job >= COLONIZE_FIELD_JOB_COUNT) {
    return 0;
  }
  const int pedia = map_pedia_terrain_index_at(map, x, y);
  int yield = colony_yield_base_for_pedia(pedia, field_job);
  const int res = map_resource_type_for_yield(map, x, y);
  if (res >= 0 && colony_yield_resource_job(res) == field_job) {
    const int bonus = colony_yield_resource_bonus(res);
    if (bonus > yield) {
      yield = bonus;
    } else {
      yield += 2;
    }
  }
  if (map_tile_is_plowed(map, x, y) && colony_yield_is_crop_job(field_job)) {
    yield += 1;
  }
  yield += colony_yield_road_or_river_bonus(map, x, y, field_job);
  return yield;
}

/*
 * Town-commons food before specials/river/plow:
 * forested → cleared-parent Farmer + 2; else Farmer + 2 (hills Farmer is 2).
 */
static int colony_yield_town_commons_food_base(int pedia) {
  if (pedia >= 8 && pedia <= 23) {
    return colony_yield_base_for_pedia(pedia & 7, COLONIZE_JOB_FARMER) + 2;
  }
  if (pedia >= 0 && pedia <= 7) {
    return colony_yield_base_for_pedia(pedia, COLONIZE_JOB_FARMER) + 2;
  }
  if (pedia == 28) {
    return colony_yield_base_for_pedia(pedia, COLONIZE_JOB_FARMER) + 2;
  }
  return 0;
}

/* Secondary commodity job for the settlement square (not best-of-table). */
static int colony_yield_town_commons_secondary_job(int pedia) {
  if (pedia >= 8 && pedia <= 23) {
    /* Rain forest → sugar; all other forests → furs (not lumber). */
    return ((pedia & 7) == 7) ? COLONIZE_JOB_SUGAR_PLANTER : COLONIZE_JOB_FUR_TRAPPER;
  }
  switch (pedia) {
  case 0: /* Tundra */
  case 1: /* Desert */
  case 28: /* Hills */
    return COLONIZE_JOB_ORE_MINER;
  case 2: /* Plains */
  case 3: /* Prairie */
    return COLONIZE_JOB_COTTON_PLANTER;
  case 4: /* Grassland */
  case 6: /* Marsh */
    return COLONIZE_JOB_TOBACCO_PLANTER;
  case 5: /* Savannah */
  case 7: /* Swamp */
    return COLONIZE_JOB_SUGAR_PLANTER;
  case 27: /* Mountains (not founding terrain) */
    return COLONIZE_JOB_SILVER_MINER;
  default:
    return -1;
  }
}

void colony_yield_town_commons(
  const ColonizeWorldMap* map,
  int x,
  int y,
  ColonizeTownCommonsYield* out
) {
  if (out) {
    memset(out, 0, sizeof(*out));
    out->secondary_job = -1;
    out->secondary_cargo = -1;
  }
  if (!map || !out) {
    return;
  }
  const int pedia = map_pedia_terrain_index_at(map, x, y);
  const int res = map_resource_type_for_yield(map, x, y);
  const bool timber = (res == 10 || res == 11);

  int food = colony_yield_town_commons_food_base(pedia);
  /* Plow applies to commons food on cleared land. */
  if (map_tile_is_plowed(map, x, y) && pedia >= 0 && pedia <= 7) {
    food += 1;
  }
  /* River boosts commons food (natural improvement). */
  if (map_tile_has_river(map, x, y)) {
    food += colony_yield_river_bonus(
      COLONIZE_JOB_FARMER,
      map_tile_has_major_river(map, x, y)
    );
  }
  /* Oasis / Wheat / Game: +2 food on commons (not absolute @RESOURCE). Skip timber. */
  if (!timber && res >= 0) {
    if (res == 1 || res == 2 || res == 9) {
      food += 2;
    }
  }
  out->food = food > 0 ? food : 0;

  const int sec_job = colony_yield_town_commons_secondary_job(pedia);
  if (sec_job < 0) {
    return;
  }
  /* NAMES base + implicit center secondary bump (+1). */
  int sec = colony_yield_base_for_pedia(pedia, sec_job) + 1;
  /* River on secondary; plow ignored. */
  if (map_tile_has_river(map, x, y)) {
    sec += colony_yield_river_bonus(sec_job, map_tile_has_major_river(map, x, y));
  }
  /* Matching special (except Prime Timber): +2 additive on commons. */
  if (!timber && res >= 0 && colony_yield_resource_job(res) == sec_job) {
    sec += 2;
  }
  out->secondary_job = sec_job;
  out->secondary_cargo = colony_yield_job_cargo(sec_job);
  out->secondary_amount = sec > 0 ? sec : 0;
}
