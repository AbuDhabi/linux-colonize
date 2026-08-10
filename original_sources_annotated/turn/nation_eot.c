/*
 * Per-nation EOT — FUN_3844_00f2 + treasure tick FUN_3844_0004.
 *
 * Source: original_sources_decompiled/viceroy_unpacked.c
 *   0004 ~58268–58301; 00f2 ~58305–58425; thunk_291f_0a58 ~58685–58690
 * Linux:  pieces split across TURN_PROC_SETUP / EURO / FINISH in src/core/turn.c
 *         (see docs/turn_between_players.md). King arm → ai_king.c.
 *
 * Reference only — not compiled into the Linux binary.
 */

#include <stdint.h>

#include "../include/viceroy_types.h"
#include "../include/viceroy_globals.h"

/* ---- Named stubs (thunks resolved to real bodies) ---------------------- */

extern int map_tile_in_bounds(int x, int y);                 /* 281f_0302 */
extern int tile_tribe_owner(int x, int y);                   /* 281f_06be → 137f_03e4 */
extern int unit_stack_cargo_query(int unit_index, int mode); /* 281f_08bc → 1427_0d38 */
extern void ensure_viewport_contains(int x, int y);          /* 281f_0d9a → 1984_03b2 */
extern void destroy_unit(int unit_index);                    /* 281f_0808 → 1427_0824 */
extern void map_viewport_refresh(void);                      /* 281f_09ba → 6b7e_0004 */
extern void dialog_side_art_flush(int art, int mode);        /* 281f_0652 → 6f74_37a2 */

extern void turn_owner_chrome(uint8_t color);                /* 281f_0590 */
extern void reveal_explore_around_unit(int unit_index);      /* 281f_07a0 → 13f1_02f8 */
extern int colony_at_xy(int x, int y);                       /* 281f_07be → 15eb_0a76 */
extern void dialog_subst_string(int slot, uint16_t str_id);  /* 281f_0438 → 6f74_03ec */
extern void dialog_prefetch_string(int slot, uint16_t ptr);  /* 281f_0416 → 6f74_03d0 */
extern void bgm_event_gate(void);                            /* 281f_04c0 → 12d8_000e */
extern void queue_sound_event(int event_id);                 /* 281f_048e → 129f_02cc */
extern int dialog_flush_run(void);                           /* 281f_03fe → 6f74_3744 */
extern int spawn_unit(int type, int nation, int a, int b);   /* 281f_095c → 1427_06b4 */
extern uint16_t nation_name_ptr(int nation_id);              /* 281f_09a4 → 15b3_01e0 */
extern void europe_screen_entry(int nation, int focus);      /* 281f_05fa → 38fd_55b6 */

extern void tally_nation_professions(int nation_id);         /* 291f_0a9e → 4962_0606 */
extern void europe_nation_eot(int nation_id);                /* 291f_0a90 → 38fd_5e52 */
extern void europe_exit_landfall_tax(void);                  /* 291f_0a82 → 48d3_06ba */
extern void colony_eot_production(int colony_index);         /* 291f_0950 → 364b_0688 */
extern void census_nation(int nation_id);                    /* 291f_0a74 → 4962_0018 */
extern void nation_sol_king_dispatch(int nation_id);         /* 291f_0a66 → 43f7_2424 */
extern uint8_t landfall_goto_duration(int unit, uint8_t a, uint8_t b); /* 291f_0aee → 48d3_0002 */
extern void apply_tax_delta(int msg_id, int mode);           /* 291f_0ae0 → 38fd_3dc8 */

/* ====================================================================== */
/* FUN_3844_0004 | eot_treasure_tick                                       */
/* ====================================================================== */

/*
 * Ghidra: FUN_3844_0004 | eot_treasure_tick
 *
 * Per-unit: Treasure (type 0 / profession 0x1b) on map, not on a tribe tile,
 * stack cargo query < 2. Increments turns_worked (0x315a); after 8 turns
 * outside own Euro colony: destroy unit + optional message for human.
 *
 * Linux: units_tick_treasure_outside_colony (turn.c EURO/FINISH).
 */
int eot_treasure_tick(int unit_index) {
  /* Simplified structure — bytes: viceroy_unpacked.c:58268 */
  uint8_t x = /* units[unit_index].x */ 0;
  uint8_t y = /* units[unit_index].y */ 0;
  (void)x;
  (void)y;
  (void)unit_index;
  /* map_tile_in_bounds; type==0; profession==0x1b; no tribe; cargo_query<2 */
  /* turns_worked++; if >8 → destroy + human msg */
  return 1; /* keep unit */
}

/* Far thunk that pages overlay then calls 0004. */
void thunk_treasure_tick(int unit_index) {
  /* FUN_210d_0dab page-in; then FUN_3844_0004 */
  (void)eot_treasure_tick(unit_index);
}

/* ====================================================================== */
/* FUN_3844_00f2 | nation_eot                                              */
/* ====================================================================== */

/*
 * Ghidra: FUN_3844_00f2 | nation_eot
 * Active nation = DS:0x5394. Player control byte at nation*0x34+0x543f.
 *
 * Phase order (DOS, before AI act or human Move Pieces):
 *   1. Turn-owner chrome (281f_0590 / @COUNTRY color at nation+0x848)
 *   2. Per-unit loop (nation match): fog reveal; treasure tick; ship-build
 *      ready chrome (type 0xd..0x12, bit0x80 on flags, != Frigate 0xb)
 *   3. Profession tally (4962_0606)
 *   4. Europe nation EOT (38fd_5e52 market/tax/pool)
 *   5. Europe-exit landfall / tax treasures (48d3_06ba)
 *   6. Optional Europe screen if ship-ready flagged (0x14c)
 *   7. Colony production pass (364b_0688) for this nation's colonies
 *   8. Census (4962_0018)
 *   9. SoL / king dispatch (43f7_2424)  ← Linux moves this to FINISH
 *  10. Occasional immigrant/ship spawn (every 8 turns, peacetime)
 *
 * Linux reshape: production + nation ticks in SETUP; treasure beside EURO/
 * human FINISH; king in FINISH; Europe market tick uses 38fd_0058 in FINISH
 * (sibling of 5e52). Coastal fort fire (364b_03f6) is Linux SETUP-only addition
 * after production.
 */
void nation_eot(void) {
  int nation_id = /* *(int*)0x5394 */ 0;
  uint8_t nation_lo = (uint8_t)nation_id;

  /* ---- 1. Turn-owner indicator ---------------------------------------- */
  turn_owner_chrome(/* *(uint8_t*)(nation_id + 0x848) */ 0);
  /* DS:0x14c ship-ready flag = 0; DS:0x14e = -1 */

  /* ---- 2. Per-unit treasure + ship-build ready ------------------------ */
  int unit_i = /* unit_count DS:0x539c */ 0;
  while (--unit_i >= 0) {
    /* if (units[unit_i].nation_nibble != nation_lo) continue; */
    reveal_explore_around_unit(unit_i);
    if (eot_treasure_tick(unit_i) == 0) {
      continue; /* destroyed */
    }
    /*
     * Ship-build ready — full gate/progress in turn/nation_eot_ship_spawn.md §A.
     * Type 0xd..0x12 + flag 0x80; +1/+2 turns_worked; threshold type*0xe+0x5235;
     * clear 0x80; human dialog; DS:0x14c=1 if not on colony tile.
     */
    (void)nation_lo;
  }

  /* ---- 3–5. Tallies / Europe / landfall ------------------------------- */
  tally_nation_professions(nation_id);
  europe_nation_eot(nation_id);
  europe_exit_landfall_tax();

  /* ---- 6. Optional Europe screen -------------------------------------- */
  /* if (DS:0x14c) europe_screen_entry(nation_id, DS:0x14e); */

  /* ---- 7. Colony production ------------------------------------------- */
  /* DS:0x84fc+0xe = 0 (clear working field); for each colony of nation: */
  /* colony_eot_production(colony_index); */

  /* ---- 8–9. Census + king/SoL ----------------------------------------- */
  census_nation(nation_id);
  nation_sol_king_dispatch(nation_id);

  /* ---- 10. Immigrant / ship spawn (rare) ------------------------------ */
  /*
   * Full gate/spawn in turn/nation_eot_ship_spawn.md §C.
   * (a89b || a89a>3) && dock flag clear && !(0x5382&1) && (turn&7)==0 →
   * human confirm → spawn type 0x11 + landfall goto + flag 0x40 + tax chrome.
   */
  (void)spawn_unit;
  (void)landfall_goto_duration;
  (void)apply_tax_delta;
  (void)nation_name_ptr;
  (void)dialog_flush_run;
  (void)queue_sound_event;
  (void)bgm_event_gate;
  (void)dialog_subst_string;
  (void)dialog_prefetch_string;
  (void)colony_at_xy;
  (void)map_tile_in_bounds;
  (void)tile_tribe_owner;
  (void)unit_stack_cargo_query;
  (void)ensure_viewport_contains;
  (void)destroy_unit;
  (void)map_viewport_refresh;
  (void)dialog_side_art_flush;
  (void)europe_screen_entry;
}

/*
 * Thunk resolution for 00f2 callees (catalog):
 *
 *   281f_0590           turn_owner_chrome (fill helper)
 *   281f_07a0 → 13f1_02f8  reveal explore around unit
 *   291f_0a58 → 3844_0004  eot_treasure_tick
 *   291f_0a9e → 4962_0606  tally professions
 *   291f_0a90 → 38fd_5e52  Europe nation EOT
 *   291f_0a82 → 48d3_06ba  Europe-exit landfall / tax treasures
 *   281f_05fa → 38fd_55b6  Europe screen entry
 *   291f_0950 → 364b_0688  colony EOT production
 *   291f_0a74 → 4962_0018  census
 *   291f_0a66 → 43f7_2424  SoL + king dispatch
 *   281f_095c → 1427_06b4  spawn unit
 *   291f_0aee → 48d3_0002  landfall goto duration
 *   291f_0ae0 → 38fd_3dc8  tax delta / boycott
 */
