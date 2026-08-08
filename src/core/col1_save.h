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
 *   stuff                  @ 727 bytes
 *   map.tile/mask/path/seen @ map_w * map_h each (standard 58x72)
 *   unknown_e              @ 504 (= 28 * 18)
 *   unknown_f              @ 110
 *   trade_route[12]        @  74 bytes each (= 888)
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
/* DOS unknown16[1] default seen on virtually all units in original starters. */
#define COL1_UNIT_UNKNOWN16_HI_DEFAULT 0x58u
#define COLONIZE_COL1_NATION_SIZE 316u
#define COLONIZE_COL1_NATION_COUNT 4u
#define COLONIZE_COL1_TRIBE_SIZE 18u
#define COLONIZE_COL1_INDIAN_SIZE 78u
#define COLONIZE_COL1_INDIAN_COUNT 8u
#define COLONIZE_COL1_STUFF_SIZE 727u
#define COLONIZE_COL1_MAP_W_STD 58u
#define COLONIZE_COL1_MAP_H_STD 72u
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
  uint16_t unused01 : 7;
  uint16_t tutorial_hints : 1;
  uint16_t water_color_cycling : 1;
  uint16_t combat_analysis : 1;
  uint16_t autosave : 1;
  uint16_t end_of_turn : 1;
  uint16_t fast_piece_slide : 1;
  uint16_t unused02 : 1;
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
  uint8_t unknown40[2];
  uint16_t active_unit;
  uint8_t unknown41[6];
  uint16_t tribe_count;
  uint16_t unit_count;
  uint16_t colony_count;
  uint8_t unknown42[6];
  uint8_t difficulty; /* 0 Discoverer .. 4 Viceroy */
  uint8_t unknown43[2];
  int8_t founding_father[COLONIZE_COL1_FF_COUNT];
  uint8_t unknown44[6];
  int16_t nation_relation[4];
  uint8_t unknown45[10];
  uint16_t expeditionary_force[4]; /* regulars, dragoons, man-o-wars, artillery */
  uint16_t backup_force[4];
  uint8_t unknown46[32]; /* includes price-group state (see supplemental-info) */
  ColonizeCol1EventFlags event;
  uint8_t unknown05[2];
} ColonizeCol1Head;

typedef struct ColonizeCol1Player {
  char name[24];
  char country_name[24];
  uint8_t unknown06;
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

typedef struct ColonizeCol1Colony {
  uint8_t x;
  uint8_t y;
  char name[24];
  uint8_t nation_id;
  uint8_t unknown08[4];
  uint8_t population;
  uint8_t occupation[COLONIZE_COL1_COLONY_POP_MAX];
  uint8_t profession[COLONIZE_COL1_COLONY_POP_MAX];
  ColonizeCol1DurationNibble duration[16];
  int8_t tiles[8]; /* citizen index per surrounding tile; -1 / 0xFF empty */
  uint8_t unknown10[12];
  ColonizeCol1Buildings buildings;
  ColonizeCol1CustomHouse custom_house;
  uint8_t unknown11[6];
  uint16_t hammers;
  uint8_t building_in_production;
  uint8_t unknown12[5];
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
  uint8_t unused06 : 4;
  uint8_t unknown15;
  uint8_t moves;
  uint8_t unknown16[2];
  uint8_t orders;
  uint8_t goto_x;
  uint8_t goto_y;
  uint8_t unknown18;
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
  uint8_t unknown23[5];
  uint16_t artillery_count;
  uint16_t boycott_bitmap;
  uint8_t unknown24[8];
  uint32_t gold;
  uint16_t current_crosses;
  uint16_t needed_crosses;
  uint8_t unknown25[6];
  uint8_t relation_by_indian[8];
  uint8_t unknown26[12];
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
  uint8_t unknown31[11];
  int16_t tons[COLONIZE_COL1_CARGO_TYPES];
  uint8_t unknown32[12];
  uint8_t met_by_player[4];
  uint8_t unknown33[8];
  uint16_t alarm_by_player[4];
} ColonizeCol1Indian;

typedef struct ColonizeCol1Stuff {
  uint8_t unknown34[15];
  uint16_t counter_decreasing_on_new_colony;
  uint8_t unknown35[2];
  uint16_t counter_increasing_on_new_colony;
  uint8_t unknown36[696]; /* includes downsampled connectivity maps */
  uint16_t x;
  uint16_t y;
  uint8_t zoom_level;
  uint8_t unknown37;
  uint16_t viewport_x;
  uint16_t viewport_y;
} ColonizeCol1Stuff;

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

typedef struct ColonizeCol1TradeRoute {
  char name[32];
  uint8_t sea; /* non-zero = sea route */
  uint8_t dest_count;
  uint8_t data[40]; /* destinations / cargo nibbles; preserve verbatim */
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
  uint8_t unknown_e[COLONIZE_COL1_UNKNOWN_E_SIZE];
  uint8_t unknown_f[COLONIZE_COL1_UNKNOWN_F_SIZE];
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
