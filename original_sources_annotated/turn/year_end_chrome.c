/*
 * Year-end Euro chrome — FUN_3844_0442.
 *
 * Source: original_sources_decompiled/viceroy_unpacked.c ~58430–58680
 * Thunk entry: FUN_281f_061e → 3844_0442
 * Called from FUN_130d_0290 after calendar tick (LAB_130d_0600 path).
 * Linux: no dedicated year-end module; pieces live in ai_king / game-over /
 *        dialog PARKED. See docs/turn_between_players.md.
 *
 * Reference only — not compiled into the Linux binary.
 */

#include <stdint.h>

#include "../include/viceroy_types.h"
#include "../include/viceroy_globals.h"

extern void census_nation(int nation_id);                 /* 291f_0a74 → 4962_0018 */
extern void dialog_prefetch_string(int slot, uint16_t p); /* 281f_0416 → 6f74_03d0 */
extern void set_bgm_track(int track);                     /* 281f_04ac → 129f_0318 */
extern int dialog_flush_run(void);                        /* 281f_03fe */
extern void victory_announce(int a, int b, int msg);      /* 291f_0aba → 75c2_20e2 */
extern void bind_active_colony(int colony_index);         /* 281f_09e6 → 15eb_002c */
extern void format_nation_subst(int slot, int a, int b, int nation); /* 291f_0ac8 → 6f74_0404 */
extern void strcpy_dos(void* dst, uint16_t src);          /* 1d1d_07e4 */
extern void dialog_subst_string(int slot, uint16_t s);    /* 281f_0438 */
extern void dialog_numeric_subst(int slot, int v, int hi); /* 281f_09ae */
extern void dialog_side_art_flush(int art, int mode);     /* 281f_0652 */
extern void reload_resource_ptrs(void);                   /* 291f_0aac → 78d8_00c4 */
extern uint16_t nation_name_ptr(int nation_id);           /* 281f_09a4 */
extern void load_dual_string(int a, int b, int nation);   /* 281f_0422 → 7314_0208 */
extern void clear_diplo_bit(int a, int b, int bit);       /* 281f_0a10 → 15b3_00d0 */
extern void format_int_blit(void* buf, int v);            /* 281f_0182 → 104b_012e */
extern void side_art_style8_flush(int msg);               /* 291f_0ad4 → 6f74_378a */
extern void status_chrome_teardown(void);                 /* 281f_056a → 1984_04f6 */
extern void endgame_hof(void);                            /* 281f_0574 → 41f2_14a8 */

/*
 * DS used here:
 *   0x5382 game flags — bit0 war/WoI; bit3 defeat handled; bit4 splash done;
 *                       bit5 / bit6 REF-related thresholds
 *   0x538a year   0x538c autumn   0x5398 human/focus nation
 *   0x53d2 crown nation id   0x53da.. REF pools
 *   0x53c2 game-running (cleared → exit year loop)
 *   nation colony-count scratch at nation + (-0x6d68)
 */

/* ====================================================================== */
/* FUN_3844_0442 | year_end_chrome                                         */
/* ====================================================================== */

/*
 * Sections (control-flow labels from decomp):
 *
 *   A. Census human (+ crown if at war)
 *   B. Early-exit "no colonies / not at war / year≥1600" → defeat dialog
 *      (LAB_3844_04ec) then maybe HoF and clear 0x53c2
 *   C. Wartime (0x5382 bit0):
 *        C1. Crown collapse / REF empty → victory announce (LAB_3844_0b4a)
 *        C2. SoL / colony-count thresholds → offer / force peace chrome
 *   D. Peacetime rival loop (EN..DU withdrawn or AI): SoL pressure dialogs,
 *      auto-declare war (OR diplo bit 0x40) when threshold crossed
 *   E. Calendar event years (1782/1840-ish and 1800/1850-ish string ids) —
 *      anniversary dialogs; 1800/1850 may clear 0x53c2 (game over)
 *   F. LAB_3844_0b4a epilogue: if game stopped, optional continue dialog,
 *      set 0x5382 bit4
 *
 * Linux: ai_king covers tax/declare/REF/war act structurally; victory /
 * defeat / anniversary dialogs mostly PARKED.
 */
void year_end_chrome(void) {
  int keep_running = 1;
  int human = /* *(int*)0x5398 */ 0;
  int crown = /* *(int*)0x53d2 */ 0;
  int year = /* *(int*)0x538a */ 1492;
  uint8_t flags = /* *(uint8_t*)0x5382 */ 0;
  int at_war = (flags & 1) != 0;

  /* ---- A. Census ------------------------------------------------------ */
  if (at_war) {
    census_nation(crown);
  }
  census_nation(human);
  /* Recount human colonies into scratch (nation + -0x6d68). */

  /* ---- B. No colonies / peacetime / year≥1600 → defeat path ----------- */
  if (!(year < 0x640 || /* human_colony_count != 0 */ 1 || at_war)) {
    /* Defeat dialog (string 0xf09); LAB_3844_04ec: */
    side_art_style8_flush(0xf09);
    /* LAB_3844_04ec: */
    if ((flags & 0x10) == 0) {
      endgame_hof();
    }
    /* *(int*)0x53c2 = 0; */
    keep_running = 0;
    goto lab_0b4a;
  }

  /* ---- C. Wartime chrome ---------------------------------------------- */
  if (at_war && (flags & 8) == 0) {
    /*
     * C1. Crown has no colonies OR flag bit5: count crown warships
     * (types 6/8/0xb). If fleets+REF pools thin → victory announce
     * (0xf20), OR flags bit3, set DS:0x104, goto LAB_3844_0b4a.
     */
    /*
     * C2. Count human colonies with rebel bit; SoL ratio vs crown;
     * thresholds force peace offer (cVar8) or pressure dialog (cVar1).
     * Peace offer → LAB_3844_04ec (may stop game).
     */
    (void)crown;
    (void)bind_active_colony;
    (void)format_nation_subst;
    (void)strcpy_dos;
    (void)victory_announce;
  }

  /* ---- D. Peacetime rival SoL / auto-declare -------------------------- */
  if (!at_war) {
    int rival;
    for (rival = 0; rival < 4; ++rival) {
      /*
       * Skip live rivals without withdrawn control oddly gated in decomp
       * (condition uses control!=0 after select). For each: compute SoL
       * pressure vs difficulty; dialog 0xf5e / 0xf69; or auto-declare
       * (OR player flags bit2, clear diplo vs others, dual-string load).
       */
      (void)rival;
      (void)dialog_numeric_subst;
      (void)nation_name_ptr;
      (void)load_dual_string;
      (void)clear_diplo_bit;
    }
  }

  /* ---- E. Calendar anniversary / game-over years ---------------------- */
  if ((flags & 0x10) == 0) {
    /*
     * Spring-ish: year==0x6fe (1790) peacetime OR year==0x730 (1840)
     * → anniversary dialog (0xf73).
     * year==0x708 (1800) peacetime OR year==0x73a (1850) → pick richest
     * colony dialog, HoF, clear 0x53c2 (end).
     */
    (void)year;
    (void)format_int_blit;
    (void)dialog_side_art_flush;
    (void)dialog_subst_string;
    (void)dialog_prefetch_string;
    (void)set_bgm_track;
    (void)dialog_flush_run;
    (void)reload_resource_ptrs;
  }

lab_0b4a:
  /* ---- F. Epilogue ---------------------------------------------------- */
  if (/* *(int*)0x53c2 == 0 */ !keep_running) {
    if (keep_running /* bVar2 true path when stopped after victory */) {
      /* if flags bit3: DS:0x53a2 = 1; teardown; continue dialog may restart */
    }
    /* *(byte*)0x5382 |= 0x10; */
  }

  (void)status_chrome_teardown;
  (void)human;
}

/*
 * Thunk resolution for 0442 major callees:
 *
 *   291f_0a74 → 4962_0018   census
 *   291f_0aba → 75c2_20e2   victory / endgame announce
 *   291f_0ac8 → 6f74_0404   format nation into dialog subst
 *   291f_0ad4 → 6f74_378a   side-art style 8 flush
 *   291f_0aac → 78d8_00c4   reload resource far-ptrs
 *   281f_0574 → 41f2_14a8   HoF / score
 *   281f_056a → 1984_04f6   status chrome teardown
 *   281f_09e6 → 15eb_002c   bind active colony
 *   281f_0a10 → 15b3_00d0   clear diplomacy bit
 */
