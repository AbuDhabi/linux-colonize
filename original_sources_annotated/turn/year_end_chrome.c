/*
 * Year-end Euro chrome — FUN_3844_0442.
 *
 * Source: original_sources_decompiled/viceroy_unpacked.c ~58430–58680
 * Thunk entry: FUN_281f_061e → 3844_0442
 * Called from FUN_130d_0290 after calendar tick (LAB_130d_0600 path).
 * Full UI map (strings / subst / thresholds): turn/year_end_chrome.md
 * Linux: no dedicated year-end module; pieces in ai_king / HoF PARKED.
 *         See docs/turn_between_players.md.
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
extern void or_diplo_bit(int a, int b, int bit);          /* caseD_10 / 15b3_0066 family */
extern void format_int_blit(void* buf, int v);            /* 281f_0182 → 104b_012e */
extern void side_art_style8_flush(int msg);               /* 291f_0ad4 → 6f74_378a */
extern void status_chrome_teardown(void);                 /* 281f_056a → 1984_04f6 */
extern void endgame_hof(void);                            /* 281f_0574 → 41f2_14a8 */
extern void euro_select_nation_context(int n);            /* 281f_0582 → 38fd_0000 */

/*
 * DS used here:
 *   0x5382 game flags — bit0 war; bit3 victory done; bit4 splash done;
 *                       bit5 force-victory; bit6 strict fleet cap
 *   0x538a year   0x538c autumn   0x5398 human/focus
 *   0x53d2 crown   0x53d4 crown name helper   0x53da.. REF pools
 *   0x53c2 game-running (cleared → exit year loop)
 *   nation colony-count scratch at nation + (−0x6d68)
 *   nation SoL byte at nation + (−0x6bf4)
 *   rival SoL table at rival + (−0x6bf0)
 */

/* Year constants (see year_end_chrome.md) */
enum {
  YE_YEAR_1600 = 0x640,
  YE_YEAR_1790 = 0x6fe,
  YE_YEAR_1800 = 0x708,
  YE_YEAR_1840 = 0x730,
  YE_YEAR_1850 = 0x73a
};

/* String resource ids */
enum {
  YE_STR_DEFEAT = 0xf09,
  YE_STR_VICTORY = 0xf20,
  YE_STR_PEACE_TMPL = 0xf29,
  YE_STR_PEACE_ANN = 0xf31,
  YE_STR_PRESSURE = 0xf39,
  YE_STR_DECLARE_A = 0xf4b,
  YE_STR_DECLARE_B = 0xf3f,
  YE_STR_RIVAL_UP = 0xf5e,
  YE_STR_RIVAL_DOWN = 0xf69,
  YE_STR_ANNIV = 0xf73
};

/* ====================================================================== */
/* FUN_3844_0442 | year_end_chrome                                         */
/* ====================================================================== */

void year_end_chrome(void) {
  int keep_running_flag = 1; /* bVar2 — false after defeat path */
  int human = /* *(int*)0x5398 */ 0;
  int crown = /* *(int*)0x53d2 */ 0;
  int year = /* *(int*)0x538a */ 1492;
  int autumn = /* *(int*)0x538c */ 0;
  uint8_t flags = /* *(uint8_t*)0x5382 */ 0;
  int at_war = (flags & 1) != 0;
  int human_colonies = 0; /* scratch nation−0x6d68 */

  /* ---- A. Census ------------------------------------------------------ */
  if (at_war) {
    census_nation(crown);
  }
  census_nation(human);
  /* recount human_colonies into −0x6d68 */

  /* ---- B. Defeat: year≥1600, no colonies, peacetime ------------------- */
  if (!(year < YE_YEAR_1600 || human_colonies != 0 || at_war)) {
    dialog_subst_string(0, /* difficulty name −0x7c6c[diff] */ 0);
    dialog_prefetch_string(1, /* human 0x540e */ 0);
    side_art_style8_flush(YE_STR_DEFEAT);
    goto lab_04ec;
  }

  /* ---- C. Wartime ----------------------------------------------------- */
  if (at_war && (flags & 8) == 0) {
    int crown_colonies = /* crown −0x6d68 */ 0;
    if (crown_colonies == 0 || (flags & 0x20) != 0) {
      /* C1: count crown types 6/8/0xb; REF pools 53da/dc/e0 */
      int fleet_cap = ((flags & 0x40) == 0) ? 8 : 1;
      int warships = 0;
      int ref_thin = /* (2-(53dc==0)-(53e0==0)+53da) < 4 */ 1;
      if ((warships < fleet_cap || (flags & 0x20)) && (ref_thin || (flags & 0x20))) {
        dialog_prefetch_string(0, /* human 0x540e */ 0);
        dialog_prefetch_string(1, /* human 0x5426 */ 0);
        set_bgm_track(3);
        dialog_flush_run();
        victory_announce(1, 2, YE_STR_VICTORY);
        /* flags |= 8; DS:0x104 = 1; */
        goto lab_0b4a;
      }
    }
    /* C2: rebel colony count local_68; SoL ratio local_8 */
    int rebel_cols = 0;
    int sol_ratio = 50; /* (crown_adj+1)*100/(human_adj+crown_adj+1) */
    int peace_sev = (rebel_cols == 0) ? 1 : 0; /* cVar8; boosted if sol>89 */
    int press_sev = (rebel_cols < 3) ? 1 : 0;  /* cVar1; boosted if sol>79 */
    if (sol_ratio > 0x59) {
      peace_sev = 3;
    }
    if (sol_ratio > 0x4f) {
      press_sev = 3;
    }
    if (human_colonies == 0) {
      peace_sev = 2;
    }
    if (human_colonies < 3) {
      press_sev = 2;
    }
    if (peace_sev != 0) {
      /* 0xf29 template + 0xf31 announce → LAB_3844_04ec */
      strcpy_dos(/* local_58 */, YE_STR_PEACE_TMPL);
      dialog_flush_run();
      victory_announce(2, 1, YE_STR_PEACE_ANN);
      reload_resource_ptrs();
      goto lab_04ec;
    }
    if (press_sev != 0) {
      /* 0xf39 + 09ae(rebel, colonies, sol) → 0652 */
      strcpy_dos(/* local_58 */, YE_STR_PRESSURE);
      dialog_numeric_subst(0, rebel_cols, 0);
      dialog_numeric_subst(1, human_colonies, 0);
      dialog_numeric_subst(2, sol_ratio, 0);
      dialog_side_art_flush(/* local_58 */, 1);
    }
    (void)bind_active_colony;
    (void)format_nation_subst;
  }

  /* ---- D. Peacetime rivals -------------------------------------------- */
  if (!at_war) {
    int rival;
    for (rival = 0; rival < 4; ++rival) {
      /* select nation; if flags bit2 already set skip */
      /* iVar5 = market[0x19] * (−0x6bf0)[rival] / 100 */
      /* local_6 = (diff−8)*−10 */
      /* rising 0xf5e / falling 0xf69 / else auto-declare 0xf4b+0xf3f */
      (void)rival;
      (void)euro_select_nation_context;
      (void)load_dual_string;
      (void)or_diplo_bit;
      (void)clear_diplo_bit;
      (void)nation_name_ptr;
      (void)YE_STR_RIVAL_UP;
      (void)YE_STR_RIVAL_DOWN;
      (void)YE_STR_DECLARE_A;
      (void)YE_STR_DECLARE_B;
    }
  }

  /* ---- E. Calendar events --------------------------------------------- */
  if ((flags & 0x10) == 0) {
    if (autumn == 0 &&
        ((year == YE_YEAR_1790 && !at_war) || year == YE_YEAR_1840)) {
      /* difficulty + human name; 0xf73; 0182 season bit; 0652 */
      (void)YE_STR_ANNIV;
      (void)format_int_blit;
      (void)dialog_side_art_flush;
      (void)dialog_subst_string;
      (void)dialog_prefetch_string;
    }
    if ((year == YE_YEAR_1800 && !at_war) || year == YE_YEAR_1850) {
      /* richest human colony by pop+0x1f; subst; 03fe; HoF; 0x53c2=0 */
      endgame_hof();
      /* *(int*)0x53c2 = 0; */
    }
  }

  goto lab_0b4a;

lab_04ec:
  /* Optional HoF if bit4 clear; stop game; keep_running_flag = 0 */
  if ((flags & 0x10) == 0) {
    endgame_hof();
  }
  /* *(int*)0x53c2 = 0; */
  keep_running_flag = 0;

lab_0b4a:
  if (/* *(int*)0x53c2 == 0 */ !keep_running_flag || 1) {
    if (keep_running_flag) {
      /* victory bit3 → 0x53a2=1; teardown; continue dialog may restart */
      status_chrome_teardown();
      if (dialog_flush_run() == 2) {
        /* *(int*)0x53c2 = 1; */
      }
    }
    /* *(byte*)0x5382 |= 0x10; */
  }

  (void)year;
  (void)autumn;
  (void)human;
  (void)crown;
  (void)YE_YEAR_1600;
}

/*
 * Thunk resolution — see year_end_chrome.md for full subst/string tables.
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
