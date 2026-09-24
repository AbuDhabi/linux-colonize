#ifndef COLONIZE_CORE_EUROPE_INTERNAL_H
#define COLONIZE_CORE_EUROPE_INTERNAL_H

/* ===== Cross-file seams of the europe.c split (2026-09-23) =====
 * europe.c was 5.4k lines; it is now split along its banners into
 * europe{,_pool,_dock,_harbor,_market}.c. The declarations below are the
 * only symbols used across those files; everything else stayed `static` in
 * its own file. Code moved verbatim - these are the only de-static'd names,
 * and they keep the `europe_` file-scope prefix.
 * ============================================================= */

#include <stdbool.h>
#include <stddef.h>

#include "core/europe.h"
#include "core/ai_popup.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/units.h"

/* @JOB id of Free Colonists — the pool's "nothing special here" value and
 * what the DOS label 4884 swaps 0x1c (job NONE) for. */
#define EUROPE_POOL_JOB_FREE_COLONIST 19

typedef struct EuropePoolRng {
  ColonizeDosRng* dos; /* shared game stream; NULL for fixture callers */
  unsigned* local;     /* LFSR stand-in state; never NULL */
} EuropePoolRng;

/*
 * The three pool-slot bytes `FUN_38fd_46d4` reads (nation+2..+4 = DS:0x84fc+2)
 * plus the two scalars it consults (DS:0x53a6 difficulty — substituted with 1
 * for a non-human bound nation — and FF 0x14 ownership). The human Europe
 * screen and the raw `ColonizeCol1Nation.recruit[3]` of an AI nation both
 * project onto this, so the roll below is shared instead of duplicated.
 */
typedef struct EuropePoolView {
  int job[EUROPE_POOL_SIZE];
  bool filled[EUROPE_POOL_SIZE];
  int difficulty;
  bool brewster;
} EuropePoolView;

/* --- owned by europe.c --- */
int europe_pool_tier_roll(EuropePoolRng* r, int lo, int hi);
int europe_pool_expert_roll(EuropePoolRng* r, int hi);
bool europe_parse_int_field(const char** cursor, int* out);
void europe_set_status(EuropeScreen* eu, const char* text);
void europe_copy_ship(EuropeHarborShip* dst, const EuropeHarborShip* src);
void europe_clear_ship(EuropeHarborShip* s);
int europe_goods_slots_used(const EuropeHarborShip* ship);
int europe_ship_cargo_cap(const EuropeHarborShip* ship, const ColonizeUnitPool* units);
int europe_ship_free_slots(const EuropeHarborShip* ship, const ColonizeUnitPool* units);
bool europe_dock_name_is_artillery(const char* name);

/* Recruit pool slot `pool_index` onto the dock (europe_dock.c); `rng` may be
 * NULL. Shared by europe_menu_confirm_ex (europe_market.c). */
bool europe_recruit_from_pool_ex(EuropeScreen* eu, int pool_index, struct ColonizeDosRng* rng);
int europe_dock_type_row_of_name(const char* name);
void europe_disembark_passengers_to_dock(
  EuropeScreen* eu,
  EuropeHarborShip* ship,
  const ColonizeUnitPool* units
);
int europe_purse_nation(const EuropeScreen* eu);
void europe_purse_move(
  EuropeScreen* eu, struct ColonizeCol1Save* col1, int nation, long delta
);
void europe_credit_sale_tax(
  struct ColonizeCol1Save* col1, int nation, int gross, int net
);
extern void (*g_europe_sound_play)(int id);
extern void (*g_europe_set_bgm)(int pool);
extern struct ColonizeCol1Save* g_europe_live_save;
extern AiPopupState* g_europe_popups;
/* --- owned by europe_pool.c --- */
const char* europe_pool_job_name(int profession);
int europe_roll_pool_profession(
  const EuropePoolView* v, int slot, bool force_expert, EuropePoolRng* st
);
int europe_clamp_voyage_turns(int t);
/* --- owned by europe_dock.c --- */
void europe_refresh_recruit_passage(EuropeScreen* eu);
const char* europe_arm_row_name(int row);
bool europe_apply_dock_menu_row_ex(
  EuropeScreen* eu,
  ColonizeUnitPool* units,
  ColonizeCol1Save* col1,
  int nation_id,
  int dock_index,
  int row
);
/* --- owned by europe_harbor.c --- */
void europe_board_sentry_dockers(
  EuropeScreen* eu,
  EuropeHarborShip* ship,
  ColonizeUnitPool* units,
  int nation_id,
  int cargo_cap
);

#endif /* COLONIZE_CORE_EUROPE_INTERNAL_H */
