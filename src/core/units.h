#ifndef COLONIZE_UNITS_H
#define COLONIZE_UNITS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

/*
 * Apply pending treasure ransom CHOICE (AI_POPUP_TAG_COMBAT_RANSOM).
 * Accept (choice_id==1) credits payload gold to nation_a; Refuse credits 0.
 * Returns true if the tag was handled.
 */
bool units_combat_apply_ransom_popup(
  ColonizeCol1Save* col1,
  const AiPopupState* popups
);

/*
 * Human-facing colony capture / burn GAME.TXT OKs (@CAPTURED* / @BURNED*).
 * plunder_gold: known cargo sum or 0 (uses CAPTURED3 / CAPTURED2 when 0).
 */
void units_combat_notify_colony_captured(
  const ColonizeCol1Save* col1,
  const ColonizeColony* colony,
  int capturer_nation,
  int plunder_gold
);
void units_combat_notify_colony_burned(
  const ColonizeCol1Save* col1,
  const char* colony_name,
  int victim_nation,
  const char* burner_label
);
/*
 * @BURNED3 "Spies report: …" bystander OK: enqueued for the human when they
 * are neither the burner nor the victim (AI colony burned by natives/rivals
 * while the human watches from elsewhere).
 */
void units_combat_notify_colony_burned_foreign(
  const ColonizeCol1Save* col1,
  const char* colony_name,
  int victim_nation,
  const char* burner_label
);

/*
 * FUN_5fef_0000: pick best defender on tile for engagement toughness.
 * Skips non-combat roles (type.attack==0). Artillery vs Indian attacker ×2.
 * Returns unit id or -1. except_id skips the attacker / mover.
 */
int units_best_defender_at(
  const ColonizeUnitPool* pool,
  const ColonizeCol1Save* col1,
  int x,
  int y,
  int attacker_id,
  int except_id
);

/*
 * Village Attack empty-tile defense (FUN_5fef_1b0e): DOS spawns a temporary
 * Brave (Armed / Mtd. by indian muskets / horse_breeding>24) on the settlement
 * — not by dragging nearby map Braves. Returns temp defender id or -1.
 * Caller must run combat then units_finish_village_temp_defender.
 */
int units_spawn_village_temp_defender(
  ColonizeUnitPool* pool,
  const ColonizeCol1Save* col1,
  int village_x,
  int village_y,
  int indian_nation,
  int attacker_id
);

/*
 * After combat vs a village temp Brave (FUN_5fef_1b0e): always despawn the
 * phantom if still alive; on attacker win, if tribe.population < 2 destroy
 * (+ fallout treasure/convert), else population--. Map Braves on the tile are
 * unrelated — killing them does not drain dwelling population.
 */
void units_finish_village_temp_defender(
  ColonizeUnitPool* pool,
  ColonizeCol1Save* col1,
  ColonizeWorldMap* map,
  int temp_id,
  int attacker_won,
  int attacker_nation,
  int village_x,
  int village_y,
  ColonizeDosRng* rng
);

/*
 * Destroy native village Col1 record at (x,y) if present. Clears map owner
 * nibble to 0xf and remaps unit home_tribe_id. Returns tribe nation_id (>=4)
 * or -1. Does not invent treasure gold.
 */
int col1_destroy_tribe_at(
  ColonizeCol1Save* col1,
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  int x,
  int y
);

/*
 * FUN_5fef_31ea conquest treasure gold (×100 from DOS amount byte).
 * rich_capital: stack-local -0xcc ← ColonizeCol1TribeState.capital.
 * Returns 0 if no treasure / no rng. Cite: viceroy_unpacked.c ~101407–101495.
 */
int units_cortes_conquest_treasure_gold(
  const ColonizeCol1Save* col1,
  int attacker_nation_id,
  ColonizeDosRng* rng,
  int rich_capital
);

/*
 * Post-win fallout stand-in for FUN_5fef_31ea / inlined FUN_5fef_1b0e:
 * If defender was native on a tribe tile and no other same-nation Braves remain
 * on that tile after win, destroy tribe. Before destroy: subjugated convert-join
 * when tribe.mission low-nibble == attacker (PEDIA Sepulveda / @INDIANSLAVES) —
 * threshold 4|8 (±Spanish/Sepulveda/Las Casas), roll dos_rng_range(0,12).
 * Cortes treasure when gold_amount>0 or gold_amount<=0 with Cortes + rng peel.
 * May adjust Indian relation via ai_diplo helpers when col1 is set.
 */
bool units_try_native_settlement_fallout(
  ColonizeCol1Save* col1,
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  int attacker_nation_id,
  int defender_nation_id,
  int tile_x,
  int tile_y,
  int gold_amount,
  ColonizeDosRng* rng
);

/* Reset the two FUN_65dd_0004 session counters (DS:0x1dc6 rumours explored,
 * DS:0x1dc7 Cibola finds) — never saved in DOS either; tests only. */
void units_lcr_reset_session_counters(void);
/*
 * FUN_65dd_0004 thin transcription: Scout on rumour tile clears it via
 * map_clear_rumour, then rolls one of the manual-documented outcomes
 * (nothing / small treasure / chief's gift / burial mounds / trespass anger
 * / survivors join / Fountain of Youth / vanish / Cibola). de Soto (FF 7)
 * restricts the draw to the non-hostile subset (always positive) plus its
 * own reveal-radius bonus. europe/human_nation are optional (NULL/-1 to
 * skip the Fountain-of-Youth Europe-dock sync — AI nations have none).
 */
bool units_resolve_lcr_rumour(
  ColonizeUnitPool* pool,
  int unit_id,
  ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
  ColonizeDosRng* rng,
  EuropeScreen* europe,
  int human_nation
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
  int icon_sprite; /* ICONS.SS 0-based blit index (@UNIT icon is 1-based in NAMES.TXT) */
  int movement;
  int attack;
  int defense;
  int cargo;
  int cost; /* NAMES.TXT @UNIT cost field (Europe purchase uses screenshot gold table) */
  int space; /* NAMES.TXT @UNIT "size" column (DOS 0x5238): ship slots this unit takes; 99 = cannot board */
  ColonizeUnitDomain domain;
} ColonizeUnitType;

typedef struct ColonizeUnit {
  int id;
  int type_index;
  int x;
  int y;
  int moves_left; /* thirds remaining (UNITS_MP_PER_TILE per plains tile) */
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
  int turns_worked; /* COL1 unit+0x16; Brave pulse / labor counter */
  int last_dir; /* DOS unit facing / Col1 facing; 0..7 for AI scoring */
  uint8_t col1_unknown15; /* round-trip; bit7 = ship damaged */
  /*
   * DOS unit+0x07 / Col1 ai_plan. Starter saves use 0x58 ('X') on essentially
   * every unit; spawn defaults to COL1_UNIT_UNKNOWN16_HI_DEFAULT.
   */
  uint8_t col1_ai_plan;
  uint8_t col1_vis_mask; /* DOS nation high nibble (unit byte+3 >> 4); 0x10<<euro */
} ColonizeUnit;

typedef struct ColonizeUnitPool {
  ColonizeUnitType types[COLONIZE_UNIT_TYPES_MAX];
  int type_count;
  ColonizeUnit units[COLONIZE_UNITS_MAX];
  int unit_count;
  int selected_id;
  int next_id;
} ColonizeUnitPool;

bool units_load_types(ColonizeUnitPool* pool, const ColonizeMsgCatalog* names);
void units_reset(ColonizeUnitPool* pool);

int units_find_type(const ColonizeUnitPool* pool, const char* name);
int units_spawn(ColonizeUnitPool* pool, int type_index, int x, int y);
/* Spawn even if the tile already has a unit (COL1 stacks / passengers). */
int units_spawn_allow_stack(ColonizeUnitPool* pool, int type_index, int x, int y);
/* Set nation_id and OR owner euro visibility bit (FUN_1427_0992). */
void units_set_nation(ColonizeUnit* unit, int nation_id);
/*
 * Spawn a Treasure Train at (x,y) for nation_id with COL1 LE16 gold in
 * hold_goods_amount[0]=lo / [1]=hi (same bridge as game_loop / ai_euro cash).
 * Cite: Colonization.pdf Treasure Trains; NAMES "Treasure"; decomp
 * FUN_5fef_31ea post-win native fallout (callers supply gold — no invented
 * rate here). Uses allow_stack (conquest tile may hold the winner).
 * Returns unit id, or -1.
 */
int units_spawn_treasure_train(
  ColonizeUnitPool* pool,
  int x,
  int y,
  int nation_id,
  int gold
);
/*
 * FUN_3844_0004 EOT treasure tick: Treasure on map not on own Euro colony
 * increments turns_worked (COL1 unit+0x16); after >8 turns despawn. On own
 * colony tile resets counter to 0. Returns number of Treasures removed.
 * Optional status receives a short line when any despawn. Cite:
 * FUNCTION_CATALOG FUN_3844_0004; Colonization.pdf Treasure Trains.
 */
int units_tick_treasure_outside_colony(
  ColonizeUnitPool* pool,
  const ColonizeColonyPool* colonies,
  int nation_id,
  char* status,
  size_t status_size
);
/*
 * FUN_3844_00f2 ship-build ready: Col1 types 0x0d..0x12 with +0x3148 bit7
 * (Linux col1_unknown15 bit7; same bit as ship_damaged for Frigate 0x0b —
 * construction gate excludes 0x0b). +1 turns_worked (+2 on any colony tile);
 * threshold = type.defense (DOS 0x5235 = NAMES @UNIT combat).
 * Clears bit7 on complete; human status line; *want_europe_open=1 if finished
 * off-colony. Returns ships completed. Cite: nation_eot_ship_spawn.md §A.
 */
int units_tick_ship_build_ready(
  ColonizeUnitPool* pool,
  const ColonizeColonyPool* colonies,
  int nation_id,
  int human_nation,
  char* status,
  size_t status_size,
  int* want_europe_open
);
/*
 * Drydock repair: clear combat-damage bit7 for finished ships on own Drydock
 * colony. Construction (turns_worked < defense thresh) stays on ship-build tick.
 * Returns ships repaired. ai_popups/messages optional — human repair emits @REFIT.
 * Cite: building_production.md; combat.md fort bit7.
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
);
/*
 * Cortes free king galleon stand-in: each Treasure of nation on an own coastal
 * colony → europe_cash_treasure (tax = Crown share) + despawn. Cite: fandom
 * Hernan Cortes; GAME.TXT @KINGGALLEON3. Syncs col1 nation gold. Returns
 * number cashed. PARK: KINGGALLEON2 / voyage chrome.
 */
int units_cortes_cash_coastal_treasures(
  ColonizeUnitPool* pool,
  ColonizeColonyPool* colonies,
  ColonizeWorldMap* map,
  EuropeScreen* europe,
  ColonizeCol1Save* col1,
  int nation_id
);

/*
 * FUN_5fef_1908 Crown share % for the King's Galleon transport offer:
 * Cortes (FF 10) → current tax rate; otherwise max((difficulty+10)*5, 2*tax);
 * both capped at 90. DS strings "KINGGALLEON"+"3"/"2" at 0x1bed/0x1bf9/0x1bfb.
 */
int units_king_galleon_share_pct(const ColonizeCol1Save* col1, int nation_id);

/*
 * FUN_465b_0000 trigger + FUN_5fef_1908 body, human nation only (DOS gates on
 * DS:0x543f == 0): each own Treasure standing on an own coastal colony tile.
 * WoI declared → cash full value at once (@CASHTREASURE). Else, if the nation
 * owns a Galleon and lacks Cortes → no offer (it can ship it itself). Else
 * enqueue the @KINGGALLEON3 (Cortes) / @KINGGALLEON2 CHOICE with payload =
 * treasure unit id; the apply step below does the cash. Returns the number
 * of treasures cashed or offered. DOS runs this on the move onto the tile;
 * Linux runs it at human turn end (same place the Cortes auto-cash lived).
 */
int units_king_galleon_offer_coastal_treasures(
  ColonizeUnitPool* pool,
  const ColonizeColonyPool* colonies,
  const ColonizeWorldMap* map,
  EuropeScreen* europe,
  ColonizeCol1Save* col1,
  int nation_id,
  AiPopupState* popups,
  const ColonizeMsgCatalog* game_txt
);

/*
 * Apply the pending AI_POPUP_TAG_KING_GALLEON result: choice 1 → Crown share
 * to nation.royal_money (DOS nation+0x22), remainder to gold, @LOOTCASH
 * notify, Treasure despawned. Refuse/cancel → Treasure stays. Returns true
 * when the result was for this tag (consumed).
 */
/*
 * Fountain of Youth (FUN_65dd_0004 case 1): DOS runs the Recruit picker
 * FUN_38fd_4884(1,0) eight times — free passage, player picks among the 3
 * pool slots each time. Enqueue one @RECRUIT CHOICE (payload = picks left);
 * the apply hook recruits the chosen slot and chains the next pick.
 */
void units_fountain_youth_enqueue_pick(
  EuropeScreen* europe, AiPopupState* popups, const ColonizeMsgCatalog* game_txt, int human,
  int remaining
);
/*
 * 5e52 Brewster branch → FUN_38fd_4884(0,1): @RECRUITCHOOSE with the 3 pool
 * names (%COUNTRY = nation, %STRING0 = Europe), passage 0. Apply moves the
 * chosen pool entry to the docks, zeroes crosses and mirrors it as the
 * Europe-map unit; cancel leaves everything (DOS re-asks next turn).
 */
void units_brewster_enqueue_pick(
  EuropeScreen* europe, AiPopupState* popups, const ColonizeMsgCatalog* game_txt, int human
);
bool units_brewster_apply_popup(
  EuropeScreen* europe, AiPopupState* popups, ColonizeUnitPool* units
);
bool units_fountain_youth_apply_popup(
  EuropeScreen* europe, AiPopupState* popups, const ColonizeMsgCatalog* game_txt
);
bool units_king_galleon_apply_popup(
  ColonizeUnitPool* pool,
  EuropeScreen* europe,
  ColonizeCol1Save* col1,
  AiPopupState* popups,
  const ColonizeMsgCatalog* game_txt
);
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
bool units_reveal_sight(
  ColonizeWorldMap* map,
  ColonizeUnitPool* pool,
  ColonizeColonyPool* colonies,
  const ColonizeUnit* u,
  const ColonizeCol1Save* col1
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
 * Enter-probe outcomes (DOS FUN_4720 / FUN_465b shaped). See docs/move_enter.md.
 * units_can_enter is true only for OK / DOCK (pathfinding / Go-To).
 */
typedef enum ColonizeEnterReason {
  COLONIZE_ENTER_OK = 0,
  COLONIZE_ENTER_DOCK = 1,
  COLONIZE_ENTER_LANDFALL = 2,
  COLONIZE_ENTER_COMBAT_LAND = 3,
  COLONIZE_ENTER_COMBAT_NAVAL = 4,
  COLONIZE_ENTER_BOUNCE_FOREIGN = 5,
  COLONIZE_ENTER_BOUNCE_PEACE = 6,
  COLONIZE_ENTER_BLOCKED_DOMAIN = 7,
  COLONIZE_ENTER_BLOCKED_EDGE = 8,
  COLONIZE_ENTER_BLOCKED_HS_SAIL = 9, /* 4720 reason 5: HS east without sail */
  COLONIZE_ENTER_VILLAGE_ILLEGAL = 10,
  COLONIZE_ENTER_NO_MP = 11,
  COLONIZE_ENTER_BLOCKED = 12,
  COLONIZE_ENTER_BOARD = 13, /* land → ocean tile with own ship that has room */
  COLONIZE_ENTER_VILLAGE_SHIP = 14 /* ship → native village (not landfall); 4528 abort */
} ColonizeEnterReason;

ColonizeEnterReason units_enter_probe(
  const ColonizeUnitPool* pool,
  int type_index,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int mover_id,
  const ColonizeColonyPool* colonies
);
/* Last reason from units_enter_probe / units_try_move (0 if none). */
ColonizeEnterReason units_last_enter_reason(void);
/* Short player status for a probe reason (never NULL). */
const char* units_enter_reason_status(ColonizeEnterReason reason);

bool units_can_enter(
  const ColonizeUnitPool* pool,
  int type_index,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int mover_id,
  const ColonizeColonyPool* colonies
);
/*
 * Movement points are DOS thirds (FUN_465b_0000 cost head; DS:0x5234 unit
 * table = @UNIT movement * 3, ships +3 with Magellan): a plains step costs
 * 3, a road/colony pair or cardinal minor-river pair costs 1, an ocean tile
 * costs 3. `moves_left` holds thirds remaining. Native Braves (ai.c) keep
 * their own bookkeeping (moves_left = DOS spent thirds, max 3).
 */
#define UNITS_MP_PER_TILE 3
/* @UNIT movement * 3 (DOS FUN_1427_065a base). */
int units_type_max_mp(const ColonizeUnitType* type);
/* Per-unit max MP in thirds: type base + 3 for ships when the nation has Magellan. */
int units_max_mp(const ColonizeUnitPool* pool, int unit_id);
/* "1", "2/3", "1 1/3" — DOS panel style. */
void units_format_mp(int thirds, char* out, size_t out_size);
/* Destination MP cost in thirds (DOS 465b cost head); sea units always 3. */
int units_move_cost(
  const ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y
);
/*
 * DOS FUN_465b gate (non-combat, deterministic half): afford if cost <= moves_left,
 * OR the unit still has its full allotment (spent MP == 0). Partial overspend is
 * decided only in units_try_move via dos_rng_range(1, cost) <= remaining.
 */
bool units_can_afford_move_cost(
  const ColonizeUnitPool* pool,
  int unit_id,
  int cost
);
bool units_try_move(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y,
  const ColonizeColonyPool* colonies,
  ColonizeDosRng* rng /* nullable; required for partial overspend / combat rolls */
);

/*
 * Observe successful on-map tile moves. The callback is process-global, like
 * the combat/FF context hooks, and is normally installed only by the
 * interactive game loop while AI turns are being shown.
 */
typedef void (*ColonizeUnitsMoveWatchFn)(
  void* user,
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int unit_id,
  int from_x,
  int from_y,
  int to_x,
  int to_y
);
void units_set_move_watch(ColonizeUnitsMoveWatchFn fn, void* user);

/*
 * Land combat (FUN_157e / FUN_5fef_1b0e peel): attacker base×8 (004a mode 1);
 * defender engagement (015e: colony/village/terrain/fortify). Probability =
 * atk/(atk+def). Optional Combat Analysis presenter after strengths, before
 * roll. Winner stays;
 * loser despawned. Naval / mixed: no fight.
 * When col1 is non-NULL and winner nation owns Washington (PEDIA/wiki George
 * Washington; docs/fandom_col1994.md: non-veteran soldiers/dragoons who win
 * always upgrade), promote winner name/type like 1eca. col1 may be NULL (no FF
 * promote). Returns true if attacker wins.
 */
bool units_resolve_land_combat_ff(
  ColonizeUnitPool* pool,
  int attacker_id,
  int defender_id,
  ColonizeDosRng* rng,
  const ColonizeCol1Save* col1
);

/*
 * AI/contact wrapper: same as units_resolve_land_combat_ff with col1 from
 * units_set_ff_col1 (g_units_ff_col1). Callers that always passed NULL missed
 * Washington promote; turn_refresh_moves_for_nation sets the global.
 */
bool units_resolve_land_combat(
  ColonizeUnitPool* pool,
  int attacker_id,
  int defender_id,
  ColonizeDosRng* rng
);

/*
 * Transfer commodity holds from loser ship into winner before naval despawn
 * (FUN_5fef_016c-shaped hold plunder). Passengers are not transferred.
 * Returns total goods amount moved into winner holds (0 if none/full).
 */
int units_plunder_ship_holds(ColonizeUnitPool* pool, int winner_id, int loser_id);

/*
 * Naval combat: FUN_157e_004a for both sides (damage/holds/Drake). Same roll
 * shape as land; ships only. Winner keeps the tile; loser despawned after hold
 * plunder into winner. Optional Combat Analysis after strengths, before roll.
 */
bool units_resolve_naval_combat_ff(
  ColonizeUnitPool* pool,
  int attacker_id,
  int defender_id,
  ColonizeDosRng* rng,
  const ColonizeCol1Save* col1
);

/*
 * AI/king wrapper: same as units_resolve_naval_combat_ff with col1 from
 * units_set_ff_col1 (g_units_ff_col1). Callers that always passed NULL missed
 * Drake privateer *3/2; turn_refresh_moves_for_nation sets the global.
 * Cite: PEDIA/wiki Francis Drake; founding_fathers.c FF_FRANCIS_DRAKE.
 */
bool units_resolve_naval_combat(
  ColonizeUnitPool* pool,
  int attacker_id,
  int defender_id,
  ColonizeDosRng* rng
);

/*
 * Coastal Fort/Fortress naval fire strength (FUN_364b_03f6).
 * Fort: 4*(1+arty); Fortress: 8*(1+arty). Artillery/Cannon on colony tile
 * (owner nation). Stockade alone → 0. Cite: decomp local_12*local_c*4;
 * fandom Fort/Fortress. PARK: ship-slow formula.
 */
int units_coastal_fort_attack_strength(
  const ColonizeColonyPool* colonies,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units
);

/*
 * EOT pulse: each Fort/Fortress colony fires on adjacent ocean ships that are
 * at war with the colony owner, or Privateers (peace ignored). Fort win →
 * sink ship (no hold plunder). Fort loss → ship-slow (moves_left=0).
 * Optional human status line. Returns ships sunk. Cite: FUN_364b_03f6.
 */
int units_coastal_fort_fire_pulse(
  ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const ColonizeWorldMap* map,
  const ColonizeCol1Save* col1,
  ColonizeDosRng* rng,
  int human_nation,
  char* status,
  size_t status_size
);

/* After units_try_move: 0 none, 1 attacker won, -1 attacker lost. */
int units_last_combat_outcome(void);

/* @ORDERS indices (NAMES.TXT) + Col1 AI order bytes seen in saves. */
#define UNITS_ORDER_NONE 0
#define UNITS_ORDER_SENTRY 1
#define UNITS_ORDER_TRADE_ROUTE 2
#define UNITS_ORDER_GOTO 3
#define UNITS_ORDER_FORTIFY 5
#define UNITS_ORDER_FORTIFIED 6
#define UNITS_ORDER_BUILD_COLONY 7
#define UNITS_ORDER_CLEAR_PLOW 8
#define UNITS_ORDER_BUILD_ROAD 9
#define UNITS_ORDER_AI_SAIL 11 /* Euro AI ship course (TURN fixtures) */
#define UNITS_ORDER_AI_MOVE 12 /* Euro AI coastal / land course */
#define UNITS_ORDER_FOLLOW 13 /* Stick to another unit (Brave escort / AI) */
#define UNITS_GOTO_NONE 0xFF

/* True if orders byte means "follow goto_x/y". */
static inline bool units_orders_follow_goto(int orders) {
  return orders == UNITS_ORDER_GOTO || orders == UNITS_ORDER_AI_SAIL ||
         orders == UNITS_ORDER_AI_MOVE || orders == UNITS_ORDER_TRADE_ROUTE;
}

/* True if unit is ordered to stick to another unit id. */
static inline bool units_orders_is_follow(int orders) {
  return orders == UNITS_ORDER_FOLLOW;
}

void units_clear_orders(ColonizeUnitPool* pool, int unit_id);
/*
 * Set sentry / fortify / fortified. Clears goto. Land units only for fortify.
 * Sentry/fortify spend remaining MP (moves_left = 0). Returns false if invalid.
 */
bool units_set_orders(ColonizeUnitPool* pool, int unit_id, int orders);
/* Fortify: orders=FORTIFY, spend MP; next nation refresh → FORTIFIED. */
bool units_order_fortify(ColonizeUnitPool* pool, int unit_id);
/*
 * Ship Anchor (ORDERS 2nd Fortify / GAME.TXT @SHIPOPTIONS). Sea unit at own
 * colony tile or adjacent sea → FORTIFY (overnight → FORTIFIED). Land rejects.
 */
bool units_order_anchor(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeColonyPool* colonies
);
/* Sentry on map (or already-aboard). Spends MP. */
bool units_order_sentry(ColonizeUnitPool* pool, int unit_id);
/* Begin Trade Route (@ORDERS index 2). Clears goto; spends remaining MP. */
bool units_order_trade_route(ColonizeUnitPool* pool, int unit_id);
/* Despawn unit (map disband). False if missing. */
bool units_disband(ColonizeUnitPool* pool, int unit_id);
/*
 * Dump first non-empty commodity hold overboard (ORDERS Dump Cargo Overboard).
 * Transport only. Returns amount dumped (0 if none/invalid).
 */
int units_dump_cargo_overboard(
  ColonizeUnitPool* pool,
  int unit_id,
  int* out_cargo_type,
  int* out_amount
);
/*
 * Thin Pillage (ORDERS): land military (attack>0) on foreign Euro colony →
 * destroy up to 100 of richest warehouse stock (not Food); on non-colony tile
 * with plow/road → clear those improvements. Spends remaining MP.
 * Cite: MENU.TXT @ORDERS Pillage; full 2b5a mega-dispatch body still thin.
 */
bool units_pillage(
  ColonizeUnitPool* pool,
  int unit_id,
  ColonizeWorldMap* map,
  ColonizeColonyPool* colonies,
  char* err,
  size_t err_size
);
/* Wake sentry/fortified/fortify-in-progress and restore full MP. */
bool units_wake(ColonizeUnitPool* pool, int unit_id);
/* True if unit skips selection until woken (sentry or fortified). */
bool units_orders_skip_turn(const ColonizeUnit* unit);

/* Set Go-To order (does not move); returns false if unit/dest invalid. */
bool units_set_goto(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y,
  const ColonizeColonyPool* colonies
);
/*
 * Order unit to stick to target_unit_id (UNITS_ORDER_FOLLOW).
 * Clears tile goto. Both units must be active and on-map; same domain preferred
 * (sea follows sea, land follows land). Returns false if invalid.
 * Cite: Brave escort / FUN_4d56_14fe needs follow-unit orders (tile goto alone is not enough).
 */
bool units_follow_unit(ColonizeUnitPool* pool, int unit_id, int target_unit_id);
/*
 * One step toward the follow target's current tile (retarget each call).
 * Clears FOLLOW if target missing/inactive. rng may be NULL.
 */
bool units_advance_follow_one_step(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  ColonizeDosRng* rng
);
/*
 * Next adjacent step toward goto (DOS FUN_6662 tiers: sign-step / cost flood / BFS).
 * Writes (out_x,out_y); returns false if stuck or already there. rng may be
 * NULL (disables the anti-backtrack wiggle reroll; deterministic geometry
 * still runs).
 */
bool units_next_goto_step(
  const ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  ColonizeDosRng* rng,
  int* out_x,
  int* out_y
);
/* One adjacent step toward goto (or clear orders if arrived). rng may be NULL. */
bool units_advance_goto_one_step(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  ColonizeDosRng* rng
);
/* Walk until MP exhausted, arrived, or blocked. Clears orders on arrival. */
bool units_advance_goto(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  ColonizeDosRng* rng
);
/* One step for every Go-To unit that can move; returns how many stepped. */
int units_advance_all_goto_one_step(
  ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies
);
/* Advance every unit with Go-To until stuck; returns how many took at least one step. */
int units_advance_all_goto(
  ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies
);

bool units_is_pioneer(const ColonizeUnitPool* pool, int unit_id);
/*
 * Start/continue pioneer terrain work (DOS FUN_479b_01a6 / 0526).
 * Clear Forest and Plow Fields are separate jobs (P on forest clears only;
 * P on open land plows). Road is R. Each job takes terr_cost[+2 for
 * clear/plow] turns (Hardy Pioneer halves); tools −20 on completion.
 * First call sets orders + one work-tick; further ticks via
 * units_pioneer_work_tick / turn_refresh.
 */
bool units_pioneer_plow(
  ColonizeUnitPool* pool,
  int unit_id,
  ColonizeWorldMap* map,
  char* err,
  size_t err_size,
  ColonizeColonyPool* colonies,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
);
bool units_pioneer_road(
  ColonizeUnitPool* pool,
  int unit_id,
  ColonizeWorldMap* map,
  char* err,
  size_t err_size,
  ColonizeColonyPool* colonies,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
);
/* One work-tick for CLEAR_PLOW / BUILD_ROAD orders. Returns true if unit still
 * has that order (in progress or just started). colonies/ai_popups/messages
 * optional — clear-forest completion grants lumber + @CLEARCUT when set. */
bool units_pioneer_work_tick(
  ColonizeUnitPool* pool,
  int unit_id,
  ColonizeWorldMap* map,
  char* err,
  size_t err_size,
  ColonizeColonyPool* colonies,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
);

/* True for high-seas / sea-lane tiles (terrain index 26). */
bool units_on_high_seas(const ColonizeWorldMap* map, int x, int y);
bool units_find_water_tile(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int occupant_id,
  int* out_x,
  int* out_y
);
bool units_find_high_seas_tile(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int* out_x,
  int* out_y
);
/*
 * FUN_48d3_048e + 0434 — expanding spiral from (start_x,start_y) for a High Seas
 * tile (terrain 0x1a) that is empty or owned by nation_id (-1 = empty only).
 * Cite: viceroy_unpacked.c ~77810; move_scoring.md §ocean.
 */
bool units_spiral_place_hs_near(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int nation_id,
  int* out_x,
  int* out_y
);
/* Prefer western rim of eastern high seas near prefer_y — Atlantic approach. */
bool units_find_eastern_high_seas_tile(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int prefer_y,
  int* out_x,
  int* out_y
);

/* Board a land unit onto an adjacent ship. Returns false if capacity/adjacency fails. */
bool units_board(ColonizeUnitPool* pool, int land_unit_id, int ship_id);
/* Board without adjacency check (COL1 import; passenger already stacked on ship tile). */
bool units_board_stacked(ColonizeUnitPool* pool, int land_unit_id, int ship_id);
/*
 * Own ship on (x,y) with free passenger capacity, or -1.
 * Cite: FUN_4720_0006 / 015c land→ocean embark probe.
 * require_galleon: Treasure Trains may only board a Galleon (Colonization.pdf
 * / P7.3) — pass true when the boarding unit's type name contains "Treasure".
 */
int units_find_boardable_ship(
  const ColonizeUnitPool* pool, int x, int y, int nation_id, bool require_galleon
);
/*
 * Board same-nation on-map land units with Sentry on (x,y) onto ship until full.
 * Used when a ship leaves a colony or stacked ocean tile. Returns count boarded.
 */
int units_board_sentries_from_tile(ColonizeUnitPool* pool, int ship_id, int x, int y);
/*
 * Departure pickup (DOS ship-switch quirk): one ascending-id first-come-
 * first-served sweep boarding sentried land units on (x,y) AND passengers
 * riding other own ships still on (x,y), until the departing ship is full.
 * Treasure Trains never transfer to a non-Galleon. Returns units taken.
 */
int units_ship_departure_pickup(ColonizeUnitPool* pool, int ship_id, int x, int y);
/* Unload oldest passenger from ship onto dest (must be enterable land). */
bool units_unload(
  ColonizeUnitPool* pool,
  int ship_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y,
  const ColonizeColonyPool* colonies
);
/* Unload a specific passenger onto dest; charges dest terrain MP (no gift). */
bool units_unload_passenger(
  ColonizeUnitPool* pool,
  int ship_id,
  int pax_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y,
  const ColonizeColonyPool* colonies
);
/* First cargo with moves_left > 0, or -1. */
int units_first_cargo_with_moves(const ColonizeUnitPool* pool, int ship_id);
/*
 * Landfall passenger pick (FUN_4720_015c): prefer moves_left > 0, else oldest
 * cargo (aboard sentry still landfall-eligible — DOS spent==0).
 */
int units_first_landfall_cargo(const ColonizeUnitPool* pool, int ship_id);
/*
 * Pick an adjacent land tile the ship's passengers can enter (8-neighbour).
 * Prefers tiles nearer to (prefer_x, prefer_y) when prefer coords are valid;
 * pass prefer_x < 0 to ignore. Returns false if none.
 */
bool units_pick_landfall_tile(
  const ColonizeUnitPool* pool,
  int ship_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int prefer_x,
  int prefer_y,
  int* out_x,
  int* out_y
);
/*
 * Unload every passenger onto dest (must be enterable land adjacent/same).
 * Wakes sentry cargo. Does not change pool->selected_id. Returns count unloaded.
 * AI beachhead helper — human @LANDFALL Make Landfall unloads one unit only.
 */
int units_landfall_unload_all(
  ColonizeUnitPool* pool,
  int ship_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y,
  const ColonizeColonyPool* colonies
);
/*
 * Colony dock: remove all passengers from the ship onto (x,y), clear sentry.
 * Does not change ship position. Returns number disembarked.
 */
int units_disembark_all(
  ColonizeUnitPool* pool,
  int ship_id,
  int x,
  int y
);

/* Collect on-map units at tile plus cargo of ships there (for stack popup). */
#define UNITS_TILE_STACK_MAX 32
int units_collect_tile_stack(
  const ColonizeUnitPool* pool,
  int x,
  int y,
  int nation_id,
  int* out_ids,
  int out_max
);

int units_ship_capacity(const ColonizeUnitPool* pool, int ship_id);

/* Passenger slots left: capacity − passengers − holds occupied by goods
 * (goods share the slots passengers ride in). */
int units_ship_free_passenger_slots(const ColonizeUnitPool* pool, int ship_id);

/* Ships and wagon trains that can carry commodity holds. */
bool units_is_transport(const ColonizeUnitPool* pool, int unit_id);
/* Number of commodity hold slots (from @UNIT cargo field). */
int units_goods_hold_count(const ColonizeUnitPool* pool, int unit_id);
/*
 * Add goods into an empty hold (or stack into a matching partial hold).
 * amount is clamped to remaining room (max 100 per hold). Returns amount loaded.
 */
int units_load_goods(ColonizeUnitPool* pool, int unit_id, int cargo_type, int amount);
/*
 * Remove goods from one hold index. Writes type/amount unloaded (optional outs).
 * Returns amount unloaded (0 if empty/invalid).
 */
int units_unload_goods_hold(
  ColonizeUnitPool* pool,
  int unit_id,
  int hold_index,
  int* out_cargo_type,
  int* out_amount
);
/* First non-empty goods hold index, or -1. */
int units_first_goods_hold(const ColonizeUnitPool* pool, int unit_id);
/* Snapshot passenger type indices (for Europe harbor transfer). */
int units_export_cargo_types(
  const ColonizeUnitPool* pool,
  int ship_id,
  int* out_types,
  int out_max
);
/*
 * Despawn ship and all passengers; fills passenger type list and optional
 * commodity hold arrays for Europe harbor transfer.
 */
bool units_despawn_ship_with_cargo(
  ColonizeUnitPool* pool,
  int ship_id,
  int* out_type_index,
  char* out_name,
  size_t out_name_size,
  int* out_cargo_types,
  int* out_cargo_count,
  int cargo_max,
  int* out_hold_goods_type,
  int* out_hold_goods_amount,
  int hold_max
);
/* Spawn ship at (x,y); recreate passengers and optional commodity holds. */
int units_spawn_ship_with_cargo(
  ColonizeUnitPool* pool,
  int ship_type_index,
  int x,
  int y,
  const int* cargo_types,
  int cargo_count,
  const int* hold_goods_type,
  const int* hold_goods_amount
);

void units_end_turn(ColonizeUnitPool* pool);

/* NAMES.TXT @JOB indices used for unit skills (COL1 profession byte). */
#define UNITS_JOB_COLONIST 19 /* Free Colonists */
#define UNITS_JOB_PIONEER 20  /* Hardy Pioneers */
#define UNITS_JOB_SOLDIER 21  /* Veteran Soldiers */
#define UNITS_JOB_SCOUT 22    /* Seasoned Scouts */
#define UNITS_JOB_DRAGOON 23  /* Veteran Dragoons */
#define UNITS_JOB_MISSIONARY 24 /* Jesuit Missionaries expert; plain bless uses NONE */
#define UNITS_JOB_NONE 28     /* no expert skill (plain Pioneer/Soldier) */

/* Equipped map/fence icons (ICONS.SS); expert variants when profession matches. */
#define UNITS_ICON_PIONEER 73
#define UNITS_ICON_SOLDIER 74
#define UNITS_ICON_SCOUT 75
#define UNITS_ICON_DRAGOON 76
#define UNITS_ICON_HARDY_PIONEER 101
#define UNITS_ICON_VETERAN_SOLDIER 102
#define UNITS_ICON_SEASONED_SCOUT 103
#define UNITS_ICON_VETERAN_DRAGOON 104
/* Working inside a colony (unequipped citizen sprites). */
#define UNITS_ICON_HARDY_PIONEER_WORK 58
#define UNITS_ICON_VETERAN_SOLDIER_WORK 59

#define UNITS_EQUIP_MUSKETS 50
#define UNITS_EQUIP_HORSES 50
#define UNITS_EQUIP_TOOLS_MAX 100
#define UNITS_EQUIP_TOOLS_STEP 20

/* Human starter: Caravel (Dutch Merchantman) on eastern high seas with Pioneer+Soldier. */
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
 * Skills from difficulty/nation (Discoverer/Explorer / French Hardy / Spanish Veteran).
 * Returns ship unit id or -1.
 */
int units_spawn_euro_starter_fleet(
  ColonizeUnitPool* pool,
  int nation_id,
  int difficulty,
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
 * the on-map UNITS_ICON_* sprites. -1 if that profession has no dedicated
 * portrait (Expert Teachers, Veteran Dragoons).
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
bool units_type_has_profession_slot(int type_index);

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
  const ColonizeColonyPool* colonies,
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

#endif
