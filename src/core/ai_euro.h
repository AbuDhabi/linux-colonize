#ifndef COLONIZE_AI_EURO_H
#define COLONIZE_AI_EURO_H

#include "core/turn.h"

/*
 * Euro AI dispatcher T0 — FUN_521d_6d8e / 0a60 / 5d04 / 5b66 / 20e6.
 * Called from ai_euro_nation_turn after crosses.
 */

void ai_euro_dispatcher_turn(ColonizeTurnContext* ctx, int nation_id);

/*
 * FUN_4962_0018 ship probe (raw 78239-78299): reset the nation's
 * ship-pressure tallies, then per own colony clear + rescan the blockade
 * pair (+0x1b bits 0x01/0x02) and refold the tallies. DOS runs this for
 * EVERY nation each turn via the nation EOT FUN_3844_00f2 (control != 2,
 * human included) — the human entry point is turn.c's TURN_PROC_FINISH;
 * AI nations get the identical probe inside ai_euro_colony_goals. The
 * human Custom House blockade gate (europe.c, ai_flags & 3) reads it.
 */
void ai_euro_census_ship_pressure_refresh(ColonizeTurnContext* ctx, int nation_id);

/*
 * FUN_48d3_06ba head: tick this AI nation's Europe sailing lanes (port_bugs.md
 * #1056). Runs at the end of the nation's FUN_3844_00f2, before its
 * FUN_521d_6d8e turn. See ai_euro_ship.c.
 */
void ai_euro_europe_lane_tick(ColonizeTurnContext* ctx, int nation_id);

/*
 * FUN_15eb_28c8 — colonist work-plot job scoring, structural reference port
 * (docs/port_plan.md T1.17 / docs/port_plan.md W1.7). See ai_euro.c's own
 * header comment above the definition, and
 * original_sources_annotated/turn/colonist_work_plot_28c8.md, for scope and
 * fidelity notes. The structural entry point scores with plain (non-expert)
 * tile yields so tests/unit/test_ai_euro_28c8_job_score.c stays
 * hand-auditable; the live AI colony tick (ai_euro_colony_tick_5952,
 * W3.1 2026-08-29, DOS FUN_5952_035e) uses the same scorer with the
 * colonist's real profession.
 */
typedef struct AiEuro28c8JobCandidate {
  int job;   /* COLONIZE_JOB_*, or -1 if nothing scored */
  int tile;  /* 0..COLONIZE_COLONY_FIELD_TILES-1 */
  int score;
  int yield; /* winning tile's raw field yield (DOS DS:0x8dbe) */
} AiEuro28c8JobCandidate;

/* ai_euro_28c8_colonist_job_score_structural: test-only, ai_euro_internal.h. */

/*
 * FUN_15eb_2ea0 (raw 13162-13196) — the plot pass of DOS's colony recompute
 * FUN_15eb_3930. Seats a colonist who is not on a plot and is not a building
 * worker on the best-scored work plot (FUN_15eb_28c8), falling back to
 * Carpenter. Runs for HUMAN colonies too. `colonist_slot` >= 0 restricts it to
 * one colonist (the join path); -1 walks the roster.
 * colonies_seat_new_colonist is the sim-side entry point (port_bugs.md #562).
 */
void ai_euro_28c8_auto_assign_plots(ColonizeTurnContext* ctx, int colony_id, int colonist_slot);
/* CYCLE.DAT, which DOS FUN_7a9d_0004 loads raw to DS:0x929e: the 5952
 * cascade's past-the-table per-nation reads land in it (ai_euro_5952_ds_byte).
 * Returns false (and keeps zeros) when the file cannot be read. */
bool ai_euro_load_cycle_dat(const char* path);


/*
 * FUN_5bfb_10ec — Euro A↔B "war-worthy" eligibility by military balance
 * (static port 2026-08-27, T1.20). Returns 1 when A may reasonably go to
 * war with B. Continent tallies via the 20e6 accessors; the −0x6a9a
 * belligerence term is real since 2026-09-06d (`ai_diplo_leader_trait`),
 * and the DS:0xa153 byte gate is skipped (never resolved).
 */
int ai_euro_10ec_war_worthy(const ColonizeTurnContext* ctx, int a, int b);

/*
 * Wagon Train village-errand latch — DOS byte DS:(unit_index * 0x1c + 0x3158).
 * The unit array base is DS:0x3144 (stride 0x1c = sizeof COL1 unit record),
 * so this is COL1 unit record +0x14 = ColonizeCol1Unit.cargo_hold[4]
 * (record map: holds_occupied +0x0c, cargo_item nibbles +0x0d..+0x0f,
 * cargo_hold[6] +0x10..+0x15 — src/core/col1_save.h ColonizeCol1Unit;
 * cross-check: +0x315b = record +0x17 = `profession`, the Treasure gold/100
 * byte, and +0x314a = record +0x06 = `origin`).
 *
 * DOS touches the byte for type 0x0c (Wagon Train) only:
 *   set 1  — FUN_521d_20e6 load matrix, land arm (viceroy_unpacked.c:84817,
 *            :90364 in the second entry point);
 *   read   — FUN_521d_20e6 dead-end band, guarded by `type == '\f'`
 *            (viceroy_unpacked.c:85154/:89943);
 *   clear  — FUN_4d56_2820 trade entry for non-person types
 *            (viceroy_unpacked.c:82121), and unit spawn init only when
 *            `type == '\f'` (viceroy_unpacked.c:7752).
 * For every sea type +0x14 is a real cargo hold, so the bridge must never
 * reinterpret it there.
 *
 * These accessors let col1_bridge round-trip the latch through the save.
 */
unsigned char ai_euro_wagon_errand_get(int unit_id);
void ai_euro_wagon_errand_set(int unit_id, unsigned char value);
void ai_euro_wagon_errand_clear_all(void);

/*
 * FUN_5952_035e raw 555-563 — the colony `+0x1b` bit 0x10 (NEEDS_COLONISTS)
 * latch, exposed so tests/unit/test_ai_euro_5d04_hire.c can pin the
 * threshold without driving a whole nation turn. See the definition in
 * ai_euro.c for the full citation.
 */
int ai_euro_colony_needs_colonists_5952(
  const ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  const ColonizeColony* c
);

/*
 * The 5d04 hire chain's DOS @UNIT view of an OpenCol unit type (NAMES.TXT
 * @UNIT file order; -1 = unknown (ai_euro_20e6_dos_type); test-only ai_euro_5d04_dos_type_code in
 * ai_euro_internal.h), and the DS:0x5238 hull-space column for
 * a DOS @UNIT code. Exposed for tests — both are name-based, because the
 * chain runs on fixture pools whose indices are not DOS indices.
 */
int ai_euro_5d04_dos_type_space(const ColonizeUnitPool* pool, int dos_code);

/*
 * New-game / load hook (sibling of ai_goals_reset / founding_fathers_reset /
 * ai_contact_reset): clears this module's per-unit/colony/nation sticky
 * latches so they don't leak from a previous campaign in the same process.
 */
void ai_euro_reset(void);

/*
 * Test-only triage (golden_idle_campaign GOLDEN_IDLE_E7C4_ORACLE): when set,
 * its 0/1 replaces the residue-decided saw-foe word at the 20e6 0x46 border
 * park gate for a unit of `nation` on (x, y) (docs/dos_bugs.md DB-4); -1 keeps
 * the modelled word. NULL in the game.
 */
extern int (*ai_euro_e7c4_oracle)(int nation, int x, int y);

#endif
