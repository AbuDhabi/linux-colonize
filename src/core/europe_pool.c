/*
 * Sections:
 *  - Purchase table & pool profession rolls (europe_purchase_option_count .. europe_sort_train_by_cost)
 *  - Cargo tables, nation setup & campaign reset (europe_cargo_burden .. europe_reset_campaign_nation)
 */

#include "core/europe.h"
#include "core/europe_art.h"
#include "core/europe_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_popup.h"
#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/dos_rng.h"
#include "core/founding_fathers.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "platform/diagnostics.h"
#include "core/ss.h"
#include "core/strutil.h"
#include "core/ui_button.h"
#include "core/units.h"
#include "platform/platform.h"

/* Forward declarations for this file's own statics (the split moved
   section order; these keep every call site legal). */
static int europe_pool_cand_index(int profession);
static int europe_pool_remap(int job);
static bool europe_job_is_expert(int job);
static void europe_pool_view_from_screen(EuropePoolView* v, const EuropeScreen* eu);
static void europe_refill_pool_slot_impl(
  EuropeScreen* eu, int slot, bool force_expert, ColonizeDosRng* dos, unsigned* rng_state
);
static void europe_refill_pool_slot(EuropeScreen* eu, int slot, unsigned* rng_state);
static void europe_init_pool(EuropeScreen* eu);
static void europe_sort_train_by_cost(EuropeTrainOption* a, int n);

/*
 * The Europe purchase list (no Man-O-War). Oracle:
 * original_screenshots/europe/purchase.png; in DOS it is the FUN_521d_5c3c
 * table at DS:0x978d, stride 6, pinned byte-identical across three
 * original_memory_dumps — 0=Artillery/500, 1=Caravel/1000,
 * 2=Merchantman/2000, 3=Galleon/3000, 4=Privateer/2000, 5=Frigate/5000.
 *
 * Audit AE-17: the same six prices were also typed out in ai_euro.c's
 * 5d04 `k_purchase` and a third time as 5d04's gold-floor switch (the
 * DS:0x9796/0x97a8/0x97ae "catch-up gold" values are literally this table's
 * Caravel / Privateer / Frigate price cells). This file owns the numbers
 * now; europe_purchase_price / europe_purchase_option_at expose them.
 */
static const EuropePurchaseOption k_purchase_opts[] = {
  {UNITS_KIND_ARTILLERY, "", 500, false},
  {UNITS_KIND_CARAVEL, "", 1000, true},
  {UNITS_KIND_MERCHANTMAN, "", 2000, true},
  {UNITS_KIND_GALLEON, "", 3000, true},
  {UNITS_KIND_PRIVATEER, "", 2000, true},
  {UNITS_KIND_FRIGATE, "", 5000, true},
};
static const int k_purchase_opt_count =
  (int)(sizeof(k_purchase_opts) / sizeof(k_purchase_opts[0]));

/* ===================== Purchase table & pool profession rolls (europe_purchase_option_count .. europe_sort_train_by_cost) ===================== */
int europe_purchase_option_count(void) {
  return k_purchase_opt_count;
}

const EuropePurchaseOption* europe_purchase_option_at(int index) {
  if (index < 0 || index >= k_purchase_opt_count) {
    return NULL;
  }
  return &k_purchase_opts[index];
}

int europe_purchase_price(ColonizeUnitKind kind) {
  for (int i = 0; i < k_purchase_opt_count; ++i) {
    if (k_purchase_opts[i].kind == kind) {
      return k_purchase_opts[i].gold;
    }
  }
  return 0;
}

/*
 * NAMES.TXT @UNIT row per purchase slot — the display name the list draws.
 * k_purchase_opts[].kind (a ColonizeUnitKind) is the price-lookup key; name[]
 * is filled live from the catalog below and is display-only, so a renamed
 * unit in a modded NAMES.TXT shows through here the way DOS's own list does.
 * A catalog miss leaves name[] empty (no DOS text baked in).
 */
static const int k_purchase_unit_rows[] = {11, 13, 14, 15, 16, 17};

void europe_init_purchase_table(EuropeScreen* eu) {
  eu->purchase_count = 0;
  for (int i = 0; i < k_purchase_opt_count && eu->purchase_count < EUROPE_PURCHASE_MAX; ++i) {
    EuropePurchaseOption* slot = &eu->purchase[eu->purchase_count++];
    *slot = k_purchase_opts[i];
    const char* live = reports_names_field("UNIT", k_purchase_unit_rows[i], 0);
    if (live && live[0]) {
      str_copy_trunc(slot->name, sizeof(slot->name), live);
    }
  }
}

/*
 * Recruit-pool profession table. The roll below can only produce these 21
 * @JOB ids; the six the DOS remap folds away (Master Sugar/Tobacco/Cotton
 * Planters, Expert Fur Trappers, Expert Teachers, Veteran Dragoons) are
 * deliberately absent — see europe_pool_remap.
 */
/*
 * @JOB ids only — no display text lives here (audit SC-13). The name
 * column this table used to carry (Petty Criminals=26, Indentured
 * Servants=25, Free Colonists=19, Expert Farmers=0, Expert Lumberjacks=5,
 * Expert Ore Miners=6, Expert Silver Miners=7, Expert Fishermen=8, Master
 * Distiller=9, Master Tobacconists=10, Master Weavers=11, Master Fur
 * Traders=12, Master Carpenters=13, Master Blacksmiths=14, Master
 * Gunsmiths=15, Firebrand Preachers=16, Elder Statesmen=17, Hardy
 * Pioneers=20, Veteran Soldiers=21, Seasoned Scouts=22, Jesuit
 * Missionaries=24) was dead: nothing ever read it, and
 * reports_job_display_name (NAMES.TXT @JOB column 1) is the live source.
 */
static const int k_pool_cands[] = {
  26, 25, 19, 0, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 20, 21, 22, 24,
};

/* Slot in k_pool_cands, or -1 when this @JOB id cannot appear in the pool. */
static int europe_pool_cand_index(int profession) {
  for (size_t i = 0; i < sizeof(k_pool_cands) / sizeof(k_pool_cands[0]); ++i) {
    if (k_pool_cands[i] == profession) {
      return (int)i;
    }
  }
  return -1;
}

/*
 * Audit SC-13: k_pool_cands stays as the DOS profession-id FILTER (the
 * remap below makes six @JOB ids unreachable from Europe), but the name
 * column no longer answers — reports_job_display_name reads NAMES.TXT @JOB
 * column 1, the same column those literals were copied from, so a
 * translated NAMES.TXT now reaches the recruit pool too.
 */
const char* europe_pool_job_name(int profession) {
  const int cand = europe_pool_cand_index(profession);
  return reports_job_display_name(cand >= 0 ? profession : EUROPE_POOL_JOB_FREE_COLONIST);
}

/*
 * DOS `FUN_38fd_46d4`'s remap block (viceroy_unpacked.c:64595-64609): the
 * expert tier rolls a raw @JOB in 0..0x18 and then rewrites seven of them
 * before the value is ever stored. Six professions are therefore
 * UNREACHABLE from Europe — they exist only as colony/Indian training
 * outcomes or promotions:
 *
 *   0x12 Expert Teachers    -> 0x0d Master Carpenters   (bugs.md)
 *   0x17 Veteran Dragoons   -> 0x0d Master Carpenters
 *   0x13 Free Colonists     -> 0x16 Seasoned Scouts     (the free-colonist
 *                                    result comes from the tier roll's own
 *                                    0x1c return, not from this table)
 *   0x01 Master Sugar Planters  -> 0x08 Expert Fishermen
 *   0x02 Master Tobacco Planters-> 0x05 Expert Lumberjacks
 *   0x03 Master Cotton Planters -> 0x00 Expert Farmers
 *   0x04 Expert Fur Trappers    -> 0x06 Expert Ore Miners
 *
 * which is also why reports.c's labour scan says Expert Teachers (18) and
 * Veteran Dragoons (23) "never appear". The port used to draw from a
 * hand-weighted candidate table that listed Expert Teachers outright.
 *
 * The Expert Teacher colonist TYPE was cut from the final DOS game: this
 * remap makes it unhirable, schools can't produce it (graduates take the
 * teacher's specialty), and its school level 4 bars it from teaching. Any
 * DOS code that seems to create or handle one is a dead leftover — never
 * port such paths.
 */
static int europe_pool_remap(int job) {
  switch (job) {
    case COLONIZE_PROF_TEACHER: return COLONIZE_PROF_CARPENTER;
    case UNITS_JOB_COLONIST: return UNITS_JOB_SCOUT;
    case COLONIZE_PROF_SUGAR_PLANTER: return COLONIZE_PROF_FISHERMAN;
    case COLONIZE_PROF_TOBACCO_PLANTER: return COLONIZE_PROF_LUMBERJACK;
    case COLONIZE_PROF_COTTON_PLANTER: return COLONIZE_PROF_FARMER;
    case COLONIZE_PROF_FUR_TRAPPER: return COLONIZE_PROF_ORE_MINER;
    case UNITS_JOB_DRAGOON: return COLONIZE_PROF_CARPENTER;
    default: return job;
  }
}

/* DOS `FUN_15eb_0002` (via FUN_281f_0c9a): 0 for @JOB 0x13 and 0x19..0x1c,
 * i.e. Free Colonists / Servants / Criminals / Converts / job NONE — the
 * non-expert classes. 1 for every expert. */
static bool europe_job_is_expert(int job) {
  return !(job == UNITS_JOB_COLONIST || (job >= 0x19 && job <= 0x1c));
}

static void europe_pool_view_from_screen(EuropePoolView* v, const EuropeScreen* eu) {
  memset(v, 0, sizeof(*v));
  if (!eu) {
    return;
  }
  for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
    v->job[i] = eu->pool[i].profession;
    v->filled[i] = eu->pool[i].filled;
  }
  v->difficulty = (int)eu->difficulty;
  v->brewster = eu->brewster_no_criminals;
}

/*
 * `FUN_38fd_46d4(force_expert)` (viceroy_unpacked.c:64554-64694) — the
 * profession a recruit-pool slot is refilled with. Two halves:
 *
 *  - Tier roll (force_expert == 0, the Recruit-click refill 64776 and the
 *    game-start seed). Threshold t = (difficulty + 3) >> 1, with the DOS
 *    difficulty byte DS:0x53a6 used only for the human nation (AI nations
 *    substitute 1). RNG(1,15) <= t -> Petty Criminals; else RNG(1,10) <= t
 *    -> Indentured Servants; else RNG(1,8) <= t -> Free Colonists; else
 *    fall through to the expert half. Brewster (FF 0x14) turns the two
 *    bottom results into Free Colonists in place, exactly as
 *    europe_apply_brewster does to slots already rolled.
 *
 *  - Expert half (force_expert != 0, the end-of-turn crosses spawn on the
 *    season quad, 68583). Guarded twice: if all three pool slots already
 *    hold experts the routine gives up and returns Free Colonists (so the
 *    pool never shows three experts at once), and a roll that duplicates a
 *    profession already in the pool is thrown away and re-rolled, up to
 *    100 attempts.
 *
 * Deviation: DOS draws the expert value from a per-nation 5-bit LFSR
 * (nation +0x44 stepped by FUN_3f3f_0006 with poly 0x14, plus the +0x45
 * salt, rejecting > 0x18) so the sequence never repeats a value inside one
 * cycle. Those two bytes are the ones the port repurposed as
 * ColonizeCol1Nation.diplo_flag[0..1], so the state is not available here;
 * a uniform draw over the same 0..0x18 range is used instead (see
 * EuropePoolRng — like the LFSR it takes no shared-stream draw). The
 * reachable set and its distribution are identical, only the ordering
 * differs. The tier rolls above are the real `04d4` stream draws.
 */
int europe_roll_pool_profession(
  const EuropePoolView* v, int slot, bool force_expert, EuropePoolRng* st
) {
  if (!force_expert) {
    /* DS:0x53a6 for the human nation; AI nations use 1 (46d4 64627-64633).
     * The port's Europe screen caches the human's difficulty (eu->difficulty);
     * the nation-record view fills the same field. */
    const int threshold = ((v ? v->difficulty : 0) + 3) >> 1;
    if (europe_pool_tier_roll(st, 1, 15) <= threshold) {
      return (v && v->brewster) ? 0x13 : 0x1a; /* Petty Criminals */
    }
    if (europe_pool_tier_roll(st, 1, 10) <= threshold) {
      return (v && v->brewster) ? 0x13 : 0x19; /* Indentured Servants */
    }
    if (europe_pool_tier_roll(st, 1, 8) <= threshold) {
      return 0x13; /* Free Colonists (DOS 0x1c, drawn as Free Colonists) */
    }
  }

  for (int tries = 0;;) {
    bool any_non_expert = false;
    for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
      if (!v->filled[i] || !europe_job_is_expert(v->job[i])) {
        any_non_expert = true;
      }
    }
    if (!any_non_expert) {
      return 0x13; /* three experts already in the pool */
    }
    const int job = europe_pool_remap(europe_pool_expert_roll(st, 0x18));
    if (++tries > 100) {
      return job;
    }
    bool duplicate = false;
    for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
      /* DOS compares against all three slots — the one being refilled still
       * holds its outgoing profession at this point, so the class that just
       * left the pool is not immediately redrawn. */
      if (v->filled[i] && v->job[i] == job) {
        duplicate = true;
      }
    }
    (void)slot;
    if (!duplicate) {
      return job;
    }
  }
}

/* bugs.md: Brewster's ban covers the EXISTING pool too — slots rolled
 * before the flag rose still held Petty Criminals / Indentured Servants,
 * so the Recruit list and the docks disagreed with the Brewster pick
 * dialog. DOS FUN_4345_0342 case 0x14 walks the three pool slot bytes
 * (nation*0x13c - 0x77f6, viceroy_unpacked.c 73160-73168) and overwrites
 * 0x19/0x1a (servant/criminal) with 0x1c — job NONE, which FUN_38fd_4884
 * draws as Free Colonists (0x1c→0x13 label swap, 64719 / 68591). A direct
 * substitution, not a reroll (bugs.md #224 kept it idempotent).
 *
 * The 0x13 stored below is that swap applied at the store instead of at the
 * draw — the port's single pool convention, not a divergence (smell audit
 * 2026-09-10 G5 secondary, refuted):
 *   - europe_set_pool_slot folds 0x1c (and every unnamed job) to 0x13 on
 *     load, and europe_roll_pool_profession's own free tier returns 0x13
 *     where DOS returns 0x1c, so 0x13 is the port's Free Colonists byte
 *     everywhere in the pool;
 *   - the roll's duplicate check cannot tell them apart: it only ever
 *     compares against europe_pool_remap output (0..0x18), which is never
 *     0x13 nor 0x1c;
 *   - col1_bridge reads a saved 0x1c as "slot empty" (col1_bridge.c:1701)
 *     and would reroll a fully-Brewstered pool on the next load if this
 *     wrote the raw DOS byte. */
void europe_apply_brewster(EuropeScreen* eu, int owned) {
  if (!eu || !owned) {
    return;
  }
  eu->brewster_no_criminals = true;
  for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
    if (eu->pool[i].filled &&
        (eu->pool[i].profession == UNITS_JOB_SERVANT ||
         eu->pool[i].profession == UNITS_JOB_CRIMINAL)) {
      eu->pool[i].profession = EUROPE_POOL_JOB_FREE_COLONIST;
      snprintf(
        eu->pool[i].name, sizeof(eu->pool[i].name), "%s",
        europe_pool_job_name(EUROPE_POOL_JOB_FREE_COLONIST)
      );
    }
  }
}

static void europe_refill_pool_slot_impl(
  EuropeScreen* eu, int slot, bool force_expert, ColonizeDosRng* dos, unsigned* rng_state
) {
  if (!eu || slot < 0 || slot >= EUROPE_POOL_SIZE) {
    return;
  }
  /* See EuropePoolRng: with a bound stream the LFSR stand-in is seeded from
   * that stream's state (a read, not a draw); the treasury seed is the
   * no-rng fallback only. */
  unsigned local = dos ? (dos->state | 1u)
                       : 1u + (unsigned)(eu->gold + eu->recruit_passage + slot * 17);
  EuropePoolRng st;
  st.dos = dos;
  st.local = rng_state ? rng_state : &local;
  EuropePoolView view;
  europe_pool_view_from_screen(&view, eu);
  const int job = europe_roll_pool_profession(&view, slot, force_expert, &st);
  EuropePoolSlot* p = &eu->pool[slot];
  snprintf(p->name, sizeof(p->name), "%s", europe_pool_job_name(job));
  p->profession = job;
  p->filled = true;
}

/* DOS 64776 (the Recruit-click tail) calls 46d4(0) — the tier roll. Audit
 * SC-24: this used to be a three-deep wrapper stack (_ex over _impl, plain
 * over _ex) with no external caller for either intermediate. */
static void europe_refill_pool_slot(EuropeScreen* eu, int slot, unsigned* rng_state) {
  europe_refill_pool_slot_impl(eu, slot, false, NULL, rng_state);
}

void europe_refill_pool_slot_rng(
  EuropeScreen* eu, int slot, bool force_expert, ColonizeDosRng* rng
) {
  europe_refill_pool_slot_impl(eu, slot, force_expert, rng, NULL);
}

/* Restore one pool slot from a saved nation+2..+4 job byte. 0x1c (job NONE,
 * what DOS stores for "nothing here") and anything unnamed fall back to
 * Free Colonists, the label 4884's 0x1c→0x13 swap gives it. */
void europe_set_pool_slot(EuropeScreen* eu, int slot, int profession) {
  if (!eu || slot < 0 || slot >= EUROPE_POOL_SIZE) {
    return;
  }
  EuropePoolSlot* p = &eu->pool[slot];
  /* Anything outside the pool's own candidate set (including 0x1c "job
   * NONE") becomes a Free Colonist, the swap label 4884 does. The test used
   * to be a strcmp against the English name, which a translated NAMES.TXT
   * would have broken (audit SC-13). */
  if (profession < 0 || profession > 0x1a || europe_pool_cand_index(profession) < 0) {
    profession = EUROPE_POOL_JOB_FREE_COLONIST;
  }
  snprintf(p->name, sizeof(p->name), "%s", europe_pool_job_name(profession));
  p->profession = profession;
  p->filled = true;
}

void europe_pool_ensure_filled(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
    if (!eu->pool[i].filled) {
      europe_refill_pool_slot(eu, i, NULL);
    }
  }
}

const char* europe_pool_label(const EuropeScreen* eu, int slot) {
  if (!eu || slot < 0 || slot >= EUROPE_POOL_SIZE) {
    return "Colonist";
  }
  const EuropePoolSlot* p = &eu->pool[slot];
  return (p->filled && p->name[0]) ? p->name : "Colonist";
}

/*
 * Game-start recruit pool — DOS `FUN_38fd_6024` (viceroy_unpacked.c:68707-
 * 68729). Difficulty is DS:0x53a6, 0 Discoverer … 4 Viceroy (difficulty.md).
 * Slot 0 is a fixed bottom-tier class (`0x53a6 < 4` -> Indentured Servants,
 * i.e. below Viceroy; Petty Criminals at Viceroy); slots 1 and 2 are 46d4
 * rolls, slot 1 forced to the expert half on `0x53a6 < 3` — below GOVERNOR,
 * so Discoverer/Explorer/Conquistador — and slot 2 always. The human player
 * then gets a hand-picked easy opener, gated `0x53a6 == 0` / `== 1`: at
 * Discoverer all three slots become Master Carpenters / Expert Farmers /
 * Seasoned Scouts, at EXPLORER only slots 1 and 2 do, and Conquistador and
 * above get nothing. Spain then forces slot 0 to Jesuit Missionaries on top
 * of all of that (LAB_38fd_6161).
 */
void europe_seed_pool(EuropeScreen* eu, int difficulty, bool human) {
  if (!eu) {
    return;
  }
  if (difficulty < 0) {
    difficulty = 0;
  }
  if (difficulty > 8) {
    difficulty = 8;
  }
  const uint8_t saved_difficulty = eu->difficulty;
  eu->difficulty = (uint8_t)difficulty;
  unsigned rng = 42u;
  for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
    eu->pool[i].filled = false;
    eu->pool[i].profession = -1;
    eu->pool[i].name[0] = '\0';
  }
  if (EUROPE_POOL_SIZE > 0) {
    const int job = (difficulty < 4) ? 0x19 : 0x1a;
    snprintf(eu->pool[0].name, sizeof(eu->pool[0].name), "%s", europe_pool_job_name(job));
    eu->pool[0].profession = job;
    eu->pool[0].filled = true;
  }
  if (EUROPE_POOL_SIZE > 1) {
    europe_refill_pool_slot_impl(eu, 1, difficulty < 3, NULL, &rng);
  }
  if (EUROPE_POOL_SIZE > 2) {
    europe_refill_pool_slot_impl(eu, 2, true, NULL, &rng);
  }
  if (human && difficulty <= 1) {
    static const int k_easy[3] = {
      COLONIZE_PROF_CARPENTER, COLONIZE_PROF_FARMER, UNITS_JOB_SCOUT
    };
    for (int i = (difficulty == 0) ? 0 : 1; i < EUROPE_POOL_SIZE && i < 3; ++i) {
      snprintf(eu->pool[i].name, sizeof(eu->pool[i].name), "%s", europe_pool_job_name(k_easy[i]));
      eu->pool[i].profession = k_easy[i];
      eu->pool[i].filled = true;
    }
  }
  /*
   * LAB_38fd_6161 (viceroy_unpacked.c 68731-68733): `if (DS:0x9e12 == 2)
   * *(byte*)(DS:0x84fc + 2) = 0x18;` — the bound nation being Spain forces
   * recruit slot 0 (record +2) to job 0x18, Jesuit Missionaries. It sits
   * after the human easy-opener block and is not gated on difficulty or on
   * human control, so it overwrites the Discoverer 0x0d pick too. Seed-time
   * only: 0x9e12 == 2 appears nowhere else in the image, so the pool refill
   * (FUN_38fd_4884) has no Spanish case and a re-rolled slot 0 is ordinary.
   * Smell audit #56.
   */
  if (EUROPE_POOL_SIZE > 0 && eu->bound_nation == 2) {
    const int job = UNITS_JOB_MISSIONARY; /* NAMES @JOB 24 = "Jesuit Missionaries" */
    snprintf(eu->pool[0].name, sizeof(eu->pool[0].name), "%s", europe_pool_job_name(job));
    eu->pool[0].profession = job;
    eu->pool[0].filled = true;
  }
  eu->difficulty = saved_difficulty;
  if (eu->brewster_no_criminals) {
    europe_apply_brewster(eu, 1);
  }
}

void europe_seed_campaign_prices(EuropeScreen* eu, ColonizeDosRng* rng) {
  /*
   * FUN_38fd_6024 price seed (viceroy_unpacked.c 68645-68654): new campaign
   * only — bid = FUN_281f_04d4(0, start_hi−start_lo) + start_lo per cargo,
   * inclusive both ends, broadcast to all four nations. Fixed 16 iterations
   * in cargo order; hi==lo still consumes a draw, so the LCG stream matches
   * DOS. No clamp to [low, high]. Smell audit #55.
   */
  if (!eu || !rng) {
    return;
  }
  for (int i = 0; i < EUROPE_CARGO_MAX; ++i) {
    EuropeCargoQuote* q = &eu->cargo[i];
    const int lo = q->start_lo;
    const int hi = q->start_hi >= lo ? q->start_hi : lo;
    int bid = dos_rng_range(rng, lo, hi);
    if (i >= eu->cargo_count) {
      continue; /* draw consumed (DOS loops all 16 slots), table row absent */
    }
    if (bid < 0) {
      bid = 0;
    }
    q->bid = bid;
    q->ask = q->bid + q->burden;
  }
}

static void europe_init_pool(EuropeScreen* eu) {
  /* Reset time: the real difficulty is not cached yet (europe_reset_campaign
   * zeroes it and the first EOT tick fills it in), so this is the Discoverer
   * seed; the new-game bridge calls europe_seed_pool again once the wizard's
   * difficulty is known. */
  europe_seed_pool(eu, eu->difficulty, true);
}

/*
 * DOS `FUN_38fd_41ce` (the Train dialog) collects every @JOB with a positive
 * hire cost in job order, exactly as the loop below does, and then hands the
 * cost array and the parallel job-id array to `FUN_291f_0ed0` ->
 * `FUN_1cf8_000a` before drawing a single row. That routine is a sort: it
 * walks for the first descending step, lifts that element out (shifting the
 * tail left), finds the first slot whose key is >= the lifted key, shifts
 * right and drops it in — an ascending sort by cost. So the Train list is
 * ordered cheapest-first, not by @JOB index the way this port had it
 * (bugs.md).
 *
 * Transcribed rather than replaced with a qsort because the tie order is
 * observable and is this algorithm's own: an element only moves on a strictly
 * descending step, and re-enters *before* every equal key. With stock
 * NAMES.TXT that puts Carpenters before Fishermen at 1000, Farmers before
 * Distiller at 1100, and Pioneers before Tobacconists at 1200. Costs come
 * from NAMES.TXT, so a modded table has to re-sort the same way.
 */
static void europe_sort_train_by_cost(EuropeTrainOption* a, int n) {
  if (!a || n < 2) {
    return;
  }
  int i = 0;
  while (i < n - 1) {
    if (a[i + 1].cost >= a[i].cost) {
      ++i;
      continue;
    }
    const EuropeTrainOption lifted = a[i + 1];
    for (int k = 0; k < n - 2 - i; ++k) {
      a[i + 1 + k] = a[i + 2 + k];
    }
    int pos = 0;
    while (pos < n - 1 && a[pos].cost < lifted.cost) {
      ++pos;
    }
    for (int k = n - 2; k >= pos; --k) {
      a[k + 1] = a[k];
    }
    a[pos] = lifted;
    /* DOS does not rewind `local_12` here — the scan resumes where it was. */
  }
}

/*
 * @CARGO burden column, cached from the last table load so screens that hold
 * no EuropeScreen (the Economic report's fallback row) can still compute the
 * DOS ask price `euro_price + burden` (FUN_38fd_0016). Smell audit #63.
 */
static int g_europe_cargo_burden[EUROPE_CARGO_MAX];
static int g_europe_cargo_burden_count;

/* ===================== Cargo tables, nation setup & campaign reset (europe_cargo_burden .. europe_reset_campaign_nation) ===================== */
int europe_cargo_burden(int cargo_type) {
  if (cargo_type < 0 || cargo_type >= g_europe_cargo_burden_count) {
    return 0;
  }
  return g_europe_cargo_burden[cargo_type];
}

bool europe_load_tables(EuropeScreen* eu, const ColonizeMsgCatalog* names) {
  eu->cargo_count = 0;
  eu->class_count = 0;
  eu->train_count = 0;

  const ColonizeMsgSection* cargo = assets_msg_find(names, "CARGO");
  if (cargo) {
    for (int i = 0; i < cargo->line_count && eu->cargo_count < EUROPE_CARGO_MAX; ++i) {
      char line[COLONIZE_MSG_LINE_LEN];
      snprintf(line, sizeof(line), "%s", cargo->lines[i]);
      if (line[0] == ';' || line[0] == '\0') {
        continue;
      }
      char* comma = strchr(line, ',');
      if (!comma) {
        continue;
      }
      *comma = '\0';
      str_trim(line);
      if (line[0] == '\0') {
        continue;
      }

      const char* p = comma + 1;
      int start_lo = 0;
      int start_hi = 0;
      int low = 0;
      int high = 0;
      int burden = 0;
      int rise = 0;
      int fall = 0;
      int attrition = 0;
      int volatility = 0;
      if (!europe_parse_int_field(&p, &start_lo) || !europe_parse_int_field(&p, &start_hi) ||
          !europe_parse_int_field(&p, &low) || !europe_parse_int_field(&p, &high) ||
          !europe_parse_int_field(&p, &burden) || !europe_parse_int_field(&p, &rise) ||
          !europe_parse_int_field(&p, &fall) || !europe_parse_int_field(&p, &attrition) ||
          !europe_parse_int_field(&p, &volatility)) {
        continue;
      }
      EuropeCargoQuote* q = &eu->cargo[eu->cargo_count++];
      str_copy_trunc(q->name, sizeof(q->name), line);
      q->start_lo = start_lo;
      q->start_hi = start_hi;
      /* Table load leaves bid at the band floor; a new campaign then rolls
       * bid within [start_lo, start_hi] (europe_seed_campaign_prices) and a
       * loaded save overwrites it with euro_price. */
      q->bid = start_lo;
      if (q->bid < 0) {
        q->bid = 0;
      }
      q->low = low;
      q->high = high;
      q->burden = burden;
      q->rise = rise;
      q->fall = fall;
      q->attrition = attrition;
      q->volatility = volatility;
      if (q->volatility < 0) {
        q->volatility = 0;
      }
      if (q->volatility > 15) {
        q->volatility = 15;
      }
      q->ask = q->bid + q->burden;
      g_europe_cargo_burden[eu->cargo_count - 1] = q->burden;
      if (eu->cargo_count > g_europe_cargo_burden_count) {
        g_europe_cargo_burden_count = eu->cargo_count;
      }
    }
  }
  memset(eu->trade_nr, 0, sizeof(eu->trade_nr));

  const ColonizeMsgSection* classes = assets_msg_find(names, "CLASS");
  if (classes) {
    for (int i = 0; i < classes->line_count && eu->class_count < EUROPE_CLASS_MAX; ++i) {
      char line[COLONIZE_MSG_LINE_LEN];
      snprintf(line, sizeof(line), "%s", classes->lines[i]);
      if (line[0] == ';' || line[0] == '\0') {
        continue;
      }
      char* comma = strchr(line, ',');
      if (!comma) {
        continue;
      }
      *comma = '\0';
      str_trim(line);
      const char* p = comma + 1;
      int cost = 0;
      if (!europe_parse_int_field(&p, &cost) || cost <= 0 || line[0] == '\0') {
        continue;
      }
      EuropeRecruitClass* c = &eu->classes[eu->class_count++];
      str_copy_trunc(c->name, sizeof(c->name), line);
      c->cost = cost;
    }
  }

  /* @JOB: name, expert_name, school_tier, europe_hire_cost */
  const ColonizeMsgSection* jobs = assets_msg_find(names, "JOB");
  if (jobs) {
    int job_index = 0;
    for (int i = 0; i < jobs->line_count && eu->train_count < EUROPE_TRAIN_MAX; ++i) {
      char line[COLONIZE_MSG_LINE_LEN];
      snprintf(line, sizeof(line), "%s", jobs->lines[i]);
      if (line[0] == ';' || line[0] == '\0') {
        continue;
      }
      char* c1 = strchr(line, ',');
      if (!c1) {
        continue;
      }
      *c1 = '\0';
      str_trim(line);
      char* c2 = strchr(c1 + 1, ',');
      if (!c2) {
        ++job_index;
        continue;
      }
      *c2 = '\0';
      char expert[40];
      snprintf(expert, sizeof(expert), "%s", c1 + 1);
      str_trim(expert);
      const char* p = c2 + 1;
      int tier = 0;
      int cost = 0;
      if (!europe_parse_int_field(&p, &tier) || !europe_parse_int_field(&p, &cost)) {
        ++job_index;
        continue;
      }
      (void)tier;
      (void)line;
      if (cost > 0 && expert[0] != '\0') {
        EuropeTrainOption* t = &eu->train[eu->train_count++];
        snprintf(t->expert_name, sizeof(t->expert_name), "%s", expert);
        t->job_index = job_index;
        t->cost = cost;
      }
      ++job_index;
    }
    europe_sort_train_by_cost(eu->train, eu->train_count);
  }

  const ColonizeMsgSection* home = assets_msg_find(names, "HOMEPORT");
  const ColonizeMsgSection* cname = assets_msg_find(names, "COLONYNAME");
  (void)home;
  (void)cname;

  europe_init_purchase_table(eu);
  return eu->cargo_count > 0;
}

void europe_set_nation(EuropeScreen* eu, int nation, const ColonizeMsgCatalog* names) {
  /* @HOMEPORT / @COUNTRY come from reports.c's shared NAMES.TXT accessors
   * now (audit SC-6/SC-7). The `names` catalog handed in here is read first
   * when it has the section — it is the *caller's* catalog, which may be a
   * different one from reports_load's. */
  /* NAMES.TXT @COLONYNAME through the shared sim-side parse, for callers
   * that hand in no catalog of their own. Copied at once: the accessor's
   * scratch buffer is reused by the next name lookup. */
  char shared_region[48];
  {
    const char* r = reports_names_field("COLONYNAME", nation < 0 || nation > 3 ? 0 : nation, 0);
    snprintf(shared_region, sizeof(shared_region), "%s", r ? r : "");
  }
  if (!eu) {
    return;
  }
  if (nation < 0 || nation > 3) {
    nation = 0;
  }
  if (names) {
    const ColonizeMsgSection* home = assets_msg_find(names, "HOMEPORT");
    const ColonizeMsgSection* reg = assets_msg_find(names, "COLONYNAME");
    if (home && nation >= 0 && nation < home->line_count) {
      char line[COLONIZE_MSG_LINE_LEN];
      snprintf(line, sizeof(line), "%s", home->lines[nation]);
      str_trim(line);
      if (line[0] && line[0] != ';') {
        str_copy_trunc(eu->port_city, sizeof(eu->port_city), line);
      } else {
        str_copy_trunc(eu->port_city, sizeof(eu->port_city), reports_home_port_name(nation));
      }
    } else {
      str_copy_trunc(eu->port_city, sizeof(eu->port_city), reports_home_port_name(nation));
    }
    if (reg && nation >= 0 && nation < reg->line_count) {
      char line[COLONIZE_MSG_LINE_LEN];
      snprintf(line, sizeof(line), "%s", reg->lines[nation]);
      str_trim(line);
      if (line[0] && line[0] != ';') {
        str_copy_trunc(eu->colony_region, sizeof(eu->colony_region), line);
      } else {
        str_copy_trunc(eu->colony_region, sizeof(eu->colony_region), shared_region);
      }
    } else {
      str_copy_trunc(eu->colony_region, sizeof(eu->colony_region), shared_region);
    }
  } else {
    str_copy_trunc(eu->port_city, sizeof(eu->port_city), reports_home_port_name(nation));
    str_copy_trunc(eu->colony_region, sizeof(eu->colony_region), shared_region);
  }
  str_copy_trunc(eu->nation_name, sizeof(eu->nation_name), reports_nation_country_name(nation));
  /* FUN_38fd_0000(nation): DS:0x9e12 = nation, DS:0x84fc = its record
   * (viceroy_unpacked.c 58696-58702). The trade-volume term reads 0x9e12 for
   * the human test and the Dutch slot-3 damping. */
  eu->bound_nation = (uint8_t)nation;
}

int europe_voyage_turns_roll(ColonizeDosRng* rng, bool magellan, int ship_count) {
  if (!rng) {
    return 1;
  }
  /* 48d3:0042 RNG(1,100) always rolled; >0x59 && ship_counts>2 && !FF5 → 2. */
  const int roll = dos_rng_range(rng, 1, 100);
  if (roll > 89 && ship_count > 2 && !magellan) {
    return 2;
  }
  return 1;
}

int europe_clamp_voyage_turns(int t) {
  if (t < 1) {
    return 1;
  }
  if (t > EUROPE_VOYAGE_TURNS_MAX) {
    return EUROPE_VOYAGE_TURNS_MAX;
  }
  return t;
}

void europe_reset_campaign(EuropeScreen* eu) {
  europe_reset_campaign_nation(eu, 0);
}

void europe_reset_campaign_nation(EuropeScreen* eu, int nation) {
  if (!eu) {
    return;
  }
  if (nation < 0 || nation > 3) {
    nation = 0;
  }
  europe_set_nation(eu, nation, NULL);
  eu->gold = 1000;
  eu->tax_percent = 0;
  eu->current_crosses = 0;
  /* Match new-game Col1 human needed seed (COLONY00); first EOT overwrites via 584a. */
  eu->needed_crosses = 9;
  eu->crosses_immigrant_seen = false;
  eu->liberty_bells_total = 0;
  eu->liberty_bells_last_turn = 0;
  eu->harbor_ships = 0;
  eu->expected_ships = 0;
  eu->bound_ships = 0;
  memset(eu->harbor, 0, sizeof(eu->harbor));
  memset(eu->expected, 0, sizeof(eu->expected));
  memset(eu->bound, 0, sizeof(eu->bound));
  eu->selected_harbor = -1;
  eu->selected_market = 0;
  eu->dock_count = 0;
  memset(eu->dock, 0, sizeof(eu->dock));
  eu->recruit_count = 0;
  eu->difficulty = 0; /* first EOT tick caches the real col1 difficulty */
  eu->bound_human = true; /* the port binds the screen to the human nation */
  europe_refresh_recruit_passage(eu);
  europe_init_pool(eu);
  europe_init_purchase_table(eu);
  eu->menu = EUROPE_MENU_NONE;
  eu->menu_selection = 0;
  eu->menu_dock_index = -1;
  eu->last_exit_valid = false;
  eu->open_on_dock = false;
  eu->price_event_count = 0;
  eu->immigration_score = 0;
  eu->immigration_pressure = 0;
  eu->boycott_bitmap = 0;
  /* DOS FUN_38fd_6024: recruit pool (+2..+4) filled; docks empty; pressure 0. */
  europe_set_status(eu, "Home port ready. Recruit / Purchase / Train / S Sail.");
}
