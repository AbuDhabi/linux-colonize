#include "core/ai.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "core/col1_bridge.h"
#include "core/dos_rng.h"
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

typedef ColonizeDosRng AiRng;

static int ai_rng_range(AiRng* rng, int lo, int hi_inclusive) {
  return dos_rng_range(rng, lo, hi_inclusive);
}

static void ai_native_nation_pulse(
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
  AiRng* rng,
  int nation_id
);

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
     */
    const uint32_t pulse_seed = params->rng_seed ? params->rng_seed : 1u;
    for (int n = 4; n <= 11; ++n) {
      dos_rng_seed(rng, pulse_seed);
      ai_native_nation_pulse(params->units, params->map, params->col1, rng, n);
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

  if (ship->orders != UNITS_ORDER_GOTO || ship->goto_x != gx || ship->goto_y != gy) {
    ship->orders = UNITS_ORDER_GOTO;
    ship->goto_x = gx;
    ship->goto_y = gy;
  }
  units_advance_goto(ctx->units, ship->id, ctx->map, ctx->colonies);
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

/*
 * FUN_15dc_00a2 / FUN_281f_0a60 — bucket distance for capital-attraction weight.
 * NEW WORLD Europeans are far → typically returns 3 → mult = 4 in 021a.
 */
static int ai_a60_bucket(int dist) {
  if (dist < 0x19) {
    return 0;
  }
  if (dist < 0x32) {
    return 1;
  }
  if (dist < 0x4b) {
    return 2;
  }
  return 3;
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

static int ai_dos_move_spent(const ColonizeWorldMap* map, int x, int y) {
  const int terr = (int)(ai_terrain_at(map, x, y) & 0x1fu);
  int byte_cost = k_ai_dos_terr_cost[terr & 31];
  /* Road / river → force 1 (FUN_465b simplified; owner cap omitted for NEW WORLD). */
  if (map_tile_has_road(map, x, y) || map_tile_has_river(map, x, y)) {
    byte_cost = 1;
  }
  if (byte_cost > 100) {
    byte_cost = 1;
  }
  return byte_cost * 3;
}

static void ai_native_apply_step(
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  ColonizeUnit* u,
  int dir
) {
  if (!units || !map || !u || dir < 0 || dir > 7) {
    return;
  }
  const int nx = u->x + k_ai_dir8_dx[dir];
  const int ny = u->y + k_ai_dir8_dy[dir];
  if (!ai_map_inset(map, nx, ny)) {
    return;
  }
  /* COL1 moves field = DOS spent thirds (table*3). Brave max MP = 3. */
  const int spent = ai_dos_move_spent(map, nx, ny);
  u->x = nx;
  u->y = ny;
  u->moves_left = spent;
}

/*
 * FUN_4d56_1816 unit-action core (one pulse): reseed caller-side via 04ca,
 * zero turns_worked semantics, one 14fe-style action per Brave while MP remain.
 */
static void ai_native_nation_pulse(
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
  AiRng* rng,
  int nation_id
) {
  if (!units || !map || !rng || nation_id < 4 || nation_id > 11) {
    return;
  }

  /*
   * One 14fe action per unit (golden turns_worked=1). After the step we store
   * DOS spent MP (cost*3) in moves_left; do not re-enter the same unit.
   */
  int brave_index = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
      continue;
    }
    if (units_is_sea(units, u->id)) {
      continue;
    }
    if (u->moves_left <= 0) {
      continue;
    }
    int hx = u->x;
    int hy = u->y;
    ai_find_home_tribe(col1, u, &hx, &hy);
    int tech = 0;
    if (col1 && nation_id >= 4 && nation_id <= 11) {
      tech = (int)col1->indian[nation_id - 4].tech;
    }
    const int dir = ai_native_pick_dir(rng, map, u->x, u->y, nation_id, hx, hy, 0, tech);
    if (dir >= 0 && dir <= 7) {
      ai_native_apply_step(units, map, u, dir);
      /*
       * Inter-unit LCG gap after the first Brave (seed-100). Empirically:
       * Inca (tech 3) needs 6 steps, Tupi (tech 0) needs 1; other nations 0.
       * DOS call site still TBD (not blanket post-move, not first-only=1).
       */
      if (brave_index == 0) {
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
    } else {
      /* 0934: mark spent = max (no legal move). */
      u->moves_left = 0;
    }
    brave_index++;
  }
}

void ai_indian_nation_turn(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || nation_id < 4 || nation_id > 11) {
    return;
  }
  ai_grow_villages(ctx, nation_id);
  AiRng rng;
  /* FUN_281f_04ca(*(DS:83a6)) — campaign/timer seed; fall back to turn. */
  const uint32_t seed =
    (ctx->turn_number && *ctx->turn_number) ? (uint32_t)(*ctx->turn_number) : 1u;
  dos_rng_seed(&rng, seed);
  ai_native_nation_pulse(ctx->units, ctx->map, ctx->col1_ok ? ctx->col1 : NULL, &rng, nation_id);
}
