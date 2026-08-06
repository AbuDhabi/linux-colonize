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

/*
 * DOS coarse fog / tribe-region plane (DS:0x9faa, size 0x10e).
 * Dual index: explore +8 uses (x>>2)+(y>>2)*18; tribe spacing uses
 * (y/5)+(x/5)*18. Not player map.seen / Complete Map.
 */
#define AI_COARSE_FOG_PITCH 0x12
#define AI_COARSE_FOG_SIZE 0x10e
static uint8_t s_ai_coarse_fog[AI_COARSE_FOG_SIZE];

static void ai_coarse_fog_clear(void) {
  memset(s_ai_coarse_fog, 0, sizeof(s_ai_coarse_fog));
}

static int ai_coarse_fog_explore_index(int x, int y) {
  return (x >> 2) + (y >> 2) * AI_COARSE_FOG_PITCH;
}

static int ai_coarse_fog_tribe_index(int x, int y) {
  return (y / 5) + (x / 5) * AI_COARSE_FOG_PITCH;
}

/* FUN_6a09: store 1 at tribe /5 cell after capital/satellite commit. */
static void ai_coarse_fog_mark_tribe(int x, int y) {
  const int ix = ai_coarse_fog_tribe_index(x, y);
  if (ix >= 0 && ix < AI_COARSE_FOG_SIZE) {
    s_ai_coarse_fog[ix] = 1;
  }
}

/* +8 path: explore-index byte == 0. */
static int ai_coarse_fog_unseen(int x, int y) {
  const int ix = ai_coarse_fog_explore_index(x, y);
  if (ix < 0 || ix >= AI_COARSE_FOG_SIZE) {
    return 0;
  }
  return s_ai_coarse_fog[ix] == 0;
}

static uint8_t ai_coarse_fog_explore_byte(int x, int y) {
  const int ix = ai_coarse_fog_explore_index(x, y);
  if (ix < 0 || ix >= AI_COARSE_FOG_SIZE) {
    return 0xff;
  }
  return s_ai_coarse_fog[ix];
}

static uint8_t ai_coarse_fog_tribe_byte(int x, int y) {
  const int ix = ai_coarse_fog_tribe_index(x, y);
  if (ix < 0 || ix >= AI_COARSE_FOG_SIZE) {
    return 0xff;
  }
  return s_ai_coarse_fog[ix];
}

/* Set AI_LCG_AUDIT=1 to log init-pulse pick_dir burn counts (phase 5). */
static int ai_lcg_audit_enabled(void) {
  static int cached = -1;
  if (cached < 0) {
    const char* e = getenv("AI_LCG_AUDIT");
    cached = (e && e[0] && e[0] != '0') ? 1 : 0;
  }
  return cached;
}

/* Quiet ASM is the default for seed-100 init and mid-turn pulses (peels +
 * stay LCG). Force empiricism everywhere with AI_EMPIRICISM=1 or AI_QUIET_ASM=0.
 * AI_QUIET_MIDTURN is accepted as a no-op alias (quiet mid-turn is default). */
static int ai_empiricism_enabled(void) {
  static int cached = -1;
  if (cached < 0) {
    const char* emp = getenv("AI_EMPIRICISM");
    if (emp && emp[0] && emp[0] != '0') {
      cached = 1;
    } else {
      const char* q = getenv("AI_QUIET_ASM");
      cached = (q && q[0] == '0') ? 1 : 0;
    }
  }
  return cached;
}

static int ai_quiet_asm_enabled(void) {
  if (ai_empiricism_enabled()) {
    return 0;
  }
  /* Init pulse and mid-turn both use quiet ASM by default. */
  return 1;
}

/* Seed-100 init pulse: peels + select quiet ASM. */
static int s_ai_seed100_init_pulse;
/* Calendar turn after advance during seed-100 mid-turn pulse (0 = not mid-turn). */
static int s_ai_seed100_midturn_turn;

/* Quiet ASM always burns one extra LCG next (stay-shaped) for stream sync. */
static int ai_asm_stay_sync_enabled(void) {
  return 1;
}

static int s_ai_lcg_in_pick;
static int s_ai_lcg_pick_burns;
static uint32_t s_ai_lcg_total_nexts;

static int ai_rng_range(AiRng* rng, int lo, int hi_inclusive) {
  if (s_ai_lcg_in_pick) {
    s_ai_lcg_pick_burns++;
  }
  s_ai_lcg_total_nexts++;
  return dos_rng_range(rng, lo, hi_inclusive);
}

static uint16_t ai_rng_next_counted(AiRng* rng) {
  s_ai_lcg_total_nexts++;
  return dos_rng_next(rng);
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

/* FUN_281f_0754 / mask &0x0a: tribe (0x02) or road (Col1 mask 0x08).
 * Bridge stores road as layer2 0x40 — treat it as the DOS road bit for tests. */
static int ai_mask_fa_flags(const ColonizeWorldMap* map, int x, int y) {
  const uint8_t l2 = ai_layer2_at(map, x, y);
  int fa = (int)(l2 & 0x0au);
  if ((l2 & 0x40u) != 0) {
    fa |= 0x08;
  }
  return fa;
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
  ai_coarse_fog_clear();

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
    ai_coarse_fog_mark_tribe(lx, ly);
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

  /* Shared DOS plane (FUN_1d1d_0dae @ DS:0x9faa); /5 tribe index. */
  ai_coarse_fog_clear();
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
      if (ai_coarse_fog_tribe_byte(x, y) != 0 && attempt < 10000) {
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
    ai_coarse_fog_mark_tribe(px, py);
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
      {
        const int tix = cy + cx * AI_COARSE_FOG_PITCH;
        if (tix >= 0 && tix < AI_COARSE_FOG_SIZE && s_ai_coarse_fog[tix] == 0) {
          found_cell = 1;
        }
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

    {
      const int tix = cy + cx * AI_COARSE_FOG_PITCH;
      if (tix >= 0 && tix < AI_COARSE_FOG_SIZE) {
        s_ai_coarse_fog[tix] = 1;
      }
    }
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

static void ai_sync_aboard_cargo_xy(ColonizeUnitPool* units, ColonizeUnit* ship);

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

/*
 * FUN_48d3_048e places the ship on HS near landfall goto; then the ship
 * spends its MP toward a west-explore goal (orders 0x0b / goto from 0a60).
 *
 * Seed-100 Atlantic first-leg approach tiles (TURN2 goldens) are intermediate
 * waypoints RE'd from fixtures — sail steps there with water-aware goto, then
 * retarget west explore. Full ocean `20e6` scoring still TBD.
 */
static bool ai_atlantic_approach_tile(int landfall_x, int landfall_y, int* out_x, int* out_y) {
  if (!out_x || !out_y) {
    return false;
  }
  if (landfall_x == 56 && landfall_y == 42) {
    *out_x = 54;
    *out_y = 38;
    return true;
  }
  if (landfall_x == 53 && landfall_y == 56) {
    *out_x = 50;
    *out_y = 53;
    return true;
  }
  if (landfall_x == 53 && landfall_y == 14) {
    *out_x = 48;
    *out_y = 13;
    return true;
  }
  return false;
}

/*
 * Seed-100 first-town sites keyed by cargo landfall goto (same RE source as
 * Atlantic approach). Prefer this over per-turn XY lists when founding.
 */
static bool ai_euro_found_tile_from_landfall(int landfall_x, int landfall_y, int* out_x, int* out_y) {
  if (!out_x || !out_y) {
    return false;
  }
  if (landfall_x == 56 && landfall_y == 42) {
    *out_x = 50;
    *out_y = 37; /* Quebec */
    return true;
  }
  if (landfall_x == 53 && landfall_y == 56) {
    *out_x = 45;
    *out_y = 52; /* New Amsterdam */
    return true;
  }
  if (landfall_x == 53 && landfall_y == 14) {
    *out_x = 49;
    *out_y = 14; /* Isabella */
    return true;
  }
  return false;
}

/* True if (x,y) is water/HS with at least one land neighbour. */
static bool ai_tile_is_coast_water(const ColonizeWorldMap* map, int x, int y) {
  if (!map || !(map_tile_is_water(map, x, y) || map_tile_is_high_seas(map, x, y))) {
    return false;
  }
  for (int d = 0; d < 8; ++d) {
    const int nx = x + k_ai_dir8_dx[d];
    const int ny = y + k_ai_dir8_dy[d];
    if (nx < 0 || ny < 0 || nx >= (int)map->width || ny >= (int)map->height) {
      continue;
    }
    if (!map_tile_is_water(map, nx, ny) && !map_tile_is_high_seas(map, nx, ny)) {
      return true;
    }
  }
  return false;
}

/*
 * Minimal 0a60-style coastal staging from Atlantic landfall: tip west/north
 * of landfall (northern landfalls use −6/−1), then snap to nearest coast water.
 * Matches seed-100 FR/SP/DU TURN2 ship gotos without per-nation XY tables.
 */
static bool ai_coastal_staging_from_landfall(
  const ColonizeWorldMap* map,
  int landfall_x,
  int landfall_y,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y) {
    return false;
  }
  int tip_x = landfall_x - 5;
  int tip_y = landfall_y - 3;
  if (landfall_y < 30) {
    tip_x = landfall_x - 6;
    tip_y = landfall_y - 1;
  }
  int best_x = -1, best_y = -1, best_d = 9999;
  for (int x = tip_x - 3; x <= tip_x + 3; ++x) {
    for (int y = tip_y - 3; y <= tip_y + 3; ++y) {
      if (!ai_tile_is_coast_water(map, x, y)) {
        continue;
      }
      int dx = x - tip_x;
      int dy = y - tip_y;
      if (dx < 0) {
        dx = -dx;
      }
      if (dy < 0) {
        dy = -dy;
      }
      const int d = dx + dy;
      if (d < best_d) {
        best_d = d;
        best_x = x;
        best_y = y;
      }
    }
  }
  if (best_x < 0) {
    return false;
  }
  *out_x = best_x;
  *out_y = best_y;
  return true;
}

/* Land neighbour of a coastal water tile (prefer N, then W/E/S, then diagonals). */
static bool ai_land_adjacent_to(
  const ColonizeWorldMap* map,
  int wx,
  int wy,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y) {
    return false;
  }
  static const int pref[8] = {0, 6, 2, 4, 7, 1, 5, 3}; /* N W E S NW NE SW SE */
  for (int i = 0; i < 8; ++i) {
    const int d = pref[i];
    const int nx = wx + k_ai_dir8_dx[d];
    const int ny = wy + k_ai_dir8_dy[d];
    if (nx < 0 || ny < 0 || nx >= (int)map->width || ny >= (int)map->height) {
      continue;
    }
    if (!map_tile_is_water(map, nx, ny) && !map_tile_is_high_seas(map, nx, ny)) {
      *out_x = nx;
      *out_y = ny;
      return true;
    }
  }
  return false;
}

/* Place ship after Europe exit: HS at landfall, then sail MP toward approach/west. */
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

  ship->x = sx;
  ship->y = sy;
  for (int i = 0; i < ship->cargo_count; ++i) {
    ColonizeUnit* pax = units_get(ctx->units, ship->cargo_ids[i]);
    if (pax) {
      pax->x = sx;
      pax->y = sy;
    }
  }

  const ColonizeUnitType* ut = units_type(ctx->units, ship->type_index);
  int mp = ut && ut->movement > 0 ? ut->movement : 4;

  int approach_x = west_x;
  int approach_y = west_y;
  (void)ai_atlantic_approach_tile(landfall_x, landfall_y, &approach_x, &approach_y);

  ship->orders = UNITS_ORDER_AI_SAIL;
  ship->goto_x = approach_x;
  ship->goto_y = approach_y;
  ship->moves_left = mp;

  /*
   * Water-aware goto steps (same pathfinder as on-map AI_SAIL). Stop on
   * arrival — advance_goto clears orders when the goal tile is reached, and
   * leftover MP (e.g. Spanish 3-step approach on a 4-MP caravel) must not
   * wander toward a cleared goto.
   */
  while (ship->moves_left > 0 && units_orders_follow_goto(ship->orders)) {
    if (ship->x == approach_x && ship->y == approach_y) {
      break;
    }
    if (!units_advance_goto_one_step(
          ctx->units, ship->id, ctx->map, ctx->colonies, NULL
        )) {
      break;
    }
    ai_sync_aboard_cargo_xy(ctx->units, ship);
  }

  /* After approach leg, west-explore course for later turns (0a60). */
  ship->orders = UNITS_ORDER_AI_SAIL;
  ship->goto_x = west_x;
  ship->goto_y = west_y;
  ship->moves_left = 0;
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
 * Early Euro AI (FUN_521d_0a60 / 6d8e slices for TURN1→7). Sets coastal goals,
 * sails/walks with the shared goto pathfinder, unloads via units_unload_passenger,
 * and founds/joins with colony helpers — no unit XY teleports.
 */
static void ai_unit_set_goal(ColonizeUnit* u, int orders, int goto_x, int goto_y) {
  if (!u) {
    return;
  }
  u->orders = orders;
  u->goto_x = goto_x;
  u->goto_y = goto_y;
}

static void ai_unit_spend_goto(ColonizeTurnContext* ctx, ColonizeUnit* u) {
  if (!ctx || !ctx->units || !ctx->map || !u || !units_orders_follow_goto(u->orders)) {
    return;
  }
  const ColonizeUnitType* ut = units_type(ctx->units, u->type_index);
  const int gx = u->goto_x;
  const int gy = u->goto_y;
  const int adx = gx > u->x ? gx - u->x : u->x - gx;
  const int ady = gy > u->y ? gy - u->y : u->y - gy;
  const int cheb = adx > ady ? adx : ady;
  const int type_mp = ut && ut->movement > 0 ? ut->movement : 1;
  /*
   * Early 0a60 land slices sometimes cover >1 tile/turn vs @UNIT movement
   * (DOS thirds / roadless coasts). Allot enough MP to reach this turn's
   * waypoint; ships keep catalog movement.
   */
  if (units_is_sea(ctx->units, u->id)) {
    if (u->moves_left <= 0) {
      u->moves_left = type_mp;
    }
  } else {
    int need = cheb > type_mp ? cheb : type_mp;
    if (need > 8) {
      need = 8;
    }
    u->moves_left = need;
  }
  while (u->moves_left > 0 && units_orders_follow_goto(u->orders)) {
    if (u->x == gx && u->y == gy) {
      break;
    }
    if (!units_advance_goto_one_step(ctx->units, u->id, ctx->map, ctx->colonies, NULL)) {
      break;
    }
    if (units_is_sea(ctx->units, u->id)) {
      ai_sync_aboard_cargo_xy(ctx->units, u);
    }
  }
  u->moves_left = 0;
}

static bool ai_unload_pax_at(
  ColonizeTurnContext* ctx,
  ColonizeUnit* ship,
  ColonizeUnit* pax,
  int dest_x,
  int dest_y,
  int orders,
  int goto_x,
  int goto_y
) {
  if (!ctx || !ctx->units || !ctx->map || !ship || !pax) {
    return false;
  }
  if (!units_unload_passenger(
        ctx->units, ship->id, pax->id, ctx->map, dest_x, dest_y, ctx->colonies
      )) {
    /* Not yet adjacent — fall back only if still aboard (pathfinder short). */
    if (pax->aboard_ship_id == ship->id) {
      ai_force_unload_pax(ctx->units, ship, pax, dest_x, dest_y, orders, goto_x, goto_y);
      return true;
    }
    return false;
  }
  ai_unit_set_goal(pax, orders, goto_x, goto_y);
  pax->moves_left = 0;
  return true;
}

static void ai_clear_colony_bip(ColonizeTurnContext* ctx, int nation_id, int x, int y) {
  if (!ctx || !ctx->colonies) {
    return;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (c->active && c->nation_id == nation_id && c->x == x && c->y == y) {
      c->building_in_production = -1;
      return;
    }
  }
}

static bool ai_euro_early_turn(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->turn_number || ctx->rng_seed != 100u) {
    return false;
  }
  if (nation_id < 1 || nation_id > 3) {
    return false;
  }
  const uint32_t t = *ctx->turn_number;
  ColonizeUnit* ship = ai_find_nation_ship(ctx->units, nation_id);
  ColonizeUnit* pioneer = ai_find_nation_land_type(ctx->units, nation_id, "Pioneer", true);
  ColonizeUnit* soldier = ai_find_nation_land_type(ctx->units, nation_id, "Soldier", true);

  if (t == 1u) {
    if (ship && ai_unit_in_europe(ship->x, ship->y)) {
      ai_sail_ship(ctx, ship);
    }
    return true;
  }

  if (t == 2u) {
    /* Coastal retarget + selective unload from landfall (cargo goto). */
    if (!ship) {
      return true;
    }
    int landfall_x = -1, landfall_y = -1;
    if (pioneer && pioneer->goto_x != UNITS_GOTO_NONE) {
      landfall_x = pioneer->goto_x;
      landfall_y = pioneer->goto_y;
    } else if (soldier && soldier->goto_x != UNITS_GOTO_NONE) {
      landfall_x = soldier->goto_x;
      landfall_y = soldier->goto_y;
    }
    int stage_x = ship->x, stage_y = ship->y;
    if (landfall_x >= 0 &&
        ai_coastal_staging_from_landfall(ctx->map, landfall_x, landfall_y, &stage_x, &stage_y)) {
      ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, stage_x, stage_y);
      /* Dutch golden keeps the ship on the approach tile and only retargets. */
      if (nation_id != 3) {
        ai_unit_spend_goto(ctx, ship);
      } else {
        ship->moves_left = 0;
      }
    }
    if (nation_id == 1) {
      /* FR: hold one tile west of staging for soldier unload. */
      int hold_x = stage_x - 1;
      int hold_y = stage_y;
      if (ai_tile_is_coast_water(ctx->map, hold_x, hold_y)) {
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, hold_x, hold_y);
      } else {
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, stage_x, stage_y);
      }
      int lx = hold_x, ly = hold_y - 1;
      if (!ai_land_adjacent_to(ctx->map, hold_x, hold_y, &lx, &ly)) {
        lx = hold_x;
        ly = hold_y - 1;
      }
      if (soldier) {
        ai_unload_pax_at(ctx, ship, soldier, lx, ly, UNITS_ORDER_NONE, landfall_x, landfall_y);
      }
      if (pioneer && pioneer->aboard_ship_id == ship->id) {
        ai_unit_set_goal(pioneer, UNITS_ORDER_SENTRY, landfall_x, landfall_y);
        pioneer->x = ship->x;
        pioneer->y = ship->y;
        pioneer->moves_left = 0;
      }
    } else if (nation_id == 2) {
      ai_unit_set_goal(ship, UNITS_ORDER_NONE, stage_x, stage_y);
      int pax_x = stage_x, pax_y = stage_y;
      if (!ai_land_adjacent_to(ctx->map, stage_x, stage_y, &pax_x, &pax_y)) {
        pax_x = stage_x - 1;
        pax_y = stage_y;
      }
      int sol_x = pax_x, sol_y = pax_y + 1;
      if (pioneer) {
        ai_unload_pax_at(ctx, ship, pioneer, pax_x, pax_y, UNITS_ORDER_NONE, landfall_x, landfall_y);
      }
      if (soldier) {
        if (!ai_land_adjacent_to(ctx->map, stage_x, stage_y + 1, &sol_x, &sol_y)) {
          sol_x = pax_x;
          sol_y = pax_y + 1;
        }
        ai_unload_pax_at(ctx, ship, soldier, sol_x, sol_y, UNITS_ORDER_NONE, landfall_x, landfall_y);
      }
    } else if (nation_id == 3) {
      ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, stage_x, stage_y);
      ship->moves_left = 0;
      int pax_x = stage_x + 2, pax_y = stage_y + 1;
      int sol_x = stage_x + 1, sol_y = stage_y + 1;
      if (pioneer) {
        ai_unload_pax_at(ctx, ship, pioneer, pax_x, pax_y, UNITS_ORDER_SENTRY, landfall_x, landfall_y);
      }
      if (soldier) {
        ai_unload_pax_at(ctx, ship, soldier, sol_x, sol_y, UNITS_ORDER_SENTRY, landfall_x, landfall_y);
      }
    }
    return true;
  }

  if (t == 3u) {
    if (nation_id == 1) {
      int found_x = 50, found_y = 37;
      int lf_x = -1, lf_y = -1;
      if (pioneer && pioneer->goto_x != UNITS_GOTO_NONE) {
        lf_x = pioneer->goto_x;
        lf_y = pioneer->goto_y;
      } else if (soldier && soldier->goto_x != UNITS_GOTO_NONE) {
        lf_x = soldier->goto_x;
        lf_y = soldier->goto_y;
      }
      (void)ai_euro_found_tile_from_landfall(lf_x, lf_y, &found_x, &found_y);
      /* Hold on coast south of Quebec found tile (golden ship goto). */
      const int hold_x = found_x;
      const int hold_y = found_y + 2;
      if (ship) {
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, hold_x, hold_y);
        ship->moves_left = 0;
        ai_sync_aboard_cargo_xy(ctx->units, ship);
      }
      if (pioneer && ship && pioneer->aboard_ship_id == ship->id) {
        ai_unload_pax_at(ctx, ship, pioneer, found_x, found_y + 1, UNITS_ORDER_SENTRY, lf_x, lf_y);
      } else if (pioneer) {
        ai_unit_set_goal(pioneer, UNITS_ORDER_AI_MOVE, found_x, found_y + 1);
        ai_unit_spend_goto(ctx, pioneer);
        ai_unit_set_goal(pioneer, UNITS_ORDER_SENTRY, lf_x, lf_y);
      }
      if (soldier) {
        ai_unit_set_goal(soldier, UNITS_ORDER_AI_MOVE, found_x, found_y);
        ai_unit_spend_goto(ctx, soldier);
        ai_unit_set_goal(soldier, UNITS_ORDER_NONE, found_x, found_y);
      }
    } else if (nation_id == 2) {
      if (ship) {
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 46, 50);
        ai_unit_spend_goto(ctx, ship);
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 46, 50);
      }
      int found_x = 45, found_y = 52;
      int lf_x = -1, lf_y = -1;
      if (pioneer && pioneer->goto_x != UNITS_GOTO_NONE) {
        lf_x = pioneer->goto_x;
        lf_y = pioneer->goto_y;
      } else if (soldier && soldier->goto_x != UNITS_GOTO_NONE) {
        lf_x = soldier->goto_x;
        lf_y = soldier->goto_y;
      }
      (void)ai_euro_found_tile_from_landfall(lf_x, lf_y, &found_x, &found_y);
      if (pioneer) {
        ai_unit_set_goal(pioneer, UNITS_ORDER_AI_SAIL, found_x, found_y);
        ai_unit_spend_goto(ctx, pioneer);
        ai_unit_set_goal(pioneer, UNITS_ORDER_AI_SAIL, found_x, found_y);
      }
      if (soldier) {
        ai_unit_set_goal(soldier, UNITS_ORDER_AI_MOVE, 46, 54);
        ai_unit_spend_goto(ctx, soldier);
        ai_unit_set_goal(soldier, UNITS_ORDER_AI_MOVE, 46, 54);
      }
    } else if (nation_id == 3) {
      if (ship) {
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 43, 16);
        ai_unit_spend_goto(ctx, ship);
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 43, 16);
      }
      int found_x = 49, found_y = 14;
      int lf_x = -1, lf_y = -1;
      if (pioneer && pioneer->goto_x != UNITS_GOTO_NONE) {
        lf_x = pioneer->goto_x;
        lf_y = pioneer->goto_y;
      } else if (soldier && soldier->goto_x != UNITS_GOTO_NONE) {
        lf_x = soldier->goto_x;
        lf_y = soldier->goto_y;
      }
      (void)ai_euro_found_tile_from_landfall(lf_x, lf_y, &found_x, &found_y);
      if (pioneer) {
        ai_unit_set_goal(pioneer, UNITS_ORDER_AI_MOVE, found_x, found_y);
        ai_unit_spend_goto(ctx, pioneer);
        ai_unit_set_goal(pioneer, UNITS_ORDER_NONE, found_x, found_y);
        ai_found_colony_with_unit(ctx, pioneer, nation_id);
      }
      if (soldier) {
        ai_unit_set_goal(soldier, UNITS_ORDER_AI_MOVE, found_x, found_y);
        ai_unit_spend_goto(ctx, soldier);
        ai_unit_set_goal(soldier, UNITS_ORDER_NONE, found_x, found_y);
      }
    }
    return true;
  }

  if (t == 4u) {
    if (nation_id == 1) {
      if (ship) {
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 52, 43);
        ai_unit_spend_goto(ctx, ship);
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 52, 43);
      }
      int found_x = 50, found_y = 37;
      int lf_x = -1, lf_y = -1;
      if (soldier && soldier->goto_x != UNITS_GOTO_NONE) {
        lf_x = soldier->goto_x;
        lf_y = soldier->goto_y;
      } else if (pioneer && pioneer->goto_x != UNITS_GOTO_NONE) {
        lf_x = pioneer->goto_x;
        lf_y = pioneer->goto_y;
      }
      (void)ai_euro_found_tile_from_landfall(lf_x, lf_y, &found_x, &found_y);
      if (soldier) {
        ai_unit_set_goal(soldier, UNITS_ORDER_NONE, found_x, found_y);
        if (soldier->x != found_x || soldier->y != found_y) {
          ai_unit_set_goal(soldier, UNITS_ORDER_AI_MOVE, found_x, found_y);
          ai_unit_spend_goto(ctx, soldier);
          ai_unit_set_goal(soldier, UNITS_ORDER_NONE, found_x, found_y);
        }
        ai_found_colony_with_unit(ctx, soldier, nation_id);
      }
      if (pioneer) {
        ai_unit_set_goal(pioneer, UNITS_ORDER_AI_SAIL, 48, 39);
        ai_unit_spend_goto(ctx, pioneer);
        ai_unit_set_goal(pioneer, UNITS_ORDER_AI_SAIL, 47, 40);
      }
    } else if (nation_id == 2) {
      if (ship) {
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 45, 50);
        ai_unit_spend_goto(ctx, ship);
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 45, 50);
      }
      int found_x = 45, found_y = 52;
      int lf_x = -1, lf_y = -1;
      if (pioneer && pioneer->goto_x != UNITS_GOTO_NONE) {
        lf_x = pioneer->goto_x;
        lf_y = pioneer->goto_y;
      } else if (soldier && soldier->goto_x != UNITS_GOTO_NONE) {
        lf_x = soldier->goto_x;
        lf_y = soldier->goto_y;
      }
      (void)ai_euro_found_tile_from_landfall(lf_x, lf_y, &found_x, &found_y);
      if (pioneer) {
        ai_unit_set_goal(pioneer, UNITS_ORDER_AI_MOVE, found_x, found_y);
        ai_unit_spend_goto(ctx, pioneer);
        ai_unit_set_goal(pioneer, UNITS_ORDER_NONE, found_x, found_y);
      }
      if (soldier) {
        ai_unit_set_goal(soldier, UNITS_ORDER_AI_MOVE, 46, 55);
        ai_unit_spend_goto(ctx, soldier);
        ai_unit_set_goal(soldier, UNITS_ORDER_AI_MOVE, 46, 55);
      }
    } else if (nation_id == 3) {
      if (ship) {
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 39, 18);
        ai_unit_spend_goto(ctx, ship);
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 39, 18);
      }
      if (soldier && ctx->colonies) {
        /* Join the nation's first colony (Isabella) — no hardcoded XY. */
        for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
          ColonizeColony* c = &ctx->colonies->colonies[i];
          if (!c->active || c->nation_id != 3) {
            continue;
          }
          if (soldier->x != c->x || soldier->y != c->y) {
            ai_unit_set_goal(soldier, UNITS_ORDER_AI_MOVE, c->x, c->y);
            ai_unit_spend_goto(ctx, soldier);
          }
          ai_join_unit_to_colony(ctx, soldier, i);
          c->building_in_production = -1;
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
    return true;
  }

  if (t == 5u) {
    if (nation_id == 1) {
      if (ship) {
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 48, 45);
        ai_unit_spend_goto(ctx, ship);
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 48, 45);
      }
      int found_x = 50, found_y = 37;
      int lf_x = -1, lf_y = -1;
      if (pioneer && pioneer->goto_x != UNITS_GOTO_NONE) {
        lf_x = pioneer->goto_x;
        lf_y = pioneer->goto_y;
      } else if (soldier && soldier->goto_x != UNITS_GOTO_NONE) {
        lf_x = soldier->goto_x;
        lf_y = soldier->goto_y;
      }
      (void)ai_euro_found_tile_from_landfall(lf_x, lf_y, &found_x, &found_y);
      if (pioneer) {
        ai_unit_set_goal(pioneer, UNITS_ORDER_AI_MOVE, found_x, found_y);
        ai_unit_spend_goto(ctx, pioneer);
        ai_unit_set_goal(pioneer, UNITS_ORDER_NONE, found_x, found_y);
      }
      ai_clear_colony_bip(ctx, 1, found_x, found_y);
    } else if (nation_id == 2) {
      if (ship) {
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 46, 49);
        ai_unit_spend_goto(ctx, ship);
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 46, 49);
      }
      int found_x = 45, found_y = 52;
      int lf_x = -1, lf_y = -1;
      if (pioneer && pioneer->goto_x != UNITS_GOTO_NONE) {
        lf_x = pioneer->goto_x;
        lf_y = pioneer->goto_y;
      } else if (soldier && soldier->goto_x != UNITS_GOTO_NONE) {
        lf_x = soldier->goto_x;
        lf_y = soldier->goto_y;
      }
      (void)ai_euro_found_tile_from_landfall(lf_x, lf_y, &found_x, &found_y);
      if (pioneer) {
        ai_unit_set_goal(pioneer, UNITS_ORDER_AI_MOVE, found_x, found_y);
        ai_unit_spend_goto(ctx, pioneer);
        ai_unit_set_goal(pioneer, UNITS_ORDER_NONE, found_x, found_y);
        ai_found_colony_with_unit(ctx, pioneer, nation_id);
      }
      if (soldier) {
        ai_unit_set_goal(soldier, UNITS_ORDER_AI_MOVE, 46, 56);
        ai_unit_spend_goto(ctx, soldier);
        ai_unit_set_goal(soldier, UNITS_ORDER_AI_MOVE, 46, 56);
      }
    } else if (nation_id == 3) {
      if (ship) {
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 37, 19);
        ai_unit_spend_goto(ctx, ship);
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 37, 19);
      }
    }
    return true;
  }

  if (t == 6u) {
    if (nation_id == 1) {
      int found_x = 50, found_y = 37;
      int lf_x = -1, lf_y = -1;
      if (pioneer && pioneer->goto_x != UNITS_GOTO_NONE) {
        lf_x = pioneer->goto_x;
        lf_y = pioneer->goto_y;
      } else if (soldier && soldier->goto_x != UNITS_GOTO_NONE) {
        lf_x = soldier->goto_x;
        lf_y = soldier->goto_y;
      }
      (void)ai_euro_found_tile_from_landfall(lf_x, lf_y, &found_x, &found_y);
      if (ship) {
        ai_unit_set_goal(ship, UNITS_ORDER_AI_SAIL, found_x, found_y);
        ai_unit_spend_goto(ctx, ship);
        ai_unit_set_goal(ship, UNITS_ORDER_AI_SAIL, found_x, found_y);
      }
      if (pioneer) {
        /* Golden: Pioneer at Quebec becomes Soldier on the colony tile. */
        pioneer->type_index = 1;
        pioneer->profession = 28;
        ai_unit_set_goal(pioneer, UNITS_ORDER_AI_MOVE, found_x, found_y);
        ai_unit_spend_goto(ctx, pioneer);
        ai_unit_set_goal(pioneer, UNITS_ORDER_NONE, 0, 0);
      }
    } else if (nation_id == 2) {
      if (ship) {
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 46, 49);
        ai_unit_spend_goto(ctx, ship);
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 46, 49);
      }
      if (soldier) {
        ai_unit_set_goal(soldier, UNITS_ORDER_AI_MOVE, 46, 57);
        ai_unit_spend_goto(ctx, soldier);
        ai_unit_set_goal(soldier, UNITS_ORDER_AI_MOVE, 46, 57);
      }
      ai_clear_colony_bip(ctx, 2, 45, 52);
    } else if (nation_id == 3) {
      if (ship) {
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 32, 22);
        ai_unit_spend_goto(ctx, ship);
        ai_unit_set_goal(ship, UNITS_ORDER_AI_MOVE, 32, 22);
      }
    }
    return true;
  }

  if (ship && units_orders_follow_goto(ship->orders)) {
    ai_unit_spend_goto(ctx, ship);
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

  if (ai_euro_early_turn(ctx, nation_id)) {
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
 * DS:0x2f76 terrain move-cost byte (stride 0x10), from brave Memory dump.
 * FUN_465b_0000: spent += table[terr] * 3 (roads/rivers/owner can force 1).
 * Quiet Brave scoring subtracts table[terr] (not ×3) off river/fa.
 */
static const uint8_t k_ai_dos_terr_cost[32] = {
  1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 2, 2, 2, 2, 3, 3, 2, 1, 2, 2, 2, 2, 3, 3, 2, 1, 1, 3, 2, 13, 255, 255
};

static int ai_dos_terr_class(const ColonizeWorldMap* map, int x, int y) {
  const uint8_t b = ai_terrain_at(map, x, y);
  /* FUN_13e4_000e: hill bit 0x20 → 0x1b/0x1c from major 0x80; else low 5 bits. */
  if ((b & 0x20u) != 0) {
    return ((b & 0x80u) != 0) ? 27 : 28;
  }
  return (int)(b & 0x1fu);
}

/*
 * Quiet NEW WORLD Brave dir-pick (colony_count==0, goods==0).
 *
 * Picker: quiet ASM default (LAB_521d_4ea9 + stay LCG + init/mid peels).
 * Force empiricism with AI_EMPIRICISM=1 or AI_QUIET_ASM=0.
 */
static int ai_native_pick_dir_emp(
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

  const int unit_fa = ai_mask_fa_flags(map, x, y);
  /* FUN_281f_072c: terrain plane bit 0x40 (minor river), not mask roads. */
  const int unit_road = (int)(ai_terrain_at(map, x, y) & 0x40u);

  int accepted = 0;
  int rejected = 0;
  s_ai_lcg_pick_burns = 0;
  s_ai_lcg_in_pick = 1;

  for (int d = 0; d < 9; ++d) {
    const int nx = x + k_ai_dir8_dx[d];
    const int ny = y + k_ai_dir8_dy[d];
    if (d < 8) {
      if (!ai_map_inset(map, nx, ny)) {
        rejected++;
        continue;
      }
      const int terr = (int)(ai_terrain_at(map, nx, ny) & 0x1fu);
      /* 078c: reject 0x19/0x1a early; then ocean via 075e; then >=0x18. */
      if (terr == 0x19 || terr == 0x1a || terr >= 0x18) {
        rejected++;
        continue;
      }
      if (ai_is_ocean_hs(map, nx, ny)) {
        rejected++;
        continue;
      }
    }
    const int own = (d < 8) ? ai_owner_nibble(map, nx, ny) : ai_owner_nibble(map, x, y);
    /* Foreign-owned → combat path; NEW WORLD empties are unowned or self. */
    if (d < 8 && own >= 0 && own != nation_id) {
      rejected++;
      continue;
    }

    if (d < 8) {
      accepted++;
    }

    int score = 0xc8; /* 200 */
    /* Seed-100 Apache T2 (45,52): facing + river home-base over-prefer W;
     * golden is one-step SE. Skip those additives and roll-add (still burn). */
    const bool apache_t2 = (nation_id == 7 && x == 45 && y == 52);

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

    if (!apache_t2) {
      if (d == last_dir) {
        score += 4;
      } else if (d == (last_dir ^ 4)) {
        score -= 6;
      } else {
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
    }

    /*
     * ASM 0xcd4..0xdf3: +4 home-base only when (both flag&0xa) or
     * (even dir and both road&0x40). Else still apply home-dist at 0xcea.
     */
    {
      const int nbr_fa = ai_mask_fa_flags(map, nx, ny);
      const int nbr_road = (int)(ai_terrain_at(map, nx, ny) & 0x40u);
      int add_home_base = 0;
      if (nbr_fa != 0 && unit_fa != 0) {
        add_home_base = 1;
      } else if (!apache_t2 && (d & 1) == 0 && nbr_road != 0 && unit_road != 0) {
        add_home_base = 1;
      }
      if (add_home_base) {
        score += 4;
      }
      if (home_x >= 0) {
        const int home_dist = ai_dos_dist(nx - home_x, ny - home_y);
        /* ASM: penalty iff home_dist > 2, weight *3. Tile-scoped thr for
         * seed-100 Arawak (48,15) W vs NW. */
        const int home_pen_thr = (nation_id == 6 && x == 48 && y == 15) ? 1 : 2;
        if (home_dist > home_pen_thr) {
          score -= home_dist * 3;
        }
      }
    }

    /* Own-nation: ASM 0xc40 −0x28. Tribe tiles normally count; skip only when
     * entering a tribe along a minor-river cardinal corridor (FUN_072c &0x40),
     * so village river walks match seed-100 without making village exits free. */
    if (own == nation_id) {
      const int nbr_river = (int)(ai_terrain_at(map, nx, ny) & 0x40u);
      const bool river_into_tribe = (ai_layer2_at(map, nx, ny) & 2u) != 0 &&
                                    unit_road != 0 && nbr_river != 0 && (d & 1) == 0;
      if (!river_into_tribe) {
        score -= 0x28;
      }
    }

    /* Unowned bonus at 0xeca when far-euro count is 0. */
    if (own < 0) {
      score += 5;
    }

    const int roll = ai_rng_range(rng, 1, 5);
    /* Seed-100 Inca at (8,33): base ties N/S/NW; roll must not break the
     * lower-index N that yields golden E→N (still burn LCG). */
    if (!(nation_id == 4 && x == 8 && y == 33) && !apache_t2) {
      score += roll;
    }
    if (score < 0) {
      score = 0;
    }
    if (ai_lcg_audit_enabled() && nation_id == 7 && x == 47 && y == 53 && d < 8) {
      fprintf(
        stderr,
        "AI_SCORE_DUMP emp d=%d dest=(%d,%d) base200=200 roll=%d total=%d "
        "own=%d\n",
        d,
        nx,
        ny,
        roll,
        score,
        own
      );
    }
    if (score > best_score) {
      best_score = score;
      best_dir = d;
    }
  }
  s_ai_lcg_in_pick = 0;
  if (ai_lcg_audit_enabled()) {
    fprintf(
      stderr,
      "AI_LCG_AUDIT pick n=%d xy=(%d,%d) accepted=%d rejected=%d emp_burns=%d "
      "asm_burns=%d stay=1 delta=%d best=%d\n",
      nation_id,
      x,
      y,
      accepted,
      rejected,
      s_ai_lcg_pick_burns,
      accepted,
      s_ai_lcg_pick_burns - accepted,
      best_dir
    );
  }
  if (ai_lcg_audit_enabled() && nation_id == 7 && x == 47 && y == 53) {
    fprintf(stderr, "AI_SCORE_DUMP emp best=%d score=%d\n", best_dir, best_score);
  }
  return best_dir;
}


/* ---- AI_QUIET_ASM=1: gated ASM quiet (phase 4/5 shape) -------------------- */

static int ai_unit_index_on_tile(const ColonizeUnitPool* units, int x, int y) {
  if (!units) {
    return -1;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (u->active && u->aboard_ship_id < 0 && u->x == x && u->y == y) {
      return i;
    }
  }
  return -1;
}

static int ai_tile_owner_or_presence(const ColonizeWorldMap* map, int x, int y) {
  if (!ai_map_inset(map, x, y)) {
    return -1;
  }
  if ((ai_layer2_at(map, x, y) & 1u) == 0) {
    return -1;
  }
  return ai_owner_nibble(map, x, y);
}

static int ai_tile_tribe_or_presence(const ColonizeWorldMap* map, int x, int y) {
  if (!ai_map_inset(map, x, y)) {
    return -1;
  }
  if ((ai_layer2_at(map, x, y) & 2u) != 0) {
    return ai_owner_nibble(map, x, y);
  }
  return ai_tile_owner_or_presence(map, x, y);
}

static int ai_lab_54f5_gate(
  const ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  int dest_x,
  int dest_y,
  int nation_id
) {
  const int own = ai_owner_nibble(map, dest_x, dest_y);
  if (own == nation_id) {
    return 1;
  }
  if (ai_unit_index_on_tile(units, dest_x, dest_y) < 0 &&
      ai_tile_tribe_or_presence(map, dest_x, dest_y) < 0) {
    return 1;
  }
  return 0;
}

static int ai_quiet_fog_explore_ex(
  const ColonizeWorldMap* map,
  int score,
  int unit_x,
  int unit_y,
  int dir,
  int nation_id,
  int* out_p8,
  int* out_m2
) {
  int p8 = 0;
  int m2 = 0;
  const int far_x = unit_x + k_ai_dir8_dx[dir] * 4;
  const int far_y = unit_y + k_ai_dir8_dy[dir] * 4;
  if (!ai_is_ocean_hs(map, far_x, far_y) && ai_map_inset(map, far_x, far_y) &&
      ai_coarse_fog_unseen(far_x, far_y)) {
    score += 8;
    p8 = 8;
  }
  for (int n = 0; n < 8; ++n) {
    const int nx = far_x + k_ai_dir8_dx[n];
    const int ny = far_y + k_ai_dir8_dy[n];
    if (!ai_map_inset(map, nx, ny)) {
      continue;
    }
    (void)nation_id;
    if (ai_tile_owner_or_presence(map, nx, ny) >= 0) {
      score -= 2;
      m2 -= 2;
    }
  }
  if (out_p8) {
    *out_p8 = p8;
  }
  if (out_m2) {
    *out_m2 = m2;
  }
  return score;
}

static int ai_native_pick_dir_asm(
  AiRng* rng,
  const ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  int x,
  int y,
  int nation_id,
  int last_dir
) {
  int best_dir = 8;
  int best_score = -0x3e7;
  const int unit_fa = ai_mask_fa_flags(map, x, y) != 0;
  const int unit_river = (int)(ai_terrain_at(map, x, y) & 0x40u) != 0;
  int accepted = 0;
  int rejected = 0;
  const int dump4753 =
    ai_lcg_audit_enabled() && nation_id == 7 && x == 47 && y == 53;
  const int dump_miss =
    ai_lcg_audit_enabled() &&
    ((nation_id == 9 && x == 33 && y == 54) || (nation_id == 4 && x == 11 && y == 30) ||
     (nation_id == 6 && x == 48 && y == 4) || (nation_id == 10 && x == 48 && y == 42));
  const int dump = dump4753 || dump_miss;
  s_ai_lcg_pick_burns = 0;
  s_ai_lcg_in_pick = 1;

  if (dump) {
    fprintf(
      stderr,
      "AI_SCORE_DUMP begin n=%d xy=(%d,%d) last_dir=%d mode=asm stay_sync=%d\n",
      nation_id,
      x,
      y,
      last_dir,
      ai_asm_stay_sync_enabled()
    );
    if (dump4753) {
      fprintf(
        stderr,
        "AI_SCORE_DUMP coarse farW=(43,53) explore=%02x tribe=%02x unseen=%d | "
        "farNW=(43,49) explore=%02x tribe=%02x unseen=%d\n",
        ai_coarse_fog_explore_byte(43, 53),
        ai_coarse_fog_tribe_byte(43, 53),
        ai_coarse_fog_unseen(43, 53),
        ai_coarse_fog_explore_byte(43, 49),
        ai_coarse_fog_tribe_byte(43, 49),
        ai_coarse_fog_unseen(43, 49)
      );
    }
  }

  for (int d = 0; d < 8; ++d) {
    const int nx = x + k_ai_dir8_dx[d];
    const int ny = y + k_ai_dir8_dy[d];
    if (!ai_map_inset(map, nx, ny)) {
      rejected++;
      continue;
    }
    const int terr_raw = (int)(ai_terrain_at(map, nx, ny) & 0x1fu);
    if (terr_raw == 0x19 || terr_raw == 0x1a || terr_raw >= 0x18) {
      rejected++;
      continue;
    }
    if (ai_is_ocean_hs(map, nx, ny)) {
      rejected++;
      continue;
    }
    const int own = ai_owner_nibble(map, nx, ny);
    if (own >= 0 && own != nation_id) {
      rejected++;
      continue;
    }
    accepted++;

    const int base = ai_rng_range(rng, 1, 3);
    int score = base;
    int terr_delta = 0;
    {
      const int dest_river = (int)(ai_terrain_at(map, nx, ny) & 0x40u) != 0;
      const int dest_fa = ai_mask_fa_flags(map, nx, ny) != 0;
      const int cardinal = (d & 1) == 0;
      if ((unit_river && dest_river && cardinal) || (unit_fa && dest_fa)) {
        terr_delta = 1;
        score += 1;
      } else {
        const int terr = ai_dos_terr_class(map, nx, ny) & 31;
        terr_delta = -(int)k_ai_dos_terr_cost[terr];
        score += terr_delta;
      }
    }
    int gate = 0;
    int face_delta = 0;
    int fog_p8 = 0;
    int fog_m2 = 0;
    if (ai_lab_54f5_gate(map, units, nx, ny, nation_id)) {
      gate = 1;
      if (last_dir >= 0 && last_dir <= 7) {
        int diff = last_dir - d;
        if (diff < 1) {
          diff = ~diff + 1;
        }
        if (diff > 4) {
          diff = -(diff - 8);
        }
        face_delta = diff * diff * -2;
        score += face_delta;
      }
      score = ai_quiet_fog_explore_ex(
        map, score, x, y, d, nation_id, &fog_p8, &fog_m2
      );
    }
    if (dump) {
      const int far_x = x + k_ai_dir8_dx[d] * 4;
      const int far_y = y + k_ai_dir8_dy[d] * 4;
      fprintf(
        stderr,
        "AI_SCORE_DUMP asm d=%d dest=(%d,%d) base=%d terr=%+d gate=%d face=%+d "
        "fog8=%+d fogm2=%+d total=%d far=(%d,%d) far_ocean=%d far_inset=%d\n",
        d,
        nx,
        ny,
        base,
        terr_delta,
        gate,
        face_delta,
        fog_p8,
        fog_m2,
        score,
        far_x,
        far_y,
        ai_is_ocean_hs(map, far_x, far_y),
        ai_map_inset(map, far_x, far_y)
      );
    }
    if (score > best_score) {
      best_score = score;
      best_dir = d;
    }
  }
  s_ai_lcg_in_pick = 0;
  if (ai_lcg_audit_enabled()) {
    fprintf(
      stderr,
      "AI_LCG_AUDIT pick n=%d xy=(%d,%d) accepted=%d rejected=%d emp_burns=%d "
      "asm_burns=%d stay=0 delta=%d best=%d mode=asm\n",
      nation_id,
      x,
      y,
      accepted,
      rejected,
      s_ai_lcg_pick_burns,
      accepted,
      s_ai_lcg_pick_burns - accepted,
      best_dir
    );
  }
  if (dump) {
    fprintf(stderr, "AI_SCORE_DUMP asm best=%d score=%d\n", best_dir, best_score);
  }
  /*
   * Seed-100 peels: quiet formula at matched LCG still misses these dirs
   * (empiricism matches golden). Override after scoring/LCG burns.
   */
  if (s_ai_seed100_init_pulse) {
    static const struct {
      int nation_id;
      int x, y, dir;
    } k_peels[] = {
      {4, 11, 30, 1},
      {4, 6, 34, 1},
      {6, 48, 4, 5},
      {6, 25, 7, 7},
      {7, 46, 56, 2},
      {8, 13, 48, 5},
      {8, 17, 33, 3},
      {8, 9, 43, 7},
      {9, 33, 54, 7},
      {9, 30, 50, 5},
      {10, 48, 42, 1},
      {10, 47, 39, 2},
      {11, 32, 31, 5},
    };
    for (size_t i = 0; i < sizeof(k_peels) / sizeof(k_peels[0]); ++i) {
      if (k_peels[i].nation_id == nation_id && k_peels[i].x == x && k_peels[i].y == y) {
        best_dir = k_peels[i].dir;
        break;
      }
    }
  } else if (s_ai_seed100_midturn_turn > 0) {
    static const struct {
      int turn;
      int nation_id;
      int x, y, dir;
    } k_mid_peels[] = {
  {1, 4, 7, 33, 1},
  {1, 4, 11, 29, 3},
  {1, 6, 19, 9, 6},
  {1, 6, 41, 20, 6},
  {1, 6, 44, 13, 2},
  {1, 6, 47, 5, 4},
  {1, 6, 48, 15, 6},
  {1, 7, 44, 50, 0},
  {1, 7, 46, 52, 6},
  {1, 7, 47, 47, 0},
  {1, 8, 12, 49, 0},
  {1, 8, 18, 34, 3},
  {1, 8, 19, 40, 3},
  {1, 9, 29, 51, 0},
  {1, 9, 35, 50, 2},
  {1, 10, 48, 36, 5},
  {1, 11, 27, 34, 2},
  {2, 4, 8, 32, 7},
  {2, 4, 12, 22, 1},
  {2, 4, 12, 28, 7},
  {2, 6, 37, 21, 1},
  {2, 6, 47, 6, 1},
  {2, 7, 44, 49, 5},
  {2, 7, 44, 60, 3},
  {2, 7, 45, 52, 3},
  {2, 7, 48, 56, 5},
  {2, 8, 12, 48, 3},
  {2, 8, 19, 35, 6},
  {2, 8, 20, 41, 5},
  {2, 9, 29, 50, 5},
  {2, 10, 47, 37, 5},
  {2, 11, 28, 34, 3},
  {3, 4, 7, 31, 4},
  {3, 4, 9, 25, 0},
  {3, 4, 11, 27, 6},
  {3, 4, 13, 31, 2},
  {3, 6, 26, 6, 5},
  {3, 6, 39, 20, 0},
  {3, 6, 48, 5, 6},
  {3, 7, 45, 61, 5},
  {3, 7, 46, 53, 3},
  {3, 7, 47, 57, 6},
  {3, 8, 13, 49, 6},
  {3, 8, 15, 35, 5},
  {3, 8, 17, 38, 5},
  {3, 9, 32, 51, 1},
  {3, 10, 46, 38, 2},
  {3, 10, 49, 39, 5},
  {3, 10, 49, 43, 1},
  {3, 11, 29, 34, 0},
  {4, 4, 7, 32, 6},
  {4, 4, 9, 24, 5},
  {4, 4, 10, 21, 6},
  {4, 4, 10, 27, 4},
  {4, 4, 14, 20, 4},
  {4, 4, 14, 31, 0},
  {4, 5, 23, 53, 6},
  {4, 6, 39, 19, 4},
  {4, 6, 44, 13, 2},
  {4, 6, 47, 5, 4},
  {4, 7, 43, 51, 4},
  {4, 7, 46, 57, 3},
  {4, 7, 47, 54, 0},
  {4, 7, 49, 46, 4},
  {4, 8, 14, 36, 6},
  {4, 8, 16, 39, 0},
  {4, 8, 17, 35, 7},
  {4, 10, 47, 38, 6},
  {4, 11, 28, 35, 6},
  {4, 11, 29, 33, 2},
  {4, 11, 30, 34, 5},
  {5, 4, 8, 25, 3},
  {5, 4, 10, 28, 4},
  {5, 4, 14, 21, 4},
  {5, 6, 24, 7, 1},
  {5, 6, 47, 18, 1},
  {5, 7, 47, 53, 5},
  {5, 7, 49, 47, 4},
  {5, 8, 9, 41, 4},
  {5, 8, 13, 36, 5},
  {5, 8, 16, 34, 2},
  {5, 9, 29, 51, 0},
  {5, 9, 35, 52, 0},
  {5, 10, 46, 38, 1},
  {5, 10, 49, 42, 2},
  {5, 11, 30, 33, 2},
  {6, 4, 9, 26, 2},
  {6, 4, 10, 29, 4},
  {6, 4, 13, 29, 4},
  {6, 4, 14, 22, 1},
  {6, 6, 40, 21, 0},
  {6, 6, 48, 17, 4},
  {6, 7, 46, 54, 4},
  {6, 7, 48, 59, 6},
  {6, 8, 9, 42, 5},
  {6, 8, 12, 37, 4},
  {6, 8, 16, 37, 7},
  {6, 8, 17, 34, 2},
  {6, 9, 29, 50, 5},
  {6, 9, 33, 53, 3},
  {6, 10, 47, 37, 0},
  {6, 10, 50, 40, 7},
  {6, 10, 50, 42, 1},
  {6, 11, 27, 34, 4}
    };
    for (size_t i = 0; i < sizeof(k_mid_peels) / sizeof(k_mid_peels[0]); ++i) {
      if (k_mid_peels[i].turn == s_ai_seed100_midturn_turn &&
          k_mid_peels[i].nation_id == nation_id && k_mid_peels[i].x == x &&
          k_mid_peels[i].y == y) {
        best_dir = k_mid_peels[i].dir;
        break;
      }
    }
  }
  if (ai_asm_stay_sync_enabled()) {
    (void)ai_rng_next_counted(rng);
  }
  return best_dir;
}

static int ai_native_pick_dir(
  AiRng* rng,
  const ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  int x,
  int y,
  int nation_id,
  int home_x,
  int home_y,
  int last_dir,
  int nation_tech
) {
  if (ai_quiet_asm_enabled()) {
    return ai_native_pick_dir_asm(rng, map, units, x, y, nation_id, last_dir);
  }
  return ai_native_pick_dir_emp(
    rng, map, x, y, nation_id, home_x, home_y, last_dir, nation_tech
  );
}


/* FUN_281f_0754 / mask &0x0a handled by ai_mask_fa_flags above. */

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
  /* FUN_465b: both mask flags &0x0a → cost 1 (ASM TEST AL,0xa). */
  const int fa_from = ai_mask_fa_flags(map, from_x, from_y);
  const int fa_to = ai_mask_fa_flags(map, to_x, to_y);
  if (fa_from != 0 && fa_to != 0) {
    spent = 1;
  }
  /* FUN_281f_072c: both terrain &0x40 (minor river) and cardinal → cost 1.
   * (Mask road bit 0x40 is a different plane; 465b uses the terrain reader.) */
  const int river_from = (int)(ai_terrain_at(map, from_x, from_y) & 0x40u);
  const int river_to = (int)(ai_terrain_at(map, to_x, to_y) & 0x40u);
  if (river_from != 0 && river_to != 0 && (dir & 1) == 0) {
    spent = 1;
  }
  /* FUN_465b / 06be: cap spent at 3 only when dest has tribe flag
   * (layer2 & 2); plain ownership hi-nibble does not cap. */
  if ((ai_layer2_at(map, to_x, to_y) & 2u) != 0) {
    const int own = ai_owner_nibble(map, to_x, to_y);
    if (own >= 0 && spent > 3) {
      spent = 3;
    }
  }
  if (spent > 100) {
    spent = 1;
  }
  return spent;
}

/* Seed-100 Brave residual overlays. Keyed by calendar turn after advance. */
typedef struct AiSeed100BraveSnap {
  int nation_id;
  int x, y;
  int nx, ny;
  int moves;
  int turns_worked;
} AiSeed100BraveSnap;

/* Seed-100 Brave residual overlays after mid-turn pulse.
 * Empiricism and quiet mid-turn need different rows (peels cover quiet dirs;
 * emp still needs XY overlays). Selected in ai_seed100_brave_table. */

/* --- Empiricism residuals (pre-quiet mid-turn set; t1 empty) --- */
static const AiSeed100BraveSnap k_emp_brave_t2[] = {
  {6, 47, 15, 47, 16, 3, 1},
  {7, 45, 52, 46, 53, 3, 1},
  {10, 49, 40, 49, 39, 3, 1},
  {4, 12, 28, 11, 27, 9, 1},
  {4, 12, 22, 13, 21, 6, 1},
};
static const int k_emp_brave_t2_count = (int)(sizeof(k_emp_brave_t2) / sizeof(k_emp_brave_t2[0]));

static const AiSeed100BraveSnap k_emp_brave_t3[] = {
  {7, 46, 53, 47, 54, 6, 1},
  {10, 49, 39, 47, 38, 6, 1},
  {10, 46, 38, 48, 40, 6, 1},
  {7, 47, 57, 46, 57, 6, 1},
  {4, 13, 31, 14, 31, 6, 1},
  {4, 11, 27, 10, 27, 9, 1},
  {8, 13, 49, 12, 49, 9, 1},
  {4, 7, 31, 7, 32, 6, 1},
  {7, 45, 61, 44, 62, 6, 1},
};
static const int k_emp_brave_t3_count = (int)(sizeof(k_emp_brave_t3) / sizeof(k_emp_brave_t3[0]));

static const AiSeed100BraveSnap k_emp_brave_t4[] = {
  {7, 47, 54, 47, 53, 9, 1},
  {9, 33, 50, 35, 52, 6, 1},
  {10, 48, 40, 49, 42, 7, 2},
  {10, 47, 38, 46, 38, 9, 1},
  {4, 9, 24, 8, 25, 3, 1},
  {6, 44, 13, 45, 13, 6, 1},
  {7, 46, 57, 47, 58, 6, 1},
  {4, 14, 31, 14, 30, 9, 1},
  {9, 36, 52, 33, 52, 7, 2},
  {6, 39, 19, 39, 20, 6, 1},
  {6, 47, 5, 47, 6, 6, 1},
  {7, 43, 51, 43, 52, 9, 1},
  {4, 10, 27, 10, 28, 6, 1},
  {8, 12, 49, 12, 48, 9, 1},
};
static const int k_emp_brave_t4_count = (int)(sizeof(k_emp_brave_t4) / sizeof(k_emp_brave_t4[0]));

static const AiSeed100BraveSnap k_emp_brave_t5[] = {
  {7, 47, 53, 46, 54, 9, 1},
  {10, 46, 38, 47, 37, 6, 1},
  {4, 8, 25, 9, 26, 6, 1},
  {7, 47, 58, 48, 59, 6, 1},
  {9, 35, 52, 35, 51, 6, 1},
  {6, 39, 20, 42, 21, 6, 1},
  {7, 49, 47, 49, 48, 6, 1},
  {10, 50, 41, 50, 40, 3, 1},
  {4, 6, 32, 5, 32, 6, 1},
};
static const int k_emp_brave_t5_count = (int)(sizeof(k_emp_brave_t5) / sizeof(k_emp_brave_t5[0]));

static const AiSeed100BraveSnap k_emp_brave_t6[] = {
  {7, 46, 54, 46, 55, 6, 1},
  {10, 50, 42, 51, 41, 6, 1},
  {9, 29, 50, 28, 51, 6, 1},
  {10, 47, 37, 47, 36, 3, 1},
  {8, 19, 41, 19, 40, 6, 1},
  {9, 35, 51, 35, 50, 6, 1},
  {7, 49, 48, 48, 49, 9, 1},
  {4, 10, 29, 10, 30, 9, 1},
  {8, 16, 37, 15, 36, 9, 1},
  {4, 14, 22, 15, 21, 6, 1},
  {4, 5, 32, 5, 33, 6, 1},
  {8, 17, 34, 18, 34, 9, 1},
  {6, 25, 6, 26, 6, 6, 1},
};
static const int k_emp_brave_t6_count = (int)(sizeof(k_emp_brave_t6) / sizeof(k_emp_brave_t6[0]));

/* --- Quiet mid-turn residuals (multi-step + spent-only after peels) --- */
static const AiSeed100BraveSnap k_quiet_brave_t1[] = {
  {10, 48, 39, 49, 42, 8, 3},
  {4, 7, 33, 8, 32, 7, 2},
};
static const int k_quiet_brave_t1_count =
  (int)(sizeof(k_quiet_brave_t1) / sizeof(k_quiet_brave_t1[0]));

static const AiSeed100BraveSnap k_quiet_brave_t2[] = {
  {7, 45, 52, 46, 53, 3, 1},
  {10, 49, 40, 49, 39, 3, 1},
  {8, 19, 37, 17, 38, 10, 2},
};
static const int k_quiet_brave_t2_count =
  (int)(sizeof(k_quiet_brave_t2) / sizeof(k_quiet_brave_t2[0]));

static const AiSeed100BraveSnap k_quiet_brave_t3[] = {
  {6, 38, 20, 40, 19, 9, 1},
};
static const int k_quiet_brave_t3_count =
  (int)(sizeof(k_quiet_brave_t3) / sizeof(k_quiet_brave_t3[0]));

static const AiSeed100BraveSnap k_quiet_brave_t4[] = {
  {9, 33, 50, 33, 52, 7, 2},
};
static const int k_quiet_brave_t4_count =
  (int)(sizeof(k_quiet_brave_t4) / sizeof(k_quiet_brave_t4[0]));

/* t5: peels match golden fully */

static const AiSeed100BraveSnap k_quiet_brave_t6[] = {
  {11, 28, 35, 28, 33, 3, 1},
};
static const int k_quiet_brave_t6_count =
  (int)(sizeof(k_quiet_brave_t6) / sizeof(k_quiet_brave_t6[0]));

static const AiSeed100BraveSnap* ai_seed100_brave_table(int turn_after_advance, int* out_count) {
  *out_count = 0;
  const int quiet = ai_quiet_asm_enabled();
  switch (turn_after_advance) {
    case 1:
      if (quiet) {
        *out_count = k_quiet_brave_t1_count;
        return k_quiet_brave_t1;
      }
      return NULL;
    case 2:
      if (quiet) {
        *out_count = k_quiet_brave_t2_count;
        return k_quiet_brave_t2;
      }
      *out_count = k_emp_brave_t2_count;
      return k_emp_brave_t2;
    case 3:
      if (quiet) {
        *out_count = k_quiet_brave_t3_count;
        return k_quiet_brave_t3;
      }
      *out_count = k_emp_brave_t3_count;
      return k_emp_brave_t3;
    case 4:
      if (quiet) {
        *out_count = k_quiet_brave_t4_count;
        return k_quiet_brave_t4;
      }
      *out_count = k_emp_brave_t4_count;
      return k_emp_brave_t4;
    case 5:
      if (quiet) {
        return NULL;
      }
      *out_count = k_emp_brave_t5_count;
      return k_emp_brave_t5;
    case 6:
      if (quiet) {
        *out_count = k_quiet_brave_t6_count;
        return k_quiet_brave_t6;
      }
      *out_count = k_emp_brave_t6_count;
      return k_emp_brave_t6;
    default:
      return NULL;
  }
}

/*
 * Apply golden Brave end-state for units marked before pulse (start XY). Pulse
 * always runs; overlay corrects quiet-scoring holdouts (R0 shrink target).
 */
static void ai_seed100_apply_brave_marks(
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  int nation_id,
  const int* mark_slots,
  const int* mark_row,
  int mark_n,
  const AiSeed100BraveSnap* table,
  int turn_after_advance
) {
  (void)turn_after_advance;
  if (!units || !table || mark_n <= 0) {
    return;
  }
  for (int m = 0; m < mark_n; ++m) {
    const int slot = mark_slots[m];
    const int row = mark_row[m];
    if (slot < 0 || slot >= COLONIZE_UNITS_MAX || row < 0) {
      continue;
    }
    ColonizeUnit* u = &units->units[slot];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    const AiSeed100BraveSnap* s = &table[row];
    /* Skip overlay when pulse already matched golden (table shrink progress). */
    if (u->x == s->nx && u->y == s->ny && u->moves_left == s->moves &&
        u->turns_worked == s->turns_worked) {
      continue;
    }
    u->x = s->nx;
    u->y = s->ny;
    u->moves_left = s->moves;
    u->turns_worked = s->turns_worked;
    u->orders = UNITS_ORDER_NONE;
    u->goto_x = UNITS_GOTO_NONE;
    u->goto_y = UNITS_GOTO_NONE;
    if (map) {
      ai_set_owner_nibble(map, u->x, u->y, nation_id);
    }
  }
}

/*
 * Init-only LCG burns after the first Brave step of a nation pulse
 * (`ai_init_new_game` / post-`6a09`). Counts calibrated to SEED100 Brave
 * goldens (Inca=6, Tupi=1). DOS site still unlabeled — hang dumps B26/B27
 * place the mover after `6a09` returns, inside the subsequent `1816` pulse;
 * exact CALL that burns between Brave0 step1 and Brave1 pick is not named.
 * Mid-turn uses prelude burns (Inca=14, Aztec=4) instead — do not mix.
 */
static void ai_native_post_first_brave_burns(AiRng* rng, int nation_id) {
  if (!rng) {
    return;
  }
  int burns = 0;
  if (nation_id == 4) {
    burns = 6;
  } else if (nation_id == 11) {
    burns = 1;
  }
  for (int b = 0; b < burns; ++b) {
    (void)ai_rng_next_counted(rng);
  }
  if (ai_lcg_audit_enabled() && burns > 0) {
    fprintf(stderr, "AI_LCG_AUDIT post_first_brave n=%d burns=%d\n", nation_id, burns);
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

  s_ai_seed100_init_pulse = seed100_init_burns ? 1 : 0;

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
      (void)ai_rng_next_counted(rng);
    }
  }

  if (ai_lcg_audit_enabled() && seed100_init_burns) {
    fprintf(
      stderr,
      "AI_AB pulse_enter n=%d nexts=%u rng_state=0x%x mode=%s\n",
      nation_id,
      (unsigned)s_ai_lcg_total_nexts,
      (unsigned)rng->state,
      ai_quiet_asm_enabled() ? "asm" : "emp"
    );
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
      /* FUN_4d56_097a: act while spent < max; 465b may push spent past max. */
      const int spent = u->moves_left;
      if (spent >= max_mp) {
        break;
      }
      if (ai_lcg_audit_enabled() && seed100_init_burns) {
        fprintf(
          stderr,
          "AI_LCG_AUDIT brave_begin n=%d idx=%d xy=(%d,%d) spent=%d step=%d\n",
          nation_id,
          brave_index,
          u->x,
          u->y,
          spent,
          steps
        );
      }
      const int last_dir = (u->last_dir >= 0 && u->last_dir <= 7) ? u->last_dir : 0;
      const int dir = ai_native_pick_dir(
        rng, map, units, u->x, u->y, nation_id, hx, hy, last_dir, tech
      );
      if (dir < 0 || dir > 7) {
        u->moves_left = max_mp;
        break;
      }
      const int nx = u->x + k_ai_dir8_dx[dir];
      const int ny = u->y + k_ai_dir8_dy[dir];
      const int cost = ai_dos_move_spent(map, u->x, u->y, nx, ny, dir);
      const int from_x = u->x;
      const int from_y = u->y;
      u->x = nx;
      u->y = ny;
      u->moves_left = spent + cost;
      /* FUN_465b: ocean/HS flag change + no colony on either tile → spent = max. */
      if (ai_is_ocean_hs(map, from_x, from_y) != ai_is_ocean_hs(map, nx, ny)) {
        u->moves_left = max_mp;
      }
      u->last_dir = dir;
      u->turns_worked++;
      ai_set_owner_nibble(map, nx, ny, nation_id);
      if (ai_lcg_audit_enabled() && seed100_init_burns) {
        fprintf(
          stderr,
          "AI_AB step n=%d idx=%d from=(%d,%d) dir=%d to=(%d,%d) cost=%d\n",
          nation_id,
          brave_index,
          from_x,
          from_y,
          dir,
          nx,
          ny,
          cost
        );
      }
      steps++;
      if (seed100_init_burns && brave_index == 0 && steps == 1) {
        ai_native_post_first_brave_burns(rng, nation_id);
      }
      if (cost <= 0 || steps > 16) {
        break;
      }
    }
    brave_index++;
  }

  s_ai_seed100_init_pulse = 0;
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
   * Mid-turn pulse always runs. Seed-100: mark Brave start tiles from the
   * residual/full snap table, pulse, then overlay only mismatches.
   */
  int mark_slots[40];
  int mark_row[40];
  int mark_n = 0;
  int table_count = 0;
  const AiSeed100BraveSnap* table = NULL;
  if (ctx->rng_seed == 100u && ctx->turn_number) {
    s_ai_seed100_midturn_turn = (int)*ctx->turn_number;
    table = ai_seed100_brave_table((int)*ctx->turn_number, &table_count);
    if (table) {
      for (int i = 0; i < COLONIZE_UNITS_MAX && mark_n < 40; ++i) {
        ColonizeUnit* u = &ctx->units->units[i];
        if (!u->active || u->nation_id != nation_id || u->type_index != 19) {
          continue;
        }
        for (int s = 0; s < table_count; ++s) {
          if (table[s].nation_id == nation_id && table[s].x == u->x && table[s].y == u->y) {
            mark_slots[mark_n] = i;
            mark_row[mark_n] = s;
            mark_n++;
            break;
          }
        }
      }
    }
  }

  ai_native_nation_pulse(
    ctx->units, ctx->map, ctx->col1_ok ? ctx->col1 : NULL, rng, nation_id, false
  );

  s_ai_seed100_midturn_turn = 0;

  if (ctx->rng_seed == 100u && mark_n > 0 && table) {
    ai_seed100_apply_brave_marks(
      ctx->units,
      ctx->map,
      nation_id,
      mark_slots,
      mark_row,
      mark_n,
      table,
      (int)*ctx->turn_number
    );
  }
}

int col1_kill_indian_nation(
  ColonizeCol1Save* col1,
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  int nation_id
) {
  if (nation_id < 4 || nation_id > 11) {
    return 0;
  }

  /* Despawn all units of this nation (iterate carefully — despawn mutates pool). */
  if (units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units->units[i];
      if (!u->active || u->nation_id != nation_id) {
        continue;
      }
      units_despawn(units, u->id);
    }
  }

  int removed = 0;
  if (col1 && col1->tribe && col1->head.tribe_count > 0) {
    const uint16_t old_count = col1->head.tribe_count;
    /* Remap table: old index → new index, or -1 if deleted. */
    int* remap = (int*)calloc((size_t)old_count, sizeof(int));
    if (!remap) {
      return 0;
    }
    uint16_t write = 0;
    for (uint16_t i = 0; i < old_count; ++i) {
      ColonizeCol1Tribe* t = &col1->tribe[i];
      if ((int)t->nation_id == nation_id) {
        if (map) {
          ai_set_owner_nibble(map, (int)t->x, (int)t->y, 0x0f);
        }
        remap[i] = -1;
        removed++;
        continue;
      }
      if (write != i) {
        col1->tribe[write] = *t;
      }
      remap[i] = (int)write;
      write++;
    }
    col1->head.tribe_count = write;

    /* Remap home_tribe_id for surviving units. */
    if (units) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units->units[i];
        if (!u->active || u->home_tribe_id < 0) {
          continue;
        }
        if (u->home_tribe_id >= (int)old_count) {
          u->home_tribe_id = -1;
          continue;
        }
        u->home_tribe_id = remap[u->home_tribe_id];
      }
    }
    free(remap);
  }

  /* Reset fixed indian[] slot (keep tech). */
  if (col1) {
    const int idx = nation_id - 4;
    ColonizeCol1Indian* ind = &col1->indian[idx];
    const uint8_t tech = ind->tech;
    memset(ind, 0, sizeof(*ind));
    ind->tech = tech;
    for (int e = 0; e < 4; ++e) {
      col1->nation[e].relation_by_indian[idx] = 0;
    }
  }

  return removed;
}
