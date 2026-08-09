#include "core/europe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/ss.h"
#include "core/strutil.h"
#include "core/units.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

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
  eu->dock_count++;
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
         * PARK value source: intended COL1 Treasure cargo_hold[0..1] LE16 gold
         * (ColonizeUnit has no treasure_gold; game_loop→europe_enqueue_expected
         * does not fill cargo_treasure_gold yet). Do not invent a rate/value.
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
  {"Master Distiller", 9, 2},
  {"Master Tobacconists", 10, 2},
  {"Master Weavers", 11, 2},
  {"Master Fur Traders", 12, 2},
  {"Expert Silver Miners", 7, 2},
};

void europe_refill_pool_slot(EuropeScreen* eu, int slot, unsigned* rng_state) {
  if (!eu || slot < 0 || slot >= EUROPE_POOL_SIZE) {
    return;
  }
  unsigned local = 1u + (unsigned)(eu->gold + eu->recruit_passage + slot * 17);
  unsigned* st = rng_state ? rng_state : &local;
  int total = 0;
  for (size_t i = 0; i < sizeof(k_pool_cands) / sizeof(k_pool_cands[0]); ++i) {
    /* Brewster (wiki): no criminals/servants on docks / recruit pool. */
    if (eu->brewster_no_criminals &&
        (k_pool_cands[i].profession == 26 || k_pool_cands[i].profession == 25)) {
      continue;
    }
    total += k_pool_cands[i].weight;
  }
  if (total <= 0) {
    return;
  }
  int pick = (int)(europe_rng_next(st) % (unsigned)total);
  for (size_t i = 0; i < sizeof(k_pool_cands) / sizeof(k_pool_cands[0]); ++i) {
    if (eu->brewster_no_criminals &&
        (k_pool_cands[i].profession == 26 || k_pool_cands[i].profession == 25)) {
      continue;
    }
    pick -= k_pool_cands[i].weight;
    if (pick < 0) {
      EuropePoolSlot* p = &eu->pool[slot];
      snprintf(p->name, sizeof(p->name), "%s", k_pool_cands[i].name);
      p->profession = k_pool_cands[i].profession;
      p->filled = true;
      return;
    }
  }
}

static void europe_init_pool(EuropeScreen* eu) {
  unsigned rng = 42u;
  for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
    europe_refill_pool_slot(eu, i, &rng);
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
      (void)low;
      (void)high;
      (void)rise;
      (void)fall;
      (void)attrition;
      (void)volatility;

      EuropeCargoQuote* q = &eu->cargo[eu->cargo_count++];
      str_copy_trunc(q->name, sizeof(q->name), line);
      q->bid = start_lo;
      if (q->bid < 0) {
        q->bid = 0;
      }
      q->ask = q->bid + burden + 1;
    }
  }

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
  }

  const ColonizeMsgSection* home = assets_msg_find(names, "HOMEPORT");
  const ColonizeMsgSection* cname = assets_msg_find(names, "COLONYNAME");
  (void)home;
  (void)cname;

  europe_init_purchase_table(eu);
  return eu->cargo_count > 0;
}

static void europe_apply_nation_names(EuropeScreen* eu, int nation, const ColonizeMsgCatalog* names) {
  static const char* k_ports[4] = {"London", "La Rochelle", "Seville", "Amsterdam"};
  static const char* k_nations[4] = {"England", "France", "Spain", "Netherlands"};
  static const char* k_regions[4] = {
    "New England", "New France", "New Spain", "New Netherlands"
  };
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

int europe_voyage_turns(bool exit_east, int ship_movement) {
  int t = exit_east ? EUROPE_VOYAGE_EAST_TURNS : EUROPE_VOYAGE_WEST_TURNS;
  if (ship_movement >= 6) {
    t -= 1;
  }
  if (t < 1) {
    t = 1;
  }
  if (t > 4) {
    t = 4;
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
  europe_apply_nation_names(eu, nation, NULL);
  eu->gold = 1000;
  eu->tax_percent = 0;
  eu->current_crosses = 0;
  eu->needed_crosses = 8;
  eu->crosses_immigrant_seen = false;
  eu->crosses_pending_needed_bump = false;
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
  eu->recruit_passage = EUROPE_RECRUIT_PASSAGE_START;
  europe_init_pool(eu);
  europe_init_purchase_table(eu);
  eu->menu = EUROPE_MENU_NONE;
  eu->menu_selection = 0;
  eu->menu_dock_index = -1;
  eu->last_exit_valid = false;
  eu->open_on_dock = false;
  /* Two free colonists already waiting — matches screenshot dock feel. */
  static const char* starters[] = {"Free Colonists", "Indentured Servants"};
  static const int starter_jobs[] = {19, 25};
  for (int i = 0; i < 2 && i < EUROPE_DOCK_MAX; ++i) {
    snprintf(eu->dock[i].name, sizeof(eu->dock[i].name), "%s", starters[i]);
    eu->dock[i].profession = starter_jobs[i];
    eu->dock[i].present = true;
    eu->dock[i].sentry = true;
    eu->dock_count = i + 1;
  }
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
  europe_apply_nation_names(eu, 0, &names);
  /* Re-init pool/purchase after reset; train table already from load_tables. */
  {
    int train_count = eu->train_count;
    EuropeTrainOption train_copy[EUROPE_TRAIN_MAX];
    memcpy(train_copy, eu->train, sizeof(train_copy));
    europe_reset_campaign(eu);
    eu->train_count = train_count;
    memcpy(eu->train, train_copy, sizeof(train_copy));
    europe_apply_nation_names(eu, 0, &names);
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

static void europe_bump_passage(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  eu->recruit_passage += EUROPE_RECRUIT_PASSAGE_STEP;
  if (eu->recruit_passage > 20000) {
    eu->recruit_passage = 20000;
  }
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
  snprintf(
    eu->status,
    sizeof(eu->status),
    "Recruited %s (-%d$).",
    slot->name,
    eu->recruit_passage
  );
  europe_bump_passage(eu);
  europe_refill_pool_slot(eu, pool_index, NULL);
  return true;
}

bool europe_immigrant_from_pool(EuropeScreen* eu) {
  if (!eu || eu->dock_count >= EUROPE_DOCK_MAX) {
    return false;
  }
  int slot = -1;
  for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
    if (eu->pool[i].filled) {
      slot = i;
      break;
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
  europe_bump_passage(eu);
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
  snprintf(eu->status, sizeof(eu->status), "Trained %s (-%d$).", t->expert_name, t->cost);
  return true;
}

bool europe_purchase(EuropeScreen* eu, int purchase_index) {
  if (!eu || purchase_index < 0 || purchase_index >= eu->purchase_count) {
    return false;
  }
  const EuropePurchaseOption* p = &eu->purchase[purchase_index];
  if (eu->gold < p->gold) {
    snprintf(eu->status, sizeof(eu->status), "Need %d$ for %s.", p->gold, p->name);
    return false;
  }
  if (p->is_ship) {
    if (eu->harbor_ships >= EUROPE_HARBOR_MAX) {
      europe_set_status(eu, "Harbor is full.");
      return false;
    }
    eu->gold -= p->gold;
    EuropeHarborShip* slot = &eu->harbor[eu->harbor_ships++];
    europe_clear_ship(slot);
    slot->type_index = -1; /* resolved by name in game_loop / caller */
    snprintf(slot->name, sizeof(slot->name), "%s", p->name);
    europe_refresh_harbor_selection(eu);
    snprintf(eu->status, sizeof(eu->status), "Purchased %s (-%d$).", p->name, p->gold);
    return true;
  }
  if (eu->dock_count >= EUROPE_DOCK_MAX) {
    europe_set_status(eu, "Docks are full.");
    return false;
  }
  eu->gold -= p->gold;
  EuropeDockImmigrant* slot = &eu->dock[eu->dock_count++];
  memset(slot, 0, sizeof(*slot));
  snprintf(slot->name, sizeof(slot->name), "%s", p->name);
  slot->profession = -1;
  slot->present = true;
  slot->sentry = true;
  snprintf(eu->status, sizeof(eu->status), "Purchased %s (-%d$).", p->name, p->gold);
  return true;
}

bool europe_recruit(EuropeScreen* eu) {
  if (!eu) {
    return false;
  }
  europe_menu_open(eu, EUROPE_MENU_RECRUIT);
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
  int ship_movement
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
  slot->turns_left = europe_voyage_turns(exit_east, ship_movement);
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
  const ColonizeUnitPool* units,
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
      const int ti = units_find_type(units, eu->dock[di].name);
      type_tag = ti >= 0 ? ti : 0;
    }
    ship->cargo_types[ship->cargo_count] = type_tag;
    ship->cargo_professions[ship->cargo_count] = eu->dock[di].profession;
    ship->cargo_count++;
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
  int ship_movement,
  const ColonizeUnitPool* units
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
  europe_board_sentry_dockers(eu, &ship, units, cargo_cap);
  bool exit_east = eu->last_exit_valid ? eu->last_exit_east : true;
  ship.exit_east = exit_east;
  if (eu->last_exit_valid) {
    ship.exit_x = eu->last_exit_x;
    ship.exit_y = eu->last_exit_y;
  }
  ship.turns_left = europe_voyage_turns(exit_east, ship_movement);
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
  for (int i = 0; i < eu->expected_ships; ++i) {
    if (eu->expected[i].turns_left > 0) {
      eu->expected[i].turns_left--;
    }
  }
  for (int i = 0; i < eu->bound_ships; ++i) {
    if (eu->bound[i].turns_left > 0) {
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
    europe_refresh_harbor_selection(eu);
    snprintf(eu->status, sizeof(eu->status), "%s has docked in %s.", ship.name, eu->port_city);
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
  /* GAME.TXT @LOOTCASH / @KINGGALLEON3: Crown share = tax rate. */
  const int credited = (treasure_value * (100 - tax)) / 100;
  eu->gold += credited;
  snprintf(
    eu->status,
    sizeof(eu->status),
    "Treasure cash-in +%d$ (Crown %d%%).",
    credited,
    tax
  );
  return credited;
}

int europe_sell_proceeds(const EuropeScreen* eu, int cargo_type, int amount) {
  if (!eu || amount <= 0 || cargo_type < 0 || cargo_type >= eu->cargo_count) {
    return 0;
  }
  const int bid = eu->cargo[cargo_type].bid;
  if (bid <= 0) {
    return 0;
  }
  int tax = eu->tax_percent;
  if (tax < 0) {
    tax = 0;
  }
  if (tax > 100) {
    tax = 100;
  }
  return (bid * amount * (100 - tax)) / 100;
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
  const int gained = europe_sell_proceeds(eu, ctype, amt);
  eu->gold += gained;
  ship->hold_goods_amount[hold_index] = 0;
  ship->hold_goods_type[hold_index] = 0;
  const char* cname =
    (ctype >= 0 && ctype < eu->cargo_count) ? eu->cargo[ctype].name : "cargo";
  snprintf(eu->status, sizeof(eu->status), "Sold %d %s for %d$.", amt, cname, gained);
  return gained;
}

/*
 * FUN_364b_0636: Custom House may auto-sell this cargo type.
 * Deny Food(0), Lumber(5), Horses(8), Tools(0xe), Muskets(0xf).
 * Ore(6) extra DOS deny path not mapped — allow Ore (no invent).
 */
static int europe_custom_house_cargo_eligible(int cargo_type) {
  if (cargo_type == COLONIZE_CARGO_FOOD || cargo_type == COLONIZE_CARGO_LUMBER ||
      cargo_type == COLONIZE_CARGO_HORSES || cargo_type == COLONIZE_CARGO_TOOLS ||
      cargo_type == COLONIZE_CARGO_MUSKETS) {
    return 0;
  }
  return cargo_type >= 0 && cargo_type < COLONIZE_CARGO_COUNT;
}

/* FUN_364b_0636 denylist — shared with AI peace Europe export sail. */
int europe_cargo_export_eligible(int cargo_type) {
  return europe_custom_house_cargo_eligible(cargo_type);
}

static int europe_custom_house_bit_enabled(uint16_t bits, int cargo_type) {
  /* bits==0 → all eligible (no per-cargo UI yet). */
  if (bits == 0) {
    return 1;
  }
  return (bits >> cargo_type) & 1u;
}

int europe_custom_house_autosell(
  EuropeScreen* eu,
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  ColonizeCol1Save* col1,
  int human_nation
) {
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
  const int woi = col1 && col1->head.unknown46[0] != 0;
  int tax = 0;
  if (!woi) {
    if (nation == human_nation) {
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

  int total = 0;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    if (!europe_custom_house_cargo_eligible(c)) {
      continue;
    }
    if (!europe_custom_house_bit_enabled(colony->custom_house_bits, c)) {
      continue;
    }
    /* FUN_364b_0688: if (99 < stock) sell stock - 0x32 (leave 50). */
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
    const int bid = eu->cargo[c].bid;
    if (bid <= 0) {
      continue;
    }
    const int gained = (bid * amount * (100 - tax)) / 100;
    colony->stock[c] = 50;
    total += gained;
    if (col1 && nation >= 0 && nation < (int)COLONIZE_COL1_NATION_COUNT) {
      col1->nation[nation].gold += (uint32_t)gained;
    }
    if (nation == human_nation) {
      eu->gold += gained;
    }
  }
  if (total > 0) {
    snprintf(eu->status, sizeof(eu->status), "Custom House sold goods for %d$.", total);
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
  const int gained = europe_sell_proceeds(eu, ctype, amt);
  eu->gold += gained;
  u->hold_goods_amount[hold_index] = 0;
  u->hold_goods_type[hold_index] = 0;
  const char* cname =
    (ctype >= 0 && ctype < eu->cargo_count) ? eu->cargo[ctype].name : "cargo";
  snprintf(eu->status, sizeof(eu->status), "Sold %d %s for %d$.", amt, cname, gained);
  return gained;
}

int europe_buy_cargo(EuropeScreen* eu, int harbor_index, int cargo_type, int amount) {
  if (!eu || harbor_index < 0 || harbor_index >= eu->harbor_ships) {
    return 0;
  }
  if (cargo_type < 0 || cargo_type >= eu->cargo_count || amount <= 0) {
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
  snprintf(
    eu->status,
    sizeof(eu->status),
    "Bought %d %s (-%d$).",
    bought,
    eu->cargo[cargo_type].name,
    bought * ask
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
    int open_holds = 0;
    if (eu->selected_harbor >= 0 && eu->selected_harbor < eu->harbor_ships) {
      open_holds = EUROPE_HOLD_MAX;
    }
    if (open_holds > 0 && my >= EUROPE_HOLD_Y && my < EUROPE_HOLD_Y + EUROPE_HOLD_H &&
        mx >= EUROPE_HOLD_X && mx < EUROPE_HOLD_X + EUROPE_HOLD_MAX * EUROPE_HOLD_PITCH) {
      const int idx = (mx - EUROPE_HOLD_X) / EUROPE_HOLD_PITCH;
      if (idx >= 0 && idx < open_holds &&
          mx < EUROPE_HOLD_X + idx * EUROPE_HOLD_PITCH + EUROPE_HOLD_W) {
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

  if (eu->dock_count > 0 && my >= EUROPE_DOCK_Y && my < EUROPE_DOCK_Y + EUROPE_DOCK_UNIT_H &&
      mx >= EUROPE_DOCK_X) {
    const int idx = (mx - EUROPE_DOCK_X) / EUROPE_DOCK_PITCH;
    if (idx >= 0 && idx < eu->dock_count &&
        mx < EUROPE_DOCK_X + idx * EUROPE_DOCK_PITCH + EUROPE_DOCK_PITCH) {
      hit.kind = EUROPE_HIT_DOCK;
      hit.index = idx;
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

bool europe_menu_confirm(EuropeScreen* eu) {
  if (!eu || eu->menu == EUROPE_MENU_NONE) {
    return false;
  }
  const EuropeMenu m = eu->menu;
  const int sel = eu->menu_selection;
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
  if (m == EUROPE_MENU_DOCK && eu->menu_dock_index >= 0 &&
      eu->menu_dock_index < eu->dock_count) {
    EuropeDockImmigrant* d = &eu->dock[eu->menu_dock_index];
    if (sel == 1) {
      d->sentry = false;
      europe_set_status(eu, "Will not board next ship.");
    } else if (sel == 2) {
      d->sentry = true;
      europe_set_status(eu, "Will board next ship.");
    } else if (sel == 3 && eu->menu_dock_index > 0) {
      EuropeDockImmigrant tmp = eu->dock[0];
      eu->dock[0] = *d;
      eu->dock[eu->menu_dock_index] = tmp;
      europe_set_status(eu, "Moved to front of dock.");
    }
    europe_menu_close(eu);
    return true;
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
