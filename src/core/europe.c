#include "core/europe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/founding_fathers.h"
#include "core/popup_msg.h"
#include "platform/diagnostics.h"
#include "core/ss.h"
#include "core/strutil.h"
#include "core/units.h"
#include "platform/diagnostics.h"

/* Sound hook (unit tests build europe.c without sound.c — same shape as
 * units_set_combat_music_hooks). */
static void (*g_europe_sound_play)(int id) = NULL;
static void (*g_europe_set_bgm)(int pool) = NULL;
#include "platform/platform.h"

static void europe_refresh_recruit_passage(EuropeScreen* eu);

/* Deterministic LCG for pool fills when no external rng passed. */
static unsigned europe_rng_next(unsigned* state) {
  unsigned s = state ? *state : 1u;
  s = s * 1103515245u + 12345u;
  if (state) {
    *state = s;
  }
  return (s >> 16) & 0x7fffu;
}

static void europe_trim(char* s) {
  char* start = s;
  while (*start == ' ' || *start == '\t') {
    ++start;
  }
  if (start != s) {
    memmove(s, start, strlen(start) + 1);
  }
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) {
    s[--n] = '\0';
  }
}

static void europe_remap_sheet_to_palette(
  ColonizeSpriteSheet* sheet,
  const ColonizePalette* dst_pal
) {
  if (!sheet || !dst_pal || !sheet->has_palette) {
    return;
  }
  uint8_t lut[256];
  for (int i = 0; i < 256; ++i) {
    if (i == COLONIZE_SS_TRANSPARENT) {
      lut[i] = (uint8_t)COLONIZE_SS_TRANSPARENT;
      continue;
    }
    const int sr = sheet->palette.rgb[i][0];
    const int sg = sheet->palette.rgb[i][1];
    const int sb = sheet->palette.rgb[i][2];
    int best = 0;
    int best_d = 1 << 30;
    for (int j = 0; j < 256; ++j) {
      const int dr = sr - dst_pal->rgb[j][0];
      const int dg = sg - dst_pal->rgb[j][1];
      const int db = sb - dst_pal->rgb[j][2];
      const int d = dr * dr + dg * dg + db * db;
      if (d < best_d) {
        best_d = d;
        best = j;
      }
    }
    lut[i] = (uint8_t)best;
  }
  for (int s = 0; s < sheet->sprite_count; ++s) {
    ColonizeSprite* sp = &sheet->sprites[s];
    if (!sp->pixels) {
      continue;
    }
    const size_t n = (size_t)sp->width * (size_t)sp->height;
    for (size_t i = 0; i < n; ++i) {
      sp->pixels[i] = lut[sp->pixels[i]];
    }
  }
  sheet->palette = *dst_pal;
}

static bool europe_parse_int_field(const char** cursor, int* out) {
  while (**cursor == ' ' || **cursor == '\t' || **cursor == ',') {
    ++(*cursor);
  }
  if (**cursor == '\0') {
    return false;
  }
  char* end = NULL;
  long v = strtol(*cursor, &end, 10);
  if (end == *cursor) {
    return false;
  }
  *out = (int)v;
  *cursor = end;
  return true;
}

static void europe_set_status(EuropeScreen* eu, const char* text) {
  if (!eu) {
    return;
  }
  snprintf(eu->status, sizeof(eu->status), "%s", text ? text : "");
}

static void europe_copy_ship(EuropeHarborShip* dst, const EuropeHarborShip* src) {
  if (!dst || !src) {
    return;
  }
  *dst = *src;
}

static void europe_clear_ship(EuropeHarborShip* s) {
  if (!s) {
    return;
  }
  memset(s, 0, sizeof(*s));
  s->type_index = -1;
  for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
    s->cargo_professions[i] = -1;
  }
}

static int europe_goods_slots_used(const EuropeHarborShip* ship) {
  if (!ship) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
    if (ship->hold_goods_amount[i] > 0 && ship->hold_goods_amount[i] < 255) {
      n++;
    }
  }
  return n;
}

static int europe_ship_cargo_cap(const EuropeHarborShip* ship, const ColonizeUnitPool* units) {
  if (!ship) {
    return 0;
  }
  if (units) {
    int ti = ship->type_index;
    if (ti < 0) {
      ti = units_find_type(units, ship->name);
    }
    const ColonizeUnitType* ut = units_type(units, ti);
    if (ut && ut->cargo > 0) {
      return ut->cargo > EUROPE_SHIP_CARGO_MAX ? EUROPE_SHIP_CARGO_MAX : ut->cargo;
    }
  }
  return EUROPE_SHIP_CARGO_MAX;
}

/* Insert at dock front (index 0). Returns false if docks are full. */
static bool europe_dock_push_front(
  EuropeScreen* eu,
  const char* name,
  int profession,
  bool sentry
) {
  if (!eu || eu->dock_count >= EUROPE_DOCK_MAX) {
    return false;
  }
  for (int i = eu->dock_count; i > 0; --i) {
    eu->dock[i] = eu->dock[i - 1];
  }
  EuropeDockImmigrant* d = &eu->dock[0];
  memset(d, 0, sizeof(*d));
  snprintf(d->name, sizeof(d->name), "%s", name ? name : "Colonists");
  d->profession = profession;
  d->present = true;
  d->sentry = sentry;
  d->dos_type = europe_dock_type_for(d->name, profession);
  eu->dock_count++;
  return true;
}

bool europe_dock_push_load(EuropeScreen* eu, const char* name, int profession) {
  if (!eu || eu->dock_count >= EUROPE_DOCK_MAX) {
    return false;
  }
  EuropeDockImmigrant* slot = &eu->dock[eu->dock_count++];
  memset(slot, 0, sizeof(*slot));
  snprintf(slot->name, sizeof(slot->name), "%s", name ? name : "Colonists");
  slot->profession = profession;
  slot->present = true;
  slot->sentry = true;
  slot->dos_type = europe_dock_type_for(slot->name, profession);
  return true;
}

bool europe_dock_slot_pos(int index, int* out_x, int* out_y) {
  /*
   * FUN_38fd_146c: tier 0 = the first EUROPE_DOCK_ROW0 slots on the upper quay,
   * tier 1 = the next EUROPE_DOCK_ROW1 on the lower one, both starting from the
   * same base x. Tier 2 (anything beyond) is computed but never blitted.
   */
  if (index < 0 || index >= EUROPE_DOCK_ROW0 + EUROPE_DOCK_ROW1) {
    return false;
  }
  int col = index;
  int y = EUROPE_DOCK_Y;
  if (index >= EUROPE_DOCK_ROW0) {
    col = index - EUROPE_DOCK_ROW0;
    y = EUROPE_DOCK_Y2;
  }
  if (out_x) {
    *out_x = EUROPE_DOCK_X + col * EUROPE_DOCK_PITCH;
  }
  if (out_y) {
    *out_y = y;
  }
  return true;
}

static int europe_type_is_treasure(const ColonizeUnitPool* units, int type_tag) {
  if (!units || type_tag < 0) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(units, type_tag);
  return ut && ut->name[0] && strstr(ut->name, "Treasure") != NULL;
}

/*
 * Treasure passengers cash in (or PARK without inventing gold) and are removed
 * before dock unload — they are not immigrants. Cite: Colonization.pdf Treasure
 * Trains; GAME.TXT @LOOTCASH / @CASHTREASURE.
 */
static void europe_cash_treasure_passengers(
  EuropeScreen* eu,
  EuropeHarborShip* ship,
  const ColonizeUnitPool* units
) {
  if (!eu || !ship || ship->cargo_count <= 0) {
    return;
  }
  int w = 0;
  for (int i = 0; i < ship->cargo_count; ++i) {
    const int tag = ship->cargo_types[i];
    const int gold = ship->cargo_treasure_gold[i];
    if (europe_type_is_treasure(units, tag)) {
      if (gold > 0) {
        (void)europe_cash_treasure(eu, gold);
      } else {
        /*
         * gold==0 here means cargo_treasure_gold was never filled for this
         * slot — game_loop.c's game_europe_fill_expected_treasure_gold (the
         * H/Return-to-Europe path) now does the real fill, so this is a
         * defensive no-op for any other boarding path, not the live gap
         * this comment used to describe. Still don't invent a rate/value.
         */
      }
      continue;
    }
    if (w != i) {
      ship->cargo_types[w] = tag;
      ship->cargo_professions[w] = ship->cargo_professions[i];
      ship->cargo_treasure_gold[w] = gold;
    }
    ++w;
  }
  for (int i = w; i < ship->cargo_count; ++i) {
    ship->cargo_types[i] = 0;
    ship->cargo_professions[i] = -1;
    ship->cargo_treasure_gold[i] = 0;
  }
  ship->cargo_count = w;
}

/* Unload passengers onto dock front (preserves on-board order); clear holds. */
static void europe_disembark_passengers_to_dock(
  EuropeScreen* eu,
  EuropeHarborShip* ship,
  const ColonizeUnitPool* units
) {
  if (!eu || !ship || ship->cargo_count <= 0) {
    return;
  }
  europe_cash_treasure_passengers(eu, ship, units);
  for (int i = ship->cargo_count - 1; i >= 0; --i) {
    char name[40];
    const int tag = ship->cargo_types[i];
    const int prof = ship->cargo_professions[i];
    if (tag == -2) {
      snprintf(name, sizeof(name), "%s", "Artillery");
    } else if (units) {
      const ColonizeUnitType* ut = units_type(units, tag);
      if (ut && ut->name[0]) {
        snprintf(name, sizeof(name), "%s", ut->name);
      } else {
        snprintf(name, sizeof(name), "%s", "Free Colonists");
      }
    } else {
      snprintf(name, sizeof(name), "%s", "Free Colonists");
    }
    /* Passengers keep sentry ("board next") — same convention as aboard ship. */
    if (!europe_dock_push_front(eu, name, prof, true)) {
      snprintf(eu->status, sizeof(eu->status), "%s", "Docks are full — some passengers remain aboard.");
      /* Leave remaining passengers (0..i) on the ship. */
      ship->cargo_count = i + 1;
      return;
    }
  }
  ship->cargo_count = 0;
  memset(ship->cargo_types, 0, sizeof(ship->cargo_types));
  memset(ship->cargo_treasure_gold, 0, sizeof(ship->cargo_treasure_gold));
  for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
    ship->cargo_professions[i] = -1;
  }
}

/* Screenshot / DOS purchase list (no Man-O-War). Oracle: original_screenshots/europe/purchase.png */
static void europe_init_purchase_table(EuropeScreen* eu) {
  static const EuropePurchaseOption k_opts[] = {
    {"Artillery", 500, false},
    {"Caravel", 1000, true},
    {"Merchantman", 2000, true},
    {"Galleon", 3000, true},
    {"Privateer", 2000, true},
    {"Frigate", 5000, true},
  };
  eu->purchase_count = 0;
  for (size_t i = 0; i < sizeof(k_opts) / sizeof(k_opts[0]) && eu->purchase_count < EUROPE_PURCHASE_MAX;
       ++i) {
    eu->purchase[eu->purchase_count++] = k_opts[i];
  }
}

/*
 * Pool candidates: basic classes + trainable experts (job index).
 * Draw weights Unverified vs DOS — mix favors common colonists.
 */
typedef struct EuropePoolCand {
  const char* name;
  int profession;
  int weight;
} EuropePoolCand;

static const EuropePoolCand k_pool_cands[] = {
  {"Petty Criminals", 26, 8},
  {"Indentured Servants", 25, 10},
  {"Free Colonists", 19, 12},
  {"Expert Farmers", 0, 4},
  {"Expert Lumberjacks", 5, 3},
  {"Expert Ore Miners", 6, 3},
  {"Expert Fishermen", 8, 3},
  {"Master Carpenters", 13, 3},
  {"Master Blacksmiths", 14, 2},
  {"Master Gunsmiths", 15, 2},
  {"Hardy Pioneers", 20, 3},
  {"Veteran Soldiers", 21, 2},
  {"Seasoned Scouts", 22, 2},
  {"Jesuit Missionaries", 24, 2},
  {"Elder Statesmen", 17, 1},
  {"Firebrand Preachers", 16, 1},
  {"Expert Teachers", 18, 1},
  {"Master Distiller", 9, 2},
  {"Master Tobacconists", 10, 2},
  {"Master Weavers", 11, 2},
  {"Master Fur Traders", 12, 2},
  {"Expert Silver Miners", 7, 2},
};

/* bugs.md: Brewster's ban covers the EXISTING pool too — slots rolled
 * before the flag rose still held Petty Criminals / Indentured Servants,
 * so the Recruit list and the docks disagreed with the Brewster pick
 * dialog. DOS FUN_4345_0342 case 0x14 walks the three pool slot bytes and
 * overwrites 0x19/0x1a (servant/criminal) with 0x1c — job NONE, which
 * FUN_38fd_4884 draws as Free Colonists (0x1c→0x13 label swap). A direct
 * substitution, not a reroll (bugs.md 230 kept it idempotent). */
void europe_apply_brewster(EuropeScreen* eu, int owned) {
  if (!eu || !owned) {
    return;
  }
  eu->brewster_no_criminals = true;
  for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
    if (eu->pool[i].filled &&
        (eu->pool[i].profession == 25 || eu->pool[i].profession == 26)) {
      snprintf(eu->pool[i].name, sizeof(eu->pool[i].name), "Free Colonists");
      eu->pool[i].profession = 19;
    }
  }
}

void europe_refill_pool_slot(EuropeScreen* eu, int slot, unsigned* rng_state) {
  if (!eu || slot < 0 || slot >= EUROPE_POOL_SIZE) {
    return;
  }
  unsigned local = 1u + (unsigned)(eu->gold + eu->recruit_passage + slot * 17);
  unsigned* st = rng_state ? rng_state : &local;
  int total = 0;
  for (size_t i = 0; i < sizeof(k_pool_cands) / sizeof(k_pool_cands[0]); ++i) {
    total += k_pool_cands[i].weight;
  }
  if (total <= 0) {
    return;
  }
  int pick = (int)(europe_rng_next(st) % (unsigned)total);
  for (size_t i = 0; i < sizeof(k_pool_cands) / sizeof(k_pool_cands[0]); ++i) {
    pick -= k_pool_cands[i].weight;
    if (pick < 0) {
      EuropePoolSlot* p = &eu->pool[slot];
      /* Brewster: DOS FUN_38fd_46d4 rolls the tier first and only then
       * checks FF 0x14 — a criminal (0x1a) or servant (0x19) result is
       * returned as 0x1c (Free Colonists) instead. Substitute here so their
       * roll mass moves to Free Colonists rather than being redistributed
       * across the whole candidate table. */
      if (eu->brewster_no_criminals &&
          (k_pool_cands[i].profession == 25 || k_pool_cands[i].profession == 26)) {
        snprintf(p->name, sizeof(p->name), "Free Colonists");
        p->profession = 19;
      } else {
        snprintf(p->name, sizeof(p->name), "%s", k_pool_cands[i].name);
        p->profession = k_pool_cands[i].profession;
      }
      p->filled = true;
      return;
    }
  }
}

void europe_pool_ensure_filled(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
    if (!eu->pool[i].filled) {
      europe_refill_pool_slot(eu, i, NULL);
    }
  }
}

const char* europe_pool_label(const EuropeScreen* eu, int slot) {
  if (!eu || slot < 0 || slot >= EUROPE_POOL_SIZE) {
    return "Colonist";
  }
  const EuropePoolSlot* p = &eu->pool[slot];
  return (p->filled && p->name[0]) ? p->name : "Colonist";
}

static void europe_init_pool(EuropeScreen* eu) {
  unsigned rng = 42u;
  for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
    europe_refill_pool_slot(eu, i, &rng);
  }
}

/*
 * DOS `FUN_38fd_41ce` (the Train dialog) collects every @JOB with a positive
 * hire cost in job order, exactly as the loop below does, and then hands the
 * cost array and the parallel job-id array to `FUN_291f_0ed0` ->
 * `FUN_1cf8_000a` before drawing a single row. That routine is a sort: it
 * walks for the first descending step, lifts that element out (shifting the
 * tail left), finds the first slot whose key is >= the lifted key, shifts
 * right and drops it in — an ascending sort by cost. So the Train list is
 * ordered cheapest-first, not by @JOB index the way this port had it
 * (bugs.md).
 *
 * Transcribed rather than replaced with a qsort because the tie order is
 * observable and is this algorithm's own: an element only moves on a strictly
 * descending step, and re-enters *before* every equal key. With stock
 * NAMES.TXT that puts Carpenters before Fishermen at 1000, Farmers before
 * Distiller at 1100, and Pioneers before Tobacconists at 1200. Costs come
 * from NAMES.TXT, so a modded table has to re-sort the same way.
 */
static void europe_sort_train_by_cost(EuropeTrainOption* a, int n) {
  if (!a || n < 2) {
    return;
  }
  int i = 0;
  while (i < n - 1) {
    if (a[i + 1].cost >= a[i].cost) {
      ++i;
      continue;
    }
    const EuropeTrainOption lifted = a[i + 1];
    for (int k = 0; k < n - 2 - i; ++k) {
      a[i + 1 + k] = a[i + 2 + k];
    }
    int pos = 0;
    while (pos < n - 1 && a[pos].cost < lifted.cost) {
      ++pos;
    }
    for (int k = n - 2; k >= pos; --k) {
      a[k + 1] = a[k];
    }
    a[pos] = lifted;
    /* DOS does not rewind `local_12` here — the scan resumes where it was. */
  }
}

static bool europe_load_tables(EuropeScreen* eu, const ColonizeMsgCatalog* names) {
  eu->cargo_count = 0;
  eu->class_count = 0;
  eu->train_count = 0;

  const ColonizeMsgSection* cargo = assets_msg_find(names, "CARGO");
  if (cargo) {
    for (int i = 0; i < cargo->line_count && eu->cargo_count < EUROPE_CARGO_MAX; ++i) {
      char line[COLONIZE_MSG_LINE_LEN];
      snprintf(line, sizeof(line), "%s", cargo->lines[i]);
      if (line[0] == ';' || line[0] == '\0') {
        continue;
      }
      char* comma = strchr(line, ',');
      if (!comma) {
        continue;
      }
      *comma = '\0';
      europe_trim(line);
      if (line[0] == '\0') {
        continue;
      }

      const char* p = comma + 1;
      int start_lo = 0;
      int start_hi = 0;
      int low = 0;
      int high = 0;
      int burden = 0;
      int rise = 0;
      int fall = 0;
      int attrition = 0;
      int volatility = 0;
      if (!europe_parse_int_field(&p, &start_lo) || !europe_parse_int_field(&p, &start_hi) ||
          !europe_parse_int_field(&p, &low) || !europe_parse_int_field(&p, &high) ||
          !europe_parse_int_field(&p, &burden) || !europe_parse_int_field(&p, &rise) ||
          !europe_parse_int_field(&p, &fall) || !europe_parse_int_field(&p, &attrition) ||
          !europe_parse_int_field(&p, &volatility)) {
        continue;
      }
      (void)start_hi;

      EuropeCargoQuote* q = &eu->cargo[eu->cargo_count++];
      str_copy_trunc(q->name, sizeof(q->name), line);
      q->bid = start_lo;
      if (q->bid < 0) {
        q->bid = 0;
      }
      q->low = low;
      q->high = high;
      q->burden = burden;
      q->rise = rise;
      q->fall = fall;
      q->attrition = attrition;
      q->volatility = volatility;
      if (q->volatility < 0) {
        q->volatility = 0;
      }
      if (q->volatility > 15) {
        q->volatility = 15;
      }
      q->ask = q->bid + q->burden;
    }
  }
  memset(eu->trade_nr, 0, sizeof(eu->trade_nr));

  const ColonizeMsgSection* classes = assets_msg_find(names, "CLASS");
  if (classes) {
    for (int i = 0; i < classes->line_count && eu->class_count < EUROPE_CLASS_MAX; ++i) {
      char line[COLONIZE_MSG_LINE_LEN];
      snprintf(line, sizeof(line), "%s", classes->lines[i]);
      if (line[0] == ';' || line[0] == '\0') {
        continue;
      }
      char* comma = strchr(line, ',');
      if (!comma) {
        continue;
      }
      *comma = '\0';
      europe_trim(line);
      const char* p = comma + 1;
      int cost = 0;
      if (!europe_parse_int_field(&p, &cost) || cost <= 0 || line[0] == '\0') {
        continue;
      }
      EuropeRecruitClass* c = &eu->classes[eu->class_count++];
      str_copy_trunc(c->name, sizeof(c->name), line);
      c->cost = cost;
    }
  }

  /* @JOB: name, expert_name, school_tier, europe_hire_cost */
  const ColonizeMsgSection* jobs = assets_msg_find(names, "JOB");
  if (jobs) {
    int job_index = 0;
    for (int i = 0; i < jobs->line_count && eu->train_count < EUROPE_TRAIN_MAX; ++i) {
      char line[COLONIZE_MSG_LINE_LEN];
      snprintf(line, sizeof(line), "%s", jobs->lines[i]);
      if (line[0] == ';' || line[0] == '\0') {
        continue;
      }
      char* c1 = strchr(line, ',');
      if (!c1) {
        continue;
      }
      *c1 = '\0';
      europe_trim(line);
      char* c2 = strchr(c1 + 1, ',');
      if (!c2) {
        ++job_index;
        continue;
      }
      *c2 = '\0';
      char expert[40];
      snprintf(expert, sizeof(expert), "%s", c1 + 1);
      europe_trim(expert);
      const char* p = c2 + 1;
      int tier = 0;
      int cost = 0;
      if (!europe_parse_int_field(&p, &tier) || !europe_parse_int_field(&p, &cost)) {
        ++job_index;
        continue;
      }
      (void)tier;
      (void)line;
      if (cost > 0 && expert[0] != '\0') {
        EuropeTrainOption* t = &eu->train[eu->train_count++];
        snprintf(t->expert_name, sizeof(t->expert_name), "%s", expert);
        t->job_index = job_index;
        t->cost = cost;
      }
      ++job_index;
    }
    europe_sort_train_by_cost(eu->train, eu->train_count);
  }

  const ColonizeMsgSection* home = assets_msg_find(names, "HOMEPORT");
  const ColonizeMsgSection* cname = assets_msg_find(names, "COLONYNAME");
  (void)home;
  (void)cname;

  europe_init_purchase_table(eu);
  return eu->cargo_count > 0;
}

void europe_set_nation(EuropeScreen* eu, int nation, const ColonizeMsgCatalog* names) {
  static const char* k_ports[4] = {"London", "La Rochelle", "Seville", "Amsterdam"};
  static const char* k_nations[4] = {"England", "France", "Spain", "Netherlands"};
  static const char* k_regions[4] = {
    "New England", "New France", "New Spain", "New Netherlands"
  };
  if (!eu) {
    return;
  }
  if (nation < 0 || nation > 3) {
    nation = 0;
  }
  if (names) {
    const ColonizeMsgSection* home = assets_msg_find(names, "HOMEPORT");
    const ColonizeMsgSection* reg = assets_msg_find(names, "COLONYNAME");
    if (home && nation >= 0 && nation < home->line_count) {
      char line[COLONIZE_MSG_LINE_LEN];
      snprintf(line, sizeof(line), "%s", home->lines[nation]);
      europe_trim(line);
      if (line[0] && line[0] != ';') {
        str_copy_trunc(eu->port_city, sizeof(eu->port_city), line);
      } else {
        str_copy_trunc(eu->port_city, sizeof(eu->port_city), k_ports[nation]);
      }
    } else {
      str_copy_trunc(eu->port_city, sizeof(eu->port_city), k_ports[nation]);
    }
    if (reg && nation >= 0 && nation < reg->line_count) {
      char line[COLONIZE_MSG_LINE_LEN];
      snprintf(line, sizeof(line), "%s", reg->lines[nation]);
      europe_trim(line);
      if (line[0] && line[0] != ';') {
        str_copy_trunc(eu->colony_region, sizeof(eu->colony_region), line);
      } else {
        str_copy_trunc(eu->colony_region, sizeof(eu->colony_region), k_regions[nation]);
      }
    } else {
      str_copy_trunc(eu->colony_region, sizeof(eu->colony_region), k_regions[nation]);
    }
  } else {
    str_copy_trunc(eu->port_city, sizeof(eu->port_city), k_ports[nation]);
    str_copy_trunc(eu->colony_region, sizeof(eu->colony_region), k_regions[nation]);
  }
  str_copy_trunc(eu->nation_name, sizeof(eu->nation_name), k_nations[nation]);
}

int europe_voyage_turns_roll(ColonizeDosRng* rng, bool magellan, int ship_count) {
  if (!rng) {
    return 1;
  }
  /* 48d3:0042 RNG(1,100) always rolled; >0x59 && ship_counts>2 && !FF5 → 2. */
  const int roll = dos_rng_range(rng, 1, 100);
  if (roll > 89 && ship_count > 2 && !magellan) {
    return 2;
  }
  return 1;
}

static int europe_clamp_voyage_turns(int t) {
  if (t < 1) {
    return 1;
  }
  if (t > EUROPE_VOYAGE_TURNS_MAX) {
    return EUROPE_VOYAGE_TURNS_MAX;
  }
  return t;
}

void europe_reset_campaign(EuropeScreen* eu) {
  europe_reset_campaign_nation(eu, 0);
}

void europe_reset_campaign_nation(EuropeScreen* eu, int nation) {
  if (!eu) {
    return;
  }
  if (nation < 0 || nation > 3) {
    nation = 0;
  }
  europe_set_nation(eu, nation, NULL);
  eu->gold = 1000;
  eu->tax_percent = 0;
  eu->current_crosses = 0;
  /* Match new-game Col1 human needed seed (COLONY00); first EOT overwrites via 584a. */
  eu->needed_crosses = 9;
  eu->crosses_immigrant_seen = false;
  eu->liberty_bells_total = 0;
  eu->liberty_bells_last_turn = 0;
  eu->harbor_ships = 0;
  eu->expected_ships = 0;
  eu->bound_ships = 0;
  memset(eu->harbor, 0, sizeof(eu->harbor));
  memset(eu->expected, 0, sizeof(eu->expected));
  memset(eu->bound, 0, sizeof(eu->bound));
  eu->selected_harbor = -1;
  eu->selected_market = 0;
  eu->dock_count = 0;
  memset(eu->dock, 0, sizeof(eu->dock));
  eu->recruit_count = 0;
  eu->difficulty = 0; /* first EOT tick caches the real col1 difficulty */
  europe_refresh_recruit_passage(eu);
  europe_init_pool(eu);
  europe_init_purchase_table(eu);
  eu->menu = EUROPE_MENU_NONE;
  eu->menu_selection = 0;
  eu->menu_dock_index = -1;
  eu->last_exit_valid = false;
  eu->open_on_dock = false;
  eu->price_event_count = 0;
  eu->immigration_score = 0;
  eu->immigration_pressure = 0;
  eu->boycott_bitmap = 0;
  /* DOS FUN_38fd_6024: recruit pool (+2..+4) filled; docks empty; pressure 0. */
  europe_set_status(eu, "Home port ready. Recruit / Purchase / Train / S Sail.");
}

bool europe_load(EuropeScreen* eu, const char* data_dir, char* err, size_t err_size) {
  if (!eu || !data_dir) {
    snprintf(err, err_size, "europe_load bad args");
    return false;
  }
  memset(eu, 0, sizeof(*eu));

  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  char names_path[512];
  if (!dos_compat_normalize_asset_path(data_dir, "NAMES.TXT", names_path, sizeof(names_path)) ||
      !assets_msg_load_file(&names, names_path)) {
    snprintf(err, err_size, "failed to load NAMES.TXT for Europe market");
    assets_msg_free(&names);
    return false;
  }
  if (!europe_load_tables(eu, &names)) {
    snprintf(err, err_size, "NAMES.TXT missing usable @CARGO table");
    assets_msg_free(&names);
    return false;
  }

  char pik_path[512];
  char pik_err[256];
  if (!dos_compat_normalize_asset_path(data_dir, "EUROPE.PIK", pik_path, sizeof(pik_path))) {
    snprintf(err, err_size, "EUROPE.PIK path resolve failed");
    assets_msg_free(&names);
    return false;
  }
  if (!pik_load(pik_path, &eu->background, pik_err, sizeof(pik_err))) {
    snprintf(err, err_size, "EUROPE.PIK: %s", pik_err);
    assets_msg_free(&names);
    return false;
  }
  eu->background_ok = true;

  char ss_path[512];
  char ss_err[256];
  if (dos_compat_normalize_asset_path(data_dir, "WOODTILE.SS", ss_path, sizeof(ss_path)) &&
      ss_load(ss_path, &eu->wood_tile, ss_err, sizeof(ss_err))) {
    if (eu->background.has_palette) {
      europe_remap_sheet_to_palette(&eu->wood_tile, &eu->background.palette);
    }
    eu->wood_tile_ok = true;
  } else {
    eu->wood_tile_ok = false;
    diag_warn("Europe WOODTILE.SS unavailable");
  }

  europe_reset_campaign(eu);
  europe_set_nation(eu, 0, &names);
  /* Re-init pool/purchase after reset; train table already from load_tables. */
  {
    int train_count = eu->train_count;
    EuropeTrainOption train_copy[EUROPE_TRAIN_MAX];
    memcpy(train_copy, eu->train, sizeof(train_copy));
    europe_reset_campaign(eu);
    eu->train_count = train_count;
    memcpy(eu->train, train_copy, sizeof(train_copy));
    europe_set_nation(eu, 0, &names);
    europe_init_purchase_table(eu);
  }
  assets_msg_free(&names);

  diag_info(
    "Europe screen loaded (%dx%d, %d cargo, %d train, %d purchase)",
    eu->background.width,
    eu->background.height,
    eu->cargo_count,
    eu->train_count,
    eu->purchase_count
  );
  return true;
}

void europe_free(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  pik_free(&eu->background);
  ss_free(&eu->wood_tile);
  memset(eu, 0, sizeof(*eu));
}

int europe_compute_recruit_passage(
  int recruit_count, int difficulty, int current_crosses, int needed_crosses
) {
  if (difficulty < 0) {
    difficulty = 0;
  }
  if (difficulty > 8) {
    difficulty = 8;
  }
  const long base = (long)(recruit_count + difficulty + 7) * 20;
  long floor_val = base / 5;
  if (floor_val < 100) {
    floor_val = 100;
  }
  /* -(needed_crosses+1): DOS's own divide-by-zero guard (~X == -X-1). */
  const long denom = -((long)needed_crosses + 1);
  const long discount = ((base - floor_val) * (long)current_crosses) / denom;
  long passage = base + discount;
  if (passage < 10) {
    passage = 10;
  }
  return (int)passage;
}

static void europe_refresh_recruit_passage(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  eu->recruit_passage = europe_compute_recruit_passage(
    eu->recruit_count, eu->difficulty, eu->current_crosses, eu->needed_crosses
  );
}

static void europe_bump_recruit_count(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  if (eu->recruit_count < 180) {
    eu->recruit_count++;
  }
  europe_refresh_recruit_passage(eu);
}

bool europe_recruit_from_pool(EuropeScreen* eu, int pool_index) {
  if (!eu || pool_index < 0 || pool_index >= EUROPE_POOL_SIZE) {
    return false;
  }
  if (!eu->pool[pool_index].filled) {
    europe_set_status(eu, "That recruit is unavailable.");
    return false;
  }
  if (eu->dock_count >= EUROPE_DOCK_MAX) {
    europe_set_status(eu, "Docks are full.");
    return false;
  }
  if (eu->gold < eu->recruit_passage) {
    snprintf(
      eu->status,
      sizeof(eu->status),
      "Need %d$ passage for %s.",
      eu->recruit_passage,
      eu->pool[pool_index].name
    );
    return false;
  }
  eu->gold -= eu->recruit_passage;
  EuropeDockImmigrant* slot = &eu->dock[eu->dock_count++];
  memset(slot, 0, sizeof(*slot));
  snprintf(slot->name, sizeof(slot->name), "%s", eu->pool[pool_index].name);
  slot->profession = eu->pool[pool_index].profession;
  slot->present = true;
  slot->sentry = true;
  slot->dos_type = europe_dock_type_for(slot->name, slot->profession);
  snprintf(
    eu->status,
    sizeof(eu->status),
    "Recruited %s (-%d$).",
    slot->name,
    eu->recruit_passage
  );
  diag_info(
    "EUROPE recruited %s (job %d) for %d$ passage (gold=%d)",
    slot->name, slot->profession, eu->recruit_passage, eu->gold
  );
  /*
   * FUN_38fd_4884 tail, param_1==0 (viceroy_unpacked.c:64765): a *paid*
   * passage clears the crosses meter (nation+0x2e = 0) before the +6
   * counter bumps. Without it the discount term kept the next price pinned
   * near the 100 floor, so buying colonists never walked the price ladder
   * up (bugs.md: "recruit price isn't increased by recruiting with gold").
   */
  eu->current_crosses = 0;
  eu->immigration_pressure = 0;
  europe_bump_recruit_count(eu);
  europe_refill_pool_slot(eu, pool_index, NULL);
  return true;
}

bool europe_recruit_free_from_pool(EuropeScreen* eu, int pool_index) {
  if (!eu || pool_index < 0 || pool_index >= EUROPE_POOL_SIZE) {
    return false;
  }
  if (!eu->pool[pool_index].filled) {
    europe_refill_pool_slot(eu, pool_index, NULL);
  }
  if (eu->dock_count >= EUROPE_DOCK_MAX) {
    europe_set_status(eu, "Docks are full.");
    return false;
  }
  /* FUN_38fd_4884 with param_1 != 0: passage forced to 0, the +6 recruit
   * counter and the +0x2e crosses word are left alone (64695-64697, 64778). */
  EuropeDockImmigrant* slot = &eu->dock[eu->dock_count++];
  memset(slot, 0, sizeof(*slot));
  snprintf(slot->name, sizeof(slot->name), "%s", eu->pool[pool_index].name);
  slot->profession = eu->pool[pool_index].profession;
  slot->present = true;
  slot->sentry = true;
  slot->dos_type = europe_dock_type_for(slot->name, slot->profession);
  snprintf(eu->status, sizeof(eu->status), "%s joins the docks.", slot->name);
  europe_refill_pool_slot(eu, pool_index, NULL);
  return true;
}

bool europe_brewster_pick_from_pool(EuropeScreen* eu, int pool_index) {
  if (!eu || !europe_recruit_free_from_pool(eu, pool_index)) {
    return false;
  }
  /* 4884 tail with param_1==0: +0x2e crosses zeroed after the pick; no +6
   * recruit-count bump (param_2!=0 skips it). */
  eu->current_crosses = 0;
  eu->immigration_pressure = 0;
  eu->crosses_immigrant_seen = true;
  /* bugs.md 229: do NOT clear open_on_dock here — that flag belongs to a
   * ship ARRIVAL (DS:0x14c). A Brewster pick answered after the end of turn
   * was wiping the pending auto-open of the ship that had just docked. */
  europe_refresh_recruit_passage(eu);
  snprintf(eu->status, sizeof(eu->status), "Immigrant arrives in Europe.");
  return true;
}

bool europe_immigrant_from_pool(EuropeScreen* eu, ColonizeDosRng* rng) {
  if (!eu || eu->dock_count >= EUROPE_DOCK_MAX) {
    return false;
  }
  int slot = -1;
  if (rng) {
    /* DOS `5e52` phase 5: 04d4(0,2) rolls the slot before rerolling it. */
    const int roll = dos_rng_range(rng, 0, EUROPE_POOL_SIZE - 1);
    if (eu->pool[roll].filled) {
      slot = roll;
    }
  }
  if (slot < 0) {
    for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
      if (eu->pool[i].filled) {
        slot = i;
        break;
      }
    }
  }
  if (slot < 0) {
    europe_refill_pool_slot(eu, 0, NULL);
    slot = 0;
  }
  EuropeDockImmigrant* d = &eu->dock[eu->dock_count++];
  memset(d, 0, sizeof(*d));
  snprintf(d->name, sizeof(d->name), "%s", eu->pool[slot].name);
  d->profession = eu->pool[slot].profession;
  d->present = true;
  d->sentry = true;
  d->dos_type = europe_dock_type_for(d->name, d->profession);
  /* DOS 0718 harbor-spawn does NOT bump Europe+6 — only 4884's own real
   * Recruit-click tail does (see europe_compute_recruit_passage). */
  europe_refill_pool_slot(eu, slot, NULL);
  return true;
}

bool europe_train(EuropeScreen* eu, int train_index) {
  if (!eu || train_index < 0 || train_index >= eu->train_count) {
    return false;
  }
  if (eu->dock_count >= EUROPE_DOCK_MAX) {
    europe_set_status(eu, "Docks are full.");
    return false;
  }
  const EuropeTrainOption* t = &eu->train[train_index];
  if (eu->gold < t->cost) {
    snprintf(eu->status, sizeof(eu->status), "Need %d$ for %s.", t->cost, t->expert_name);
    return false;
  }
  eu->gold -= t->cost;
  EuropeDockImmigrant* slot = &eu->dock[eu->dock_count++];
  memset(slot, 0, sizeof(*slot));
  snprintf(slot->name, sizeof(slot->name), "%s", t->expert_name);
  slot->profession = t->job_index;
  slot->present = true;
  slot->sentry = true;
  slot->dos_type = europe_dock_type_for(slot->name, slot->profession);
  snprintf(eu->status, sizeof(eu->status), "Trained %s (-%d$).", t->expert_name, t->cost);
  diag_info("EUROPE trained %s for %d$ (gold=%d)", t->expert_name, t->cost, eu->gold);
  return true;
}

int europe_purchase_cost(const EuropeScreen* eu, int purchase_index) {
  if (!eu || purchase_index < 0 || purchase_index >= eu->purchase_count) {
    return -1;
  }
  const EuropePurchaseOption* p = &eu->purchase[purchase_index];
  int cost = p->gold;
  /* FUN_38fd_4b50: type 0xb (Artillery) costs base + nation+0x1e * 100. */
  if (strcmp(p->name, "Artillery") == 0 && eu->artillery_bought > 0) {
    cost += eu->artillery_bought * 100;
  }
  return cost;
}

bool europe_purchase(EuropeScreen* eu, int purchase_index) {
  if (!eu || purchase_index < 0 || purchase_index >= eu->purchase_count) {
    return false;
  }
  const EuropePurchaseOption* p = &eu->purchase[purchase_index];
  const int cost = europe_purchase_cost(eu, purchase_index);
  const bool is_artillery = strcmp(p->name, "Artillery") == 0;
  if (eu->gold < cost) {
    snprintf(eu->status, sizeof(eu->status), "Need %d$ for %s.", cost, p->name);
    return false;
  }
  if (p->is_ship) {
    if (eu->harbor_ships >= EUROPE_HARBOR_MAX) {
      europe_set_status(eu, "Harbor is full.");
      return false;
    }
    eu->gold -= cost;
    EuropeHarborShip* slot = &eu->harbor[eu->harbor_ships++];
    europe_clear_ship(slot);
    slot->type_index = -1; /* resolved by name in game_loop / caller */
    snprintf(slot->name, sizeof(slot->name), "%s", p->name);
    europe_refresh_harbor_selection(eu);
    snprintf(eu->status, sizeof(eu->status), "Purchased %s (-%d$).", p->name, cost);
    diag_info("EUROPE purchased ship %s for %d$ (gold=%d)", p->name, cost, eu->gold);
    return true;
  }
  if (eu->dock_count >= EUROPE_DOCK_MAX) {
    europe_set_status(eu, "Docks are full.");
    return false;
  }
  eu->gold -= cost;
  /* FUN_38fd_4b50 purchase arm: nation+0x1e += 1 after charging. */
  if (is_artillery) {
    eu->artillery_bought += 1;
  }
  EuropeDockImmigrant* slot = &eu->dock[eu->dock_count++];
  memset(slot, 0, sizeof(*slot));
  snprintf(slot->name, sizeof(slot->name), "%s", p->name);
  slot->profession = -1;
  slot->present = true;
  slot->sentry = true;
  slot->dos_type = europe_dock_type_for(slot->name, slot->profession);
  snprintf(eu->status, sizeof(eu->status), "Purchased %s (-%d$).", p->name, cost);
  diag_info("EUROPE purchased %s for %d$ (gold=%d)", p->name, cost, eu->gold);
  return true;
}

bool europe_recruit(EuropeScreen* eu) {
  if (!eu) {
    return false;
  }
  europe_menu_open(eu, EUROPE_MENU_RECRUIT);
  return true;
}

int europe_dock_unit_dos_type(int profession, int difficulty, bool human, ColonizeDosRng* rng) {
  int type = 0; /* Colonists */
  if (profession == 0x14) {
    type = 2; /* Pioneers */
  } else if (profession == 0x18) {
    type = 3; /* Missionaries */
  } else if (profession == 0x16) {
    type = 5; /* Scouts */
  } else if (profession == 0x15) {
    type = 1; /* Soldiers */
    if (rng) {
      const int bound = human ? difficulty : 1;
      if (dos_rng_range(rng, 0, bound + 4) == 0) {
        type = 4; /* Dragoons */
      }
    }
  }
  return type;
}

int europe_dock_type_for(const char* name, int profession) {
  static const char* const k_names[6] = {"Colonists", "Soldiers",  "Pioneers",
                                         "Missionaries", "Dragoons", "Scouts"};
  if (name && name[0]) {
    for (int i = 0; i < 6; ++i) {
      if (strcmp(name, k_names[i]) == 0) {
        return i;
      }
    }
  }
  return europe_dock_unit_dos_type(profession, 0, true, NULL);
}

int europe_dock_type_tools(int dos_type) {
  return dos_type == EUROPE_DOCK_TYPE_PIONEERS ? 100 : 0;
}

int europe_dock_type_muskets(int dos_type) {
  return (dos_type == EUROPE_DOCK_TYPE_SOLDIERS || dos_type == EUROPE_DOCK_TYPE_DRAGOONS) ? 50 : 0;
}

int europe_dock_type_horses(int dos_type) {
  return (dos_type == EUROPE_DOCK_TYPE_DRAGOONS || dos_type == EUROPE_DOCK_TYPE_SCOUTS) ? 50 : 0;
}

int europe_dock_unit_type_index(const ColonizeUnitPool* units, int dos_type) {
  static const char* const k_names[6] = {"Colonists", "Soldiers",  "Pioneers",
                                         "Missionaries", "Dragoons", "Scouts"};
  if (!units || dos_type < 0 || dos_type > 5) {
    return -1;
  }
  return units_find_type((ColonizeUnitPool*)units, k_names[dos_type]);
}

/*
 * @UNIT display type for a dock entry — what unit_chrome uses to place the
 * orders/allegiance box (Dragoons/Scouts top-left, Artillery top-center,
 * everyone else bottom-right), matching what units_display_type_index
 * derives from a landed unit's kit. A dock entry's name is a profession
 * ("Veteran Soldiers"), never an @UNIT type, so looking the name up first
 * missed and fell back to Colonists: every armed or mounted immigrant wore
 * the plain bottom-right box, a Veteran Dragoon most visibly (bugs.md).
 * dos_type is the field the @ARMOPTIONS rows move around and is what
 * europe_dock_sprite already picks the sprite from; Artillery carries no
 * dos_type of its own and is name-flagged there, so it is here too.
 */
int europe_dock_display_type_index(
  const ColonizeUnitPool* units, const EuropeDockImmigrant* d
) {
  if (!units || !d) {
    return -1;
  }
  int ti = -1;
  if (strcmp(d->name, "Artillery") == 0) {
    ti = units_find_type(units, "Artillery");
  }
  if (ti < 0) {
    ti = europe_dock_unit_type_index(units, d->dos_type);
  }
  if (ti < 0) {
    ti = units_find_type(units, d->name);
  }
  if (ti < 0) {
    ti = units_find_type(units, "Colonists");
  }
  return ti;
}

void europe_apply_dock_unit_kit(ColonizeUnit* u, int dos_type) {
  if (!u) {
    return;
  }
  /*
   * DOS's harbor spawn writes only the Tools byte (+0x3159) because
   * muskets and horses ride on the @UNIT type itself. This port keeps them
   * as explicit unit fields, so set all three from the type — which is also
   * what the @ARMOPTIONS rows need when they move an immigrant between
   * Colonists / Soldiers / Pioneers / Dragoons / Scouts.
   */
  u->tools = europe_dock_type_tools(dos_type);
  u->muskets = europe_dock_type_muskets(dos_type);
  u->horses = europe_dock_type_horses(dos_type);
}

/*
 * Re-type and re-kit the (236,236) mirror unit behind a dock entry after an
 * @ARMOPTIONS row changed it. Matched on profession the same way
 * europe_remove_dock_mirror_unit matches, with the pre-change type as the
 * tie-break so two immigrants of the same profession do not swap.
 */
void europe_retype_dock_mirror_unit(
  ColonizeUnitPool* units,
  int nation_id,
  int profession,
  int from_dos_type,
  int to_dos_type
) {
  if (!units || nation_id < 0 || nation_id > 3) {
    return;
  }
  const int from_ti = europe_dock_unit_type_index(units, from_dos_type);
  const int to_ti = europe_dock_unit_type_index(units, to_dos_type);
  if (to_ti < 0) {
    return;
  }
  int fallback = -1;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id || u->x != 236 || u->y != 236 ||
        u->aboard_ship_id >= 0 || units_is_sea(units, u->id)) {
      continue;
    }
    if (u->profession != profession) {
      continue;
    }
    if (from_ti >= 0 && u->type_index == from_ti) {
      u->type_index = to_ti;
      europe_apply_dock_unit_kit(u, to_dos_type);
      return;
    }
    if (fallback < 0) {
      fallback = i;
    }
  }
  if (fallback >= 0) {
    units->units[fallback].type_index = to_ti;
    europe_apply_dock_unit_kit(&units->units[fallback], to_dos_type);
  }
}

int europe_spawn_dock_mirror_unit(
  ColonizeUnitPool* units,
  int nation_id,
  int profession,
  int difficulty,
  bool human,
  ColonizeDosRng* rng
) {
  if (!units || nation_id < 0 || nation_id > 3) {
    return -1;
  }
  const int dos_type = europe_dock_unit_dos_type(profession, difficulty, human, rng);
  int ti = europe_dock_unit_type_index(units, dos_type);
  if (ti < 0) {
    ti = units_find_type(units, "Colonists");
  }
  const int id = units_spawn_allow_stack(units, ti >= 0 ? ti : 0, 236, 236);
  ColonizeUnit* u = units_get(units, id);
  if (!u) {
    return -1;
  }
  units_set_nation(u, nation_id);
  u->orders = UNITS_ORDER_SENTRY; /* DOS +0x314c = 1 */
  u->profession = profession;
  u->goto_x = 0;
  u->goto_y = 0;
  u->moves_left = 0;
  europe_apply_dock_unit_kit(u, dos_type);
  return id;
}

void europe_remove_dock_mirror_unit(ColonizeUnitPool* units, int nation_id, int profession) {
  if (!units || nation_id < 0 || nation_id > 3) {
    return;
  }
  int fallback = -1;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id || u->x != 236 || u->y != 236 ||
        u->aboard_ship_id >= 0 || units_is_sea(units, u->id)) {
      continue;
    }
    if (u->profession == profession) {
      (void)units_disband(units, u->id);
      return;
    }
    if (fallback < 0) {
      fallback = u->id;
    }
  }
  if (fallback >= 0) {
    (void)units_disband(units, fallback);
  }
}

/*
 * GAME.TXT @ARMOPTIONS — the menu behind a click on a dock immigrant.
 *
 * Prices, from DOS 38fd:3745..3830: Muskets are @CARGO 0x0f at 50 a set,
 * Tools 0x0e at 100, Horses 0x08 at 50, each priced with the buy accessor
 * (FUN_281f_0c3e) or the sell one (FUN_281f_09ea) times that quantity.
 * Which of the pair a row shows is the unit's @UNIT type: Muskets sell for
 * Soldiers or Dragoons and buy otherwise, Tools sell for Pioneers, Horses
 * sell for Dragoons or Scouts.
 */
static int europe_arm_buy_cost(const EuropeScreen* eu, int cargo, int qty) {
  return europe_buy_price(eu, cargo) * qty;
}

static int europe_arm_sell_gain(const EuropeScreen* eu, int cargo, int qty) {
  return europe_sell_price(eu, cargo) * qty;
}

/*
 * Per-row enable, transcribed from the switch at 38fd:388e..3a04. `price` is
 * DOS's [bp-0x6a]: non-zero only on the three buy rows, and the tail greys
 * the row when the treasury cannot cover it. A boycotted cargo disables its
 * rows outright (the FUN_38fd_68c7 test each arm row runs), and an Indian
 * Convert (@JOB 0x1b) can be neither armed, equipped nor blessed.
 */
static bool europe_arm_row_enabled(
  const EuropeScreen* eu,
  const EuropeDockImmigrant* d,
  int row,
  int* out_price
) {
  const int t = d->dos_type;
  const bool convert = (d->profession == 0x1b);
  *out_price = 0;
  switch (row) {
    case EUROPE_ARM_ROW_NO_BOARD:
      return d->sentry;
    case EUROPE_ARM_ROW_BOARD:
      return !d->sentry;
    case EUROPE_ARM_ROW_TO_FRONT:
      /* DS:0x9e2c is this immigrant's place in the dock queue. */
      return eu->menu_dock_index > 0;
    case EUROPE_ARM_ROW_BUY_MUSKETS:
      *out_price = europe_arm_buy_cost(eu, COLONIZE_CARGO_MUSKETS, EUROPE_ARM_MUSKETS);
      if (europe_cargo_boycotted(eu, COLONIZE_CARGO_MUSKETS)) {
        return false;
      }
      return !convert &&
             (t == EUROPE_DOCK_TYPE_COLONISTS || t == EUROPE_DOCK_TYPE_SCOUTS);
    case EUROPE_ARM_ROW_SELL_MUSKETS:
      if (europe_cargo_boycotted(eu, COLONIZE_CARGO_MUSKETS)) {
        return false;
      }
      return t == EUROPE_DOCK_TYPE_SOLDIERS || t == EUROPE_DOCK_TYPE_DRAGOONS;
    case EUROPE_ARM_ROW_BUY_TOOLS:
      *out_price = europe_arm_buy_cost(eu, COLONIZE_CARGO_TOOLS, EUROPE_ARM_TOOLS);
      if (europe_cargo_boycotted(eu, COLONIZE_CARGO_TOOLS)) {
        return false;
      }
      return !convert && t == EUROPE_DOCK_TYPE_COLONISTS;
    case EUROPE_ARM_ROW_SELL_TOOLS:
      if (europe_cargo_boycotted(eu, COLONIZE_CARGO_TOOLS)) {
        return false;
      }
      return t == EUROPE_DOCK_TYPE_PIONEERS;
    case EUROPE_ARM_ROW_BUY_HORSES:
      *out_price = europe_arm_buy_cost(eu, COLONIZE_CARGO_HORSES, EUROPE_ARM_HORSES);
      if (europe_cargo_boycotted(eu, COLONIZE_CARGO_HORSES)) {
        return false;
      }
      return !convert &&
             (t == EUROPE_DOCK_TYPE_COLONISTS || t == EUROPE_DOCK_TYPE_SOLDIERS);
    case EUROPE_ARM_ROW_SELL_HORSES:
      if (europe_cargo_boycotted(eu, COLONIZE_CARGO_HORSES)) {
        return false;
      }
      return t == EUROPE_DOCK_TYPE_SCOUTS || t == EUROPE_DOCK_TYPE_DRAGOONS;
    case EUROPE_ARM_ROW_BLESS:
      return !convert && t == EUROPE_DOCK_TYPE_COLONISTS;
    case EUROPE_ARM_ROW_UNBLESS:
      /*
       * DOS 38fd:39ec..3a09: enabled when the @UNIT type is Missionaries
       * (0x3146 == 3) AND the profession is NOT @JOB 0x18 (38fd:39fa
       * `cmp byte [bx+0x315b],0x18; jnz enable`) — i.e. only a *blessed*
       * ordinary colonist can cancel Missionary status; a born Jesuit
       * Missionary specialist cannot. Was inverted.
       */
      return t == EUROPE_DOCK_TYPE_MISSIONARIES && d->profession != 0x18;
    case EUROPE_ARM_ROW_NO_CHANGES:
    default:
      return true;
  }
}

void europe_build_dock_menu(
  EuropeScreen* eu,
  const struct ColonizeMsgCatalog* messages,
  int dock_index
) {
  if (!eu) {
    return;
  }
  eu->dock_menu_count = 0;
  if (dock_index < 0 || dock_index >= eu->dock_count) {
    return;
  }
  const EuropeDockImmigrant* d = &eu->dock[dock_index];
  const int t = d->dos_type;
  const bool sell_muskets =
    (t == EUROPE_DOCK_TYPE_SOLDIERS || t == EUROPE_DOCK_TYPE_DRAGOONS);
  const bool sell_tools = (t == EUROPE_DOCK_TYPE_PIONEERS);
  const bool sell_horses =
    (t == EUROPE_DOCK_TYPE_DRAGOONS || t == EUROPE_DOCK_TYPE_SCOUTS);

  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.has_number0 = true;
  tok.number0 = sell_muskets
                  ? europe_arm_sell_gain(eu, COLONIZE_CARGO_MUSKETS, EUROPE_ARM_MUSKETS)
                  : europe_arm_buy_cost(eu, COLONIZE_CARGO_MUSKETS, EUROPE_ARM_MUSKETS);
  tok.has_number1 = true;
  tok.number1 = sell_tools
                  ? europe_arm_sell_gain(eu, COLONIZE_CARGO_TOOLS, EUROPE_ARM_TOOLS)
                  : europe_arm_buy_cost(eu, COLONIZE_CARGO_TOOLS, EUROPE_ARM_TOOLS);
  tok.has_number2 = true;
  tok.number2 = sell_horses
                  ? europe_arm_sell_gain(eu, COLONIZE_CARGO_HORSES, EUROPE_ARM_HORSES)
                  : europe_arm_buy_cost(eu, COLONIZE_CARGO_HORSES, EUROPE_ARM_HORSES);

  /* GAME.TXT @ARMOPTIONS verbatim, for a build with no catalog loaded. */
  static const char* const k_fallback[EUROPE_DOCK_MENU_MAX] = {
    "Don't get on next ship.",
    "Board next ship.",
    "Move to front of dock.",
    "Arm with {Muskets} (costs {%NUMBER0$}).",
    "Sell {Muskets} (save {%NUMBER0$}).",
    "Equip with {Tools} (costs {%NUMBER1$}).",
    "Sell {Tools} (save {%NUMBER1$}).",
    "Equip with {Horses} (costs {%NUMBER2$}).",
    "Sell {Horses} (save {%NUMBER2$}).",
    "Bless as {Missionaries}.",
    "Cancel {Missionary} Status.",
    "No changes."
  };
  const ColonizeMsgSection* sec =
    messages ? assets_msg_find((const ColonizeMsgCatalog*)messages, "ARMOPTIONS") : NULL;

  int row = 0; /* 0-based index into the section's non-directive lines */
  const int line_count = (sec && sec->line_count > 0) ? sec->line_count : EUROPE_DOCK_MENU_MAX;
  for (int i = 0; i < line_count && row < EUROPE_DOCK_MENU_MAX; ++i) {
    const char* line = (sec && i < sec->line_count) ? sec->lines[i] : k_fallback[row];
    if (sec && (popup_msg_is_directive(line) || line[0] == '\0')) {
      continue;
    }
    const int row_id = row + 1;
    ++row;
    int price = 0;
    if (!europe_arm_row_enabled(eu, d, row_id, &price)) {
      continue; /* DOS omits a disabled row rather than greying it. */
    }
    const int slot = eu->dock_menu_count;
    popup_msg_apply_tokens(
      eu->dock_menu_label[slot], sizeof(eu->dock_menu_label[slot]), line, &tok
    );
    eu->dock_menu_row[slot] = (uint8_t)row_id;
    eu->dock_menu_greyed[slot] = (price > 0 && eu->gold < price);
    eu->dock_menu_count++;
  }
}

bool europe_apply_dock_menu_row(
  EuropeScreen* eu,
  ColonizeUnitPool* units,
  int nation_id,
  int dock_index,
  int row
) {
  if (!eu || dock_index < 0 || dock_index >= eu->dock_count) {
    return false;
  }
  EuropeDockImmigrant* d = &eu->dock[dock_index];
  const int from = d->dos_type;
  int to = from;
  int gold_delta = 0;
  int ledger_cargo = -1;
  int ledger_qty = 0;
  int ledger_is_buy = 0;
  int sound = -1;

  switch (row) {
    case EUROPE_ARM_ROW_NO_BOARD:
      d->sentry = false;
      europe_set_status(eu, "Will not board next ship.");
      return true;
    case EUROPE_ARM_ROW_BOARD:
      d->sentry = true;
      europe_set_status(eu, "Will board next ship.");
      return true;
    case EUROPE_ARM_ROW_TO_FRONT: {
      if (dock_index <= 0) {
        return false;
      }
      const EuropeDockImmigrant moved = *d;
      for (int i = dock_index; i > 0; --i) {
        eu->dock[i] = eu->dock[i - 1];
      }
      eu->dock[0] = moved;
      eu->menu_dock_index = 0;
      europe_set_status(eu, "Moved to front of dock.");
      return true;
    }
    /* 38fd:3ade — Scouts become Dragoons, anyone else Soldiers. */
    case EUROPE_ARM_ROW_BUY_MUSKETS:
      to = (from == EUROPE_DOCK_TYPE_SCOUTS) ? EUROPE_DOCK_TYPE_DRAGOONS
                                             : EUROPE_DOCK_TYPE_SOLDIERS;
      gold_delta = -europe_arm_buy_cost(eu, COLONIZE_CARGO_MUSKETS, EUROPE_ARM_MUSKETS);
      ledger_cargo = COLONIZE_CARGO_MUSKETS;
      ledger_qty = EUROPE_ARM_MUSKETS;
      ledger_is_buy = 1;
      sound = 0x58;
      break;
    case EUROPE_ARM_ROW_SELL_MUSKETS:
      to = (from == EUROPE_DOCK_TYPE_DRAGOONS) ? EUROPE_DOCK_TYPE_SCOUTS
                                               : EUROPE_DOCK_TYPE_COLONISTS;
      gold_delta = europe_arm_sell_gain(eu, COLONIZE_CARGO_MUSKETS, EUROPE_ARM_MUSKETS);
      ledger_cargo = COLONIZE_CARGO_MUSKETS;
      ledger_qty = EUROPE_ARM_MUSKETS;
      break;
    case EUROPE_ARM_ROW_BUY_TOOLS:
      to = EUROPE_DOCK_TYPE_PIONEERS;
      gold_delta = -europe_arm_buy_cost(eu, COLONIZE_CARGO_TOOLS, EUROPE_ARM_TOOLS);
      ledger_cargo = COLONIZE_CARGO_TOOLS;
      ledger_qty = EUROPE_ARM_TOOLS;
      ledger_is_buy = 1;
      break;
    case EUROPE_ARM_ROW_SELL_TOOLS:
      to = EUROPE_DOCK_TYPE_COLONISTS;
      gold_delta = europe_arm_sell_gain(eu, COLONIZE_CARGO_TOOLS, EUROPE_ARM_TOOLS);
      ledger_cargo = COLONIZE_CARGO_TOOLS;
      ledger_qty = EUROPE_ARM_TOOLS;
      break;
    /* 38fd:3bbe — Soldiers become Dragoons, anyone else Scouts. */
    case EUROPE_ARM_ROW_BUY_HORSES:
      to = (from == EUROPE_DOCK_TYPE_SOLDIERS) ? EUROPE_DOCK_TYPE_DRAGOONS
                                               : EUROPE_DOCK_TYPE_SCOUTS;
      gold_delta = -europe_arm_buy_cost(eu, COLONIZE_CARGO_HORSES, EUROPE_ARM_HORSES);
      ledger_cargo = COLONIZE_CARGO_HORSES;
      ledger_qty = EUROPE_ARM_HORSES;
      ledger_is_buy = 1;
      sound = 0x5c;
      break;
    case EUROPE_ARM_ROW_SELL_HORSES:
      to = (from == EUROPE_DOCK_TYPE_DRAGOONS) ? EUROPE_DOCK_TYPE_SOLDIERS
                                               : EUROPE_DOCK_TYPE_COLONISTS;
      gold_delta = europe_arm_sell_gain(eu, COLONIZE_CARGO_HORSES, EUROPE_ARM_HORSES);
      ledger_cargo = COLONIZE_CARGO_HORSES;
      ledger_qty = EUROPE_ARM_HORSES;
      break;
    case EUROPE_ARM_ROW_BLESS:
      to = EUROPE_DOCK_TYPE_MISSIONARIES;
      sound = 0x8024; /* the same church chord the colony assign plays */
      break;
    case EUROPE_ARM_ROW_UNBLESS:
      to = EUROPE_DOCK_TYPE_COLONISTS;
      break;
    case EUROPE_ARM_ROW_NO_CHANGES:
    default:
      return true;
  }

  if (gold_delta < 0 && eu->gold < -gold_delta) {
    europe_set_status(eu, "The treasury cannot afford that.");
    return false;
  }
  eu->gold += gold_delta;
  d->dos_type = to;
  if (ledger_cargo >= 0) {
    europe_apply_volume_price(eu, ledger_cargo, ledger_qty, ledger_is_buy);
  }
  if (units) {
    europe_retype_dock_mirror_unit(units, nation_id, d->profession, from, to);
  }
  if (sound >= 0 && g_europe_sound_play) {
    g_europe_sound_play(sound);
  }
  return true;
}

bool europe_pop_dock_immigrant(EuropeScreen* eu, char* out_name, size_t out_name_size) {
  return europe_pop_dock_immigrant_ex(eu, out_name, out_name_size, NULL);
}

bool europe_pop_dock_immigrant_ex(
  EuropeScreen* eu,
  char* out_name,
  size_t out_name_size,
  int* out_profession
) {
  if (!eu || eu->dock_count <= 0) {
    return false;
  }
  if (out_name && out_name_size > 0) {
    snprintf(out_name, out_name_size, "%s", eu->dock[0].name);
  }
  if (out_profession) {
    *out_profession = eu->dock[0].profession;
  }
  for (int i = 1; i < eu->dock_count; ++i) {
    eu->dock[i - 1] = eu->dock[i];
  }
  eu->dock_count--;
  memset(&eu->dock[eu->dock_count], 0, sizeof(eu->dock[0]));
  return true;
}

bool europe_harbor_push(
  EuropeScreen* eu,
  int type_index,
  const char* name,
  const int* cargo_types,
  int cargo_count,
  const int* hold_goods_type,
  const int* hold_goods_amount
) {
  if (!eu) {
    return false;
  }
  if (eu->harbor_ships >= EUROPE_HARBOR_MAX) {
    europe_set_status(eu, "Harbor is full.");
    return false;
  }
  EuropeHarborShip* slot = &eu->harbor[eu->harbor_ships++];
  europe_clear_ship(slot);
  slot->type_index = type_index;
  snprintf(slot->name, sizeof(slot->name), "%s", name ? name : "Ship");
  if (cargo_types && cargo_count > 0) {
    const int n = cargo_count > EUROPE_SHIP_CARGO_MAX ? EUROPE_SHIP_CARGO_MAX : cargo_count;
    for (int i = 0; i < n; ++i) {
      slot->cargo_types[i] = cargo_types[i];
      slot->cargo_professions[i] = -1;
    }
    slot->cargo_count = n;
  }
  if (hold_goods_type && hold_goods_amount) {
    for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
      slot->hold_goods_type[i] = hold_goods_type[i];
      slot->hold_goods_amount[i] = hold_goods_amount[i];
    }
  }
  slot->turns_left = 0;
  snprintf(eu->status, sizeof(eu->status), "%s in harbor.", slot->name);
  europe_refresh_harbor_selection(eu);
  return true;
}

bool europe_enqueue_expected(
  EuropeScreen* eu,
  int type_index,
  const char* name,
  const int* cargo_types,
  const int* cargo_professions,
  int cargo_count,
  const int* hold_goods_type,
  const int* hold_goods_amount,
  int exit_x,
  int exit_y,
  bool exit_east,
  int voyage_turns
) {
  if (!eu) {
    return false;
  }
  if (eu->expected_ships >= EUROPE_HARBOR_MAX) {
    europe_set_status(eu, "Expected Soon is full.");
    return false;
  }
  EuropeHarborShip* slot = &eu->expected[eu->expected_ships++];
  europe_clear_ship(slot);
  slot->type_index = type_index;
  snprintf(slot->name, sizeof(slot->name), "%s", name ? name : "Ship");
  if (cargo_types && cargo_count > 0) {
    const int n = cargo_count > EUROPE_SHIP_CARGO_MAX ? EUROPE_SHIP_CARGO_MAX : cargo_count;
    for (int i = 0; i < n; ++i) {
      slot->cargo_types[i] = cargo_types[i];
      slot->cargo_professions[i] =
        cargo_professions ? cargo_professions[i] : -1;
    }
    slot->cargo_count = n;
  }
  if (hold_goods_type && hold_goods_amount) {
    for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
      slot->hold_goods_type[i] = hold_goods_type[i];
      slot->hold_goods_amount[i] = hold_goods_amount[i];
    }
  }
  slot->exit_x = exit_x;
  slot->exit_y = exit_y;
  slot->exit_east = exit_east;
  slot->turns_left = europe_clamp_voyage_turns(voyage_turns);
  slot->departed_this_turn = true;
  eu->last_exit_x = exit_x;
  eu->last_exit_y = exit_y;
  eu->last_exit_east = exit_east;
  eu->last_exit_valid = true;
  snprintf(
    eu->status,
    sizeof(eu->status),
    "%s expected in %d turn(s).",
    slot->name,
    slot->turns_left
  );
  return true;
}

static void europe_board_sentry_dockers(
  EuropeScreen* eu,
  EuropeHarborShip* ship,
  ColonizeUnitPool* units,
  int nation_id,
  int cargo_cap
) {
  if (!eu || !ship || cargo_cap <= 0) {
    return;
  }
  const int goods = europe_goods_slots_used(ship);
  while (ship->cargo_count + goods < cargo_cap) {
    int di = -1;
    for (int i = 0; i < eu->dock_count; ++i) {
      if (eu->dock[i].present && eu->dock[i].sentry) {
        di = i;
        break; /* front of queue first */
      }
    }
    if (di < 0) {
      break;
    }
    int type_tag = 0;
    const bool is_artillery = (strcmp(eu->dock[di].name, "Artillery") == 0);
    if (is_artillery) {
      type_tag = -2;
    } else if (units) {
      /*
       * The dock entry's name is the expert plural ("Hardy Pioneers"), which
       * matches no @UNIT type, so this used to fall back to Colonists for
       * every specialist and the passenger landed as a plain colonist with no
       * kit. The entry now carries its own DOS type — set when it arrived and
       * moved by the @ARMOPTIONS rows — so a Soldier boards as Soldiers and a
       * Hardy Pioneer as Pioneers. The name lookup still wins for purchases
       * that really are typed by name (Artillery is handled above).
       */
      int ti = units_find_type(units, eu->dock[di].name);
      if (ti < 0) {
        ti = europe_dock_unit_type_index(units, eu->dock[di].dos_type);
      }
      type_tag = ti >= 0 ? ti : 0;
    }
    ship->cargo_types[ship->cargo_count] = type_tag;
    ship->cargo_professions[ship->cargo_count] = eu->dock[di].profession;
    ship->cargo_count++;
    /* The dock immigrant's (236,236) mirror unit leaves the pool with it. */
    europe_remove_dock_mirror_unit(units, nation_id, eu->dock[di].profession);
    for (int j = di + 1; j < eu->dock_count; ++j) {
      eu->dock[j - 1] = eu->dock[j];
    }
    eu->dock_count--;
    memset(&eu->dock[eu->dock_count], 0, sizeof(eu->dock[0]));
  }
}

bool europe_set_sail_from_harbor(
  EuropeScreen* eu,
  int harbor_index,
  int voyage_turns,
  ColonizeUnitPool* units,
  int nation_id
) {
  if (!eu || harbor_index < 0 || harbor_index >= eu->harbor_ships) {
    return false;
  }
  if (eu->bound_ships >= EUROPE_HARBOR_MAX) {
    europe_set_status(eu, "Outbound lane is full.");
    return false;
  }
  EuropeHarborShip ship;
  europe_copy_ship(&ship, &eu->harbor[harbor_index]);
  const int cargo_cap = europe_ship_cargo_cap(&ship, units);
  europe_board_sentry_dockers(eu, &ship, units, nation_id, cargo_cap);
  bool exit_east = eu->last_exit_valid ? eu->last_exit_east : true;
  ship.exit_east = exit_east;
  if (eu->last_exit_valid) {
    ship.exit_x = eu->last_exit_x;
    ship.exit_y = eu->last_exit_y;
  }
  ship.turns_left = europe_clamp_voyage_turns(voyage_turns);
  ship.departed_this_turn = true;
  for (int i = harbor_index + 1; i < eu->harbor_ships; ++i) {
    eu->harbor[i - 1] = eu->harbor[i];
  }
  eu->harbor_ships--;
  europe_clear_ship(&eu->harbor[eu->harbor_ships]);
  if (eu->selected_harbor == harbor_index) {
    eu->selected_harbor = -1;
  } else if (eu->selected_harbor > harbor_index) {
    eu->selected_harbor--;
  }
  europe_refresh_harbor_selection(eu);
  eu->bound[eu->bound_ships++] = ship;
  snprintf(
    eu->status,
    sizeof(eu->status),
    "%s bound for %s (%d turns).",
    ship.name,
    eu->colony_region[0] ? eu->colony_region : "New World",
    ship.turns_left
  );
  diag_info(
    "EUROPE %s sails for the New World: %d turns, exit (%d,%d) %s",
    ship.name, ship.turns_left, ship.exit_x, ship.exit_y,
    ship.exit_east ? "east" : "west"
  );
  return true;
}

bool europe_reverse_transit(EuropeScreen* eu, bool from_expected, int index) {
  if (!eu) {
    return false;
  }
  if (from_expected) {
    if (index < 0 || index >= eu->expected_ships || eu->bound_ships >= EUROPE_HARBOR_MAX) {
      return false;
    }
    EuropeHarborShip ship = eu->expected[index];
    for (int i = index + 1; i < eu->expected_ships; ++i) {
      eu->expected[i - 1] = eu->expected[i];
    }
    eu->expected_ships--;
    europe_clear_ship(&eu->expected[eu->expected_ships]);
    eu->bound[eu->bound_ships++] = ship;
    europe_set_status(eu, "Reversed — now bound for the New World.");
    return true;
  }
  if (index < 0 || index >= eu->bound_ships || eu->expected_ships >= EUROPE_HARBOR_MAX) {
    return false;
  }
  EuropeHarborShip ship = eu->bound[index];
  for (int i = index + 1; i < eu->bound_ships; ++i) {
    eu->bound[i - 1] = eu->bound[i];
  }
  eu->bound_ships--;
  europe_clear_ship(&eu->bound[eu->bound_ships]);
  eu->expected[eu->expected_ships++] = ship;
  europe_set_status(eu, "Reversed — now expected in Europe.");
  return true;
}

bool europe_harbor_pop(
  EuropeScreen* eu,
  int* out_type_index,
  char* out_name,
  size_t out_name_size,
  int* out_cargo_types,
  int* out_cargo_count,
  int cargo_max,
  int* out_hold_goods_type,
  int* out_hold_goods_amount,
  int hold_max
) {
  if (!eu || eu->harbor_ships <= 0) {
    return false;
  }
  if (out_type_index) {
    *out_type_index = eu->harbor[0].type_index;
  }
  if (out_name && out_name_size > 0) {
    snprintf(out_name, out_name_size, "%s", eu->harbor[0].name);
  }
  if (out_cargo_count) {
    *out_cargo_count = 0;
  }
  if (out_cargo_types && out_cargo_count && cargo_max > 0) {
    const int n =
      eu->harbor[0].cargo_count > cargo_max ? cargo_max : eu->harbor[0].cargo_count;
    for (int i = 0; i < n; ++i) {
      out_cargo_types[i] = eu->harbor[0].cargo_types[i];
    }
    *out_cargo_count = n;
  }
  if (out_hold_goods_type && out_hold_goods_amount && hold_max > 0) {
    const int n = hold_max > EUROPE_SHIP_CARGO_MAX ? EUROPE_SHIP_CARGO_MAX : hold_max;
    for (int i = 0; i < n; ++i) {
      out_hold_goods_type[i] = eu->harbor[0].hold_goods_type[i];
      out_hold_goods_amount[i] = eu->harbor[0].hold_goods_amount[i];
    }
    for (int i = n; i < hold_max; ++i) {
      out_hold_goods_type[i] = 0;
      out_hold_goods_amount[i] = 0;
    }
  }
  for (int i = 1; i < eu->harbor_ships; ++i) {
    eu->harbor[i - 1] = eu->harbor[i];
  }
  eu->harbor_ships--;
  europe_clear_ship(&eu->harbor[eu->harbor_ships]);
  if (eu->selected_harbor == 0) {
    eu->selected_harbor = -1;
  } else if (eu->selected_harbor > 0) {
    eu->selected_harbor--;
  }
  europe_refresh_harbor_selection(eu);
  return true;
}

bool europe_bound_pop_arrived(
  EuropeScreen* eu,
  int* out_type_index,
  char* out_name,
  size_t out_name_size,
  int* out_cargo_types,
  int* out_cargo_count,
  int cargo_max,
  int* out_hold_goods_type,
  int* out_hold_goods_amount,
  int hold_max,
  int* out_exit_x,
  int* out_exit_y,
  bool* out_exit_east
) {
  if (!eu) {
    return false;
  }
  int idx = -1;
  for (int i = 0; i < eu->bound_ships; ++i) {
    if (eu->bound[i].turns_left <= 0) {
      idx = i;
      break;
    }
  }
  if (idx < 0) {
    return false;
  }
  EuropeHarborShip* ship = &eu->bound[idx];
  if (out_type_index) {
    *out_type_index = ship->type_index;
  }
  if (out_name && out_name_size > 0) {
    snprintf(out_name, out_name_size, "%s", ship->name);
  }
  if (out_cargo_count) {
    *out_cargo_count = 0;
  }
  if (out_cargo_types && out_cargo_count && cargo_max > 0) {
    const int n = ship->cargo_count > cargo_max ? cargo_max : ship->cargo_count;
    for (int i = 0; i < n; ++i) {
      out_cargo_types[i] = ship->cargo_types[i];
    }
    *out_cargo_count = n;
  }
  if (out_hold_goods_type && out_hold_goods_amount && hold_max > 0) {
    const int n = hold_max > EUROPE_SHIP_CARGO_MAX ? EUROPE_SHIP_CARGO_MAX : hold_max;
    for (int i = 0; i < n; ++i) {
      out_hold_goods_type[i] = ship->hold_goods_type[i];
      out_hold_goods_amount[i] = ship->hold_goods_amount[i];
    }
  }
  if (out_exit_x) {
    *out_exit_x = ship->exit_x;
  }
  if (out_exit_y) {
    *out_exit_y = ship->exit_y;
  }
  if (out_exit_east) {
    *out_exit_east = ship->exit_east;
  }
  for (int i = idx + 1; i < eu->bound_ships; ++i) {
    eu->bound[i - 1] = eu->bound[i];
  }
  eu->bound_ships--;
  europe_clear_ship(&eu->bound[eu->bound_ships]);
  return true;
}

void europe_refresh_harbor_selection(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  if (eu->harbor_ships <= 0) {
    eu->selected_harbor = -1;
    return;
  }
  if (eu->selected_harbor >= 0 && eu->selected_harbor < eu->harbor_ships) {
    return;
  }
  eu->selected_harbor = 0;
}

void europe_tick_voyages(EuropeScreen* eu, const ColonizeUnitPool* units) {
  if (!eu) {
    return;
  }
  eu->open_on_dock = false;
  eu->docked_with_goods = false;
  /*
   * A ship that entered its lane during the turn just ending spends this
   * tick at sea without the counter moving — see EuropeHarborShip's
   * departed_this_turn. Without it an ordinary 1-turn crossing docked on
   * the very next turn, where DOS needs two End Turns (bugs.md).
   */
  for (int i = 0; i < eu->expected_ships; ++i) {
    if (eu->expected[i].departed_this_turn) {
      eu->expected[i].departed_this_turn = false;
    } else if (eu->expected[i].turns_left > 0) {
      eu->expected[i].turns_left--;
    }
  }
  for (int i = 0; i < eu->bound_ships; ++i) {
    if (eu->bound[i].departed_this_turn) {
      eu->bound[i].departed_this_turn = false;
    } else if (eu->bound[i].turns_left > 0) {
      eu->bound[i].turns_left--;
    }
  }
  /* Move due Expected ships into harbor; passengers go to dock front. */
  int i = 0;
  while (i < eu->expected_ships) {
    if (eu->expected[i].turns_left > 0) {
      ++i;
      continue;
    }
    if (eu->harbor_ships >= EUROPE_HARBOR_MAX) {
      break;
    }
    EuropeHarborShip ship = eu->expected[i];
    for (int j = i + 1; j < eu->expected_ships; ++j) {
      eu->expected[j - 1] = eu->expected[j];
    }
    eu->expected_ships--;
    europe_clear_ship(&eu->expected[eu->expected_ships]);
    ship.turns_left = 0;
    europe_disembark_passengers_to_dock(eu, &ship, units);
    eu->harbor[eu->harbor_ships++] = ship;
    eu->open_on_dock = true;
    for (int g = 0; g < EUROPE_SHIP_CARGO_MAX; ++g) {
      if (ship.hold_goods_amount[g] > 0) {
        eu->docked_with_goods = true;
        break;
      }
    }
    europe_refresh_harbor_selection(eu);
    snprintf(eu->status, sizeof(eu->status), "%s has docked in %s.", ship.name, eu->port_city);
  }
}

void europe_set_sound_hook(void (*play_fn)(int id)) {
  g_europe_sound_play = play_fn;
}
void europe_set_bgm_hook(void (*set_bgm_fn)(int pool)) {
  g_europe_set_bgm = set_bgm_fn;
}
void europe_notify_immigrant_sound(EuropeScreen* eu) {
  (void)eu;
  if (g_europe_set_bgm) {
    g_europe_set_bgm(2); /* 281f_0498(2) for the human's immigrant beat */
  }
}

int europe_cash_treasure(EuropeScreen* eu, int treasure_value) {
  if (!eu || treasure_value <= 0) {
    return 0;
  }
  int tax = eu->tax_percent;
  if (tax < 0) {
    tax = 0;
  }
  if (tax > 100) {
    tax = 100;
  }
  /*
   * FUN_48d3_06ba: Crown cut = min(tax_rate, 50). GAME.TXT @LOOTCASH still
   * describes tax-rate share; DOS clamps the treasure path at 50%.
   */
  if (tax > 50) {
    tax = 50;
  }
  const int credited = (treasure_value * (100 - tax)) / 100;
  eu->gold += credited;
  /*
   * GAME.TXT @LOOTCASH: "{%STRING0} treasure fleet laden with {%NUMBER0$}
   * arrives safely in %STRING1! Crown takes {%NUMBER1%%} share. {%NUMBER2$}
   * added to %STRING0 treasury." No ColonizeMsgCatalog reachable from this
   * call depth (europe.c has no catalog handle); wording matches the real
   * section verbatim rather than an invented "Treasure cash-in" stub.
   */
  const char* nation = eu->nation_name[0] ? eu->nation_name : "Our";
  const char* port = eu->port_city[0] ? eu->port_city : "Europe";
  /* %.16s bounds nation/port so the worst case (both fields maxed, nation
   * used twice) always fits eu->status[160] — silences -Wformat-truncation. */
  snprintf(
    eu->status,
    sizeof(eu->status),
    "%.16s treasure fleet laden with %d$ arrives safely in %.16s! Crown takes %d%% share. "
    "%d$ added to %.16s treasury.",
    nation,
    treasure_value,
    port,
    tax,
    credited,
    nation
  );
  if (g_europe_sound_play) {
    g_europe_sound_play(0x24); /* FUN_48d3_06ba 48d3:0b8f: Fiddler's Dance queued on the cash-in */
  }
  return credited;
}

int europe_sell_price(const EuropeScreen* eu, int cargo_type) {
  /* FUN_38fd_0040: max(euro_price − 1, 0). */
  if (!eu || cargo_type < 0 || cargo_type >= eu->cargo_count) {
    return 0;
  }
  const int p = eu->cargo[cargo_type].bid - 1;
  return p < 0 ? 0 : p;
}

int europe_buy_price(const EuropeScreen* eu, int cargo_type) {
  /* FUN_38fd_0016: max(euro_price + burden, 0) — cached as `ask`. */
  if (!eu || cargo_type < 0 || cargo_type >= eu->cargo_count) {
    return 0;
  }
  return eu->cargo[cargo_type].ask < 0 ? 0 : eu->cargo[cargo_type].ask;
}

int europe_net_after_tax(int gross, int tax_percent) {
  if (gross <= 0) {
    return 0;
  }
  if (tax_percent < 0) {
    tax_percent = 0;
  }
  if (tax_percent > 100) {
    tax_percent = 100;
  }
  /* FUN_364b_0688: tax = (tax·gross)/100 (32-bit), net = gross − tax. */
  const long taxed = ((long)tax_percent * (long)gross) / 100L;
  return gross - (int)taxed;
}

static int europe_1d44_term(int amount, int seller_is_human, int difficulty) {
  /* FUN_38fd_1d44: ((human ? difficulty − 2 : −2) · 16 · amount) / 100,
   * C division (truncates toward zero — the AI −384/100 → −3 case is what
   * the dutch2 pair needs). */
  const int k = seller_is_human ? (difficulty - 2) : -2;
  return (k * 16 * amount) / 100;
}

void europe_apply_trade_volume(
  EuropeScreen* eu,
  struct ColonizeCol1Save* col1,
  int seller_nation,
  int human_nation,
  int cargo_type,
  int amount,
  int is_buy,
  int immediate_threshold
) {
  if (!eu || amount <= 0 || cargo_type < 0 || cargo_type >= eu->cargo_count ||
      cargo_type >= EUROPE_CARGO_MAX) {
    return;
  }
  EuropeCargoQuote* q = &eu->cargo[cargo_type];
  int shift = q->volatility;
  if (shift < 0) {
    shift = 0;
  }
  if (shift > 15) {
    shift = 15;
  }
  int difficulty = eu->difficulty;
  if (col1) {
    difficulty = (int)col1->head.difficulty;
  }
  const int seller_is_human = (seller_nation == human_nation);
  int term = (amount << shift) + europe_1d44_term(amount, seller_is_human, difficulty);
  /* Only the human's record is live here; DOS also adds it to the other
   * three nation records (their nr is not ticked on this side). Nation 3
   * (the Dutch) takes (term·2)/3 regardless of who sold. */
  if (human_nation == 3) {
    term = (term * 2) / 3;
  }
  int nr = (int)eu->trade_nr[cargo_type];
  if (is_buy) {
    nr -= term;
  } else {
    nr += term;
  }
  if (col1 && seller_nation >= 0 && seller_nation < (int)COLONIZE_COL1_NATION_COUNT &&
      (unsigned)cargo_type < COLONIZE_COL1_CARGO_TYPES) {
    ColonizeCol1NationTrade* t = &col1->nation[seller_nation].trade;
    const int32_t signed_amt = is_buy ? -(int32_t)amount : (int32_t)amount;
    t->tons[cargo_type] += signed_amt;
    t->tons2[cargo_type] += signed_amt;
    if (is_buy) {
      /* 1d80: gold[cargo] −= buy_price·amount. */
      t->gold[cargo_type] -= (int32_t)europe_buy_price(eu, cargo_type) * (int32_t)amount;
    } else {
      /* 1dfa: gold[cargo] += (sell_price·amount·(100−tax))/100 — note the
       * ledger rounds the other way from the treasury credit (54 lumber @1,
       * 35% → ledger +35, treasury +36). */
      const int tax = (int)col1->nation[seller_nation].tax_rate;
      const long gross = (long)europe_sell_price(eu, cargo_type) * (long)amount;
      t->gold[cargo_type] += (int32_t)((gross * (long)(100 - tax)) / 100L);
    }
  }
  if (!immediate_threshold) {
    if (nr < -32768) {
      nr = -32768;
    }
    if (nr > 32767) {
      nr = 32767;
    }
    eu->trade_nr[cargo_type] = (int16_t)nr;
    return;
  }
  /* 0058 single-cargo: temporary attrition then rise/fall thresholds. */
  const int bid_before = q->bid; /* bugs.md 231: player-move price popups */
  int attrition = q->attrition;
  nr += attrition;
  const int rise = q->rise;
  const int fall = q->fall;
  if (rise > 0 && nr <= -(rise * 100)) {
    nr += rise * 100; /* shed regardless; bid step gated (see tick) */
    if (q->bid < q->high) {
      q->bid += 1;
    }
  }
  if (fall > 0 && nr >= fall * 100) {
    nr -= fall * 100;
    if (q->bid > q->low) {
      q->bid -= 1;
    }
  }
  nr -= attrition;
  /* Only clamp when @CARGO low/high were loaded (high > low). */
  if (q->high > q->low) {
    if (q->bid < q->low) {
      q->bid = q->low;
    }
    if (q->bid > q->high) {
      q->bid = q->high;
    }
  }
  if (q->bid < 0) {
    q->bid = 0;
  }
  q->ask = q->bid + q->burden;
  /* bugs.md 231: a player transaction that moved the price gets the same
   * @PRICEUP/@PRICEDOWN dialog the EOT market tick shows — record the event;
   * game_loop drains it into a popup right after the sell/buy. */
  if (q->bid != bid_before && eu->price_event_count < EUROPE_CARGO_MAX) {
    eu->price_event_cargo[eu->price_event_count] = cargo_type;
    eu->price_event_dir[eu->price_event_count] = q->bid > bid_before ? 1 : -1;
    eu->price_event_count++;
  }
  if (nr < -32768) {
    nr = -32768;
  }
  if (nr > 32767) {
    nr = 32767;
  }
  eu->trade_nr[cargo_type] = (int16_t)nr;
}

void europe_apply_volume_price(EuropeScreen* eu, int cargo_type, int amount, int is_buy) {
  /* Harbor buy/sell: human seller (nation unknown here → 1d44 uses
   * eu->difficulty, Dutch rule off), then FUN_38fd_0058(0, cargo). Callers
   * with a col1 should use europe_apply_trade_volume directly. */
  europe_apply_trade_volume(eu, NULL, -1, -1, cargo_type, amount, is_buy, 1);
}

void europe_tick_market_prices(
  EuropeScreen* eu,
  struct ColonizeCol1Save* col1,
  struct ColonizeColonyPool* colonies,
  int human_nation,
  uint32_t turn
) {
  /*
   * FUN_38fd_0058(0, 0xffff) — the human's 5e52 phase-3 call, with nation 0's
   * pass folded in. Validated 2026-08-28 against two real-DOS turn pairs
   * (golden_market_prices01; python replica iterated until both matched):
   *   phase 1 — ledger[g] = price_group[g] (signed) + Σ_n max(0, tons2[n][g])
   *             (nation +0xfc, NOT tons); in nation 0's pass only, and only
   *             while nation 0 is not withdrawn: price_group[g] -= ledger>>7.
   *             The colony-stock approximation that used to sit here is gone.
   *   phase 2 — cargos 9..12: sign(bid − 3·Σ/L) · (rise+fall)/2 · 100
   *   phase 3 — cargos 1..4: sign · (rise+fall)/2 (no ×100; fur year bias)
   *   phase 4 — nr += attrition (Dutch ×2 on odd post-increment turns),
   *             rise/fall ±1 bid within [low, high]; @PRICEUP/@PRICEDOWN.
   * Column roles (NAMES.TXT @CARGO): rise=c6, fall=c7, attrition=c8 — the
   * same fields europe_load_tables already fills. AI nations' own records
   * are not ticked here (their bids only feed ai_euro purchases).
   * Cite: viceroy_unpacked.c 58741–59005; turn/europe_nation_eot.md.
   */
  (void)colonies;
  if (!eu) {
    return;
  }
  eu->price_event_count = 0;
  if (col1) {
    eu->difficulty = col1->head.difficulty > 8 ? 8 : col1->head.difficulty;
  }

  /* Phase 1 — pool decay + ledger. */
  long ledger[16];
  if (col1) {
    const bool nation0_active = col1->player[0].control != 2;
    for (int c = 0; c < 16; ++c) {
      long s = (long)(int16_t)col1->head.price_group_state[c];
      for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
        const int32_t t = col1->nation[n].trade.tons2[c];
        if (t > 0) {
          s += (long)t;
        }
      }
      if (nation0_active) {
        /* Nation 0 decays in its own pass; a later human pass sees the
         * decayed pool, nation 0 itself (as human) the pre-decay copy. */
        const long decayed = (long)(int16_t)col1->head.price_group_state[c] - (s >> 7);
        col1->head.price_group_state[c] = (uint16_t)(int16_t)decayed;
        if (human_nation != 0) {
          s -= (s >> 7);
        }
      }
      ledger[c] = s;
    }
  }

  /* Phases 2–3. */
  if (col1) {

    /* Phase 2 — Rum..Coats (9..12): pressure += sign * mid * 100. */
    {
      long sum = ledger[9] + ledger[10] + ledger[11] + ledger[12];
      if (sum <= 0) {
        sum = 1;
      }
      for (int c = 9; c <= 12; ++c) {
        if (c >= eu->cargo_count) {
          break;
        }
        long L = ledger[c];
        if (L <= 0) {
          L = 1;
        }
        const long ratio = (sum * 3) / L;
        const int bid = eu->cargo[c].bid;
        int sign = 0;
        if (bid > (int)ratio) {
          sign = 1;
        } else if (bid < (int)ratio) {
          sign = -1;
        }
        const int mid = (eu->cargo[c].rise + eu->cargo[c].fall) / 2;
        const int delta = sign * mid * 100;
        int nr = (int)eu->trade_nr[c] + delta;
        if (nr < -32768) {
          nr = -32768;
        }
        if (nr > 32767) {
          nr = 32767;
        }
        eu->trade_nr[c] = (int16_t)nr;
      }
    }

    /* Phase 3 — Sugar..Furs (1..4): pressure += mid * sign (no ×100). */
    {
      long half0 = ledger[0] / 2;
      long sum = half0 + ledger[1] + ledger[2] + ledger[3];
      if (sum <= 0) {
        sum = 1;
      }
      const int year = (int)col1->head.year;
      for (int c = 1; c <= 4; ++c) {
        if (c >= eu->cargo_count) {
          break;
        }
        long L = ledger[c];
        if (c == 4) {
          L /= 2;
        }
        if (L <= 0) {
          L = 1;
        }
        long ratio = (sum * 3) / L;
        if (c == 4) {
          if (year < 0x6a4) {
            ratio += 1; /* < 1700 */
          }
          if (year < 0x640) {
            ratio += 1; /* < 1600 */
          }
        }
        const int bid = eu->cargo[c].bid;
        int sign = 0;
        if (bid > (int)ratio) {
          sign = 1;
        } else if (bid < (int)ratio) {
          sign = -1;
        }
        const int mid = (eu->cargo[c].rise + eu->cargo[c].fall) / 2;
        const int delta = mid * sign;
        int nr = (int)eu->trade_nr[c] + delta;
        if (nr < -32768) {
          nr = -32768;
        }
        if (nr > 32767) {
          nr = 32767;
        }
        eu->trade_nr[c] = (int16_t)nr;
      }
    }
  }

  int last_rise = -1;
  int last_fall = -1;
  /* DOS: `0x9e12 == 3 && (turn & 1)` — the Netherlands' market recovers twice
   * as fast on odd turns (turn already incremented for this EOT). */
  const bool dutch_double = (human_nation == 3) && ((turn & 1u) != 0u);
  for (int c = 0; c < eu->cargo_count && c < EUROPE_CARGO_MAX; ++c) {
    EuropeCargoQuote* q = &eu->cargo[c];
    int nr = (int)eu->trade_nr[c] + (dutch_double ? q->attrition * 2 : q->attrition);
    const int rise = q->rise;
    const int fall = q->fall;
    /* DOS: the pressure word always sheds rise*100 / fall*100 at the
     * threshold; only the ±1 bid step is gated by [low, high]. Gating the
     * shed on the bid too (the old code) let a capped cargo's pressure run
     * away — golden_market_prices01 caught it on Rum at the 20 cap. */
    if (rise > 0 && nr <= -(rise * 100)) {
      nr += rise * 100;
      if (q->bid < q->high) {
        q->bid += 1;
        last_rise = c;
        if (eu->price_event_count < EUROPE_CARGO_MAX) {
          eu->price_event_cargo[eu->price_event_count] = c;
          eu->price_event_dir[eu->price_event_count] = 1;
          eu->price_event_count++;
        }
      }
    }
    if (fall > 0 && nr >= fall * 100) {
      nr -= fall * 100;
      if (q->bid > q->low) {
        q->bid -= 1;
        last_fall = c;
        if (eu->price_event_count < EUROPE_CARGO_MAX) {
          eu->price_event_cargo[eu->price_event_count] = c;
          eu->price_event_dir[eu->price_event_count] = -1;
          eu->price_event_count++;
        }
      }
    }
    if (q->high > q->low) {
      if (q->bid < q->low) {
        q->bid = q->low;
      }
      if (q->bid > q->high) {
        q->bid = q->high;
      }
    }
    if (q->bid < 0) {
      q->bid = 0;
    }
    q->ask = q->bid + q->burden;
    if (nr < -32768) {
      nr = -32768;
    }
    if (nr > 32767) {
      nr = 32767;
    }
    eu->trade_nr[c] = (int16_t)nr;
  }
  /*
   * Phase 4 dialog crumbs 0xfa8/0xfb0 -> price_event_cargo[]/dir[] (queued
   * in loop order above, one entry per cargo that actually crossed
   * threshold this tick — DOS calls FUN_281f_0652 inline per cargo, so two
   * different cargos changing the same turn both get their own OK dialog;
   * turn.c walks the full list). The status line below stays a single-line
   * summary (rise wins ties, matching the old behaviour) — cosmetic only.
   * Real DOS wording from GAME.TXT @PRICEUP/@PRICEDOWN (COLONIZE/GAME.TXT:1683-1689):
   *   "The price of {%STRING0} in %STRING1 has risen to {%NUMBER0$}."
   *   "The price of {%STRING0} in %STRING1 has fallen to {%NUMBER0$}."
   * STRING0 = cargo name (-0x6840 @CARGO table), STRING1 = nation home-port
   * city (-0x7c74 table == eu->port_city), NUMBER0 = new bid.
   */
  if (last_rise >= 0) {
    const char* nm =
      (eu->cargo[last_rise].name[0]) ? eu->cargo[last_rise].name : "Goods";
    const char* port = eu->port_city[0] ? eu->port_city : "Europe";
    snprintf(
      eu->status, sizeof(eu->status),
      "The price of %s in %s has risen to %d.", nm, port, eu->cargo[last_rise].bid
    );
  } else if (last_fall >= 0) {
    const char* nm =
      (eu->cargo[last_fall].name[0]) ? eu->cargo[last_fall].name : "Goods";
    const char* port = eu->port_city[0] ? eu->port_city : "Europe";
    snprintf(
      eu->status, sizeof(eu->status),
      "The price of %s in %s has fallen to %d.", nm, port, eu->cargo[last_fall].bid
    );
  }
}

int europe_compute_immigration_score(
  const ColonizeColonyPool* colonies,
  const ColonizeUnitPool* units,
  const ColonizeCol1Save* col1,
  int nation_id
) {
  /*
   * FUN_38fd_584a: score ≈ colony pop sum + unit count; <<1 if <4000; +8;
   * cap 4000; AI/non-human ((8-diff)*score)>>3; nation0 *2/3.
   * Cite: europe_nation_eot.md phase 4; ~68248.
   */
  if (nation_id < 0 || nation_id > 3) {
    return 0;
  }
  int pop = 0;
  if (colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &colonies->colonies[i];
      if (c->active && c->nation_id == nation_id) {
        pop += c->colonist_count > 0 ? c->colonist_count : c->population;
      }
    }
  }
  int units_n = 0;
  if (units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units->units[i];
      if (u->active && u->nation_id == nation_id) {
        units_n++;
      }
    }
  }
  /* DOS counts every unit record of the nation, including colonists waiting
   * on the Europe docks — those are the (236,236) mirror units in the pool
   * (seed-100 TURN5→6: needed 9→10 once the first immigrant landed). */
  int score = pop + units_n;
  if (score < 4000) {
    score <<= 1;
  }
  score += 8;
  if (score > 4000) {
    score = 4000;
  }
  if (col1) {
    const int control =
      (nation_id < 4) ? (int)col1->player[nation_id].control : 1;
    if (nation_id > 3 || control != 0) {
      int diff = (int)col1->head.difficulty;
      if (diff < 0) {
        diff = 0;
      }
      if (diff > 8) {
        diff = 8;
      }
      score = ((8 - diff) * score) >> 3;
    }
    if (nation_id == 0) {
      score = (score << 1) / 3;
    }
  }
  return score;
}

int europe_tick_immigration_pressure(
  EuropeScreen* eu,
  const ColonizeColonyPool* colonies,
  const ColonizeUnitPool* units,
  const ColonizeCol1Save* col1,
  int nation_id,
  ColonizeDosRng* rng
) {
  /*
   * DOS: +0x30 = 584a score (needed_crosses); +0x2e += 2 (and church crosses
   * already applied by caller); spawn when score < pressure. Cite: 5e52 ~68558.
   */
  if (!eu || nation_id < 0 || nation_id > 3) {
    return 0;
  }
  if (col1) {
    int diff = (int)col1->head.difficulty;
    if (diff < 0) {
      diff = 0;
    }
    if (diff > 8) {
      diff = 8;
    }
    eu->difficulty = (uint8_t)diff;
  }
  const int score = europe_compute_immigration_score(colonies, units, col1, nation_id);
  int need = score;
  if (need < 0) {
    need = 0;
  }
  if (need > 65535) {
    need = 65535;
  }
  eu->needed_crosses = (uint16_t)need;
  eu->immigration_score = (int16_t)(need > 32767 ? 32767 : need);

  /*
   * 584a *param_2 (viceroy_unpacked.c:68258-68280): +2 a turn, but once the
   * nation's dock latch is up (nation_flags 0x40, set by the first crosses
   * immigrant = crosses_immigrant_seen) every colonist still waiting on the
   * Europe dock flips the tick negative: 2 → -2 → -4 …, i.e. -2 per waiting
   * immigrant. Empty the dock and the +2 resumes — the port used to freeze
   * the meter forever after the first immigrant.
   * Cite: test-saves-ai TURN1–4 = 2/4/6/8 (flag clear, no dock unit),
   * TURN5–7 = 0 (flag 0x40 + one dock colonist, drain clamped at 0).
   */
  int delta = 2;
  if (eu->crosses_immigrant_seen) {
    for (int i = 0; i < eu->dock_count && i < EUROPE_DOCK_MAX; ++i) {
      if (!eu->dock[i].present) {
        continue;
      }
      delta = (delta < 1) ? delta - 2 : -2;
    }
  }
  int cur = (int)eu->current_crosses + delta;
  if (cur < 0) {
    cur = 0; /* 5e52 clamps the sum at 0 before the compare. */
  }
  if (cur > 65535) {
    cur = 65535;
  }
  eu->current_crosses = (uint16_t)cur;
  eu->immigration_pressure = (int16_t)(cur > 32767u ? 32767 : (int)cur);
  europe_refresh_recruit_passage(eu);

  /* Phase 5: needed < current → dock immigrant; clear current. */
  if (need > 0 && (int)eu->current_crosses > need) {
    /*
     * 5e52 ~68577: FF 0x14 (Brewster) owned → FUN_38fd_4884(0,1) instead of
     * the random 04d4(0,2) pool pick: the player chooses (@RECRUITCHOOSE),
     * crosses are only zeroed once a pick lands (4884 tail, param_1==0), so
     * a cancelled dialog re-asks next turn. Caller enqueues the CHOICE and
     * europe_brewster_pick_from_pool applies it.
     */
    if ((col1 && founding_fathers_nation_has(col1, nation_id, FF_WILLIAM_BREWSTER)) ||
        eu->brewster_no_criminals) {
      europe_apply_brewster(eu, 1); /* bugs.md 230: purge stale slots BEFORE the pick */
      return 2;
    }
    eu->current_crosses = 0;
    eu->immigration_pressure = 0;
    eu->crosses_immigrant_seen = true;
    europe_refresh_recruit_passage(eu);
    if (europe_immigrant_from_pool(eu, rng)) {
      /* bugs.md 229: keep open_on_dock — arrivals own it (see above). */
      snprintf(eu->status, sizeof(eu->status), "Immigrant arrives in Europe.");
      return 1;
    }
  }
  return 0;
}

int europe_cargo_boycotted(const EuropeScreen* eu, int cargo_type) {
  if (!eu || cargo_type < 0 || cargo_type >= EUROPE_CARGO_MAX) {
    return 0;
  }
  return (eu->boycott_bitmap & (uint16_t)(1u << cargo_type)) != 0;
}

int europe_buyback_boycott(
  EuropeScreen* eu, struct ColonizeCol1Save* col1, int human_nation, int cargo_type
) {
  /*
   * FUN_38fd_2dfe (viceroy_unpacked.c:60904-60945), clean disassembly, no
   * warnings. Real DOS trigger per GAME.TXT @SOMEBOYCOTT: "Some of the
   * cargo could not be unloaded because of a parliamentary boycott. If you
   * want to ask that the boycott be lifted, click on the cargo type in
   * question." — i.e. clicking a boycotted cell on the Europe market strip
   * (wired at that exact click site: game_loop.c EUROPE_HIT_MARKET).
   *
   * Traced formula (iVar3 = thunk_FUN_291f_0c3e -> FUN_38fd_0016, the same
   * "effective ask price" this file already exposes as
   * eu->cargo[cargo_type].ask):
   *   cost = ask_price * 500         // fandom "500 tons of that good"
   *   if nation.gold < cost: GAME.TXT @KISSSORRY, no state change
   *   else: nation.gold -= cost
   *         nation.royal_money += cost   // DOS write at nation+0x22, the
   *                                      // exact byte offset col1_save.h
   *                                      // already documents as royal_money
   *                                      // (REF budget, FUN_43f7_1d42) --
   *                                      // paying back taxes funds the Crown
   *         nation.boycott_bitmap &= ~(1 << cargo_type)   // nation+0x20
   * DOS gates this to the human-controlled nation (player_control[nation]
   * check at 0x543f); callers here only ever pass the human nation for the
   * same reason (only the human clicks their own market strip).
   *
   * GAME.TXT @KISSUP's Pay/Cancel CHOICE dialog chrome PARKED (VGA modal,
   * matching this file's existing chrome-PARKED precedent -- e.g.
   * europe_custom_house_autosell); ported as an immediate action + status
   * line instead, same pattern as the '+'/'U' immediate buy/sell keys.
   * Returns gold paid (>0) on success, 0 on no-op/insufficient funds.
   */
  if (!eu || !col1) {
    return 0;
  }
  if (human_nation < 0 || human_nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0;
  }
  if (cargo_type < 0 || cargo_type >= eu->cargo_count) {
    return 0;
  }
  if (!europe_cargo_boycotted(eu, cargo_type)) {
    return 0;
  }
  const int price = eu->cargo[cargo_type].ask;
  if (price <= 0) {
    return 0;
  }
  const int cost = price * 500;
  if (eu->gold < cost) {
    snprintf(eu->status, sizeof(eu->status), "Unfortunately, we only have %d$ available.", eu->gold);
    return 0;
  }
  eu->gold -= cost;
  ColonizeCol1Nation* nation = &col1->nation[human_nation];
  nation->gold = (uint32_t)(eu->gold < 0 ? 0 : eu->gold);
  nation->royal_money += cost;
  nation->boycott_bitmap &= (uint16_t)~(1u << cargo_type);
  eu->boycott_bitmap = nation->boycott_bitmap;
  const char* cname = eu->cargo[cargo_type].name[0] ? eu->cargo[cargo_type].name : "That cargo";
  snprintf(
    eu->status, sizeof(eu->status), "Paid %d$ in back taxes -- boycott on %s lifted.", cost, cname
  );
  return cost;
}

int europe_sell_proceeds(const EuropeScreen* eu, int cargo_type, int amount) {
  if (!eu || amount <= 0 || cargo_type < 0 || cargo_type >= eu->cargo_count) {
    return 0;
  }
  /* FUN_38fd_1f0c: gross = (euro_price − 1)·amount; tax taken by the caller
   * as gross − gross·tax/100. */
  const int price = europe_sell_price(eu, cargo_type);
  if (price <= 0) {
    return 0;
  }
  return europe_net_after_tax(price * amount, eu->tax_percent);
}

int europe_sell_hold(EuropeScreen* eu, int harbor_index, int hold_index) {
  if (!eu || harbor_index < 0 || harbor_index >= eu->harbor_ships) {
    return 0;
  }
  if (hold_index < 0 || hold_index >= EUROPE_SHIP_CARGO_MAX) {
    return 0;
  }
  EuropeHarborShip* ship = &eu->harbor[harbor_index];
  const int amt = ship->hold_goods_amount[hold_index];
  const int ctype = ship->hold_goods_type[hold_index];
  if (amt <= 0 || amt >= 255) {
    return 0;
  }
  if (europe_cargo_boycotted(eu, ctype)) {
    const char* cname =
      (ctype >= 0 && ctype < eu->cargo_count) ? eu->cargo[ctype].name : "That cargo";
    snprintf(
      eu->status, sizeof(eu->status), "%s is boycotted — cannot trade in Europe.", cname
    );
    return 0;
  }
  const int gained = europe_sell_proceeds(eu, ctype, amt);
  eu->gold += gained;
  ship->hold_goods_amount[hold_index] = 0;
  ship->hold_goods_type[hold_index] = 0;
  europe_apply_volume_price(eu, ctype, amt, 0);
  const char* cname =
    (ctype >= 0 && ctype < eu->cargo_count) ? eu->cargo[ctype].name : "cargo";
  snprintf(eu->status, sizeof(eu->status), "Sold %d %s for %d$.", amt, cname, gained);
  diag_info(
    "EUROPE sold %d %s from %s: bid=%d tax=%d%% proceeds=%d gold=%d",
    amt, cname, ship->name[0] ? ship->name : "ship",
    europe_sell_price(eu, ctype), eu->tax_percent, gained, eu->gold
  );
  return gained;
}

/*
 * FUN_364b_0636: Custom House may auto-sell this cargo type.
 * Deny Food(0), Horses(8), Tools(0xe), Muskets(0xf).
 * Ore(6) extra DOS deny path not mapped — allow Ore (no invent).
 *
 * **2026-08-27: a same-day attempt to also deny Lumber(5) here (matching a
 * `param_1 != 5` term this function's raw decompile appears to have) was
 * reverted — both `golden_colony_prod01`/`02` (real DOS `.SAV` ground
 * truth) show Lumber genuinely auto-sold down to 50, contradicting that
 * reading. The `param_1==5` deny term is real in the decompile but must
 * gate something other than plain cargo-index-5 in this calling context
 * (`local_b6` in `FUN_364b_0688`'s loop may not be a raw cargo index the
 * way the other four terms' values are), or a second, different type-gate
 * function is the real one `0688` calls — not re-investigated this pass.
 * Golden evidence overrides the static read; leaving the deny list at its
 * original 4 entries, unchanged from before this attempt.
 */
static int europe_custom_house_cargo_eligible(int cargo_type) {
  /*
   * Resolved 2026-08-28: FUN_364b_0688 picks the gate by controller —
   * human colonies use FUN_281f_0cfe → FUN_15eb_0302 (colony +0x8a bit per
   * cargo == custom_house_bits), and ONLY AI colonies use FUN_364b_0636
   * (thunk_FUN_291f_09c0, confirmed via address_mapping.csv). So the
   * Lumber deny term is real, but it never applied to the human's Custom
   * House — which is why the real-DOS saves sold lumber. This function is
   * now the AI-only 0636 gate: deny Food/Lumber/Horses/Tools/Muskets; Ore
   * has an extra deny arm (building 3 present or DS:0x8de4/0x8de6 set)
   * that is not modelled here — Ore stays allowed.
   */
  if (cargo_type == COLONIZE_CARGO_FOOD || cargo_type == COLONIZE_CARGO_LUMBER ||
      cargo_type == COLONIZE_CARGO_HORSES || cargo_type == COLONIZE_CARGO_TOOLS ||
      cargo_type == COLONIZE_CARGO_MUSKETS) {
    return 0;
  }
  return cargo_type >= 0 && cargo_type < COLONIZE_CARGO_COUNT;
}

/* FUN_364b_0636 denylist — AI Custom House + AI peace Europe export sail. */
int europe_cargo_export_eligible(int cargo_type) {
  return europe_custom_house_cargo_eligible(cargo_type);
}

static int europe_custom_house_bit_enabled(uint16_t bits, int cargo_type) {
  /*
   * bits==0 → nothing configured yet, sell nothing (per-cargo UI PARKED —
   * see fandom_col1994.md Custom House: sells a "configured" cargo type,
   * not everything by default). Player-confirmed 2026-08-16 (real DOS
   * COLONY00/01_no-transports.SAV pair, colony-prod-tests): a real save
   * with Custom House built but custom_house_bits==0 (no cargo toggled)
   * sold *nothing* that turn — the old "bits==0 → all eligible" stand-in
   * auto-sold every stock>99 cargo down to 50 and was never DOS-verified.
   */
  if (bits == 0) {
    return 0;
  }
  return (bits >> cargo_type) & 1u;
}

bool europe_custom_house_cargo_enabled(uint16_t custom_house_bits, int cargo_type) {
  if (cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT) {
    return false;
  }
  return europe_custom_house_bit_enabled(custom_house_bits, cargo_type) != 0;
}

int europe_custom_house_autosell_ex(
  EuropeScreen* eu,
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  ColonizeCol1Save* col1,
  int human_nation,
  EuropeCustomHouseSale* out,
  int out_max,
  int* out_count
) {
  if (out_count) {
    *out_count = 0;
  }
  /*
   * FUN_364b_0688 after production: Custom House + type gate + stock>99 →
   * sell stock-50. Cite: viceroy_unpacked.c FUN_364b_0688 / FUN_364b_0636;
   * docs/fandom_col1994.md Custom House (boycott bypass; WoI untaxed).
   */
  if (!eu || !pool || !colony || !colony->active) {
    return 0;
  }
  const int ch_id = colonies_find_building(pool, "Custom House");
  if (ch_id < 0 || ch_id >= COLONIZE_BUILDING_TYPES_MAX || !colony->has_building[ch_id]) {
    return 0;
  }
  const int nation = colony->nation_id;
  const int is_human = (nation == human_nation);
  /* FUN_364b_0688: a human colony's Custom House is shut while an enemy
   * armed ship / Man-O-War sits next to it (colony +0x1b & 3). */
  if (is_human && (colony->ai_flags & 0x03u) != 0u) {
    return 0;
  }
  const int woi = col1 && col1->head.game_options.woi != 0;
  int tax = 0;
  if (!woi) {
    if (is_human) {
      tax = eu->tax_percent;
    } else if (col1 && nation >= 0 && nation < (int)COLONIZE_COL1_NATION_COUNT) {
      tax = (int)col1->nation[nation].tax_rate;
    } else {
      tax = eu->tax_percent;
    }
  }
  if (tax < 0) {
    tax = 0;
  }
  if (tax > 100) {
    tax = 100;
  }
  ColonizeCol1Nation* nat =
    (col1 && nation >= 0 && nation < (int)COLONIZE_COL1_NATION_COUNT) ? &col1->nation[nation]
                                                                       : NULL;

  int total = 0;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    /* Human: per-cargo toggle bits (FUN_15eb_0302). AI: FUN_364b_0636. */
    if (is_human) {
      if (!europe_custom_house_bit_enabled(colony->custom_house_bits, c)) {
        continue;
      }
    } else if (!europe_custom_house_cargo_eligible(c)) {
      continue;
    }
    /* FUN_364b_0688: if (99 < stock) sell stock - 50 (leave 50 in warehouse). */
    if (colony->stock[c] <= 99) {
      continue;
    }
    const int amount = colony->stock[c] - 50;
    if (amount <= 0) {
      continue;
    }
    if (c >= eu->cargo_count) {
      continue;
    }
    /* Sells at euro_price − 1 (FUN_291f_09ea → FUN_38fd_0040); a zero price
     * still moves the goods (DOS has no price gate here). */
    const int price = europe_sell_price(eu, c);
    const int gross = price * amount;
    const int gained = europe_net_after_tax(gross, tax);
    const int tax_paid = gross - gained;
    colony->stock[c] = 50;
    total += gained;
    if (out && out_count && *out_count < out_max) {
      EuropeCustomHouseSale* rec = &out[*out_count];
      rec->cargo = c;
      rec->amount = amount;
      rec->gross = gross;
      rec->tax_percent = tax;
      rec->tax_paid = tax_paid;
      rec->net = gained;
      (*out_count)++;
    }
    if (nat) {
      nat->gold += (uint32_t)gained;
      /* nation +0x22 (royal_money) += tax paid; +0x26 write-only cumulative
       * net trade income (unknown24_pad, int32 LE). */
      nat->royal_money += tax_paid;
      uint32_t cum = (uint32_t)nat->unknown24_pad[0] | ((uint32_t)nat->unknown24_pad[1] << 8) |
                     ((uint32_t)nat->unknown24_pad[2] << 16) |
                     ((uint32_t)nat->unknown24_pad[3] << 24);
      cum += (uint32_t)gained;
      nat->unknown24_pad[0] = (uint8_t)(cum & 0xffu);
      nat->unknown24_pad[1] = (uint8_t)((cum >> 8) & 0xffu);
      nat->unknown24_pad[2] = (uint8_t)((cum >> 16) & 0xffu);
      nat->unknown24_pad[3] = (uint8_t)((cum >> 24) & 0xffu);
    }
    if (is_human) {
      eu->gold += gained;
    }
    /* FUN_291f_0a2e → FUN_38fd_1dfa; no FUN_38fd_0058 step here. */
    europe_apply_trade_volume(eu, col1, nation, human_nation, c, amount, 0, 0);
  }
  if (total > 0 && nation == human_nation) {
    if (colony->name[0]) {
      snprintf(
        eu->status,
        sizeof(eu->status),
        "Custom House in %s sold for %d$.",
        colony->name,
        total
      );
    } else {
      snprintf(eu->status, sizeof(eu->status), "Custom House sold goods for %d$.", total);
    }
  }
  return total;
}

int europe_custom_house_autosell(
  EuropeScreen* eu,
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  ColonizeCol1Save* col1,
  int human_nation
) {
  return europe_custom_house_autosell_ex(
    eu, pool, colony, col1, human_nation, NULL, 0, NULL
  );
}

int europe_ai_colony_dump_sell(
  EuropeScreen* eu,
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  ColonizeCol1Save* col1,
  int human_nation
) {
  /*
   * FUN_364b_0688 phase O: non-human Euro (nation≤3, control≠0) sells warehouse
   * surplus for gold before spoilage. Stock is not reduced here — spoilage clamps.
   * Cite: viceroy_unpacked.c ~57806–57848; 291f_0a2e → 38fd_1dfa.
   */
  if (!eu || !colony || !colony->active) {
    return 0;
  }
  const int nation = colony->nation_id;
  if (nation < 0 || nation > 3 || nation == human_nation) {
    return 0;
  }

  int tax = eu->tax_percent;
  if (col1 && nation < (int)COLONIZE_COL1_NATION_COUNT) {
    tax = (int)col1->nation[nation].tax_rate;
  }
  if (tax < 0) {
    tax = 0;
  }
  if (tax > 100) {
    tax = 100;
  }

  int total = 0;
  for (int c = 1; c < COLONIZE_CARGO_COUNT; ++c) {
    const int cap = colonies_warehouse_capacity(pool, colony, c);
    if (cap <= 0) {
      continue;
    }
    const int surplus = colony->stock[c] - cap;
    if (surplus <= 0) {
      continue;
    }
    /* Horses: DOS adds surplus to Europe horses word; sell amount → 0. */
    if (c == COLONIZE_CARGO_HORSES) {
      unsigned h = (unsigned)eu->nation_horses[nation] + (unsigned)surplus;
      if (h > 65535u) {
        h = 65535u;
      }
      eu->nation_horses[nation] = (uint16_t)h;
      continue;
    }
    if (c >= eu->cargo_count) {
      continue;
    }
    const int bid = europe_sell_price(eu, c);
    if (bid <= 0) {
      continue;
    }
    /*
     * Muskets: DOS while surplus>49: Europe musket counter++, amount−50; then
     * sell remainder for gold.
     */
    int amount = surplus;
    if (c == COLONIZE_CARGO_MUSKETS) {
      while (amount > 49) {
        if (eu->nation_musket_batches[nation] < 65535u) {
          eu->nation_musket_batches[nation]++;
        }
        amount -= 50;
      }
      if (amount <= 0) {
        continue;
      }
    }
    const int gained = europe_net_after_tax(bid * amount, tax);
    total += gained;
    if (col1 && nation < (int)COLONIZE_COL1_NATION_COUNT) {
      col1->nation[nation].gold += (uint32_t)gained;
    }
    /* 291f_0a2e → 38fd_1dfa: ledgers + volume, no 0058 step. */
    europe_apply_trade_volume(eu, col1, nation, human_nation, c, amount, 0, 0);
  }
  if (total > 0) {
    snprintf(eu->status, sizeof(eu->status), "AI warehouse dump-sold for %d$.", total);
  }
  return total;
}

int europe_sell_unit_hold(
  EuropeScreen* eu,
  ColonizeUnitPool* units,
  int unit_id,
  int hold_index
) {
  /*
   * Map/transport dump-sell (no harbor chrome). Tax path = europe_sell_proceeds:
   * bid * amount * (100 - eu->tax_percent) / 100 — same Crown cut as
   * europe_sell_hold. Cite: Colonization.pdf Europe buy/sell + tax;
   * docs/manual_gap.md Europe commodity trade.
   */
  if (!eu || !units) {
    return 0;
  }
  ColonizeUnit* u = units_get(units, unit_id);
  if (!u || !u->active || !units_is_transport(units, unit_id)) {
    return 0;
  }
  if (hold_index < 0 || hold_index >= COLONIZE_UNIT_CARGO_MAX) {
    return 0;
  }
  const int amt = u->hold_goods_amount[hold_index];
  const int ctype = u->hold_goods_type[hold_index];
  if (amt <= 0 || amt >= 255) {
    return 0;
  }
  if (europe_cargo_boycotted(eu, ctype)) {
    const char* cname =
      (ctype >= 0 && ctype < eu->cargo_count) ? eu->cargo[ctype].name : "That cargo";
    snprintf(
      eu->status, sizeof(eu->status), "%s is boycotted — cannot trade in Europe.", cname
    );
    return 0;
  }
  const int gained = europe_sell_proceeds(eu, ctype, amt);
  eu->gold += gained;
  u->hold_goods_amount[hold_index] = 0;
  u->hold_goods_type[hold_index] = 0;
  europe_apply_volume_price(eu, ctype, amt, 0);
  const char* cname =
    (ctype >= 0 && ctype < eu->cargo_count) ? eu->cargo[ctype].name : "cargo";
  snprintf(eu->status, sizeof(eu->status), "Sold %d %s for %d$.", amt, cname, gained);
  diag_info(
    "EUROPE sold %d %s from unit %d: bid=%d tax=%d%% proceeds=%d gold=%d",
    amt, cname, unit_id, europe_sell_price(eu, ctype), eu->tax_percent, gained, eu->gold
  );
  return gained;
}

int europe_buy_unit_cargo(
  EuropeScreen* eu,
  ColonizeUnitPool* units,
  int unit_id,
  int cargo_type,
  int amount
) {
  /*
   * Map/transport buy (no harbor chrome) — trade-route load list at a Europe
   * stop (DOS FUN_479b_0bd0 → FUN_38fd_1fa2 via FUN_291f_0b42). Flat ask ×
   * bought, boycott gated, volume price applied, same as europe_buy_cargo.
   */
  if (!eu || !units || cargo_type < 0 || cargo_type >= eu->cargo_count || amount <= 0) {
    return 0;
  }
  ColonizeUnit* u = units_get(units, unit_id);
  if (!u || !u->active || !units_is_transport(units, unit_id)) {
    return 0;
  }
  if (europe_cargo_boycotted(eu, cargo_type)) {
    return 0;
  }
  const int ask = eu->cargo[cargo_type].ask;
  if (ask <= 0) {
    return 0;
  }
  int buy = amount > 100 ? 100 : amount;
  const int can_afford = eu->gold / ask;
  if (buy > can_afford) {
    buy = can_afford;
  }
  if (buy <= 0) {
    return 0;
  }
  const int loaded = units_load_goods(units, unit_id, cargo_type, buy);
  if (loaded <= 0) {
    return 0;
  }
  eu->gold -= loaded * ask;
  europe_apply_volume_price(eu, cargo_type, loaded, 1);
  diag_info(
    "EUROPE bought %d %s onto unit %d: ask=%d cost=%d gold=%d",
    loaded, eu->cargo[cargo_type].name, unit_id, ask, loaded * ask, eu->gold
  );
  return loaded;
}

int europe_buy_cargo(EuropeScreen* eu, int harbor_index, int cargo_type, int amount) {
  if (!eu || harbor_index < 0 || harbor_index >= eu->harbor_ships) {
    return 0;
  }
  if (cargo_type < 0 || cargo_type >= eu->cargo_count || amount <= 0) {
    return 0;
  }
  if (europe_cargo_boycotted(eu, cargo_type)) {
    const char* cname = eu->cargo[cargo_type].name[0] ? eu->cargo[cargo_type].name : "That cargo";
    snprintf(
      eu->status, sizeof(eu->status), "%s is boycotted — cannot trade in Europe.", cname
    );
    return 0;
  }
  const int ask = eu->cargo[cargo_type].ask;
  if (ask <= 0) {
    europe_set_status(eu, "Cannot buy that cargo.");
    return 0;
  }
  EuropeHarborShip* ship = &eu->harbor[harbor_index];
  int room_total = 0;
  for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
    const int amt = ship->hold_goods_amount[i];
    if (amt > 0 && amt < 255) {
      if (ship->hold_goods_type[i] == cargo_type) {
        room_total += 100 - amt;
      }
    } else {
      room_total += 100;
    }
  }
  if (room_total <= 0) {
    europe_set_status(eu, "No empty hold.");
    return 0;
  }
  int can_afford = eu->gold / ask;
  if (can_afford <= 0) {
    europe_set_status(eu, "Need gold.");
    return 0;
  }
  int buy = amount;
  if (buy > 100) {
    buy = 100;
  }
  if (buy > room_total) {
    buy = room_total;
  }
  if (buy > can_afford) {
    buy = can_afford;
  }
  if (buy <= 0) {
    return 0;
  }

  int remaining = buy;
  for (int i = 0; i < EUROPE_SHIP_CARGO_MAX && remaining > 0; ++i) {
    const int amt = ship->hold_goods_amount[i];
    if (amt <= 0 || amt >= 255 || ship->hold_goods_type[i] != cargo_type) {
      continue;
    }
    const int room = 100 - amt;
    if (room <= 0) {
      continue;
    }
    const int add = remaining < room ? remaining : room;
    ship->hold_goods_amount[i] += add;
    remaining -= add;
  }
  for (int i = 0; i < EUROPE_SHIP_CARGO_MAX && remaining > 0; ++i) {
    const int amt = ship->hold_goods_amount[i];
    if (amt > 0 && amt < 255) {
      continue;
    }
    const int add = remaining < 100 ? remaining : 100;
    ship->hold_goods_type[i] = cargo_type;
    ship->hold_goods_amount[i] = add;
    remaining -= add;
  }
  const int bought = buy - remaining;
  eu->gold -= bought * ask;
  if (bought > 0) {
    europe_apply_volume_price(eu, cargo_type, bought, 1);
  }
  snprintf(
    eu->status,
    sizeof(eu->status),
    "Bought %d %s (-%d$).",
    bought,
    eu->cargo[cargo_type].name,
    bought * ask
  );
  diag_info(
    "EUROPE bought %d %s onto %s: ask=%d cost=%d gold=%d",
    bought, eu->cargo[cargo_type].name, ship->name[0] ? ship->name : "ship",
    ask, bought * ask, eu->gold
  );
  return bought;
}

int europe_best_sell_hold(const EuropeScreen* eu, int harbor_index) {
  if (!eu || harbor_index < 0 || harbor_index >= eu->harbor_ships) {
    return -1;
  }
  const EuropeHarborShip* ship = &eu->harbor[harbor_index];
  int best = -1;
  int best_v = 0;
  int best_amt = 0;
  static const int k_value[COLONIZE_CARGO_COUNT] = {
    1, 5, 4, 3, 5, 0, 4, 20, 0, 8, 8, 7, 7, 2, 0, 0
  };
  for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
    const int amt = ship->hold_goods_amount[i];
    const int ctype = ship->hold_goods_type[i];
    if (amt <= 0 || amt >= 255) {
      continue;
    }
    const int v =
      (ctype >= 0 && ctype < COLONIZE_CARGO_COUNT) ? k_value[ctype] : 0;
    if (v <= 0) {
      continue;
    }
    if (v > best_v || (v == best_v && amt > best_amt)) {
      best_v = v;
      best_amt = amt;
      best = i;
    }
  }
  return best;
}

static bool europe_in_rect(int mx, int my, int x, int y, int w, int h) {
  return mx >= x && my >= y && mx < x + w && my < y + h;
}

static int europe_ship_icon_sprite(const ColonizeUnitPool* units, const EuropeHarborShip* ship) {
  if (!units || !ship) {
    return -1;
  }
  int ti = ship->type_index;
  if (ti < 0) {
    ti = units_find_type(units, ship->name);
  }
  const ColonizeUnitType* ut = units_type(units, ti);
  return ut ? ut->icon_sprite : -1;
}

int europe_transit_ship_at(
  const EuropeHarborShip* ships,
  int count,
  const ColonizeUnitPool* units,
  const ColonizeSpriteSheet* unit_icons,
  int box_x,
  int box_y,
  int box_w,
  int box_h,
  int transit_line_h,
  int mx,
  int my
) {
  if (!ships || count <= 0 || !units || !unit_icons || !europe_in_rect(mx, my, box_x, box_y, box_w, box_h)) {
    return -1;
  }
  const int line_h = transit_line_h > 0 ? transit_line_h : 8;
  const int header_h = EUROPE_TRANSIT_HEADER_LINES * line_h;
  const int ship_y0 = box_y + 2 + header_h + 10;
  const int ship_area_h = box_y + box_h - ship_y0 - 1;
  if (ship_area_h < 8) {
    return -1;
  }

  int x = box_x + 3;
  int y = ship_y0;
  int row_h = 0;
  for (int i = 0; i < count; ++i) {
    const int sprite = europe_ship_icon_sprite(units, &ships[i]);
    if (sprite < 0 || sprite >= unit_icons->sprite_count) {
      continue;
    }
    const ColonizeSprite* sp = &unit_icons->sprites[sprite];
    const int sw = sp->width > 0 ? sp->width : 14;
    const int sh = sp->height > 0 ? sp->height : 16;
    if (x + sw > box_x + box_w - 2) {
      x = box_x + 3;
      y += row_h + 1;
      row_h = 0;
      if (y + sh > box_y + box_h - 1) {
        break;
      }
    }
    if (sh > row_h) {
      row_h = sh;
    }
    if (mx >= x && my >= y && mx < x + sw && my < y + sh) {
      return i;
    }
    x += sw + 2;
  }
  /* Clicked the box but not an icon — use first ship. */
  return count > 0 ? 0 : -1;
}

EuropeHitResult europe_hit_test(const EuropeScreen* eu, int mx, int my) {
  return europe_hit_test_ex(eu, mx, my, NULL, NULL, 8);
}

EuropeHitResult europe_hit_test_ex(
  const EuropeScreen* eu,
  int mx,
  int my,
  const ColonizeUnitPool* units,
  const ColonizeSpriteSheet* unit_icons,
  int transit_line_h
) {
  EuropeHitResult hit;
  hit.kind = EUROPE_HIT_NONE;
  hit.index = -1;
  if (!eu) {
    return hit;
  }

  if (mx >= EUROPE_EXIT_X && mx < EUROPE_SCREEN_W && my >= EUROPE_EXIT_Y && my < EUROPE_SCREEN_H) {
    hit.kind = EUROPE_HIT_EXIT;
    return hit;
  }

  if (europe_in_rect(mx, my, EUROPE_BTN_X, EUROPE_BTN_Y, EUROPE_BTN_W, EUROPE_BTN_H)) {
    hit.kind = EUROPE_HIT_BTN_RECRUIT;
    return hit;
  }
  if (europe_in_rect(
        mx, my, EUROPE_BTN_X, EUROPE_BTN_Y + EUROPE_BTN_PITCH, EUROPE_BTN_W + 8, EUROPE_BTN_H
      )) {
    hit.kind = EUROPE_HIT_BTN_PURCHASE;
    return hit;
  }
  if (europe_in_rect(
        mx,
        my,
        EUROPE_BTN_X,
        EUROPE_BTN_Y + 2 * EUROPE_BTN_PITCH,
        EUROPE_BTN_W,
        EUROPE_BTN_H
      )) {
    hit.kind = EUROPE_HIT_BTN_TRAIN;
    return hit;
  }

  {
    if (eu->harbor_ships > 0 && my >= EUROPE_HOLD_Y && my < EUROPE_HOLD_Y + EUROPE_HOLD_H &&
        mx >= EUROPE_HOLD_X && mx < EUROPE_HOLD_X + EUROPE_HOLD_MAX * EUROPE_HOLD_PITCH) {
      const int idx = (mx - EUROPE_HOLD_X) / EUROPE_HOLD_PITCH;
      if (idx >= 0 && idx < EUROPE_HOLD_MAX) {
        hit.kind = EUROPE_HIT_HOLD;
        hit.index = idx;
        return hit;
      }
    }
  }

  if (eu->harbor_ships > 0 &&
      europe_in_rect(mx, my, EUROPE_LOADING_X, EUROPE_LOADING_Y, EUROPE_LOADING_W, EUROPE_LOADING_H)) {
    hit.kind = EUROPE_HIT_HARBOR_SHIP;
    if (units && unit_icons) {
      const int si = europe_transit_ship_at(
        eu->harbor,
        eu->harbor_ships,
        units,
        unit_icons,
        EUROPE_LOADING_X,
        EUROPE_LOADING_Y,
        EUROPE_LOADING_W,
        EUROPE_LOADING_H,
        transit_line_h,
        mx,
        my
      );
      hit.index = si >= 0 ? si : (eu->selected_harbor >= 0 ? eu->selected_harbor : 0);
    } else {
      hit.index = eu->selected_harbor >= 0 ? eu->selected_harbor : 0;
    }
    return hit;
  }

  /* Transit boxes stay hittable when empty so ships can be dropped into them. */
  if (europe_in_rect(mx, my, EUROPE_EXPECTED_X, EUROPE_EXPECTED_Y, EUROPE_EXPECTED_W, EUROPE_EXPECTED_H)) {
    hit.kind = EUROPE_HIT_EXPECTED;
    if (eu->expected_ships > 0 && units && unit_icons) {
      hit.index = europe_transit_ship_at(
        eu->expected,
        eu->expected_ships,
        units,
        unit_icons,
        EUROPE_EXPECTED_X,
        EUROPE_EXPECTED_Y,
        EUROPE_EXPECTED_W,
        EUROPE_EXPECTED_H,
        transit_line_h,
        mx,
        my
      );
    } else {
      hit.index = eu->expected_ships > 0 ? 0 : -1;
    }
    return hit;
  }
  if (europe_in_rect(mx, my, EUROPE_BOUND_X, EUROPE_BOUND_Y, EUROPE_BOUND_W, EUROPE_BOUND_H)) {
    hit.kind = EUROPE_HIT_BOUND;
    if (eu->bound_ships > 0 && units && unit_icons) {
      hit.index = europe_transit_ship_at(
        eu->bound,
        eu->bound_ships,
        units,
        unit_icons,
        EUROPE_BOUND_X,
        EUROPE_BOUND_Y,
        EUROPE_BOUND_W,
        EUROPE_BOUND_H,
        transit_line_h,
        mx,
        my
      );
    } else {
      hit.index = eu->bound_ships > 0 ? 0 : -1;
    }
    return hit;
  }

  for (int i = 0; i < eu->dock_count; ++i) {
    int dx = 0;
    int dy = 0;
    if (!europe_dock_slot_pos(i, &dx, &dy)) {
      break;
    }
    if (europe_in_rect(mx, my, dx, dy, EUROPE_DOCK_UNIT_W, EUROPE_DOCK_UNIT_H)) {
      hit.kind = EUROPE_HIT_DOCK;
      hit.index = i;
      return hit;
    }
  }

  if (my >= EUROPE_MARKET_Y && my < EUROPE_MARKET_Y + EUROPE_MARKET_H && mx >= EUROPE_MARKET_X) {
    const int idx = (mx - EUROPE_MARKET_X) / EUROPE_MARKET_PITCH;
    if (idx >= 0 && idx < eu->cargo_count && idx < EUROPE_CARGO_MAX &&
        mx < EUROPE_MARKET_X + idx * EUROPE_MARKET_PITCH + EUROPE_MARKET_CELL) {
      hit.kind = EUROPE_HIT_MARKET;
      hit.index = idx;
      return hit;
    }
  }

  return hit;
}

void europe_menu_open(EuropeScreen* eu, EuropeMenu menu) {
  if (!eu) {
    return;
  }
  eu->menu = menu;
  eu->menu_selection = 0;
  if (menu == EUROPE_MENU_RECRUIT) {
    europe_pool_ensure_filled(eu);
    snprintf(
      eu->status,
      sizeof(eu->status),
      "Recruit (passage %d$). Esc cancels.",
      eu->recruit_passage
    );
  } else if (menu == EUROPE_MENU_TRAIN) {
    europe_set_status(eu, "Royal University. Esc cancels.");
  } else if (menu == EUROPE_MENU_PURCHASE) {
    europe_set_status(eu, "Purchase. Esc cancels.");
  } else if (menu == EUROPE_MENU_DOCK) {
    europe_set_status(eu, "Dock orders. Esc cancels.");
  }
}

void europe_menu_close(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  eu->menu = EUROPE_MENU_NONE;
  eu->menu_selection = 0;
  eu->menu_dock_index = -1;
}

/*
 * Turn the highlighted row of the open dock menu into its @ARMOPTIONS row id
 * and apply it. Row ids are carried per-row because DOS omits the rows it
 * disabled, so the visible index is not the id.
 */
bool europe_dock_menu_apply_selection(
  EuropeScreen* eu,
  ColonizeUnitPool* units,
  int nation_id
) {
  if (!eu || eu->menu != EUROPE_MENU_DOCK) {
    return false;
  }
  const int sel = eu->menu_selection;
  if (sel < 0 || sel >= eu->dock_menu_count) {
    return false;
  }
  if (eu->dock_menu_greyed[sel]) {
    europe_set_status(eu, "The treasury cannot afford that.");
    return false;
  }
  return europe_apply_dock_menu_row(
    eu, units, nation_id, eu->menu_dock_index, (int)eu->dock_menu_row[sel]
  );
}

bool europe_menu_confirm(EuropeScreen* eu) {
  if (!eu || eu->menu == EUROPE_MENU_NONE) {
    return false;
  }
  const EuropeMenu m = eu->menu;
  const int sel = eu->menu_selection;
  if (m == EUROPE_MENU_DOCK) {
    /* Units-less path (no pool to keep the mirror unit in step); game_loop
     * calls europe_dock_menu_apply_selection with the pool so equipment,
     * type and dock sprite follow the row that was picked. */
    const bool ok = europe_dock_menu_apply_selection(eu, NULL, -1);
    europe_menu_close(eu);
    return ok;
  }
  if (sel == 0) {
    europe_menu_close(eu);
    europe_set_status(eu, "Cancelled.");
    return true;
  }
  if (m == EUROPE_MENU_RECRUIT) {
    const int pool_i = sel - 1;
    const bool ok = europe_recruit_from_pool(eu, pool_i);
    europe_menu_close(eu);
    return ok;
  }
  if (m == EUROPE_MENU_TRAIN) {
    const bool ok = europe_train(eu, sel - 1);
    europe_menu_close(eu);
    return ok;
  }
  if (m == EUROPE_MENU_PURCHASE) {
    const bool ok = europe_purchase(eu, sel - 1);
    europe_menu_close(eu);
    return ok;
  }
  europe_menu_close(eu);
  return false;
}

void europe_cheat_add_gold(EuropeScreen* eu, int amount) {
  if (!eu) {
    return;
  }
  eu->gold += amount;
  if (eu->gold < 0) {
    eu->gold = 0;
  }
  snprintf(eu->status, sizeof(eu->status), "Treasury now %d$.", eu->gold);
}

void europe_cheat_adjust_tax(EuropeScreen* eu, int delta) {
  if (!eu) {
    return;
  }
  eu->tax_percent += delta;
  if (eu->tax_percent < 0) {
    eu->tax_percent = 0;
  }
  if (eu->tax_percent > 100) {
    eu->tax_percent = 100;
  }
  snprintf(eu->status, sizeof(eu->status), "Tax rate %d%%.", eu->tax_percent);
}
