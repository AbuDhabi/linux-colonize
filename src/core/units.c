#include "core/units.h"

/*
 * Unit pool, type loading, spawning, treasure trains, ship/drydock ticks.
 *
 * Sections:
 *  - Unit pool slots, type loading & kind/type predicates (units_slot .. units_equip_role_type_name)
 *  - Spawning, treasure trains, drydock/ship ticks, Fountain/Brewster/King-galleon popups (units_spawn .. units_king_galleon_apply_popup)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/col1_save.h"
#include "core/combat_analysis.h"
#include "core/combat_strength.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/sound.h"
#include "core/strutil.h"
#include "core/unit_chrome.h"
#include "core/village_trade_intel.h"
#include "core/woodcut.h"
#include "platform/diagnostics.h"
#include "core/units_internal.h"

/* ===================== Unit pool slots, type loading & kind/type predicates (units_slot .. units_equip_role_type_name) ===================== */


ColonizeUnit* units_slot(ColonizeUnitPool* pool) {
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    if (!pool->units[i].active) {
      return &pool->units[i];
    }
  }
  return NULL;
}

/*
 * NAMES.TXT @NATIONALITY adjectives ("English"/"French"/"Spanish"/"Dutch"),
 * cached at type-load time: DOS's nation_name_ptr (FUN_15b3_01e0, reached as
 * func_0x00018b94 from FUN_5fef_0352) reads the DS:-0x72f6 adjective table
 * for every combat / unit substitution, never the player's @COLONYNAME
 * ("New Spain"). Copied rather than aliased so a caller's catalog going out
 * of scope cannot dangle here. bugs.md: "New Spain Privateer".
 */
char g_units_nationality[4][24];

/*
 * NAMES.TXT @HOMEPORT ("London"/"La Rochelle"/"Seville"/"Amsterdam") — DOS's
 * DS:-0x7c74 table, the @SHIPDAMAGE %STRING2 when the loser's nation owns no
 * Drydock colony (FUN_5fef_0352: the ship goes to Europe, not to some other
 * colony).
 */
char g_units_homeport[4][24];

/* Bounded copy: NAMES rows are COLONIZE_MSG_LINE_LEN wide, these cells 24. */
static void units_copy_row(char* dst, size_t cap, const char* src) {
  size_t n = 0;
  while (src && src[n] && n + 1 < cap) {
    dst[n] = src[n];
    n++;
  }
  dst[n] = '\0';
}

static void units_cache_names_rows(
  const ColonizeMsgCatalog* names, const char* section, char out[4][24],
  const char* const fallback[4]
) {
  for (int i = 0; i < 4; ++i) {
    units_copy_row(out[i], 24, fallback[i]);
  }
  const ColonizeMsgSection* sec = names ? assets_msg_find(names, section) : NULL;
  if (!sec) {
    return;
  }
  int row = 0;
  for (int i = 0; i < sec->line_count && row < 4; ++i) {
    char line[COLONIZE_MSG_LINE_LEN];
    snprintf(line, sizeof(line), "%s", sec->lines[i]);
    char* semi = strchr(line, ';');
    if (semi) {
      *semi = '\0';
    }
    char* comma = strchr(line, ',');
    if (comma) {
      *comma = '\0';
    }
    str_trim(line);
    if (line[0] == '\0') {
      continue;
    }
    units_copy_row(out[row], 24, line);
    row++;
  }
}

/*
 * NAMES.TXT @LEVELS column 1 ("Camp"/"Village"/"City"/"City"/"Capital") —
 * DOS's DS:0x9634 stride-6 table, the @LOOT/@LOOT2 %STRING2 indexed by the
 * razed tribe's tech, or 4 for a capital (FUN_5fef_31ea local -0xc0).
 */
char g_units_levels[5][24];

static void units_cache_nationality(const ColonizeMsgCatalog* names) {
  static const char* const k_euro[4] = {"", "", "", ""};
  static const char* const k_port[4] = {"", "", "", ""};
  units_cache_names_rows(names, "NATIONALITY", g_units_nationality, k_euro);
  units_cache_names_rows(names, "HOMEPORT", g_units_homeport, k_port);
  for (int i = 0; i < 5; ++i) {
    if (!names ||
        !assets_msg_row_field(names, "LEVELS", i, 1, g_units_levels[i], sizeof(g_units_levels[i]))) {
      g_units_levels[i][0] = '\0';
    }
  }
}

/*
 * @UNIT construction rows (indices 11..17), cached so the raw-code decoders
 * below need no pool. Seeded with the shipped NAMES.TXT "cost"/"tools"
 * columns so a headless fixture that never loads a catalog still answers
 * DOS's numbers; units_load_types overwrites them from the real file.
 */
typedef struct UnitsBuildRow {
  char name[32];
  int cost_col;  /* DS:0x5239 */
  int tools_col; /* DS:0x523a */
} UnitsBuildRow;

static UnitsBuildRow g_units_build_rows[COLONIZE_UNIT_BUILD_CODE_COUNT] = {
  {"", 6, 4},    /* UNITS_KIND_ARTILLERY   -> code 42: 192 hammers / 40 tools */
  {"", 1, 0},    /* UNITS_KIND_WAGON       -> code 43: 32 -> clamped to 40 / 0 */
  {"", 4, 4},    /* UNITS_KIND_CARAVEL     -> code 44: 128 / 40 */
  {"", 6, 8},    /* UNITS_KIND_MERCHANTMAN -> code 45: 192 / 80 */
  {"", 10, 10},  /* UNITS_KIND_GALLEON     -> code 46: 320 / 100 */
  {"", 8, 12},   /* UNITS_KIND_PRIVATEER   -> code 47: 256 / 120 */
  {"", 16, 20}   /* UNITS_KIND_FRIGATE     -> code 48: 512 / 200 */
};

int units_build_code_to_index(int raw_code) {
  /* DOS-LITERAL FUN_15eb_32f8 raw 13423-13448. */
  if (raw_code < COLONIZE_UNIT_BUILD_CODE_FIRST) {
    return -1;
  }
  if (raw_code - COLONIZE_UNIT_BUILD_CODE_FIRST >= COLONIZE_UNIT_BUILD_CODE_COUNT) {
    return -1;
  }
  return raw_code - COLONIZE_UNIT_BUILD_CODE_BIAS;
}

bool units_build_project_info(int raw_code, const char** name, int* hammers, int* tools_cost) {
  const int idx = units_build_code_to_index(raw_code);
  if (idx < 0) {
    return false;
  }
  const UnitsBuildRow* row = &g_units_build_rows[idx - COLONIZE_UNIT_INDEX_ARTILLERY];
  if (name) {
    *name = row->name;
  }
  if (hammers) {
    /* DOS-LITERAL FUN_15eb_33aa raw 13489-13497. The two clamps are DOS's
     * own and only the 0x28 arm ever fires (cost*32 is never 40..51). */
    int h = row->cost_col * 0x20;
    if (h < 0x28) {
      h = 0x28;
    } else if (h < 0x34) {
      h = 0x34;
    }
    *hammers = h;
  }
  if (tools_cost) {
    *tools_cost = row->tools_col * 10; /* raw 13504: uVar2 * 10 */
  }
  return true;
}

static void units_cache_build_rows(const ColonizeUnitPool* pool) {
  for (int i = 0; i < COLONIZE_UNIT_BUILD_CODE_COUNT; ++i) {
    const int type_index = COLONIZE_UNIT_INDEX_ARTILLERY + i;
    if (type_index >= pool->type_count) {
      break;
    }
    const ColonizeUnitType* t = &pool->types[type_index];
    str_copy_trunc(g_units_build_rows[i].name, sizeof(g_units_build_rows[i].name), t->name);
    g_units_build_rows[i].cost_col = t->cost;
    g_units_build_rows[i].tools_col = t->tools;
  }
}

bool units_load_types(ColonizeUnitPool* pool, const ColonizeMsgCatalog* names) {
  if (!pool || !names) {
    return false;
  }
  pool->type_count = 0;
  units_cache_nationality(names);

  const ColonizeMsgSection* section = assets_msg_find(names, "UNIT");
  if (!section) {
    diag_warn("NAMES.TXT missing @UNIT section.");
    return false;
  }

  for (int i = 0; i < section->line_count && pool->type_count < COLONIZE_UNIT_TYPES_MAX; ++i) {
    char line[COLONIZE_MSG_LINE_LEN];
    snprintf(line, sizeof(line), "%s", section->lines[i]);
    const char* p = str_split_name_row(line);
    if (!p) {
      continue;
    }
    int icon = 0;
    int movement = 0;
    int attack = 0;
    int defense = 0;
    int cargo = 0;
    int size = 0;
    int cost = 0;
    int tools = 0;
    int guns = 0;
    int hull = 0;
    if (!str_next_int_field(&p, &icon) || !str_next_int_field(&p, &movement) ||
        !str_next_int_field(&p, &attack) || !str_next_int_field(&p, &defense) ||
        !str_next_int_field(&p, &cargo) || !str_next_int_field(&p, &size) ||
        !str_next_int_field(&p, &cost) || !str_next_int_field(&p, &tools) ||
        !str_next_int_field(&p, &guns) || !str_next_int_field(&p, &hull)) {
      continue;
    }
    /*
     * Column 12: the capability bit-string (DOS `0x523d + row*0xe`, loader
     * raw 121132-121134 — FUN_2a1f_0b2e reads it as a bit-string, which is
     * why it cannot go through str_next_int_field: "00111100" is not 111100).
     * MSB-first, leftmost character = bit 7. bugs.md #655.
     */
    unsigned cap_bits = 0u;
    {
      const char* q = p;
      while (*q == ' ' || *q == '\t' || *q == ',') {
        ++q;
      }
      for (int b = 7; b >= 0 && (*q == '0' || *q == '1'); --b, ++q) {
        if (*q == '1') {
          cap_bits |= (1u << b);
        }
      }
    }
    ColonizeUnitType* t = &pool->types[pool->type_count++];
    str_copy_trunc(t->name, sizeof(t->name), line);
    /* The @UNIT row is the type's identity (ColonizeUnitKind). */
    t->kind_plus1 = pool->type_count;
    /* NAMES.TXT @UNIT icon is 1-based (DOS / MAPEDIT style); ICONS.SS blit is 0-based. */
    t->icon_sprite = icon > 0 ? icon - 1 : -1;
    t->movement = movement > 0 ? movement : 1;
    t->attack = attack;
    t->defense = defense;
    t->cargo = cargo;
    t->cost = cost;
    t->tools = tools;
    t->space = size;
    t->guns = guns;
    t->hull = hull;
    t->cap_bits = (uint8_t)cap_bits;
    t->domain = hull > 0 ? COLONIZE_UNIT_DOMAIN_SEA : COLONIZE_UNIT_DOMAIN_LAND;
  }

  diag_info("Loaded %d unit types from NAMES.TXT @UNIT", pool->type_count);
  units_cache_build_rows(pool);
  return pool->type_count > 0;
}

void units_reset(ColonizeUnitPool* pool) {
  if (!pool) {
    return;
  }
  memset(pool->units, 0, sizeof(pool->units));
  pool->unit_count = 0;
  pool->selected_id = -1;
  pool->board_first_slot = -1;
  pool->next_id = 1;
  pool->next_tile_stack_order = 1;
}

int units_kind_type_index(const ColonizeUnitPool* pool, ColonizeUnitKind kind) {
  if (!pool || kind < 0) {
    return -1;
  }
  /* A row stamped by the loader is authoritative (and on a loaded pool it is
   * simply slot == kind). Only a pool nobody stamped — a test fixture built
   * out of names — falls through to the resolver pass. */
  for (int i = 0; i < pool->type_count; ++i) {
    if (pool->types[i].kind_plus1 == (int)kind + 1) {
      return i;
    }
  }
  for (int i = 0; i < pool->type_count; ++i) {
    if (pool->types[i].kind_plus1 == 0 && units_type_kind(&pool->types[i]) == kind) {
      return i;
    }
  }
  return -1;
}

int units_find_type(const ColonizeUnitPool* pool, const char* name) {
  if (!pool || !name) {
    return -1;
  }
  for (int i = 0; i < pool->type_count; ++i) {
    if (strcmp(pool->types[i].name, name) == 0) {
      return i;
    }
  }
  /*
   * NAMES.TXT @UNIT ships plural / abbreviated type names ("Regulars",
   * "Dragoons", "Soldiers", "Scouts", "Cont. Cav.") while the AI asks for
   * the singular ("Regular", "Dragoon", ...). Against the real asset the
   * exact match above never hit for those — the REF land pools silently
   * never spawned (found 2026-08-28 by a headless WoI run). Accept a
   * trailing "s" / "." difference on either side.
   */
  const size_t n = strlen(name);
  for (int i = 0; i < pool->type_count; ++i) {
    const char* t = pool->types[i].name;
    const size_t tn = strlen(t);
    if (tn == n + 1 && strncmp(t, name, n) == 0 && (t[n] == 's' || t[n] == '.')) {
      return i;
    }
    if (n == tn + 1 && strncmp(t, name, tn) == 0 && (name[tn] == 's' || name[tn] == '.')) {
      return i;
    }
  }
  return -1;
}

/*
 * @UNIT name -> DOS type code (units.h ColonizeUnitKind). The table is the
 * NAMES.TXT @UNIT row order (COLONIZE/NAMES.TXT:300-322), the same scale
 * ai_euro.c:11742 ai_euro_20e6_dos_type encodes and the same one every
 * `type < 0xb` / `type == 0x12` range test in the decompile reads out of unit
 * +0x3146. Matched by name, never by pool index: synthetic test fixtures place
 * types at arbitrary slots (combat_strength.c FUN_157e_004a carries the same
 * note).
 *
 * Order is most-specific-first and the accepted spellings are the union of
 * every substring set that existed at a call site before the predicates landed
 * ("Cav" vs "Cavalry" vs "Cav.", the four Man-O-War lists, the three
 * missionary rules). On the stock roster each row still resolves to exactly
 * one @UNIT type, so folding the sets changed no classification:
 *   "Cont. Cav." hits 7 before "Cavalry"/"Cav" can claim it, "Cont. Army"
 *   hits 9 before bare "Army", "Armed Braves" and "Mtd. Braves" hit 20/21
 *   before bare "Brave", and "Mtd. Warriors" hits 22 before either.
 */
/*
 * No name table lives here any more: the port compiles none of the game's
 * wording, so a unit's kind comes from its @UNIT ROW (ColonizeUnitType.
 * kind_plus1, stamped by units_load_types), never from its English name.
 * The old spelling table moved to tests/common/test_name_kinds.c, which
 * registers it through this hook for the fixtures that hand-build a pool
 * out of names. Production never installs a resolver.
 */
static UnitsNameKindResolver g_units_name_kind_resolver = NULL;

void units_set_name_kind_resolver(UnitsNameKindResolver fn) {
  g_units_name_kind_resolver = fn;
}

ColonizeUnitKind units_name_kind(const char* name) {
  if (!name || !name[0] || !g_units_name_kind_resolver) {
    return UNITS_KIND_UNKNOWN;
  }
  return g_units_name_kind_resolver(name);
}

ColonizeUnitKind units_type_kind(const ColonizeUnitType* type) {
  if (!type) {
    return UNITS_KIND_UNKNOWN;
  }
  if (type->kind_plus1 > 0) {
    return (ColonizeUnitKind)(type->kind_plus1 - 1);
  }
  return units_name_kind(type->name);
}

int units_type_dos_code(const ColonizeUnitType* type) {
  return (int)units_type_kind(type);
}

bool units_kind_is_continental(ColonizeUnitKind k) {
  return k == UNITS_KIND_CONT_CAV || k == UNITS_KIND_CONT_ARMY;
}

bool units_kind_is_royal(ColonizeUnitKind k) {
  return k == UNITS_KIND_REGULAR || k == UNITS_KIND_CAVALRY;
}

bool units_kind_is_military(ColonizeUnitKind k) {
  return k == UNITS_KIND_SOLDIER || k == UNITS_KIND_DRAGOON || k == UNITS_KIND_ARTILLERY ||
         units_kind_is_royal(k) || units_kind_is_continental(k);
}

bool units_kind_is_mounted(ColonizeUnitKind k) {
  return k == UNITS_KIND_DRAGOON || k == UNITS_KIND_SCOUT || k == UNITS_KIND_CONT_CAV ||
         k == UNITS_KIND_CAVALRY || k == UNITS_KIND_MTD_BRAVE || k == UNITS_KIND_MTD_WARRIOR;
}

bool units_kind_is_ship(ColonizeUnitKind k) {
  return k >= UNITS_KIND_CARAVEL && k <= UNITS_KIND_MAN_O_WAR;
}

bool units_kind_is_native(ColonizeUnitKind k) {
  return k >= UNITS_KIND_BRAVE && k <= UNITS_KIND_MTD_WARRIOR;
}

#define UNITS_TYPE_PREDICATE(fn, kindval)                  \
  bool fn(const ColonizeUnitType* t) {                     \
    return units_type_kind(t) == (kindval);                \
  }
UNITS_TYPE_PREDICATE(units_type_is_colonist, UNITS_KIND_COLONIST)
UNITS_TYPE_PREDICATE(units_type_is_soldier, UNITS_KIND_SOLDIER)
UNITS_TYPE_PREDICATE(units_type_is_pioneer, UNITS_KIND_PIONEER)
UNITS_TYPE_PREDICATE(units_type_is_missionary, UNITS_KIND_MISSIONARY)
UNITS_TYPE_PREDICATE(units_type_is_dragoon, UNITS_KIND_DRAGOON)
UNITS_TYPE_PREDICATE(units_type_is_scout, UNITS_KIND_SCOUT)
UNITS_TYPE_PREDICATE(units_type_is_regular, UNITS_KIND_REGULAR)
UNITS_TYPE_PREDICATE(units_type_is_cont_cav, UNITS_KIND_CONT_CAV)
UNITS_TYPE_PREDICATE(units_type_is_cavalry, UNITS_KIND_CAVALRY)
UNITS_TYPE_PREDICATE(units_type_is_cont_army, UNITS_KIND_CONT_ARMY)
UNITS_TYPE_PREDICATE(units_type_is_treasure, UNITS_KIND_TREASURE)
UNITS_TYPE_PREDICATE(units_type_is_artillery, UNITS_KIND_ARTILLERY)
UNITS_TYPE_PREDICATE(units_type_is_wagon, UNITS_KIND_WAGON)
UNITS_TYPE_PREDICATE(units_type_is_caravel, UNITS_KIND_CARAVEL)
UNITS_TYPE_PREDICATE(units_type_is_merchantman, UNITS_KIND_MERCHANTMAN)
UNITS_TYPE_PREDICATE(units_type_is_galleon, UNITS_KIND_GALLEON)
UNITS_TYPE_PREDICATE(units_type_is_privateer, UNITS_KIND_PRIVATEER)
UNITS_TYPE_PREDICATE(units_type_is_frigate, UNITS_KIND_FRIGATE)
UNITS_TYPE_PREDICATE(units_type_is_man_o_war, UNITS_KIND_MAN_O_WAR)
#undef UNITS_TYPE_PREDICATE

bool units_type_is_continental(const ColonizeUnitType* t) {
  return units_kind_is_continental(units_type_kind(t));
}

bool units_type_is_royal(const ColonizeUnitType* t) {
  return units_kind_is_royal(units_type_kind(t));
}

bool units_type_is_ship(const ColonizeUnitType* t) {
  return units_kind_is_ship(units_type_kind(t));
}

bool units_type_is_military(const ColonizeUnitType* t) {
  return units_kind_is_military(units_type_kind(t));
}

bool units_type_is_mounted(const ColonizeUnitType* t) {
  return units_kind_is_mounted(units_type_kind(t));
}

bool units_type_is_native(const ColonizeUnitType* t) {
  return units_kind_is_native(units_type_kind(t));
}

/*
 * The three @UNIT rows whose name carries "Brave" (19 Braves, 20 Armed Braves,
 * 21 Mtd. Braves) — deliberately NOT 22 Mtd. Warriors, which the substring
 * test this replaced also let through.
 */
bool units_type_is_brave_named(const ColonizeUnitType* t) {
  const ColonizeUnitKind k = units_type_kind(t);
  return k == UNITS_KIND_BRAVE || k == UNITS_KIND_ARMED_BRAVE || k == UNITS_KIND_MTD_BRAVE;
}

bool units_is_missionary(const ColonizeUnitPool* pool, const ColonizeUnit* u) {
  if (!u) {
    return false;
  }
  if (u->profession == UNITS_JOB_MISSIONARY) {
    return true;
  }
  return units_type_is_missionary(pool ? units_type(pool, u->type_index) : NULL);
}

/*
 * DOS-LITERAL FUN_15eb_0916 raw 9949-9955 — the destination @UNIT type for an
 * equipment change. FUN_15eb_1068 case 1 (raw 11268-11270) writes the new type
 * byte (+0x3146) straight from `*(char*)(job + 0x2f5)`, a flat @JOB->@UNIT
 * table with no tier branch anywhere in the body or in its thunk
 * FUN_281f_0c36 (raw 33600-33604). VICEROY.EXE DS:0x2f5 (file offset
 * 121248+0x2f5), rows 0x13..0x18:
 *   0x13 Colonist->0, 0x14 Pioneer->2, 0x15 Soldier->1,
 *   0x16 Scout->5,    0x17 Dragoon->4,  0x18 Missionary->3
 * bugs.md #648: this used to preserve the Continental/Royal tier, so mounting
 * a Cont. Army gave Cont. Cavalry. The reverse table DS:0x30e (raw 9931/9944,
 * type->job: 7 "Cont. Cav."->0x17, 9 "Cont. Army"->0x15) only files the
 * Continentals under the colonial jobs for the *menu* lookup; it is never read
 * back to re-type, so an equipment change in DOS always lands on the flat
 * colonial type and drops the tier.
 */
const char* units_equip_role_type_name(
  const ColonizeUnitPool* units,
  int cur_type_index,
  int role
) {
  (void)cur_type_index;
  ColonizeUnitKind dest_kind = UNITS_KIND_COLONIST;
  switch (role) {
  case COLONIZE_EJECT_PIONEER:
    dest_kind = UNITS_KIND_PIONEER;
    break;
  case COLONIZE_EJECT_SOLDIER:
    dest_kind = UNITS_KIND_SOLDIER;
    break;
  case COLONIZE_EJECT_SCOUT:
    dest_kind = UNITS_KIND_SCOUT;
    break;
  case COLONIZE_EJECT_DRAGOON:
    dest_kind = UNITS_KIND_DRAGOON;
    break;
  case COLONIZE_EJECT_MISSIONARY:
    dest_kind = UNITS_KIND_MISSIONARY;
    break;
  case COLONIZE_EJECT_COLONIST:
  default:
    dest_kind = UNITS_KIND_COLONIST;
    break;
  }
  const int dest_index = units_kind_type_index(units, dest_kind);
  if (dest_index >= 0) {
    const ColonizeUnitType* dest_type = units_type(units, dest_index);
    if (dest_type && dest_type->name[0]) {
      return dest_type->name;
    }
  }
  return "";
}
/* ===================== Spawning, treasure trains, drydock/ship ticks, Fountain/Brewster/King-galleon popups (units_spawn .. units_king_galleon_apply_popup) ===================== */


int units_spawn(ColonizeUnitPool* pool, int type_index, int x, int y) {
  if (!pool || type_index < 0 || type_index >= pool->type_count) {
    return -1;
  }
  if (units_id_at(pool, x, y) >= 0) {
    return -1;
  }
  return units_spawn_allow_stack(pool, type_index, x, y);
}

/*
 * Field-for-field slot init shared by units_spawn_allow_stack and
 * units_spawn_aboard (UN-2). Everything here is what both DOS spawn paths
 * agree on; the aboard path then overrides the three fields FUN_1427_10be
 * differs on (moves 0, orders 1 = sentry aboard, profession always
 * UNITS_JOB_NONE because a passenger holds no colony job) and stamps
 * aboard_ship_id itself — the helper deliberately does NOT touch
 * aboard_ship_id, because units_set_nation reads it (through units_is_on_map)
 * and the aboard path calls set_nation before the id is known.
 */
void units_slot_reset_defaults(
  ColonizeUnitPool* pool,
  ColonizeUnit* slot,
  const ColonizeUnitType* type,
  int type_index,
  int x,
  int y
) {
  slot->id = pool->next_id++;
  slot->type_index = type_index;
  slot->x = x;
  slot->y = y;
  slot->tile_stack_order = 0;
  slot->moves = units_type_max_mp(type);
  slot->active = true;
  slot->nation_id = 0;
  slot->col1_vis_mask = 0; /* FUN_1427_0992: owner bit via units_set_nation */
  slot->cargo_count = 0;
  memset(slot->cargo_ids, 0, sizeof(slot->cargo_ids));
  memset(slot->hold_goods_type, 0, sizeof(slot->hold_goods_type));
  memset(slot->hold_goods_amount, 0, sizeof(slot->hold_goods_amount));
  slot->orders = UNITS_ORDER_NONE;
  slot->goto_x = 0xFF;
  slot->goto_y = 0xFF;
  slot->follow_unit_id = -1;
  /*
   * FUN_1427_06b4: type cargo>0 → profession 0 (ships/wagons); else 0x1c.
   * Exporting ships as profession 28 makes DOS treat the tile as a land stack
   * and peel the caravel off its transport_chain (sidebar "unloaded").
   *
   * bugs.md #638: DOS raw 7744-7749 has a third arm ahead of the 0x1c
   * default — type 7 (Cont. Cavalry) / type 9 (Cont. Army) spawn with
   * profession 0x15 (Veteran Soldiers), since every Continental is a
   * promoted veteran. The port omitted it and gave them 0x1c.
   */
  const ColonizeUnitKind spawn_kind = units_type_kind(type);
  if (spawn_kind == UNITS_KIND_CONT_CAV || spawn_kind == UNITS_KIND_CONT_ARMY) {
    slot->profession = UNITS_JOB_SOLDIER;
  } else {
    slot->profession = type->cargo > 0 ? 0 : UNITS_JOB_NONE;
  }
  slot->tools = 0;
  slot->muskets = 0;
  slot->horses = 0;
  slot->home_tribe_id = -1;
  slot->col1_counter16 = 0;
  slot->park_nights = 0;
  slot->mp_spent_turn = 0;
  slot->aboard_moves = -1;
  slot->last_dir = 0;
  /* COL1 +0x06 origin: DOS leaves it unbound at create; 0xff is the "no
   * home colony / tribe" sentinel every DOS reader tests as < 0. */
  slot->col1_origin = 0xff;
  slot->col1_flags15 = 0;
  slot->col1_ai_plan = COL1_UNIT_UNKNOWN16_HI_DEFAULT;
  slot->repair_pending = 0;
  slot->ai_landfall_wait = false;
  units_sync_equip_after_type_change(slot, type);
}

int units_spawn_allow_stack(ColonizeUnitPool* pool, int type_index, int x, int y) {
  if (!pool || type_index < 0 || type_index >= pool->type_count) {
    return -1;
  }
  ColonizeUnit* slot = units_slot(pool);
  if (!slot) {
    return -1;
  }
  const ColonizeUnitType* type = &pool->types[type_index];
  units_slot_reset_defaults(pool, slot, type, type_index, x, y);
  slot->aboard_ship_id = -1;
  pool->unit_count++;
  units_tile_stack_arrive(pool, slot->id);
  if (units_is_on_map(slot)) {
    units_occupancy_refresh_tile(pool, slot->x, slot->y, -1);
  }
  diag_info("Spawned unit id=%d type=%s at (%d,%d)", slot->id, type->name, x, y);
  return slot->id;
}

void units_set_nation(ColonizeUnit* unit, int nation_id) {
  if (!unit) {
    return;
  }
  unit->nation_id = nation_id;
  if (nation_id >= 0 && nation_id < 4) {
    /* Euro owner visibility only — clear polluted foreign bits (DOS draw uses hi nibble). */
    unit->col1_vis_mask = (uint8_t)(1u << (nation_id & 3));
  } else {
    /* Natives: not visible through euro fog until observed (EOT / contact). */
    unit->col1_vis_mask = 0;
  }
  /*
   * Spawn sets nation after units_spawn_allow_stack already refreshed occupancy
   * with nation 0 — restamp owner now (FUN_1427_02ca).
   */
  if (units_occupancy_map && units_is_on_map(unit) && unit->x < 200 && unit->y < 200) {
    if (nation_id > 3 && units_occupancy_map->layer2) {
      const int i = unit->y * units_occupancy_map->width + unit->x;
      if (i >= 0 && (size_t)i < units_occupancy_map->tile_count &&
          (units_occupancy_map->layer2[i] & MAP_OCCUPANCY_HAS_CITY) != 0) {
        return;
      }
    }
    map_set_owner_nibble(units_occupancy_map, unit->x, unit->y, nation_id);
  }
}

int units_spawn_treasure_train(
  ColonizeUnitPool* pool,
  int x,
  int y,
  int nation_id,
  int gold
) {
  /*
   * DOS-LITERAL: every DOS spawn site stores a Treasure's value in unit
   * +0x315b (COL1 record +0x17, the `profession` byte) as gold/100 —
   * FUN_65dd_0004 raw 103543 and 103686, FUN_5fef_31ea raw 101489, all
   * `*(undefined1 *)(param_1 * 0x1c + 0x315b) = (undefined1)local_34;`.
   * DS:0x30e[10] = -1, so a Treasure has no real profession slot and the byte
   * is free. Gold amount is caller-supplied — do not invent a conquest rate
   * here (FUN_5fef_31ea / Cortes gate decide that).
   */
  if (!pool || gold < 0) {
    return -1;
  }
  const int ti = units_kind_type_index(pool, UNITS_KIND_TREASURE);
  if (ti < 0) {
    return -1;
  }
  const int id = units_spawn_allow_stack(pool, ti, x, y);
  if (id < 0) {
    return -1;
  }
  ColonizeUnit* u = units_get(pool, id);
  if (!u) {
    return -1;
  }
  units_set_nation(u, nation_id);
  /* DOS stores a byte of hundreds; every DOS amount is a multiple of 100 and
   * well under 25500 (max is Cibola 7000). */
  int hundreds = gold / 100;
  if (hundreds > 255) {
    hundreds = 255;
  }
  u->profession = hundreds;
  return id;
}

/* DOS-LITERAL FUN_3844_0004 raw 58268 (bugs.md #725)
 *
 * The per-nation EOT tick this slot always held is NOT a Treasure-train
 * expiry — no DOS site ages or despawns a type-0x0a Treasure outside a colony
 * (byte tally over every `+0x3146 == '\n'` site and every `+0x315a` writer in
 * viceroy_unpacked.c / viceroy_overlays.c found none). 3844_0004 walks the
 * unit slot and gates on:
 *   FUN_281f_0302(x,y)              tile in bounds
 *   +0x3146 == 0                    @UNIT type 0 (Colonists)
 *   +0x315b == 0x1b                 @JOB 27 (Indian Convert)
 *   FUN_281f_06be(x,y) < 0          no settlement on the tile (layer2 & 2,
 *                                   colony OR village, any owner)
 *   FUN_281f_08bc(unit,2) < 2       FUN_1427_0d38 case 2 stack size < 2,
 *                                   i.e. the Convert stands alone
 * then `+0x315a += 1` and, once that byte exceeds 8, removes the unit
 * (FUN_281f_0808). For a human-controlled owner it also repaints the tile and
 * raises FUN_281f_0652(0xee2, 4) = @DEADCONVERTS; for an AI owner the unit
 * just vanishes. There is no reset arm: the counter only ever climbs.
 */
int units_tick_convert_outside_colony(
  ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int nation_id
) {
  if (!pool || nation_id < 0 || nation_id > 3) {
    return 0;
  }
  int removed = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    /*
     * bugs.md #887: the aboard-ship skip has no literal DOS counterpart and is
     * KEPT as the port's equivalent of the stack test. DOS has no cargo field —
     * a passenger is an ordinary unit linked into the same tile chain as its
     * ship (see col1_bridge_sanitize_units_for_dos, which has to rebuild
     * boarding on import) — and FUN_1427_0d38 case 2 is a bare `INC DI` over
     * that whole chain (1427:0db9, reached through the jump table at
     * 1427:0d78 entry 2; FUN_1427_0002/004a walk the +0x315c/+0x315e links).
     * So the carrying ship IS counted and a Convert aboard a ship always
     * measures stack >= 2, i.e. never ages. The port's stack loop below counts
     * only non-cargo units, so the skip here is what reproduces that.
     */
    if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
      continue;
    }
    if (units_type_kind(units_type(pool, u->type_index)) != UNITS_KIND_COLONIST) {
      continue;
    }
    if (u->profession != UNITS_JOB_CONVERT) {
      continue;
    }
    /* FUN_281f_0302 */
    if (!map || !map_in_bounds(map, u->x, u->y)) {
      continue;
    }
    /* FUN_281f_06be < 0 — layer2 bit 1 carries colonies and villages alike. */
    if (map_tile_has_city(map, u->x, u->y)) {
      continue;
    }
    /* FUN_1427_0d38 case 2: total stack size at the tile. */
    int stack = 0;
    for (int j = 0; j < COLONIZE_UNITS_MAX; ++j) {
      const ColonizeUnit* o = &pool->units[j];
      if (o->active && o->aboard_ship_id < 0 && o->x == u->x && o->y == u->y) {
        stack++;
      }
    }
    if (stack >= 2) {
      continue;
    }
    u->col1_counter16 = (u->col1_counter16 + 1) & 0xff;
    if (u->col1_counter16 <= 8) {
      continue;
    }
    (void)units_despawn(pool, u->id);
    removed++;
  }
  return removed;
}

int units_tick_ship_build_ready(
  ColonizeUnitPool* pool,
  const ColonizeColonyPool* colonies,
  int nation_id,
  int human_nation,
  char* status,
  size_t status_size,
  int* want_europe_open
) {
  if (!pool || nation_id < 0 || nation_id > 3) {
    return 0;
  }
  if (want_europe_open) {
    *want_europe_open = 0;
  }
  int completed = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
      continue;
    }
    /* DOS: type > 0x0c && type < 0x13 && type != 0x0b (redundant). */
    if (u->type_index <= 0x0c || u->type_index >= 0x13) {
      continue;
    }
    if ((u->col1_flags15 & 0x80u) == 0) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(pool, u->type_index);
    /*
     * DOS type*0xe+0x5235 = NAMES @UNIT combat (Linux defense). Loader writes
     * attack→5236 then combat→5235. Cite: viceroy ~121115; nation_eot_ship_spawn.md.
     */
    int threshold = ty && ty->defense > 0 ? ty->defense : 4;
    /*
     * bugs.md #254: combat damage presets col1_counter16 BELOW threshold (DOS
     * repair timer) and this same tick counts it up — construction and
     * repair share the loop like DOS. At/past threshold with bit7 still set
     * (legacy save parked by the old drydock model): leave for
     * units_tick_drydock_repair to clear this EOT.
     */
    if (u->col1_counter16 >= threshold) {
      continue;
    }
    /*
     * bugs.md ("Ships damaged don't get a timeout. They should. It is AT
     * LEAST one turn."): the turn a ship is damaged is not a repair turn.
     * A Caravel victor presets remaining = 2 and a ship parked on a colony
     * tile counts +2, so without this the whole repair landed inside the
     * same end-of-turn the combat happened in and the player never saw a
     * damaged ship. repair_pending 2 = damaged this turn; the first tick
     * downgrades it to 1 and counts nothing.
     */
    if (u->repair_pending > 1) {
      u->repair_pending = 1;
      continue;
    }
    if (u->col1_counter16 < 255) {
      u->col1_counter16++;
    }
    int on_colony = 0;
    if (colonies && colonies_id_at(colonies, u->x, u->y) >= 0) {
      on_colony = 1;
      if (u->col1_counter16 < 255) {
        u->col1_counter16++;
      }
    }
    if (u->col1_counter16 < threshold) {
      continue;
    }
    if (u->repair_pending) {
      continue; /* repair completion (bit7 clear + @REFIT) is the repair tick's */
    }
    u->col1_flags15 = (uint8_t)(u->col1_flags15 & 0x7fu);
    completed++;
    if (nation_id == human_nation && status && status_size > 0) {
      const char* name = (ty && ty->name[0]) ? ty->name : "Ship";
      snprintf(status, status_size, "%s construction complete.", name);
    }
    if (!on_colony && want_europe_open) {
      *want_europe_open = 1;
    }
  }
  return completed;
}

/*
 * Repair completion (bugs.md #254): combat-damage bit7 clears when the repair
 * TIMER (col1_counter16, counted by units_tick_ship_build_ready, double speed
 * in port) reaches the type threshold — DOS's model; no Drydock building
 * required. Also clears legacy-save ships parked at/past threshold by the
 * old instant-drydock model. Construction ships (repair_pending==0 below
 * threshold) stay on units_tick_ship_build_ready.
 */
int units_tick_drydock_repair(
  ColonizeUnitPool* pool,
  const ColonizeColonyPool* colonies,
  int nation_id,
  int human_nation,
  char* status,
  size_t status_size,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!pool || nation_id < 0 || nation_id > 3) {
    return 0;
  }
  int repaired = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
      continue;
    }
    if (!units_is_sea(pool, u->id) || (u->col1_flags15 & 0x80u) == 0) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(pool, u->type_index);
    const int threshold = ty && ty->defense > 0 ? ty->defense : 4;
    /* Timer still running (construction OR repair) — build tick owns bit7. */
    if (u->col1_counter16 < threshold) {
      continue;
    }
    /* repair_pending ships arrive here at threshold; legacy-save damaged
     * ships parked at/past threshold (repair_pending 0) clear here too —
     * fresh construction never reaches this point with bit7 set (the build
     * tick clears it at threshold). */
    const ColonizeColony* col =
      colonies ? colonies_get(colonies, colonies_id_at(colonies, u->x, u->y)) : NULL;
    u->col1_flags15 = (uint8_t)(u->col1_flags15 & 0x7fu);
    u->repair_pending = 0;
    repaired++;
    if (nation_id == human_nation) {
      /* FUN_3844_00f2 / @REFIT: repaired human ship, asm 89523-89534. */
      units_play_event_sound(0x54);
      const char* ship_name = (ty && ty->name[0]) ? ty->name : "Ship";
      const char* col_name = (col && col->name[0]) ? col->name : "port";
      if (status && status_size > 0) {
        snprintf(status, status_size, "%s repaired.", ship_name);
      }
      if (ai_popups) {
        char body[AI_POPUP_BODY_LEN];
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = ship_name;
        tok.string1 = col_name;
        popup_msg_fill(
          messages,
          "REFIT",
          &tok,
          status && status[0] ? status : "Ship repaired.",
          body,
          sizeof(body)
        );
        ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
      }
    }
  }
  return repaired;
}


const char* units_combat_nation_label(const ColonizeCol1Save* col1, int nation_id);
bool units_move_crosses_shore(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int from_x,
  int from_y,
  int to_x,
  int to_y
);

void units_play_event_sound(int id);
void units_set_bgm_pool(int pool);
bool units_combat_is_visible(const ColonizeUnitPool* pool, int a_id, int b_id);
const char* units_home_port_name(const ColonizeCol1Save* col1, int nation_id);

int units_treasure_value_gold(const ColonizeUnit* treasure) {
  if (!treasure) {
    return 0;
  }
  /*
   * DOS-LITERAL FUN_48d3_06ba raw 77986 / FUN_521d_20e6 raw ~78913:
   *   `iVar7 = (uint)*(byte *)(iVar6 + 0x315b) * 100;`
   * The value lives in unit +0x315b (COL1 record +0x17 = `profession`) as
   * gold/100, written by every spawn site; there is no second representation
   * in DOS and the port no longer keeps one (the old LE16 mirror in
   * hold_goods_amount was dropped — it never survived a save round trip and
   * made a 2800-gold save Treasure indistinguishable from an unset one).
   */
  const unsigned b = (unsigned)treasure->profession & 0xffu;
  return (int)b * 100;
}

static int units_king_galleon_treasure_value(const ColonizeUnit* treasure) {
  return units_treasure_value_gold(treasure);
}

int units_king_galleon_share_pct(const ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 0 || nation_id > 3) {
    return 0;
  }
  /* FUN_5fef_1908: local_5a = nation.tax; if !Cortes: max((diff+10)*5, tax*2); cap 0x5a. */
  int pct = (int)col1->nation[nation_id].tax_rate;
  if (!founding_fathers_nation_has(col1, nation_id, FF_HERNAN_CORTES)) {
    const int by_diff = ((int)col1->head.difficulty + 10) * 5;
    const int by_tax = pct * 2;
    pct = by_diff < by_tax ? by_tax : by_diff;
  }
  if (pct > 90) {
    pct = 90;
  }
  if (pct < 0) {
    pct = 0;
  }
  return pct;
}

static void units_king_galleon_credit(
  ColonizeUnitPool* pool,
  EuropeScreen* europe,
  ColonizeCol1Save* col1,
  int nation_id,
  int treasure_id,
  int pct,
  AiPopupState* popups,
  const ColonizeMsgCatalog* game_txt
) {
  ColonizeUnit* treasure = units_get(pool, treasure_id);
  if (!treasure || !treasure->active || treasure->nation_id != nation_id) {
    return;
  }
  const int value = units_king_galleon_treasure_value(treasure);
  const int share = (value * pct) / 100;
  const int net = value - share;
  ColonizeCol1Nation* nat = &col1->nation[nation_id];
  /* Audit G3: the accessor picks the store by nation — the old unconditional
   * `europe->gold = nat->gold` handed the human's purse an AI's treasury. */
  europe_nation_gold_add(europe, col1, nation_id, (long)(net > 0 ? net : 0));
  nat->royal_money += share; /* DOS nation+0x22 += Crown share */
  /* DOS-LITERAL FUN_5fef_1908 raw 100239: the write-only per-nation
   * accumulator at nation +0x26 (`unknown24_pad`, int32 LE) takes the same
   * NET the purse got — not the gross and not the fee. Same bump as
   * FUN_48d3_06ba raw 78012 and the Custom House mirror (europe.c:4198). */
  {
    const int credited = net > 0 ? net : 0;
    uint32_t cum = (uint32_t)nat->unknown24_pad[0] | ((uint32_t)nat->unknown24_pad[1] << 8) |
                   ((uint32_t)nat->unknown24_pad[2] << 16) |
                   ((uint32_t)nat->unknown24_pad[3] << 24);
    cum += (uint32_t)credited;
    nat->unknown24_pad[0] = (uint8_t)(cum & 0xffu);
    nat->unknown24_pad[1] = (uint8_t)((cum >> 8) & 0xffu);
    nat->unknown24_pad[2] = (uint8_t)((cum >> 16) & 0xffu);
    nat->unknown24_pad[3] = (uint8_t)((cum >> 24) & 0xffu);
  }
  units_play_event_sound(0x5a); /* FUN_5fef_1908: cheering + fireworks (COLDIG 15) */
  if (popups) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = units_combat_nation_label(col1, nation_id);
    tok.string1 = europe && europe->port_city[0] ? europe->port_city : "";
    tok.number0 = value;
    tok.has_number0 = true;
    tok.number1 = pct;
    tok.has_number1 = true;
    tok.number2 = net;
    tok.has_number2 = true;
    char body[AI_POPUP_BODY_LEN];
    if (game_txt) {
      /* DOS-LITERAL FUN_5fef_1908: the tag is picked by the WoI bit
       * (DS:0x5382 & 1), not by the share — pre-WoI 0x1bfd = @LOOTCASH
       * (raw 100225), post-WoI 0x1be0 = @CASHTREASURE (raw 100243). Cortes
       * with tax 0 computes share 0 and still gets @LOOTCASH. */
      const bool woi = col1->head.game_options.woi != 0;
      popup_msg_fill(game_txt, woi ? "CASHTREASURE" : "LOOTCASH", &tok, "", body, sizeof(body));
    } else {
      body[0] = '\0';
    }
    ai_popup_enqueue_ok(popups, AI_POPUP_TAG_INFO, NULL, body);
  }
  (void)units_despawn(pool, treasure_id);
}

int units_ai_treasure_cash_in_colony(
  ColonizeUnitPool* pool,
  ColonizeCol1Save* col1,
  int nation_id,
  int treasure_id,
  AiPopupState* popups,
  const ColonizeMsgCatalog* game_txt
) {
  /*
   * FUN_521d_20e6 treasure act band, first arm (move_scoring_20e6_full.md raw
   * ~2315-2331, `if (*(char *)(param_2 * 0x1c + 0x3146) == '\n') { if
   * (iStack_2e == 0) { ... } }`):
   *
   *   uVar13   = (uint)*(byte *)(param_2 * 0x1c + 0x315b) * 100;
   *   nation+0x2a/+0x2c += uVar13            (32-bit gold, NO Crown cut)
   *   if ((*(byte *)0x5382 & 1) == 0) {      pre-WoI only
   *     FUN_1000_8628(0, func_0x00018b94(nation));   STRING0 = @NATIONALITY
   *     FUN_1000_8628(1, *(word *)(nation*2 + -0x7c74));  STRING1 = @HOMEPORT
   *     FUN_1000_8b9e(0, uVar13, 0);                 NUMBER0 = the gold
   *     FUN_1000_8842(0x181f, 0x1786, 2);            popup @LOOTFOREIGN
   *   }
   *   goto LAB_OVL14_L0000__0047b9;          FUN_1000_89f8 = destroy_unit
   *
   * Resolutions: `-0x72f6` (via func_0x00018b94 = FUN_281f_09a4 →
   * FUN_15b3_01e0) is the NAMES.TXT @NATIONALITY adjective table, NOT the
   * player's @COLONYNAME (docs/archive/bugs_resolutions_full_2026-09-05.md
   * "New Spain Privateer"); `-0x7c74` is the @HOMEPORT table (already the
   * DS reading behind units_home_port_name / europe.c:2699); popup id 0x1786
   * is the DS address of the tag string `LOOTFOREIGN` (docs/popup_tag_ids.md
   * line 183). DOS queues no sound here — 48d3_06ba's FUN_281f_048e(0x24)
   * Fiddler's Dance belongs to the Europe cash-in, not this one.
   *
   * No Crown share and no Cortes / galleon term appear anywhere in this band:
   * an AI Treasure standing in ANY own colony cashes at full face value.
   *
   * Caller owns the "standing in an own colony" gate (DOS iStack_2e == 0);
   * this function only re-checks ownership of the unit. Returns the gold
   * credited, 0 when nothing was done (the unit is left alive in that case).
   */
  ColonizeUnit* treasure = units_get(pool, treasure_id);
  if (!pool || !col1 || nation_id < 0 || nation_id > 3 || !treasure || !treasure->active) {
    return 0;
  }
  if (treasure->nation_id != nation_id) {
    return 0;
  }
  const ColonizeUnitType* ty = units_type(pool, treasure->type_index);
  if (!units_type_is_treasure(ty)) {
    return 0;
  }
  const int value = units_treasure_value_gold(treasure);
  if (value > 0) {
    /* DOS nation+0x2a/+0x2c, 32-bit — one store per nation (audit G3). */
    europe_nation_gold_add(NULL, col1, nation_id, (long)value);
  }
  /* DS:0x5382 bit0 = the War of Independence flag; the popup is pre-WoI only. */
  if (value > 0 && popups && col1->head.game_options.woi == 0) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = units_combat_nation_label(col1, nation_id);
    tok.string1 = units_home_port_name(col1, nation_id);
    tok.number0 = value;
    tok.has_number0 = true;
    char body[AI_POPUP_BODY_LEN];
    if (game_txt) {
      popup_msg_fill(game_txt, "LOOTFOREIGN", &tok, "", body, sizeof(body));
    } else {
      body[0] = '\0';
    }
    ai_popup_enqueue_ok(popups, AI_POPUP_TAG_INFO, NULL, body);
  }
  (void)units_despawn(pool, treasure_id); /* LAB_0047b9 → FUN_1427_0824 destroy */
  return value;
}

int units_king_galleon_offer_for_unit_w(
  const ColonizeWorld* w,
  int nation_id,
  int treasure_id,
  AiPopupState* popups,
  const ColonizeMsgCatalog* game_txt
) {
  ColonizeUnitPool* pool = w->units;
  const ColonizeColonyPool* colonies = w->colonies;
  EuropeScreen* europe = w->europe;
  ColonizeCol1Save* col1 = w->col1;

  /*
   * DOS-LITERAL FUN_465b_0000 raw 75800-75815 — the ONLY King's-Galleon
   * trigger in the image, at the tail of a completed move, for the unit that
   * just moved:
   *
   *   if (unit[+0x3146] == '\n' && nation < 4 && DS[nation*0x34+0x543f] == 0) {
   *     if (-1 < FUN_281f_0696(dx,dy)) {                 // Euro colony on dest
   *       if (DS[nation*0x13 - 0x6da5] != 0 && (*0x5382 & 1) == 0)  // owns a Galleon, pre-WoI
   *         if (FUN_281f_07b4(nation,10) == 0) goto skip;           // ... unless Cortes
   *       cid = FUN_281f_07be(dx,dy);
   *       if (DS[cid*0xca + 0x5d62] & 0x40) FUN_2a1f_0186(unit, nation);  // COASTAL bit
   *     }
   *   }
   *
   * FUN_2a1f_0186 -> FUN_5fef_1908 (the offer/cash body). There is no
   * end-of-turn sweep in DOS: a parked or refused Treasure is never asked
   * again unless it moves onto a coastal colony tile again.
   * Returns 1 when an offer was enqueued or a WoI cash happened, else 0.
   */
  if (!pool || !colonies || !col1 || nation_id < 0 || nation_id > 3) {
    return 0;
  }
  const ColonizeUnit* treasure = units_get_const(pool, treasure_id);
  if (!treasure || !treasure->active || treasure->nation_id != nation_id ||
      treasure->aboard_ship_id >= 0) {
    return 0;
  }
  if (!units_type_is_treasure(units_type(pool, treasure->type_index))) {
    return 0;
  }
  const bool woi = col1->head.game_options.woi != 0;
  const bool cortes = founding_fathers_nation_has(col1, nation_id, FF_HERNAN_CORTES);
  /* FUN_465b_0000: per-nation unit-type count table (-0x6db4, stride 0x13), type 0xf = Galleon. */
  bool has_galleon = false;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(pool, u->type_index);
    if (ty && ty->name[0] && units_type_is_galleon(ty)) {
      has_galleon = true;
      break;
    }
  }
  if (!woi && !cortes && has_galleon) {
    return 0;
  }
  {
    const int cid = colonies_id_at(colonies, treasure->x, treasure->y);
    const ColonizeColony* c = cid >= 0 ? colonies_get(colonies, cid) : NULL;
    if (!c || !c->active || c->nation_id != nation_id) {
      return 0;
    }
    /* DOS colony record +0x1c & 0x40 = the COASTAL bit written once at
     * founding by FUN_364b_1ba8 raw 58105-58110 — a lake-only site never
     * gets it, so the map adjacency probe is the wrong test. */
    if ((c->colony_flags & COLONIZE_COLONY_FLAG_COASTAL) == 0) {
      return 0;
    }
    if (units_king_galleon_treasure_value(treasure) <= 0) {
      return 0;
    }
    if (woi) {
      /* FUN_5fef_1908 else-branch: no King, full value, @CASHTREASURE. */
      units_king_galleon_credit(pool, europe, col1, nation_id, treasure_id, 0, popups, game_txt);
      return 1;
    }
    if (!popups) {
      return 0;
    }
    /* Don't stack a second offer for the same Treasure while one is queued. */
    for (int q = 0; q < popups->queue_count; ++q) {
      if (popups->queue[q].tag == AI_POPUP_TAG_KING_GALLEON &&
          popups->queue[q].payload == treasure_id) {
        return 0;
      }
    }
    const int d = (int)col1->head.difficulty;
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = reports_difficulty_title(d >= 0 && d < 5 ? d : 0);
    tok.string1 = col1->player[nation_id].name[0] ? col1->player[nation_id].name
                                                  : units_combat_nation_label(col1, nation_id);
    tok.string2 = europe && europe->port_city[0] ? europe->port_city : "";
    tok.number0 = (int)col1->nation[nation_id].tax_rate;
    tok.has_number0 = true;
    char body[AI_POPUP_BODY_LEN];
    const char* galleon_section = cortes ? "KINGGALLEON3" : "KINGGALLEON2";
    if (game_txt) {
      popup_msg_fill(game_txt, galleon_section, &tok, "", body, sizeof(body));
    } else {
      body[0] = '\0';
    }
    /* DOS-LITERAL FUN_5fef_1908 raw 100189: FUN_281f_048e(0x3e) — the royal
     * sting plays before the CHOICE is raised. */
    units_play_event_sound(0x3e);
    /* GAME.TXT @KINGGALLEON2/@KINGGALLEON3 choice lines (same pair in both
     * sections); the catalog spells the second "sooner {kiss} your" —
     * braces kept (renderer/plain-sink convention), same as the ai_king.c /
     * ai_contact.c popup_msg_choices callers. */
    char raw_choices[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
    char c0[AI_POPUP_CHOICE_LEN];
    char c1[AI_POPUP_CHOICE_LEN];
    const ColonizeMsgSection* galleon_sec =
      game_txt ? assets_msg_find(game_txt, galleon_section) : NULL;
    const int galleon_nch =
      galleon_sec ? popup_msg_choices(galleon_sec, raw_choices, AI_POPUP_CHOICE_MAX) : 0;
    popup_msg_apply_tokens(
      c0, sizeof(c0),
      galleon_nch >= 2 ? raw_choices[0] : "",
      NULL
    );
    popup_msg_apply_tokens(
      c1, sizeof(c1),
      galleon_nch >= 2 ? raw_choices[1] : "",
      NULL
    );
    const char* choices[2] = {c0, c1};
    const int cids[2] = {1, 0};
    (void)ai_popup_enqueue_choice_ctx(
      popups, AI_POPUP_TAG_KING_GALLEON, nation_id, -1, treasure_id, NULL, body, choices, cids, 2
    );
    return 1;
  }
}


void units_fountain_youth_enqueue_pick(
  EuropeScreen* europe, AiPopupState* popups, const ColonizeMsgCatalog* game_txt, int human,
  int remaining
) {
  if (!europe || !popups || remaining <= 0) {
    return;
  }
  europe_pool_ensure_filled(europe);
  const char* labels[EUROPE_POOL_SIZE];
  int ids[EUROPE_POOL_SIZE];
  for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
    labels[i] = europe_pool_label(europe, i);
    ids[i] = i;
  }
  /* @RECRUIT with %NUMBER0 = 0: 4884(1,0) zeroes the passage before drawing
   * the same list; the FoY tail carries no section of its own. */
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.number0 = 0;
  tok.has_number0 = true;
  char body[AI_POPUP_BODY_LEN];
  if (game_txt) {
    popup_msg_fill(game_txt, "RECRUIT", &tok, "", body, sizeof(body));
  } else {
    body[0] = '\0';
  }
  (void)ai_popup_enqueue_choice_ctx(
    popups, AI_POPUP_TAG_FOUNTAIN_YOUTH, human, -1, remaining, NULL, body, labels, ids,
    EUROPE_POOL_SIZE
  );
  /* DOS 4884(1,0) latches DS:0x1f5e = 3 (MSS3 frontiersman) for the FoY
   * picks — not the Europe recruit courtier the @RECRUIT section implies. */
  ai_popup_set_last_graphic_mss(popups, 3);
}

bool units_fountain_youth_apply_popup(
  EuropeScreen* europe, AiPopupState* popups, const ColonizeMsgCatalog* game_txt
) {
  return units_fountain_youth_apply_popup_ex(europe, popups, game_txt, NULL);
}

bool units_fountain_youth_apply_popup_ex(
  EuropeScreen* europe,
  AiPopupState* popups,
  const ColonizeMsgCatalog* game_txt,
  ColonizeDosRng* rng
) {
  if (!popups || popups->result_tag != AI_POPUP_TAG_FOUNTAIN_YOUTH) {
    return false;
  }
  if (!europe) {
    return true;
  }
  const int remaining = popups->result_payload;
  int slot = popups->result_cancelled ? 0 : popups->result_choice_id;
  if (slot < 0 || slot >= EUROPE_POOL_SIZE) {
    slot = 0;
  }
  /* The 4884 tail refills the emptied slot with a `46d4` roll off the shared
   * game stream — pass the real rng through (smell audit 2026-09-10 G5). */
  (void)europe_recruit_free_from_pool_ex(europe, slot, rng);
  if (remaining - 1 > 0) {
    units_fountain_youth_enqueue_pick(
      europe, popups, game_txt, popups->result_nation_a, remaining - 1
    );
  }
  return true;
}

void units_brewster_enqueue_pick(
  EuropeScreen* europe, AiPopupState* popups, const ColonizeMsgCatalog* game_txt, int human
) {
  if (!europe || !popups) {
    return;
  }
  if (ai_popup_busy(popups)) {
    return; /* one outstanding pick at a time; the tick re-offers next turn */
  }
  europe_pool_ensure_filled(europe);
  const char* labels[EUROPE_POOL_SIZE];
  int ids[EUROPE_POOL_SIZE];
  for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
    labels[i] = europe_pool_label(europe, i);
    ids[i] = i;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.country = europe->nation_name[0] ? europe->nation_name : "Europe";
  tok.string0 = "Europe";
  char body[AI_POPUP_BODY_LEN];
  if (game_txt) {
    popup_msg_fill(game_txt, "RECRUITCHOOSE", &tok, "", body, sizeof(body));
  } else {
    body[0] = '\0';
  }
  (void)ai_popup_enqueue_choice_ctx(
    popups, AI_POPUP_TAG_BREWSTER_PICK, human, -1, 0, NULL, body, labels, ids, EUROPE_POOL_SIZE
  );
  /* DOS 4884(0,1) latches DS:0x1f5e = 4 (MSS4 friar) for the Brewster pick. */
  ai_popup_set_last_graphic_mss(popups, 4);
}

bool units_brewster_apply_popup(
  EuropeScreen* europe, AiPopupState* popups, ColonizeUnitPool* units
) {
  return units_brewster_apply_popup_ex_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(units), .rng=(ColonizeDosRng*)(NULL), .europe=(EuropeScreen*)(europe)}, popups);
}

bool units_brewster_apply_popup_ex_w(
  const ColonizeWorld* w,
  AiPopupState* popups
) {
  EuropeScreen* europe = w->europe;
  ColonizeUnitPool* units = w->units;
  ColonizeDosRng* rng = w->rng;

  if (!popups || popups->result_tag != AI_POPUP_TAG_BREWSTER_PICK) {
    return false;
  }
  if (!europe || popups->result_cancelled) {
    return true; /* 4884: local_58 < 0 → nothing moves, crosses kept */
  }
  const int slot = popups->result_choice_id;
  if (slot < 0 || slot >= EUROPE_POOL_SIZE) {
    return true;
  }
  const int human = popups->result_nation_a;
  /* Same 4884 tail refill as the FoY pick — the emptied slot's `46d4` roll
   * belongs on the shared game stream (smell audit 2026-09-10 G5). */
  if (!europe_brewster_pick_from_pool_ex(europe, slot, rng)) {
    return true;
  }
  /* Mirror the dock immigrant as the Europe-map unit (Col1 capture), same
   * shape as turn.c's random-pick path. */
  if (units && europe->dock_count > 0 && human >= 0 && human < 4) {
    const EuropeDockImmigrant* d = &europe->dock[europe->dock_count - 1];
    /* Same shared stream turn.c's imm==1 path uses: europe_dock_unit_dos_type's
     * Dragoon roll (46d4 bound difficulty+4 for a human) is a real DOS draw, and
     * passing NULL here dropped it — every Brewster Soldier mirrored as Soldiers.
     * Draw order matches 4884: pool refill first, then the mirror-unit roll. */
    (void)europe_spawn_dock_mirror_unit(
      units, human, d->profession, (int)europe->difficulty, true, rng
    );
  }
  return true;
}


bool units_king_galleon_apply_popup_w(
  const ColonizeWorld* w,
  AiPopupState* popups,
  const ColonizeMsgCatalog* game_txt
) {
  ColonizeUnitPool* pool = w->units;
  EuropeScreen* europe = w->europe;
  ColonizeCol1Save* col1 = w->col1;

  if (!popups || popups->result_tag != AI_POPUP_TAG_KING_GALLEON) {
    return false;
  }
  if (!pool || !col1 || popups->result_cancelled || popups->result_choice_id != 1) {
    return true; /* FUN_5fef_1908: choice != 1 → return, Treasure untouched */
  }
  const int nation = popups->result_nation_a;
  if (nation < 0 || nation > 3) {
    return true;
  }
  units_king_galleon_credit(
    pool, europe, col1, nation, popups->result_payload,
    units_king_galleon_share_pct(col1, nation), popups, game_txt
  );
  return true;
}
