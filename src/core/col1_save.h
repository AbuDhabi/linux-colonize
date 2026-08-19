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
/* DOS citizen-index table at colony+0x70 (FUN_364b_1ba8 memset 0x14); ring = [0..7]. */
#define COLONIZE_COL1_COLONY_TILES 20u
#define COLONIZE_COL1_COLONY_TILE_RING 8u

#pragma pack(push, 1)

typedef struct ColonizeCol1Tut1 {
  uint8_t nr13 : 1; /* nawagers sheet: Pioneer message flag */
  uint8_t nr14 : 1; /* nawagers sheet: Soldier message flag */
  uint8_t unknown01 : 1; /* DS:0x5380 bit2; confirmed dead, never bit-tested in DOS */
  uint8_t nr15 : 1;
  uint8_t nr16 : 1;
  uint8_t nr17 : 1;
  uint8_t unknown02 : 1; /* DS:0x5380 bit6; confirmed dead, never bit-tested in DOS */
  uint8_t nr19 : 1;
} ColonizeCol1Tut1;

typedef struct ColonizeCol1GameOptions {
  /* DS:0x5382 low byte — WoI/REF latches (was unused01). */
  uint16_t woi : 1; /* 0x01 — declare independence */
  uint16_t ref_present : 1; /* 0x02 — REF arrived */
  uint16_t woi_crosses_event : 1; /* 0x04 — confirmed 2026-08-18 live DOSBox-X
                                      capture: fires on the "foreign
                                      intervention force, pending enough
                                      liberty bells" dismissal popup */
  uint16_t independence_chrome : 1; /* 0x08 */
  uint16_t calendar_latch : 1; /* 0x10 */
  uint16_t independence_force : 1; /* 0x20 — bypass REF/event gates */
  uint16_t ref_unit_threshold : 1; /* 0x40 — raises REF unit count thresh */
  uint16_t tutorial_hints : 1;
  uint16_t water_color_cycling : 1; /* DOS 0x5383 bit0 is inverted: 0 = on */
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
  uint8_t unknown03; /* head pad; no gameplay cite in unpacked — save R/W */
  ColonizeCol1GameOptions game_options;
  ColonizeCol1ColonyReportOptions colony_report_options;
  ColonizeCol1Tut2 tut2;
  ColonizeCol1Tut3 tut3;
  uint8_t unknown39[2]; /* head pad; no gameplay cite — save R/W */
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
  uint8_t unknown43[2]; /* head pad; no gameplay cite — save R/W */
  int8_t founding_father[COLONIZE_COL1_FF_COUNT];
  /* DS:0x53c2 / 0x53c4 / 0x53c6 — UI/turn latches (was unknown44[6]). */
  uint16_t turn_loop_running; /* 0x53c2; Esc clears (FUN_2b5a_3104); main loop sets */
  uint16_t map_modal_active; /* 0x53c4; map modal pump gate */
  uint16_t no_unit_selected; /* 0x53c6; View-idle when active_unit < 0 */
  int16_t nation_relation[4];
  int16_t rebel_sentiment_report; /* DS:0x53d0; congress UI 0..100 */
  uint8_t unknown45_pad[8];
  uint16_t expeditionary_force[4]; /* regulars, dragoons, man-o-wars, artillery.
                                       index 2 (0x53DE) confirmed 2026-08-18
                                       live DOSBox-X capture: dec'd right as
                                       each REF wave lands post-independence */
  uint16_t backup_force[4];
  /*
   * DS:0x53ea — DOS uint16 price_group_state[16] (FUN_38fd_0058).
   * Linux king stand-ins overlay first 6 bytes of the same 32 (ai_king.c):
   *   [0] WoI  [1] REF  [2] boycott  [3] merc  [4] unused  [5] congress
   * Those bytes collide with DOS price words 0–2 until stand-ins migrate to
   * game_options.woi / ref_present (0x5382).
   */
  union {
    uint8_t unknown46[32];
    uint16_t price_group_state[16];
  };
  ColonizeCol1EventFlags event;
  uint8_t unknown05[2]; /* head pad after event; no gameplay cite — save R/W */
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

/* Colony +0x60 — packed colonist specialty nibbles (FUN_15eb_0c7a / 0cbc). */
typedef struct ColonizeCol1SpecialtyNibble {
  uint8_t even : 4; /* colonist 2*i */
  uint8_t odd : 4; /* colonist 2*i+1 */
} ColonizeCol1SpecialtyNibble;
typedef ColonizeCol1SpecialtyNibble ColonizeCol1DurationNibble; /* legacy alias */

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

/* Colony +0x1b — FUN_4962_0018 ship probe + FUN_5952_035e AI planner. */
typedef struct ColonizeCol1ColonyAiFlags {
  uint8_t nearby_armed_ship : 1; /* 0x01 — non-MoW armed ship nearby */
  uint8_t nearby_man_o_war : 1; /* 0x02 — MoW / cargo-ship pressure */
  uint8_t needs_military : 1; /* 0x04 */
  uint8_t defense_surplus : 1; /* 0x08 */
  uint8_t needs_colonists : 1; /* 0x10 */
  uint8_t specialist_pressure : 1; /* 0x20 */
  uint8_t needs_garrison : 1; /* 0x40 */
  uint8_t expansion_pressure : 1; /* 0x80 — unworked / expand */
} ColonizeCol1ColonyAiFlags;

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
  ColonizeCol1ColonyAiFlags ai_flags; /* +0x1b */
  ColonizeCol1ColonyFlags flags; /* +0x1c */
  uint8_t build_ai_flags; /* +0x1d; bit7 wants_construction (0x80); other bits unnamed */
  uint8_t garrison_quota; /* +0x1e; threat>>3 — FUN_5952_035e; DEC on assign */
  uint8_t population;
  uint8_t occupation[COLONIZE_COL1_COLONY_POP_MAX];
  uint8_t profession[COLONIZE_COL1_COLONY_POP_MAX];
  ColonizeCol1SpecialtyNibble specialty[16]; /* +0x60; was duration[] */
  /* +0x70 — 20-byte citizen-index table (0xff empty); ring [0..7] is map surround. */
  int8_t tiles[COLONIZE_COL1_COLONY_TILES];
  ColonizeCol1Buildings buildings;
  ColonizeCol1CustomHouse custom_house;
  uint8_t improve_timer; /* +0x8c; INC cap 0x7f — FUN_5952_035e; gates pioneer */
  uint8_t specialty_cargo; /* +0x8d; 0xff = none — FUN_5952_0306 */
  uint8_t labor_shortage; /* +0x8e; LABOR unload decrements */
  uint8_t cargo_idle_turns; /* +0x8f; cleared on unload; AI score *8 */
  uint16_t cargo_produced_mask; /* +0x90; bit per cargo this tick — FUN_364b_0688 */
  uint16_t hammers;
  uint8_t building_in_production;
  uint8_t warehouse_level; /* +0x95; capacity 100*(1+level) — FUN_15eb_0a50 */
  uint8_t capitol_level; /* +0x96; INC on Capitol/Expansion (0x1e/0x1f) — FUN_364b_0114 */
  uint8_t depletion_counter; /* +0x97; INC, wrap at 50 */
  uint16_t hammers_purchased; /* +0x98; FUN_2f2b_5e44 BUY adds remainder */
  uint16_t stock[COLONIZE_COL1_CARGO_TYPES];
  uint8_t visible_to_euro[4]; /* +0xba; smcol population_on_map — fog pop per Euro; founding writes 1 (FUN_364b_1ba8) */
  uint8_t unknown13_pad[4]; /* +0xbe; smcol fortification_on_map[4] (0..3); founding 0; lategame fixtures non-zero */
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
  uint8_t unknown15_lo : 7; /* live AI/cargo/orders latches @ 0x3148 */
  uint8_t ship_damaged : 1; /* bit7 — FUN_1427_13b0 */
  uint8_t moves;
  uint8_t origin; /* unknown16[0]: home tribe / origin settlement */
  uint8_t ai_plan; /* unknown16[1]: ASCII plan; default 'X' (0x58) */
  uint8_t orders;
  uint8_t goto_x;
  uint8_t goto_y;
  uint8_t facing : 3; /* 0..7 last dir — FUN_1427_0968 */
  uint8_t facing_pad : 5;
  uint8_t holds_occupied;
  uint8_t cargo_item_0 : 4;
  uint8_t cargo_item_1 : 4;
  uint8_t cargo_item_2 : 4;
  uint8_t cargo_item_3 : 4;
  uint8_t cargo_item_4 : 4;
  uint8_t cargo_item_5 : 4;
  uint8_t cargo_hold[6];
  uint8_t turns_worked;
  uint8_t profession; /* Treasure (type 0x0a): gold/100 — FUN_48d3_06ba / smcol */
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
  uint8_t nation_flags; /* +0; live bits 0x04/0x08/0x40 @ −0x77f8 (was unknown19) */
  uint8_t tax_rate;
  uint8_t recruit[3];
  uint8_t tax_hike_count; /* +5; FUN_38fd_44a4 INC (was unused07) */
  uint8_t recruit_count;
  uint8_t founding_fathers[4];
  uint8_t unknown21; /* +0xb; no reader cite — opaque */
  uint16_t liberty_bells_total;
  uint16_t liberty_bells_last_turn;
  int16_t unknown22; /* +0x10; written FUN_38fd_5be8; role thin */
  int16_t next_founding_father;
  uint16_t founding_father_count;
  uint16_t ff_count_end_prob; /* smcol; cleared on independence; no FF-prob reader */
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
  uint8_t return_from_europe_x; /* +0x32; FUN_48d3_007a landfall */
  uint8_t return_from_europe_y;
  uint8_t euro_relation[4]; /* −0x77c4 peer bytes / FUN_15b3_* */
  uint8_t relation_by_indian[8];
  /* Linux diplo stand-ins (exact DS PARKED). Array + named views. */
  union {
    uint8_t unknown26[12];
    struct {
      uint8_t treaty_timer[4];
      uint8_t diplo_flag[4];
      uint8_t indian_hostility_sticky;
      uint8_t privateer_spawn_mask;
      uint8_t unknown26_pad[2];
    };
  };
  ColonizeCol1NationTrade trade;
} ColonizeCol1Nation;

typedef struct ColonizeCol1TribeState {
  uint8_t artillery : 1;
  uint8_t learned : 1;
  uint8_t capital : 1;
  uint8_t scouted : 1;
  /*
   * DOS settlement-record +3 bit0x1 "needs first colonist" — cleared by
   * FUN_4d56_152e right after a founding-colonist assignment succeeds
   * (settlement_record_8d4a.md). No Linux producer sets this bit yet
   * (village creation, FUN_4d56_0038, is unported) — structurally present
   * and read by ai_indian_152e_village_growth, but always 0 for now, same
   * "wired but not fed" class as ai_euro_5d04_compute_flags.
   */
  uint8_t needs_colonist : 1;
  uint8_t unused09 : 3;
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
  /*
   * 0xff none; else low nibble = European nation id (0..3).
   * Bit 0x10 = Jesuit-grade mission (FUN_5bfb / FUN_5fef_31ea convert odds).
   */
  uint8_t mission;
#define COL1_TRIBE_MISSION_NONE 0xffu
#define COL1_TRIBE_MISSION_NATION_MASK 0x0fu
#define COL1_TRIBE_MISSION_JESUIT_BIT 0x10u
  uint8_t growth_accum; /* +0; +=pop, clear when >19 — FUN_4d56_152e */
  uint8_t unknown28_pad; /* +1; unproven */
  uint8_t last_bought;
  uint8_t last_sold;
  ColonizeCol1TribeAlarm alarm[4];
} ColonizeCol1Tribe;

typedef struct ColonizeCol1Indian {
  uint8_t capitol_x;
  uint8_t capitol_y;
  uint8_t tech;
  uint8_t unknown31_lo_pad : 5; /* bits 0-4; no reader cite */
  /* bit 0x20 (bit5): WoI tribe-defection one-shot latch — FUN_4d56_1816,
   * indian_woi_defect_1816.md. Set once the roll resolves (hit or miss)
   * so the tribe is not re-checked every turn for the rest of the war. */
  uint8_t woi_defect_resolved : 1;
  /* bit 0x40 (bit6): WoI guaranteed-defect override — FUN_4d56_1816. When
   * set, skips the relation/RNG eligibility gate and always rolls the
   * final defect chance. Trigger for this bit not identified (not set
   * anywhere read this pass) — reserved for a future find. */
  uint8_t woi_defect_forced : 1;
  uint8_t extinct : 1; /* bit7 */
  uint8_t unknown31b;
  uint8_t lands_bought; /* FUN_479b_00ca INC; purchase cost */
  uint8_t unknown31_flags; /* Linux: bit 0x20 = contact prelude fired */
  uint8_t muskets;
  uint8_t horse_herds;
  uint8_t unknown31c;
  uint16_t horse_breeding; /* +10; ±0x32 on acquire/tick — FUN_5bfb_* / 4d56 */
  uint8_t unknown31d[2]; /* no reader cite */
  int16_t tons[COLONIZE_COL1_CARGO_TYPES];
  /* +0x2e — per-euro contact FSM 0/1/2 (FUN_5bfb_*); was unknown32[12]. */
  int16_t contact_state[4];
  /* +0x36 — signed relation accumulator; spill ±8 → FUN_281f_0d6c (FUN_4d56_152e). */
  int8_t euro_relation_accum[4];
  /*
   * +0x3a — per-euro diplo flags (was met_by_player).
   * DOS: bit0x20 met, bit0x40 peace (FUN_5bfb_0182 / FUN_15b3_*).
   * Linux meet still sets the byte non-zero; peace uses bit 0x40.
   */
  uint8_t euro_diplo[4];
  uint8_t unknown33[8]; /* +0x3e; opaque in DOS (Linux formerly parked peace here) */
  uint16_t alarm_by_player[4];
} ColonizeCol1Indian;

#define COL1_INDIAN_MET_BIT 0x20u
#define COL1_INDIAN_PEACE_BIT 0x40u

/*
 * Stuff (727): FUN_75c2_0288 writes 33 DS chunks (not one contiguous RAM block).
 * Chunk sizes sum to 727; see docs/save_format_map.md §Stuff. Port keeps one
 * packed blob for RMW. Census fields named from FUN_4962_0018 + save I/O.
 *
 * Census: mid-campaign RMW preserves lag (no freshen). Blank templates only:
 * col1_stuff_census_fill_blank (FUN_4962_0018).
 *
 * Late DS chunks (file 140+) are NOT map connectivity (that is post_map);
 * export-OK zero when blank (save I/O only in unpacked VICEROY).
 */
typedef struct ColonizeCol1Stuff {
  uint8_t unknown34[12]; /* DS:0x9566 — save R/W only (vestigial) */
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
  /*
   * DS:0x9428 — AI RNG ≤ byte → profession 0x15 (Veteran Soldier).
   * Reader-only / vestigial in observed saves; writer not in unpacked VICEROY
   * (skipped by FUN_4962_0018). RMW-preserved.
   */
  uint8_t veteran_teach_threshold[4];
  uint8_t field_combat_totals[4]; /* DS:0x942c — land combat not in colony / not A|G */
  uint8_t unit_type_counts[4][19]; /* DS:0x924c — nation × unit-type (FUN_4962_0018) */
  /* File 140..716 — was unknown36[577]; DS-named save chunks (FUN_75c2_0288). */
  uint8_t unknown_ds_947e[16];
  uint8_t unknown_ds_95f2[16]; /* AI flag bytes — FUN_4d56_4528 / 5952_035e */
  uint8_t unknown_ds_94a6[64];
  uint8_t unknown_ds_94e6[64]; /* FUN_5952_035e tallies */
  uint8_t unknown_ds_95b2[64];
  uint8_t unknown_ds_9526[64];
  uint8_t unknown_ds_918c[64];
  uint8_t unknown_ds_9572[64];
  uint8_t unknown_ds_944e[8]; /* pop word totals sibling (FUN_4962_0018 ADD) */
  uint8_t ui_toggle_336; /* DS:0x336 — FUN_2f2b_* (smcol: show_colony_prod_quantities) */
  uint8_t tribe_data_9184[8];
  uint8_t unknown_ds_9622[8];
  uint8_t unknown_ds_962a[8];
  uint8_t tribe_dwellings_91cc[128];
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
  uint8_t save_path_blob[4]; /* SS:local_8 on FUN_75c2_0288; was unknown_post_604 */
  uint32_t boot_timer; /* DS:0x8d80 — FUN_75c2_2d46; LCG mix-in; not seed */
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

/*
 * Packed cargo types in trade-stop nibble blobs (3 bytes → 6 slots).
 * Same low-then-high nibble order as ColonizeCol1Unit cargo_item_0..5.
 */
static inline int col1_trade_nibble_cargo(const uint8_t nibbles[3], int i) {
  if (!nibbles || i < 0 || i >= 6) {
    return -1;
  }
  const uint8_t b = nibbles[i >> 1];
  return (i & 1) ? (int)((b >> 4) & 0x0fu) : (int)(b & 0x0fu);
}

static inline void col1_trade_nibble_set(uint8_t nibbles[3], int i, int cargo) {
  if (!nibbles || i < 0 || i >= 6) {
    return;
  }
  const unsigned t = (unsigned)cargo & 0x0fu;
  const int bi = i >> 1;
  if (i & 1) {
    nibbles[bi] = (uint8_t)((nibbles[bi] & 0x0fu) | (t << 4));
  } else {
    nibbles[bi] = (uint8_t)((nibbles[bi] & 0xf0u) | t);
  }
}

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
  /* DS:0x948e × 0x18 — save R/W only in unpacked VICEROY.
   * Smcol: unknown[18] + click_before_open_colony x,y + unknown[2]. */
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
