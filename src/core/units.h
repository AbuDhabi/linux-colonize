#ifndef COLONIZE_UNITS_H
#define COLONIZE_UNITS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/world.h"

#include "core/assets.h"
#include "core/ai_popup.h"
#include "core/colony.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/font.h"
#include "core/map.h"
#include "core/ss.h"

typedef struct EuropeScreen EuropeScreen;

/*
 * Set Col1 save used by units_try_move for FF combat hooks (Washington promote,
 * Drake naval, Paul Revere auto-arm). turn_refresh_moves_for_nation sets this.
 * Pass NULL to clear. Pointer is not owned.
 */
void units_set_ff_col1(const ColonizeCol1Save* col1);

/* Optional live map for has_unit occupancy maintenance (spawn/move/despawn). */
void units_set_occupancy_map(ColonizeWorldMap* map);
/* Recompute the has_unit presence bit on every tile from the pool (owner nibble restamped only where units stand). */
void units_occupancy_rebuild(ColonizeUnitPool* pool);
/* Presence bit refresh for the two tiles of a direct x/y write (DOS UNITFLAG clear+set). */
void units_occupancy_notify_moved(ColonizeUnitPool* pool, int old_x, int old_y, int new_x, int new_y);

/*
 * Optional post-win native settlement fallout context for
 * units_resolve_land_combat_ff (FUN_5fef_31ea-shaped). When col1/map are non-NULL
 * and attacker beats defender nation>=4, units_try_native_settlement_fallout runs.
 * conquest_gold: caller-known treasure amount, or -1 → Cortes peels FUN_5fef_31ea
 * amount via combat rng (non-Cortes still skips). Pass NULL map to disable.
 */
void units_set_native_fallout_context(
  ColonizeCol1Save* col1,
  ColonizeWorldMap* map,
  int conquest_gold
);

/*
 * Optional colony pool for land combat fortification defense
 * (Stockade/Fort/Fortress via FUN_157e_015e). Pass NULL to clear. Pointer is
 * not owned. units_try_move sets this from its colonies arg before combat.
 */
void units_set_combat_colonies(const ColonizeColonyPool* colonies);

/*
 * Human nation id for Combat Analysis gate (FUN_5fef_1b0e 0x5383&2 path).
 * Pass -1 to clear. game_loop / turn set this alongside units_set_ff_col1.
 */
void units_set_combat_human_nation(int human_nation);

/*
 * Optional GAME.TXT + ai_popup for structural combat outcome modals
 * (@EUROPEWIN/LOSE, @LOOT*, @SHIP*, @DEMOTE, …). Pass NULL to clear.
 */
void units_set_combat_popups(AiPopupState* popups, const ColonizeMsgCatalog* game_txt);

/*
 * Europe screen for the DOS damaged-ship teleport (FUN_5fef_0352 raw 99610-
 * 99623 / asm 5fef:0bc0-5fef:0cf9): a ship that loses at sea and has no own
 * Drydock/Shipyard colony is unlinked from its tile and re-placed at
 * (nation-20, nation-20) — the off-map Europe slot — on the spot. Pass NULL
 * to clear (headless callers leave it unset and keep the ship on its tile).
 */
struct EuropeScreen;
void units_set_combat_europe(struct EuropeScreen* europe);

/*
 * ai_contact's ambush arm draws its own @INDIANWIN1/2 chrome (muskets/horses
 * seizure line, chief portrait), so it sets this around its
 * units_resolve_land_combat call to keep the generic native-attacker
 * @INDIANWIN0/@INDIANLOSE chrome in units_combat_outcome_popups quiet.
 * Every other native attack path (braves stepping onto human tiles via
 * units_try_move, alarm marches) gets the generic chrome.
 */
void units_set_native_combat_chrome_owned(int owned);

/*
 * Optional sound.c hooks for the DOS-evidenced combat "Military" BGM sting
 * (SOUND_MILITARY_BGM_ID, see sound.h) — kept as function pointers rather
 * than a direct link so units.c stays linkable without sound.c (several
 * unit_* test binaries compile units.c standalone). Pass NULL/NULL to
 * clear; game_loop wires the real sound_play/sound_active_song_id once at
 * startup, matching units_set_combat_popups's wiring convention.
 */
typedef void (*ColonizeSoundPlayFn)(int id);
typedef int (*ColonizeSoundActiveIdFn)(void);
/* 281f_0498 pool switch from combat (naval win/loss beats). */
void units_set_bgm_hook(ColonizeSoundPlayFn set_bgm_fn);
void units_set_combat_music_hooks(
  ColonizeSoundPlayFn play_fn, ColonizeSoundActiveIdFn active_id_fn
);

/* Original COLONY.SAV can hold well over 64 map units (natives + Europeans). */
#define COLONIZE_UNITS_MAX 256
#define COLONIZE_UNIT_TYPES_MAX 32
#define COLONIZE_UNIT_CARGO_MAX 6 /* Man-O-War hold size */

typedef enum ColonizeUnitDomain {
  COLONIZE_UNIT_DOMAIN_LAND = 0,
  COLONIZE_UNIT_DOMAIN_SEA = 1
} ColonizeUnitDomain;

typedef struct ColonizeUnitType {
  char name[32];
  /* NAMES.TXT @UNIT row + 1 (== ColonizeUnitKind + 1); 0 = not stamped. The
   * row, not the name, is what identifies a unit type — see units_type_kind. */
  int kind_plus1;
  int icon_sprite; /* ICONS.SS 0-based blit index (@UNIT icon is 1-based in NAMES.TXT) */
  int movement;
  int attack;
  int defense;
  int cargo;
  int cost; /* NAMES.TXT @UNIT cost column (DOS 0x5239): colony hammers/32 — FUN_15eb_33aa */
  int tools; /* NAMES.TXT @UNIT tools column (DOS 0x523a): colony tools/10 — FUN_15eb_33aa */
  int space; /* NAMES.TXT @UNIT "size" column (DOS 0x5238): ship slots this unit takes; 99 = cannot board */
  int guns; /* NAMES.TXT @UNIT guns column (DOS 0x523b): naval sink power */
  int hull; /* NAMES.TXT @UNIT hull column (DOS 0x523c): naval survive-as-damaged weight */
  ColonizeUnitDomain domain;
} ColonizeUnitType;

typedef struct ColonizeUnit {
  int id;
  int type_index;
  int x;
  int y;
  /* MP gauge in thirds (UNITS_MP_PER_TILE per plains tile). Euro units: thirds
   * REMAINING. Native units (nation_id >= 4): DOS SPENT byte, counting up to
   * max — go through units_mp_charge / units_move.h helpers, never compare
   * raw across the two domains. Was moves_left. */
  int moves;
  bool active;
  int nation_id; /* 0..3 European, 4..11 native tribes (COL1) */
  int aboard_ship_id; /* -1 = on map; else id of carrying ship */
  int cargo_ids[COLONIZE_UNIT_CARGO_MAX]; /* passenger unit ids (ships only) */
  int cargo_count;
  /* Commodity holds (ships/wagons): type is @CARGO index; amount 0 = empty. */
  int hold_goods_type[COLONIZE_UNIT_CARGO_MAX];
  int hold_goods_amount[COLONIZE_UNIT_CARGO_MAX];
  int orders; /* @ORDERS: 0=none, 1=sentry, 3=goto, … */
  int goto_x; /* UNITS_GOTO_NONE (0xFF) = none */
  int goto_y;
  int follow_unit_id; /* -1 none; target when orders==UNITS_ORDER_FOLLOW */
  int profession; /* NAMES.TXT @JOB index; 28 = none (COL1 plain colonist) */
  int tools; /* carried tools (Pioneers); 0–100 in steps of 20 */
  int muskets; /* 0 or 50 when armed */
  int horses; /* 0 or 50 when mounted */
  int home_tribe_id; /* DOS unit+0x06 / DS:314a; -1 = none */
  /* COL1 unit+0x16, DOS multi-purpose counter: Brave labor pulse, treasure
   * clock, ship repair timer, trade-route stop index, cower timer, Europe
   * voyage turns. Was turns_worked. */
  int col1_counter16;
  /*
   * Port-only nights-parked counter for the units_wake MP refund (DOS derives
   * wake MP from the spent byte alone; +0x16 is the shared treasure-clock /
   * repair-timer / route-stop / cower counter and must not be borrowed for
   * this). Not serialized: a freshly loaded parked unit imports its real
   * moves, so no refund is needed before the first turn refresh.
   */
  uint8_t park_nights;
  /*
   * Port-only "this zero is a SPEND, not a park" flag for units whose
   * moves the port zeroes for bookkeeping reasons. Boarding parks a
   * passenger at moves 0 while DOS's spent byte (+0x3149) may hold
   * either 0 (loaded in port) or max_mp (walked aboard from open shore —
   * the 465b_05ca ocean force-to-max), and the two cases behave
   * differently: FUN_4720_015c only offers landfall to cargo whose
   * spent byte is BELOW its max (viceroy_unpacked.c:76010-76026). Set when
   * the port zeroes an allotment that DOS would have spent; cleared by the
   * per-turn refresh (DOS clears every spent byte at the day top, viceroy
   * 6355-6357). Not serialized — a reloaded unit imports its real
   * moves / spent byte.
   */
  uint8_t mp_spent_turn;
  /*
   * Port-only: the allotment (thirds) a passenger still has while it rides
   * in a hold, i.e. max_mp minus DOS's spent byte +0x3149, which DOS leaves
   * untouched aboard — the ship's move and dock path never write a
   * passenger's +0x3149 (only the day-top reset FUN_130d_0290 does). Needed
   * because `moves` is the hold's park zero while aboard. -1 = full
   * allotment (nothing spent this turn / unknown). Set at boarding, read
   * when the passenger wakes or is put ashore, reset by the per-turn
   * refresh. Round-trips through the Col1 spent byte (bugs.md #544).
   */
  int aboard_moves;
  int last_dir; /* DOS unit facing / Col1 facing; 0..7 for AI scoring */
  uint8_t col1_flags15; /* DOS unit+0x15 flag byte (bits named in ColonizeCol1Unit); bit7 = ship damaged. Was col1_unknown15. */
  /*
   * DOS unit+0x07 / Col1 ai_plan. Starter saves use 0x58 ('X') on essentially
   * every unit; spawn defaults to COL1_UNIT_UNKNOWN16_HI_DEFAULT.
   */
  uint8_t col1_ai_plan;
  uint8_t col1_vis_mask; /* DOS nation high nibble (unit byte+3 >> 4); 0x10<<euro */
  /*
   * Raw DOS unit bytes +0x0c..+0x15 (holds_occupied, cargo_item nibbles,
   * cargo_hold[6]) exactly as loaded. DOS repurposes this region on land
   * units for state the port doesn't model (french-campaign originals:
   * braves carry a per-settlement counter in hold[2]; Euro land units and
   * even ships carry 196/216/236 in hold[5] — not pioneer tools). Kept so
   * capture can round-trip those bytes instead of fabricating sentinels.
   */
  uint8_t col1_hold_raw[10];
  uint8_t col1_hold_raw_valid;
  /* Raw DOS unit +0x06 (origin). Euro units DO carry values here in original
   * campaign saves (home colony index, 0-based) — not just Braves. */
  uint8_t col1_origin;
  /* Raw DOS facing byte +0x0b upper 5 bits (facing_pad) — set on some
   * original units; dropped bits changed the byte on round-trip. */
  uint8_t col1_facing_pad;
  /* Port-only, not saved: bit7 came from combat damage (repair timer), so the
   * completion popup says "repaired", not "construction complete". */
  uint8_t repair_pending;
} ColonizeUnit;

typedef struct ColonizeUnitPool {
  ColonizeUnitType types[COLONIZE_UNIT_TYPES_MAX];
  int type_count;
  ColonizeUnit units[COLONIZE_UNITS_MAX];
  int unit_count;
  int selected_id;
  /* bugs.md "Move to front": the unit in this pool SLOT (units[i], NOT a
   * unit id) boards departing ships first and heads the colony rosters
   * (-1 = none; DOS reorders its unit chain). Was board_first_id. */
  int board_first_slot;
  int next_id;
} ColonizeUnitPool;

bool units_load_types(ColonizeUnitPool* pool, const ColonizeMsgCatalog* names);
void units_reset(ColonizeUnitPool* pool);

int units_find_type(const ColonizeUnitPool* pool, const char* name);

/*
 * Colony construction raw-code decode — DOS-LITERAL FUN_15eb_32f8
 * (viceroy_unpacked.c raw 13423-13448), reached from the colony EOT via
 * FUN_281f_0cc2 -> FUN_364b_0114 (raw 56897):
 *
 *   code < 0       -> kind 0 (no project)
 *   code < 0x2a    -> kind 1, @BUILDING index = code
 *   code - 0x2a < 7 -> kind 2, @UNIT index = code - 0x1f
 *
 * @BUILDING has exactly 0x2a = 42 rows, so the seven unit codes 42..48 map to
 * @UNIT rows 11..17: Artillery, Wagon Train, Caravel, Merchantman, Galleon,
 * Privateer, Frigate. Man-O-War is @UNIT row 18 and is NOT reachable — the
 * `< 7` bound is what excludes it (FUN_15eb_38ba/38e8 likewise stop the
 * build-menu walk at code 0x30).
 */
#define COLONIZE_UNIT_BUILD_CODE_FIRST 42 /* 0x2a */
#define COLONIZE_UNIT_BUILD_CODE_COUNT 7
#define COLONIZE_UNIT_BUILD_CODE_BIAS 31 /* 0x1f */

/* @UNIT row index for a construction raw code, or -1 if the code is not a
 * unit project (FUN_15eb_32f8 kind != 2). */
int units_build_code_to_index(int raw_code);

/* @UNIT row indices the decode above can yield. */
#define COLONIZE_UNIT_INDEX_ARTILLERY 11
#define COLONIZE_UNIT_INDEX_WAGON_TRAIN 12
#define COLONIZE_UNIT_INDEX_SHIP_FIRST 13 /* Caravel */
#define COLONIZE_UNIT_INDEX_SHIP_LAST 18  /* Man-O-War (gate range, not buildable) */

/*
 * Name + colony cost of a unit construction project — DOS-LITERAL
 * FUN_15eb_33aa kind-2 arm (viceroy_unpacked.c raw 13482-13509), thunked as
 * FUN_281f_0ac4 and read by the colony EOT at raw 57742:
 *
 *   hammers = 0x5239[idx] * 0x20;
 *   if (hammers < 0x28) hammers = 0x28; else if (hammers < 0x34) hammers = 0x34;
 *   tools   = 0x523a[idx] * 10;
 *
 * 0x5239 / 0x523a are the @UNIT "cost" / "tools" columns (stride 0xe from
 * DS:0x5230, the same record `space` reads at 0x5238). Artillery = 6*32 = 192
 * hammers / 4*10 = 40 tools (golden-confirmed); Wagon Train = 1*32 = 32, which
 * the first clamp lifts to 40.
 *
 * The table is cached by units_load_types, so this takes no pool (the UI, the
 * turn loop and the dialogs all resolve raw codes without one). Falls back to
 * the shipped NAMES.TXT columns when no catalog has been loaded (tests).
 * Returns false for anything that is not a unit project code.
 */
bool units_build_project_info(int raw_code, const char** name, int* hammers, int* tools_cost);

/*
 * Destination @UNIT type name for a COLONIZE_EJECT_* equipment change applied
 * to an EXISTING unit of type cur_type_index — keeps a Continental
 * (Cont. Army <-> Cont. Cav.) or royal (Regulars <-> Cavalry) body in its own
 * tier instead of dropping it to the plain colonial pair. See units.c.
 */
const char* units_equip_role_type_name(
  const ColonizeUnitPool* units,
  int cur_type_index,
  int role
);
int units_spawn(ColonizeUnitPool* pool, int type_index, int x, int y);
/* Spawn even if the tile already has a unit (COL1 stacks / passengers). */
int units_spawn_allow_stack(ColonizeUnitPool* pool, int type_index, int x, int y);
/* Set nation_id and OR owner euro visibility bit (FUN_1427_0992). */
void units_set_nation(ColonizeUnit* unit, int nation_id);



bool units_despawn(ColonizeUnitPool* pool, int unit_id);
int units_id_at(const ColonizeUnitPool* pool, int x, int y);
/* First on-map unit at (x,y) that is neither except_unit_id nor
 * except_nation_id — i.e. "is this tile still contested". Colony-capture
 * call sites must check this before flipping ownership: a won combat only
 * clears the defender that fought, not every unit stacked on the tile. */
int units_foreign_unit_at(
  const ColonizeUnitPool* pool,
  int x,
  int y,
  int except_unit_id,
  int except_nation_id
);
ColonizeUnit* units_get(ColonizeUnitPool* pool, int unit_id);
const ColonizeUnit* units_get_const(const ColonizeUnitPool* pool, int unit_id);
const ColonizeUnitType* units_type(const ColonizeUnitPool* pool, int type_index);
bool units_is_sea(const ColonizeUnitPool* pool, int unit_id);

/*
 * DOS @UNIT type codes (COLONIZE/NAMES.TXT @UNIT row order, which is what DOS
 * stores in unit +0x3146 and what every `type < 0xb` / `type == 0x12` range
 * test in the decompile means; ai_euro.c:11742 ai_euro_20e6_dos_type carries
 * the same table). A Linux pool index is NOT a DOS code — synthetic fixtures
 * place types at arbitrary slots — so the mapping goes through the @UNIT name.
 */
typedef enum ColonizeUnitKind {
  UNITS_KIND_UNKNOWN = -1,
  UNITS_KIND_COLONIST = 0,     /* Colonists */
  UNITS_KIND_SOLDIER = 1,      /* Soldiers */
  UNITS_KIND_PIONEER = 2,      /* Pioneers */
  UNITS_KIND_MISSIONARY = 3,   /* Missionaries */
  UNITS_KIND_DRAGOON = 4,      /* Dragoons */
  UNITS_KIND_SCOUT = 5,        /* Scouts */
  UNITS_KIND_REGULAR = 6,      /* Regulars (King) */
  UNITS_KIND_CONT_CAV = 7,     /* Cont. Cav. */
  UNITS_KIND_CAVALRY = 8,      /* Cavalry (King) */
  UNITS_KIND_CONT_ARMY = 9,    /* Cont. Army */
  UNITS_KIND_TREASURE = 10,    /* Treasure */
  UNITS_KIND_ARTILLERY = 11,   /* Artillery */
  UNITS_KIND_WAGON = 12,       /* Wagon Train */
  UNITS_KIND_CARAVEL = 13,
  UNITS_KIND_MERCHANTMAN = 14,
  UNITS_KIND_GALLEON = 15,
  UNITS_KIND_PRIVATEER = 16,
  UNITS_KIND_FRIGATE = 17,
  UNITS_KIND_MAN_O_WAR = 18,
  UNITS_KIND_BRAVE = 19,       /* Braves */
  UNITS_KIND_ARMED_BRAVE = 20, /* Armed Braves */
  UNITS_KIND_MTD_BRAVE = 21,   /* Mtd. Braves */
  UNITS_KIND_MTD_WARRIOR = 22  /* Mtd. Warriors */
} ColonizeUnitKind;

/*
 * Classify an @UNIT type name. Every spelling that any call site in the tree
 * used before the predicates landed is accepted, most-specific first:
 *   7  "Cont. Cav" / "Continental Cav"
 *   9  "Cont. Army" / "Continental Army" / bare "Army"
 *   6  "Regular"
 *   8  "Cavalry" / "Cav." / bare "Cav"
 *   18 "Man-O-War" / "Man-o-War" / "Man O War" / "Man of War" / "Man-O'-War"
 *   14 "Merchantman"  15 "Galleon"  16 "Privateer"  17 "Frigate"  13 "Caravel"
 *   10 "Treasure"     11 "Artillery" / "Cannon"     12 "Wagon"
 *   22 "Mtd. Warrior" / "Mtd Warrior" / "Mounted Warrior"
 *   21 "Mtd. Brave" / "Mtd Brave" / "Mounted Brave"
 *   20 "Armed Brave"  19 "Brave"
 *   4  "Dragoon"      5  "Scout"    2  "Pioneer" / "Hardy"
 *   3  "Missionar" / "Mission" / "Jesuit"
 *   1  "Soldier"      0  "Colonist"
 * Returns UNITS_KIND_UNKNOWN for a name none of those match.
 */
/*
 * Pool slot for a @UNIT row / ColonizeUnitKind. Unit types are loaded from
 * NAMES.TXT @UNIT in file order, so the kind IS the row index; this only
 * range-checks it against what the catalog actually provided. Prefer it over
 * units_find_type(pool, "Artillery") — the port compiles no MicroProse names
 * of its own, and a renamed row must not break the lookup.
 */
int units_kind_type_index(const ColonizeUnitPool* pool, ColonizeUnitKind kind);

ColonizeUnitKind units_name_kind(const char* name);
/* Test-only seam: name -> kind for fixtures that build a pool out of names.
 * The production binary never installs one (it carries no unit names). */
typedef ColonizeUnitKind (*UnitsNameKindResolver)(const char* name);
void units_set_name_kind_resolver(UnitsNameKindResolver fn);
ColonizeUnitKind units_type_kind(const ColonizeUnitType* type);
/* DOS @UNIT code for a type, or -1 when the name is not a stock @UNIT row. */
int units_type_dos_code(const ColonizeUnitType* type);

/* Kind-set tests (see units.c for the exact @UNIT rows each covers). */
bool units_kind_is_continental(ColonizeUnitKind k); /* 7, 9 */
bool units_kind_is_royal(ColonizeUnitKind k);       /* 6, 8 */
bool units_kind_is_military(ColonizeUnitKind k);    /* 1, 4, 6, 7, 8, 9, 11 */
bool units_kind_is_mounted(ColonizeUnitKind k);     /* 4, 5, 7, 8, 21, 22 */
bool units_kind_is_ship(ColonizeUnitKind k);        /* 13..18 */
bool units_kind_is_native(ColonizeUnitKind k);      /* 19..22 */

bool units_type_is_colonist(const ColonizeUnitType* t);
bool units_type_is_soldier(const ColonizeUnitType* t);
bool units_type_is_pioneer(const ColonizeUnitType* t);
bool units_type_is_missionary(const ColonizeUnitType* t);
bool units_type_is_dragoon(const ColonizeUnitType* t);
bool units_type_is_scout(const ColonizeUnitType* t);
bool units_type_is_regular(const ColonizeUnitType* t);
bool units_type_is_cont_cav(const ColonizeUnitType* t);
bool units_type_is_cavalry(const ColonizeUnitType* t);
bool units_type_is_cont_army(const ColonizeUnitType* t);
bool units_type_is_continental(const ColonizeUnitType* t);
bool units_type_is_royal(const ColonizeUnitType* t);
bool units_type_is_treasure(const ColonizeUnitType* t);
bool units_type_is_artillery(const ColonizeUnitType* t);
bool units_type_is_wagon(const ColonizeUnitType* t);
bool units_type_is_caravel(const ColonizeUnitType* t);
bool units_type_is_merchantman(const ColonizeUnitType* t);
bool units_type_is_galleon(const ColonizeUnitType* t);
bool units_type_is_privateer(const ColonizeUnitType* t);
bool units_type_is_frigate(const ColonizeUnitType* t);
bool units_type_is_man_o_war(const ColonizeUnitType* t);
bool units_type_is_ship(const ColonizeUnitType* t);
bool units_type_is_military(const ColonizeUnitType* t);
bool units_type_is_mounted(const ColonizeUnitType* t);
bool units_type_is_native(const ColonizeUnitType* t);
/*
 * Unit-level missionary test (AC-34 / IN-45): the @UNIT Missionaries type or
 * the NAMES @JOB 24 Missionary profession, which is what "Jesuit Missionaries"
 * is in this port (units_display_name never spells "Jesuit"). Widest of the
 * three former rules (ai_contact "Mission", ai_euro "Missionary"|"Jesuit",
 * col1_bridge "Missionary").
 */
bool units_is_missionary(const ColonizeUnitPool* pool, const ColonizeUnit* u);


/*
 * Tile-stack iterator (theme M): *slot is the pool index to resume from (start
 * at 0). Returns the next active, on-map unit standing on (x,y) — aboard
 * passengers are excluded, matching DOS parking them at (-2,-2) — or NULL.
 */
ColonizeUnit* units_next_on_tile(ColonizeUnitPool* pool, int x, int y, int* slot);
const ColonizeUnit* units_next_on_tile_const(
  const ColonizeUnitPool* pool, int x, int y, int* slot
);
/* Active, on-map, not-aboard units standing on (x,y). */
int units_count_at(const ColonizeUnitPool* pool, int x, int y);
/*
 * FUN_13f1_02f8 sight radius: 1; Galleon/Privateer/Frigate 2; de Soto (FF 7)
 * makes every non-ship unit 2; Scouts +1 on top. FUN_13f1_02b4 then reveals
 * through map_reveal_sight (outer ring domain-gated). col1 may be NULL.
 */
int units_sight_radius(
  const ColonizeUnitPool* pool, const ColonizeUnit* u, const ColonizeCol1Save* col1
);
/*
 * FUN_13f1_02f8 → 0158 unit sight reveal with the DOS per-tile side effects
 * (FUN_13f1_000a): seen bit; unowned non-rumour tiles get the nation's owner
 * nibble (FUN_137f_0228); units on the tile get this nation's vis bit
 * (FUN_1427_09ac — natives only inside the |d|<2 core); a colony on the tile
 * gets its pop/fort snapshot (FUN_364b_1b4c). colonies / col1 may be NULL.
 * Returns true when the core ring touched a Pacific-strip water tile
 * (FUN_13f1_0158 DS:0x1e8 arm) — caller decides on the woodcut.
 */
bool units_reveal_sight_w(
  const ColonizeWorld* w,
  const ColonizeUnit* u
);
/*
 * FUN_1427_0c9a: vis mask a unit of mover_nation acquires by standing on
 * (x,y): tile owner nibble's bit (Euro movers only) | every nation watching
 * the tile (map_nation_watches_tile). Low-nibble form (1<<nation).
 */
uint8_t units_vis_mask_for_tile(const ColonizeWorldMap* map, int x, int y, int mover_nation);
/*
 * Move commit (FUN_465b_0000 / 48d3): FUN_1427_0968 clears the mover's (and
 * its cargo's) vis bits, then FUN_1427_0ce6 + 07fe OR the tile mask back in.
 * The mover's own bit is kept — DOS re-adds it through the reveal that
 * always follows a move (own tile is in the sight core).
 */
void units_vis_mask_after_move(
  ColonizeUnitPool* pool, const ColonizeWorldMap* map, int unit_id, int x, int y
);
/* Live ships of one nation (DOS -0x6be8 ship_counts[nation] equivalent). */
int units_count_sea_for_nation(const ColonizeUnitPool* pool, int nation_id);
bool units_is_on_map(const ColonizeUnit* unit);

/* Equipment the unit carries into a new colony warehouse when founding. */
void units_founder_loot(
  const ColonizeUnitPool* pool,
  int unit_id,
  int* out_tools,
  int* out_muskets,
  int* out_horses
);



/*
 * Test-fixture hygiene: process-global callback hooks (move/combat watch,
 * dissolve, raid-repelled, popup pump, bgm) persist across
 * units_reset(pool) since they aren't per-pool state. Call this to put them
 * all back to their unregistered (NULL) initial values, e.g. at the top of a
 * shared test fixture's setup, so one test's registrations can't leak into
 * the next.
 */
void units_reset_hooks(void);

/*
 * Per-unit-id shadow state that outlives units_reset(pool) because it is
 * indexed by unit_id rather than owned by the pool: the goto anti-backtrack
 * shadow (s_units_goto_last_dir). Unit ids are reused by a fresh pool on New
 * Game / Load, so without this a slot's stale direction from the outgoing
 * campaign could false-positive the anti-backtrack check for a unrelated
 * unit that happens to reuse the same id. Call at the same point as the
 * other new-game/load resets (ai_init_new_game, game_apply_col1_save).
 */
void units_reset_state(void);







/*
 * NAMES.TXT @JOB indices used for unit skills (COL1 profession byte).
 * 0-based over the COLONIZE/NAMES.TXT `@JOB` rows (Farmer = 0 ... Convert = 27);
 * 0..18 (the colony work skills) are spelled COLONIZE_PROF_* in
 * colony_production.h, which also carries aliases for 25/26/27/28.
 */
#define UNITS_JOB_COLONIST 19 /* Free Colonists */
#define UNITS_JOB_PIONEER 20  /* Hardy Pioneers */
#define UNITS_JOB_SOLDIER 21  /* Veteran Soldiers */
#define UNITS_JOB_SCOUT 22    /* Seasoned Scouts */
#define UNITS_JOB_DRAGOON 23  /* Veteran Dragoons */
#define UNITS_JOB_MISSIONARY 24 /* Jesuit Missionaries expert; plain bless uses NONE */
#define UNITS_JOB_SERVANT 25  /* Indentured Servants */
#define UNITS_JOB_CRIMINAL 26 /* Petty Criminals */
#define UNITS_JOB_CONVERT 27  /* Indian Converts (= COLONIZE_PROF_CONVERT) */
#define UNITS_JOB_NONE 28     /* no expert skill (plain Pioneer/Soldier) */

/* Equipped map/fence icons (ICONS.SS); expert variants when profession matches. */
#define UNITS_ICON_PIONEER 73
#define UNITS_ICON_SOLDIER 74
#define UNITS_ICON_SCOUT 75
#define UNITS_ICON_DRAGOON 76
/*
 * The commissioned-but-not-Jesuit missionary: DOS FUN_112b_0060 tail,
 * `if (type == 3 && profession != 0x18) icon = 0x4e`, i.e. the last of the
 * 0x4a..0x4e generic-kit poses. The Jesuit keeps the @UNIT icon 106 →
 * sprite 105 (same black cassock, but coloured trim).
 */
#define UNITS_ICON_MISSIONARY 77
#define UNITS_ICON_JESUIT_MISSIONARY 105
#define UNITS_ICON_DAMAGED_ARTILLERY 65 /* DOS FUN_112b icon 0x42 (type 0xb + bit7) */
#define UNITS_ICON_HARDY_PIONEER 101
#define UNITS_ICON_VETERAN_SOLDIER 102
#define UNITS_ICON_SEASONED_SCOUT 103
#define UNITS_ICON_VETERAN_DRAGOON 104
/* Working inside a colony (unequipped citizen sprites). */
#define UNITS_ICON_HARDY_PIONEER_WORK 58
#define UNITS_ICON_VETERAN_SOLDIER_WORK 59
/* FUN_112b_0002 case 5 (profession 0x18) → 0x3e: the bookless black cassock. */
#define UNITS_ICON_JESUIT_MISSIONARY_WORK 61

#define UNITS_EQUIP_MUSKETS 50
#define UNITS_EQUIP_HORSES 50
#define UNITS_EQUIP_TOOLS_MAX 100
#define UNITS_EQUIP_TOOLS_STEP 20

/*
 * Human starter: Caravel (Dutch Merchantman) with Pioneer+Soldier, placed by the
 * FUN_48d3_048e ring hunt from the nation's @SCENARIO / mapgen landfall tile.
 */
void units_new_world_start(
  ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int nation_id,
  int difficulty
);

/*
 * Spawn European starter fleet (ship + Pioneer + Soldier) at (x,y).
 * FUN_75c2_235c (raw 121612-121647): type 0xd ship (0xe for the Dutch, nation 3),
 * then type 2 Pioneers, then type 1 Soldiers. Profession overrides: French
 * (nation 1) Pioneers → @JOB 0x14 Hardy Pioneer; Soldiers → @JOB 0x15 Veteran
 * Soldier when the nation is Spanish (2) **or** this nation is the human
 * (`is_human`) on difficulty < 2. Returns ship unit id or -1.
 */
int units_spawn_euro_starter_fleet(
  ColonizeUnitPool* pool,
  int nation_id,
  int difficulty,
  bool is_human,
  int x,
  int y,
  int goto_x,
  int goto_y
);

/* Panel label: "Hardy Pioneer", "Veteran Soldier", unit type name, … */
const char* units_display_name(const ColonizeUnitPool* pool, const ColonizeUnit* unit);

/*
 * ICONS.SS index per NAMES.TXT @JOB profession (0..28) for a colonist
 * working inside a colony / waiting on a dock — no field equipment, unlike
 * the on-map UNITS_ICON_* sprites; -1 if a profession has no dedicated
 * portrait (currently none). Job 18's sprite exists but the Expert Teacher
 * colonist type was cut from the final DOS game (unreachable leftover).
 */
int units_job_icon_sprite(int profession);

/*
 * DOS DS:0x30e indexed by @UNIT type — the default @JOB that type carries,
 * -1 when the type has no profession slot at all (FUN_15eb_0902, reached as
 * FUN_281f_0b78). True for the colonist-carrying types only; ships, wagons,
 * artillery and treasure trains are false. DOS uses this, not a unit's own
 * profession byte, to decide who is a person: the census population count
 * (FUN_4962_0018 → DS:0x9410) and the sidebar profession line both gate on it.
 */
/* DS:0x30e[@UNIT type] — the type's default @JOB, or -1 for no profession
 * slot (FUN_15eb_0902). */
int units_type_default_job(int type_index);
bool units_type_has_profession_slot(int type_index);
/*
 * Expert-skill label for a unit row: the plural NAMES @JOB field, or NULL for
 * the five non-expert professions (NONE/19/25/26/27) and for @UNIT types with
 * no profession slot. Shared by unit_stack.c and map_panel.c (UN-22).
 */
const char* units_profession_label(
  const ColonizeMsgCatalog* names, int type_index, int profession
);
/*
 * DOS-LITERAL FUN_49dd_0386: the map-panel profession LINE (singular @JOB
 * column 0, with the @MISC "Veteran"/"Expert" overrides). `allow_unskilled`
 * is the DOS param_3: 1 at the selected-unit call site (raw 78892), 0 at the
 * stack-list one (raw 79205). bugs.md #507.
 */
const char* units_profession_line(
  const ColonizeMsgCatalog* names, int type_index, int profession, bool allow_unskilled
);

/*
 * ICONS.SS index for a colonist working inside a colony (no field
 * equipment): units_job_icon_sprite(profession) if it has one, else the
 * @UNIT icon for unit_type_index.
 */
int units_working_colonist_sprite(
  const ColonizeUnitPool* pool,
  int unit_type_index,
  int profession
);

bool units_deploy_colonist(
  ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int x,
  int y,
  const char* immigrant_name
);

int units_map_sprite(const ColonizeUnitPool* pool, int unit_id);
/* Col1 @UNIT index after equipment remap (Scout/Soldier/Dragoon/Pioneer). */
int units_display_type_index(const ColonizeUnitPool* pool, int unit_id);
/*
 * "More units here" tab state for a unit's map chrome: another piece shares its
 * tile, or it is carrying (DOS decides it from the unit's own chain,
 * FUN_1427_0002/004a behind FUN_112b_01ba). Shared so the move slide and combat
 * lunge wear the same chrome as the standing draw (bugs.md #365).
 */
bool units_map_stack_chrome(const ColonizeUnitPool* pool, int unit_id);

/*
 * Which unit (if any) draws on the map at (x,y): prefers the selected unit
 * (subject to selected_visible's blink-off hide), else highest id — except
 * on a colony tile (map_tile_has_city), which never shows a non-selected
 * garrison unit at all, only the active/selected one while it's actually
 * visible. -1 = nothing drawn. Exposed (not just used internally by
 * units_render_on_map) so this rule is directly testable without a
 * framebuffer/sprite sheet.
 */
int units_top_on_map_tile(
  const ColonizeUnitPool* pool,
  int x,
  int y,
  bool selected_visible,
  const ColonizeWorldMap* map
);

/* selected_visible: when false, hide the selected unit (blink off frame). */
void units_render_on_map(
  const ColonizeUnitPool* pool,
  const ColonizeSpriteSheet* nation_sheet,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y,
  bool selected_visible,
  const ColonizeWorldMap* fog_map, /* nullable — skip tiles unseen by fog_nation */
  int fog_nation,
  const ColonizePalette* active_palette
);


/*
 * Split into three focused headers below (movement/pathing, combat, cargo);
 * kept here so existing includers of "core/units.h" keep compiling unchanged.
 * New/narrowed includers should pick the specific header(s) they need.
 */
#include "core/units_move.h"
#include "core/units_combat.h"
#include "core/units_cargo.h"

#endif
