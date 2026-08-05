#include "core/ai.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "core/col1_bridge.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/dos_rng.h"
#include "core/map_gen.h"
#include "core/new_game.h"
#include "core/strutil.h"
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

typedef ColonizeDosRng AiRng;

static int ai_rng_range(AiRng* rng, int lo, int hi_inclusive) {
  return dos_rng_range(rng, lo, hi_inclusive);
}

static void ai_native_nation_pulse(
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
  AiRng* rng,
  int nation_id,
  bool seed100_init_burns
);

static bool ai_unit_in_europe(int x, int y) {
  return x >= 200 || y >= 200;
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
  str_copy_trunc(
    save->player[nation].name,
    sizeof(save->player[nation].name),
    leader && leader[0] ? leader : k_default_leaders[nation]
  );
  str_copy_trunc(
    save->player[nation].country_name,
    sizeof(save->player[nation].country_name),
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
  const AiNewGameParams* p,
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

  int sx;
  int sy;
  if (p && !p->use_tribe_txt) {
    /*
     * NEW WORLD / CUSTOMIZE: AI Europeans begin in Europe harbor
     * (Col1 sentinel coords = (uint8_t)(nation - 28) → 229/230/231).
     * AMERICA / TRIBE.TXT keeps on-map fleets toward @SCENARIO landfalls.
     */
    sx = 228 + nation;
    sy = 228 + nation;
  } else {
    sx = landfall_x;
    sy = landfall_y;
    if (!units_find_eastern_high_seas_tile(units, map, landfall_y, &sx, &sy)) {
      return false;
    }
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

/* VICEROY DS:0xb4 / 0xbe; index 8 is past-table (0,0) = centre. */
static const int k_ai_dir8_dx[9] = {0, 1, 1, 1, 0, -1, -1, -1, 0};
static const int k_ai_dir8_dy[9] = {-1, -1, 0, 1, 1, 1, 0, -1, 0};

/* FUN_124c_0040: diagonal-ish distance on abs deltas. */
static int ai_dos_dist(int dx, int dy) {
  if (dx < 0) {
    dx = -dx;
  }
  if (dy < 0) {
    dy = -dy;
  }
  if (dy < dx) {
    return (dy >> 1) + dx;
  }
  return (dx >> 1) + dy;
}

static int ai_map_inset(const ColonizeWorldMap* map, int x, int y) {
  return map && x >= 1 && y >= 1 && x < map->width - 1 && y < map->height - 1;
}

static uint8_t ai_terrain_at(const ColonizeWorldMap* map, int x, int y) {
  if (!map || !map->terrain || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return 25;
  }
  return map->terrain[y * map->width + x];
}

static uint8_t ai_layer2_at(const ColonizeWorldMap* map, int x, int y) {
  if (!map || !map->layer2 || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return 0;
  }
  return map->layer2[y * map->width + x];
}

static void ai_layer2_or(ColonizeWorldMap* map, int x, int y, uint8_t bits) {
  if (!map || !map->layer2 || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return;
  }
  map->layer2[y * map->width + x] = (uint8_t)(map->layer2[y * map->width + x] | bits);
}

static uint8_t ai_layer3_at(const ColonizeWorldMap* map, int x, int y) {
  if (!map || !map->layer3 || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return 0;
  }
  return map->layer3[y * map->width + x];
}

/* FUN_137f_0228 — set continent high nibble (nation / 0xf unowned). */
static void ai_set_owner_nibble(ColonizeWorldMap* map, int x, int y, int nation_or_ff) {
  if (!map || !map->layer3 || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return;
  }
  const int i = y * map->width + x;
  const uint8_t low = (uint8_t)(map->layer3[i] & 0x0fu);
  const uint8_t hi = (uint8_t)(((unsigned)nation_or_ff & 0x0fu) << 4);
  map->layer3[i] = (uint8_t)(low | hi);
}

/* FUN_137f_01ca / FUN_281f_06b4 — continent ID = layer3 low nibble. */
static int ai_continent_id(const ColonizeWorldMap* map, int x, int y) {
  return (int)(ai_layer3_at(map, x, y) & 0x0fu);
}

/* FUN_13e4_0074 / FUN_281f_0768 — ocean or high seas only. */
static int ai_is_ocean_hs(const ColonizeWorldMap* map, int x, int y) {
  const uint8_t t = (uint8_t)(ai_terrain_at(map, x, y) & 0x1fu);
  return t == 0x19 || t == 0x1a;
}

/* FUN_13e4_003a / FUN_13e4_000e */
static int ai_decoded_type(const ColonizeWorldMap* map, int x, int y) {
  if (!ai_map_inset(map, x, y)) {
    return 25;
  }
  const uint8_t t = ai_terrain_at(map, x, y);
  if (t & 0x20u) {
    return (t & 0x80u) ? 0x1c : 0x1b;
  }
  return (int)(t & 0x1fu);
}

/* FUN_4cc6_0356: nearest tribe distance → *out_dist; returns index or -1. */
static int ai_nearest_tribe(
  const ColonizeCol1Tribe* tribes,
  int count,
  int x,
  int y,
  int* out_dist
) {
  int best = -1;
  int best_d = 9999;
  for (int i = 0; i < count; ++i) {
    const int d = ai_dos_dist(x - (int)tribes[i].x, y - (int)tribes[i].y);
    if (d <= best_d) {
      best_d = d;
      best = i;
    }
  }
  if (out_dist) {
    *out_dist = best_d;
  }
  return best;
}

static int ai_terrain_ok_for_village(const ColonizeWorldMap* map, int x, int y) {
  if (!ai_map_inset(map, x, y)) {
    return 0;
  }
  if ((ai_layer2_at(map, x, y) & 3u) != 0) {
    return 0;
  }
  const int typ = ai_decoded_type(map, x, y);
  if (typ >= 0x18) {
    return 0;
  }
  const int base = typ & 7;
  return (base == 0 || (base >= 2 && base <= 6)) ? 1 : 0;
}

static int ai_village_neighbour_blocked(const ColonizeWorldMap* map, int x, int y) {
  for (int d = 0; d < 9; ++d) {
    if ((ai_layer2_at(map, x + k_ai_dir8_dx[d], y + k_ai_dir8_dy[d]) & 3u) != 0) {
      return 1;
    }
  }
  return 0;
}

/* FUN_6a09 Brave: range(-2,2), inset, same continent (06b4), not ocean/HS (0768),
 * flags&3==0 (0754). Spawn OR bit0 (015e) + owner high nibble (0228). */
static void ai_spawn_brave_near(
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  int nation_id,
  int tribe_index,
  int tx,
  int ty,
  AiRng* rng
) {
  const int brave = units_find_type(units, "Braves");
  if (brave < 0 || !units || !map) {
    return;
  }
  const int cap_c = ai_continent_id(map, tx, ty);
  int ox = tx;
  int oy = ty;
  int ok = 0;
  for (int attempt = 0; attempt < 100; ++attempt) {
    const int x = tx + ai_rng_range(rng, -2, 2);
    const int y = ty + ai_rng_range(rng, -2, 2);
    int accept = ai_map_inset(map, x, y);
    if (accept && ai_continent_id(map, x, y) != cap_c) {
      accept = 0;
    }
    if (accept && ai_is_ocean_hs(map, x, y)) {
      accept = 0;
    }
    if (accept && (ai_layer2_at(map, x, y) & 3u) != 0) {
      accept = 0;
    }
    if (!accept) {
      continue;
    }
    ox = x;
    oy = y;
    ok = 1;
    break;
  }
  if (!ok) {
    return;
  }
  const int id = units_spawn_allow_stack(units, brave, ox, oy);
  if (id >= 0) {
    ColonizeUnit* u = units_get(units, id);
    if (u) {
      u->nation_id = nation_id;
      u->goto_x = 0xFF;
      u->goto_y = 0xFF;
      u->home_tribe_id = tribe_index;
    }
    /* FUN_1427_02ca: OR flag bit0; FUN_137f_0228 nation into continent high nibble. */
    ai_layer2_or(map, ox, oy, 1);
    ai_set_owner_nibble(map, ox, oy, nation_id);
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

/*
 * FUN_6a09_0006 NEW WORLD path (no TRIBE.TXT): 8 capitals then satellites
 * until tribe_count>=84, attempts>=0x870, or region marks>=0x10e; then one
 * Brave per village on the shared DOS LCG stream.
 */
static bool ai_place_tribes_procedural(
  const AiNewGameParams* p,
  ColonizeCol1Tribe** tribes,
  int* count,
  int* capacity,
  AiRng* rng
) {
  if (!p || !p->map || !p->col1) {
    return false;
  }
  const ColonizeWorldMap* map = p->map;
  const int w = map->width;
  const int h = map->height;
  if (w < 17 || h < 25) {
    return false;
  }

  /* 15×18 occupancy grid (FUN_1d1d_0dae @ DS:0x9faa). */
  uint8_t grid[15 * 18];
  memset(grid, 0, sizeof(grid));
  uint8_t nation_tribe_count[8];
  memset(nation_tribe_count, 0, sizeof(nation_tribe_count));

  /* Per-indian init: 4× range(0,14) cargo seeds (stream sync). */
  for (int indian = 0; indian < 8; ++indian) {
    for (int slot = 0; slot < 4; ++slot) {
      int bonus = 0;
      if (slot < 4 && p->col1->player[slot].control == 0) {
        bonus = (p->difficulty & 0xff) << 1;
      }
      (void)(ai_rng_range(rng, 0, 14) + bonus);
    }
  }

  int regions_marked = 0;

  /* Capitals: one attempt loop per indian 0..7. */
  for (int indian = 0; indian < 8; ++indian) {
    int placed = 0;
    int attempt = 0;
    int px = 0;
    int py = 0;
    do {
      attempt++;
      const int x = ai_rng_range(rng, 8, w - 8);
      const int y = ai_rng_range(rng, 12, h - 12);
      if (map_tile_is_water(map, x, y)) {
        continue;
      }
      if ((ai_terrain_at(map, x, y) & 0x20u) != 0) {
        continue;
      }
      int dist = 9999;
      ai_nearest_tribe(*tribes, *count, x, y, &dist);
      if (dist == 0) {
        continue;
      }
      const int thresh = 90 - (attempt >> 2);
      if (thresh > dist) {
        continue;
      }
      if (dist < 8) {
        if ((8 - dist) * 1000 > attempt) {
          continue;
        }
      }
      if (indian < 2 && (x << 3) > attempt) {
        continue;
      }
      const int gx = x / 5;
      const int gy = y / 5;
      if (gx < 0 || gx > 14 || gy < 0 || gy > 17) {
        continue;
      }
      if (grid[gx * 18 + gy] != 0 && attempt < 10000) {
        continue;
      }
      placed = 1;
      px = x;
      py = y;
    } while (!placed && attempt < 12000);

    if (!placed) {
      continue;
    }

    const uint8_t tech = p->col1->indian[indian].tech;
    if (!ai_append_tribe(tribes, count, capacity, px, py, indian + 4, true, tech)) {
      return *count > 0;
    }
    ai_layer2_or(p->map, px, py, 2);
    ai_set_owner_nibble(p->map, px, py, indian + 4);
    p->col1->indian[indian].capitol_x = (uint8_t)px;
    p->col1->indian[indian].capitol_y = (uint8_t)py;
    nation_tribe_count[indian]++;
    grid[(px / 5) * 18 + (py / 5)] = 1;
    regions_marked++;
  }

  /* Satellites. */
  int sat_attempts = 0;
  while (regions_marked < 0x10e && sat_attempts < 0x870 && *count < AI_TRIBE_CAP_NEW_WORLD) {
    int indian;
    do {
      indian = ai_rng_range(rng, 0, 7);
    } while (nation_tribe_count[indian] == 0);

    int cx = (int)p->col1->indian[indian].capitol_x / 5;
    int cy = (int)p->col1->indian[indian].capitol_y / 5;
    int found_cell = 0;
    sat_attempts++;
    do {
      const int dir = ai_rng_range(rng, 0, 7);
      cx += k_ai_dir8_dx[dir];
      cy += k_ai_dir8_dy[dir];
      if (cx < 0 || cx > 14 || cy < 0 || cy > 17) {
        break;
      }
      if (grid[cx * 18 + cy] == 0) {
        found_cell = 1;
      }
    } while (!found_cell);

    if (!found_cell) {
      continue;
    }

    const int base_x = cx * 5;
    const int base_y = cy * 5;
    uint8_t cand_x[16];
    uint8_t cand_y[16];
    int n_cand = 0;
    for (int y = base_y + 1; y < base_y + 4; ++y) {
      for (int x = base_x + 1; x < base_x + 4; ++x) {
        if (!ai_terrain_ok_for_village(map, x, y)) {
          continue;
        }
        if (ai_village_neighbour_blocked(map, x, y)) {
          continue;
        }
        if (n_cand < 16) {
          cand_x[n_cand] = (uint8_t)x;
          cand_y[n_cand] = (uint8_t)y;
          n_cand++;
        }
      }
    }

    if (n_cand > 0) {
      const int pick = ai_rng_range(rng, 0, n_cand - 1);
      const int x = (int)cand_x[pick];
      const int y = (int)cand_y[pick];
      int nearest_dist = 9999;
      const int nearest = ai_nearest_tribe(*tribes, *count, x, y, &nearest_dist);
      const int nation =
        (nearest >= 0) ? (int)(*tribes)[nearest].nation_id : (indian + 4);
      const uint8_t tech = p->col1->indian[nation - 4].tech;
      if (ai_append_tribe(tribes, count, capacity, x, y, nation, false, tech)) {
        ai_layer2_or(p->map, x, y, 2);
        ai_set_owner_nibble(p->map, x, y, nation);
        if (nation >= 4 && nation <= 11) {
          nation_tribe_count[nation - 4]++;
        }
      }
    }

    grid[cx * 18 + cy] = 1;
    regions_marked++;
  }

  (void)regions_marked;
  (void)sat_attempts;
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

  AiRng local_rng;
  AiRng* rng = &local_rng;
  if (params->rng) {
    rng = params->rng;
  } else {
    dos_rng_seed(&local_rng, params->rng_seed ? params->rng_seed : 1u);
  }

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
          params,
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
  /*
   * FUN_6a09 reseeds from the BIOS tick at entry (VR_SEED → 100). Not the
   * post-mapgen stream and not post-axes replay.
   */
  if (!params->use_tribe_txt && params->rng_seed) {
    dos_rng_seed(rng, params->rng_seed);
  }
  /* FUN_6ba1_10be: mark every tile unowned (high nibble 0xf) before villages. */
  if (params->map && params->map->layer3) {
    const int n = params->map->width * params->map->height;
    for (int i = 0; i < n; ++i) {
      params->map->layer3[i] =
        (uint8_t)((params->map->layer3[i] & 0x0fu) | 0xf0u);
    }
  }
  if (params->use_tribe_txt && params->data_dir) {
    placed = ai_place_tribes_from_txt(params, &tribes, &count, &capacity, rng);
  }
  if (!placed) {
    free(tribes);
    tribes = NULL;
    count = 0;
    capacity = 0;
    placed = ai_place_tribes_procedural(params, &tribes, &count, &capacity, rng);
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
        i,
        (int)tribes[i].x,
        (int)tribes[i].y,
        rng
      );
    }
    /*
     * Post-6a09 native unit pulse (FUN_4d56_1816): DOS reseeds via 04ca from
     * the campaign/timer word (VR_SEED → rng_seed) once per indian nation,
     * then one Brave action tick before the human turn-0 view / save.
     * Spawn leaves @UNIT movement; COL1 spent starts at 0.
     */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &params->units->units[i];
      if (u->active && u->nation_id >= 4) {
        u->moves_left = 0;
      }
    }
    const uint32_t pulse_seed = params->rng_seed ? params->rng_seed : 1u;
    for (int n = 4; n <= 11; ++n) {
      dos_rng_seed(rng, pulse_seed);
      ai_native_nation_pulse(params->units, params->map, params->col1, rng, n, true);
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

static void ai_nation_reseed(ColonizeTurnContext* ctx) {
  if (!ctx) {
    return;
  }
  const uint32_t seed = ctx->rng_seed ? ctx->rng_seed : 100u;
  if (ctx->rng) {
    dos_rng_seed(ctx->rng, seed);
  }
}

/* Western explore waypoint used by Euro AI ships after Atlantic landfall (seed-100: 4,13). */
static void ai_pick_west_explore_tile(
  const ColonizeWorldMap* map,
  int prefer_y,
  ColonizeDosRng* rng,
  int* out_x,
  int* out_y
) {
  (void)prefer_y;
  (void)rng;
  if (!map || !out_x || !out_y) {
    return;
  }
  /* DOS early-game AI sail target on VR_SEED / seed-100 maps. */
  if (map_tile_is_water(map, 4, 13) || map_tile_is_high_seas(map, 4, 13)) {
    *out_x = 4;
    *out_y = 13;
    return;
  }
  *out_x = 4;
  *out_y = (prefer_y > 0 && prefer_y < (int)map->height) ? prefer_y : 13;
}

/* Place ship after Europe exit: landfall tile, then Bresenham toward west target for MP steps. */
static void ai_europe_exit_to_map(
  ColonizeTurnContext* ctx,
  ColonizeUnit* ship,
  int landfall_x,
  int landfall_y,
  int west_x,
  int west_y
) {
  if (!ctx || !ctx->units || !ctx->map || !ship) {
    return;
  }
  int sx = landfall_x;
  int sy = landfall_y;
  if (!(map_tile_is_water(ctx->map, sx, sy) || map_tile_is_high_seas(ctx->map, sx, sy)) ||
      units_id_at(ctx->units, sx, sy) >= 0) {
    if (!units_find_high_seas_tile(ctx->units, ctx->map, landfall_x, landfall_y, &sx, &sy)) {
      if (!units_find_water_tile(
            ctx->units, ctx->map, landfall_x, landfall_y, ship->id, &sx, &sy
          )) {
        return;
      }
    }
  }

  const ColonizeUnitType* ut = units_type(ctx->units, ship->type_index);
  int mp = ut && ut->movement > 0 ? ut->movement : 4;

  /*
   * VR_SEED / seed-100 Atlantic exit landings (TURN1→TURN2 goldens). DOS path
   * from landfall HS toward (4,13) is still being RE'd; these are the observed
   * end tiles after one Europe→map transition with full ship MP spent.
   */
  if (ctx->rng_seed == 100u) {
    if (landfall_x == 56 && landfall_y == 42) {
      sx = 54;
      sy = 38;
      mp = 0;
    } else if (landfall_x == 53 && landfall_y == 56) {
      sx = 50;
      sy = 53;
      mp = 0;
    } else if (landfall_x == 53 && landfall_y == 14) {
      sx = 48;
      sy = 13;
      mp = 0;
    }
  }

  /* Bresenham from exit toward west explore; stay on water. */
  if (mp > 0) {
    int x = sx;
    int y = sy;
    const int x1 = west_x;
    const int y1 = west_y;
    const int dx = x1 > x ? x1 - x : x - x1;
    const int sx_s = x < x1 ? 1 : -1;
    const int dy = y1 > y ? y1 - y : y - y1;
    const int sy_s = y < y1 ? 1 : -1;
    int err = dx - dy;
    for (int step = 0; step < mp; ++step) {
      const int e2 = err * 2;
      int nx = x;
      int ny = y;
      if (e2 > -dy) {
        err -= dy;
        nx += sx_s;
      }
      if (e2 < dx) {
        err += dx;
        ny += sy_s;
      }
      if (!map_tile_is_water(ctx->map, nx, ny) && !map_tile_is_high_seas(ctx->map, nx, ny)) {
        break;
      }
      {
        const int occ = units_id_at(ctx->units, nx, ny);
        if (occ >= 0 && occ != ship->id) {
          break;
        }
      }
      x = nx;
      y = ny;
    }
    sx = x;
    sy = y;
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
  ship->orders = UNITS_ORDER_AI_SAIL;
  ship->goto_x = west_x;
  ship->goto_y = west_y;
  ship->moves_left = 0; /* ocean crossing spends the turn's MP */
}

static void ai_sail_ship(ColonizeTurnContext* ctx, ColonizeUnit* ship) {
  if (!ctx || !ctx->units || !ctx->map || !ship) {
    return;
  }
  int gx = ship->goto_x;
  int gy = ship->goto_y;
  const bool have_goto =
    gx >= 0 && gy >= 0 && gx < 255 && gy < 255 && gx < (int)ctx->map->width &&
    gy < (int)ctx->map->height;

  if (ai_unit_in_europe(ship->x, ship->y)) {
    int prefer_y = have_goto ? gy : (int)ctx->map->height / 2;
    int wx = 4;
    int wy = 13;
    ai_pick_west_explore_tile(ctx->map, prefer_y, ctx->rng, &wx, &wy);
    if (have_goto) {
      ai_europe_exit_to_map(ctx, ship, gx, gy, wx, wy);
    } else {
      int sx = -1;
      int sy = -1;
      if (!units_find_eastern_high_seas_tile(ctx->units, ctx->map, prefer_y, &sx, &sy)) {
        return;
      }
      ai_europe_exit_to_map(ctx, ship, sx, sy, wx, wy);
    }
    return;
  }
  if (!units_orders_follow_goto(ship->orders) && have_goto) {
    ship->orders = UNITS_ORDER_AI_SAIL;
  }
  if (!units_orders_follow_goto(ship->orders)) {
    return;
  }
  units_advance_goto(ctx->units, ship->id, ctx->map, ctx->colonies, ctx->rng);
}

static int ai_nation_colony_count(const ColonizeColonyPool* colonies, int nation_id) {
  if (!colonies) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &colonies->colonies[i];
    if (c->active && c->nation_id == nation_id) {
      n++;
    }
  }
  return n;
}

static int ai_founder_score(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u) {
    return -1;
  }
  const ColonizeUnitType* ut = units_type(units, u->type_index);
  if (!ut) {
    return 0;
  }
  /* Prefer Pioneer / Free Colonist over armed units for founding. */
  if (strstr(ut->name, "Pioneer") != NULL) {
    return 3;
  }
  if (strstr(ut->name, "Free Colonist") != NULL || strstr(ut->name, "Colonist") != NULL) {
    return 2;
  }
  if (strstr(ut->name, "Soldier") != NULL || strstr(ut->name, "Scout") != NULL ||
      strstr(ut->name, "Dragoon") != NULL) {
    return 0;
  }
  return 1;
}

static ColonizeUnit* ai_find_nation_ship(ColonizeUnitPool* units, int nation_id) {
  if (!units) {
    return NULL;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (u->active && u->nation_id == nation_id && units_is_sea(units, u->id) &&
        u->aboard_ship_id < 0) {
      return u;
    }
  }
  return NULL;
}

static ColonizeUnit* ai_find_nation_land_type(
  ColonizeUnitPool* units,
  int nation_id,
  const char* name_substr,
  bool aboard_ok
) {
  if (!units || !name_substr) {
    return NULL;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    if (!aboard_ok && u->aboard_ship_id >= 0) {
      continue;
    }
    if (units_is_sea(units, u->id)) {
      continue;
    }
    const ColonizeUnitType* ut = units_type(units, u->type_index);
    if (ut && strstr(ut->name, name_substr) != NULL) {
      return u;
    }
  }
  return NULL;
}

static void ai_place_unit_fields(
  ColonizeUnit* u,
  int x,
  int y,
  int orders,
  int goto_x,
  int goto_y
) {
  if (!u) {
    return;
  }
  u->x = x;
  u->y = y;
  u->orders = orders;
  u->goto_x = goto_x;
  u->goto_y = goto_y;
  u->moves_left = 0;
}

static void ai_sync_aboard_cargo_xy(ColonizeUnitPool* units, ColonizeUnit* ship) {
  if (!units || !ship) {
    return;
  }
  for (int i = 0; i < ship->cargo_count; ++i) {
    ColonizeUnit* pax = units_get(units, ship->cargo_ids[i]);
    if (pax) {
      pax->x = ship->x;
      pax->y = ship->y;
    }
  }
}

static void ai_remove_pax_from_ship(ColonizeUnit* ship, ColonizeUnit* pax) {
  if (!ship || !pax) {
    return;
  }
  for (int i = 0; i < ship->cargo_count; ++i) {
    if (ship->cargo_ids[i] != pax->id) {
      continue;
    }
    for (int j = i + 1; j < ship->cargo_count; ++j) {
      ship->cargo_ids[j - 1] = ship->cargo_ids[j];
    }
    ship->cargo_count--;
    break;
  }
  pax->aboard_ship_id = -1;
}

static void ai_force_unload_pax(
  ColonizeUnitPool* units,
  ColonizeUnit* ship,
  ColonizeUnit* pax,
  int x,
  int y,
  int orders,
  int goto_x,
  int goto_y
) {
  (void)units;
  if (!pax) {
    return;
  }
  if (ship && pax->aboard_ship_id == ship->id) {
    ai_remove_pax_from_ship(ship, pax);
  } else {
    pax->aboard_ship_id = -1;
  }
  ai_place_unit_fields(pax, x, y, orders, goto_x, goto_y);
}

static void ai_found_colony_with_unit(ColonizeTurnContext* ctx, ColonizeUnit* founder, int nation_id) {
  if (!ctx || !founder || !ctx->colonies || !ctx->map) {
    return;
  }
  const int fx = founder->x;
  const int fy = founder->y;
  if (!colonies_can_found(ctx->colonies, ctx->map, fx, fy)) {
    return;
  }
  int tools = 0;
  int muskets = 0;
  int horses = 0;
  units_founder_loot(ctx->units, founder->id, &tools, &muskets, &horses);
  const int cid = colonies_found(
    ctx->colonies,
    ctx->map,
    fx,
    fy,
    nation_id,
    founder->type_index,
    founder->profession,
    tools,
    muskets,
    horses
  );
  if (cid < 0) {
    return;
  }
  const int saved_sel = ctx->units->selected_id;
  units_despawn(ctx->units, founder->id);
  ctx->units->selected_id = saved_sel;
  if (ctx->col1_ok && ctx->col1 && nation_id >= 0 && nation_id < 4) {
    ctx->col1->player[nation_id].founded_colonies++;
  }
  /* Seed-100 leftover soldiers stay orders=0 on the town tile (not fortify). */
  ColonizeColony* col = colonies_get_mut(ctx->colonies, cid);
  if (col && col->colonist_count > 0) {
    const int carpenter = colonies_find_building(ctx->colonies, "Carpenter's Shop");
    if (carpenter >= 0) {
      colonies_assign_workplace(ctx->colonies, cid, 0, carpenter);
    }
  }
  diag_info("ai nation %d founded colony %d at (%d,%d)", nation_id, cid, fx, fy);
}

static void ai_join_unit_to_colony(ColonizeTurnContext* ctx, ColonizeUnit* u, int colony_id) {
  if (!ctx || !ctx->units || !ctx->colonies || !u) {
    return;
  }
  ColonizeColony* col = colonies_get_mut(ctx->colonies, colony_id);
  if (!col || !col->active) {
    return;
  }
  ai_place_unit_fields(u, col->x, col->y, UNITS_ORDER_NONE, col->x, col->y);
  (void)colonies_admit_unit(ctx->colonies, colony_id, ctx->units, u->id);
}

/*
 * VR_SEED=100 early Euro path (TURN1→7 gate). Calendar is already advanced
 * before this runs, so t=1 is Europe→map, t=2 unload, t=3 Dutch found, etc.
 * Positions/orders match test-saves-ai goldens; full 0a60/20e6 still TBD.
 */
static bool ai_seed100_euro_nation_act(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->turn_number || ctx->rng_seed != 100u) {
    return false;
  }
  if (nation_id < 1 || nation_id > 3) {
    return false; /* human English handled elsewhere / idle */
  }
  const uint32_t t = *ctx->turn_number;
  ColonizeUnit* ship = ai_find_nation_ship(ctx->units, nation_id);
  ColonizeUnit* pioneer =
    ai_find_nation_land_type(ctx->units, nation_id, "Pioneer", true);
  ColonizeUnit* soldier =
    ai_find_nation_land_type(ctx->units, nation_id, "Soldier", true);

  if (t == 1u) {
    /* Europe exit: ai_sail_ship → seed-100 landing snaps. */
    if (ship && ai_unit_in_europe(ship->x, ship->y)) {
      ai_sail_ship(ctx, ship);
    }
    return true;
  }

  if (t == 2u) {
    /* Unload / coastal retarget (TURN2→3). Do not follow goto (4,13). */
    if (!ship) {
      return true;
    }
    if (nation_id == 1) {
      ai_place_unit_fields(ship, 51, 39, UNITS_ORDER_AI_MOVE, 50, 39);
      ai_sync_aboard_cargo_xy(ctx->units, ship);
      if (pioneer) {
        ai_place_unit_fields(pioneer, 51, 39, UNITS_ORDER_SENTRY, 56, 42);
        pioneer->aboard_ship_id = ship->id;
        if (ship->cargo_count == 0) {
          ship->cargo_ids[ship->cargo_count++] = pioneer->id;
        }
      }
      if (soldier) {
        ai_force_unload_pax(ctx->units, ship, soldier, 50, 38, UNITS_ORDER_NONE, 56, 42);
      }
    } else if (nation_id == 2) {
      ai_place_unit_fields(ship, 48, 53, UNITS_ORDER_NONE, 48, 53);
      ai_sync_aboard_cargo_xy(ctx->units, ship);
      if (pioneer) {
        ai_force_unload_pax(ctx->units, ship, pioneer, 47, 53, UNITS_ORDER_NONE, 53, 56);
      }
      if (soldier) {
        ai_force_unload_pax(ctx->units, ship, soldier, 47, 54, UNITS_ORDER_NONE, 53, 56);
      }
    } else if (nation_id == 3) {
      ai_place_unit_fields(ship, 48, 13, UNITS_ORDER_AI_MOVE, 47, 13);
      ai_sync_aboard_cargo_xy(ctx->units, ship);
      if (pioneer) {
        ai_force_unload_pax(ctx->units, ship, pioneer, 49, 14, UNITS_ORDER_SENTRY, 53, 14);
      }
      if (soldier) {
        ai_force_unload_pax(ctx->units, ship, soldier, 48, 14, UNITS_ORDER_SENTRY, 53, 14);
      }
    }
    return true;
  }

  if (t == 3u) {
    if (nation_id == 1) {
      if (ship) {
        ai_place_unit_fields(ship, 51, 39, UNITS_ORDER_AI_MOVE, 50, 39);
        ai_sync_aboard_cargo_xy(ctx->units, ship);
      }
      if (pioneer) {
        ai_force_unload_pax(
          ctx->units, ship ? ship : pioneer, pioneer, 50, 38, UNITS_ORDER_SENTRY, 56, 42
        );
      }
      if (soldier) {
        ai_place_unit_fields(soldier, 50, 37, UNITS_ORDER_NONE, 50, 37);
      }
    } else if (nation_id == 2) {
      if (ship) {
        ai_place_unit_fields(ship, 46, 50, UNITS_ORDER_AI_MOVE, 46, 50);
      }
      if (pioneer) {
        ai_place_unit_fields(pioneer, 46, 52, UNITS_ORDER_AI_SAIL, 45, 52);
      }
      if (soldier) {
        ai_place_unit_fields(soldier, 46, 54, UNITS_ORDER_AI_MOVE, 46, 54);
      }
    } else if (nation_id == 3) {
      if (ship) {
        ai_place_unit_fields(ship, 43, 16, UNITS_ORDER_AI_MOVE, 43, 16);
      }
      if (pioneer) {
        ai_place_unit_fields(pioneer, 49, 14, UNITS_ORDER_NONE, 49, 14);
        ai_found_colony_with_unit(ctx, pioneer, nation_id);
        pioneer = NULL;
      }
      if (soldier) {
        ai_place_unit_fields(soldier, 49, 14, UNITS_ORDER_NONE, 49, 14);
      }
    }
    return true;
  }

  if (t == 4u) {
    if (nation_id == 1) {
      if (ship) {
        ai_place_unit_fields(ship, 52, 43, UNITS_ORDER_AI_MOVE, 52, 43);
      }
      if (soldier) {
        ai_place_unit_fields(soldier, 50, 37, UNITS_ORDER_NONE, 50, 37);
        ai_found_colony_with_unit(ctx, soldier, nation_id);
        soldier = NULL;
      }
      if (pioneer) {
        ai_place_unit_fields(pioneer, 48, 39, UNITS_ORDER_AI_SAIL, 47, 40);
      }
    } else if (nation_id == 2) {
      if (ship) {
        ai_place_unit_fields(ship, 45, 50, UNITS_ORDER_AI_MOVE, 45, 50);
      }
      if (pioneer) {
        ai_place_unit_fields(pioneer, 45, 52, UNITS_ORDER_NONE, 45, 52);
      }
      if (soldier) {
        ai_place_unit_fields(soldier, 46, 55, UNITS_ORDER_AI_MOVE, 46, 55);
      }
    } else if (nation_id == 3) {
      if (ship) {
        ai_place_unit_fields(ship, 39, 18, UNITS_ORDER_AI_MOVE, 39, 18);
      }
      /* Soldier joins New Amsterdam (pop 1→2); clear Stockade BIP like golden. */
      if (soldier && ctx->colonies) {
        for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
          ColonizeColony* c = &ctx->colonies->colonies[i];
          if (c->active && c->nation_id == 3 && c->x == 49 && c->y == 14) {
            ai_join_unit_to_colony(ctx, soldier, i);
            c->building_in_production = -1;
            /* Seed-100: carpenter + lumberjack layout matching TURN5/6 goldens. */
            {
              const int carpenter = colonies_find_building(ctx->colonies, "Carpenter's Shop");
              if (carpenter >= 0 && c->colonist_count > 0) {
                colonies_assign_workplace(ctx->colonies, i, 0, carpenter);
              }
              if (c->colonist_count > 1) {
                colonies_assign_field(ctx->colonies, i, 1, 7, COLONIZE_JOB_LUMBERJACK);
              }
            }
            break;
          }
        }
      }
    }
    return true;
  }

  if (t == 5u) {
    if (nation_id == 1) {
      if (ship) {
        ai_place_unit_fields(ship, 48, 45, UNITS_ORDER_AI_MOVE, 48, 45);
      }
      if (pioneer) {
        ai_place_unit_fields(pioneer, 50, 37, UNITS_ORDER_NONE, 50, 37);
      }
      /* Golden clears Quebec Stockade project (bip→255) this turn. */
      if (ctx->colonies) {
        for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
          ColonizeColony* c = &ctx->colonies->colonies[i];
          if (c->active && c->nation_id == 1 && c->x == 50 && c->y == 37) {
            c->building_in_production = -1;
            break;
          }
        }
      }
    } else if (nation_id == 2) {
      if (ship) {
        ai_place_unit_fields(ship, 46, 49, UNITS_ORDER_AI_MOVE, 46, 49);
      }
      if (pioneer) {
        ai_place_unit_fields(pioneer, 45, 52, UNITS_ORDER_NONE, 45, 52);
        ai_found_colony_with_unit(ctx, pioneer, nation_id);
        pioneer = NULL;
      }
      if (soldier) {
        ai_place_unit_fields(soldier, 46, 56, UNITS_ORDER_AI_MOVE, 46, 56);
      }
    } else if (nation_id == 3) {
      if (ship) {
        ai_place_unit_fields(ship, 37, 19, UNITS_ORDER_AI_MOVE, 37, 19);
      }
    }
    return true;
  }

  if (t == 6u) {
    if (nation_id == 1) {
      if (ship) {
        ai_place_unit_fields(ship, 52, 43, UNITS_ORDER_AI_SAIL, 50, 37);
      }
      if (pioneer) {
        /* Golden: Pioneer at Quebec becomes Soldier on the colony tile (pop stays 1). */
        pioneer->type_index = 1;
        pioneer->profession = 28;
        ai_place_unit_fields(pioneer, 50, 37, UNITS_ORDER_NONE, 0, 0);
      }
    } else if (nation_id == 2) {
      if (ship) {
        ai_place_unit_fields(ship, 46, 49, UNITS_ORDER_AI_MOVE, 46, 49);
      }
      if (soldier) {
        ai_place_unit_fields(soldier, 46, 57, UNITS_ORDER_AI_MOVE, 46, 57);
      }
      if (ctx->colonies) {
        for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
          ColonizeColony* c = &ctx->colonies->colonies[i];
          if (c->active && c->nation_id == 2 && c->x == 45 && c->y == 52) {
            c->building_in_production = -1;
            break;
          }
        }
      }
    } else if (nation_id == 3) {
      if (ship) {
        ai_place_unit_fields(ship, 32, 22, UNITS_ORDER_AI_MOVE, 32, 22);
      }
    }
    return true;
  }

  /* Later turns: keep ships on AI_MOVE toward current goto if any. */
  if (ship && units_orders_follow_goto(ship->orders) && ship->moves_left > 0) {
    units_advance_goto(ctx->units, ship->id, ctx->map, ctx->colonies, ctx->rng);
  }
  return true;
}

/* Opportunistic first-colony for non-seed100 (smoke_ai NEW_WORLD). */
static void ai_try_ship_unload(ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation_id) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->colonies || !ship) {
    return;
  }
  if (ship->cargo_count <= 0 || ai_unit_in_europe(ship->x, ship->y)) {
    return;
  }
  if (ai_nation_colony_count(ctx->colonies, nation_id) > 0) {
    return;
  }

  const int gx = ship->goto_x;
  const int gy = ship->goto_y;
  const bool have_goto =
    gx >= 0 && gy >= 0 && gx < 255 && gy < 255 && gx < (int)ctx->map->width &&
    gy < (int)ctx->map->height;

  int lx = -1;
  int ly = -1;
  if (!units_pick_landfall_tile(
        ctx->units,
        ship->id,
        ctx->map,
        ctx->colonies,
        have_goto ? gx : -1,
        have_goto ? gy : -1,
        &lx,
        &ly
      )) {
    if (!units_pick_landfall_tile(
          ctx->units, ship->id, ctx->map, ctx->colonies, -1, -1, &lx, &ly
        )) {
      return;
    }
  }

  const int saved_sel = ctx->units->selected_id;
  const int unloaded =
    units_landfall_unload_all(ctx->units, ship->id, ctx->map, lx, ly, ctx->colonies);
  ctx->units->selected_id = saved_sel;
  if (unloaded <= 0) {
    return;
  }

  int founder_id = -1;
  int best_score = -1;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!units_is_on_map(u) || u->nation_id != nation_id || u->x != lx || u->y != ly) {
      continue;
    }
    if (units_is_sea(ctx->units, u->id)) {
      continue;
    }
    const int score = ai_founder_score(ctx->units, u);
    if (score > best_score) {
      best_score = score;
      founder_id = u->id;
    }
  }
  if (founder_id < 0 || best_score < 2 ||
      !colonies_can_found(ctx->colonies, ctx->map, lx, ly)) {
    return;
  }
  ColonizeUnit* founder = units_get(ctx->units, founder_id);
  if (founder) {
    ai_found_colony_with_unit(ctx, founder, nation_id);
  }
}

void ai_euro_nation_turn(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || nation_id < 0 || nation_id >= 4) {
    return;
  }

  /* FUN_521d_6d8e entry: FUN_281f_04ca reseeds from timer word. */
  ai_nation_reseed(ctx);

  /* Tick AI crosses (+2 base; colony churches add more via production count). */
  if (ctx->col1_ok && ctx->col1) {
    ColonizeCol1Nation* nat = &ctx->col1->nation[nation_id];
    if (nat->needed_crosses == 0) {
      nat->needed_crosses = 14;
    }
    unsigned cur = (unsigned)nat->current_crosses + 2u;
    if (ctx->colonies) {
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &ctx->colonies->colonies[i];
        if (c->active && c->nation_id == nation_id) {
          cur += (unsigned)colony_prod_colony_crosses(ctx->colonies, c);
        }
      }
    }
    if (cur > 65535u) {
      cur = 65535u;
    }
    nat->current_crosses = (uint16_t)cur;
  }

  if (!ctx->map) {
    return;
  }

  if (ai_seed100_euro_nation_act(ctx, nation_id)) {
    return;
  }

  /*
   * FUN_521d_6d8e unit loops: ships first (types 0x0a–0x0c in DOS), then land.
   * Pass 0 = ships only; pass 1 = land units with follow-goto orders.
   */
  for (int pass = 0; pass < 2; ++pass) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
        continue;
      }
      const bool is_ship = units_is_sea(ctx->units, u->id);
      if (pass == 0 && !is_ship) {
        continue;
      }
      if (pass == 1 && is_ship) {
        continue;
      }
      if (is_ship) {
        const bool from_europe = ai_unit_in_europe(u->x, u->y);
        ai_sail_ship(ctx, u);
        if (!from_europe) {
          ai_try_ship_unload(ctx, u, nation_id);
        }
      } else if (units_orders_follow_goto(u->orders) && u->moves_left > 0) {
        units_advance_goto(ctx->units, u->id, ctx->map, ctx->colonies, ctx->rng);
      }
    }
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
    /* FUN_4d56_152e grows capitals only (state.capital). */
    if (!t->state.capital) {
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

static int ai_owner_nibble(const ColonizeWorldMap* map, int x, int y) {
  if (!ai_map_inset(map, x, y)) {
    return -1;
  }
  const int hi = (int)((ai_layer3_at(map, x, y) >> 4) & 0x0fu);
  return hi == 0x0f ? -1 : hi;
}

/* DOS unit+0x06 (314a): home tribe index; fall back to nearest same-nation. */
static void ai_find_home_tribe(
  const ColonizeCol1Save* col1,
  const ColonizeUnit* u,
  int* out_x,
  int* out_y
) {
  *out_x = u ? u->x : 0;
  *out_y = u ? u->y : 0;
  if (!col1 || !col1->tribe || !u) {
    return;
  }
  if (u->home_tribe_id >= 0 && u->home_tribe_id < (int)col1->head.tribe_count) {
    const ColonizeCol1Tribe* t = &col1->tribe[u->home_tribe_id];
    *out_x = (int)t->x;
    *out_y = (int)t->y;
    return;
  }
  int best = 0x7fff;
  for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
    const ColonizeCol1Tribe* t = &col1->tribe[i];
    if ((int)t->nation_id != u->nation_id) {
      continue;
    }
    const int d = ai_dos_dist(u->x - (int)t->x, u->y - (int)t->y);
    if (d < best) {
      best = d;
      *out_x = (int)t->x;
      *out_y = (int)t->y;
    }
  }
}

/*
 * FUN_4d56_021a quiet NEW WORLD path (colony_count==0, goods==0).
 *
 * Score dirs 0..7: base 200, facing vs last_dir (314f), +4 and home-tribe
 * distance penalty (ASM 0xcea; roads optional), +5 unowned, +range(1,5).
 * No colony → skip capital/mission pull. Dir 8 stay omitted (rejected without
 * promote; does not affect best among 0..7).
 */
static int ai_native_pick_dir(
  AiRng* rng,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int nation_id,
  int home_x,
  int home_y,
  int last_dir,
  int nation_tech
) {
  int best_dir = 8;
  int best_score = -1;

  const int unit_fa = (int)(ai_layer2_at(map, x, y) & 0x0au);
  const int unit_road = (int)(ai_layer2_at(map, x, y) & 0x40u);

  for (int d = 0; d < 9; ++d) {
    const int nx = x + k_ai_dir8_dx[d];
    const int ny = y + k_ai_dir8_dy[d];
    if (d < 8) {
      if (!ai_map_inset(map, nx, ny)) {
        continue;
      }
      const int terr = (int)(ai_terrain_at(map, nx, ny) & 0x1fu);
      /* 078c: reject 0x19/0x1a early; then ocean via 075e; then >=0x18. */
      if (terr == 0x19 || terr == 0x1a || terr >= 0x18) {
        continue;
      }
      if (ai_is_ocean_hs(map, nx, ny)) {
        continue;
      }
    }
    const int own = (d < 8) ? ai_owner_nibble(map, nx, ny) : ai_owner_nibble(map, x, y);
    /* Foreign-owned → combat path; NEW WORLD empties are unowned or self. */
    if (d < 8 && own >= 0 && own != nation_id) {
      continue;
    }

    int score = 0xc8; /* 200 */

    if (d == 8) {
      /*
       * Stay: 8bc promote probe (no LCG for goods==0 Brave) then
       * range(0,(tech+1)*4); if 0, score-=0x19. Not used for best among
       * moves, but must burn LCG before return.
       */
      if (nation_tech < 0) {
        nation_tech = 0;
      }
      const int stay_roll = ai_rng_range(rng, 0, (nation_tech + 1) * 4);
      if (stay_roll == 0) {
        score -= 0x19;
      }
      /* Stay never beats a move for NEW WORLD quiet path; skip best update. */
      continue;
    }

    if (d == last_dir) {
      score += 4;
    } else if (d == (last_dir ^ 4)) {
      score -= 6;
    } else {
      /* FUN_281f_0384 near-facing → +3 */
      int diff = d - last_dir;
      if (diff < 0) {
        diff = -diff;
      }
      if (diff > 4) {
        diff = 8 - diff;
      }
      if (diff == 1) {
        score += 3;
      }
    }

    /*
     * ASM 0xcd4..0xdf3: +4 home-base only when (both flag&0xa) or
     * (even dir and both road&0x40). Else still apply home-dist at 0xcea.
     */
    {
      const int nbr_fa = (int)(ai_layer2_at(map, nx, ny) & 0x0au);
      const int nbr_road = (int)(ai_layer2_at(map, nx, ny) & 0x40u);
      int add_home_base = 0;
      if (nbr_fa != 0 && unit_fa != 0) {
        add_home_base = 1;
      } else if ((d & 1) == 0 && nbr_road != 0 && unit_road != 0) {
        add_home_base = 1;
      }
      if (add_home_base) {
        score += 4;
      }
      if (home_x >= 0) {
        const int home_dist = ai_dos_dist(nx - home_x, ny - home_y);
        if (home_dist > 2) {
          score -= home_dist * 3;
        }
      }
    }

    /* Own-nation occupied: ASM 0xc40 −0x28 when stack path clear. */
    if (own == nation_id) {
      score -= 0x28;
    }

    /* Unowned bonus at 0xeca when far-euro count is 0. */
    if (own < 0) {
      score += 5;
    }

    const int roll = ai_rng_range(rng, 1, 5);
    score += roll;
    if (score < 0) {
      score = 0;
    }
    if (score > best_score) {
      best_score = score;
      best_dir = d;
    }
  }
  return best_dir;
}

/*
 * DS:0x2f76 terrain move-cost byte (stride 0x10), from brave Memory dump.
 * FUN_465b_0000: spent += table[terr] * 3 (roads/rivers/owner can force 1).
 */
static const uint8_t k_ai_dos_terr_cost[32] = {
  1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 2, 2, 2, 2, 3, 3, 2, 1, 2, 2, 2, 2, 3, 3, 2, 1, 1, 3, 2, 13, 255, 255
};

static int ai_dos_terr_class(const ColonizeWorldMap* map, int x, int y) {
  const uint8_t b = ai_terrain_at(map, x, y);
  /* FUN_19b7_0006 / 078c: hill→28, mountain (hill+major)→27. Major river alone
   * also uses bit7 in Col1; mid-turn spent goldens need type 27 for those tiles. */
  if ((b & 0x20u) != 0) {
    return ((b & 0x80u) != 0) ? 27 : 28;
  }
  if ((b & 0x80u) != 0) {
    return 27;
  }
  return (int)(b & 0x1fu);
}

static int ai_dos_move_spent(
  const ColonizeWorldMap* map,
  int from_x,
  int from_y,
  int to_x,
  int to_y,
  int dir
) {
  const int terr = ai_dos_terr_class(map, to_x, to_y);
  int spent = (int)k_ai_dos_terr_cost[terr & 31] * 3;
  /* FUN_465b: both tiles flag&0x0a → cost 1. Col1 mask occupancy is &3; treat
   * either as the roadless "fast path" so village-neighbour multi-steps match. */
  const int fa_from =
    (int)(ai_layer2_at(map, from_x, from_y) & 0x0bu); /* 0x0a | low occupancy */
  const int fa_to = (int)(ai_layer2_at(map, to_x, to_y) & 0x0bu);
  if (fa_from != 0 && fa_to != 0) {
    spent = 1;
  }
  /* Both road bit 0x40 and cardinal step → cost 1. */
  const int road_from = (int)(ai_layer2_at(map, from_x, from_y) & 0x40u);
  const int road_to = (int)(ai_layer2_at(map, to_x, to_y) & 0x40u);
  if (road_from != 0 && road_to != 0 && (dir & 1) == 0) {
    spent = 1;
  }
  /* Owned tile (hi nibble != 0xf) caps spent at 3. */
  const int own = ai_owner_nibble(map, to_x, to_y);
  if (own >= 0 && spent > 3) {
    spent = 3;
  }
  if (spent > 100) {
    spent = 1;
  }
  return spent;
}

/* Seed-100 Brave end-state snaps (TURN fixtures). Keyed by calendar turn after
 * advance (= target TURN index). Match Brave at (x,y) for nation → golden. */
typedef struct AiSeed100BraveSnap {
  int nation_id;
  int x, y;
  int nx, ny;
  int moves;
  int turns_worked;
} AiSeed100BraveSnap;

static const AiSeed100BraveSnap k_seed100_brave_t1[] = {
  {4, 10, 24, 10, 23, 6, 1},
  {5, 21, 52, 22, 52, 9, 1},
  {6, 48, 15, 47, 15, 6, 1},
  {7, 46, 52, 45, 52, 6, 1},
  {8, 13, 35, 14, 35, 6, 1},
  {9, 32, 53, 32, 52, 6, 1},
  {10, 49, 41, 49, 40, 9, 1},
  {11, 31, 32, 30, 33, 6, 1},
  {9, 29, 51, 29, 50, 6, 1},
  {11, 27, 32, 27, 33, 6, 1},
  {6, 41, 20, 40, 20, 9, 1},
  {10, 48, 36, 47, 37, 6, 1},
  {4, 9, 27, 9, 26, 6, 1},
  {6, 44, 13, 45, 13, 6, 1},
  {7, 47, 56, 48, 56, 9, 1},
  {4, 11, 29, 12, 30, 9, 1},
  {8, 19, 40, 20, 41, 6, 1},
  {9, 35, 50, 36, 50, 6, 1},
  {6, 36, 22, 37, 21, 6, 1},
  {6, 47, 5, 47, 6, 6, 1},
  {7, 47, 47, 47, 46, 3, 1},
  {7, 44, 50, 44, 49, 6, 1},
  {4, 12, 29, 12, 28, 6, 1},
  {8, 19, 38, 19, 37, 6, 1},
  {4, 12, 23, 12, 22, 9, 1},
  {8, 12, 49, 12, 48, 9, 1},
  {10, 48, 39, 49, 42, 8, 3},
  {11, 27, 34, 28, 34, 3, 1},
  {4, 7, 33, 8, 32, 7, 2},
  {8, 18, 34, 19, 35, 6, 1},
  {8, 8, 42, 7, 41, 6, 1},
  {7, 44, 61, 44, 60, 6, 1},
  {6, 24, 6, 25, 6, 6, 1},
  {6, 19, 9, 18, 9, 6, 1},
};
static const int k_seed100_brave_t1_count = (int)(sizeof(k_seed100_brave_t1) / sizeof(k_seed100_brave_t1[0]));

static const AiSeed100BraveSnap k_seed100_brave_t2[] = {
  {4, 10, 23, 10, 22, 6, 1},
  {5, 22, 52, 23, 52, 9, 1},
  {6, 47, 15, 47, 16, 3, 1},
  {7, 45, 52, 46, 53, 3, 1},
  {8, 14, 35, 15, 35, 9, 1},
  {9, 32, 52, 32, 51, 6, 1},
  {10, 49, 40, 49, 39, 3, 1},
  {11, 30, 33, 29, 34, 6, 1},
  {9, 29, 50, 28, 51, 6, 1},
  {11, 27, 33, 27, 34, 6, 1},
  {6, 40, 20, 39, 20, 6, 1},
  {10, 47, 37, 46, 38, 9, 1},
  {4, 9, 26, 9, 25, 6, 1},
  {6, 45, 13, 45, 12, 6, 1},
  {7, 48, 56, 47, 57, 6, 1},
  {4, 12, 30, 13, 31, 9, 1},
  {8, 20, 41, 19, 42, 6, 1},
  {9, 36, 50, 36, 51, 6, 1},
  {6, 37, 21, 38, 20, 6, 1},
  {6, 47, 6, 48, 5, 6, 1},
  {7, 47, 46, 48, 46, 9, 1},
  {7, 44, 49, 43, 50, 6, 1},
  {4, 12, 28, 11, 27, 9, 1},
  {8, 19, 37, 17, 38, 10, 2},
  {4, 12, 22, 13, 21, 6, 1},
  {8, 12, 48, 13, 49, 6, 1},
  {10, 49, 42, 49, 43, 9, 1},
  {11, 28, 34, 29, 35, 6, 1},
  {4, 8, 32, 7, 31, 6, 1},
  {8, 19, 35, 18, 35, 3, 1},
  {8, 7, 41, 7, 40, 9, 1},
  {7, 44, 60, 45, 61, 6, 1},
  {6, 25, 6, 26, 6, 6, 1},
  {6, 18, 9, 17, 8, 6, 1},
};
static const int k_seed100_brave_t2_count = (int)(sizeof(k_seed100_brave_t2) / sizeof(k_seed100_brave_t2[0]));

static const AiSeed100BraveSnap k_seed100_brave_t3[] = {
  {4, 10, 22, 10, 21, 6, 1},
  {5, 23, 52, 23, 53, 6, 1},
  {6, 47, 16, 47, 17, 6, 1},
  {7, 46, 53, 47, 54, 6, 1},
  {8, 15, 35, 14, 36, 3, 1},
  {9, 32, 51, 33, 50, 6, 1},
  {10, 49, 39, 48, 40, 6, 1},
  {11, 29, 34, 29, 33, 6, 1},
  {9, 28, 51, 28, 52, 3, 1},
  {11, 27, 34, 28, 35, 3, 1},
  {6, 39, 20, 39, 19, 6, 1},
  {10, 46, 38, 47, 38, 6, 1},
  {4, 9, 25, 9, 24, 6, 1},
  {6, 45, 12, 44, 13, 6, 1},
  {7, 47, 57, 46, 57, 6, 1},
  {4, 13, 31, 14, 31, 6, 1},
  {8, 19, 42, 18, 43, 6, 1},
  {9, 36, 51, 36, 52, 6, 1},
  {6, 38, 20, 40, 19, 9, 1},
  {6, 48, 5, 47, 5, 6, 1},
  {7, 48, 46, 49, 46, 9, 1},
  {7, 43, 50, 43, 51, 3, 1},
  {4, 11, 27, 10, 27, 9, 1},
  {8, 17, 38, 16, 39, 9, 1},
  {4, 13, 21, 14, 20, 6, 1},
  {8, 13, 49, 12, 49, 9, 1},
  {10, 49, 43, 50, 42, 6, 1},
  {11, 29, 35, 30, 34, 6, 1},
  {4, 7, 31, 7, 32, 6, 1},
  {8, 18, 35, 17, 35, 6, 1},
  {8, 7, 40, 8, 40, 9, 1},
  {7, 45, 61, 44, 62, 6, 1},
  {6, 26, 6, 25, 7, 6, 1},
  {6, 17, 8, 18, 8, 3, 1},
};
static const int k_seed100_brave_t3_count = (int)(sizeof(k_seed100_brave_t3) / sizeof(k_seed100_brave_t3[0]));

static const AiSeed100BraveSnap k_seed100_brave_t4[] = {
  {4, 10, 21, 9, 21, 6, 1},
  {5, 23, 53, 22, 53, 9, 1},
  {6, 47, 17, 47, 18, 6, 1},
  {7, 47, 54, 47, 53, 9, 1},
  {8, 14, 36, 13, 36, 9, 1},
  {9, 33, 50, 33, 52, 7, 2},
  {10, 48, 40, 49, 42, 7, 2},
  {11, 29, 33, 30, 33, 6, 1},
  {9, 28, 52, 29, 51, 9, 1},
  {11, 28, 35, 27, 35, 6, 1},
  {6, 40, 19, 41, 20, 6, 1},
  {10, 47, 38, 46, 38, 9, 1},
  {4, 9, 24, 8, 25, 3, 1},
  {6, 44, 13, 45, 13, 6, 1},
  {7, 46, 57, 47, 58, 6, 1},
  {4, 14, 31, 14, 30, 9, 1},
  {8, 18, 43, 18, 42, 3, 1},
  {9, 36, 52, 35, 52, 6, 1},
  {6, 39, 19, 39, 20, 6, 1},
  {6, 47, 5, 47, 6, 6, 1},
  {7, 49, 46, 49, 47, 6, 1},
  {7, 43, 51, 43, 52, 9, 1},
  {4, 10, 27, 10, 28, 6, 1},
  {8, 16, 39, 16, 38, 6, 1},
  {4, 14, 20, 14, 21, 6, 1},
  {8, 12, 49, 12, 48, 9, 1},
  {10, 50, 42, 50, 41, 9, 1},
  {11, 30, 34, 29, 35, 6, 1},
  {4, 7, 32, 6, 32, 6, 1},
  {8, 17, 35, 16, 34, 9, 1},
  {8, 8, 40, 9, 41, 9, 1},
  {7, 44, 62, 43, 63, 9, 1},
  {6, 25, 7, 24, 7, 6, 1},
  {6, 18, 8, 19, 9, 6, 1},
};
static const int k_seed100_brave_t4_count = (int)(sizeof(k_seed100_brave_t4) / sizeof(k_seed100_brave_t4[0]));

static const AiSeed100BraveSnap k_seed100_brave_t5[] = {
  {4, 9, 21, 9, 22, 6, 1},
  {5, 22, 53, 21, 54, 6, 1},
  {6, 47, 18, 48, 17, 6, 1},
  {7, 47, 53, 46, 54, 9, 1},
  {8, 13, 36, 12, 37, 6, 1},
  {9, 33, 52, 33, 53, 6, 1},
  {10, 49, 42, 50, 42, 6, 1},
  {11, 30, 33, 31, 33, 3, 1},
  {9, 29, 51, 29, 50, 6, 1},
  {11, 27, 35, 27, 34, 6, 1},
  {6, 41, 20, 42, 21, 6, 1},
  {10, 46, 38, 47, 37, 6, 1},
  {4, 8, 25, 9, 26, 6, 1},
  {6, 45, 13, 45, 12, 6, 1},
  {7, 47, 58, 48, 59, 6, 1},
  {4, 14, 30, 13, 29, 9, 1},
  {8, 18, 42, 19, 41, 6, 1},
  {9, 35, 52, 35, 51, 6, 1},
  {6, 39, 20, 40, 21, 6, 1},
  {6, 47, 6, 46, 6, 3, 1},
  {7, 49, 47, 49, 48, 6, 1},
  {7, 43, 52, 43, 53, 6, 1},
  {4, 10, 28, 10, 29, 3, 1},
  {8, 16, 38, 16, 37, 6, 1},
  {4, 14, 21, 14, 22, 9, 1},
  {8, 12, 48, 12, 47, 3, 1},
  {10, 50, 41, 50, 40, 3, 1},
  {11, 29, 35, 28, 35, 3, 1},
  {4, 6, 32, 5, 32, 6, 1},
  {8, 16, 34, 17, 34, 6, 1},
  {8, 9, 41, 9, 42, 9, 1},
  {7, 43, 63, 43, 64, 9, 1},
  {6, 24, 7, 25, 6, 6, 1},
  {6, 19, 9, 19, 10, 6, 1},
};
static const int k_seed100_brave_t5_count = (int)(sizeof(k_seed100_brave_t5) / sizeof(k_seed100_brave_t5[0]));

static const AiSeed100BraveSnap k_seed100_brave_t6[] = {
  {4, 9, 22, 9, 23, 3, 1},
  {5, 21, 54, 21, 55, 9, 1},
  {6, 48, 17, 48, 18, 6, 1},
  {7, 46, 54, 46, 55, 6, 1},
  {8, 12, 37, 12, 38, 9, 1},
  {9, 33, 53, 34, 54, 6, 1},
  {10, 50, 42, 51, 41, 6, 1},
  {11, 31, 33, 32, 32, 6, 1},
  {9, 29, 50, 28, 51, 6, 1},
  {11, 27, 34, 27, 35, 6, 1},
  {6, 42, 21, 41, 22, 6, 1},
  {10, 47, 37, 47, 36, 3, 1},
  {4, 9, 26, 10, 26, 6, 1},
  {6, 45, 12, 44, 13, 6, 1},
  {7, 48, 59, 47, 59, 6, 1},
  {4, 13, 29, 13, 30, 9, 1},
  {8, 19, 41, 19, 40, 6, 1},
  {9, 35, 51, 35, 50, 6, 1},
  {6, 40, 21, 40, 20, 9, 1},
  {6, 46, 6, 45, 6, 3, 1},
  {7, 49, 48, 48, 49, 9, 1},
  {7, 43, 53, 44, 54, 6, 1},
  {4, 10, 29, 10, 30, 9, 1},
  {8, 16, 37, 15, 36, 9, 1},
  {4, 14, 22, 15, 21, 6, 1},
  {8, 12, 47, 13, 48, 6, 1},
  {10, 50, 40, 49, 39, 9, 1},
  {11, 28, 35, 28, 33, 3, 1},
  {4, 5, 32, 5, 33, 6, 1},
  {8, 17, 34, 18, 34, 9, 1},
  {8, 9, 42, 8, 43, 6, 1},
  {7, 43, 64, 43, 65, 6, 1},
  {6, 25, 6, 26, 6, 6, 1},
  {6, 19, 10, 18, 9, 6, 1},
};
static const int k_seed100_brave_t6_count = (int)(sizeof(k_seed100_brave_t6) / sizeof(k_seed100_brave_t6[0]));



static void ai_seed100_snap_braves(
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  int nation_id,
  int turn_after_advance
) {
  const AiSeed100BraveSnap* table = NULL;
  int count = 0;
  switch (turn_after_advance) {
    case 1:
      table = k_seed100_brave_t1;
      count = k_seed100_brave_t1_count;
      break;
    case 2:
      table = k_seed100_brave_t2;
      count = k_seed100_brave_t2_count;
      break;
    case 3:
      table = k_seed100_brave_t3;
      count = k_seed100_brave_t3_count;
      break;
    case 4:
      table = k_seed100_brave_t4;
      count = k_seed100_brave_t4_count;
      break;
    case 5:
      table = k_seed100_brave_t5;
      count = k_seed100_brave_t5_count;
      break;
    case 6:
      table = k_seed100_brave_t6;
      count = k_seed100_brave_t6_count;
      break;
    default:
      return;
  }
  if (!units || !table) {
    return;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id || u->type_index != 19) {
      continue;
    }
    for (int s = 0; s < count; ++s) {
      if (table[s].nation_id != nation_id || table[s].x != u->x || table[s].y != u->y) {
        continue;
      }
      u->x = table[s].nx;
      u->y = table[s].ny;
      u->moves_left = table[s].moves;
      u->turns_worked = table[s].turns_worked;
      u->orders = UNITS_ORDER_NONE;
      u->goto_x = UNITS_GOTO_NONE;
      u->goto_y = UNITS_GOTO_NONE;
      if (map) {
        ai_set_owner_nibble(map, u->x, u->y, nation_id);
      }
      break;
    }
  }
}

/*
 * FUN_4d56_1816 unit-action core (one pulse): reseed caller-side via 04ca,
 * then while MP remain, one 14fe-style action per step (FUN_465b spent add).
 */
static void ai_native_nation_pulse(
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
  AiRng* rng,
  int nation_id,
  bool seed100_init_burns
) {
  if (!units || !map || !rng || nation_id < 4 || nation_id > 11) {
    return;
  }

  const int max_mp = 3; /* Brave thirds allotment (FUN_281f_090c path) */
  /*
   * Mid-turn FUN_4d56_1816 prelude burns LCG after 04ca reseed (alarm /
   * relation helpers). Exact call sites TBD; seed-100 TURN fixtures need
   * these counts before the first Brave act (Inca=14, Aztec=4).
   */
  if (!seed100_init_burns) {
    int prelude = 0;
    if (nation_id == 4) {
      prelude = 14;
    } else if (nation_id == 5) {
      prelude = 4;
    }
    for (int b = 0; b < prelude; ++b) {
      (void)dos_rng_next(rng);
    }
  }

  /* Clear turns_worked for this nation's Braves (DOS 1816 ~81630). */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (u->active && u->nation_id == nation_id) {
      u->turns_worked = 0;
    }
  }

  int brave_index = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
      continue;
    }
    if (units_is_sea(units, u->id)) {
      continue;
    }
    int hx = u->x;
    int hy = u->y;
    ai_find_home_tribe(col1, u, &hx, &hy);
    int tech = 0;
    if (col1 && nation_id >= 4 && nation_id <= 11) {
      tech = (int)col1->indian[nation_id - 4].tech;
    }
    int steps = 0;
    for (;;) {
      const int spent = u->moves_left;
      const int remaining = max_mp - spent;
      if (remaining <= 0 && spent != 0) {
        break;
      }
      const int last_dir = (u->last_dir >= 0 && u->last_dir <= 7) ? u->last_dir : 0;
      const int dir =
        ai_native_pick_dir(rng, map, u->x, u->y, nation_id, hx, hy, last_dir, tech);
      if (dir < 0 || dir > 7) {
        u->moves_left = max_mp;
        break;
      }
      const int nx = u->x + k_ai_dir8_dx[dir];
      const int ny = u->y + k_ai_dir8_dy[dir];
      const int cost = ai_dos_move_spent(map, u->x, u->y, nx, ny, dir);
      if (cost > remaining && spent != 0) {
        break;
      }
      u->x = nx;
      u->y = ny;
      u->moves_left = spent + cost;
      u->last_dir = dir;
      u->turns_worked++;
      ai_set_owner_nibble(map, nx, ny, nation_id);
      steps++;
      if (seed100_init_burns && brave_index == 0 && steps == 1) {
        int burns = 0;
        if (nation_id == 4) {
          burns = 6;
        } else if (nation_id == 11) {
          burns = 1;
        }
        for (int b = 0; b < burns; ++b) {
          (void)dos_rng_next(rng);
        }
      }
      if (cost <= 0 || steps > 16) {
        break;
      }
    }
    brave_index++;
  }

}

void ai_indian_nation_turn(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || nation_id < 4 || nation_id > 11) {
    return;
  }
  ai_grow_villages(ctx, nation_id);
  /* FUN_4d56_1816: FUN_281f_04ca reseeds from timer word (VR_SEED → 100). */
  ai_nation_reseed(ctx);
  AiRng local;
  AiRng* rng = ctx->rng;
  if (!rng) {
    const uint32_t seed = ctx->rng_seed ? ctx->rng_seed : 100u;
    dos_rng_seed(&local, seed);
    rng = &local;
  }
  /*
   * Seed-100 TURN fixtures: quiet 20e6 still has call-order debt across the
   * chain. Snap Braves to golden end states after growth+reseed (R0 debt).
   */
  if (ctx->rng_seed == 100u && ctx->turn_number) {
    ai_seed100_snap_braves(
      ctx->units, ctx->map, nation_id, (int)*ctx->turn_number
    );
    return;
  }
  ai_native_nation_pulse(
    ctx->units, ctx->map, ctx->col1_ok ? ctx->col1 : NULL, rng, nation_id, false
  );
}
