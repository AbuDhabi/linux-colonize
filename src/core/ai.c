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
 * Sections:
 *  - Startup RNG seeding
 *  - Coarse fog-of-war tracking
 *  - Diagnostics/audit gates & RNG range utils
 *  - New-game world setup: nation identity, col1 template, landfall, fleet & tribe placement
 *  - Game init entry & human-nation id fixups
 *  - Euro nation turn dispatch
 */

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
 * Sections:
 *  - Startup RNG seeding (~line 33)
 *  - Coarse fog-of-war tracking (~line 95)
 *  - Diagnostics/audit gates & RNG range utils (~line 187)
 *  - New-game world setup: nation identity, col1 template, landfall, fleet & tribe placement (~line 300)
 *  - Game init entry & human-nation id fixups (~line 1138)
 *  - Euro nation turn dispatch (~line 1309)
 *  - Indian village worth, brave spawn, mission & threat scoring (~line 1417)
 *  - Village growth tick, tile/owner helpers & native pull scoring (~line 2084)
 *  - Native direction-scoring ASM port (~line 2485)
 *  - 021a direction-scorer structural port (tile/occupant/terrain/angry/score) (~line 2864)
 *  - Native pick-dir dispatch, move-spent accounting & init schedule (~line 3966)
 *  - First contact, brave-turn origin & brave step/order execution (~line 4195)
 *  - Nation pulse, indian nation turn, kill-nation & reset (~line 4640)
 */

/*
 * FUN_38fd_6024 (viceroy_unpacked.c ~68666-68677): human starting treasury
 * by difficulty — Discoverer 1000, Explorer 300, Conquistador+ 0. AI nations
 * always start at 0. See docs/difficulty.md.
 */
/* ===================== Startup RNG seeding (ai_starting_gold .. ai_turn_seed) ===================== */
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

uint32_t ai_turn_seed(const ColonizeTurnContext* ctx) {
  if (!ctx) {
    return 100u;
  }
  if (ctx->rng_seed_set) {
    return ctx->rng_seed;
  }
  return ctx->rng_seed ? ctx->rng_seed : 100u;
}

static const char* k_new_country[4] = {
  "", "", "", ""
};
static const char* k_default_leaders[4] = {
  "", "", "", ""
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

/* ===================== Coarse fog-of-war tracking (ai_coarse_fog_clear .. ai_coarse_fog_tribe_byte) ===================== */
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
int ai_coarse_fog_unseen(int x, int y) {
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

uint8_t ai_coarse_fog_explore_byte(int x, int y) {
  const int ix = ai_coarse_fog_explore_index(x, y);
  if (ix < 0 || ix >= AI_COARSE_FOG_SIZE) {
    return 0xff;
  }
  return s_ai_coarse_fog[ix];
}

uint8_t ai_coarse_fog_tribe_byte(int x, int y) {
  const int ix = ai_coarse_fog_tribe_index(x, y);
  if (ix < 0 || ix >= AI_COARSE_FOG_SIZE) {
    return 0xff;
  }
  return s_ai_coarse_fog[ix];
}

/* Set AI_LCG_AUDIT=1 to log init-pulse pick_dir burn counts (phase 5). */
/* ===================== Diagnostics/audit gates & RNG range utils (ai_lcg_audit_enabled .. ai_rng_next_counted) ===================== */
int ai_lcg_audit_enabled(void) {
  static int cached = -1;
  if (cached < 0) {
    const char* e = getenv("AI_LCG_AUDIT");
    cached = (e && e[0] && e[0] != '0') ? 1 : 0;
  }
  return cached;
}

/* AI_SCORE_AT="n:x:y[,n:x:y...]" — extra pick_dir score-dump targets. */
int ai_score_at_match(int nation_id, int x, int y) {
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
int ai_step_audit_enabled(void) {
  static int cached = -1;
  if (cached < 0) {
    const char* e = getenv("AI_STEP_AUDIT");
    cached = (e && e[0] && e[0] != '0') ? 1 : 0;
  }
  return cached;
}

/* Seed-100 init pulse: peels + select quiet ASM. */
int ai_s_seed100_init_pulse;
uint32_t ai_s_init_pulse_seed;
/* Calendar turn after advance during seed-100 mid-turn pulse (0 = not mid-turn). */
int ai_s_seed100_midturn_turn;

/* AI_PEEL_AUDIT=1: classify each firing peel row against both branch scorers. */
int ai_peel_audit_enabled(void) {
  static int cached = -1;
  if (cached < 0) {
    const char* e = getenv("AI_PEEL_AUDIT");
    cached = (e && e[0] == '1') ? 1 : 0;
  }
  return cached;
}

int ai_peel_audit_argmax(const int score[8]) {
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
int ai_brave_peels_disabled(void) {
  static int cached = -1;
  if (cached < 0) {
    const char* e = getenv("AI_NO_BRAVE_PEELS");
    cached = (e && e[0] && e[0] != '0') ? 1 : 0;
  }
  return cached;
}

int ai_s_lcg_in_pick;
int ai_s_lcg_pick_burns;
uint32_t ai_s_lcg_total_nexts;

int ai_rng_range(AiRng* rng, int lo, int hi_inclusive) {
  if (ai_s_lcg_in_pick) {
    ai_s_lcg_pick_burns++;
  }
  ai_s_lcg_total_nexts++;
  return dos_rng_range(rng, lo, hi_inclusive);
}

uint16_t ai_rng_next_counted(AiRng* rng) {
  ai_s_lcg_total_nexts++;
  return dos_rng_next(rng);
}

/* ===================== New-game world setup: nation identity, col1 template, landfall, fleet & tribe placement (ai_set_nation_identity .. ai_install_tribes) ===================== */
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
    /* NAMES.TXT @COLONYNAME rows 0-3 (New England/France/Spain/Netherlands);
       k_new_country[] is the fallback when the names catalog is absent. */
    const char* country_name =
      assets_msg_line_or(p->names, "COLONYNAME", i, k_new_country[i]);
    ai_set_nation_identity(p->col1, i, i == human ? 0 : 1, leader, country_name);
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
int ai_tribe_initial_pop(uint8_t tech) {
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
const int k_ai_dir8_dx[9] = {0, 1, 1, 1, 0, -1, -1, -1, 0};
const int k_ai_dir8_dy[9] = {-1, -1, 0, 1, 1, 1, 0, -1, 0};

uint8_t ai_layer2_at(const ColonizeWorldMap* map, int x, int y) {
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
int ai_mask_fa_flags(const ColonizeWorldMap* map, int x, int y) {
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
void ai_set_owner_nibble_move(ColonizeWorldMap* map, int x, int y, int nation) {
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
int ai_continent_id(const ColonizeWorldMap* map, int x, int y) {
  return (int)(map_get_layer3(map, x, y) & 0x0fu);
}

/* FUN_13e4_0074 / FUN_281f_0768 — ocean or high seas only. */
int ai_is_ocean_hs(const ColonizeWorldMap* map, int x, int y) {
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
  const int brave = units_kind_type_index(units, UNITS_KIND_BRAVE);
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

/* ===================== Game init entry & human-nation id fixups (ai_fix_human_nation_ids .. ai_init_new_game) ===================== */
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
    ai_s_init_pulse_seed = pulse_seed;
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

/* ===================== Euro nation turn dispatch (ai_nation_reseed .. ai_euro_nation_turn) ===================== */
void ai_nation_reseed(ColonizeTurnContext* ctx) {
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
