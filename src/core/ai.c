#include "core/ai.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "core/col1_bridge.h"
#include "core/map_gen.h"
#include "core/new_game.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

static const char* k_new_country[4] = {
  "New England", "New France", "New Spain", "New Netherlands"
};
static const char* k_default_leaders[4] = {
  "Walter Raleigh", "Jacques Cartier", "Christopher Columbus", "Michiel De Ruyter"
};

/* TRIBE.TXT section → Col1 nation_id (4..11) via @TRIBES order. */
static const struct {
  const char* section;
  int nation_id;
} k_tribe_txt_nations[] = {
  {"INCA", 4},
  {"AZTEC", 5},
  {"ARAWAK", 6},
  {"IROQUOIS", 7},
  {"CHEROKEE", 8},
  {"APACHE", 9},
  {"SIOUX", 10},
  {"TUPI", 11},
};

typedef struct AiRng {
  uint32_t state;
} AiRng;

static uint32_t ai_rng_next(AiRng* rng) {
  /* Same LCG family as map_gen (approx FUN_281f_04d4). */
  rng->state = rng->state * 1103515245u + 12345u;
  return (rng->state >> 16) & 0x7fffu;
}

static int ai_rng_range(AiRng* rng, int lo, int hi_inclusive) {
  if (hi_inclusive <= lo) {
    return lo;
  }
  return lo + (int)(ai_rng_next(rng) % (uint32_t)(hi_inclusive - lo + 1));
}

static bool ai_unit_in_europe(int x, int y) {
  return x >= 200 || y >= 200;
}

static int ai_type_or(ColonizeUnitPool* units, const char* primary, const char* fallback) {
  int t = units_find_type(units, primary);
  if (t < 0 && fallback) {
    t = units_find_type(units, fallback);
  }
  return t;
}

static void ai_set_nation_identity(
  ColonizeCol1Save* save,
  int nation,
  int control,
  const char* leader,
  const char* country
) {
  if (!save || nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return;
  }
  save->player[nation].control = (uint8_t)control;
  snprintf(
    save->player[nation].name,
    sizeof(save->player[nation].name),
    "%s",
    leader && leader[0] ? leader : k_default_leaders[nation]
  );
  snprintf(
    save->player[nation].country_name,
    sizeof(save->player[nation].country_name),
    "%s",
    country && country[0] ? country : k_new_country[nation]
  );
}

static bool ai_setup_col1_template(const AiNewGameParams* p, char* err, size_t err_size) {
  if (!p || !p->col1 || !p->map || !p->col1_ok) {
    if (err && err_size) {
      snprintf(err, err_size, "ai_setup_col1_template bad args");
    }
    return false;
  }
  if (!col1_bridge_init_template(
        p->col1, (uint16_t)p->map->width, (uint16_t)p->map->height, err, err_size
      )) {
    return false;
  }

  const int human = p->human_nation;
  for (int i = 0; i < (int)COLONIZE_COL1_NATION_COUNT; ++i) {
    const char* leader = (i == human) ? p->leader_name : k_default_leaders[i];
    ai_set_nation_identity(p->col1, i, i == human ? 0 : 1, leader, k_new_country[i]);
    p->col1->head.nation_relation[i] = -1;
    p->col1->nation[i].gold = (i == human) ? 1000u : 0u;
    p->col1->nation[i].current_crosses = (i == human) ? 2u : 0u;
    p->col1->nation[i].needed_crosses = (i == human) ? 9u : 0u;
  }
  p->col1->head.difficulty = (uint8_t)(p->difficulty < 0 ? 0 : (p->difficulty > 4 ? 4 : p->difficulty));
  p->col1->head.year = 1492;
  p->col1->head.autumn = 0;
  p->col1->head.turn = 0;

  /* Seed indian tech from @TRIBES when available. */
  for (int t = 0; t < 8; ++t) {
    p->col1->indian[t].capitol_x = 1;
    p->col1->indian[t].capitol_y = 1;
    p->col1->indian[t].tech = (uint8_t)(t < 2 ? 3 - t : (t < 5 ? 1 : 0));
  }
  if (p->names) {
    const ColonizeMsgSection* tribes = assets_msg_find(p->names, "TRIBES");
    if (tribes) {
      int idx = 0;
      for (int i = 0; i < tribes->line_count && idx < 8; ++i) {
        const char* line = tribes->lines[i];
        if (!line || line[0] == '\0' || line[0] == ';') {
          continue;
        }
        /* Name, short, good, tech, color */
        char a[32], b[32], c[32];
        int tech = 0, color = 0;
        if (sscanf(line, "%31[^,], %31[^,], %31[^,], %d, %d", a, b, c, &tech, &color) >= 4) {
          if (tech < 0) {
            tech = 0;
          }
          if (tech > 3) {
            tech = 3;
          }
          p->col1->indian[idx].tech = (uint8_t)tech;
          idx++;
        }
      }
    }
  }

  if (p->europe) {
    p->europe->gold = 1000;
  }
  *p->col1_ok = true;
  return true;
}

static void ai_pick_landfall(
  const AiNewGameParams* p,
  int nation,
  int avoid_x,
  int avoid_y,
  int* out_x,
  int* out_y
) {
  *out_x = p->human_start_x;
  *out_y = p->human_start_y;
  if (!p->map) {
    return;
  }
  if (p->use_tribe_txt && p->names && p->map_stem && p->map_stem[0]) {
    new_game_scenario_start(p->names, p->map_stem, nation, out_x, out_y);
    return;
  }
  if (!map_gen_pick_start(p->map, nation, avoid_x, avoid_y, 8, out_x, out_y)) {
    *out_x = p->map->width / 2;
    *out_y = p->map->height / 2;
  }
}

static bool ai_spawn_euro_fleet(
  ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  int nation,
  int difficulty,
  int landfall_x,
  int landfall_y
) {
  if (!units || !map || nation < 0 || nation > 3) {
    return false;
  }

  /* Place on eastern high seas at turn 0 so rivals are on the mapboard immediately. */
  int sx = landfall_x;
  int sy = landfall_y;
  if (!units_find_eastern_high_seas_tile(units, map, landfall_y, &sx, &sy)) {
    return false;
  }

  const int ship_id = units_spawn_euro_starter_fleet(
    units, nation, difficulty, sx, sy, landfall_x, landfall_y
  );
  return ship_id >= 0;
}

static int ai_tribe_initial_pop(uint8_t tech) {
  return 3 + 2 * (int)tech;
}

static bool ai_find_land_near(
  const ColonizeWorldMap* map,
  int x,
  int y,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y) {
    return false;
  }
  if (map_tile_is_land(map, x, y)) {
    *out_x = x;
    *out_y = y;
    return true;
  }
  for (int r = 1; r < 12; ++r) {
    for (int dy = -r; dy <= r; ++dy) {
      for (int dx = -r; dx <= r; ++dx) {
        if (abs(dx) != r && abs(dy) != r) {
          continue;
        }
        const int nx = x + dx;
        const int ny = y + dy;
        if (map_tile_is_land(map, nx, ny)) {
          *out_x = nx;
          *out_y = ny;
          return true;
        }
      }
    }
  }
  return false;
}

static bool ai_tile_has_tribe(const ColonizeCol1Tribe* tribes, int count, int x, int y) {
  for (int i = 0; i < count; ++i) {
    if ((int)tribes[i].x == x && (int)tribes[i].y == y) {
      return true;
    }
  }
  return false;
}

static bool ai_append_tribe(
  ColonizeCol1Tribe** tribes,
  int* count,
  int* capacity,
  int x,
  int y,
  int nation_id,
  bool capital,
  uint8_t tech
) {
  if (*count >= AI_TRIBE_CAP_NEW_WORLD) {
    return false;
  }
  if (*count >= *capacity) {
    const int neu = *capacity < 16 ? 16 : *capacity * 2;
    ColonizeCol1Tribe* grown = realloc(*tribes, (size_t)neu * sizeof(ColonizeCol1Tribe));
    if (!grown) {
      return false;
    }
    *tribes = grown;
    *capacity = neu;
  }
  ColonizeCol1Tribe* t = &(*tribes)[*count];
  memset(t, 0, sizeof(*t));
  t->x = (uint8_t)x;
  t->y = (uint8_t)y;
  t->nation_id = (uint8_t)nation_id;
  t->state.capital = capital ? 1 : 0;
  t->population = (uint8_t)ai_tribe_initial_pop(tech);
  t->mission = 0xFF;
  (*count)++;
  return true;
}

static void ai_spawn_brave_near(
  ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  int nation_id,
  int tx,
  int ty,
  AiRng* rng
) {
  const int brave = units_find_type(units, "Braves");
  if (brave < 0 || !units || !map) {
    return;
  }
  /* Prefer an adjacent empty land tile; else stack on village. */
  static const int k_dx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
  static const int k_dy[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  const int start = (int)(ai_rng_next(rng) & 7u);
  for (int i = 0; i < 8; ++i) {
    const int d = (start + i) & 7;
    const int x = tx + k_dx[d];
    const int y = ty + k_dy[d];
    if (!map_tile_is_land(map, x, y)) {
      continue;
    }
    if (units_id_at(units, x, y) >= 0) {
      continue;
    }
    const int id = units_spawn(units, brave, x, y);
    if (id >= 0) {
      ColonizeUnit* u = units_get(units, id);
      if (u) {
        u->nation_id = nation_id;
        u->goto_x = 0xFF;
        u->goto_y = 0xFF;
      }
      return;
    }
  }
  const int id = units_spawn_allow_stack(units, brave, tx, ty);
  if (id >= 0) {
    ColonizeUnit* u = units_get(units, id);
    if (u) {
      u->nation_id = nation_id;
      u->goto_x = 0xFF;
      u->goto_y = 0xFF;
    }
  }
}

static int ai_tribe_txt_nation(const char* section) {
  for (size_t i = 0; i < sizeof(k_tribe_txt_nations) / sizeof(k_tribe_txt_nations[0]); ++i) {
    if (strcasecmp(section, k_tribe_txt_nations[i].section) == 0) {
      return k_tribe_txt_nations[i].nation_id;
    }
  }
  return -1;
}

static bool ai_place_tribes_from_txt(
  const AiNewGameParams* p,
  ColonizeCol1Tribe** tribes,
  int* count,
  int* capacity,
  AiRng* rng
) {
  char path[640];
  if (!dos_compat_normalize_asset_path(p->data_dir, "TRIBE.TXT", path, sizeof(path))) {
    snprintf(path, sizeof(path), "%s/TRIBE.TXT", p->data_dir ? p->data_dir : ".");
  }
  FILE* f = fopen(path, "rb");
  if (!f) {
    diag_warn("ai: cannot open %s", path);
    return false;
  }

  char line[256];
  int cur_nation = -1;
  bool first_of_nation[8];
  memset(first_of_nation, 1, sizeof(first_of_nation));

  while (fgets(line, sizeof(line), f)) {
    char* s = line;
    while (*s == ' ' || *s == '\t') {
      s++;
    }
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ')) {
      s[--n] = '\0';
    }
    if (n == 0 || s[0] == ';') {
      continue;
    }
    if (s[0] == '@') {
      if (strcasecmp(s, "@STOP") == 0) {
        cur_nation = -1;
        continue;
      }
      cur_nation = ai_tribe_txt_nation(s + 1);
      continue;
    }
    if (cur_nation < 4 || cur_nation > 11) {
      continue;
    }
    int x = 0, y = 0;
    if (sscanf(s, "%d,%d", &x, &y) != 2) {
      continue;
    }
    int lx = x, ly = y;
    if (!ai_find_land_near(p->map, x, y, &lx, &ly)) {
      continue;
    }
    if (ai_tile_has_tribe(*tribes, *count, lx, ly)) {
      continue;
    }
    const int indian = cur_nation - 4;
    const bool capital = first_of_nation[indian];
    const uint8_t tech = p->col1->indian[indian].tech;
    if (!ai_append_tribe(tribes, count, capacity, lx, ly, cur_nation, capital, tech)) {
      break;
    }
    if (capital) {
      first_of_nation[indian] = false;
      p->col1->indian[indian].capitol_x = (uint8_t)lx;
      p->col1->indian[indian].capitol_y = (uint8_t)ly;
    }
  }
  fclose(f);
  (void)rng;
  return *count > 0;
}

static bool ai_place_tribes_procedural(
  const AiNewGameParams* p,
  ColonizeCol1Tribe** tribes,
  int* count,
  int* capacity,
  AiRng* rng
) {
  if (!p->map) {
    return false;
  }
  /* Capitals first. */
  for (int indian = 0; indian < 8; ++indian) {
    const int nation = indian + 4;
    bool placed = false;
    for (int attempt = 0; attempt < 800 && !placed; ++attempt) {
      const int x = ai_rng_range(rng, 2, p->map->width - 3);
      const int y = ai_rng_range(rng, 2, p->map->height - 3);
      if (!map_tile_is_land(p->map, x, y)) {
        continue;
      }
      /* Spacing vs existing capitals. */
      bool ok = true;
      for (int i = 0; i < *count; ++i) {
        if (!(*tribes)[i].state.capital) {
          continue;
        }
        const int dx = (int)(*tribes)[i].x - x;
        const int dy = (int)(*tribes)[i].y - y;
        if (dx * dx + dy * dy < 64) {
          ok = false;
          break;
        }
      }
      if (!ok) {
        continue;
      }
      const uint8_t tech = p->col1->indian[indian].tech;
      if (!ai_append_tribe(tribes, count, capacity, x, y, nation, true, tech)) {
        return *count > 0;
      }
      p->col1->indian[indian].capitol_x = (uint8_t)x;
      p->col1->indian[indian].capitol_y = (uint8_t)y;
      placed = true;
    }
  }

  /* Satellites until soft cap (~56–84). */
  const int target = 56 + (int)(ai_rng_next(rng) % 29u);
  int stall = 0;
  while (*count < target && *count < AI_TRIBE_CAP_NEW_WORLD && stall < 2000) {
    stall++;
    const int indian = ai_rng_range(rng, 0, 7);
    const int nation = indian + 4;
    const int cx = p->col1->indian[indian].capitol_x;
    const int cy = p->col1->indian[indian].capitol_y;
    const int x = cx + ai_rng_range(rng, -10, 10);
    const int y = cy + ai_rng_range(rng, -10, 10);
    if (!map_tile_is_land(p->map, x, y) || ai_tile_has_tribe(*tribes, *count, x, y)) {
      continue;
    }
    /* Keep satellites near own capital region. */
    const int dx = x - cx;
    const int dy = y - cy;
    if (dx * dx + dy * dy > 225) {
      continue;
    }
    const uint8_t tech = p->col1->indian[indian].tech;
    if (ai_append_tribe(tribes, count, capacity, x, y, nation, false, tech)) {
      stall = 0;
    }
  }
  return *count > 0;
}

static bool ai_install_tribes(
  const AiNewGameParams* p,
  ColonizeCol1Tribe* tribes,
  int count
) {
  if (!p->col1) {
    return false;
  }
  free(p->col1->tribe);
  p->col1->tribe = NULL;
  p->col1->head.tribe_count = 0;
  if (count <= 0) {
    return true;
  }
  ColonizeCol1Tribe* owned = calloc((size_t)count, sizeof(ColonizeCol1Tribe));
  if (!owned) {
    return false;
  }
  memcpy(owned, tribes, (size_t)count * sizeof(ColonizeCol1Tribe));
  p->col1->tribe = owned;
  p->col1->head.tribe_count = (uint16_t)count;
  p->col1->owned = true;
  return true;
}

static void ai_fix_human_nation_ids(ColonizeUnitPool* units, int human_nation) {
  if (!units) {
    return;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (!u->active) {
      continue;
    }
    /* Only retag units that still carry the default England id from spawn. */
    if (u->nation_id == 0 && human_nation != 0) {
      /* Heuristic: human starter set is the only nation-0 units before AI fleets.
         Call this BEFORE spawning AI fleets, or tag only pre-AI units. */
      u->nation_id = human_nation;
    }
  }
}

bool ai_init_new_game(const AiNewGameParams* params, char* err, size_t err_size) {
  if (!params || !params->col1 || !params->col1_ok || !params->map || !params->units) {
    if (err && err_size) {
      snprintf(err, err_size, "ai_init_new_game bad args");
    }
    return false;
  }
  if (err && err_size) {
    err[0] = '\0';
  }

  if (!ai_setup_col1_template(params, err, err_size)) {
    return false;
  }

  /* Human starters exist; fix nation_id before AI fleets claim other nations. */
  if (params->human_nation != 0) {
    ai_fix_human_nation_ids(params->units, params->human_nation);
  } else {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      if (params->units->units[i].active) {
        params->units->units[i].nation_id = 0;
      }
    }
  }

  AiRng rng;
  rng.state = params->rng_seed ? params->rng_seed : 1u;

  int landfalls[4][2];
  for (int n = 0; n < 4; ++n) {
    if (n == params->human_nation) {
      landfalls[n][0] = params->human_start_x;
      landfalls[n][1] = params->human_start_y;
      continue;
    }
    int ax = params->human_start_x;
    int ay = params->human_start_y;
    /* Space vs already-chosen AI landfalls. */
    for (int prev = 0; prev < n; ++prev) {
      if (prev == params->human_nation) {
        continue;
      }
      ax = landfalls[prev][0];
      ay = landfalls[prev][1];
    }
    ai_pick_landfall(params, n, ax, ay, &landfalls[n][0], &landfalls[n][1]);
    if (!ai_spawn_euro_fleet(
          params->units,
          params->map,
          n,
          params->difficulty,
          landfalls[n][0],
          landfalls[n][1]
        )) {
      diag_warn("ai: failed to spawn fleet for nation %d", n);
    }
  }

  ColonizeCol1Tribe* tribes = NULL;
  int count = 0;
  int capacity = 0;
  bool placed = false;
  if (params->use_tribe_txt && params->data_dir) {
    placed = ai_place_tribes_from_txt(params, &tribes, &count, &capacity, &rng);
  }
  if (!placed) {
    free(tribes);
    tribes = NULL;
    count = 0;
    capacity = 0;
    placed = ai_place_tribes_procedural(params, &tribes, &count, &capacity, &rng);
  }
  if (placed) {
    if (!ai_install_tribes(params, tribes, count)) {
      free(tribes);
      if (err && err_size) {
        snprintf(err, err_size, "oom installing tribes");
      }
      return false;
    }
    for (int i = 0; i < count; ++i) {
      ai_spawn_brave_near(
        params->units,
        params->map,
        (int)tribes[i].nation_id,
        (int)tribes[i].x,
        (int)tribes[i].y,
        &rng
      );
    }
  } else {
    diag_warn("ai: no tribes placed");
  }
  free(tribes);

  diag_info(
    "ai_init_new_game: human=%d tribes=%u units=%d",
    params->human_nation,
    (unsigned)params->col1->head.tribe_count,
    params->units->unit_count
  );
  return true;
}

static int ai_sign(int v) {
  if (v < 0) {
    return -1;
  }
  if (v > 0) {
    return 1;
  }
  return 0;
}

static bool ai_step_toward(
  ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  ColonizeUnit* unit,
  int tx,
  int ty
) {
  if (!units || !map || !unit || unit->moves_left <= 0) {
    return false;
  }
  const int dx = ai_sign(tx - unit->x);
  const int dy = ai_sign(ty - unit->y);
  if (dx == 0 && dy == 0) {
    return false;
  }
  /* Prefer diagonal/cardinal toward target; try alternatives if blocked. */
  const int try_dx[5] = {dx, dx, 0, dx, -dx};
  const int try_dy[5] = {dy, 0, dy, -dy, dy};
  for (int i = 0; i < 5; ++i) {
    if (try_dx[i] == 0 && try_dy[i] == 0) {
      continue;
    }
    if (units_try_move(units, unit->id, map, unit->x + try_dx[i], unit->y + try_dy[i])) {
      return true;
    }
  }
  return false;
}

static void ai_sail_ship(ColonizeTurnContext* ctx, ColonizeUnit* ship) {
  if (!ctx || !ctx->units || !ctx->map || !ship) {
    return;
  }
  if (ship->goto_x < 0 || ship->goto_x >= 255 || ship->goto_y < 0 || ship->goto_y >= 255) {
    return;
  }
  const int gx = ship->goto_x;
  const int gy = ship->goto_y;

  if (ai_unit_in_europe(ship->x, ship->y)) {
    int sx = gx;
    int sy = gy;
    if (!units_find_high_seas_tile(ctx->units, ctx->map, gx, gy, &sx, &sy)) {
      if (!units_find_water_tile(ctx->units, ctx->map, gx, gy, ship->id, &sx, &sy)) {
        return;
      }
    }
    ship->x = sx;
    ship->y = sy;
    for (int i = 0; i < ship->cargo_count; ++i) {
      ColonizeUnit* pax = units_get(ctx->units, ship->cargo_ids[i]);
      if (pax) {
        pax->x = sx;
        pax->y = sy;
      }
    }
  }

  while (ship->moves_left > 0 && (ship->x != gx || ship->y != gy)) {
    if (!ai_step_toward(ctx->units, ctx->map, ship, gx, gy)) {
      break;
    }
  }
}

void ai_euro_nation_turn(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || nation_id < 0 || nation_id >= 4) {
    return;
  }

  /* Tick AI crosses (save-diff COLONY00→01 advances rivals). */
  if (ctx->col1_ok && ctx->col1) {
    ColonizeCol1Nation* nat = &ctx->col1->nation[nation_id];
    if (nat->needed_crosses == 0) {
      nat->needed_crosses = 14;
    }
    unsigned cur = (unsigned)nat->current_crosses + 2u;
    if (cur > 65535u) {
      cur = 65535u;
    }
    nat->current_crosses = (uint16_t)cur;
  }

  if (!ctx->map) {
    return;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    if (!units_is_sea(ctx->units, u->id)) {
      continue;
    }
    if (u->aboard_ship_id >= 0) {
      continue;
    }
    ai_sail_ship(ctx, u);
  }
}

static void ai_grow_villages(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe) {
    return;
  }
  for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
    ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    /* FUN_4d56_152e: accumulate population into unknown28[0]; overflow → pop++. */
    int acc = (int)t->unknown28[0] + (int)t->population;
    if (acc > AI_VILLAGE_GROWTH_THRESHOLD) {
      t->unknown28[0] = 0;
      if (t->population < 15) {
        t->population++;
      }
    } else {
      t->unknown28[0] = (uint8_t)acc;
    }
  }
}

static void ai_wander_braves(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || !ctx->map) {
    return;
  }
  AiRng rng;
  rng.state = (uint32_t)(nation_id * 97u + (ctx->turn_number ? *ctx->turn_number : 1u) * 13u + 7u);
  static const int k_dx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
  static const int k_dy[8] = {0, 1, 1, 1, 0, -1, -1, -1};

  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
      continue;
    }
    if (units_is_sea(ctx->units, u->id)) {
      continue;
    }
    if (u->moves_left <= 0) {
      continue;
    }
    /* ~50% chance to wander one tile. */
    if ((ai_rng_next(&rng) & 1u) == 0u) {
      continue;
    }
    const int d = (int)(ai_rng_next(&rng) & 7u);
    units_try_move(ctx->units, u->id, ctx->map, u->x + k_dx[d], u->y + k_dy[d]);
  }
}

void ai_indian_nation_turn(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || nation_id < 4 || nation_id > 11) {
    return;
  }
  ai_grow_villages(ctx, nation_id);
  ai_wander_braves(ctx, nation_id);
}
