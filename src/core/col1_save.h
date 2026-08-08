#ifndef COLONIZE_COL1_SAVE_H
#define COLONIZE_COL1_SAVE_H

/*
 * Original Sid Meier's Colonization (DOS 3.0) COLONY##.SAV layout.
 *
 * Layout derived from the community reverse-engineering in
 * hegemogy/viceroy (savegame.h), hegemogy/Colonization-SAV-files
 * (Format.md), and pavelbel/smcol_saves_utility — cross-checked
 * against section sizes in Format.md.
 *
 * File order:
 *   head (158) + player[4] (208) + other (24)           = 390 prefix
 *   colony[colony_count]   @ 202 bytes each
 *   unit[unit_count]       @  28 bytes each
 *   nation[4]              @ 316 bytes each (= 1264)
 *   tribe[tribe_count]     @  18 bytes each   (Indian villages)
 *   indian[8]              @  78 bytes each   (= 624)
 *   stuff                  @ 727 bytes (= 33 discrete DS writes in FUN_75c2_0288)
 *   map.tile/mask/path/seen @ map_w * map_h each (standard 58x72)
 *   post_map               @ 614 (= sea/land connectivity 2×270 + tallies + tail)
 *   trade_route[12]        @  74 bytes each (= 888)
 *
 * Post-map is NOT “28×18 mystery records”: FUN_67f4_0088 builds two 15×18
 * (pitch 18) neighbor-bitmask planes at DS:0x86f6 (sea) and DS:0x85e8 (land);
 * FUN_75c2_0288 writes them then continent tallies + a 10-byte tail.
 *
 * Multi-byte integers are little-endian. Bitfields assume GCC/Clang
 * LSB-first packing on little-endian hosts (matches DOS saves).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COLONIZE_COL1_SIG "COLONIZE"
/* DOS writes signature as C-string then 0x1A (FUN_1b2c_0040); version follows. */
#define COLONIZE_COL1_SIG_EOF 0x1Au
/* Colonization 3.0 save format version (DS:0x81a / FUN_75c2_0840). */
#define COLONIZE_COL1_SAVE_VERSION 73u
#define COLONIZE_COL1_PREFIX_SIZE 390u
#define COLONIZE_COL1_HEAD_SIZE 158u
#define COLONIZE_COL1_PLAYER_SIZE 52u
#define COLONIZE_COL1_OTHER_SIZE 24u
#define COLONIZE_COL1_COLONY_SIZE 202u
#define COLONIZE_COL1_UNIT_SIZE 28u
/* DOS unit ai_plan default seen on virtually all units in original starters. */
#define COL1_UNIT_UNKNOWN16_HI_DEFAULT 0x58u
#define COLONIZE_COL1_NATION_SIZE 316u
#define COLONIZE_COL1_NATION_COUNT 4u
#define COLONIZE_COL1_TRIBE_SIZE 18u
#define COLONIZE_COL1_INDIAN_SIZE 78u
#define COLONIZE_COL1_INDIAN_COUNT 8u
#define COLONIZE_COL1_STUFF_SIZE 727u
#define COLONIZE_COL1_MAP_W_STD 58u
#define COLONIZE_COL1_MAP_H_STD 72u
/* Post-map (after seen[]): FUN_75c2_0288 / FUN_67f4_0088. */
#define COLONIZE_COL1_CONNECT_PLANE_W 15u
#define COLONIZE_COL1_CONNECT_PLANE_H 18u
#define COLONIZE_COL1_CONNECT_PLANE_SIZE 270u /* 15×18 */
#define COLONIZE_COL1_POST_MAP_SIZE 614u
/* Legacy aliases: unknown_e|f was a conventional 504+110 split of post_map. */
#define COLONIZE_COL1_UNKNOWN_E_SIZE 504u
#define COLONIZE_COL1_UNKNOWN_F_SIZE 110u
#define COLONIZE_COL1_TRADE_ROUTE_SIZE 74u
#define COLONIZE_COL1_TRADE_ROUTE_COUNT 12u
#define COLONIZE_COL1_CARGO_TYPES 16u
#define COLONIZE_COL1_FF_COUNT 25u
#define COLONIZE_COL1_COLONY_POP_MAX 32u

#pragma pack(push, 1)

typedef struct ColonizeCol1Tut1 {
  uint8_t nr13 : 1;
  uint8_t nr14 : 1;
  uint8_t unknown01 : 1;
  uint8_t nr15 : 1;
  uint8_t nr16 : 1;
  uint8_t nr17 : 1;
  uint8_t unknown02 : 1;
  uint8_t nr19 : 1;
} ColonizeCol1Tut1;

typedef struct ColonizeCol1GameOptions {
  uint16_t unused01 : 7; /* DOS 0x5382 bits used for scenario/WoI/REF — not pure pad */
  uint16_t tutorial_hints : 1;
  uint16_t water_color_cycling : 1;
  uint16_t combat_analysis : 1;
  uint16_t autosave : 1;
  uint16_t end_of_turn : 1;
  uint16_t fast_piece_slide : 1;
  uint16_t cheats_enabled : 1; /* DS:0x5383 bit5; Alt-WIN unlock (was unused02) */
  uint16_t show_foreign_moves : 1;
  uint16_t show_indian_moves : 1;
} ColonizeCol1GameOptions;

typedef struct ColonizeCol1ColonyReportOptions {
  uint16_t labels_on_cargo_and_terrain : 1;
  uint16_t labels_on_buildings : 1;
  uint16_t report_new_cargos_available : 1;
  uint16_t report_inefficient_government : 1;
  uint16_t report_tools_needed_for_production : 1;
  uint16_t report_raw_materials_shortages : 1;
  uint16_t report_food_shortages : 1;
  uint16_t report_when_colonists_trained : 1;
  uint16_t report_sons_of_liberty_membership : 1;
  uint16_t report_rebel_majorities : 1;
  uint16_t unused03 : 6;
} ColonizeCol1ColonyReportOptions;

typedef struct ColonizeCol1Tut2 {
  uint8_t howtowin : 1;
  uint8_t background_music : 1;
  uint8_t event_music : 1;
  uint8_t sound_effects : 1;
  uint8_t nr1 : 1;
  uint8_t unused04 : 1;
  uint8_t nr3 : 1;
  uint8_t nr4 : 1;
} ColonizeCol1Tut2;

typedef struct ColonizeCol1Tut3 {
  uint8_t nr5 : 1;
  uint8_t nr6 : 1;
  uint8_t nr7 : 1;
  uint8_t nr8 : 1;
  uint8_t nr9 : 1;
  uint8_t nr10 : 1;
  uint8_t nr11 : 1;
  uint8_t nr12 : 1;
} ColonizeCol1Tut3;

typedef struct ColonizeCol1EventFlags {
  uint16_t discovery_of_the_new_world : 1;
  uint16_t building_a_colony : 1;
  uint16_t meeting_the_natives : 1;
  uint16_t the_aztec_empire : 1;
  uint16_t the_inca_nation : 1;
  uint16_t discovery_of_the_pacific_ocean : 1;
  uint16_t entering_indian_village : 1;
  uint16_t the_fountain_of_youth : 1;
  uint16_t cargo_from_the_new_world : 1;
  uint16_t meeting_fellow_europeans : 1;
  uint16_t colony_burning : 1;
  uint16_t colony_destroyed : 1;
  uint16_t indian_raid : 1;
  uint16_t woodcut14 : 1;
  uint16_t woodcut15 : 1;
  uint16_t woodcut16 : 1;
} ColonizeCol1EventFlags;

typedef struct ColonizeCol1Head {
  char sig_colonize[9]; /* "COLONIZE\0" */
  uint8_t sig_eof; /* 0x1A after signature (FUN_1b2c_0040) */
  uint16_t save_version; /* COL 3.0 = 73; FUN_75c2_0840 vs DS:0x81a */
  uint16_t map_size_x; /* typically 58 (visible 56 + border) */
  uint16_t map_size_y; /* typically 72 (visible 70 + border) */
  ColonizeCol1Tut1 tut1;
  uint8_t unknown03;
  ColonizeCol1GameOptions game_options;
  ColonizeCol1ColonyReportOptions colony_report_options;
  ColonizeCol1Tut2 tut2;
  ColonizeCol1Tut3 tut3;
  uint8_t unknown39[2];
  uint16_t year;
  uint16_t autumn; /* non-zero if autumn */
  uint16_t turn;
  /* DS:0x5390 — 0=Move Pieces, 1=View Pieces (FUN_2b5a_0902 / 0e52). */
  uint16_t map_mode;
  uint16_t active_unit;
  uint16_t nation_turn; /* DS:0x5394 — active AI/turn nation */
  uint16_t curr_nation_map_view; /* DS:0x5396 */
  uint16_t human_player; /* DS:0x5398 */
  uint16_t tribe_count;
  uint16_t unit_count;
  uint16_t colony_count;
  uint16_t trade_route_count; /* DS:0x53a0 */
  uint16_t show_entire_map; /* DS:0x53a2 — Complete Map cheat / post-win */
  uint16_t fixed_nation_map_view; /* DS:0x53a4; 0xffff = none */
  uint8_t difficulty; /* 0 Discoverer .. 4 Viceroy */
  uint8_t unknown43[2];
  int8_t founding_father[COLONIZE_COL1_FF_COUNT];
  /* DS:0x53c2 / 0x53c4 / 0x53c6 — UI/turn latches (was unknown44[6]). */
  uint16_t turn_loop_running; /* 0x53c2; Esc clears (FUN_2b5a_3104); main loop sets */
  uint16_t map_modal_active; /* 0x53c4; map modal pump gate */
  uint16_t no_unit_selected; /* 0x53c6; View-idle when active_unit < 0 */
  int16_t nation_relation[4];
  int16_t rebel_sentiment_report; /* DS:0x53d0; congress UI 0..100 */
  uint8_t unknown45_pad[8];
  uint16_t expeditionary_force[4]; /* regulars, dragoons, man-o-wars, artillery */
  uint16_t backup_force[4];
  /*
   * DS:0x53ea — DOS price_group_state[16] (FUN_38fd_0058). Linux also overlays
   * king stand-ins in the first bytes (WoI/REF/boycott/merc/congress) — do not
   * treat as pure price groups until stand-ins migrate (ai_king.c).
   */
  union {
    uint8_t unknown46[32];
    uint16_t price_group_state[16];
  };
  ColonizeCol1EventFlags event;
  uint8_t unknown05[2];
} ColonizeCol1Head;

typedef struct ColonizeCol1Player {
  char name[24];
  char country_name[24];
  uint8_t unknown06_lo : 7;
  uint8_t named_new_world : 1; /* bit7 @ player+0x30; discovery one-shot (FUN_4720_049e) */
  uint8_t control; /* 0 player, 1 AI, 2 withdrawn */
  uint8_t founded_colonies;
  uint8_t diplomacy;
} ColonizeCol1Player;

typedef struct ColonizeCol1DurationNibble {
  uint8_t low : 4;
  uint8_t high : 4;
} ColonizeCol1DurationNibble;

typedef struct ColonizeCol1Buildings {
  uint32_t fortification : 3;
  uint32_t armory : 3;
  uint32_t docks : 3;
  uint32_t town_hall : 3;
  uint32_t schoolhouse : 3;
  uint32_t warehouse : 2;
  uint32_t stables : 1;
  uint32_t custom_house : 1;
  uint32_t printing_press : 2;
  uint32_t weavers_house : 3;
  uint32_t tobacconists_house : 3;
  uint32_t rum_distillers_house : 3;
  uint32_t capitol : 2;
  uint16_t fur_traders_house : 3;
  uint16_t carpenters_shop : 2;
  uint16_t church : 2;
  uint16_t blacksmiths_house : 3;
  uint16_t unused05 : 6;
} ColonizeCol1Buildings;

typedef struct ColonizeCol1CustomHouse {
  uint16_t food : 1;
  uint16_t sugar : 1;
  uint16_t tobacco : 1;
  uint16_t cotton : 1;
  uint16_t furs : 1;
  uint16_t lumber : 1;
  uint16_t ore : 1;
  uint16_t silver : 1;
  uint16_t horses : 1;
  uint16_t rum : 1;
  uint16_t cigars : 1;
  uint16_t cloth : 1;
  uint16_t coats : 1;
  uint16_t trade_goods : 1;
  uint16_t tools : 1;
  uint16_t muskets : 1;
} ColonizeCol1CustomHouse;

/* Colony +0x1c — FUN_364b_0688 / 0114 / founding paths. */
typedef struct ColonizeCol1ColonyFlags {
  uint8_t ref_landing : 1; /* 0x01 — REF landing target */
  uint8_t sol_100 : 1; /* 0x02 — SoL ≥ 100 latch */
  uint8_t sol_50 : 1; /* 0x04 — SoL ≥ 50 latch */
  uint8_t starvation : 1; /* 0x08 — food shortfall latch */
  uint8_t small_colony_ai : 1; /* 0x10 — AI pop < 10 */
  uint8_t wagon_train : 1; /* 0x20 — wagon in colony */
  uint8_t coastal : 1; /* 0x40 — coastal / docks founding path */
  uint8_t build_complete : 1; /* 0x80 — construction-just-finished chrome */
} ColonizeCol1ColonyFlags;

typedef struct ColonizeCol1Colony {
  uint8_t x;
  uint8_t y;
  char name[24];
  uint8_t nation_id;
  uint8_t unknown08_1b; /* +0x1b; census clears low bits (& 0xfc) */
  ColonizeCol1ColonyFlags flags; /* +0x1c */
  uint8_t unknown08_1d; /* +0x1d; found-path zero */
  uint8_t unknown08_1e; /* +0x1e */
  uint8_t population;
  uint8_t occupation[COLONIZE_COL1_COLONY_POP_MAX];
  uint8_t profession[COLONIZE_COL1_COLONY_POP_MAX];
  ColonizeCol1DurationNibble duration[16];
  int8_t tiles[8]; /* citizen index per surrounding tile; -1 / 0xFF empty */
  uint8_t unknown10[12]; /* +0x78..; production touches at +0x7c — names HOLD */
  ColonizeCol1Buildings buildings;
  ColonizeCol1CustomHouse custom_house;
  uint8_t unknown11_8c; /* +0x8c; AI colony counter (INC cap 0x7f) */
  uint8_t specialty_cargo; /* +0x8d; 0xff = none — FUN_5952_0306 */
  uint8_t labor_shortage; /* +0x8e; LABOR unload decrements */
  uint8_t unknown11_8f; /* +0x8f; AI counter; cleared on unload path */
  uint16_t cargo_produced_mask; /* +0x90; bit per cargo this tick — FUN_364b_0688 */
  uint16_t hammers;
  uint8_t building_in_production;
  uint8_t warehouse_level; /* +0x95; capacity 100*(1+level) — FUN_15eb_0a50 */
  uint8_t capitol_level; /* +0x96; INC on Capitol/Expansion (0x1e/0x1f) — FUN_364b_0114 */
  uint8_t depletion_counter; /* +0x97; INC, wrap at 50 */
  uint16_t hammers_purchased; /* +0x98; FUN_2f2b_5e44 BUY adds remainder */
  uint16_t stock[COLONIZE_COL1_CARGO_TYPES];
  uint8_t unknown13[8]; /* includes per-nation visible counts at +0..+3 in some saves */
  uint32_t rebel_dividend;
  uint32_t rebel_divisor;
} ColonizeCol1Colony;

typedef struct ColonizeCol1TransportChain {
  int16_t next_unit_idx;
  int16_t prev_unit_idx;
} ColonizeCol1TransportChain;

typedef struct ColonizeCol1Unit {
  uint8_t x;
  uint8_t y;
  uint8_t type;
  uint8_t nation_id : 4;
  uint8_t vis_mask : 4; /* DS:0x3147 hi; 0x10<<euro — FUN_1427_0992/0c72 */
  uint8_t unknown15; /* bit7 = ship damaged (FUN_1427_13b0); other bits live */
  uint8_t moves;
  uint8_t origin; /* unknown16[0]: home tribe / origin settlement */
  uint8_t ai_plan; /* unknown16[1]: ASCII plan; default 'X' (0x58) */
  uint8_t orders;
  uint8_t goto_x;
  uint8_t goto_y;
  uint8_t unknown18; /* low 3 = facing */
  uint8_t holds_occupied;
  uint8_t cargo_item_0 : 4;
  uint8_t cargo_item_1 : 4;
  uint8_t cargo_item_2 : 4;
  uint8_t cargo_item_3 : 4;
  uint8_t cargo_item_4 : 4;
  uint8_t cargo_item_5 : 4;
  uint8_t cargo_hold[6];
  uint8_t turns_worked;
  uint8_t profession;
  ColonizeCol1TransportChain transport_chain;
} ColonizeCol1Unit;

typedef struct ColonizeCol1NationTrade {
  uint8_t euro_price[COLONIZE_COL1_CARGO_TYPES];
  int16_t nr[COLONIZE_COL1_CARGO_TYPES];
  int32_t gold[COLONIZE_COL1_CARGO_TYPES];
  int32_t tons[COLONIZE_COL1_CARGO_TYPES];
  int32_t tons2[COLONIZE_COL1_CARGO_TYPES];
} ColonizeCol1NationTrade;

typedef struct ColonizeCol1Nation {
  uint8_t unknown19;
  uint8_t tax_rate;
  uint8_t recruit[3];
  uint8_t unused07;
  uint8_t recruit_count;
  uint8_t founding_fathers[4];
  uint8_t unknown21;
  uint16_t liberty_bells_total;
  uint16_t liberty_bells_last_turn;
  uint8_t unknown22[2];
  int16_t next_founding_father;
  uint16_t founding_father_count;
  uint16_t unused08;
  uint8_t villages_burned;
  uint8_t rebel_sentiment; /* nation+0x19 */
  uint8_t unknown23_pad[4];
  uint16_t artillery_count;
  uint16_t boycott_bitmap;
  int32_t royal_money; /* nation+0x22; FUN_43f7_1d42 REF budget */
  uint8_t unknown24_pad[4];
  uint32_t gold;
  uint16_t current_crosses;
  uint16_t needed_crosses;
  uint8_t unknown25[6]; /* [2..5] euro peer bytes @ −0x77c4 path */
  uint8_t relation_by_indian[8];
  uint8_t unknown26[12]; /* Linux diplo stand-ins; exact DS PARKED */
  ColonizeCol1NationTrade trade;
} ColonizeCol1Nation;

typedef struct ColonizeCol1TribeState {
  uint8_t artillery : 1;
  uint8_t learned : 1;
  uint8_t capital : 1;
  uint8_t scouted : 1;
  uint8_t unused09 : 4;
} ColonizeCol1TribeState;

typedef struct ColonizeCol1TribeAlarm {
  uint8_t friction;
  uint8_t attacks;
} ColonizeCol1TribeAlarm;

typedef struct ColonizeCol1Tribe {
  uint8_t x;
  uint8_t y;
  uint8_t nation_id;
  ColonizeCol1TribeState state;
  uint8_t population;
  uint8_t mission; /* 0xff none; else European nation id */
  uint8_t unknown28[2];
  uint8_t last_bought;
  uint8_t last_sold;
  ColonizeCol1TribeAlarm alarm[4];
} ColonizeCol1Tribe;

typedef struct ColonizeCol1Indian {
  uint8_t capitol_x;
  uint8_t capitol_y;
  uint8_t tech;
  uint8_t unknown31_lo : 7;
  uint8_t extinct : 1; /* bit7 */
  uint8_t unknown31b;
  uint8_t lands_bought; /* FUN_479b_00ca INC; purchase cost */
  uint8_t unknown31_flags; /* Linux: bit 0x20 = contact prelude fired */
  uint8_t muskets;
  uint8_t horse_herds;
  uint8_t unknown31c;
  uint16_t horse_breeding; /* smcol; weaker DOS cite */
  uint8_t unknown31d[2];
  int16_t tons[COLONIZE_COL1_CARGO_TYPES];
  /* +0x2e — per-euro contact FSM 0/1/2 (FUN_5bfb_*); was unknown32[12]. */
  int16_t contact_state[4];
  uint8_t unknown32_tail[4]; /* +0x36..+0x39; no DOS reader cite yet */
  uint8_t met_by_player[4];
  uint8_t unknown33[8]; /* per-euro peace bit 0x40 */
  uint16_t alarm_by_player[4];
} ColonizeCol1Indian;

/*
 * Stuff (727): FUN_75c2_0288 writes 33 DS chunks (not one contiguous RAM block).
 * Chunk sizes sum to 727; see docs/save_format_map.md §Stuff. Port keeps one
 * packed blob for RMW. Census fields named from FUN_4962_0018 + save I/O.
 *
 * Census bytes are DOS-parity preserved on RMW/export — do not recompute from
 * live pools to “freshen” mid-turn lag (intentional interop).
 *
 * unknown36 is NOT map connectivity (that is post_map). It holds remaining FA /
 * tribe tallies / padding after the proven early census window.
 */
typedef struct ColonizeCol1Stuff {
  uint8_t unknown34[12]; /* DS:0x9566 — save R/W only */
  uint8_t all_unit_counts[4]; /* DS:0x8cfc — per-euro unit totals (FUN_4962_0018) */
  uint8_t colony_counts[4]; /* DS:0x9298 — per-euro colony totals */
  /* File 20..63 mid-window (was unknown_stuff_20[44]) — FUN_4962_0018. */
  uint8_t free_colonist_counts[4]; /* DS:0x9408 — units with type==0 */
  uint8_t colony_pop_totals[4]; /* DS:0x940c — Σ colony population */
  uint8_t census_pop_proxy[4]; /* DS:0x9410 — +1 skilled unit + Σ colony pop */
  uint8_t land_combat_totals[4]; /* DS:0x9180 — Σ land combat (mode 0) */
  uint8_t ship_cargo_totals[4]; /* DS:0x9414 — Σ ship cargo capacity */
  uint8_t ship_counts[4]; /* DS:0x9418 — ship unit count */
  uint16_t land_combat_strength[4]; /* DS:0x941c — Σ land combat mode 1 (word) */
  uint8_t armed_ship_counts[4]; /* DS:0x9424 — ships with combat table≠0 */
  uint8_t unknown_9428[4]; /* DS:0x9428 — AI reads; writer outside 0018 */
  uint8_t field_combat_totals[4]; /* DS:0x942c — land combat not in colony / not A|G */
  uint8_t unit_type_counts[4][19]; /* DS:0x924c — nation × unit-type (FUN_4962_0018) */
  uint8_t unknown36[577]; /* remaining FA / tribe blobs — NOT connectivity */
  uint16_t x; /* DS:0x8540 — focus tile */
  uint16_t y; /* DS:0x853e */
  uint8_t zoom_level; /* DS:0x184 lo — 0..3 (FUN_2b5a_0f92) */
  uint8_t zoom_pad; /* DS:0x184 hi — normally 0 */
  uint16_t viewport_x; /* DS:0x17c — camera center */
  uint16_t viewport_y; /* DS:0x17e */
} ColonizeCol1Stuff;

/*
 * Post-map (614): immediately after map.seen. Save order from FUN_75c2_0288 asm:
 *   sea 0x10e @ DS:0x86f6, land 0x10e @ DS:0x85e8,
 *   tally_a 0x20 @ DS:0x945e, tally_b 0x20 @ DS:0x85c8,
 *   4 + 4 + 2 tail (SS:local_8, DS:0x8d80, DS:0x190).
 * Planes filled by FUN_67f4_0088 (15×18 neighbor bitmasks, pitch 18).
 */
typedef struct ColonizeCol1PostMap {
  uint8_t sea_connectivity[COLONIZE_COL1_CONNECT_PLANE_SIZE];
  uint8_t land_connectivity[COLONIZE_COL1_CONNECT_PLANE_SIZE];
  uint16_t continent_tally_a[16];
  uint16_t continent_tally_b[16];
  uint8_t unknown_post_604[4]; /* save-path SS local_8 / LCG reseed blob */
  uint8_t unknown_ds_8d80[4]; /* DS:0x8d80 — boot timer dword (FUN_75c2_2d46); not seed */
  uint16_t prime_resource_seed; /* DS:0x190 — FUN_684c_08c0 mapgen; full u16 */
} ColonizeCol1PostMap;

typedef struct ColonizeCol1Tile {
  uint8_t base : 3;
  uint8_t forest : 1;
  uint8_t special : 1;
  uint8_t hills : 1;
  uint8_t river : 1;
  uint8_t major : 1;
} ColonizeCol1Tile;

typedef struct ColonizeCol1Mask {
  uint8_t has_unit : 1;
  uint8_t has_city : 1;
  uint8_t suppress : 1;
  uint8_t road : 1;
  uint8_t purchased : 1;
  uint8_t pacific : 1;
  uint8_t plowed : 1;
  uint8_t unused : 1;
} ColonizeCol1Mask;

typedef struct ColonizeCol1Path {
  uint8_t region : 4;
  uint8_t visitor : 4;
} ColonizeCol1Path;

typedef struct ColonizeCol1Seen {
  uint8_t score : 4;
  uint8_t english : 1;
  uint8_t french : 1;
  uint8_t spanish : 1;
  uint8_t dutch : 1;
} ColonizeCol1Seen;

/*
 * One trade-route stop (10 B). DOS FUN_647e: unload cargo at +3, load at +6
 * (smcol names those blobs swapped — follow DOS).
 */
typedef struct ColonizeCol1TradeStop {
  uint16_t colony_index; /* 999 = Europe */
  uint8_t unload_count : 4;
  uint8_t load_count : 4;
  uint8_t unload_cargo_nibbles[3];
  uint8_t load_cargo_nibbles[3];
  uint8_t pad;
} ColonizeCol1TradeStop;

typedef struct ColonizeCol1TradeRoute {
  char name[32];
  uint8_t sea; /* non-zero = sea route */
  uint8_t dest_count;
  ColonizeCol1TradeStop stop[4];
} ColonizeCol1TradeRoute;

#pragma pack(pop)

typedef struct ColonizeCol1Map {
  uint16_t width;
  uint16_t height;
  size_t tile_count;
  uint8_t* tile; /* ColonizeCol1Tile bits as bytes */
  uint8_t* mask;
  uint8_t* path;
  uint8_t* seen;
} ColonizeCol1Map;

/*
 * Full original savegame state. Variable-length sections are heap-owned
 * when loaded via col1_save_read_* / col1_save_alloc_empty.
 */
typedef struct ColonizeCol1Save {
  ColonizeCol1Head head;
  ColonizeCol1Player player[COLONIZE_COL1_NATION_COUNT];
  uint8_t other[COLONIZE_COL1_OTHER_SIZE];
  ColonizeCol1Colony* colony;
  ColonizeCol1Unit* unit;
  ColonizeCol1Nation nation[COLONIZE_COL1_NATION_COUNT];
  ColonizeCol1Tribe* tribe;
  ColonizeCol1Indian indian[COLONIZE_COL1_INDIAN_COUNT];
  ColonizeCol1Stuff stuff;
  ColonizeCol1Map map;
  ColonizeCol1PostMap post_map;
  ColonizeCol1TradeRoute trade_route[COLONIZE_COL1_TRADE_ROUTE_COUNT];
  bool owned; /* true if colony/unit/tribe/map buffers owned by this struct */
} ColonizeCol1Save;

void col1_save_init(ColonizeCol1Save* save);
void col1_save_free(ColonizeCol1Save* save);

/* Compile-time layout checks (also run at runtime in smoke tests). */
bool col1_save_check_layout(char* err, size_t err_size);

/*
 * FUN_75c2_0840 header probe: COLONIZE sig + 0x1A + version + optional map size.
 * expect_map_w/h < 0 → skip LOADSIZE check (title / no live map).
 * Error text mirrors @LOADNOT / @LOADOLD / @LOADSIZE.
 */
bool col1_save_validate_head(
  const ColonizeCol1Head* head,
  int expect_map_w,
  int expect_map_h,
  char* err,
  size_t err_size
);

/* Ensure DOS signature / EOF / version fields before write. */
void col1_save_stamp_head(ColonizeCol1Head* head);

size_t col1_save_expected_size(const ColonizeCol1Save* save);
size_t col1_save_expected_size_counts(
  uint16_t map_w,
  uint16_t map_h,
  uint16_t colony_count,
  uint16_t unit_count,
  uint16_t tribe_count
);

bool col1_save_read_file(
  const char* path,
  ColonizeCol1Save* out,
  char* err,
  size_t err_size
);
bool col1_save_write_file(
  const char* path,
  const ColonizeCol1Save* save,
  char* err,
  size_t err_size
);

bool col1_save_read_memory(
  const uint8_t* data,
  size_t size,
  ColonizeCol1Save* out,
  char* err,
  size_t err_size
);
bool col1_save_write_memory(
  const ColonizeCol1Save* save,
  uint8_t** out_data,
  size_t* out_size,
  char* err,
  size_t err_size
);

/* Allocate empty standard-size map buffers; counts must already be set in head. */
bool col1_save_alloc_sections(ColonizeCol1Save* save, char* err, size_t err_size);

#endif
