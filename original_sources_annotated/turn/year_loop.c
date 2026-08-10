/*
 * Main year / multi-nation turn loop — FUN_130d_0290 (+ autosave / splash helpers).
 *
 * Source: original_sources_decompiled/viceroy_unpacked.c
 *   0172 ~6220–6234; 019e ~6238–6257; 0222 ~6262+; 0290 ~6283–6514
 * Thunk entry: FUN_281f_0546 → 130d_0290
 * Linux:  src/core/turn.c TURN_PROC_* is a post-human batch reshape of this
 *         interleaved loop (see docs/turn_between_players.md).
 *
 * Reference only — not compiled into the Linux binary.
 */

#include <stdint.h>

#include "../include/viceroy_types.h"
#include "../include/viceroy_globals.h"

/* ---- Thunk → real body (catalog) --------------------------------------- */

extern void build_menu_bar(void);                 /* 281f_05a8 → 74a4_0000 */
extern void turn_owner_chrome(uint8_t color);     /* 1984_00aa / 281f_0590 */
extern void minimap_clamp_origin(void);           /* 281f_059a → 6a9f_00d8 */
extern void status_chrome_teardown(void);         /* 1984_04f6 / 281f_056a */
extern void euro_select_nation_context(int n);    /* 281f_0582 → 38fd_0000 */
extern void drain_input_queue(void);              /* 1262_00da */
extern void set_bgm_track(int track);             /* 129f_0318 */
extern int dialog_flush_run(void);                /* 281f_03fe → 6f74_3744 */
extern void camera_follow_chrome(int a, int b);   /* 281f_055e → 49dd_0424 */
extern void rank_euro_nations(void);              /* 281f_0550 → 5bfb_00f8 */
extern void mid_turn_indian_tables(void);         /* 281f_0676 → 4d56_1b3a */
extern void nation_eot(void);                     /* 281f_0644 → 3844_00f2 */
extern void euro_nation_turn(int nation_id);      /* 281f_0638 → 521d_6d8e */
extern void human_merc_offer(void);               /* 281f_0668 → 43f7_2244 */
extern void move_pieces_loop(void);               /* 281f_062c → 2b5a_3b68 */
extern void year_end_chrome(void);                /* 281f_061e → 3844_0442 */
extern void direct_save_slot(int slot);           /* 281f_05b6 → 7562_0034 */
extern void endgame_hof(void);                    /* 281f_0574 → 41f2_14a8 */
extern uint16_t nation_alt_name(int n);           /* 15b3_024e */
extern void dialog_subst_string(int slot, uint16_t s); /* 281f_0438 */
extern void dialog_side_art_flush(int a, int b);  /* 281f_0652 */
extern int nearest_colony_set_active(int x, int y, int nation, int unk); /* 15eb_0142 */
extern void filled_rect(int x0, int y0, int x1, int y1); /* 1b8d_0004 */
extern void video_restore_flush(int a, int b, int c);    /* 1b70_003a */
extern int poll_mouse(void* a, void* b);          /* 1a58_038b */
extern int discovery_event_dispatch(int id);      /* 12fd_006c */
extern int rng_range(int lo, int hi, int nation, int unk); /* 19ef_0032 */
extern void europe_screen_entry(int nation, int focus); /* 281f_05fa → 38fd_55b6 */
extern void colony_screen_entry(int colony);      /* 281f_0608 → 2f2b_6cd4 */
extern uint16_t read_timer_tick(void);            /* 1c0c_0006 */

/* DS anchors used in this loop (also viceroy_globals.h):
 *   0x538a year          0x538c autumn       0x538e turn
 *   0x5394 active nation 0x5396 human/focus  0x5398 focus nation
 *   0x53a2 / 0x53a4      0x53c2 game-running
 *   0x543f + n*0x34      player.control (0 human / 1 AI / 2 withdrawn)
 *   0x829                mid-pass / continue flag
 *   0x826 / 0x828        mouse / demo mode
 *   0x5381 / 0x5382 / 0x5383  game flags (autosave bit4 of 0x5383)
 *   0x3149 + u*0x1c      moves_spent clear
 */

/* ====================================================================== */
/* Helpers                                                                 */
/* ====================================================================== */

/*
 * Ghidra: FUN_130d_0172 | autosave_pick_slot
 * Decade Spring (year%10==0 && autumn==0 && turn>2) → slot 8; else slot 9.
 * Linux: turn_processor FINISH autosave flags + game_apply_turn_autosave.
 */
void autosave_pick_slot(void) {
  int slot = 9;
  /* if (year % 10 == 0 && autumn == 0 && turn > 2) slot = 8; */
  direct_save_slot(slot);
}

/*
 * Ghidra: FUN_130d_019e | demo_end_splash  (~lines in year_loop extract)
 * PARKED. Compose demo/autoplay end → FUN_130d_000a.
 * LAB sketch: load string ids 0x106…; strcat nation/year crumbs; call 000a.
 */
void demo_end_splash(void) {
  /* strcpy/strcat string ids 0x106..; FUN_130d_000a(buf) */
}

/*
 * Ghidra: FUN_130d_0222 | independence_splash
 * PARKED (king path thin). Compose independence-declared → 000a.
 * LAB sketch: string ids 0x11b…; optional crown name subst; FUN_130d_000a.
 */
void independence_splash(void) {
  /* string ids 0x11b..; FUN_130d_000a(buf) */
}

/* ====================================================================== */
/* FUN_130d_0290 | year_turn_loop                                          */
/* ====================================================================== */

/*
 * Outer do { ... } while (DS:0x53c2 != 0). One iteration ≈ one calendar tick
 * after the EN..DU nation pass (and optional year-end chrome).
 *
 * Interleaved model (DOS):
 *   mid: clear spent, rank euros, mid-turn Indian tables (4d56_1b3a)
 *   for nation 0..3:
 *     if not withdrawn: nation_eot (3844_00f2)
 *     if AI: euro_nation_turn (521d_6d8e)
 *     if human: merc offer + Move Pieces (2b5a_3b68)   ← Linux already done
 *   calendar: year++ or autumn toggle (@TIMECHANGE shape, year≥1600)
 *   year_end_chrome (3844_0442)
 *
 * Linux batch-after-human never re-enters Move Pieces here; see TURN_PROC_*.
 */
void year_turn_loop(void) {
  int first_pass_done = 0;
  int human_acted = 0;
  int nation_slot;
  int unit_i;

  /* ---- Boot chrome ---------------------------------------------------- */
  build_menu_bar();
  turn_owner_chrome(4);
  minimap_clamp_origin();
  turn_owner_chrome(5);
  status_chrome_teardown();

  /* Optional Europe-market price snapshot when flag 0x829 set. */
  /* Independence / demo bootstrap when DS:0x104 set — may return early. */

  do {
    human_acted = 0;

    /* ---- Mid-pass (when 0x829 clear) ---------------------------------- */
    if (/* !*(char*)0x829 */ 1) {
      if (!first_pass_done) {
        camera_follow_chrome(1, 1);
        first_pass_done = 1;
      }
      rank_euro_nations();
      /* Clear all units' moves_spent (0x3149). */
      for (unit_i = 0; unit_i < /* unit_count */ 0; ++unit_i) {
        /* units[unit_i].moves_spent = 0; */
      }
      /* Restore human/focus nation from 0x5398 / override 0x53a4. */
      mid_turn_indian_tables(); /* FUN_4d56_1b3a — tables only */
      /*
       * FUN_4d56_1816 (full Indian nation turn) is LIVE but not called from
       * resolved 130d text: overlay thunk 0x1C9A0 → loader → JMPF 1816, far
       * ret 1930:1554 (Return Vector). Dispatcher unknown — do not invent a
       * 130d call edge. See mid_pass_indian_rank.md.
       */
    }

    /* ---- Nation loop EN..DU ------------------------------------------- */
    for (nation_slot = 0; nation_slot < 4 && /* game_running */ 1; ++nation_slot) {
      /* Skip if mid-continue flag and nation_slot < focus — demo/partial. */
      /* DS:0x5394 = nation_slot; */

      /* Withdrawn (control==2) skips EOT+act. control at 0x543f+n*0x34. */
      uint8_t control = /* player[nation_slot].control */ 1;

      /* New-nation intro chrome when control==0 && flag 0x5381 bit7 — PARKED. */

      if (/* !0x829 && */ control != 2) {
        nation_eot(); /* 281f_0644 → 3844_00f2 */
      }

      if (control == 1) {
        /* AI: optional camera_follow; autosave before human focus AI; */
        /* then euro_nation_turn(nation_slot) via 281f_0638 → 521d_6d8e */
        euro_nation_turn(nation_slot);
        (void)autosave_pick_slot;
      } else if (control == 0) {
        /* Human: set focus; camera; maybe autosave; merc offer; Move Pieces */
        human_merc_offer();   /* 43f7_2244 */
        move_pieces_loop();   /* 2b5a_3b68 — Linux: already finished */
        human_acted = 1;
      }

      /* Clear mid-continue flags 0x829 / 0x53c6. */
      (void)control;
    }

    /* If no human acted and mouse idle: re-enter Move Pieces for focus. */
    if (human_acted == 0) {
      /* move_pieces_loop(); */
    }

    /* ---- Calendar tick (@TIMECHANGE shape) ---------------------------- */
    /*
     * turn++; if year > 0x63f (1599): autumn++; if autumn>=2 reset autumn
     * and year++. Special dialog at year==0x640 && autumn==0.
     * Linux: turn_advance_calendar in TURN_PROC_SETUP (before AI).
     */
    /* LAB_130d_0600: */

    /* ---- Year-end chrome ---------------------------------------------- */
    if (/* game_running && !demo */ 1) {
      year_end_chrome(); /* 281f_061e → 3844_0442 */
      /* if DS:0x104: save slot 10, clear running, independence_splash */
    }

    /* ---- Demo autoplay tail (0x828) — PARKED -------------------------- */
    /*
     * Periodic discovery_event_dispatch / random colony or Europe screen;
     * timer + flags may force demo_end_splash and exit.
     */

    (void)nation_alt_name;
    (void)dialog_subst_string;
    (void)dialog_side_art_flush;
    (void)nearest_colony_set_active;
    (void)filled_rect;
    (void)video_restore_flush;
    (void)poll_mouse;
    (void)discovery_event_dispatch;
    (void)rng_range;
    (void)europe_screen_entry;
    (void)colony_screen_entry;
    (void)read_timer_tick;
    (void)drain_input_queue;
    (void)set_bgm_track;
    (void)dialog_flush_run;
    (void)endgame_hof;
    (void)demo_end_splash;
    (void)independence_splash;

  } while (/* DS:0x53c2 != 0 */ 0);
}

/*
 * Thunk resolution for 0290 major callees:
 *
 *   281f_0546 → 130d_0290   (this)
 *   281f_0644 → 3844_00f2   nation_eot
 *   281f_0638 → 521d_6d8e   euro_nation_turn
 *   281f_062c → 2b5a_3b68   Move/View Pieces
 *   281f_0676 → 4d56_1b3a   mid-turn Indian tables
 *   281f_061e → 3844_0442   year_end_chrome
 *   281f_0668 → 43f7_2244   human merc offer
 *   281f_0550 → 5bfb_00f8   rank Euro nations
 *   281f_05b6 → 7562_0034   save slot
 *   281f_055e → 49dd_0424   camera-follow chrome
 */
