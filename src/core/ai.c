#include "core/internal.h"
#include "core/ai.h"
#include "core/combat_strength.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/ai_euro.h"
#include "core/founding_fathers.h"
#include "core/ai_goals.h"
#include "core/ai_king.h"
#include "core/col1_bridge.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/ai_internal.h"
#include "core/dos_rng.h"
#include "core/map_gen.h"
#include "core/new_game.h"
#include "core/strutil.h"
#include "core/turn.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

/*
 * FUN_38fd_6024 (viceroy_unpacked.c ~68666-68677): human starting treasury
 * by difficulty — Discoverer 1000, Explorer 300, Conquistador+ 0. AI nations
 * always start at 0. See docs/difficulty.md.
 */
static uint32_t ai_starting_gold(int difficulty) {
  if (difficulty <= 0) {
    return 1000u;
  }
  if (difficulty == 1) {
    return 300u;
  }
  return 0u;
}

/* 0 is a valid campaign seed when *_set; otherwise 0 means "not provided". */
static uint32_t ai_new_game_seed(const AiNewGameParams* p) {
  if (!p) {
    return 1u;
  }
  if (p->rng_seed_set) {
    return p->rng_seed;
  }
  return p->rng_seed ? p->rng_seed : 1u;
}

static uint32_t ai_turn_seed(const ColonizeTurnContext* ctx) {
  if (!ctx) {
    return 100u;
  }
  if (ctx->rng_seed_set) {
    return ctx->rng_seed;
  }
  return ctx->rng_seed ? ctx->rng_seed : 100u;
}

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

/*
 * DOS coarse fog / tribe-region plane (DS:0x9faa, size 0x10e).
 * Dual index: explore +8 uses (y>>2)+(x>>2)*18 (ASM 521d:56d8); tribe
 * spacing uses (y/5)+(x/5)*18. Not player map.seen / Complete Map.
 */
#define AI_COARSE_FOG_PITCH 0x12
#define AI_COARSE_FOG_SIZE 0x10e
static uint8_t s_ai_coarse_fog[AI_COARSE_FOG_SIZE];

static void ai_coarse_fog_clear(void) {
  memset(s_ai_coarse_fog, 0, sizeof(s_ai_coarse_fog));
}

static int ai_coarse_fog_explore_index(int x, int y) {
  /* DOS: BX=(far_y>>2), SI=(far_x>>2)*18 → [BX+SI+0x9faa]. */
  return (y >> 2) + (x >> 2) * AI_COARSE_FOG_PITCH;
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

/*
 * DS:0x9faa is FUN_521d_0a60's scratch: cleared at every Euro nation's 0a60
 * entry (memset 0x10e) and re-stamped with that nation's units
 * ((y>>2)+(x>>2)*18 |= 1 or 5) and colonies (|= 2). Mapgen (FUN_6a09) writes
 * its tribe marks into the same bytes, so the init pulse sees those, but by
 * the first mid-turn Brave pulse the plane only holds the *last* Euro
 * nation's stamps — the far-probe +8 fires almost everywhere. Linux kept the
 * mapgen tribe marks forever (2026-08-28 fix).
 */
void ai_coarse_fog_euro_restamp(
  const ColonizeUnitPool* units, const ColonizeColonyPool* colonies, int nation_id
) {
  ai_coarse_fog_clear();
  if (units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units->units[i];
      if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0 || u->x >= 200 ||
          u->y >= 200) {
        continue;
      }
      const int ix = ai_coarse_fog_explore_index(u->x, u->y);
      if (ix >= 0 && ix < AI_COARSE_FOG_SIZE) {
        s_ai_coarse_fog[ix] |= 1; /* 0a60: |= 5 (or 0x01 via 0xfc+5) — nonzero either way */
      }
    }
  }
  if (colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      const int ix = ai_coarse_fog_explore_index(c->x, c->y);
      if (ix >= 0 && ix < AI_COARSE_FOG_SIZE) {
        s_ai_coarse_fog[ix] |= 2;
      }
    }
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

/* Public read of the +8 far-probe test (FUN_521d_20e6 raw 88837). */
int ai_coarse_fog_explore_unseen(int x, int y) {
  return ai_coarse_fog_unseen(x, y);
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

/* AI_SCORE_AT="n:x:y[,n:x:y...]" — extra pick_dir score-dump targets. */
static int ai_score_at_match(int nation_id, int x, int y) {
  static char buf[256];
  static int loaded = -1;
  if (loaded < 0) {
    const char* e = getenv("AI_SCORE_AT");
    buf[0] = 0;
    if (e) {
      snprintf(buf, sizeof(buf), "%s", e);
    }
    loaded = 1;
  }
  const char* p = buf;
  while (*p) {
    int n = 0, px = 0, py = 0, consumed = 0;
    if (sscanf(p, "%d:%d:%d%n", &n, &px, &py, &consumed) == 3) {
      if (n == nation_id && px == x && py == y) {
        return 1;
      }
      p += consumed;
    } else {
      break;
    }
    if (*p == ',') {
      p++;
    }
  }
  return 0;
}

/* Set AI_STEP_AUDIT=1 to log mid-turn Brave step paths (phase 13 multi-step). */
static int ai_step_audit_enabled(void) {
  static int cached = -1;
  if (cached < 0) {
    const char* e = getenv("AI_STEP_AUDIT");
    cached = (e && e[0] && e[0] != '0') ? 1 : 0;
  }
  return cached;
}

/* Seed-100 init pulse: peels + select quiet ASM. */
static int s_ai_seed100_init_pulse;
static uint32_t s_ai_init_pulse_seed;
/* Calendar turn after advance during seed-100 mid-turn pulse (0 = not mid-turn). */
static int s_ai_seed100_midturn_turn;

/* AI_PEEL_AUDIT=1: classify each firing peel row against both branch scorers. */
static int ai_peel_audit_enabled(void) {
  static int cached = -1;
  if (cached < 0) {
    const char* e = getenv("AI_PEEL_AUDIT");
    cached = (e && e[0] == '1') ? 1 : 0;
  }
  return cached;
}

static int ai_peel_audit_argmax(const int score[8]) {
  int best = -0x3e7;
  int dir = 8;
  for (int d = 0; d < 8; ++d) {
    if (score[d] > best) {
      best = score[d];
      dir = d;
    }
  }
  return dir;
}

/* AI_NO_BRAVE_PEELS=1: skip seed-100 dir peels (audit how many quiet misses remain). */
static int ai_brave_peels_disabled(void) {
  static int cached = -1;
  if (cached < 0) {
    const char* e = getenv("AI_NO_BRAVE_PEELS");
    cached = (e && e[0] && e[0] != '0') ? 1 : 0;
  }
  return cached;
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
    p->col1->nation[i].gold = (i == human) ? ai_starting_gold(p->difficulty) : 0u;
    p->col1->nation[i].current_crosses = 0u;
    p->col1->nation[i].needed_crosses = (i == human) ? 9u : 8u;
    memset(p->col1->nation[i].euro_relation, 0, sizeof(p->col1->nation[i].euro_relation));
  }
  p->col1->head.difficulty = (uint8_t)(p->difficulty < 0 ? 0 : (p->difficulty > 4 ? 4 : p->difficulty));
  /*
   * DOS new-game REF seed (75c2:360b..3643, diff = DS:0x53a6): the
   * Expeditionary Force exists from the first turn. This is the wizard's
   * actual new-game path — the same seed in game_save_col1_slot only covers
   * the lazy no-col1 save template and never ran for a wizard start, which
   * left every nation's Congress force bars empty (bugs.md).
   */
  {
    const int diff = p->col1->head.difficulty;
    p->col1->head.expeditionary_force[0] = (uint16_t)(8 * diff + 15);
    p->col1->head.expeditionary_force[1] = (uint16_t)(5 * (diff + 1));
    p->col1->head.expeditionary_force[2] = (uint16_t)(3 * diff + 2);
    p->col1->head.expeditionary_force[3] = (uint16_t)(6 * diff + 2);
  }
  p->col1->head.year = 1492;
  p->col1->head.autumn = 0;
  p->col1->head.turn = 0;
  /*
   * FUN_75c2_235c: market_demand_pool[16] = FUN_281f_04d4(600, 1000) each.
   * Without this the EOT market ledger (FUN_38fd_0058 phases 2-3) clamps
   * every group to 1, the target ratio collapses to 3 and Rum..Coats lose a
   * point of price every turn from 1492. A private LCG keeps the campaign
   * RNG stream (map gen / tribe placement goldens) untouched.
   */
  {
    ColonizeDosRng pg_rng;
    dos_rng_seed(&pg_rng, p->rng_seed ? p->rng_seed ^ 0x53eau : 0x53eau);
    for (int c = 0; c < 16; ++c) {
      p->col1->head.market_demand_pool[c] = (uint16_t)dos_rng_range(&pg_rng, 600, 1000);
    }
  }
  /*
   * FUN_75c2_235c (viceroy_unpacked_2.c:112401) writes DS:0x5382 = 0xc600:
   * Indian moves, foreign moves, autosave and combat analysis enabled; fast
   * slide and end-of-turn disabled; water cycling enabled (the stored water
   * bit is an inverted disable flag).
   *
   * Tutorial hints are NOT in that word. The new-game wizard ORs 0x80 in
   * afterwards only when the chosen difficulty is 0 / Discoverer
   * (viceroy_unpacked_2.c:111468).
   */
  p->col1->head.game_options.show_indian_moves = 1;
  p->col1->head.game_options.show_foreign_moves = 1;
  p->col1->head.game_options.fast_piece_slide = 0;
  p->col1->head.game_options.end_of_turn = 0;
  p->col1->head.game_options.autosave = 1;
  p->col1->head.game_options.combat_analysis = 1;
  p->col1->head.game_options.water_color_cycling = 0;
  p->col1->head.game_options.tutorial_hints = (p->col1->head.difficulty == 0) ? 1 : 0;

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
    p->europe->gold = (int)ai_starting_gold(p->difficulty);
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
  /* NEW WORLD: FUN_684c HS-rim landfalls from map_generate. */
  if (map_gen_euro_landfall(p->map, nation, out_x, out_y)) {
    (void)avoid_x;
    (void)avoid_y;
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

  /*
   * DOS-LITERAL FUN_75c2_235c (raw 121612-121646): the new-game bootstrap is
   * path-independent. Whatever the map source — generated, CUSTOMIZE or an
   * AMERICA / TRIBE.TXT scenario — every one of the four fleets is created by
   * FUN_281f_095c(type, nation, nation-0x1c, nation-0x1c), i.e. on the Europe
   * westbound sentinel (228+n, 228+n) with voyage counter +0x315a = 0 ("lands
   * at the next FUN_48d3_03d0 pass"), and the nation's landfall tile
   * (-0x77c6/-0x77c5, seeded from @SCENARIO raw 121035 on a scenario map or
   * from LAB_684c_1b4c's HS-rim walk on a generated one) is copied into the
   * unit's +0x314d/+0x314e (raw 121627) as the arrival target. The human
   * nation's own FUN_48d3_06ba pass then lands his fleet before turn 1; the
   * three AI fleets land at their first tick through the ported 06ba/048e
   * chain. (bugs.md #487/#489/#490)
   */
  (void)p;
  const int sx = 228 + nation;
  const int sy = 228 + nation;
  const int goto_x = landfall_x;
  const int goto_y = landfall_y;

  const int ship_id = units_spawn_euro_starter_fleet(
    units, nation, difficulty, false, sx, sy, goto_x, goto_y
  );
  return ship_id >= 0;
}

/*
 * FUN_4d56_0000 non-capital branch: `2*indian[nation-4].tech + 3`
 * (OVL13 start; confirmed 2026-08-24). Capital branch of that same
 * helper returns `tech+1` instead — not used here because DOS clears
 * +3 flags before create finishes, then ORs capital bit 0x04 only
 * after the worth/create call returns (6a09), so founding always
 * stores the non-capital formula into settlement+4 / population.
 */
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
  t->mission = COL1_TRIBE_MISSION_NONE;
  (*count)++;
  return true;
}

/* VICEROY DS:0xb4 / 0xbe = MAP_DIR8_DX/DY, plus one past-table entry: index
 * 8 reads (0,0) = "stay", which several DOS loops here walk (d < 9) and the
 * quiet Brave picker returns as its stay direction. That 9th slot is why this
 * cannot just be MAP_DIR8_DX (which is [8]). */
static const int k_ai_dir8_dx[9] = {0, 1, 1, 1, 0, -1, -1, -1, 0};
static const int k_ai_dir8_dy[9] = {-1, -1, 0, 1, 1, 1, 0, -1, 0};

static uint8_t ai_layer2_at(const ColonizeWorldMap* map, int x, int y) {
  if (!map || !map->layer2 || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return 0;
  }
  return map->layer2[y * map->width + x];
}

/* FUN_281f_0754 / mask &0x0a: village (0x02) or road (Col1 mask 0x08).
 * 2026-08-27: the road half now reads the real improvement plane
 * (map_tile_has_road) — layer2 bit 0x08 is Linux's rumour-cleared stand-in
 * and bit 0x40 is plowed, neither is a road; reading them here made the
 * river/road "+1" pair term fire on 2 of the 3 parked seed-100 Braves. */
static int ai_mask_fa_flags(const ColonizeWorldMap* map, int x, int y) {
  const uint8_t l2 = ai_layer2_at(map, x, y);
  int fa = (int)(l2 & 0x02u);
  if (map_tile_has_road(map, x, y)) {
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

/*
 * Mover's stamp (FUN_1427_02ca's tail). Same write, minus settlement tiles for
 * native units — bugs.md #416.
 *
 * The nibble on a `layer2 & 2` tile IS the settlement's owner for every reader
 * (FUN_137f_03e4 tile_tribe_owner, and through it the FUN_5bfb_3180 first-
 * contact scan). DOS can stamp unconditionally because a unit never stands on
 * a settlement tile it does not own; the port's Brave walkers do cross them,
 * and one crossing repainted a village for good — after which every Euro unit
 * that stepped beside that village "met" the passing Brave's nation instead
 * (the user's Aztec, playing France). units.c's own two stamp sites
 * (units_set_nation, units_claim_tile_owner_from_stack) already carry exactly
 * this guard; the AI walkers did not.
 */
static void ai_set_owner_nibble_move(ColonizeWorldMap* map, int x, int y, int nation) {
  if (nation > 3 && map && map->layer2 && x >= 0 && y >= 0 && x < map->width &&
      y < map->height) {
    const size_t li = (size_t)y * (size_t)map->width + (size_t)x;
    if (li < map->tile_count && (map->layer2[li] & MAP_OCCUPANCY_HAS_CITY) != 0) {
      return;
    }
  }
  map_set_owner_nibble(map, x, y, nation);
}

/* FUN_137f_01ca / FUN_281f_06b4 — continent ID = layer3 low nibble. */
static int ai_continent_id(const ColonizeWorldMap* map, int x, int y) {
  return (int)(map_get_layer3(map, x, y) & 0x0fu);
}

/* FUN_13e4_0074 / FUN_281f_0768 — ocean or high seas only. */
static int ai_is_ocean_hs(const ColonizeWorldMap* map, int x, int y) {
  const uint8_t t = (uint8_t)(map_get_terrain_or(map, x, y, 25) & 0x1fu);
  return t == 0x19 || t == 0x1a;
}

/*
 * FUN_13e4_003a / FUN_13e4_000e.
 * 2026-08-24: bit-test polarity fixed against the raw `.asm`, not the
 * decompile. `FUN_13e4_000e`'s decompile cites an unresolved
 * `unaff_1000000c` for the class-27-vs-28 branch, ambiguous on its own;
 * direct disassembly (`viceroy_unpacked.asm:7847-7860`) shows that
 * register is just the same input byte re-read into AL (`MOV BX,
 * [BP+..]; TEST BL,0x20; ...; MOV AL,BL; AND AX,0x80; CMP AX,1; SBB
 * DX,DX; AND DX,1; ADD DX,0x1b`): AX==0 (bit 0x80 clear) borrows, giving
 * DX=0x1c; AX==0x80 (bit 0x80 set) does not borrow, giving DX=0x1b. So
 * **bit 0x80 set -> class 0x1b (Mountains)**, bit 0x80 clear -> class
 * 0x1c (Hills) -- matches `map.c`'s independently-derived MAPEDIT
 * convention (`map_byte_is_mountain`: `terrain & 0xa0 == 0xa0`)
 * exactly. The previous ternary here had the two classes swapped (a
 * transcription slip, not a cited DOS behavior); harmless for this
 * function's only caller (`ai_terrain_ok_for_village`, which just tests
 * `typ >= 0x18` and doesn't care which of 0x1b/0x1c it is) but real bit
 * rot for any future 0x1b/0x1c-specific caller — see
 * `ai_place_tribes_procedural`'s hill_silver_bid_bonus tail below, the
 * first caller that actually needs the distinction right.
 */
static int ai_decoded_type(const ColonizeWorldMap* map, int x, int y) {
  if (!map_coords_inset(map, x, y)) {
    return 25;
  }
  const uint8_t t = map_get_terrain_or(map, x, y, 25);
  if (t & 0x20u) {
    return (t & 0x80u) ? 0x1b : 0x1c;
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
    const int d = map_dos_dist(x - (int)tribes[i].x, y - (int)tribes[i].y);
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
  if (!map_coords_inset(map, x, y)) {
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
    int accept = map_coords_inset(map, x, y);
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
      units_set_nation(u, nation_id);
      u->goto_x = 0xFF;
      u->goto_y = 0xFF;
      u->home_tribe_id = tribe_index;
    }
    /* FUN_1427_02ca: OR flag bit0; FUN_137f_0228 nation into continent high nibble. */
    ai_layer2_or(map, ox, oy, 1);
    map_set_owner_nibble(map, ox, oy, nation_id);
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
      if ((map_get_terrain_or(map, x, y, 25) & 0x20u) != 0) {
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
    map_set_owner_nibble(p->map, px, py, indian + 4);
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
        map_set_owner_nibble(p->map, x, y, nation);
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

  /*
   * FUN_6a09_0006 tail (viceroy_unpacked.c:108006-108020), unwired until
   * now: once every capital/satellite tribe for this game is placed
   * (DOS loops `local_cc < *(int*)0x539a`, VICEROY_DS_TRIBE_COUNT, the
   * running total that already includes satellites here -- not
   * capitals only, despite col1_save.h's older summary comment), re-walk
   * every tribe and scan the 5x5 window centered on its OWN tile (not
   * just capitals) for terrain class 0x1b -- confirmed **Mountains**,
   * not Hills, see `ai_decoded_type`'s 2026-08-24 comment above (also
   * thematically right: Silver comes from Mountains in Col1, not
   * Hills). Each Mountain tile found adds the tribe's nation `tech` to
   * that nation's `indian[].hill_silver_bid_bonus` (DOS: plain 16-bit
   * `*(int*)(indian+0xc) += indian.tech`; field kept its original name,
   * not renamed, per project convention against renaming a live field).
   * Bounds are implicit: `ai_decoded_type` already returns 25 (never
   * 0x1b) for any (x,y) outside `map_coords_inset`, matching DOS's own
   * separate `FUN_281f_0302` bounds-gate ahead of the class read.
   */
  for (int i = 0; i < *count; ++i) {
    const ColonizeCol1Tribe* vt = &(*tribes)[i];
    const int nation = (int)vt->nation_id;
    if (nation < 4 || nation > 11) {
      continue;
    }
    ColonizeCol1Indian* vind = &p->col1->indian[nation - 4];
    for (int yy = (int)vt->y - 2; yy <= (int)vt->y + 2; ++yy) {
      for (int xx = (int)vt->x - 2; xx <= (int)vt->x + 2; ++xx) {
        if (ai_decoded_type(map, xx, yy) == 0x1b) {
          const int v = (int)vind->hill_silver_bid_bonus + (int)vind->tech;
          vind->hill_silver_bid_bonus = (int16_t)v;
        }
      }
    }
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
  /* DS:0x54f6 attitude[euro] is field +10 of each record (alarm[4]), so it
   * comes in with the copy below — a new game's records are zeroed. */
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
      units_set_nation(u, human_nation);
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

  ai_goals_reset();
  founding_fathers_reset();
  ai_contact_reset(); /* pending reparations offer + per-tribe cooldowns (#58) */
  ai_euro_reset();
  ai_native_reset();
  turn_reset();
  units_reset_state(); /* goto anti-backtrack shadow, indexed by reused unit_id */

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
    dos_rng_seed(&local_rng, ai_new_game_seed(params));
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
  if (!params->use_tribe_txt && (params->rng_seed || params->rng_seed_set)) {
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
        u->moves = 0;
      }
    }
    const uint32_t pulse_seed = ai_new_game_seed(params);
    s_ai_init_pulse_seed = pulse_seed;
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
  const uint32_t seed = ai_turn_seed(ctx);
  if (ctx->rng) {
    dos_rng_seed(ctx->rng, seed);
  }
}

void ai_euro_nation_turn(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || nation_id < 0 || nation_id >= 4) {
    return;
  }

  /* FUN_521d_6d8e entry: FUN_281f_04ca reseeds from timer word. */
  ai_nation_reseed(ctx);

  /*
   * FUN_521d_6d8e prelude (decomp 93109, 93139-93141): rebuild this
   * nation's planning scratch — DS:0xa0b8 (own colonies flagged
   * "needs colonists") is recounted here every AI turn, and the four
   * FUN_4962_0018 census bytes plus the DS:0x9567 leader trait that
   * FUN_521d_03d0 / 052c read are latched alongside it. Before 2026-09-07
   * this block was all zeroes, which pinned 03d0 on its `return 8`
   * early-out for the whole game.
   */
  ai_goals_plan_scratch_refresh(
    ctx->col1_ok ? ctx->col1 : NULL,
    ctx->colonies,
    nation_id,
    ai_diplo_leader_trait(ctx, nation_id, 1)
  );

  /* FUN_43f7_2244 used to be called here as an AI "peacetime gift"; its DOS
   * caller FUN_281f_0668 is in the HUMAN arm of the year loop (raw 6418),
   * so it moved to turn.c's TURN_PROC_KING tail as the @MERCENARIES offer
   * (ai_king_peacetime_merc_offer, 2026-09-15). */
  /* FUN_3844_00f2 tail: @KINGFRIGATE auto-accept for AI nations. */
  ai_king_frigate_offer(ctx, nation_id);

  if (!ctx->map) {
    return;
  }

  /*
   * FUN_521d_6d8e body: the full Euro dispatcher. The seed-100 early-turn
   * fixture that used to sit in front of this (opt-in via
   * AI_EURO_EARLY_FIXTURE=1, with its own hand-written ship/land unit loop
   * as the fallback) was deleted 2026-09-14 — the bisect aid it provided is
   * superseded by the structural port and every one of its helpers had a
   * live twin here or in ai_euro.c.
   */
  ai_euro_dispatcher_turn(ctx, nation_id);
}

/*
 * Village growth worth-cap (152e: `if (value < worth) local_16 = 2`).
 *
 * 2026-08-24 (T1.15), resolved same day in a follow-up pass: the thunk
 * chain from the 2026-08-24 note above (`4d56:0038`/`4d56:152e` ->
 * near-`CALL 4c54` -> `JMPF 2a1f:0410`) was re-walked with Ghidra
 * headless directly against the raw bytes (not the flattened export's
 * `FUN_41f2_0294` misresolve) and fully confirmed:
 *   - `4d56:0086`'s near `CALL` encodes as `E8 CB 4B` -> target
 *     `0x0089 + 0x4BCB = 0x4C54` (same segment, matches the prior note).
 *   - `4d56:4c54` disassembles as `JMPF 2A1F:0410` (raw operand bytes
 *     `EA 10 04 1F 2A`; Ghidra's rendered `0x2000:a600` display is a
 *     segmented-space rendering artifact, not the real target).
 *   - Raw bytes at `2a1f:0410` (read directly, not via a stale cached
 *     function boundary): `CALLF 210D:0DAB` (`FUN_210d_0dab`, the RTLink
 *     overlay loader) immediately followed by `JMPF 4D56:0000` — a
 *     12-byte RTLink thunk-table entry. So thunk offset `0` in the
 *     `2a1f` slot really does resolve to `FUN_4d56_0000`, confirming the
 *     earlier session's guess. (Table also gives `041c`->`4d56:39ea`,
 *     `0428`->`4d56:3646`, `0434`->`4d56:2154`, consistent with this
 *     project's other already-resolved `2a1f` thunk-table entries.)
 *
 * **The earlier session's own transcription of `FUN_4d56_0000`'s capital
 * arm was the actual bug**, not the callee identification. Decompiling
 * `4d56:0000` fresh gives:
 *   tech = indian[nation_id-4].tech;      // DS:0x5AD6 stride 0x4e, +2
 *   worth = tech*2 + 3;                   // ASM: base in CX
 *   if (capital_flag) worth = tech + worth + 1;   // ASM: ADD CX,AX; INC CX
 * i.e. the capital arm is **`3*tech + 4`**, not `tech+1` as previously
 * transcribed (the earlier read dropped the pre-existing `2*tech+3` base
 * before the `ADD`/`INC`). This resolves the 2026-08-24 contradiction
 * cleanly: seed-100 capitals have `pop == 2*tech+3`, and
 * `3*tech+4 > 2*tech+3` for every `tech >= 0`, so `pop < worth` holds and
 * `growth_accum += pop` fires every turn, exactly matching golden
 * TURN1->2 behavior. The non-capital arm (`2*tech+3`, unused at this
 * call site since 152e's growth block is capital-gated, see
 * `ai_indian_152e_village_growth`) still matches `ai_tribe_initial_pop`.
 *
 * `FUN_4d56_0000` takes `(unused_cs_word, tribe_index)`; the byte-pair
 * it reads (`DS:0x54ec` stride `0x12`, `+2` nation id / `+3 bit0x4`
 * capital flag) is a separate per-tribe worklist table mirroring the
 * settlement record's own `nation_id`/`state.capital` fields already
 * wired in this port — read those directly off `t` instead of porting
 * the redundant lookup table. DOS stores the result through a `byte`
 * local before comparing (`byte bVar5 = FUN_41f2_0294(...)`), so the
 * value is truncated mod 256 same as this project's established
 * "byte-truncate on port" convention elsewhere.
 *
 * `FUN_41f2_0092`/`0294` (nation-score + report UI, prior 2026-08-19..22
 * notes) remains a real function — just never called from 152e/0038.
 * Full trace: `docs/port_plan.md` T1.15.
 */
static int ai_indian_152e_worth_cap(
  const ColonizeTurnContext* ctx,
  const ColonizeCol1Tribe* t
) {
  const ColonizeCol1Indian* ind = &ctx->col1->indian[t->nation_id - 4];
  int tech = ind->tech;
  int worth = tech * 2 + 3;
  if (t->state.capital) {
    worth = tech + worth + 1; /* == 3*tech + 4 */
  }
  return (uint8_t)worth; /* DOS truncates through a `byte` before the compare */
}

/*
 * FUN_281f_095c -> FUN_1427_06b4 (unit CREATE) — de-stubbed 2026-09-06d.
 *
 * Resolved via the standard chain: `FUN_281f_095c` = `FUN_1000_8b4c`
 * (address_mapping.csv), whose overlay body is the two-instruction RTLink
 * thunk `CALLF FUN_1000_1e61; JMPF 0000:4924` -> `FUN_1427_06b4`
 * (viceroy_unpacked.c:7710-7772).
 *
 * **The old stub's premise was wrong**: DOS's `local_4` is not a "cost", it
 * is `param_1` of `1427:06b4` — the *unit type*. 152e builds it as
 * `0x13 + 1*(a musket was spent) + 2*(50 horse-breeding was spent)`, i.e.
 * exactly the four native unit types Linux already knows from
 * `ai_euro.c`'s NAMES map: 0x13 Brave, 0x14 Armed Brave, 0x15 Mtd. Brave,
 * 0x16 Mtd. Warrior. So this branch arms the newborn out of the nation's
 * own musket/horse stock — the muskets/horses were never a "cost paid for
 * nothing", they decide what walks out of the village.
 *
 * `1427:06b4` body, Indian path (`param_2 >= 4`):
 *   - hard cap `unit_count < 0x124` (292) — the `0x543f` human gate and the
 *     `< 300` / `tribe_population_totals < 0xc9` arms only apply to Euro
 *     slots, and the `@NOMOREUNITS` beep (`281f_03fe`) is Euro-only too.
 *   - zeroes the per-unit scratch (`+0x3148/49/4c/50/54/55/5a`), sets
 *     `+0x314b = 0x58` (orders 'X' = fortify-ish default), `+0x3156` =
 *     `DS:0x538e` for natives, `+0x314a` = settlement at (x,y) — which 152e
 *     then overwrites with the *founding* village index anyway.
 *   - `FUN_1427_02ca(idx, x, y)` finally places the unit on the tile (the
 *     `0xff/0xff` writes just above it are pre-placement scratch; Ghidra
 *     drops 02ca's register args).
 *
 * Linux uses `units_spawn_allow_stack` for the placement half (native units
 * routinely stack on their own village tile) and mirrors the DOS pool cap.
 * The branch is still **unreachable today** — its only gate is
 * `t->state.needs_colonist`, whose DOS producer (village CREATE,
 * `FUN_4d56_0038`) is unported, so the bit is always 0. Ported anyway so the
 * arm is correct the day that producer lands (same "wired but not fed"
 * convention as `ai_euro_5d04_compute_flags`).
 */
static int ai_indian_152e_spawn_brave(
  ColonizeTurnContext* ctx,
  const ColonizeCol1Tribe* t,
  int dos_type,
  int tribe_index
) {
  if (!ctx || !ctx->units || !t) {
    return -1;
  }
  /* DOS: `*(int *)0x539c < 0x124` — the native arm's only pool gate. */
  int live = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    if (ctx->units->units[i].active) {
      live++;
    }
  }
  if (live >= 0x124) {
    return -1;
  }
  /* NAMES pool index == DOS type index for 0x13..0x16 (ai_euro.c k[] map). */
  int type_index = dos_type;
  if (type_index < 0x13) {
    type_index = UNITS_KIND_BRAVE;
  }
  if (type_index > 0x16) {
    type_index = UNITS_KIND_MTD_WARRIOR;
  }
  if (type_index >= ctx->units->type_count) {
    type_index = UNITS_KIND_BRAVE; /* small NAMES pools: fall back to plain Brave. */
    if (type_index >= ctx->units->type_count) {
      return -1;
    }
  }
  const int id = units_spawn_allow_stack(ctx->units, type_index, (int)t->x, (int)t->y);
  if (id < 0) {
    return -1;
  }
  ColonizeUnit* u = units_get(ctx->units, id);
  if (!u) {
    return -1;
  }
  u->nation_id = (int)t->nation_id;
  u->home_tribe_id = tribe_index; /* DOS +0x314a, stamped by 152e itself. */
  /*
   * DOS +0x3149 = 0. Careful: for a native unit `moves` carries DOS
   * SPENT-thirds semantics (turn.c:186 sets natives to 0 at every refresh,
   * decomp ~6357), so 0 means "nothing spent" — the fresh Brave is free to
   * act on the turn it is born, which is what DOS does. The old comment here
   * read it with European remaining-MP polarity and claimed the opposite
   * ("created spent, acts next turn"); the code was right, the comment was a
   * trap (smell #55, 2026-09-09).
   */
  u->moves = 0;
  u->col1_counter16 = 0;
  return id;
}

/*
 * The two readers of the settlement mission byte (+5) in this file used to
 * spell "no mission" two different ways (`!= 0xff` in the growth tick,
 * `(int8_t) >= 0` in the threat picker) while the other twelve readers in the
 * port use COL1_TRIBE_MISSION_NONE. Unified here (smell sweep-3 E6).
 *
 * DOS is the sign test: FUN_4d56_152e (viceroy_unpacked.c raw 81472-81476)
 * does `uVar7 = (uint)*(char *)(iVar8 + 5); if (-1 < (int)uVar7) uVar7 &= 0xf;`
 * — it sign-extends the byte FIRST and only masks the nibble when the result
 * is non-negative, so every 0x80..0xff byte (the 0xff sentinel included)
 * reads as "no mission". FUN_4cc6_03f8 is identical (raw 81040 sign-extends
 * into local_e, raw 81090 gates on `-1 < (int)local_e`).
 */
static bool ai_indian_tribe_has_mission(const ColonizeCol1Tribe* t) {
  if (t->mission == COL1_TRIBE_MISSION_NONE) {
    return false;
  }
  return (int)(int8_t)t->mission >= 0;
}

/*
 * The European nation owning the mission, 0..3, or -1 for none.
 *
 * The >= 4 rejection is a port-safety bound over the documented 0..3 domain
 * (col1_save.h:729), NOT DOS: DOS indexes `indian + nibble + 0x36` (raw
 * 81488) and `settlement + nibble*2 + 0xa` (raw 81490) with the raw nibble
 * and would walk straight past the 4-entry arrays on a 4..15 nibble. Every
 * writer in the port honours the domain, so an out-of-range nibble can only
 * come from a corrupt or foreign save; skipping the mission arm is the safe
 * reading of an undefined value, where indexing would be an out-of-bounds
 * write past `euro_relation_accum[4]` (col1_save.h:854) into the
 * neighbouring euro_diplo[] bytes.
 */
static int ai_indian_tribe_mission_nation(const ColonizeCol1Tribe* t) {
  if (!ai_indian_tribe_has_mission(t)) {
    return -1;
  }
  const int nation = (int)(t->mission & COL1_TRIBE_MISSION_NATION_MASK);
  if (nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return -1;
  }
  return nation;
}

/*
 * FUN_281f_0316 -> FUN_4cc6_03f8 (viceroy_unpacked.c:80991): which European
 * nation this settlement feels most threatened by, and how strongly. Ported
 * 2026-08-30 — it was the stub that kept `t->alarm[e]` at zero forever, so
 * settling and developing next to a tribe never raised any alarm and the
 * village map chrome had nothing to show.
 *
 * Two passes:
 *
 *  1. Military pressure. The 20 tiles of DS:0xc8/0xde around the village (the
 *     colony work radius: 8 neighbours, the 4 at ±2 orthogonal, the 8
 *     knight-ish ones). On each in-bounds land tile, sum `type.attack` over
 *     every Euro-owned unit there whose attack is > 1 and which is not a ship
 *     (DOS types 0xd..0x12). Halve inside a native settlement, and halve again
 *     unless the tile is one of the 8 immediate neighbours. Accumulate per
 *     nation.
 *
 *  2. Colonies within distance 7 (FUN_281f_037a = map_dos_dist). Each scores
 *
 *       base  = 2*max(0, pop-6) + min(tribe.tech, pop/2) + min(pop, 6)
 *               + difficulty + ((buildings*c/e - 8) >> 2)
 *       score = (base*2 - d - 1) / (d + 4)
 *
 *     where c/e come from difficulty for a *human* colony only
 *     ({1,2},{3,4},{1,1},{3,2},{2,1}) and are 1/1 for an AI's. Halve when the
 *     colony is on a different continent, add that nation's military pressure,
 *     halve for the French (nation 1 — their standing native-relations bonus),
 *     halve again on FF 16 (Pocahontas). The highest-scoring colony names the
 *     threatening nation.
 *
 *     The `min(..., pop/2)` operand is the tribe's TECH level, not
 *     `capitol_x`: DOS reads `*(byte *)(tribe.nation_id * 0x4e + 0x59a0)`
 *     (raw 81038) and `0x59a0 + 4*0x4e == 0x5ad8 == indian_base(0x5ad6) + 2`,
 *     i.e. `indian[nation_id-4].tech`. Corrected 2026-09-06d; this header kept
 *     claiming capitol_x, plus the argument that "capitol_x is a map column so
 *     it exceeds pop/2 and the term behaves as pop/2" — which is backwards for
 *     tech (0..~5), a genuinely BINDING cap on pop/2 for any colony of pop >= 2.
 *     Do not "restore" pop/2. Smell audit 2026-09-10 D6.
 *
 * Finally the village's own mission rescales the winner: owned by the threat
 * nation → ×3/4 plain, ×1/2 Jesuit; owned by a rival → ×3/2 plain, ×2 Jesuit.
 */
int ai_indian_village_threat_w(
  const ColonizeWorld* w,
  int human_nation,
  int tribe_index,
  int* out_score
) {
  const ColonizeCol1Save* col1 = w->col1;
  const ColonizeWorldMap* map = w->map;
  const ColonizeUnitPool* pool = w->units;
  const ColonizeColonyPool* colonies = w->colonies;

  if (out_score) {
    *out_score = 0;
  }
  if (!col1 || !map || !colonies || !col1->tribe) {
    return -1;
  }
  if (tribe_index < 0 || tribe_index >= (int)col1->head.tribe_count) {
    return -1;
  }
  const ColonizeCol1Tribe* t = &col1->tribe[tribe_index];

  /* DS:0xc8 / DS:0xde — the 20-tile ring the threat scan walks. */

  const int vx = (int)t->x;
  const int vy = (int)t->y;
  const int village_continent = map_continent_id_at(map, vx, vy);

  int pressure[4] = {0, 0, 0, 0};
  if (pool) {
    /*
     * One pass over the unit pool bucketed into the ring, rather than 20
     * passes: this runs per visible village per frame for the map chrome.
     * `first` is the tile's leading unit whatever its nation — DOS reads the
     * head of the tile's unit list and abandons the tile when it is not a
     * European's (`(unit.nation & 0xf) < 4`).
     */
    signed char in_ring[5][5];
    memset(in_ring, 0, sizeof(in_ring));
    for (int i = 0; i < 20; ++i) {
      in_ring[MAP_RING20_DY[i] + 2][MAP_RING20_DX[i] + 2] = 1;
    }
    int first[5][5];
    int score[5][5];
    for (int ry = 0; ry < 5; ++ry) {
      for (int rx = 0; rx < 5; ++rx) {
        first[ry][rx] = -1;
        score[ry][rx] = 0;
      }
    }
    for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
      const ColonizeUnit* u = &pool->units[ui];
      if (!u->active || !units_is_on_map(u)) {
        continue;
      }
      const int rx = u->x - vx + 2;
      const int ry = u->y - vy + 2;
      if (rx < 0 || rx > 4 || ry < 0 || ry > 4 || !in_ring[ry][rx]) {
        continue;
      }
      if (first[ry][rx] < 0) {
        first[ry][rx] = u->nation_id;
      }
      if (u->nation_id < 0 || u->nation_id > 3 || units_is_sea(pool, u->id)) {
        continue;
      }
      const ColonizeUnitType* ty_def = units_type(pool, u->type_index);
      if (ty_def && ty_def->attack > 1) {
        score[ry][rx] += ty_def->attack;
      }
    }
    for (int i = 0; i < 20; ++i) {
      const int rx = MAP_RING20_DX[i] + 2;
      const int ry = MAP_RING20_DY[i] + 2;
      const int owner = first[ry][rx];
      int s = score[ry][rx];
      if (owner < 0 || owner > 3 || s <= 0) {
        continue;
      }
      const int tx = vx + MAP_RING20_DX[i];
      const int ty = vy + MAP_RING20_DY[i];
      if (!map_coords_inset(map, tx, ty) || map_tile_is_water(map, tx, ty)) {
        continue;
      }
      if (map_tile_tribe_or_presence(map, tx, ty) >= 0) {
        s >>= 1;
      }
      const int adx = MAP_RING20_DX[i] < 0 ? -MAP_RING20_DX[i] : MAP_RING20_DX[i];
      const int ady = MAP_RING20_DY[i] < 0 ? -MAP_RING20_DY[i] : MAP_RING20_DY[i];
      if (adx >= 2 || ady >= 2) {
        s >>= 1;
      }
      pressure[owner] += s;
    }
  }

  const int indian_idx = (int)t->nation_id - 4;
  /*
   * 2026-09-06d: DOS reads `*(byte *)(tribe.nation_id * 0x4e + 0x59a0)`.
   * `0x59a0 + 4*0x4e == 0x5ad8 == indian_base(0x5ad6) + 2`, so the operand is
   * the tribe's **tech level**, not `capitol_x` (+0). The old name/mapping
   * happened to read the neighbouring field of the same record. Cite
   * viceroy_unpacked.c:81038 + `ai_indian_152e_worth_cap`'s own DS note.
   */
  const int tribe_tech = (indian_idx >= 0 && indian_idx < 8)
    ? (int)col1->indian[indian_idx].tech
    : 0;
  const int difficulty = (int)col1->head.difficulty;

  int best_score = 0;
  int best_nation = -1;
  for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
    const ColonizeColony* c = &colonies->colonies[ci];
    if (!c->active || c->nation_id < 0 || c->nation_id > 3) {
      continue;
    }
    const int d = map_dos_dist(vx - c->x, vy - c->y);
    if (d >= 7) {
      continue;
    }
    const int human =
      (human_nation >= 0 && human_nation <= 3) ? (c->nation_id == human_nation) : 0;
    int mul = 1;
    int div = 1;
    int diff_term = 0;
    if (human) {
      diff_term = difficulty;
      switch (difficulty) {
        case 0: mul = 1; div = 2; break;
        case 1: mul = 3; div = 4; break;
        case 2: mul = 1; div = 1; break;
        case 3: mul = 3; div = 2; break;
        default: mul = 2; div = 1; break;
      }
    }
    int buildings = 0;
    for (int b = 0; b < COLONIZE_BUILDING_TYPES_MAX; ++b) {
      if (c->has_building[b]) {
        buildings++;
      }
    }
    buildings = (buildings * mul) / (div > 0 ? div : 1);

    const int pop = c->colonist_count > 0 ? c->colonist_count : c->population;
    const int capped_pop = pop > 6 ? 6 : pop;
    int cap_term = pop >> 1;
    if (tribe_tech < cap_term) {
      cap_term = tribe_tech;
    }
    const int base =
      (capped_pop - pop) * -2 + cap_term + capped_pop + diff_term + ((buildings - 8) >> 2);
    int score = ((base * 2 - d) - 1) / (d + 4);

    if (map_continent_id_at(map, c->x, c->y) != village_continent) {
      score >>= 1;
    }
    score += pressure[c->nation_id];
    if (c->nation_id == 1) {
      score >>= 1; /* French: half the native alarm everyone else earns */
    }
    /*
     * FUN_281f_07b4(nation, 0x10) = FF 16 Pocahontas — de-stubbed 2026-09-06d
     * (see ai_indian_152e_ff_bit). This IS the PEDIA "all Indian alarm is
     * generated half as fast" clause: it halves the threat score that feeds
     * `euro_relation_accum`, which is the only DOS producer of Indian alarm.
     */
    if (founding_fathers_nation_has(col1, c->nation_id, FF_POCAHONTAS)) {
      score >>= 1;
    }
    if (score > best_score) {
      best_score = score;
      best_nation = c->nation_id;
    }
  }

  if (best_score <= 0) {
    return -1;
  }
  /*
   * The nibble is only compared here, never used as an index, so this arm
   * keeps DOS's raw compare (an out-of-domain nibble takes the "rival"
   * branch, as in FUN_4cc6_03f8) instead of ai_indian_tribe_mission_nation's
   * port-safety bound.
   */
  if (ai_indian_tribe_has_mission(t)) {
    const int jesuit = (t->mission & COL1_TRIBE_MISSION_JESUIT_BIT) != 0;
    if ((int)(t->mission & COL1_TRIBE_MISSION_NATION_MASK) == best_nation) {
      best_score = jesuit ? (best_score >> 1) : (best_score - (best_score >> 2));
    } else {
      best_score = jesuit ? (best_score << 1) : (best_score + (best_score >> 1));
    }
  }
  if (out_score) {
    *out_score = best_score;
  }
  return best_nation;
}


/* ColonizeTurnContext adapter for the village tick. */
static int ai_indian_152e_best_threat_nation(
  const ColonizeTurnContext* ctx,
  int tribe_index,
  int* out_score
) {
  if (out_score) {
    *out_score = 0;
  }
  if (!ctx || !ctx->col1_ok) {
    return -1;
  }
  return ai_indian_village_threat_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1), .col1_ok=((ctx->col1) != NULL)}, ctx->human_nation, tribe_index, out_score);
}

/*
 * FUN_281f_07b4 -> FUN_15eb_3960 — de-stubbed 2026-09-06d.
 *
 * `FUN_281f_07b4` = `FUN_1000_89a4` = RTLink thunk `CALLF FUN_1000_1e61;
 * JMPF 0000:9810` -> `FUN_15eb_3960` (address_mapping.csv). Its whole body
 * (viceroy_unpacked.c:13832-13844) is:
 *
 *   if (ff < 0) return 1;                       // "no gate" sentinel
 *   if (nation > 3) return 0;                   // natives own no Fathers
 *   return bitmap[nation*0x13c + (ff>>3)] & (1 << (ff & 7));
 *
 * i.e. it is plainly the **Founding-Father ownership bit test** (per-nation
 * player record, stride 0x13c) — not an unidentified "feature" flag. The
 * project already reads the same table by index elsewhere (FF 0x13 Franklin
 * in the 153e treaty timer, FF 0xd Drake in `157e_004a`, FF 7 de Soto in
 * `13f1_02f8`), so `founding_fathers_nation_has` is the exact counterpart.
 *
 * 152e's two indices resolve semantically, not just numerically:
 *   0x18 = 24 Bartolome de las Casas -> mission goodwill DOUBLES,
 *   0x17 = 23 Juan de Sepulveda      -> mission goodwill HALVES.
 * (Applied in that order, so a nation holding both nets ×1.) Both readings
 * match the PEDIA text in docs/founding_fathers.md: Las Casas assimilates
 * converts, Sepulveda subjugates them.
 */
static bool ai_indian_152e_ff_bit(
  const ColonizeTurnContext* ctx,
  int euro_nation,
  int ff_index
) {
  if (ff_index < 0) {
    return true; /* DOS returns 1 for a negative index. */
  }
  if (!ctx || !ctx->col1_ok || !ctx->col1 || euro_nation < 0 || euro_nation > 3) {
    return false;
  }
  return founding_fathers_nation_has(ctx->col1, euro_nation, ff_index);
}

/*
 * FUN_4d56_152e | ai_indian_152e_village_growth — full port.
 *
 * Own control flow ported in full (raw decomp viceroy_unpacked.c:81387-
 * 81534, ~156 lines). **2026-09-06d: no callee stubs are left** — the last
 * two (`FUN_281f_095c` spawn, `FUN_281f_07b4` FF bit) were resolved through
 * the `FUN_281f_X -> FUN_1000_X -> JMPF 0000:Y -> address_mapping.csv`
 * chain and ported; see each helper's own header above.
 *
 * Field mapping (DOS 0x8d4a "settlement record" == this tribe;
 * DOS 0x8d4e "indian state" == col1->indian[nation_id-4]):
 *   +3 bit0 "needs first colonist"     -> t->state.needs_colonist (new bit,
 *                                          no Linux producer yet, see its
 *                                          own comment in col1_save.h).
 *   +3 bit4 "capital-class multiplier" -> t->state.capital.
 *   +4 "worth"                         -> t->population (existing T0 map).
 *   +5 "owner_flags" (nibble = euro nation with a mission here / -1 none,
 *      bit0x10 = Jesuit)               -> t->mission (exact match, see
 *                                          ColonizeCol1Tribe.mission comment).
 *   +6 "growth accumulator"            -> t->growth_accum.
 *   +10..+16 "attitude[euro]"          -> t->alarm[euro].friction.
 *   indian +7 "musket throttle"        -> ind->muskets.
 *   indian +10 "horse breeding"        -> ind->horse_breeding.
 *   indian +0x36 "euro_relation_accum" -> ind->euro_relation_accum[euro].
 *   FUN_281f_0a38 bit 0x20 "met"       -> ind->euro_diplo & COL1_INDIAN_MET_BIT.
 *   FUN_281f_030c relation get         -> ai_diplo_indian_relation.
 *   FUN_281f_0d6c relation-delta spill -> ai_diplo_indian_relation_delta;
 *                                          the DOS reason code (0/3/5, likely
 *                                          selects a dialog) is dropped —
 *                                          only the sign of the delta is
 *                                          kept, dialog chrome PARKED.
 *   FUN_281f_04d4 RNG range            -> dos_rng_range.
 *   DS:0x5382 bit0 (WoI)               -> ai_king_independence_declared.
 */
static void ai_indian_152e_village_growth(
  ColonizeTurnContext* ctx,
  int tribe_index,
  int nation_id,
  AiRng* rng
) {
  ColonizeCol1Save* col1 = ctx->col1;
  ColonizeCol1Tribe* t = &col1->tribe[tribe_index];
  ColonizeCol1Indian* ind = &col1->indian[nation_id - 4];

  /*
   * Capital-only growth gate, regression fix 2026-08-19 (golden_ai_turns
   * TURN1->2 tribe[8] pop/acc mismatch). The 2026-08-18 structural rewrite
   * dropped the prior "FUN_4d56_152e grows capitals only (state.capital)"
   * check when it replaced the flat `population < 15` cap with the
   * worth-cap callee — both now unconditionally reachable for satellite
   * villages too, making them accumulate growth every turn.
   * Real DOS TURN1.SAV/TURN2.SAV (seed-100) show satellite tribes (non-
   * capital) with growth_accum frozen at 0 across the turn while capital
   * tribes accrue normally. Now that `ai_indian_152e_worth_cap` is ported
   * for real (see its own 2026-08-24 header, `FUN_4d56_0000`'s
   * `2*tech+3` / `3*tech+4` formula), this gate is still kept — it isn't
   * redundant: `ai_indian_152e_worth_cap`'s non-capital arm (`2*tech+3`)
   * is never exercised at this call site regardless, since the block
   * below only runs for capitals; removing this gate would just start
   * calling the capital-arm formula for satellites too, which is wrong.
   * Keep the known-good capital-only restriction.
   * Scoped to this block only — the friction-roll / mission-relation tail
   * below still runs per-settlement (every tribe, satellites included);
   * an earlier version of this fix gated the whole function on capital and
   * silently dropped satellites' RNG draws there too, desyncing the LCG
   * stream and breaking TURN6->7 relation_by_indian (94 vs golden 96).
   */
  if (t->state.capital) {
    int local_16 = 0;
    if ((int)t->population < ai_indian_152e_worth_cap(ctx, t)) {
      local_16 = 2;
    }
    if (t->state.needs_colonist) {
      local_16 = 1;
    }
    if (local_16 != 0) {
      const int acc = (int)t->growth_accum + (int)t->population;
      if (acc > AI_VILLAGE_GROWTH_THRESHOLD) {
        t->growth_accum = 0;
        if (local_16 == 2) {
          t->population++;
        } else {
          /*
           * local_16 == 1: the village hands out its founding Brave. DOS's
           * `local_4` is the *unit type* (0x13 Brave), armed up out of the
           * nation's own stock: +1 for a musket (spent on a 1-in-difficulty
           * roll), +2 for 50 horse-breeding — 0x16 "Mtd. Warrior" when both.
           * (2026-09-06d: it was mis-transcribed as a "cost".)
           */
          int dos_type = UNITS_KIND_BRAVE;
          if ((int8_t)ind->muskets > 0) { /* DOS reads +7 as a signed byte. */
            const int roll = dos_rng_range(rng, 0, (int)col1->head.difficulty);
            if (roll == 0) {
              ind->muskets--;
            }
            dos_type++;
          }
          if (ind->horse_breeding > 0x31) {
            ind->horse_breeding -= 0x32;
            dos_type += 2;
          }
          const int spawned = ai_indian_152e_spawn_brave(ctx, t, dos_type, tribe_index);
          if (spawned >= 0) {
            t->state.needs_colonist = 0;
          }
        }
      } else {
        t->growth_accum = (uint8_t)acc;
      }
    }
  }

  /* Friction-roll loop, gated on !WoI (DS:0x5382 bit0). */
  if (!ai_king_independence_declared(col1)) {
    for (int e = 0; e < 4; ++e) {
      /* raw 81454: FUN_281f_0a38(e, *0x8d50) = FUN_15b3_0004(e, tribe+4), i.e.
       * the EURO-side byte nation[e].relation_by_indian[n-4], not the
       * Indian-side euro_diplo[e]. Same bit, other quadrant (2026-09-17). */
      if (!(ai_diplo_read(col1, e, nation_id) & AI_DIPLO_MET)) {
        continue;
      }
      const int alarm = ai_diplo_indian_alarm(col1, nation_id, e); /* FUN_281f_030c */
      const int quartile = ai_relation_quartile(alarm) /* FUN_281f_0a60 -> FUN_15dc_00a2 */;
      const int iters = quartile * quartile + 1;
      const int hi = 0xc - quartile * quartile;
      int gain = 0;
      for (int k = 0; k < iters; ++k) {
        if (dos_rng_range(rng, 0, hi) == 0) {
          gain++;
        }
      }
      ind->euro_relation_accum[e] = (int8_t)(ind->euro_relation_accum[e] + gain);
    }
  }

  int threat_score = 0;
  const int threat_nation = ai_indian_152e_best_threat_nation(ctx, tribe_index, &threat_score);
  /* 0..3 or -1; indexes euro_relation_accum[]/col1_tribe_attitude below. */
  const int mission_nation = ai_indian_tribe_mission_nation(t);

  if (mission_nation >= 0 || threat_nation >= 0) {
    const bool capital_mult = t->state.capital != 0;
    /* The upper bound is redundant (ai_indian_tribe_mission_nation already
     * rejects >= COLONIZE_COL1_NATION_COUNT) but gcc cannot see that through
     * the inline and warns about euro_relation_accum[]. */
    if (mission_nation >= 0 && mission_nation < (int)COLONIZE_COL1_NATION_COUNT) {
      const bool jesuit = (t->mission & COL1_TRIBE_MISSION_JESUIT_BIT) != 0;
      int local_8 = (jesuit ? 4 : 1) << (capital_mult ? 1 : 0);
      if (ai_indian_152e_ff_bit(ctx, mission_nation, 0x18)) {
        local_8 <<= 1;
      }
      if (ai_indian_152e_ff_bit(ctx, mission_nation, 0x17)) {
        local_8 >>= 1;
      }
      ind->euro_relation_accum[mission_nation] =
        (int8_t)(ind->euro_relation_accum[mission_nation] + local_8);
      /*
       * DOS reads/writes settlement+0xa+e*2 as a whole signed int16 here too
       * (raw 81490-81496: `*piVar1 = *piVar1 + local_8 * -3;` then clamp at
       * 0) — same word the threat arm below maintains. Touching only the low
       * `friction` byte made the mission relief a no-op once the word passed
       * 255: friction bottomed at 0 while the attacks high byte held the
       * value up. Fixed 2026-09-09 (smell #45).
       */
      int atti = col1_tribe_attitude(t, mission_nation) + local_8 * -3;
      if (atti < 0) {
        atti = 0;
      }
      col1_tribe_attitude_set(t, mission_nation, atti);
    }
    if (threat_nation >= 0) {
      int local_c = threat_score << (capital_mult ? 1 : 0);
      ind->euro_relation_accum[threat_nation] =
        (int8_t)(ind->euro_relation_accum[threat_nation] - local_c);
      if (threat_nation == mission_nation) {
        local_c >>= 1;
      }
      const int alarm = ai_diplo_indian_alarm(col1, nation_id, threat_nation);
      /* DOS keeps settlement+0xa+e*2 as an int16 (friction | attacks<<8), so
       * the bump carries out of the low byte instead of wrapping in it. */
      int w = (int)t->alarm[threat_nation].friction |
              ((int)t->alarm[threat_nation].attacks << 8);
      w += local_c + alarm / 5;
      if (w > 0x7fff) {
        w = 0x7fff;
      }
      t->alarm[threat_nation].friction = (uint8_t)(w & 0xff);
      t->alarm[threat_nation].attacks = (uint8_t)((w >> 8) & 0xff);
    }
    /*
     * DOS spends the accumulator through FUN_281f_0d6c (raw 80240/80247/
     * 80255), and 0d6c is a bare thunk to FUN_4cc6_00f2 — the WHOLE of it,
     * escalation tail included. 0x5b1c is written nowhere else in any
     * decompiled export, so there is no "bare writer" in DOS: the Linux
     * split into ai_diplo_indian_alarm_delta (first half) and
     * ai_contact_alarm_delta_00f2 (+ tail) is a port artifact, and this —
     * DOS's sole alarm-growth channel — must take the full function or the
     * alarm-100 mission burn can never fire from it. Fixed 2026-09-09
     * (smell #46).
     */
    if (mission_nation >= 0) {
      while (ind->euro_relation_accum[mission_nation] > 7) {
        ind->euro_relation_accum[mission_nation] -= 8;
        ai_contact_alarm_delta_00f2(ctx, nation_id, mission_nation, -1); /* 4cc6_00f2 */
      }
    }
    if (threat_nation >= 0) {
      while (ind->euro_relation_accum[threat_nation] < -7) {
        ind->euro_relation_accum[threat_nation] += 8;
        ai_contact_alarm_delta_00f2(ctx, nation_id, threat_nation, 1);
      }
    }
  }

  for (int e = 0; e < 4; ++e) {
    while (ind->euro_relation_accum[e] > 7) {
      ind->euro_relation_accum[e] -= 8;
      ai_contact_alarm_delta_00f2(ctx, nation_id, e, -1);
    }
  }
}

COLONIZE_INTERNAL int ai_021a_trace_enabled(void);

static void ai_grow_villages(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe) {
    return;
  }
  AiRng local;
  AiRng* rng = ctx->rng;
  if (!rng) {
    const uint32_t seed = ai_turn_seed(ctx);
    dos_rng_seed(&local, seed);
    rng = &local;
  }
  for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
    ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    const uint32_t before = rng->state;
    ai_indian_152e_village_growth(ctx, (int)i, nation_id, rng);
    if (ai_021a_trace_enabled()) {
      int draws = 0;
      AiRng probe;
      probe.state = before;
      while (draws < 1000 && probe.state != rng->state) {
        (void)dos_rng_next(&probe);
        draws++;
      }
      fprintf(
        stderr, "AI_152E_DRAWS n=%d tribe=%u xy=(%u,%u) pop=%u draws=%d met=%d%d%d%d\n", nation_id,
        (unsigned)i, t->x, t->y, t->population, draws,
        (ctx->col1->indian[nation_id - 4].euro_diplo[0] & 0x20) != 0,
        (ctx->col1->indian[nation_id - 4].euro_diplo[1] & 0x20) != 0,
        (ctx->col1->indian[nation_id - 4].euro_diplo[2] & 0x20) != 0,
        (ctx->col1->indian[nation_id - 4].euro_diplo[3] & 0x20) != 0
      );
    }
  }
}

static int ai_owner_nibble(const ColonizeWorldMap* map, int x, int y) {
  if (!map_coords_inset(map, x, y)) {
    return -1;
  }
  const int hi = (int)((map_get_layer3(map, x, y) >> 4) & 0x0fu);
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
    const int d = map_dos_dist(u->x - (int)t->x, u->y - (int)t->y);
    if (d < best) {
      best = d;
      *out_x = (int)t->x;
      *out_y = (int)t->y;
    }
  }
}

/* terr_cost table + class: map_dos_terr_cost_byte / map_dos_terr_class_at. */

static int ai_dos_terr_class(const ColonizeWorldMap* map, int x, int y) {
  return map_dos_terr_class_at(map, x, y);
}

/* ---- Quiet ASM Brave picker (phase 4/5 shape). The AI_EMPIRICISM /
 * AI_QUIET_ASM golden-curve-fit alternative was deleted 2026-09-14. ------- */

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
  /* map_tile_tribe_or_presence takes plain bounds (DOS FUN_281f_0682); the
   * ai.c copy this replaced gated on FUN_137f_000a inset, so keep that here. */
  if (ai_unit_index_on_tile(units, dest_x, dest_y) < 0 &&
      (!map_coords_inset(map, dest_x, dest_y) ||
       map_tile_tribe_or_presence(map, dest_x, dest_y) < 0)) {
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
  if (!ai_is_ocean_hs(map, far_x, far_y) && map_coords_inset(map, far_x, far_y) &&
      ai_coarse_fog_unseen(far_x, far_y)) {
    score += 8;
    p8 = 8;
  }
  for (int n = 0; n < 8; ++n) {
    const int nx = far_x + k_ai_dir8_dx[n];
    const int ny = far_y + k_ai_dir8_dy[n];
    if (!map_coords_inset(map, nx, ny)) {
      continue;
    }
    (void)nation_id;
    if (map_tile_owner_or_presence(map, nx, ny) >= 0) {
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

/*
 * LAB_521d_52aa — foreign-Euro "attack pull" arm of the quiet dir loop
 * (T1.9, wired 2026-08-27; trace: original_sources_annotated/ai/
 * quiet_brave_scoring.c `quiet_score_colony_pull`). Reached for a candidate
 * tile owned by a Euro nation (owner < 4, != mover) when the mover's Indian
 * side is not at PEACE with that owner (FUN_281f_0a38 & 0x40 on the 23000
 * Indian table = indian[].euro_diplo) — or either unit is a Privateer — and
 * (not WoI, or the owner is the human / not a Euro slot). The mover needs a
 * nonzero @UNIT attack byte (DS:0x5236; Braves = 1).
 *
 *   e8 = ((field0 + 1) / max(1, field2)) * strength / max(1, cost)
 *        field0 = Σ cost over the mover's own stack (1 for a lone Brave),
 *        field2 = # military land {1,4,6,7,8,9} in that stack (0 -> 1),
 *        strength = FUN_5fef_1b0e attack strength vs the tile
 *   *3 if a Euro colony sits there; <<1 if a native village does;
 *   Missionary (0xb) with neither -> 0; crown nation at home (0x8db8==0)
 *   with neither -> >>1; (@UNIT flag 0x10 && G-table stance 4) -> *3 —
 *   the stance table is Euro-only in Linux (ai_euro_continent_stance_at
 *   returns 0 for nations >= 4), so that last term never fires for Braves.
 *   clamp: >999 or <0 -> 1000; then score -= 999 when e8 < 12 for a land
 *   unit, else score += max(1, e8) * 4.
 *
 * The col1/colonies pointers come from the nation-turn entry (module
 * statics — the picker's signature is shared with the fixture path).
 */
static const ColonizeColonyPool* s_ai_native_colonies = NULL;
static const ColonizeCol1Save* s_ai_native_col1 = NULL;
static int s_ai_native_home_dist = 0; /* DS:0x8db8 for the unit being scored */

static int ai_native_foreign_euro_pull_open(
  const ColonizeWorldMap* map, const ColonizeUnitPool* units, int x, int y, int nation_id,
  int dest_x, int dest_y, int owner
) {
  if (owner < 0 || owner > 3 || owner == nation_id || !s_ai_native_col1) {
    return 0;
  }
  /*
   * 2026-08-27, later: seed-100 TURN2->3 golden (Aztec Brave at (47,15) next
   * to an unmet Dutch unit at (47,14)) shows DOS does NOT pull a quiet Brave
   * toward a foreign Euro tile — the arm belongs to the Euro land path of
   * 20e6, and the Brave quiet arm rejects foreign-owned tiles as the port
   * always did. Keep the arm for Euro movers only.
   */
  if (nation_id >= 4) {
    return 0;
  }
  const ColonizeCol1Save* col1 = s_ai_native_col1;
  const int mover = ai_unit_index_on_tile(units, x, y);
  const int dest_unit = ai_unit_index_on_tile(units, dest_x, dest_y);
  const int mover_type = mover >= 0 ? units->units[mover].type_index : -1;
  const int dest_type = dest_unit >= 0 ? units->units[dest_unit].type_index : -1;
  int at_peace = 0;
  if (nation_id >= 4 && nation_id <= 11) {
    at_peace = (col1->indian[nation_id - 4].euro_diplo[owner] & COL1_INDIAN_PEACE_BIT) != 0;
  } else if (nation_id >= 0 && nation_id < 4) {
    at_peace = (col1->nation[nation_id].euro_relation[owner] & 0x40) != 0;
  }
  if (at_peace && mover_type != 0x10 && dest_type != 0x10) {
    return 0;
  }
  if (col1->head.game_options.woi && owner != (int)col1->head.human_player) {
    return 0;
  }
  (void)map;
  return 1;
}

static int ai_native_foreign_euro_pull(
  const ColonizeWorldMap* map, const ColonizeUnitPool* units, int x, int y, int nation_id,
  int dest_x, int dest_y, int score
) {
  const int mover = ai_unit_index_on_tile(units, x, y);
  if (mover < 0) {
    return score;
  }
  const ColonizeUnit* u = &units->units[mover];
  const ColonizeUnitType* t = units_type(units, u->type_index);
  if (!t || t->attack == 0) {
    return score;
  }
  ColonizeCombatStrengthCtx sctx;
  sctx.units = units;
  sctx.map = map;
  sctx.colonies = s_ai_native_colonies;
  sctx.col1 = s_ai_native_col1;
  /* FUN_5fef_1b0e vs the tile: engage the stack there when it has one, else
   * the open-field attacker formula ((stash 0 + 4) * base >> 2) * 3 >> 1. */
  int strength;
  const int dest_unit = ai_unit_index_on_tile(units, dest_x, dest_y);
  if (dest_unit >= 0) {
    ColonizeCombatEngageResult r;
    memset(&r, 0, sizeof(r));
    combat_land_engage(&sctx, mover, dest_unit, &r);
    strength = r.atk_strength;
  } else {
    strength = ((4 * combat_unit_base_x8(&sctx, mover, 1, NULL)) >> 2) * 3 >> 1;
  }
  int stack_cost = 0;
  int stack_military = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* o = &units->units[i];
    if (!o->active || o->aboard_ship_id >= 0 || o->x != x || o->y != y) {
      continue;
    }
    const ColonizeUnitType* ot = units_type(units, o->type_index);
    stack_cost += ot ? ot->cost : 0;
    if (o->type_index == UNITS_KIND_SOLDIER || o->type_index == UNITS_KIND_DRAGOON ||
        (o->type_index >= 6 && o->type_index <= 9)) {
      stack_military++;
    }
  }
  if (stack_military < 1) {
    stack_military = 1;
  }
  int divisor = t->cost;
  if (divisor < 1) {
    divisor = 1;
  }
  int e8 = ((stack_cost + 1) / stack_military) * strength / divisor;
  int bonus = 0;
  if (s_ai_native_colonies && colonies_id_at(s_ai_native_colonies, dest_x, dest_y) >= 0) {
    e8 *= 3;
    bonus = 1;
  }
  if (map_coords_inset(map, dest_x, dest_y) && (ai_layer2_at(map, dest_x, dest_y) & 2u) != 0) {
    e8 <<= 1;
    bonus = 1;
  }
  if (u->type_index == UNITS_KIND_ARTILLERY && !bonus) {
    e8 = 0;
  }
  if (s_ai_native_col1 && nation_id == (int)s_ai_native_col1->head.crown_nation_id && !bonus &&
      s_ai_native_home_dist == 0) {
    e8 >>= 1;
  }
  if (e8 > 999 || e8 < 0) {
    e8 = 1000;
  }
  if (e8 < 0xc && (u->type_index < 0xd || u->type_index > 0x12)) {
    return score - 999;
  }
  if (e8 < 1) {
    e8 = 1;
  }
  return score + e8 * 4;
}

/*
 * Seed-100 dir peels (init + mid-turn tables) shared by both Brave pickers.
 * Returns the (possibly overridden) dir. audit_unseen/audit_seen may be NULL
 * (the 021a picker has no seen/unseen branch pair).
 */
static int ai_native_apply_seed100_peels(
  int nation_id,
  int x,
  int y,
  int best_dir,
  int dump,
  const int* audit_unseen,
  const int* audit_seen,
  int unit_seen_by_any
) {
  /*
   * Seed-100 peels: quiet formula at matched LCG still misses these dirs
   * (empiricism matches golden). Override after scoring/LCG burns.
   */
  /* Init-pulse peels retired 2026-09-15: SEED100.SAV matches without them. */
  if (s_ai_seed100_midturn_turn > 0 && !ai_brave_peels_disabled()) {
    /*
     * Mid-turn dir peels — residue after the FUN_4d56_021a picker landed
     * (2026-09-15). 99 scoring holdouts + 6 river/multi-step + 2 cascade rows
     * of the retired 20e6-shaped scorer collapsed to the six below: each is
     * a 1..5-point near-tie the 021a transcription still resolves the other
     * way (see docs/port_plan.md "D3 determinism debt"). AI_PEEL_AUDIT=1
     * re-classifies them; drop a row the moment picked == golden.
     */
    static const struct {
      int turn;
      int nation_id;
      int x, y, dir;
    } k_mid_peels[] = {
      {1, 6, 48, 15, 6}, /* W (47,15); 207 vs NW 212 */
      {3, 6, 26, 6, 5}, /* SW (25,7); tie 203/203 with S, strict > keeps S */
      {3, 7, 46, 53, 3}, /* SE (47,54); 208 vs W 209 */
      {3, 10, 49, 39, 5}, /* SW (48,40); 204 vs N 207 (French-owned N) */
      {4, 7, 47, 54, 0}, /* N (47,53); 202 vs S 207 */
      {4, 10, 47, 38, 6}, /* W (46,38); 200 vs NE 212 */
    };
    for (size_t i = 0; i < sizeof(k_mid_peels) / sizeof(k_mid_peels[0]); ++i) {
      if (k_mid_peels[i].turn == s_ai_seed100_midturn_turn &&
          k_mid_peels[i].nation_id == nation_id && k_mid_peels[i].x == x &&
          k_mid_peels[i].y == y) {
        if (ai_peel_audit_enabled()) {
          fprintf(
            stderr,
            "AI_PEEL_AUDIT turn=%d n=%d xy=(%d,%d) golden=%d picked=%d "
            "unseen_best=%d seen_best=%d gate_now=%d\n",
            s_ai_seed100_midturn_turn,
            nation_id,
            x,
            y,
            k_mid_peels[i].dir,
            best_dir,
            audit_unseen ? ai_peel_audit_argmax(audit_unseen) : -1,
            audit_seen ? ai_peel_audit_argmax(audit_seen) : -1,
            unit_seen_by_any
          );
        }
        if (dump) {
          /* Keep the LCG trace honest: the peel below overrides the dir the
           * scored walk just printed. */
          fprintf(
            stderr,
            "AI_PEEL n=%d xy=(%d,%d) dir %d -> %d\n",
            nation_id,
            x,
            y,
            best_dir,
            k_mid_peels[i].dir
          );
        }
        best_dir = k_mid_peels[i].dir;
        break;
      }
    }
  }
  return best_dir;
}

/* Sentinel for a direction the ASM scorer rejected outright (was `continue;`). */
#define AI_ASM_DIR_REJECTED (-0x7fffffff)

/*
 * One direction of the quiet FUN_4d56_4753 ASM scorer. Extracted verbatim
 * from ai_native_pick_dir_asm; returns AI_ASM_DIR_REJECTED for a tile the
 * scan drops.
 */
static int ai_native_asm_score_dir(
  AiRng* rng, const ColonizeWorldMap* map, const ColonizeUnitPool* units,
  int x, int y, int nation_id, int last_dir, int unit_fa, int unit_river,
  int fog_enable, int unit_seen_by_any, int dump, int d,
  int* accepted, int* rejected, int audit_seen[8], int audit_unseen[8]
) {
  const int nx = x + k_ai_dir8_dx[d];
  const int ny = y + k_ai_dir8_dy[d];
  if (!map_coords_inset(map, nx, ny)) {
    (*rejected)++;
    return AI_ASM_DIR_REJECTED;
  }
  const int terr_raw = (int)(map_get_terrain_or(map, nx, ny, 25) & 0x1fu);
  if (terr_raw == 0x19 || terr_raw == 0x1a || terr_raw >= 0x18) {
    (*rejected)++;
    return AI_ASM_DIR_REJECTED;
  }
  if (ai_is_ocean_hs(map, nx, ny)) {
    (*rejected)++;
    return AI_ASM_DIR_REJECTED;
  }
  /*
   * DOS FUN_281f_06d2 tribe_or_presence: settlement owner (layer2 bit 0x02)
   * else the nation of a unit standing there (bit 0x01) — never the bare
   * layer3 nibble, which DOS leaves behind after a unit moves on (seed-100
   * TURN4: the Dutch ship's whole route still reads 3). Only correct now
   * that every mover keeps the presence bit exact
   * (units_occupancy_notify_moved / units_occupancy_rebuild).
   */
  const int own = map_tile_tribe_or_presence(map, nx, ny);
  const int foreign_euro_pull =
    own >= 0 && own != nation_id &&
    ai_native_foreign_euro_pull_open(map, units, x, y, nation_id, nx, ny, own);
  if (own >= 0 && own != nation_id && !foreign_euro_pull) {
    (*rejected)++;
    return AI_ASM_DIR_REJECTED;
  }
  (*accepted)++;

  /*
   * FUN_521d_20e6 outer branch (2026-08-13 seed-100 finding,
   * "Root cause candidate"): quiet Brave scoring splits on whether *this*
   * unit has been seen by any Euro nation yet — DOS unit+0x3147 high
   * nibble, bit (0x10<<nation), same convention as MAP_SEEN_NATION_BIT.
   * Not previously implemented; always took the "unseen" branch below.
   * Approximated here via map->seen at the unit's own (x,y) — the DOS
   * field is very likely just a cached mirror of that same fog-of-war
   * plane (same bit layout), not independently verified byte-for-byte.
   */
  int base;
  int score;
  int terr_delta = 0;
  /* One raw 15-bit draw feeds either branch's range map (same LCG burn
   * either way — lets AI_PEEL_AUDIT score both branches per dir). */
  const uint32_t rraw = ai_rng_next_counted(rng);
  if (s_ai_lcg_in_pick) {
    s_ai_lcg_pick_burns++;
  }
  /* Unseen branch: RNG(1,3); river/fa pair +1, else -terr cost. */
  int score_unseen = 1 + (int)((3u * rraw) >> 15);
  int terr_unseen = 0;
  {
    const int dest_river = (int)(map_get_terrain_or(map, nx, ny, 25) & 0x40u) != 0;
    const int dest_fa = ai_mask_fa_flags(map, nx, ny) != 0;
    const int cardinal = (d & 1) == 0;
    if ((unit_river && dest_river && cardinal) || (unit_fa && dest_fa)) {
      terr_unseen = 1;
    } else {
      const int terr = ai_dos_terr_class(map, nx, ny) & 31;
      terr_unseen = -map_dos_terr_cost_byte(terr);
    }
    score_unseen += terr_unseen;
  }
  /*
   * "Seen" branch (RNG(1,5), not RNG(1,3)) — real DOS table read
   * confirmed live (map_dos_terr_found_score_byte / DS:0x2f77, same
   * stride-16 records map_dos_terr_cost_byte already uses at +0).
   * Gate (FUN_1000_89d0 / FUN_1000_88cc traced 2026-08-13): add the
   * scaled table term unless dest already holds a unit AND is owned by
   * this same nation (own-tile stacking discouragement); the outer
   * ownership reject above already excludes foreign-owned dest tiles,
   * so in practice this reduces to "dest is unowned" for anything that
   * reaches here.
   */
  int score_seen = 1 + (int)((5u * rraw) >> 15);
  int terr_seen = 0;
  {
    const int dest_has_unit = units_id_at(units, nx, ny) >= 0;
    if (!dest_has_unit || own != nation_id) {
      const int terr = ai_dos_terr_class(map, nx, ny) & 31;
      terr_seen = map_dos_terr_found_score_byte(terr) << 2;
      score_seen += terr_seen;
    }
  }
  if (!unit_seen_by_any) {
    base = 1 + (int)((3u * rraw) >> 15);
    score = score_unseen;
    terr_delta = terr_unseen;
  } else {
    base = 1 + (int)((5u * rraw) >> 15);
    score = score_seen;
    terr_delta = terr_seen;
  }
  const int score_pre_gate = score;
  int gate = 0;
  int face_delta = 0;
  int fog_p8 = 0;
  int fog_m2 = 0;
  if (foreign_euro_pull) {
    /* LAB_521d_52aa arm (T1.9) — replaces the 54f5 facing/fog terms. */
    score = ai_native_foreign_euro_pull(map, units, x, y, nation_id, nx, ny, score);
  } else if (ai_lab_54f5_gate(map, units, nx, ny, nation_id)) {
    gate = 1;
    /* 521d:54f5 facing guard: the unit byte +0x314f enters the term only
     * when 0 <= v < 8 — the value 8 is written on every stay (521d:5899)
     * and means "no facing bias", it is NOT direction 0. */
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
    if (fog_enable) {
      score = ai_quiet_fog_explore_ex(
        map, score, x, y, d, nation_id, &fog_p8, &fog_m2
      );
    }
  }
  {
    /* Post-branch delta (face/fog/pull) applies to either branch total. */
    const int post = score - score_pre_gate;
    audit_unseen[d] = score_unseen + post;
    audit_seen[d] = score_seen + post;
  }
  if (dump) {
    const int far_x = x + k_ai_dir8_dx[d] * 4;
    const int far_y = y + k_ai_dir8_dy[d] * 4;
    fprintf(
      stderr,
      "AI_SCORE_DUMP asm d=%d dest=(%d,%d) base=%d terr=%+d gate=%d face=%+d "
      "fog8=%+d fogm2=%+d total=%d far=(%d,%d) far_ocean=%d far_inset=%d "
      "l2u=%02x l2d=%02x tu=%02x td=%02x b3=%d b5=%d tU=%+d tS=%+d own=%d dhu=%d "
      "ownnib=%d pull=%d\n",
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
      map_coords_inset(map, far_x, far_y),
      ai_layer2_at(map, x, y),
      ai_layer2_at(map, nx, ny),
      map_get_terrain_or(map, x, y, 25),
      map_get_terrain_or(map, nx, ny, 25),
      1 + (int)((3u * rraw) >> 15),
      1 + (int)((5u * rraw) >> 15),
      terr_unseen,
      terr_seen,
      own,
      units_id_at(units, nx, ny) >= 0,
      ai_owner_nibble(map, nx, ny),
      foreign_euro_pull
    );
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
  const int unit_river = (int)(map_get_terrain_or(map, x, y, 25) & 0x40u) != 0;
  int accepted = 0;
  int rejected = 0;
  /* Peel-audit scratch: both branch totals per dir (AI_PEEL_AUDIT=1). */
  int audit_seen[8];
  int audit_unseen[8];
  for (int i = 0; i < 8; ++i) {
    audit_seen[i] = audit_unseen[i] = -0x3e7;
  }
  const int dump4753 =
    ai_lcg_audit_enabled() && nation_id == 7 && x == 47 && y == 53;
  /* All 13 seed-100 init peels (+ Apache fog probe) for AI_LCG_AUDIT term diffs. */
  const int dump_miss =
    ai_lcg_audit_enabled() &&
    ((nation_id == 4 && x == 12 && y == 23) || (nation_id == 8 && x == 8 && y == 42) ||
     (nation_id == 6 && x == 47 && y == 15) ||
     (nation_id == 10 && x == 49 && y == 41) ||
     (nation_id == 4 && x == 11 && y == 30) || (nation_id == 4 && x == 6 && y == 34) ||
     (nation_id == 6 && x == 48 && y == 4) || (nation_id == 6 && x == 25 && y == 7) ||
     (nation_id == 7 && x == 46 && y == 56) || (nation_id == 8 && x == 13 && y == 48) ||
     (nation_id == 8 && x == 17 && y == 33) || (nation_id == 8 && x == 9 && y == 43) ||
     (nation_id == 9 && x == 33 && y == 54) || (nation_id == 9 && x == 30 && y == 50) ||
     (nation_id == 10 && x == 48 && y == 42) || (nation_id == 10 && x == 47 && y == 39) ||
     (nation_id == 11 && x == 32 && y == 31));
  const int dump = dump4753 || dump_miss || ai_score_at_match(nation_id, x, y) ||
                   (ai_peel_audit_enabled() && s_ai_seed100_midturn_turn > 0);
  s_ai_lcg_pick_burns = 0;
  s_ai_lcg_in_pick = 1;

  /*
   * DOS unit+0x3147 high nibble: per-Euro "currently observed" cache —
   * cleared+recomputed on the unit's own move (adjacency + sight-ring
   * writers), NOT the sticky explored-fog plane. The port's
   * col1_vis_mask carries exactly this nibble (units_vis_mask_after_move /
   * units_reveal_tile_effects), so read the mover's mask directly.
   */
  int unit_seen_by_any = 0;
  {
    const int mover = ai_unit_index_on_tile(units, x, y);
    if (mover >= 0) {
      unit_seen_by_any = (units->units[mover].col1_vis_mask & 0x0fu) != 0;
    }
  }
  /*
   * local_ec (521d:4d46, computed once per act): the whole fog band — far
   * probe +8, ship +4, and the −2 owner/presence ring (56ce jumps straight
   * to the keep-max when 0) — is DISABLED when the adjacent-foreign-claim
   * probe (FUN_2a1f_047c → 521d_0906) returns >= 0 at the unit's own tile.
   * Braves' @UNIT combat byte is nonzero (DS:0x5236[19] = 1), so the
   * rescue clause for unarmed land types never fires and the probe alone
   * decides.
   */
  int fog_enable = 1;
  {
    int side = -1;
    if (ai_goals_probe_adjacent_contact_claim_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(units), .colonies=(ColonizeColonyPool*)(s_ai_native_colonies), .map=(ColonizeWorldMap*)(map), .col1=(ColonizeCol1Save*)(s_ai_native_col1), .col1_ok=((s_ai_native_col1) != NULL)}, x, y, nation_id, 0, &side) >= 0) {
      fog_enable = 0;
    }
  }

  if (dump) {
    fprintf(
      stderr,
      "AI_SCORE_DUMP begin n=%d xy=(%d,%d) last_dir=%d mode=asm stay_sync=%d\n",
      nation_id,
      x,
      y,
      last_dir,
      1
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
    const int score = ai_native_asm_score_dir(
      rng, map, units, x, y, nation_id, last_dir, unit_fa, unit_river, fog_enable,
      unit_seen_by_any, dump, d, &accepted, &rejected, audit_seen, audit_unseen
    );
    if (score == AI_ASM_DIR_REJECTED) {
      continue;
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
  best_dir = ai_native_apply_seed100_peels(
    nation_id, x, y, best_dir, dump, audit_unseen, audit_seen, unit_seen_by_any
  );
  /* Quiet ASM always burns one extra LCG next (stay-shaped) for stream sync. */
  (void)ai_rng_next_counted(rng);
  return best_dir;
}


/*
 * ===========================================================================
 * FUN_4d56_021a — the REAL quiet Brave move picker (2026-09-15).
 *
 * The 20e6 "quiet Brave" branch above was never what DOS runs for an Indian
 * unit. 4d56:021a..14fd (ndisasm of overlay 13, origin 0x21a) carries its OWN
 * nine-way direction loop (dirs 0..7 plus index 8 = stay, DS:0xb4/0xbe both
 * hold a 9th (0,0) entry). The far call at 021a:1182 that the docs read as
 * "reaches the 521d scorer through thunk 291f:012c" is FUN_7a65_0008 — the
 * on-map debug number plotter behind the "Show Indian moves" option, called
 * with (x, y, score, colour 0xf). It never scores anything.
 *
 * Shape (asm offsets are 4d56:xxxx):
 *   0x21a  prologue: cool = turn - turn/25; unit fa/river; 0bce adjacent
 *          foreign probe (+ DS:0x8cfa nation); 15eb_0142 nearest colony of
 *          any nation on this continent; home village (destroy if bad);
 *          home unreachable when on another continent; "encroaching" colony
 *          = dist(colony, village) <= max(2, pop/2); Euro military stacks
 *          adjacent to the VILLAGE (count - colony pop/4, total >= 2);
 *          angry = #(alarm >= 75) + #(village attitude >= 0x80) over Euros.
 *   0x5aa  for d in 0..8: score = 200, flags = 0
 *          skip ocean/HS, DOS rumour tile (137f_0598), arctic 0x18
 *          owner / presence / settlement owner / fa / river / prime resource
 *          foreign owner: flags|=1, grudge/hostile (|=0x40 / |=4)
 *          occupied by foreign: flags|=2
 *          !occupied: adjacent Euro colony (100) / wagon (50) + alarm/2, best
 *            -> cooldown gate, += val + 2*(cool - last_visit), flags|=0x80
 *          !occupied & no visit & encroaching colony & cooldown: += (12-d)*10
 *          occupied: other tribe -> skip; land only -> -= 50-alarm; units ->
 *            +4*found[terr], per-type stack bonuses (treasure/artillery/wagon
 *            loot arms with RNG(50,100) and RNG(0,7) draws), colony +20/+10,
 *            -= 50-alarm, need loot or grudge, flags|=8 (attack)
 *          own village tile: musket/horse upgrade +20
 *          own unit on dest: 2+ off-village -> skip, else -40
 *          stay: (1-stack)*40, -25 (attack intent or RNG(0,(tech+1)*4)==0)
 *          facing +4 / +3 adjacent (byte math: facing 8 counts 1 and 7) / -6
 *          road pair +4 else cardinal river pair +4
 *          home tether: dist>2 -> -= 3*dist (>>1 angry, >>1 armed, >>2 mtd)
 *          09dc adjacent-foreign at dest: Euro -> +50 unmet, +(alarm-50)/4
 *            when alarm >= 25; other tribe -> -25
 *          stay-only arms (adjacent foreign at own tile, no-stay unless upg)
 *          !angry: unclaimed +5, drift toward nearest colony
 *            += (tier(alarm)+1)*(12-d)>>2 within 12
 *          angry: +5, +10 resource, colony +500, unit stack combat estimate,
 *            hostile colony -2*d (met) else drift
 *          += RNG(1,5); clamp 0; strict > keeps first
 *   0x11ae facing = dir (8 on stay); stay: orders 5/6 latch + in-field
 *          arm/mount; move: orders = 0
 *   0x1277 tail: contact_state[owner]==2 -> stay; ==1 stamp; contact/attack
 *          need 3 thirds left; human-colony warning chrome (not ported);
 *          pending-encounter bit (unit +0x3148 & 8) -> encounter, stay;
 *          peaceful visit (0x80): last_visit = turn, bit 8 set;
 *          encroach march via 6662_0f74 pathfinder after the long cooldown;
 *          foreign-land quiet move with spent > 0 -> stay.
 *
 * DOS unit +0x12 word (COL1 cargo_hold[2..3], the "spawn turn stamp" with
 * "no reader found") is this routine's last-visit stamp: port keeps it in
 * col1_hold_raw[6..7].
 * ===========================================================================
 */

COLONIZE_INTERNAL int ai_021a_settle_owner(const ColonizeWorldMap* map, int x, int y) {
  /* FUN_281f_06be -> FUN_137f_03e4: layer2 bit 0x02 then the owner nibble. */
  if (!map || !map->layer2 || !map_coords_inset(map, x, y)) {
    return -1;
  }
  if ((map->layer2[(size_t)y * (size_t)map->width + (size_t)x] & 0x02u) == 0) {
    return -1;
  }
  return ai_owner_nibble(map, x, y);
}

COLONIZE_INTERNAL int ai_021a_colony_at(const ColonizeColonyPool* colonies, int x, int y) {
  /* FUN_15eb_0a76: colony index at tile (pool order == COL1 order). */
  if (!colonies) {
    return -1;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &colonies->colonies[i];
    if (c->active && c->x == x && c->y == y) {
      return i;
    }
  }
  return -1;
}

COLONIZE_INTERNAL int ai_021a_type_attack(const ColonizeUnitPool* units, int type_index) {
  const ColonizeUnitType* t = units_type(units, type_index);
  return t ? t->attack : 0;
}

/* FUN_1427_0fec / 0fc0 — armed / mounted type sets. */
COLONIZE_INTERNAL int ai_021a_type_armed(int t) {
  return t == 1 || t == 4 || t == 0xb || t == 0x14 || t == 0x16;
}
COLONIZE_INTERNAL int ai_021a_type_mounted(int t) {
  return t == 4 || t == 5 || t == 0x15 || t == 0x16;
}

/*
 * FUN_1427_0b08 (via 0bce) / FUN_1427_09dc: first adjacent foreign owner on
 * the same medium. 0b08 checks unit presence then settlement per tile (the
 * settlement write wins), stopping after the first tile that found one; 09dc
 * checks settlement first, else presence, stopping at the first hit. Both
 * leave the nation in DS:0x8cfa.
 */
static int s_021a_adj_fx = -1;
static int s_021a_adj_fy = -1;
COLONIZE_INTERNAL int ai_021a_adjacent_foreign(
  const ColonizeWorldMap* map, int x, int y, int nation_id, int settlement_first
) {
  const int medium = ai_is_ocean_hs(map, x, y);
  int found = -1;
  s_021a_adj_fx = s_021a_adj_fy = -1;
  for (int d = 0; d < 8 && found < 0; ++d) {
    const int nx = x + k_ai_dir8_dx[d];
    const int ny = y + k_ai_dir8_dy[d];
    if (!map_coords_inset(map, nx, ny)) {
      continue;
    }
    if (ai_is_ocean_hs(map, nx, ny) != medium) {
      continue;
    }
    const int p = map_tile_owner_or_presence(map, nx, ny);
    const int s = ai_021a_settle_owner(map, nx, ny);
    if (settlement_first) {
      const int o = s >= 0 ? s : p;
      if (o >= 0 && o != nation_id) {
        found = o;
      }
    } else {
      if (p >= 0 && p != nation_id) {
        found = p;
      }
      if (s >= 0 && s != nation_id) {
        found = s;
      }
    }
    if (found >= 0) {
      s_021a_adj_fx = nx;
      s_021a_adj_fy = ny;
    }
  }
  return found;
}

COLONIZE_INTERNAL int ai_021a_visit_turn(const ColonizeUnit* u) {
  if (!u->col1_hold_raw_valid) {
    return 0;
  }
  return (int)(int16_t)((unsigned)u->col1_hold_raw[6] | ((unsigned)u->col1_hold_raw[7] << 8));
}

COLONIZE_INTERNAL void ai_021a_set_visit_turn(ColonizeUnit* u, int turn) {
  u->col1_hold_raw[6] = (uint8_t)(turn & 0xff);
  u->col1_hold_raw[7] = (uint8_t)((turn >> 8) & 0xff);
  u->col1_hold_raw_valid = 1;
}

typedef struct Ai021aResult {
  int dir; /* 0..7 move, 8 stay */
  int flags; /* DOS [bp-0x82] of the winning dir */
} Ai021aResult;

/* FUN_15dc_00a2 alarm tier: <25 0, <50 1, <75 2, else 3. */
COLONIZE_INTERNAL int ai_021a_alarm_tier(int alarm) {
  if (alarm < 0x19) return 0;
  if (alarm < 0x32) return 1;
  if (alarm < 0x4b) return 2;
  return 3;
}

/* struct ai_021a_ctx / Ai021aDirStatus now live in ai_internal.h. */

/*
 * 021a:0x59a-0x8f7 — tile facts, owner/grudge, occupancy, the adjacent-visit
 * scan and the encroachment pull. Extracted verbatim from
 * ai_native_pick_dir_021a.
 */
COLONIZE_INTERNAL Ai021aDirStatus ai_021a_dir_tile(struct ai_021a_ctx* c) {
  const ColonizeWorldMap* const map = c->map;
  const ColonizeUnitPool* const units = c->units;
  const ColonizeCol1Save* const col1 = c->col1;
  const int nation_id = c->nation_id;
  const int x = c->x;
  const int y = c->y;
  const int cool = c->cool;
  const int dump = c->dump;
  const ColonizeColony* const col = c->col;
  const ColonizeCol1Tribe* const village = c->village;
  const int vx = c->vx;
  const int vy = c->vy;
  const int encroach = c->encroach;
  const int threat_nation = c->threat_nation;
  const int visit_turn = c->visit_turn;
  const int d = c->d;
  int grudge = c->grudge;

  const int nx = x + (d < 8 ? k_ai_dir8_dx[d] : 0);
  const int ny = y + (d < 8 ? k_ai_dir8_dy[d] : 0);
  int score = 200;
  int flags = 0;
  const int terr = ai_dos_terr_class(map, nx, ny);
  if (terr == 0x19 || terr == 0x1a) {
    if (dump) fprintf(stderr, "AI_021A d=%d skip water\n", d);
    c->grudge = grudge; /* the grudge latch outlives the skip */
    return AI_021A_DIR_SKIP;
  }
  if (map_dos_0598_rumour_tile(map, nx, ny)) {
    if (dump) fprintf(stderr, "AI_021A d=%d skip rumour dest=(%d,%d)\n", d, nx, ny);
    c->grudge = grudge; /* the grudge latch outlives the skip */
    return AI_021A_DIR_SKIP;
  }
  if (terr == 0x18) {
    if (dump) fprintf(stderr, "AI_021A d=%d skip arctic\n", d);
    c->grudge = grudge; /* the grudge latch outlives the skip */
    return AI_021A_DIR_SKIP;
  }
  const int owner = ai_owner_nibble(map, nx, ny);
  const int presence = map_tile_owner_or_presence(map, nx, ny);
  const int settle = ai_021a_settle_owner(map, nx, ny);
  const int dfa = ai_mask_fa_flags(map, nx, ny) & 0x0a;
  const int driver = (int)(map_get_terrain_or(map, nx, ny, 25) & 0x40u);
  const int dres = map_resource_type_at(map, nx, ny) >= 0;
  int hostile = 0;
  int att = 0;
  if (owner < 0 || owner == nation_id) {
    hostile = 0;
    grudge = 0;
    att = 0;
  } else {
    flags |= 1;
    att = (owner < 4 && village) ? col1_tribe_attitude(village, owner) : 0;
    if (owner >= 4) {
      hostile = 0;
    } else {
      grudge = att >= 0x80;
      hostile = ai_diplo_indian_alarm(col1, nation_id, owner) >= 0x4b;
      if (hostile) {
        grudge = 1;
      }
      if (owner == threat_nation) {
        grudge = 1;
      }
    }
    if (hostile) {
      flags |= 4;
    }
    if (grudge) {
      flags |= 0x40;
    }
  }
  int occ = 0;
  {
    int t = presence >= 0 ? presence : settle;
    if (t == nation_id) {
      t = -1;
    }
    if (t >= 0) {
      flags |= 2;
      occ = 1;
    }
  }
  int visit_nation = -1;
  int visit_val = -1;
  if (!occ) {
    for (int n = 0; n < 8; ++n) {
      const int ax = nx + k_ai_dir8_dx[n];
      const int ay = ny + k_ai_dir8_dy[n];
      if (ax == x && ay == y) {
        continue;
      }
      int val = 100;
      int e = ai_021a_settle_owner(map, ax, ay);
      if (e < 0) {
        const int ui = ai_unit_index_on_tile(units, ax, ay);
        if (ui < 0) {
          continue;
        }
        const ColonizeUnit* au = &units->units[ui];
        if (au->nation_id == nation_id) {
          continue;
        }
        if (au->type_index != UNITS_KIND_WAGON) {
          continue;
        }
        e = au->nation_id;
        val >>= 1;
      }
      if (e < 0 || e >= 4) {
        continue;
      }
      val += ai_diplo_indian_alarm(col1, nation_id, e) >> 1;
      if (val >= visit_val) {
        visit_val = val;
        visit_nation = e;
      }
    }
  }
  const int vdist = map_dos_dist(nx - vx, ny - vy);
  if (visit_nation >= 0) {
    att = village ? col1_tribe_attitude(village, visit_nation) : 0;
    grudge = att >= 0x80;
    hostile = ai_diplo_indian_alarm(col1, nation_id, visit_nation) >= 0x4b;
    if (!grudge && !hostile) {
      if (cool - visit_turn < vdist * 2 + 5) {
        if (dump) fprintf(stderr, "AI_021A d=%d skip visit-cooldown\n", d);
        c->grudge = grudge; /* the grudge latch outlives the skip */
        return AI_021A_DIR_SKIP;
      }
    }
    visit_val += (cool - visit_turn) * 2;
    score += visit_val;
    if (!hostile) {
      flags |= 0x80;
    }
  }
  if (!occ && visit_nation < 0 && encroach >= 0 && col) {
    const int cn = col->nation_id;
    const int census = (cn >= 0 && cn < 4) ? (int)col1->stuff.census_pop_proxy[cn] : 0;
    const int cx = (census >> 3) + encroach * 2 + 5;
    if (cx <= cool - visit_turn) {
      const int dcol = map_dos_dist(col->x - nx, col->y - ny);
      if (dcol < 12) {
        score += (12 - dcol) * 10;
      }
    }
  }

  c->nx = nx;
  c->ny = ny;
  c->score = score;
  c->flags = flags;
  c->terr = terr;
  c->owner = owner;
  c->presence = presence;
  c->settle = settle;
  c->dfa = dfa;
  c->driver = driver;
  c->dres = dres;
  c->hostile = hostile;
  c->att = att;
  c->occ = occ;
  c->visit_nation = visit_nation;
  c->visit_val = visit_val;
  c->vdist = vdist;
  c->grudge = grudge;
  return AI_021A_DIR_OK;
}

/* 021a:0x8f8-0xbb5 — occupied-destination attack intent / alarm arms. */
COLONIZE_INTERNAL Ai021aDirStatus ai_021a_dir_occupant(struct ai_021a_ctx* c) {
  AiRng* const rng = c->rng;
  const ColonizeUnitPool* const units = c->units;
  const ColonizeCol1Save* const col1 = c->col1;
  const int nation_id = c->nation_id;
  const int dump = c->dump;
  const int threat = c->threat;
  const int lone = c->lone;
  const int d = c->d;
  const int nx = c->nx;
  const int ny = c->ny;
  const int terr = c->terr;
  const int owner = c->owner;
  const int presence = c->presence;
  const int settle = c->settle;
  const int att = c->att;
  const int occ = c->occ;
  const int vdist = c->vdist;
  int grudge = c->grudge;
  int score = c->score;
  int flags = c->flags;

  int attack_intent = 0;
  int alarm = 0;
  if (occ) {
    if (owner >= 4) {
      if (dump) fprintf(stderr, "AI_021A d=%d skip other-tribe\n", d);
      c->grudge = grudge; /* the grudge latch outlives the skip */
      return AI_021A_DIR_SKIP;
    }
    alarm = ai_diplo_indian_alarm(col1, nation_id, owner);
    if (alarm > 100) {
      alarm = 100;
    }
    if (presence < 0 && settle < 0) {
      score -= 50 - alarm;
    } else {
      int loot = 0;
      int want = 0;
      int treasure = 0;
      if (presence >= 0) {
        score += map_dos_terr_found_score_byte(terr & 31) << 2;
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          const ColonizeUnit* su = &units->units[i];
          if (!su->active || su->aboard_ship_id >= 0 || su->x != nx || su->y != ny) {
            continue;
          }
          const int st = su->type_index;
          switch (st) {
            case 0:
              score += 5;
              loot = lone; /* 0xa42 -> 0xa4c */
              break;
            case 1:
            case 4:
              if (vdist <= 1) {
                score += (threat + 5) * 4;
              }
              break;
            case 2:
              score += 10;
              loot = lone; /* 0xa48 -> 0xa4c */
              break;
            case 5:
              score += 10;
              break;
            case 0xa:
              if (lone) {
                score += 50;
                loot = 1;
              }
              treasure = 1;
              break;
            case 0xb:
              score += 35;
              want = 1;
              loot = lone;
              break;
            case 0xc: {
              score += 2;
              int holds = 0;
              for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
                if (su->hold_goods_amount[h] > 0 && su->hold_goods_amount[h] < 255) {
                  holds++;
                }
              }
              if (holds != 0) {
                want = 1;
                flags |= 0x10;
                score += 8;
              }
              if (!want) {
                if (ai_rng_range(rng, 50, 100) >= alarm) {
                  break;
                }
              }
              /* +0x315c/+0x315e both negative: not in a transport chain. */
              if (su->aboard_ship_id < 0) {
                score += 10;
                want = 1;
                loot = 1; /* 0xaa8: ax=1 -> [bp-0x20] */
              }
              break;
            }
            default:
              break;
          }
        }
      }
      if (settle >= 0) {
        if (alarm >= 0x4b) {
          score += 0x14;
        }
        if (alarm >= 0x32) {
          score += 10;
        }
      }
      if (alarm <= 0x19 && !grudge) {
        if (!loot) {
          c->grudge = grudge; /* the grudge latch outlives the skip */
          return AI_021A_DIR_SKIP;
        }
        if (!treasure) {
          c->grudge = grudge; /* the grudge latch outlives the skip */
          return AI_021A_DIR_SKIP;
        }
        if (ai_rng_range(rng, 0, 7) != 0) {
          c->grudge = grudge; /* the grudge latch outlives the skip */
          return AI_021A_DIR_SKIP;
        }
      }
      if (settle < 0 && att >= 0x20) {
        grudge = 1;
      }
      score -= 0x32 - alarm;
      if (!loot && !grudge) {
        c->grudge = grudge; /* the grudge latch outlives the skip */
        return AI_021A_DIR_SKIP;
      }
      flags |= 8;
      attack_intent = 1;
    }
  }

  c->score = score;
  c->flags = flags;
  c->attack_intent = attack_intent;
  c->alarm = alarm;
  c->grudge = grudge;
  return AI_021A_DIR_OK;
}

/* 021a:0xbb6-0xd5b — upgrade beacon, facing/road/river bias, home tether. */
COLONIZE_INTERNAL Ai021aDirStatus ai_021a_dir_terrain(struct ai_021a_ctx* c) {
  AiRng* const rng = c->rng;
  const ColonizeWorldMap* const map = c->map;
  const ColonizeUnitPool* const units = c->units;
  const ColonizeCol1Save* const col1 = c->col1;
  const ColonizeUnit* const u = c->u;
  const int nation_id = c->nation_id;
  const int unit_fa = c->unit_fa;
  const int unit_river = c->unit_river;
  const int dump = c->dump;
  const int adj_foreign = c->adj_foreign;
  const int adj_nation = c->adj_nation;
  const int vx = c->vx;
  const int vy = c->vy;
  const int home_reach = c->home_reach;
  const int angry = c->angry;
  const int self_stack = c->self_stack;
  const ColonizeCol1Indian* const ind = c->ind;
  const int tech = c->tech;
  const int facing = c->facing;
  const int d = c->d;
  const int nx = c->nx;
  const int ny = c->ny;
  const int presence = c->presence;
  const int settle = c->settle;
  const int dfa = c->dfa;
  const int driver = c->driver;
  const int attack_intent = c->attack_intent;
  int score = c->score;

  /* 0xbb6: own village tile — musket / horse upgrade beacon. */
  int upg = 0;
  if (settle == nation_id && ind) {
    if ((int8_t)ind->muskets > 0 &&
        (u->type_index == UNITS_KIND_BRAVE ||
         u->type_index == UNITS_KIND_MTD_BRAVE)) {
      score += 0x14;
      upg = 1;
    }
    if (ind->horse_breeding >= 0x19 && units_max_mp(units, u->id) <= 3) {
      score += 0x14;
      upg = 1;
    }
  }
  /* 0xc0b */
  if (d != 8) {
    if (presence >= 0 && presence == nation_id) {
      if (units_count_at(units, nx, ny) >= 2 && settle < 0) {
        if (dump) fprintf(stderr, "AI_021A d=%d skip own-stack\n", d);
        return AI_021A_DIR_SKIP;
      }
      score -= 0x28;
    }
  } else {
    score += (1 - self_stack) * 0x28;
    if (attack_intent) {
      score -= 0x19;
    } else if (ai_rng_range(rng, 0, (tech + 1) * 4) == 0) {
      score -= 0x19;
    }
  }
  /* 0xc82: facing / road / river */
  if (d != 8) {
    if (facing == d) {
      score += 4;
    } else if (((facing + 1) & 7) == d || ((facing - 1) & 7) == d) {
      score += 3;
    } else if (((facing ^ 4) & 0xff) == d) {
      score -= 6;
    }
    if (dfa && unit_fa) {
      score += 4;
    } else if ((d & 1) == 0 && driver && unit_river) {
      score += 4;
    }
  } else {
    /* 0xdf6: stay-only arms */
    if (!angry) {
      if (adj_foreign) {
        if (adj_nation < 4) {
          const int a = ai_diplo_indian_alarm(col1, nation_id, adj_nation);
          if (ai_021a_alarm_tier(a) > 0) {
            score += ((a - 0x32) >> 1) + 8;
          }
        }
      } else if (!upg) {
        if (dump) fprintf(stderr, "AI_021A d=8 skip stay (tech roll drawn)\n");
        return AI_021A_DIR_SKIP;
      }
    } else if (adj_foreign && adj_nation < 4) {
      const int a = ai_diplo_indian_alarm(col1, nation_id, adj_nation);
      if (a >= 0x5f) {
        score += 0x10;
      } else if (ai_021a_alarm_tier(a) > 0) {
        score += ((a - 0x32) >> 1) + 5;
      }
    }
  }
  /* 0xcea: home tether */
  if (home_reach >= 0) {
    const int hd = map_dos_dist(nx - vx, ny - vy);
    if (hd > 2) {
      int t = hd * 3;
      if (angry) {
        t >>= 1;
      }
      if (ai_021a_type_armed(u->type_index)) {
        t >>= 1;
      }
      if (ai_021a_type_mounted(u->type_index)) {
        t >>= 2;
      }
      score -= t;
    }
  }
  /* 0xd5c: FUN_1427_09dc adjacent foreign at dest */
  {
    const int fn = ai_021a_adjacent_foreign(map, nx, ny, nation_id, 1);
    if (fn >= 0) {
      if (fn < 4) {
        const uint8_t rel = ai_diplo_read(col1, nation_id, fn);
        if ((rel & 0x20) == 0) {
          score += 0x32;
        }
        const int a = ai_diplo_indian_alarm(col1, nation_id, fn);
        if (ai_021a_alarm_tier(a) > 0) {
          score += (a - 0x32) >> 2;
        }
      } else {
        score -= 0x19;
      }
    }
  }

  c->score = score;
  c->upg = upg;
  return AI_021A_DIR_OK;
}

/* 021a:0xeba-0x1157 — the quiet / angry destination bands. */
COLONIZE_INTERNAL Ai021aDirStatus ai_021a_dir_angry(struct ai_021a_ctx* c) {
  const ColonizeWorldMap* const map = c->map;
  const ColonizeUnitPool* const units = c->units;
  const ColonizeCol1Save* const col1 = c->col1;
  const ColonizeColonyPool* const colonies = c->colonies;
  const ColonizeUnit* const u = c->u;
  const int nation_id = c->nation_id;
  const int continent = c->continent;
  const ColonizeColony* const col = c->col;
  const int angry = c->angry;
  const int grudge = c->grudge;
  const int nx = c->nx;
  const int ny = c->ny;
  const int owner = c->owner;
  const int presence = c->presence;
  const int settle = c->settle;
  const int dres = c->dres;
  const int hostile = c->hostile;
  int score = c->score;

  /* 0xeba */
  if (!angry) {
    if (owner < 0) {
      score += 5;
    }
    if (col && ai_continent_id(map, nx, ny) == continent) {
      const int dcol = map_dos_dist(col->x - nx, col->y - ny);
      const int a = ai_diplo_indian_alarm(col1, nation_id, col->nation_id);
      if (dcol < 12) {
        score += ((ai_021a_alarm_tier(a) + 1) * (12 - dcol)) >> 2;
      }
    }
  } else {
    if (!grudge && !hostile) {
      if (settle >= 0) {
        return AI_021A_DIR_SKIP;
      }
    } else {
      score += 5;
      if (dres) {
        score += 10;
      }
      const int ci = ai_021a_colony_at(colonies, nx, ny);
      if (ci >= 0) {
        score += 500;
      } else if (presence >= 0) {
        ColonizeCombatStrengthCtx sctx;
        sctx.units = units;
        sctx.map = map;
        sctx.colonies = colonies;
        sctx.col1 = col1;
        const int def_id = units_best_defender_at(units, col1, nx, ny, u->id, -1);
        const int atk = (combat_unit_base_x8(&sctx, u->id, 1, NULL) * 3) >> 1;
        int defs = 0;
        if (def_id >= 0) {
          defs = combat_engagement_strength(&sctx, def_id, u->id, NULL);
          const ColonizeUnit* du = units_get_const(units, def_id);
          if (du && du->type_index == UNITS_KIND_ARTILLERY) {
            defs >>= 3;
          }
        }
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          const ColonizeUnit* su = &units->units[i];
          if (!su->active || su->aboard_ship_id >= 0 || su->x != nx || su->y != ny) {
            continue;
          }
          switch (su->type_index) {
            case UNITS_KIND_COLONIST: score += 4; break;
            case UNITS_KIND_SOLDIER: score -= 2; break;
            case UNITS_KIND_PIONEER:
            case UNITS_KIND_MISSIONARY:
            case UNITS_KIND_SCOUT: score += 8; break;
            case UNITS_KIND_DRAGOON: score -= 1; break;
            case UNITS_KIND_TREASURE:
            case UNITS_KIND_ARTILLERY:
            case UNITS_KIND_WAGON: score += 0x10; break;
            default: break;
          }
        }
        if (defs <= atk) {
          score += (atk - defs) + 0x1e;
        } else {
          score += (atk - defs) * 2;
        }
      }
    }
    if (col && ai_continent_id(map, nx, ny) == continent) {
      const int dcol = map_dos_dist(col->x - nx, col->y - ny);
      const uint8_t rel = ai_diplo_read(col1, nation_id, col->nation_id);
      const int a = ai_diplo_indian_alarm(col1, nation_id, col->nation_id);
      if (a >= 0x4b) {
        if (rel & 0x20) {
          score -= dcol * 2;
        }
      } else if (dcol < 12) {
        score += ((ai_021a_alarm_tier(a) + 1) * (12 - dcol)) >> 2;
      }
    }
  }

  c->score = score;
  return AI_021A_DIR_OK;
}

/* One direction: the four scoring stages plus the 021a:0x1158 roll/pick tail. */
COLONIZE_INTERNAL void ai_021a_score_dir(struct ai_021a_ctx* c) {
  if (ai_021a_dir_tile(c) == AI_021A_DIR_SKIP) {
    return;
  }
  if (ai_021a_dir_occupant(c) == AI_021A_DIR_SKIP) {
    return;
  }
  if (ai_021a_dir_terrain(c) == AI_021A_DIR_SKIP) {
    return;
  }
  if (ai_021a_dir_angry(c) == AI_021A_DIR_SKIP) {
    return;
  }

  AiRng* const rng = c->rng;
  const ColonizeWorldMap* const map = c->map;
  const ColonizeUnitPool* const units = c->units;
  const int nation_id = c->nation_id;
  const int unit_fa = c->unit_fa;
  const int unit_river = c->unit_river;
  const int dump = c->dump;
  const int vx = c->vx;
  const int vy = c->vy;
  const int d = c->d;
  const int nx = c->nx;
  const int ny = c->ny;
  const int flags = c->flags;
  const int terr = c->terr;
  const int owner = c->owner;
  const int presence = c->presence;
  const int settle = c->settle;
  const int dfa = c->dfa;
  const int driver = c->driver;
  const int dres = c->dres;
  const int occ = c->occ;
  const int visit_nation = c->visit_nation;
  const int visit_val = c->visit_val;
  const int vdist = c->vdist;
  int score = c->score;
  int best = c->best;
  int best_dir = c->best_dir;
  int best_flags = c->best_flags;

  /* 0x1158 */
  const int roll = ai_rng_range(rng, 1, 5);
  score += roll;
  if (score < 0) {
    score = 0;
  }
  if (dump) {
    fprintf(
      stderr,
      "AI_021A d=%d dest=(%d,%d) score=%d roll=%d flags=%02x terr=%02x own=%d pres=%d set=%d "
      "fa=%d/%d riv=%d/%d res=%d vdist=%d adjf=%d visit=%d/%d occ=%d rum=%d hd=%d cont=%d\n",
      d, nx, ny, score, roll, flags, terr, owner, presence, settle, unit_fa, dfa,
      unit_river != 0, driver != 0, dres, vdist,
      ai_021a_adjacent_foreign(map, nx, ny, nation_id, 1), visit_nation, visit_val, occ,
      map_dos_0598_rumour_tile(map, nx, ny), map_dos_dist(nx - vx, ny - vy),
      ai_continent_id(map, nx, ny)
    );
    if (s_021a_adj_fx >= 0) {
      fprintf(
        stderr, "AI_021A   adjf tile=(%d,%d) l2=%02x l3=%02x unit=%d\n", s_021a_adj_fx,
        s_021a_adj_fy, ai_layer2_at(map, s_021a_adj_fx, s_021a_adj_fy),
        map_get_layer3(map, s_021a_adj_fx, s_021a_adj_fy),
        ai_unit_index_on_tile(units, s_021a_adj_fx, s_021a_adj_fy)
      );
    }
  }
  if (score > best) {
    best = score;
    best_dir = d;
    best_flags = flags;
  }

  c->score = score;
  c->best = best;
  c->best_dir = best_dir;
  c->best_flags = best_flags;
}

static int ai_native_pick_dir_021a(
  AiRng* rng,
  const ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  const ColonizeCol1Save* col1,
  const ColonizeColonyPool* colonies,
  const ColonizeUnit* u,
  int nation_id,
  Ai021aResult* out
) {
  out->dir = 8;
  out->flags = 0;
  if (!rng || !map || !units || !col1 || !u) {
    return 8;
  }
  const int x = u->x;
  const int y = u->y;
  const int indian = nation_id - 4;
  const int turn_w = (int)col1->head.turn;
  const int cool = turn_w + turn_w / -25; /* 021a:22b idiv by -25 */
  const int unit_fa = ai_mask_fa_flags(map, x, y) & 0x0a;
  const int unit_river = (int)(map_get_terrain_or(map, x, y, 25) & 0x40u);
  const int home = u->home_tribe_id;
  const int dump = ai_score_at_match(nation_id, x, y) ||
                   (ai_peel_audit_enabled() && s_ai_seed100_midturn_turn > 0);

  /* 0x291: FUN_1427_0bce — 0 when standing on a settlement tile. */
  int adj_foreign = 0;
  int adj_nation = -1;
  if (ai_021a_settle_owner(map, x, y) < 0) {
    adj_nation = ai_021a_adjacent_foreign(map, x, y, nation_id, 0);
    adj_foreign = adj_nation >= 0;
  }
  const int continent = ai_continent_id(map, x, y);
  /* 0x31d: nearest colony of any nation on this continent. */
  int col_dist = 9999;
  const int col_idx =
    ai_goals_nearest_colony_15eb_0142(map, colonies, x, y, -1, continent, &col_dist);
  const ColonizeColony* col = (col_idx >= 0) ? &colonies->colonies[col_idx] : NULL;
  /* Home village (validity already enforced by the pulse). */
  const ColonizeCol1Tribe* village =
    (home >= 0 && home < (int)col1->head.tribe_count) ? &col1->tribe[home] : NULL;
  const int vx = village ? (int)village->x : x;
  const int vy = village ? (int)village->y : y;
  int home_reach = home;
  if (village && ai_continent_id(map, vx, vy) != continent) {
    home_reach = -1;
  }
  /* 0x3bd: encroaching colony = within max(2, pop/2) of the village. */
  int encroach = -1;
  if (col) {
    const int d = map_dos_dist(col->x - vx, col->y - vy);
    int lim = (int)((int8_t)col->population >> 1);
    if (lim < 2) {
      lim = 2;
    }
    encroach = (lim >= d) ? d : -1;
  }
  /* 0x40a: Euro military stacks adjacent to the village. */
  int threat = 0;
  int threat_nation = -1;
  for (int d = 0; d < 8; ++d) {
    const int nx = vx + k_ai_dir8_dx[d];
    const int ny = vy + k_ai_dir8_dy[d];
    const int o = ai_owner_nibble(map, nx, ny);
    if (o < 0 || o >= 4) {
      continue;
    }
    if (ai_is_ocean_hs(map, nx, ny)) {
      continue;
    }
    if (ai_unit_index_on_tile(units, nx, ny) < 0) {
      continue;
    }
    int cnt = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* o_u = &units->units[i];
      if (!o_u->active || o_u->aboard_ship_id >= 0 || o_u->x != nx || o_u->y != ny) {
        continue;
      }
      if (o_u->type_index >= 0xd && o_u->type_index <= 0x12) {
        continue;
      }
      if (ai_021a_type_attack(units, o_u->type_index) > 1) {
        cnt++;
      }
    }
    if (cnt > 0) {
      const int ci = ai_021a_colony_at(colonies, nx, ny);
      if (ci >= 0) {
        cnt -= (int)((int8_t)colonies->colonies[ci].population >> 2);
      }
    }
    if (cnt > 0) {
      threat += cnt;
      threat_nation = o;
    }
  }
  if (threat < 2) {
    threat = 0;
    threat_nation = -1;
  }
  /* 0x52e: angry count. */
  int angry = 0;
  for (int e = 0; e < 4; ++e) {
    if (ai_diplo_indian_alarm(col1, nation_id, e) >= 0x4b) {
      angry++;
    }
    if (village && col1_tribe_attitude(village, e) >= 0x80) {
      angry++;
    }
  }
  const int visit_turn = ai_021a_visit_turn(u);
  const int self_stack = units_count_at(units, x, y);
  const int lone = self_stack == 1;
  const ColonizeCol1Indian* ind =
    (indian >= 0 && indian < 8) ? &col1->indian[indian] : NULL;
  const int tech = ind ? (int)ind->tech : 0;
  const int facing = u->last_dir;

  {
    /* AI_021A_WATCH="x:y" — print that tile's layer2/layer3 at every act. */
    static int wx = -2, wy = -2;
    if (wx == -2) {
      const char* e = getenv("AI_021A_WATCH");
      if (!e || sscanf(e, "%d:%d", &wx, &wy) != 2) {
        wx = wy = -1;
      }
    }
    if (wx >= 0) {
      fprintf(
        stderr, "AI_021A_WATCH n=%d act@(%d,%d) tile(%d,%d) l2=%02x l3=%02x\n", nation_id, x, y,
        wx, wy, ai_layer2_at(map, wx, wy), map_get_layer3(map, wx, wy)
      );
    }
  }
  if (dump) {
    fprintf(
      stderr,
      "AI_021A begin n=%d xy=(%d,%d) facing=%d cool=%d visit=%d adj=%d/%d col=%d encroach=%d "
      "threat=%d/%d angry=%d stack=%d seed=%u\n",
      nation_id, x, y, facing, cool, visit_turn, adj_foreign, adj_nation, col_idx, encroach,
      threat, threat_nation, angry, self_stack, (unsigned)map->prime_resource_seed
    );
  }

  int best = -1;
  int best_dir = 8;
  int best_flags = 0;
  int grudge = 0; /* [bp-0x14] — NOT reset on the other-tribe owner arm (DOS) */

  struct ai_021a_ctx c;
  memset(&c, 0, sizeof(c));
  c.rng = rng;
  c.map = map;
  c.units = units;
  c.col1 = col1;
  c.colonies = colonies;
  c.u = u;
  c.nation_id = nation_id;
  c.x = x;
  c.y = y;
  c.indian = indian;
  c.turn_w = turn_w;
  c.cool = cool;
  c.unit_fa = unit_fa;
  c.unit_river = unit_river;
  c.home = home;
  c.dump = dump;
  c.adj_foreign = adj_foreign;
  c.adj_nation = adj_nation;
  c.continent = continent;
  c.col_dist = col_dist;
  c.col_idx = col_idx;
  c.col = col;
  c.village = village;
  c.vx = vx;
  c.vy = vy;
  c.home_reach = home_reach;
  c.encroach = encroach;
  c.threat = threat;
  c.threat_nation = threat_nation;
  c.angry = angry;
  c.visit_turn = visit_turn;
  c.self_stack = self_stack;
  c.lone = lone;
  c.ind = ind;
  c.tech = tech;
  c.facing = facing;
  c.grudge = grudge;
  c.best = best;
  c.best_dir = best_dir;
  c.best_flags = best_flags;

  for (int d = 0; d < 9; ++d) {
    c.d = d;
    ai_021a_score_dir(&c);
  }
  best = c.best;
  best_dir = c.best_dir;
  best_flags = c.best_flags;
  if (dump) {
    fprintf(stderr, "AI_021A best=%d score=%d flags=%02x\n", best_dir, best, best_flags);
  }
  out->dir = best_dir;
  out->flags = best_flags;
  return best_dir;
}

/*
 * FUN_4d56_021a post-loop tail (021a:1277..14e6). Returns the final dir (8 =
 * stay). Not ported: the human-colony warning chrome (021a:1329..13ae) and
 * the pending-encounter resolver call (2a1f_0192 -> 5bfb_3180) — the port's
 * §9 contact pass after the pulse plays the visit from the unit's position,
 * so here the bit only costs the act, as in DOS.
 */
static int ai_native_021a_tail(
  ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
  AiRng* rng,
  ColonizeUnit* u,
  int nation_id,
  int dir,
  int flags
) {
  const int indian = nation_id - 4;
  ColonizeCol1Indian* ind = (indian >= 0 && indian < 8) ? &col1->indian[indian] : NULL;
  const int turn_w = (int)col1->head.turn;
  const int cool = turn_w + turn_w / -25;
  int owner = -1;
  if (flags & 0x0a) {
    const int dx = (dir < 8) ? u->x + k_ai_dir8_dx[dir] : u->x;
    const int dy = (dir < 8) ? u->y + k_ai_dir8_dy[dir] : u->y;
    owner = ai_owner_nibble(map, dx, dy);
    if (ind && owner >= 0 && owner < 4) {
      if (ind->contact_state[owner] == 2) {
        return 8;
      }
      ind->contact_state[owner] = 1;
    }
  }
  const int hostile = flags & 0x04;
  if (flags & 0x0a) {
    /* 021a:12f8 — a contact/attack step needs a full move left. */
    if (units_max_mp(units, u->id) - u->moves < 3) {
      return 8;
    }
  }
  if ((flags & 0x0a) || hostile) {
    u->col1_flags15 &= (uint8_t)~0x08u;
    flags &= 0x7f;
  }
  if (u->col1_flags15 & 0x08u) {
    /* 021a:13cc — pending encounter at own tile resolves, unit stays. */
    u->col1_flags15 &= (uint8_t)~0x08u;
    return 8;
  }
  if (flags & 0x80) {
    ai_021a_set_visit_turn(u, turn_w);
    u->col1_flags15 |= 0x08u;
  }
  if ((flags & 0x8a) == 0 && s_ai_native_colonies) {
    /* 021a:1419 — march on an encroaching colony after the long cooldown. */
    const int continent = ai_continent_id(map, u->x, u->y);
    int col_dist = 9999;
    const int col_idx = ai_goals_nearest_colony_15eb_0142(
      map, s_ai_native_colonies, u->x, u->y, -1, continent, &col_dist
    );
    const int home = u->home_tribe_id;
    const ColonizeCol1Tribe* village =
      (home >= 0 && home < (int)col1->head.tribe_count) ? &col1->tribe[home] : NULL;
    if (col_idx >= 0 && village) {
      const ColonizeColony* col = &s_ai_native_colonies->colonies[col_idx];
      const int d = map_dos_dist(col->x - (int)village->x, col->y - (int)village->y);
      int lim = (int)((int8_t)col->population >> 1);
      if (lim < 2) {
        lim = 2;
      }
      if (lim >= d) {
        const int cn = col->nation_id;
        const int census = (cn >= 0 && cn < 4) ? (int)col1->stuff.census_pop_proxy[cn] : 0;
        const int cx = (census >> 3) + d * 4 + 10;
        if (cx <= cool - ai_021a_visit_turn(u)) {
          const int save_gx = u->goto_x;
          const int save_gy = u->goto_y;
          u->goto_x = col->x;
          u->goto_y = col->y;
          int sx = 0;
          int sy = 0;
          const bool ok =
            units_next_goto_step_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(units), .colonies=(ColonizeColonyPool*)(s_ai_native_colonies), .map=(ColonizeWorldMap*)(map), .rng=(ColonizeDosRng*)(rng)}, u->id, &sx, &sy);
          u->goto_x = save_gx;
          u->goto_y = save_gy;
          if (ok) {
            int step = -1;
            for (int k = 0; k < 8; ++k) {
              if (u->x + k_ai_dir8_dx[k] == sx && u->y + k_ai_dir8_dy[k] == sy) {
                step = k;
              }
            }
            if (step >= 0) {
              const int so = ai_021a_settle_owner(map, sx, sy);
              if (so < 0 || so == nation_id) {
                const int po = map_tile_owner_or_presence(map, sx, sy);
                if (po < 0 || po == nation_id) {
                  return step;
                }
              }
            }
          }
        }
      }
    }
  }
  /* 021a:14ca — quiet step onto foreign-owned land only with a fresh allotment. */
  if ((flags & 0x1a) == 0 && (flags & 0x01) && u->moves != 0) {
    return 8;
  }
  return dir;
}

/* AI_021A_TRACE=1 — one line per Brave act (pick, flags, final dir). */
COLONIZE_INTERNAL int ai_021a_trace_enabled(void) {
  static int cached = -1;
  if (cached < 0) {
    const char* e = getenv("AI_021A_TRACE");
    cached = (e && e[0] == '1') ? 1 : 0;
  }
  return cached;
}

/* AI_BRAVE_PICK=20e6 — live opt-in fallback to the retired 20e6-shaped quiet scorer. */
static int ai_brave_pick_20e6_fallback(void) {
  static int cached = -1;
  if (cached < 0) {
    const char* e = getenv("AI_BRAVE_PICK");
    cached = (e && strcmp(e, "20e6") == 0) ? 1 : 0;
  }
  return cached;
}

static int ai_native_pick_dir(
  AiRng* rng,
  const ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  const ColonizeCol1Save* col1,
  const ColonizeUnit* u,
  int nation_id,
  int last_dir,
  Ai021aResult* res
) {
  res->dir = 8;
  res->flags = 0;
  if (ai_brave_pick_20e6_fallback() || !col1) {
    res->dir = ai_native_pick_dir_asm(rng, map, units, u->x, u->y, nation_id, last_dir);
    return res->dir;
  }
  int dir = ai_native_pick_dir_021a(
    rng, map, units, col1, s_ai_native_colonies, u, nation_id, res
  );
  dir = ai_native_apply_seed100_peels(
    nation_id, u->x, u->y, dir, ai_score_at_match(nation_id, u->x, u->y), NULL, NULL, 0
  );
  res->dir = dir;
  return dir;
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
  int spent = map_dos_terr_cost_byte(terr) * 3;
  /* FUN_465b: both mask flags &0x0a → cost 1 (ASM TEST AL,0xa). */
  const int fa_from = ai_mask_fa_flags(map, from_x, from_y);
  const int fa_to = ai_mask_fa_flags(map, to_x, to_y);
  if (fa_from != 0 && fa_to != 0) {
    spent = 1;
  }
  /* FUN_281f_072c: both terrain &0x40 (minor river) and cardinal → cost 1.
   * (Mask road bit 0x40 is a different plane; 465b uses the terrain reader.) */
  const int river_from = (int)(map_get_terrain_or(map, from_x, from_y, 25) & 0x40u);
  const int river_to = (int)(map_get_terrain_or(map, to_x, to_y, 25) & 0x40u);
  if (river_from != 0 && river_to != 0 && (dir & 1) == 0) {
    spent = 1;
  }
  /* FUN_465b / 06be: cap spent at 3 when dest has tribe flag + owner. */
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

/* T4.6 (closed statically 2026-09-08): the seed-100 Brave "writer after ADD"
 * was FUN_5bfb_022e's exhaust tail (LAB_5bfb_1005) — 465b's commit tail runs
 * 0984 (adjacent-foreign probe) -> 2a1f_0192 -> FUN_5bfb_3180 -> 2a1f_066c ->
 * FUN_5bfb_022e; on a FIRST contact (met bit 0x20 clear) the ceremony runs and
 * the tail exhausts the MOVER when it is Indian (0934 -> 1427_155e, spent :=
 * max MP = 3 for a Brave). Both TURN2->3 rows had an unmet Euro land unit
 * adjacent to the dest tile (France soldier at (50,38); Spain units at
 * (47,53)/(47,54)) — Euro phase runs BEFORE the Indian phase (dump_1816 /
 * vr_2a02_v3). Ported in ai_contact_indian_meet_trade's first-meet arm; the
 * AI_EMPIRICISM-only Brave end-state overlay tables that stood in for it were
 * deleted 2026-09-14 with the rest of the empirical picker. */

/*
 * Init-only LCG burns after the first Brave step of the Inca pulse
 * (`ai_init_new_game` / post-`6a09`). Calibrated to SEED100.SAV; every
 * other tribe needs exactly 0 (Sioux, also 6 villages, misses with any
 * k in 1..10), so this is not a per-village draw.
 *
 * 2026-09-16 static audit (no DOS source found, and none can exist on the
 * visible path): every FUN_281f_04d4/04ca/0d90/0e68 call site in all 31
 * overlays plus every direct FUN_19ef_0032/002c caller in resident code was
 * enumerated and attributed. Between 1816's entry reseed (BIOS tick read
 * FUN_1c0c_0012, which VR_SEED.EXE patches to `mov ax,100`) and the first
 * 021a pick the only RNG consumers are the ones already modelled: 152e's
 * met-Euro loop (needs FUN_15b3_0004 bit 0x20, all zero at start), the
 * 465b overspend gate (needs spent != 0), 3180/022e (need a foreign
 * neighbour) and the UI pump's music picker (FUN_129f_00f6, which reseeds
 * from the tick on BOTH sides of its draws, so it nets a reset, never a
 * burn). SEED100_REGEN1/2 are byte-identical here, so it is not wall-clock.
 * The golden is a weak constraint: with k=0 only Brave 1 at (9,28) misses,
 * a 1-point near-tie (bases 209/208/208 decided by RNG(1,5)), and k in
 * {6,14,25,32} before Brave 1 all pass, as does "reseed + 3 draws before
 * every Brave". Treat 6 as a fit for that single near-tie, same class as
 * the k_mid_peels residue; AI_INIT_SCHED (below) is the sweep tool.
 */
static void ai_native_post_first_brave_burns(AiRng* rng, int nation_id) {
  int burns = 0;
  if (nation_id == 4) {
    burns = 6;
  }
  for (int b = 0; b < burns; ++b) {
    (void)ai_rng_next_counted(rng);
  }
  if (ai_lcg_audit_enabled() && burns > 0) {
    fprintf(stderr, "AI_LCG_AUDIT post_first_brave n=%d burns=%d\n", nation_id, burns);
  }
}

/*
 * Init-pulse burn-schedule sweep tool. AI_INIT_SCHED="n:idx:count[:R];..."
 * burns `count` draws before Brave `idx` of nation `n` picks (idx = -1:
 * before that nation's pulse); a trailing `R` reseeds to the pulse seed
 * first. Setting it disables the default Inca burns above.
 */
static bool ai_init_sched_apply(AiRng* rng, int nation_id, int brave_index) {
  const char* p = getenv("AI_INIT_SCHED");
  if (!p) {
    return false;
  }
  while (*p) {
    int n = 0;
    int idx = 0;
    int cnt = 0;
    int used = 0;
    char r = 0;
    if (sscanf(p, "%d:%d:%d:%c%n", &n, &idx, &cnt, &r, &used) < 4) {
      r = 0;
      if (sscanf(p, "%d:%d:%d%n", &n, &idx, &cnt, &used) < 3) {
        break;
      }
    }
    if (n == nation_id && idx == brave_index) {
      if (r == 'R') {
        dos_rng_seed(rng, s_ai_init_pulse_seed);
      }
      for (int b = 0; b < cnt; ++b) {
        (void)ai_rng_next_counted(rng);
      }
      if (ai_lcg_audit_enabled()) {
        fprintf(stderr, "AI_LCG_AUDIT sched n=%d idx=%d burns=%d reset=%d\n", n, idx, cnt, r == 'R');
      }
    }
    p += used;
    while (*p == ';') {
      ++p;
    }
  }
  return true;
}

/*
 * FUN_4d56_1816 unit-action core (one pulse): reseed caller-side via 04ca,
 * then while MP remain, one 14fe-style action per step (FUN_465b spent add).
 *
 * 2026-09-08 — 1816 §7/§8 + FUN_4d56_14fe re-read off the raw asm (Ghidra
 * mis-resolves every `PUSH CS; CALL` in this overlay into CODE_112, so the
 * decompile's callee names are wrong; the real map is the 15-entry JMPF stub
 * table at 4d56:4c22..4c6c, turn/mid_pass_indian_rank.md).
 *
 *   §7  4d56:1a6c..1a8a  for u in 0..DS:0x539c: if (u+0x3147 & 0xf) ==
 *       DS:0x5394 -> u+0x315a = 0.  (+0x315a = COL1 unit +0x16 =
 *       `col1_counter16`, the per-unit act counter.)
 *   §8  4d56:1a8c..1b1a
 *         do { ui_pump(281f:0470); acted = 0;
 *              for (i = 0; !acted && i < DS:0x539c; ) {
 *                while (FUN_281f_097a(i)) {      // AX-register arg
 *                  ++u[i]+0x315a;
 *                  if (u[i]+0x315a <= 0x14) { 14fe(i); acted = 1; }
 *                  else { FUN_281f_0934(i); u[i]+0x315a = 0; }
 *                }
 *                ++i;
 *              }
 *         } while (acted);
 *       FUN_281f_097a -> FUN_1427_13b0 (:8766) gates on: 0 <= i < unit count,
 *       (int8)u+0x3144 >= 0, (u+0x3147 & 0xf) == DS:0x5394, (u+0x3148 & 0x80)
 *       == 0 || type == 0x0b, and u+0x3149 (spent) < the unit's max MP.
 *       The `acted` restart re-scans from index 0, but a unit that just acted
 *       still has MP, so DOS drains one unit fully before moving on — the same
 *       order this per-unit loop uses.
 *   14fe (4d56:14fe..152c) is only three branches:
 *         dir = FUN_4d56_021a(unit)             // near CALL 4c31 -> stub 4c3b
 *         if (dir != 8) FUN_2a1f_0150(unit, dir)  // -> FUN_465b_0c1e step
 *         else if (dir >= 0) FUN_281f_0934(unit)  // exhaust MP; test is dead,
 *                                                 // dir is 8 on that arm
 *       FUN_4d56_021a already exhausts on its own dir == 8 exit (021a:14e6),
 *       so the single `moves = max_mp` below covers both writes.
 *
 * The four 021a/§8 deltas above the pulse loop (per-attempt act counter +
 * 0x14 cap, full-byte facing write incl. stay=8, homeless despawn, in-field
 * arm/mount on stay) were WIRED 2026-09-08 — see the inline citations in
 * ai_native_nation_pulse. Two deliberate port guards, documented inline:
 * the stay/move orders latch only touches NONE/FORTIFY/FORTIFIED (DOS
 * escorts leave 021a via the raid dispatch, never the wander tail, so the
 * Linux FOLLOW/GOTO machinery must survive the latch), and a Brave upgraded
 * to a mounted type keeps this pulse's max_mp=3 until the next turn refresh
 * (DOS re-reads 090c per act; the port's spent-byte semantics make the
 * difference invisible outside the upgrade turn itself).
 */
/* Pre-pulse Brave tiles for this turn — see ai_native_brave_turn_origin. */
static int16_t s_brave_origin_x[COLONIZE_UNITS_MAX];
static int16_t s_brave_origin_y[COLONIZE_UNITS_MAX];
static uint8_t s_brave_origin_ok[COLONIZE_UNITS_MAX];

/*
 * FUN_5bfb_3180 on a BRAVE step (465b commit tail -> 0984 -> 0192 -> 3180 ->
 * 022e): first contact with a Euro unit/colony beside the new tile is opened
 * at the step itself, and the mover's MP is exhausted (LAB_5bfb_1005,
 * brave_spent_callgraph.md). Later acts of the same nation in the same pulse
 * therefore already read MET (seed-100 TURN2: the Sioux Brave at (45,52)
 * meets Spain, so the (48,56) Brave scores the Spanish soldier as met).
 * The turn context is stashed here because the pulse itself is ctx-free.
 */
static ColonizeTurnContext* s_ai_native_ctx = NULL;
static uint8_t s_ai_first_contact_this_turn[8][4];

int ai_native_first_contact_this_turn(int nation_id, int euro_nation) {
  if (nation_id < 4 || nation_id > 11 || euro_nation < 0 || euro_nation > 3) {
    return 0;
  }
  return s_ai_first_contact_this_turn[nation_id - 4][euro_nation] != 0;
}

/* Returns 1 when a first contact fired (MP exhausted by the caller). */
static int ai_native_step_first_contact(
  ColonizeUnitPool* units, const ColonizeWorldMap* map, ColonizeCol1Save* col1,
  ColonizeUnit* u, int nation_id
) {
  if (!s_ai_native_ctx || !col1 || nation_id < 4 || nation_id > 11) {
    return 0;
  }
  ColonizeCol1Indian* ind = &col1->indian[nation_id - 4];
  int fired = 0;
  int done[4] = {0, 0, 0, 0};
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + k_ai_dir8_dx[d];
    const int ny = u->y + k_ai_dir8_dy[d];
    if (!map_coords_inset(map, nx, ny)) {
      continue;
    }
    int e = -1;
    const int oid = units_id_at(units, nx, ny);
    const ColonizeUnit* o = oid >= 0 ? units_get_const(units, oid) : NULL;
    if (o && o->active && o->nation_id >= 0 && o->nation_id <= 3 && !units_is_sea(units, oid)) {
      e = o->nation_id;
    } else if (s_ai_native_colonies) {
      const int cid = colonies_id_at(s_ai_native_colonies, nx, ny);
      const ColonizeColony* c = cid >= 0 ? colonies_get(s_ai_native_colonies, cid) : NULL;
      if (c && c->active && c->nation_id >= 0 && c->nation_id <= 3) {
        e = c->nation_id;
      }
    }
    if (e < 0 || done[e]) {
      continue;
    }
    done[e] = 1; /* 3180: one 022e per other nation per scan (aiStack_20) */
    if (ind->euro_diplo[e] == 0) {
      (void)ai_contact_try_first_welcome(s_ai_native_ctx, e, nation_id);
      s_ai_first_contact_this_turn[nation_id - 4][e] = 1;
      fired = 1;
      continue;
    }
    /*
     * Already met: 022e's visit-mood rolls (raw 96735-96760) — skipped when
     * the mover carries the pending-encounter bit, stands on an Indian
     * settlement tile (FUN_137f_0392 >= 0), or has no home village.
     */
    if (fired || (u->col1_flags15 & 0x08u) || ai_021a_settle_owner(map, u->x, u->y) >= 4 ||
        u->home_tribe_id < 0) {
      continue;
    }
    if (ai_021a_trace_enabled()) {
      fprintf(stderr, "AI_021A_VISITROLL n=%d xy=(%d,%d) e=%d\n", nation_id, u->x, u->y, e);
    }
    (void)ai_contact_visit_step_roll(s_ai_native_ctx, nation_id, e, u->id);
  }
  return fired;
}

void ai_native_note_brave_turn_origin(int unit_id, int x, int y) {
  if (unit_id < 0 || unit_id >= COLONIZE_UNITS_MAX) {
    return;
  }
  s_brave_origin_x[unit_id] = (int16_t)x;
  s_brave_origin_y[unit_id] = (int16_t)y;
  s_brave_origin_ok[unit_id] = 1;
}

int ai_native_brave_turn_origin(int unit_id, int* out_x, int* out_y) {
  if (unit_id < 0 || unit_id >= COLONIZE_UNITS_MAX || !s_brave_origin_ok[unit_id]) {
    return 0;
  }
  if (out_x) {
    *out_x = (int)s_brave_origin_x[unit_id];
  }
  if (out_y) {
    *out_y = (int)s_brave_origin_y[unit_id];
  }
  return 1;
}

/*
 * bugs.md ("I refused, and they didn't attack my wagon train next turn"):
 * a Brave whose HOME village holds an attitude word over 0x7f toward a
 * European (LAB_5bfb_0ff2's refused-demand +0x80 — the same `0x7f <` band
 * FUN_521d_0906 reads out of DS:0x54f6) is an ALARMED unit: DOS's 021a
 * dispatches it to the raid/attack path instead of the quiet 14fe wander.
 * That alarmed dispatch is PARKED in the port, and §9's raid pass runs AFTER
 * this pulse and needs remaining MP — so a grudge-carrying Brave wandered its
 * whole allotment away and could never answer the refusal. Leave it standing
 * for the §9 raid dispatch when the offender is right there.
 */
static bool ai_native_brave_grudge_hold(
  const ColonizeUnitPool* units, const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1, const ColonizeUnit* u
) {
  if (!units || !col1 || !col1->tribe || !u) {
    return false;
  }
  if (u->home_tribe_id < 0 || u->home_tribe_id >= (int)col1->head.tribe_count) {
    return false;
  }
  const ColonizeCol1Tribe* t = &col1->tribe[u->home_tribe_id];
  for (int e = 0; e < 4; ++e) {
    if (col1_tribe_attitude(t, e) <= 0x7f) {
      continue;
    }
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* f = &units->units[i];
      if (!f->active || f->nation_id != e || !units_is_on_map(f)) {
        continue;
      }
      if (abs(f->x - u->x) <= 1 && abs(f->y - u->y) <= 1) {
        return true;
      }
    }
    if (colonies) {
      for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
        const ColonizeColony* c = &colonies->colonies[ci];
        if (!c->active || c->nation_id != e) {
          continue;
        }
        if (abs(c->x - u->x) <= 1 && abs(c->y - u->y) <= 1) {
          return true;
        }
      }
    }
  }
  return false;
}
typedef enum {
  AI_NATIVE_STEP_MORE = 0, /* the Brave may act again (loop continues) */
  AI_NATIVE_STEP_STOP = 1  /* the Brave is done this turn (was a bare `break;`) */
} AiNativeStepStatus;

/*
 * One FUN_1427_13b0 act for one Brave: MP gate, grudge hold, 021a direction
 * pick, the partial-MP gamble and the commit. Extracted verbatim from
 * ai_native_nation_pulse.
 */
static AiNativeStepStatus ai_native_brave_step(
  ColonizeUnitPool* units, ColonizeWorldMap* map, ColonizeCol1Save* col1, AiRng* rng,
  int nation_id, bool seed100_init_burns, ColonizeUnit* u, int hx, int hy, int tech,
  int max_mp, int brave_index, int* steps
) {
  /* FUN_281f_097a / 1427_13b0: act while moves_spent < max_mp (=3).
   * River/fa cost=1 steps keep spent < 3 so the inner loop continues —
   * that is the multi-step path (not a second act after spent >= max). */
  const int spent = u->moves;
  if (spent >= max_mp) {
    return AI_NATIVE_STEP_STOP;
  }
  /*
   * 1816 §8 (4d56:1af3..1b1a): the act counter bumps once per ATTEMPT,
   * before 021a runs — a Brave that only stays still ends its turn at 1.
   * Past 0x14 the unit is exhausted and the counter zeroed, no act.
   */
  u->col1_counter16++;
  if (u->col1_counter16 > 0x14) {
    u->moves = max_mp;
    u->col1_counter16 = 0;
    return AI_NATIVE_STEP_STOP;
  }
  /*
   * 021a:0337..0365 — a unit whose home village slot is out of range
   * (u+0x314a < 0 or >= DS:0x539a) is destroyed (FUN_281f_0808), not
   * re-homed; 021a returns -1 and 14fe's "step" lands on the freed slot.
   */
  if (col1 &&
      (u->home_tribe_id < 0 ||
       u->home_tribe_id >= (int)col1->head.tribe_count)) {
    units_despawn(units, u->id);
    return AI_NATIVE_STEP_STOP;
  }
  /* Alarmed dispatch stand-in — see ai_native_brave_grudge_hold. */
  if (ai_native_brave_grudge_hold(units, s_ai_native_colonies, col1, u)) {
    u->col1_counter16--;
    return AI_NATIVE_STEP_STOP;
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
      *steps
    );
  }
  /* DOS reads unit+0x314f raw; values outside 0..7 (8 = stayed last
   * act) legitimately disable the facing term — do NOT clamp to 0. */
  const int last_dir = u->last_dir;
  if (seed100_init_burns && *steps == 0) {
    (void)ai_init_sched_apply(rng, nation_id, brave_index);
  }
  s_ai_native_home_dist = map_dos_dist(u->x - hx, u->y - hy); /* DS:0x8db8 */
  (void)tech;
  Ai021aResult pick;
  int dir = ai_native_pick_dir(rng, map, units, col1, u, nation_id, last_dir, &pick);
  if (!ai_brave_pick_20e6_fallback() && col1) {
    const int picked = dir;
    dir = ai_native_021a_tail(units, map, col1, rng, u, nation_id, dir, pick.flags);
    if (ai_021a_trace_enabled()) {
      fprintf(
        stderr,
        "AI_021A_ACT t=%d n=%d idx=%d xy=(%d,%d) facing=%d spent=%d tw=%d pick=%d flags=%02x dir=%d\n",
        s_ai_seed100_midturn_turn, nation_id, brave_index, u->x, u->y, last_dir,
        u->moves, u->col1_counter16, picked, pick.flags, dir
      );
    }
  }
  if (dir < 0 || dir > 7) {
    /*
     * Stay (dir == 8). 021a:11b9 writes the picked dir into the facing
     * byte (COL1 +0x0b) unconditionally — the full byte value 8 (facing
     * bits 0 + pad bit0 in the save split). Keep last_dir = 8 in memory:
     * 521d:54f5 skips the facing term for any byte >= 8.
     */
    u->last_dir = 8;
    u->col1_facing_pad = 1;
    /*
     * 021a:11cd orders latch: stay -> 5 (FORTIFY), repeat stay -> 6
     * (FORTIFIED) — the DOS byte values equal the port enum. DOS stomps
     * any orders byte; the port latches only over NONE/FORTIFY/FORTIFIED
     * so the Linux-side FOLLOW/GOTO escort machinery survives (DOS
     * escorts exit 021a through the raid dispatch, never this tail).
     */
    if (u->orders == UNITS_ORDER_NONE || u->orders == UNITS_ORDER_FORTIFY ||
        u->orders == UNITS_ORDER_FORTIFIED) {
      u->orders = (u->orders == UNITS_ORDER_NONE) ? UNITS_ORDER_FORTIFY
                                                  : UNITS_ORDER_FORTIFIED;
    }
    /*
     * 021a:11ef..126c in-field arm/mount, gated on standing on a
     * settlement tile owned by this nation (FUN_281f_06be
     * tile_tribe_owner == nation): type 0x13/0x15 with tribe muskets > 0
     * (signed byte) -> ++type, musket spent on rng(0, difficulty) == 0;
     * then horse_breeding >= 0x19 with max MP <= 3 (FUN_281f_090c, read
     * AFTER the musket arm) -> type += 2, horse_breeding -= 0x19.
     * (Field-upgrade path; the 152e spawn path uses 0x31/0x32.)
     */
    if (col1 && nation_id >= 4 && nation_id <= 11 &&
        map_tile_has_city(map, u->x, u->y) &&
        ai_owner_nibble(map, u->x, u->y) == nation_id) {
      ColonizeCol1Indian* ind = &col1->indian[nation_id - 4];
      if ((int8_t)ind->muskets > 0 &&
          (u->type_index == UNITS_KIND_BRAVE || u->type_index == UNITS_KIND_MTD_BRAVE)) {
        u->type_index++;
        if (ai_rng_range(rng, 0, (int)col1->head.difficulty) == 0) {
          ind->muskets--;
        }
      }
      if (ind->horse_breeding >= 0x19 && units_max_mp(units, u->id) <= 3) {
        u->type_index += 2;
        ind->horse_breeding -= 0x19;
      }
    }
    u->moves = max_mp;
    return AI_NATIVE_STEP_STOP;
  }
  const int nx = u->x + k_ai_dir8_dx[dir];
  const int ny = u->y + k_ai_dir8_dy[dir];
  const int cost = ai_dos_move_spent(map, u->x, u->y, nx, ny, dir);
  const int from_x = u->x;
  const int from_y = u->y;
  /*
   * FUN_465b_0000 cost gate (viceroy_unpacked.c 75643-75647 + the
   * `else` at :75820): `(cost <= left) || (spent == 0) || (04ca(timer),
   * attack)`; a quiet step that overspends with MP already spent RESEEDS
   * the LCG from the timer word (= the fixed seed under VR_SEED / --seed)
   * and then rolls RNG(1, cost) — only `roll <= left` moves. The denied
   * unit stays put with `spent += cost` already booked at :75617, its
   * facing already stamped by 021a. Seed-100 TURN2: the Arawak Brave's
   * river second step (spent 1, cost 9) is exactly this roll, and every
   * later Arawak act reads the restarted stream.
   */
  if (spent != 0 && cost > max_mp - spent) {
    dos_rng_seed(rng, ai_turn_seed(s_ai_native_ctx));
    const int roll = ai_rng_range(rng, 1, cost);
    if (ai_021a_trace_enabled()) {
      fprintf(
        stderr, "AI_021A_GAMBLE n=%d xy=(%d,%d) dir=%d cost=%d left=%d roll=%d -> %s\n",
        nation_id, u->x, u->y, dir, cost, max_mp - spent, roll,
        roll <= max_mp - spent ? "move" : "denied"
      );
    }
    if (roll > max_mp - spent) {
      u->moves = spent + cost;
      u->last_dir = dir;
      u->col1_facing_pad = 0;
      if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
        u->orders = UNITS_ORDER_NONE;
      }
      (*steps)++;
      return AI_NATIVE_STEP_STOP;
    }
  }
  if (ai_step_audit_enabled() && s_ai_seed100_midturn_turn > 0) {
    fprintf(
      stderr,
      "AI_STEP_AUDIT t=%d n=%d from=(%d,%d) dir=%d to=(%d,%d) cost=%d spent_before=%d "
      "tw=%d step=%d\n",
      s_ai_seed100_midturn_turn,
      nation_id,
      from_x,
      from_y,
      dir,
      nx,
      ny,
      cost,
      spent,
      u->col1_counter16,
      *steps
    );
  }
  {
    const int step_ox = u->x;
    const int step_oy = u->y;
    u->x = nx;
    u->y = ny;
    units_occupancy_notify_moved(units, step_ox, step_oy, nx, ny);
    /* 465b commit tail clears+recomputes unit+0x3147's observed nibble
     * (FUN_281f_08da / 084e / 07fe) on every step — braves included. */
    units_vis_mask_after_move(units, map, u->id, nx, ny);
  }
  u->moves = spent + cost;
  /*
   * FUN_465b LAB_465b_05ca: ocean/HS flag change AND
   * euro_settlement_owner(from) < 0 AND euro_settlement_owner(dest) < 0
   * → spent = max_mp (FUN_281f_090c).
   * euro_settlement = tribe bit + Euro owner 0..3 (FUN_137f_0358).
   */
  if (ai_is_ocean_hs(map, from_x, from_y) != ai_is_ocean_hs(map, nx, ny)) {
    const int from_euro_set =
      ((ai_layer2_at(map, from_x, from_y) & 2u) != 0 &&
       ai_owner_nibble(map, from_x, from_y) >= 0 &&
       ai_owner_nibble(map, from_x, from_y) < 4);
    const int to_euro_set =
      ((ai_layer2_at(map, nx, ny) & 2u) != 0 && ai_owner_nibble(map, nx, ny) >= 0 &&
       ai_owner_nibble(map, nx, ny) < 4);
    if (!from_euro_set && !to_euro_set) {
      u->moves = max_mp;
    }
  }
  /* 021a:11b9 full-byte facing write (pad cleared on a real dir), and
   * 021a:126e — any move resets the stay latch (guarded as above). */
  u->last_dir = dir;
  u->col1_facing_pad = 0;
  if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
    u->orders = UNITS_ORDER_NONE;
  }
  ai_set_owner_nibble_move(map, nx, ny, nation_id);
  if (!seed100_init_burns && ai_native_step_first_contact(units, map, col1, u, nation_id)) {
    u->moves = max_mp; /* LAB_5bfb_1005: FUN_281f_0934 on the Indian mover */
    (*steps)++;
    return AI_NATIVE_STEP_STOP;
  }
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
  (*steps)++;
  if (seed100_init_burns && brave_index == 0 && *steps == 1 && !getenv("AI_INIT_SCHED")) {
    ai_native_post_first_brave_burns(rng, nation_id);
  }
  /*
   * The DOS 0x14 act cap now trips on `col1_counter16` at the attempt top
   * (4d56:1af7). `cost <= 0` stays as a Linux-only belt (DOS has no such
   * break — it keeps acting until the counter or MP gate trips).
   */
  if (cost <= 0) {
    return AI_NATIVE_STEP_STOP;
  }
  return AI_NATIVE_STEP_MORE;
}


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
  if (seed100_init_burns) {
    (void)ai_init_sched_apply(rng, nation_id, -1);
  }
  memset(s_ai_first_contact_this_turn[nation_id - 4], 0, sizeof(s_ai_first_contact_this_turn[0]));

  const int max_mp = 3; /* Brave thirds allotment (FUN_281f_090c path) */
  /*
   * Mid-turn FUN_4d56_1816 prelude: DOS burns nothing on the shared stream
   * before the act loop except the 152e growth-loop draws (already on
   * ctx->rng via ai_grow_villages) and the post-independence alarm roll. The
   * old "Inca=14 / Aztec=4" burns were a fit against the retired 20e6-shaped
   * scorer; with the 021a picker they cost every Inca act (2026-09-15
   * sweep: k=0 -> zero Inca misses on all six seed-100 turns).
   */
  if (ai_lcg_audit_enabled() && seed100_init_burns) {
    fprintf(
      stderr,
      "AI_AB pulse_enter n=%d nexts=%u rng_state=0x%x mode=%s\n",
      nation_id,
      (unsigned)s_ai_lcg_total_nexts,
      (unsigned)rng->state,
      "asm"
    );
  }

  /* Clear col1_counter16 for this nation's Braves (DOS 1816 ~81630). */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (u->active && u->nation_id == nation_id) {
      u->col1_counter16 = 0;
    }
  }

  /*
   * Remember every Brave's pre-pulse tile — see ai_native_brave_turn_origin
   * in ai.h for why the §9 contact arms need it. Stamped fresh for this
   * nation's units on every pulse, and §9 runs immediately after this pulse
   * for the same nation, so a reader never sees another nation's turn.
   */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    ai_native_note_brave_turn_origin(u->id, u->x, u->y);
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
      if (ai_native_brave_step(
            units, map, col1, rng, nation_id, seed100_init_burns, u, hx, hy, tech,
            max_mp, brave_index, &steps
          ) == AI_NATIVE_STEP_STOP) {
        break;
      }
    }
    brave_index++;
  }

  s_ai_seed100_init_pulse = 0;
}

/*
 * ===========================================================================
 * FUN_4d56_1b3a — the year-loop Indian mid-pass (raw viceroy_unpacked.c:
 * 81684-81738). Ported 2026-09-06d; previously "partial (known; not raid)"
 * with only phase 2 present.
 *
 * DOS structure, in order:
 *   phase 1  clear DS:0x5b04 — 8 Indian slots x 4 Euro words
 *   phase 2  for slot 0..7: if !(DS:0x5ad9 + 0x4e*slot & 0x80) -> 1816(slot)
 *   phase 3  for every colony: claim worked ring tiles off the natives
 *
 * Phase 2 is already the Linux `TURN_PROC_INDIAN` cursor loop (the DOS call
 * looks like `FUN_41f2_0266` only because of the reloc-0000 stub misresolve
 * documented in turn/mid_pass_indian_rank.md). The two halves below are
 * phases 1 and 3, and they bracket that loop exactly as DOS does.
 * ===========================================================================
 */

/*
 * Phase 1 — `for (i=0;i<8;i++) for (j=0;j<4;j++) word[(i*0x27 + j)*2 +
 * 0x5b04] = 0;`.
 *
 * Address decode: the Indian record base is DS:0x5ad6 with stride 0x4e (=
 * 0x27 words), so `0x5b04 = 0x5ad6 + 0x2e` and the four words are exactly
 * `ColonizeCol1Indian.contact_state[4]` (+0x2e). Nothing new to name.
 *
 * **Behavioural correction this exposes:** ai_contact.c's beg/gift arm
 * described `contact_state` as a permanent per-(tribe, Euro) latch ("a tribe
 * that has ever brought gifts to a nation never begs from it again"). DOS
 * wipes all 32 entries at the top of every FUN_4d56_1b3a call — once per
 * game turn (viceroy 81704-81707) — so the latch is really *per-turn*: one
 * gift-or-beg resolution per tribe/nation pair per turn. That is why DOS
 * villages keep visiting instead of going quiet forever.
 */
void ai_indian_midpass_clear_tables(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  for (int slot = 0; slot < 8; ++slot) {
    ColonizeCol1Indian* ind = &ctx->col1->indian[slot];
    for (int e = 0; e < 4; ++e) {
      ind->contact_state[e] = 0;
    }
  }
}

/*
 * Phase 3 — worked-tile ownership claim.
 *
 *   for each colony c:
 *     owner  = c[0x1a]                       // colony nation id
 *     count  = DS:0x329[FUN_281f_0c5e()]     // ring size for the Town-Hall tier
 *     for i in 0..count-1:
 *       if (c[0x70 + i] < 0) continue;       // tile not being worked
 *       x = c.x + DS:0xc8[i]; y = c.y + DS:0xde[i];
 *       n = FUN_281f_06dc(x, y);             // = FUN_137f_0200 owner nibble
 *       if (n <= 3 || n == owner) continue;  // only take from natives
 *       if (FUN_281f_06d2(x, y) >= 0) continue;  // = FUN_137f_0428:
 *                                            //   settlement (layer2 0x02) or
 *                                            //   unit (layer2 0x01) present
 *       FUN_281f_0704(x, y, owner);          // = FUN_137f_0228 owner stamp
 *
 * So: a colonist actually working a tile the natives still claim quietly
 * transfers that tile's owner nibble to the colony — unless a village or any
 * unit is standing on it. `0704`'s @SEIZURE-style popup arm can't fire here
 * (it needs a native settlement on the tile, which the `06d2` gate already
 * excluded), so this is a pure ownership stamp with no chrome.
 *
 * Ring mapping: Linux drives the loop off `colonies_field_tile_delta` +
 * `colony->tiles[]` rather than re-deriving DS:0xc8/0xde by index. DOS's own
 * enumeration order differs from `colony.h`'s `tiles[]` convention, but both
 * pair index -> offset self-consistently and cover the identical 8-tile set,
 * so the set of tiles claimed is the same. Tiers 3/4's outer ring (DS:0x329
 * = 12/20) has no Linux storage — the deliberate P4.2 decision, and every
 * real DOS `.SAV` leaves those slots 0xff, so nothing is skipped in practice.
 */
void ai_indian_midpass_claim_worked_tiles(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->colonies || !ctx->map) {
    return;
  }
  for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
    ColonizeColony* c = &ctx->colonies->colonies[ci];
    if (!c->active || c->nation_id < 0 || c->nation_id > 3) {
      continue;
    }
    for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES; ++i) {
      if (c->tiles[i] < 0) {
        continue; /* DOS: `colony[0x70 + i] < 0` — nobody works this plot. */
      }
      int dx = 0;
      int dy = 0;
      if (!colonies_field_tile_delta(i, &dx, &dy)) {
        continue;
      }
      const int tx = c->x + dx;
      const int ty = c->y + dy;
      if (!map_coords_inset(ctx->map, tx, ty)) {
        continue;
      }
      const int owner = ai_owner_nibble(ctx->map, tx, ty);
      if (owner < 4 || owner == c->nation_id) {
        continue;
      }
      /* FUN_137f_0428: settlement (0x02) or unit (0x01) occupancy blocks it. */
      if ((ai_layer2_at(ctx->map, tx, ty) & 0x03u) != 0) {
        continue;
      }
      map_set_owner_nibble(ctx->map, tx, ty, c->nation_id);
    }
  }
}

void ai_indian_nation_turn(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || nation_id < 4 || nation_id > 11) {
    return;
  }
  /*
   * FUN_4d56_1816 phase order (annotated indian_nation_turn / indian_contact.md):
   *   1 reseed → 2–4 prelude/clamp → 5 growth → 6 relation → 7–8 quiet pulse →
   *   9 meet/trade + raids (other paths; not inside 14fe).
   * Pulse LCG burns (Inca=14 / Aztec=4) stay inside ai_native_nation_pulse after
   * reseed — prelude uses isolated contact RNG only.
   */
  ai_nation_reseed(ctx);

  /* §2 WoI tribe defection (isolated RNG, no pulse LCG burn). */
  ai_contact_indian_woi_defect(ctx, nation_id);

  /* §2–4 alarm prelude (flags/mission); LCG stream-cost burns remain in pulse. */
  ai_contact_indian_prelude(ctx, nation_id);

  /* §5 tribe growth. */
  ai_grow_villages(ctx, nation_id);

  /* §6 relation / goods tick. */
  ai_contact_indian_relation_tick(ctx, nation_id);

  AiRng local;
  AiRng* rng = ctx->rng;
  if (!rng) {
    const uint32_t seed = ai_turn_seed(ctx);
    dos_rng_seed(&local, seed);
    rng = &local;
  }
  /* Mid-turn pulse always runs; seed-100 latches the calendar turn so the
   * mid-turn dir peels below can key off it. */
  if (ctx->rng_seed == 100u && ctx->turn_number) {
    s_ai_seed100_midturn_turn = (int)*ctx->turn_number;
  }

  /* §7–8 quiet 14fe act loop (+ seed-100 overlays). */
  s_ai_native_colonies = ctx->colonies;
  s_ai_native_col1 = ctx->col1_ok ? ctx->col1 : NULL;
  s_ai_native_ctx = ctx;
  ai_native_nation_pulse(
    ctx->units, ctx->map, ctx->col1_ok ? ctx->col1 : NULL, rng, nation_id, false
  );
  s_ai_native_ctx = NULL;

  s_ai_seed100_midturn_turn = 0;

  /* §9 meet/trade + raids (5bfb / 4528 paths — not quiet 14fe). */
  ai_contact_indian_meet_trade(ctx, nation_id);
  /*
   * bugs.md 2026-09-04: FUN_5bfb_022e's peaceful visit picks ONE of two
   * halves — generous (@INDIANGIVEFOOD/@INDIANGIVESTUFF) or demanding
   * (@INDIANBEGFOOD / tribute). The gift half was never wired, so the only
   * peaceful visitor the player ever saw was a beggar.
   */
  if (!ai_contact_try_village_gifts(ctx, nation_id)) {
    ai_contact_try_village_beg_food(ctx, nation_id);
  }
  ai_contact_indian_raids(ctx, nation_id);
}

/*
 * Linux-only whole-nation wipe. The DOS analogue is FUN_4d56_01e2 (raw
 * :81352, asm 4d56:01e2..0219): nation = param_1 + 4, then walk the tribe
 * array DOWNWARD (i = DS:0x539a - 1 .. 0, stride 0x12 at DS:0x54ec) and call
 * FUN_4d56_00e0(i) — i.e. col1_destroy_tribe_at — for every row whose +2
 * nation byte matches; descending order is what keeps 00e0's array compaction
 * from skipping rows.
 *
 * 2026-09-08: 01e2 is DEAD CODE in the shipped VICEROY.EXE. It is not one of
 * the 15 entries in overlay 0x0C's export stub table (4d56:4c22..4c6c), no
 * near CALL/JMP anywhere in segment 4d56 targets 0x01e2, and no overlay bank
 * record JMPFs to it. So this helper is a Linux invention, not a port of it,
 * and it deliberately does more than an 01e2 loop would: it despawns *every*
 * unit of the nation (00e0 only despawns units whose +0x314a names the village
 * being razed), paints owner nibble 0x0f, and resets the indian[] slot, while
 * skipping 00e0's per-village horse fold / village-count decrement / @EXTINCT
 * popup (those live in col1_destroy_tribe_at).
 */
int col1_kill_indian_nation_w(
  const ColonizeWorld* w,
  int nation_id
) {
  ColonizeCol1Save* col1 = w->col1;
  ColonizeUnitPool* units = w->units;
  ColonizeWorldMap* map = w->map;

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
          map_set_owner_nibble(map, (int)t->x, (int)t->y, 0x0f);
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

    /* DS:0x54f6 attitude[euro] needs no remap of its own: it is field +10 of
     * the settlement record (tribe.alarm[4]), so it moved with the tribe
     * array compaction above. The parallel `indian_tension` array this used
     * to shift was a misdecode and is gone (2026-09-08). */
    free(remap);
  }

  /* Reset fixed indian[] slot (keep tech). */
  if (col1) {
    const int idx = nation_id - 4;
    ColonizeCol1Indian* ind = &col1->indian[idx];
    const uint8_t tech = ind->tech;
    memset(ind, 0, sizeof(*ind));
    ind->tech = tech;
    /*
     * The 15b3 matrix is symmetric storage in two different Linux fields —
     * nation[e].relation_by_indian[idx] is the Euro→tribe direction, and the
     * indian[idx].euro_diplo[e] the memset above just cleared is the other —
     * so a raw one-sided assignment here was the last write to it outside the
     * FUN_15b3_0066/00d0 pair helpers (audit #16 class). DOS never assigns
     * the byte: its two idioms are FUN_43f7_0108's `clear_both(0xb)` +
     * `or_both(0x60)` surrender (raw 73555-73557) and the new-game reset,
     * which zeroes the whole 12-wide row a column at a time
     * (`for (c = 0; c < 0xc; ++c) *(nation*0x13c + c - 0x77c4) = 0`, raw
     * 121620-121622). This site is the second: a nation that no longer
     * exists has no relation with anybody, in either direction. Routed
     * through ai_diplo_clear_both with a full mask so both halves fall
     * together and ai_diplo_write's dual-mode addressing (plus its
     * player.diplomacy mirror) is the only channel into the matrix.
     *
     * FUN_4d56_00e0, the per-village DOS razer this whole helper stands in
     * for, does not touch the matrix at all (raw 81292-81346) — it only ORs
     * the extinct bit 0x80 into indian[].+3 — which is why this is a Linux
     * invention routed to the DOS helper rather than a port of a DOS write.
     */
    for (int e = 0; e < 4; ++e) {
      ai_diplo_clear_both(col1, e, nation_id, 0xffu);
    }
  }

  return removed;
}

/*
 * New-game / load hook (sibling of ai_euro_reset / ai_goals_reset /
 * founding_fathers_reset / ai_contact_reset): zeroes this module's
 * per-unit/per-pulse statics that would otherwise leak across a new game
 * or Load in the same process. Restores each to its declaration-time
 * initializer (the dangling ctx pointer to NULL; everything else to 0,
 * matching how these arrays start at program launch).
 */
void ai_native_reset(void) {
  s_ai_seed100_init_pulse = 0;
  s_ai_seed100_midturn_turn = 0;
  s_ai_lcg_in_pick = 0;
  s_ai_lcg_pick_burns = 0;
  s_ai_lcg_total_nexts = 0;
  memset(s_brave_origin_x, 0, sizeof(s_brave_origin_x));
  memset(s_brave_origin_y, 0, sizeof(s_brave_origin_y));
  memset(s_brave_origin_ok, 0, sizeof(s_brave_origin_ok));
  s_ai_native_ctx = NULL;
  memset(s_ai_first_contact_this_turn, 0, sizeof(s_ai_first_contact_this_turn));
}
