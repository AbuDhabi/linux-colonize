/*
 * Sections:
 *  - Recruit, train & purchase commit flows (europe_compute_recruit_passage .. europe_open_recruit_menu)
 *  - Dock unit typing, kit application & mirror units (europe_dock_unit_dos_type .. europe_remove_dock_mirror_unit)
 *  - Dock arms buy/sell menu (europe_arm_buy_cost .. europe_pop_dock_immigrant_ex)
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
static void europe_bump_recruit_count(EuropeScreen* eu);
static int europe_dock_type_roll(
  const EuropeScreen* eu, const char* name, int profession, ColonizeDosRng* rng
);
static int europe_dock_type_tools(int dos_type);
static int europe_dock_type_muskets(int dos_type);
static int europe_dock_type_horses(int dos_type);
static void europe_retype_dock_mirror_unit(
  ColonizeUnitPool* units,
  int nation_id,
  int profession,
  int from_dos_type,
  int to_dos_type
);
static int europe_arm_buy_cost(const EuropeScreen* eu, int cargo, int qty);
static int europe_arm_sell_gain(const EuropeScreen* eu, int cargo, int qty);
static bool europe_arm_row_enabled(
  const EuropeScreen* eu,
  const EuropeDockImmigrant* d,
  int row,
  int* out_price
);
static bool europe_pop_dock_immigrant_ex(
  EuropeScreen* eu,
  char* out_name,
  size_t out_name_size,
  int* out_profession
);

/* ===================== Recruit, train & purchase commit flows (europe_compute_recruit_passage .. europe_open_recruit_menu) ===================== */
int europe_compute_recruit_passage(
  int recruit_count, int difficulty, int current_crosses, int needed_crosses
) {
  if (difficulty < 0) {
    difficulty = 0;
  }
  if (difficulty > 8) {
    difficulty = 8;
  }
  const long base = (long)(recruit_count + difficulty + 7) * 20;
  long floor_val = base / 5;
  if (floor_val < 100) {
    floor_val = 100;
  }
  /* -(needed_crosses+1): DOS's own divide-by-zero guard (~X == -X-1). */
  const long denom = -((long)needed_crosses + 1);
  const long discount = ((base - floor_val) * (long)current_crosses) / denom;
  long passage = base + discount;
  if (passage < 10) {
    passage = 10;
  }
  return (int)passage;
}

void europe_refresh_recruit_passage(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  eu->recruit_passage = europe_compute_recruit_passage(
    eu->recruit_count, eu->difficulty, eu->current_crosses, eu->needed_crosses
  );
}

static void europe_bump_recruit_count(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  if (eu->recruit_count < 180) {
    eu->recruit_count++;
  }
  europe_refresh_recruit_passage(eu);
}

/*
 * DOS FUN_38fd_0718 (raw 59098-59140) is the ONE harbor-spawn behind every
 * dock arrival — recruit (64432/64768), the crosses immigrant (68585) and
 * the AI hires (90665/92616) all reach it through thunk_FUN_291f_0b26 — so
 * the Soldier->Dragoon roll happens on every one of them, off the shared
 * stream. europe_dock_type_for's name lookup stays in front of it (the port
 * files purchases by @UNIT name, which DOS spawns by a different path).
 */
static int europe_dock_type_roll(
  const EuropeScreen* eu, const char* name, int profession, ColonizeDosRng* rng
) {
  const int row = europe_dock_type_row_of_name(name);
  if (row >= 0) {
    return row;
  }
  return europe_dock_unit_dos_type(
    profession, eu ? (int)eu->difficulty : 0, eu ? eu->bound_human : true, rng
  );
}

bool europe_recruit_from_pool_ex(EuropeScreen* eu, int pool_index, ColonizeDosRng* rng) {
  if (!eu || pool_index < 0 || pool_index >= EUROPE_POOL_SIZE) {
    return false;
  }
  if (!eu->pool[pool_index].filled) {
    europe_set_status(eu, "That recruit is unavailable.");
    return false;
  }
  if (eu->dock_count >= EUROPE_DOCK_MAX) {
    europe_set_status(eu, "Docks are full.");
    return false;
  }
  if (!europe_recruit_affordable(eu)) {
    /*
     * FUN_38fd_4884 raw 64736-64741: when the nation's 32-bit purse cannot
     * cover the passage DOS greys the row (FUN_291f_01b6(list, row, 1)) and
     * the pick does nothing — no status line, no popup. bugs.md #588.
     */
    return false;
  }
  eu->gold -= eu->recruit_passage;
  EuropeDockImmigrant* slot = &eu->dock[eu->dock_count++];
  memset(slot, 0, sizeof(*slot));
  snprintf(slot->name, sizeof(slot->name), "%s", eu->pool[pool_index].name);
  slot->profession = eu->pool[pool_index].profession;
  slot->present = true;
  slot->sentry = true;
  slot->dos_type = europe_dock_type_roll(eu, slot->name, slot->profession, rng);
  snprintf(
    eu->status,
    sizeof(eu->status),
    "Recruited %s (-%d$).",
    slot->name,
    eu->recruit_passage
  );
  diag_info(
    "EUROPE recruited %s (job %d) for %d$ passage (gold=%d)",
    slot->name, slot->profession, eu->recruit_passage, eu->gold
  );
  /*
   * FUN_38fd_4884 tail, param_1==0 (viceroy_unpacked.c:64765): a *paid*
   * passage clears the crosses meter (nation+0x2e = 0) before the +6
   * counter bumps. Without it the discount term kept the next price pinned
   * near the 100 floor, so buying colonists never walked the price ladder
   * up (bugs.md: "recruit price isn't increased by recruiting with gold").
   */
  eu->current_crosses = 0;
  eu->immigration_pressure = 0;
  europe_bump_recruit_count(eu);
  /* 64776: the emptied slot is refilled by `46d4(0)` off the shared stream. */
  europe_refill_pool_slot_rng(eu, pool_index, false, rng);
  return true;
}

bool europe_recruit_free_from_pool_ex(
  EuropeScreen* eu, int pool_index, ColonizeDosRng* rng
) {
  if (!eu || pool_index < 0 || pool_index >= EUROPE_POOL_SIZE) {
    return false;
  }
  if (!eu->pool[pool_index].filled) {
    europe_refill_pool_slot_rng(eu, pool_index, false, rng);
  }
  if (eu->dock_count >= EUROPE_DOCK_MAX) {
    europe_set_status(eu, "Docks are full.");
    return false;
  }
  /* FUN_38fd_4884 with param_1 != 0: passage forced to 0, the +6 recruit
   * counter and the +0x2e crosses word are left alone (64695-64697, 64778). */
  EuropeDockImmigrant* slot = &eu->dock[eu->dock_count++];
  memset(slot, 0, sizeof(*slot));
  snprintf(slot->name, sizeof(slot->name), "%s", eu->pool[pool_index].name);
  slot->profession = eu->pool[pool_index].profession;
  slot->present = true;
  slot->sentry = true;
  slot->dos_type = europe_dock_type_roll(eu, slot->name, slot->profession, rng);
  snprintf(eu->status, sizeof(eu->status), "%s joins the docks.", slot->name);
  europe_refill_pool_slot_rng(eu, pool_index, false, rng);
  return true;
}

bool europe_brewster_pick_from_pool_ex(
  EuropeScreen* eu, int pool_index, ColonizeDosRng* rng
) {
  if (!eu || !europe_recruit_free_from_pool_ex(eu, pool_index, rng)) {
    return false;
  }
  /* 4884 tail with param_1==0: +0x2e crosses zeroed after the pick; no +6
   * recruit-count bump (param_2!=0 skips it). */
  eu->current_crosses = 0;
  eu->immigration_pressure = 0;
  /* bugs.md #672: no nation_flags 0x40 latch on the Brewster branch — 5e52
   * raw 68601 sets it only inside the 07b4(nation,0x14)==0 arm. */
  /* bugs.md #223: do NOT clear open_on_dock here — that flag belongs to a
   * ship ARRIVAL (DS:0x14c). A Brewster pick answered after the end of turn
   * was wiping the pending auto-open of the ship that had just docked. */
  europe_refresh_recruit_passage(eu);
  snprintf(eu->status, sizeof(eu->status), "Immigrant arrives in Europe.");
  return true;
}

bool europe_immigrant_from_pool(EuropeScreen* eu, ColonizeDosRng* rng) {
  if (!eu || eu->dock_count >= EUROPE_DOCK_MAX) {
    return false;
  }
  /* DOS `5e52` raw 68578: recruit[04d4(0,2)] is used unconditionally — an
   * unfilled slot reads as job 0x1c and spawns Free Colonists (bugs.md #676;
   * the old "first filled slot / force-refill slot 0" fallback was invented). */
  int slot = 0;
  if (rng) {
    slot = dos_rng_range(rng, 0, EUROPE_POOL_SIZE - 1);
  }
  char name[sizeof(eu->pool[0].name)];
  int profession;
  if (eu->pool[slot].filled) {
    snprintf(name, sizeof(name), "%s", eu->pool[slot].name);
    profession = eu->pool[slot].profession;
  } else {
    snprintf(name, sizeof(name), "%s", europe_pool_job_name(EUROPE_POOL_JOB_FREE_COLONIST));
    profession = EUROPE_POOL_JOB_FREE_COLONIST;
  }
  /* 68583: the slot is refilled with `46d4((turn & 3) == 0)` BEFORE the
   * 0b26/0718 harbor spawn (raw 68585) rolls the Soldier->Dragoon type, all
   * off the same shared stream (bugs.md #671). The 4884 Recruit-click paths
   * really are spawn-then-refill and stay as they are. */
  europe_refill_pool_slot_rng(eu, slot, eu->pool_force_expert, rng);
  EuropeDockImmigrant* d = &eu->dock[eu->dock_count++];
  memset(d, 0, sizeof(*d));
  snprintf(d->name, sizeof(d->name), "%s", name);
  d->profession = profession;
  d->present = true;
  d->sentry = true;
  d->dos_type = europe_dock_type_roll(eu, d->name, d->profession, rng);
  /* DOS 0718 harbor-spawn does NOT bump Europe+6 — only 4884's own real
   * Recruit-click tail does (see europe_compute_recruit_passage). */
  return true;
}

/*
 * FUN_38fd_41ce raw 64399-64403: the Train dialog compares the nation's
 * 32-bit purse (+0x2a low / +0x2c high) against the @JOB cost and calls
 * FUN_291f_01b6(list, row, 1) — greys the row out — when the purse is
 * short, so an unaffordable expert cannot be picked at all. bugs.md #566.
 */
bool europe_recruit_affordable(const EuropeScreen* eu) {
  return eu && eu->gold >= eu->recruit_passage;
}

bool europe_train_affordable(const EuropeScreen* eu, int train_index) {
  if (!eu || train_index < 0 || train_index >= eu->train_count) {
    return false;
  }
  return eu->gold >= eu->train[train_index].cost;
}

bool europe_train(EuropeScreen* eu, int train_index) {
  return europe_train_ex(eu, train_index, NULL);
}

bool europe_train_ex(EuropeScreen* eu, int train_index, ColonizeDosRng* rng) {
  if (!eu || train_index < 0 || train_index >= eu->train_count) {
    return false;
  }
  if (eu->dock_count >= EUROPE_DOCK_MAX) {
    europe_set_status(eu, "Docks are full.");
    return false;
  }
  const EuropeTrainOption* t = &eu->train[train_index];
  if (eu->gold < t->cost) {
    snprintf(eu->status, sizeof(eu->status), "Need %d$ for %s.", t->cost, t->expert_name);
    return false;
  }
  eu->gold -= t->cost;
  EuropeDockImmigrant* slot = &eu->dock[eu->dock_count++];
  memset(slot, 0, sizeof(*slot));
  snprintf(slot->name, sizeof(slot->name), "%s", t->expert_name);
  slot->profession = t->job_index;
  slot->present = true;
  slot->sentry = true;
  slot->dos_type = europe_dock_type_roll(eu, slot->name, slot->profession, rng);
  snprintf(eu->status, sizeof(eu->status), "Trained %s (-%d$).", t->expert_name, t->cost);
  diag_info("EUROPE trained %s for %d$ (gold=%d)", t->expert_name, t->cost, eu->gold);
  return true;
}

int europe_purchase_cost(const EuropeScreen* eu, int purchase_index) {
  if (!eu || purchase_index < 0 || purchase_index >= eu->purchase_count) {
    return -1;
  }
  const EuropePurchaseOption* p = &eu->purchase[purchase_index];
  int cost = p->gold;
  /* FUN_38fd_4b50: type 0xb (Artillery) costs base + nation+0x1e * 100. */
  if (p->kind == UNITS_KIND_ARTILLERY && eu->artillery_bought > 0) {
    cost += eu->artillery_bought * 100;
  }
  return cost;
}

bool europe_purchase(EuropeScreen* eu, int purchase_index) {
  return europe_purchase_ex(eu, purchase_index, NULL);
}

/* bugs.md #754: FUN_38fd_4b50 raw 64858-64862 greys a row the purse cannot
 * cover and the pick does nothing — no status line, no popup. */
bool europe_purchase_affordable(const EuropeScreen* eu, int purchase_index) {
  if (!eu || purchase_index < 0 || purchase_index >= eu->purchase_count) {
    return false;
  }
  return eu->gold >= europe_purchase_cost(eu, purchase_index);
}

bool europe_purchase_open_confirm(EuropeScreen* eu, int purchase_index) {
  if (!eu || purchase_index < 0 || purchase_index >= eu->purchase_count) {
    return false;
  }
  if (!europe_purchase_affordable(eu, purchase_index)) {
    return false;
  }
  const EuropePurchaseOption* p = &eu->purchase[purchase_index];
  const int cost = europe_purchase_cost(eu, purchase_index);
  /*
   * FUN_38fd_4b50 raw 64868-64870: the Artillery escalation counter
   * (nation+0x1e) bumps right here, before the @REALLYBUY popup is even
   * shown — unconditionally of the eventual Yes/No answer. A cancelled
   * purchase still leaves the next Artillery row 100$ pricier.
   */
  if (p->kind == UNITS_KIND_ARTILLERY) {
    eu->artillery_bought += 1;
  }
  eu->purchase_confirming = true;
  eu->purchase_confirm_index = purchase_index;
  eu->purchase_confirm_cost = cost;
  /* DOS default (@REALLYBUY has no @default directive) is the first choice,
   * "Yes" — UI row 0 here, GAME.TXT order verbatim (raw 64874 `iVar6 == 1`
   * = row 0 = Yes; row 1 = No). europe_menu_confirm_ex special-cases
   * purchase_confirming ahead of its generic sel==0 cancel path. */
  eu->menu_selection = 0;
  snprintf(eu->status, sizeof(eu->status), "Purchase %s for %d$?", p->name, cost);
  diag_info("EUROPE purchase confirm %s for %d$ (gold=%d)", p->name, cost, eu->gold);
  return true;
}

bool europe_purchase_commit(
  EuropeScreen* eu, int purchase_index, int cost, ColonizeDosRng* rng
) {
  if (!eu || purchase_index < 0 || purchase_index >= eu->purchase_count) {
    return false;
  }
  const EuropePurchaseOption* p = &eu->purchase[purchase_index];
  if (p->is_ship) {
    if (eu->harbor_ships >= EUROPE_HARBOR_MAX) {
      europe_set_status(eu, "Harbor is full.");
      return false;
    }
    eu->gold -= cost;
    EuropeHarborShip* slot = &eu->harbor[eu->harbor_ships++];
    europe_clear_ship(slot);
    slot->type_index = -1; /* resolved by name in game_loop / caller */
    snprintf(slot->name, sizeof(slot->name), "%s", p->name);
    europe_refresh_harbor_selection(eu);
    snprintf(eu->status, sizeof(eu->status), "Purchased %s (-%d$).", p->name, cost);
    diag_info("EUROPE purchased ship %s for %d$ (gold=%d)", p->name, cost, eu->gold);
    return true;
  }
  if (eu->dock_count >= EUROPE_DOCK_MAX) {
    europe_set_status(eu, "Docks are full.");
    return false;
  }
  eu->gold -= cost;
  EuropeDockImmigrant* slot = &eu->dock[eu->dock_count++];
  memset(slot, 0, sizeof(*slot));
  snprintf(slot->name, sizeof(slot->name), "%s", p->name);
  slot->profession = -1;
  slot->present = true;
  slot->sentry = true;
  slot->dos_type = europe_dock_type_roll(eu, slot->name, slot->profession, rng);
  snprintf(eu->status, sizeof(eu->status), "Purchased %s (-%d$).", p->name, cost);
  diag_info("EUROPE purchased %s for %d$ (gold=%d)", p->name, cost, eu->gold);
  return true;
}

/*
 * bugs.md #753: DOS never buys on the spot from the PURCHASE list — it
 * fills @REALLYBUY's STRING0/NUMBER0 and asks a 2-choice popup
 * (FUN_281f_0652(0x111f, 2)), only debiting/spawning on choice 1 (Yes).
 * This wrapper stays for direct/legacy callers and tests: it opens the
 * confirm and immediately answers Yes, matching the old always-buy
 * behaviour without duplicating the debit logic.
 */
bool europe_purchase_ex(EuropeScreen* eu, int purchase_index, ColonizeDosRng* rng) {
  if (!europe_purchase_open_confirm(eu, purchase_index)) {
    return false;
  }
  const int cost = eu->purchase_confirm_cost;
  eu->purchase_confirming = false;
  return europe_purchase_commit(eu, purchase_index, cost, rng);
}

bool europe_open_recruit_menu(EuropeScreen* eu) {
  if (!eu) {
    return false;
  }
  europe_menu_open(eu, EUROPE_MENU_RECRUIT);
  return true;
}

/* ===================== Dock unit typing, kit application & mirror units (europe_dock_unit_dos_type .. europe_remove_dock_mirror_unit) ===================== */
int europe_dock_unit_dos_type(int profession, int difficulty, bool human, ColonizeDosRng* rng) {
  int type = 0; /* Colonists */
  if (profession == UNITS_JOB_PIONEER) {
    type = 2; /* Pioneers */
  } else if (profession == UNITS_JOB_MISSIONARY) {
    type = 3; /* Missionaries */
  } else if (profession == UNITS_JOB_SCOUT) {
    type = 5; /* Scouts */
  } else if (profession == UNITS_JOB_SOLDIER) {
    type = 1; /* Soldiers */
    if (rng) {
      const int bound = human ? difficulty : 1;
      if (dos_rng_range(rng, 0, bound + 4) == 0) {
        type = 4; /* Dragoons */
      }
    }
  }
  return type;
}

int europe_dock_type_for(const char* name, int profession) {
  const int row = europe_dock_type_row_of_name(name);
  if (row >= 0) {
    return row;
  }
  return europe_dock_unit_dos_type(profession, 0, true, NULL);
}

/*
 * FUN_38fd_3694 (raw 61183-61197): the dock caption on the status line —
 * 0056(1) opens the line, 0074 appends the @NATIONALITY adjective of the
 * bound nation (DS -0x72f6) and the @UNIT plural of the immigrant's type
 * (0x5230 + type*0xe); then, only when +0x315b (profession) != 0x1c, it
 * appends " (" + the @JOB name at `-0x715e + prof*8` + ")". That is @JOB
 * COLUMN 0 (the singular "Expert Farmer"): col0 = -0x715e, col1 = -0x715c,
 * col2 = -0x715a (raw 61193, ndisasm 0x00033C65 `push word [bx-0x715e]`;
 * cross-checked by FUN_49dd_0386 raw 78620 and FUN_15eb_3454 raw 13544).
 * bugs.md #613-wave #624: bugs.md #577 had been "fixed" onto the plural on
 * a bad cite — viceroy_overlays.c:61190 is the OVL03 `2f2b` colony site
 * (which really does read -0x715c), not this 3694 Europe dock caption.
 */
bool europe_dock_caption(const EuropeScreen* eu, int dock_index, char* out, size_t cap) {
  if (!eu || !out || cap == 0) {
    return false;
  }
  out[0] = 0;
  if (dock_index < 0 || dock_index >= eu->dock_count || dock_index >= EUROPE_DOCK_MAX) {
    return false;
  }
  const EuropeDockImmigrant* d = &eu->dock[dock_index];
  const char* adj = reports_nation_adjective_display_name((int)eu->bound_nation);
  const char* type_name = reports_dock_type_name(d->dos_type);
  const int prof = d->profession;
  const char* job_name = (prof >= 0 && prof != UNITS_JOB_NONE) ? reports_job_short_name(prof) : NULL;
  if (job_name && job_name[0]) {
    snprintf(out, cap, "%s %s (%s)", adj ? adj : "", type_name ? type_name : "", job_name);
  } else {
    snprintf(out, cap, "%s %s", adj ? adj : "", type_name ? type_name : "");
  }
  return true;
}

static int europe_dock_type_tools(int dos_type) {
  return dos_type == EUROPE_DOCK_TYPE_PIONEERS ? 100 : 0;
}

static int europe_dock_type_muskets(int dos_type) {
  return (dos_type == EUROPE_DOCK_TYPE_SOLDIERS || dos_type == EUROPE_DOCK_TYPE_DRAGOONS ||
          dos_type == (int)UNITS_KIND_CONT_ARMY || dos_type == (int)UNITS_KIND_CONT_CAV)
           ? 50
           : 0;
}

static int europe_dock_type_horses(int dos_type) {
  return (dos_type == EUROPE_DOCK_TYPE_DRAGOONS || dos_type == EUROPE_DOCK_TYPE_SCOUTS ||
          dos_type == (int)UNITS_KIND_CONT_CAV)
           ? 50
           : 0;
}

/*
 * bugs.md #669: a dock entry's dos_type is a DOS @UNIT type (== ColonizeUnitKind).
 * The six @ARMOPTIONS rows deal in 0..5; a Continental (7 / 9) disembarked in
 * Europe keeps its own row the way DOS keeps +0x3146 (dock caption
 * FUN_38fd_3694 raw 61183-61197 reads `0x5230 + type*0xe`).
 */
bool europe_dock_dos_type_is_valid(int dos_type) {
  return (dos_type >= 0 && dos_type < EUROPE_DOCK_TYPE_COUNT) ||
         dos_type == (int)UNITS_KIND_CONT_CAV || dos_type == (int)UNITS_KIND_CONT_ARMY;
}

/*
 * Audit SC-20/AE-18. The six dock type names are NAMES.TXT @UNIT rows 0..5
 * (reports_dock_type_name) — they were typed out twice in this file 24
 * lines apart, and a third time in ai_euro.c's ai_euro_5d04_linux_type_for
 * with singular fallbacks bolted on.
 *
 * `with_singular_fallback` is that third spelling: when the pool has no
 * "Soldiers" row, try "Soldier"; likewise Pioneer/Missionary/Dragoon/Scout,
 * and Colonists → "Free Colonist" → "Colonist". The Europe screen itself
 * always passes false (its pool is built from the same @UNIT rows, so the
 * plural always resolves); the 5d04 AI purchase path passes true because it
 * also runs against hand-built test pools.
 */
int europe_dock_unit_type_index_ex(
  const ColonizeUnitPool* units, int dos_type, bool with_singular_fallback
) {
  /* bugs.md #657: EUROPE_DOCK_TYPE_* (0..5) shares its order with
   * ColonizeUnitKind's COLONIST/SOLDIER/PIONEER/MISSIONARY/DRAGOON/SCOUT
   * (0..5) directly — DOS never round-trips through a name here at all, it
   * indexes the @UNIT row table by kind. This used to call
   * units_find_type(units, reports_dock_type_name(dos_type)) first and only
   * fall back to the row lookup, so an @UNIT pool whose plural name text
   * didn't match verbatim (or was empty) silently mis-typed a dock entry.
   * with_singular_fallback is now unused (kept in the signature: callers
   * pass true/false without changing behaviour) since the row lookup always
   * succeeds and never needs a fallback. */
  (void)with_singular_fallback;
  if (!units || !europe_dock_dos_type_is_valid(dos_type)) {
    return -1;
  }
  return units_kind_type_index(units, (ColonizeUnitKind)dos_type);
}

int europe_dock_unit_type_index(const ColonizeUnitPool* units, int dos_type) {
  return europe_dock_unit_type_index_ex(units, dos_type, false);
}

/*
 * @UNIT display type for a dock entry — what unit_chrome uses to place the
 * orders/allegiance box (Dragoons/Scouts top-left, Artillery top-center,
 * everyone else bottom-right), matching what units_display_type_index
 * derives from a landed unit's kit. A dock entry's name is a profession
 * ("Veteran Soldiers"), never an @UNIT type, so looking the name up first
 * missed and fell back to Colonists: every armed or mounted immigrant wore
 * the plain bottom-right box, a Veteran Dragoon most visibly (bugs.md).
 * dos_type is the field the @ARMOPTIONS rows move around and is what
 * europe_dock_sprite already picks the sprite from; Artillery carries no
 * dos_type of its own and is name-flagged there, so it is here too.
 */
int europe_dock_display_type_index(
  const ColonizeUnitPool* units, const EuropeDockImmigrant* d
) {
  if (!units || !d) {
    return -1;
  }
  int ti = -1;
  if (europe_dock_name_is_artillery(d->name)) {
    ti = units_kind_type_index(units, UNITS_KIND_ARTILLERY);
  }
  if (ti < 0) {
    ti = europe_dock_unit_type_index(units, d->dos_type);
  }
  if (ti < 0) {
    ti = units_find_type(units, d->name);
  }
  if (ti < 0) {
    ti = units_kind_type_index(units, UNITS_KIND_COLONIST);
  }
  return ti;
}

void europe_apply_dock_unit_kit(ColonizeUnit* u, int dos_type) {
  if (!u) {
    return;
  }
  /*
   * DOS's harbor spawn writes only the Tools byte (+0x3159) because
   * muskets and horses ride on the @UNIT type itself. This port keeps them
   * as explicit unit fields, so set all three from the type — which is also
   * what the @ARMOPTIONS rows need when they move an immigrant between
   * Colonists / Soldiers / Pioneers / Dragoons / Scouts.
   */
  u->tools = europe_dock_type_tools(dos_type);
  u->muskets = europe_dock_type_muskets(dos_type);
  u->horses = europe_dock_type_horses(dos_type);
}

/*
 * Re-type and re-kit the (236,236) mirror unit behind a dock entry after an
 * @ARMOPTIONS row changed it. Matched on profession the same way
 * europe_remove_dock_mirror_unit matches, with the pre-change type as the
 * tie-break so two immigrants of the same profession do not swap.
 */
static void europe_retype_dock_mirror_unit(
  ColonizeUnitPool* units,
  int nation_id,
  int profession,
  int from_dos_type,
  int to_dos_type
) {
  if (!units || nation_id < 0 || nation_id > 3) {
    return;
  }
  const int from_ti = europe_dock_unit_type_index(units, from_dos_type);
  const int to_ti = europe_dock_unit_type_index(units, to_dos_type);
  if (to_ti < 0) {
    return;
  }
  int fallback = -1;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id || u->x != 236 || u->y != 236 ||
        u->aboard_ship_id >= 0 || units_is_sea(units, u->id)) {
      continue;
    }
    if (u->profession != profession) {
      continue;
    }
    if (from_ti >= 0 && u->type_index == from_ti) {
      u->type_index = to_ti;
      europe_apply_dock_unit_kit(u, to_dos_type);
      return;
    }
    if (fallback < 0) {
      fallback = i;
    }
  }
  if (fallback >= 0) {
    units->units[fallback].type_index = to_ti;
    europe_apply_dock_unit_kit(&units->units[fallback], to_dos_type);
  }
}

int europe_spawn_dock_mirror_unit(
  ColonizeUnitPool* units,
  int nation_id,
  int profession,
  int difficulty,
  bool human,
  ColonizeDosRng* rng
) {
  if (!units || nation_id < 0 || nation_id > 3) {
    return -1;
  }
  const int dos_type = europe_dock_unit_dos_type(profession, difficulty, human, rng);
  int ti = europe_dock_unit_type_index(units, dos_type);
  if (ti < 0) {
    ti = units_kind_type_index(units, UNITS_KIND_COLONIST);
  }
  const int id = units_spawn_allow_stack(units, ti >= 0 ? ti : 0, 236, 236);
  ColonizeUnit* u = units_get(units, id);
  if (!u) {
    return -1;
  }
  units_set_nation(u, nation_id);
  u->orders = UNITS_ORDER_SENTRY; /* DOS +0x314c = 1 */
  u->profession = profession;
  u->goto_x = 0;
  u->goto_y = 0;
  u->moves = 0;
  europe_apply_dock_unit_kit(u, dos_type);
  return id;
}

void europe_remove_dock_mirror_unit(ColonizeUnitPool* units, int nation_id, int profession) {
  if (!units || nation_id < 0 || nation_id > 3) {
    return;
  }
  int fallback = -1;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id || u->x != 236 || u->y != 236 ||
        u->aboard_ship_id >= 0 || units_is_sea(units, u->id)) {
      continue;
    }
    if (u->profession == profession) {
      (void)units_disband(units, u->id);
      return;
    }
    if (fallback < 0) {
      fallback = u->id;
    }
  }
  if (fallback >= 0) {
    (void)units_disband(units, fallback);
  }
}

/*
 * GAME.TXT @ARMOPTIONS — the menu behind a click on a dock immigrant.
 *
 * Prices, from DOS 38fd:3745..3830: Muskets are @CARGO 0x0f at 50 a set,
 * Tools 0x0e at 100, Horses 0x08 at 50, each priced with the buy accessor
 * (FUN_281f_0c3e) or the sell one (FUN_281f_09ea) times that quantity.
 * Which of the pair a row shows is the unit's @UNIT type: Muskets sell for
 * Soldiers or Dragoons and buy otherwise, Tools sell for Pioneers, Horses
 * sell for Dragoons or Scouts.
 */
/* ===================== Dock arms buy/sell menu (europe_arm_buy_cost .. europe_pop_dock_immigrant_ex) ===================== */
static int europe_arm_buy_cost(const EuropeScreen* eu, int cargo, int qty) {
  return europe_buy_price(eu, cargo) * qty;
}

/*
 * Disarm/de-equip proceeds are UNTAXED — smell audit #62, REFUTED: DOS credits
 * the raw gross.
 *
 * The three sell rows of the @ARMOPTIONS switch (FUN_38fd_3746's jump table at
 * 38fd:3c5a; Ghidra leaves the case bodies as raw bytes, so read them in
 * original_sources_decompiled/viceroy_ndisasm.asm at file offsets 0x3402E.. —
 * segment base 38fd = 0x30550) each do exactly two things:
 *
 *   38fd:3b4a  Sell Muskets  add [bx+0x2a],[bp-0x5e]   ; treasury += sell·50
 *                            call 291f_0a2e(0x0f, 0x32)
 *   38fd:3ba0  Sell Tools    add [bx+0x2a],[bp-0x5c]   ; treasury += sell·100
 *                            call 291f_0a2e(0x0e, 0x64)
 *   38fd:3bfc  Sell Horses   add [bx+0x2a],[bp-0x5a]   ; treasury += sell·50
 *                            call 291f_0a2e(0x08, 0x32)
 *
 * where [bp-0x5e/0x5c/0x5a] are the prologue's sell_price(cargo)·qty (38fd:3ce4
 * ..3d0e, via the 291f_09ea accessor) and 291f_0a2e is the sell ledger
 * FUN_38fd_1dfa (viceroy_unpacked.c 60247-60299).
 *
 * The harbor cargo sale is the contrast: FUN_38fd_23c4 (viceroy 60557-60575)
 * takes 1f0c's gross, computes tax = tax_rate·gross/100, credits only
 * gross − tax to the treasury and books tax into nation +0x22 (royal_money)
 * and the net into +0x26. None of that appears at the arm rows — no tax split,
 * no royal_money, no +0x26 — so the disarm gross lands whole in the treasury.
 * The Crown's cut is still reflected in the per-cargo revenue ledger, which is
 * FUN_38fd_1dfa's own (100−tax) term, applied below by the ledger call — the
 * `europe_apply_trade_volume` in europe_apply_dock_menu_row_ex, which must be
 * handed a non-NULL col1 for that ledger to move at all.
 */
static int europe_arm_sell_gain(const EuropeScreen* eu, int cargo, int qty) {
  return europe_sell_price(eu, cargo) * qty;
}

/*
 * Per-row enable, transcribed from the arm-enable dispatch at 38fd:3e0c..3f59
 * (`dec ax; cmp ax,0xb; jmp [cs:bx+0x3a1a]` at 38fd:3f5d). `price` is DOS's
 * [bp-0x6a]: non-zero only on the three buy rows, and the tail greys the
 * row when the treasury cannot cover it. A boycotted cargo disables its rows
 * outright (the FUN_38fd_68c7 test each arm row runs), and an Indian Convert
 * (@JOB 0x1b) can be neither armed, equipped nor blessed: the four disables
 * are ndisasm raw 0x33E2F (buy muskets), 0x33E8C (buy tools), 0x33EF7
 * (buy horses), 0x33F32 (bless).
 */
static bool europe_arm_row_enabled(
  const EuropeScreen* eu,
  const EuropeDockImmigrant* d,
  int row,
  int* out_price
) {
  const int t = d->dos_type;
  const bool convert = (d->profession == UNITS_JOB_CONVERT);
  *out_price = 0;
  switch (row) {
    case EUROPE_ARM_ROW_NO_BOARD:
      return d->sentry;
    case EUROPE_ARM_ROW_BOARD:
      return !d->sentry;
    case EUROPE_ARM_ROW_TO_FRONT:
      /* DS:0x9e2c is this immigrant's place in the dock queue. */
      return eu->menu_dock_index > 0;
    case EUROPE_ARM_ROW_BUY_MUSKETS:
      *out_price = europe_arm_buy_cost(eu, COLONIZE_CARGO_MUSKETS, EUROPE_ARM_MUSKETS);
      if (europe_cargo_boycotted(eu, COLONIZE_CARGO_MUSKETS)) {
        return false;
      }
      return !convert &&
             (t == EUROPE_DOCK_TYPE_COLONISTS || t == EUROPE_DOCK_TYPE_SCOUTS);
    case EUROPE_ARM_ROW_SELL_MUSKETS:
      if (europe_cargo_boycotted(eu, COLONIZE_CARGO_MUSKETS)) {
        return false;
      }
      return t == EUROPE_DOCK_TYPE_SOLDIERS || t == EUROPE_DOCK_TYPE_DRAGOONS;
    case EUROPE_ARM_ROW_BUY_TOOLS:
      *out_price = europe_arm_buy_cost(eu, COLONIZE_CARGO_TOOLS, EUROPE_ARM_TOOLS);
      if (europe_cargo_boycotted(eu, COLONIZE_CARGO_TOOLS)) {
        return false;
      }
      return !convert && t == EUROPE_DOCK_TYPE_COLONISTS;
    case EUROPE_ARM_ROW_SELL_TOOLS:
      if (europe_cargo_boycotted(eu, COLONIZE_CARGO_TOOLS)) {
        return false;
      }
      return t == EUROPE_DOCK_TYPE_PIONEERS;
    case EUROPE_ARM_ROW_BUY_HORSES:
      *out_price = europe_arm_buy_cost(eu, COLONIZE_CARGO_HORSES, EUROPE_ARM_HORSES);
      if (europe_cargo_boycotted(eu, COLONIZE_CARGO_HORSES)) {
        return false;
      }
      return !convert &&
             (t == EUROPE_DOCK_TYPE_COLONISTS || t == EUROPE_DOCK_TYPE_SOLDIERS);
    case EUROPE_ARM_ROW_SELL_HORSES:
      if (europe_cargo_boycotted(eu, COLONIZE_CARGO_HORSES)) {
        return false;
      }
      return t == EUROPE_DOCK_TYPE_SCOUTS || t == EUROPE_DOCK_TYPE_DRAGOONS;
    case EUROPE_ARM_ROW_BLESS:
      return !convert && t == EUROPE_DOCK_TYPE_COLONISTS;
    case EUROPE_ARM_ROW_UNBLESS:
      /*
       * DOS 38fd:39ec..3a09: enabled when the @UNIT type is Missionaries
       * (0x3146 == 3) AND the profession is NOT @JOB 0x18 (raw 0x33F4A =
       * 38fd:3f4a `cmp byte [bx+0x315b],0x18; jnz enable`) — i.e. only a *blessed*
       * ordinary colonist can cancel Missionary status; a born Jesuit
       * Missionary specialist cannot. Was inverted.
       */
      return t == EUROPE_DOCK_TYPE_MISSIONARIES && d->profession != UNITS_JOB_MISSIONARY;
    case EUROPE_ARM_ROW_NO_CHANGES:
    default:
      return true;
  }
}

void europe_build_dock_menu(
  EuropeScreen* eu,
  const struct ColonizeMsgCatalog* messages,
  int dock_index
) {
  if (!eu) {
    return;
  }
  eu->dock_menu_count = 0;
  if (dock_index < 0 || dock_index >= eu->dock_count) {
    return;
  }
  const EuropeDockImmigrant* d = &eu->dock[dock_index];
  const int t = d->dos_type;
  const bool sell_muskets =
    (t == EUROPE_DOCK_TYPE_SOLDIERS || t == EUROPE_DOCK_TYPE_DRAGOONS);
  const bool sell_tools = (t == EUROPE_DOCK_TYPE_PIONEERS);
  const bool sell_horses =
    (t == EUROPE_DOCK_TYPE_DRAGOONS || t == EUROPE_DOCK_TYPE_SCOUTS);

  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.has_number0 = true;
  tok.number0 = sell_muskets
                  ? europe_arm_sell_gain(eu, COLONIZE_CARGO_MUSKETS, EUROPE_ARM_MUSKETS)
                  : europe_arm_buy_cost(eu, COLONIZE_CARGO_MUSKETS, EUROPE_ARM_MUSKETS);
  tok.has_number1 = true;
  tok.number1 = sell_tools
                  ? europe_arm_sell_gain(eu, COLONIZE_CARGO_TOOLS, EUROPE_ARM_TOOLS)
                  : europe_arm_buy_cost(eu, COLONIZE_CARGO_TOOLS, EUROPE_ARM_TOOLS);
  tok.has_number2 = true;
  tok.number2 = sell_horses
                  ? europe_arm_sell_gain(eu, COLONIZE_CARGO_HORSES, EUROPE_ARM_HORSES)
                  : europe_arm_buy_cost(eu, COLONIZE_CARGO_HORSES, EUROPE_ARM_HORSES);

  /* GAME.TXT @ARMOPTIONS is required at startup (assets_validate_required_files);
   * a missing catalog is already fatal, so this fallback carries no MicroProse
   * wording — a catalog miss here just shows blank rows. */
  static const char* const k_fallback[EUROPE_DOCK_MENU_MAX] = {
    "", "", "", "", "", "", "", "", "", "", "", ""
  };
  const ColonizeMsgSection* sec =
    messages ? assets_msg_find((const ColonizeMsgCatalog*)messages, "ARMOPTIONS") : NULL;

  int row = 0; /* 0-based index into the section's non-directive lines */
  const int line_count = (sec && sec->line_count > 0) ? sec->line_count : EUROPE_DOCK_MENU_MAX;
  for (int i = 0; i < line_count && row < EUROPE_DOCK_MENU_MAX; ++i) {
    const char* line = (sec && i < sec->line_count) ? sec->lines[i] : k_fallback[row];
    if (sec && (popup_msg_is_directive(line) || line[0] == '\0')) {
      continue;
    }
    const int row_id = row + 1;
    ++row;
    int price = 0;
    if (!europe_arm_row_enabled(eu, d, row_id, &price)) {
      continue; /* DOS omits a disabled row rather than greying it. */
    }
    const int slot = eu->dock_menu_count;
    popup_msg_apply_tokens(
      eu->dock_menu_label[slot], sizeof(eu->dock_menu_label[slot]), line, &tok
    );
    eu->dock_menu_row[slot] = (uint8_t)row_id;
    eu->dock_menu_greyed[slot] = (price > 0 && eu->gold < price);
    eu->dock_menu_count++;
  }
}

/* @ARMOPTIONS row id -> readable name, for the debug log only. */
const char* europe_arm_row_name(int row) {
  switch (row) {
    case EUROPE_ARM_ROW_NO_BOARD: return "no board";
    case EUROPE_ARM_ROW_BOARD: return "board";
    case EUROPE_ARM_ROW_TO_FRONT: return "to front";
    case EUROPE_ARM_ROW_BUY_MUSKETS: return "buy muskets";
    case EUROPE_ARM_ROW_SELL_MUSKETS: return "sell muskets";
    case EUROPE_ARM_ROW_BUY_TOOLS: return "buy tools";
    case EUROPE_ARM_ROW_SELL_TOOLS: return "sell tools";
    case EUROPE_ARM_ROW_BUY_HORSES: return "buy horses";
    case EUROPE_ARM_ROW_SELL_HORSES: return "sell horses";
    case EUROPE_ARM_ROW_BLESS: return "bless";
    case EUROPE_ARM_ROW_UNBLESS: return "unbless";
    case EUROPE_ARM_ROW_NO_CHANGES: return "no changes";
    default: return "row";
  }
}

bool europe_apply_dock_menu_row_ex(
  EuropeScreen* eu,
  ColonizeUnitPool* units,
  ColonizeCol1Save* col1,
  int nation_id,
  int dock_index,
  int row
) {
  if (!eu || dock_index < 0 || dock_index >= eu->dock_count) {
    return false;
  }
  EuropeDockImmigrant* d = &eu->dock[dock_index];
  const int from = d->dos_type;
  int to = from;
  int gold_delta = 0;
  int ledger_cargo = -1;
  int ledger_qty = 0;
  int ledger_is_buy = 0;
  int sound = -1;

  switch (row) {
    case EUROPE_ARM_ROW_NO_BOARD:
      d->sentry = false;
      /* GAME.TXT @ARMOPTIONS row 0 ("Don't get on next ship."). */
      europe_set_status(eu, assets_msg_line_or(eu->messages, "ARMOPTIONS", 0, ""));
      return true;
    case EUROPE_ARM_ROW_BOARD:
      d->sentry = true;
      /* GAME.TXT @ARMOPTIONS row 1 ("Board next ship."). */
      europe_set_status(eu, assets_msg_line_or(eu->messages, "ARMOPTIONS", 1, ""));
      return true;
    case EUROPE_ARM_ROW_TO_FRONT: {
      if (dock_index <= 0) {
        return false;
      }
      const EuropeDockImmigrant moved = *d;
      for (int i = dock_index; i > 0; --i) {
        eu->dock[i] = eu->dock[i - 1];
      }
      eu->dock[0] = moved;
      eu->menu_dock_index = 0;
      /* GAME.TXT @ARMOPTIONS row 2 ("Move to front of dock."). */
      europe_set_status(eu, assets_msg_line_or(eu->messages, "ARMOPTIONS", 2, ""));
      return true;
    }
    /* 38fd:3ade — Scouts become Dragoons, anyone else Soldiers. */
    case EUROPE_ARM_ROW_BUY_MUSKETS:
      to = (from == EUROPE_DOCK_TYPE_SCOUTS) ? EUROPE_DOCK_TYPE_DRAGOONS
                                             : EUROPE_DOCK_TYPE_SOLDIERS;
      gold_delta = -europe_arm_buy_cost(eu, COLONIZE_CARGO_MUSKETS, EUROPE_ARM_MUSKETS);
      ledger_cargo = COLONIZE_CARGO_MUSKETS;
      ledger_qty = EUROPE_ARM_MUSKETS;
      ledger_is_buy = 1;
      sound = 0x58;
      break;
    case EUROPE_ARM_ROW_SELL_MUSKETS:
      to = (from == EUROPE_DOCK_TYPE_DRAGOONS) ? EUROPE_DOCK_TYPE_SCOUTS
                                               : EUROPE_DOCK_TYPE_COLONISTS;
      gold_delta = europe_arm_sell_gain(eu, COLONIZE_CARGO_MUSKETS, EUROPE_ARM_MUSKETS);
      ledger_cargo = COLONIZE_CARGO_MUSKETS;
      ledger_qty = EUROPE_ARM_MUSKETS;
      break;
    case EUROPE_ARM_ROW_BUY_TOOLS:
      to = EUROPE_DOCK_TYPE_PIONEERS;
      gold_delta = -europe_arm_buy_cost(eu, COLONIZE_CARGO_TOOLS, EUROPE_ARM_TOOLS);
      ledger_cargo = COLONIZE_CARGO_TOOLS;
      ledger_qty = EUROPE_ARM_TOOLS;
      ledger_is_buy = 1;
      break;
    case EUROPE_ARM_ROW_SELL_TOOLS:
      to = EUROPE_DOCK_TYPE_COLONISTS;
      gold_delta = europe_arm_sell_gain(eu, COLONIZE_CARGO_TOOLS, EUROPE_ARM_TOOLS);
      ledger_cargo = COLONIZE_CARGO_TOOLS;
      ledger_qty = EUROPE_ARM_TOOLS;
      break;
    /* 38fd:3bbe — Soldiers become Dragoons, anyone else Scouts. */
    case EUROPE_ARM_ROW_BUY_HORSES:
      to = (from == EUROPE_DOCK_TYPE_SOLDIERS) ? EUROPE_DOCK_TYPE_DRAGOONS
                                               : EUROPE_DOCK_TYPE_SCOUTS;
      gold_delta = -europe_arm_buy_cost(eu, COLONIZE_CARGO_HORSES, EUROPE_ARM_HORSES);
      ledger_cargo = COLONIZE_CARGO_HORSES;
      ledger_qty = EUROPE_ARM_HORSES;
      ledger_is_buy = 1;
      sound = 0x5c;
      break;
    case EUROPE_ARM_ROW_SELL_HORSES:
      to = (from == EUROPE_DOCK_TYPE_DRAGOONS) ? EUROPE_DOCK_TYPE_SOLDIERS
                                               : EUROPE_DOCK_TYPE_COLONISTS;
      gold_delta = europe_arm_sell_gain(eu, COLONIZE_CARGO_HORSES, EUROPE_ARM_HORSES);
      ledger_cargo = COLONIZE_CARGO_HORSES;
      ledger_qty = EUROPE_ARM_HORSES;
      break;
    case EUROPE_ARM_ROW_BLESS:
      to = EUROPE_DOCK_TYPE_MISSIONARIES;
      sound = 0x8024; /* the same church chord the colony assign plays */
      break;
    case EUROPE_ARM_ROW_UNBLESS:
      to = EUROPE_DOCK_TYPE_COLONISTS;
      break;
    case EUROPE_ARM_ROW_NO_CHANGES:
    default:
      return true;
  }

  if (gold_delta < 0 && eu->gold < -gold_delta) {
    europe_set_status(eu, "The treasury cannot afford that.");
    return false;
  }
  /* 38fd:3b4a/3ba0/3bfc `add [bx+0x2a],..` — the bound record's treasury, so
   * the col1 word moves with the purse when a save is bound (audit G3). */
  europe_purse_move(eu, col1, nation_id, gold_delta);
  d->dos_type = to;
  if (ledger_cargo >= 0) {
    /*
     * Ledger only — no rise/fall step. The arm rows call the volume routines
     * bare (291f_0c14 = FUN_38fd_1d80 on a buy, 291f_0a2e = FUN_38fd_1dfa on a
     * sell) and nothing else; the 0058 threshold pass is what the *harbor*
     * handlers add at their tail (thunk_FUN_291f_0cbc(0, cargo) —
     * viceroy_unpacked.c 60500 for the buy FUN_38fd_1fa2, 60644 for the sell
     * FUN_38fd_23c4). Arming an immigrant therefore never moves the bid, so it
     * never raises @PRICEUP/@PRICEDOWN either. europe_apply_volume_price would
     * have run 0058, so call the full form with immediate_threshold = 0.
     */
    /*
     * `col1` is passed through so the per-cargo tons/tons2/gold ledger really
     * is written, as the disarm-tax note 300 lines above promises: DOS's
     * FUN_38fd_1dfa (viceroy_unpacked.c 60272-60295) adds the amount into
     * nation+0xbc and +0xfc and the (100−tax)-scaled gross into +0x7c on every
     * call, and the three arm sell rows call it bare. NULL is still accepted
     * (tests, and any caller with no save bound) and then only the price pool
     * moves, exactly as on the harbor channels.
     */
    const int bound = (int)eu->bound_nation;
    europe_apply_trade_volume(
      eu, col1, bound, bound, ledger_cargo, ledger_qty, ledger_is_buy, 0
    );
  }
  diag_info(
    "EUROPE dock %s: %s (%d) type %d -> %d, gold %+d (gold=%d)",
    d->name[0] ? d->name : "immigrant",
    europe_arm_row_name(row), row, from, to, gold_delta, eu->gold
  );
  if (units) {
    europe_retype_dock_mirror_unit(units, nation_id, d->profession, from, to);
  }
  if (sound >= 0 && g_europe_sound_play) {
    g_europe_sound_play(sound);
  }
  return true;
}

bool europe_apply_dock_menu_row(
  EuropeScreen* eu,
  ColonizeUnitPool* units,
  int nation_id,
  int dock_index,
  int row
) {
  return europe_apply_dock_menu_row_ex(eu, units, NULL, nation_id, dock_index, row);
}

static bool europe_pop_dock_immigrant_ex(
  EuropeScreen* eu,
  char* out_name,
  size_t out_name_size,
  int* out_profession
);

bool europe_pop_dock_immigrant(EuropeScreen* eu, char* out_name, size_t out_name_size) {
  return europe_pop_dock_immigrant_ex(eu, out_name, out_name_size, NULL);
}

static bool europe_pop_dock_immigrant_ex(
  EuropeScreen* eu,
  char* out_name,
  size_t out_name_size,
  int* out_profession
) {
  if (!eu || eu->dock_count <= 0) {
    return false;
  }
  if (out_name && out_name_size > 0) {
    snprintf(out_name, out_name_size, "%s", eu->dock[0].name);
  }
  if (out_profession) {
    *out_profession = eu->dock[0].profession;
  }
  for (int i = 1; i < eu->dock_count; ++i) {
    eu->dock[i - 1] = eu->dock[i];
  }
  eu->dock_count--;
  memset(&eu->dock[eu->dock_count], 0, sizeof(eu->dock[0]));
  return true;
}
