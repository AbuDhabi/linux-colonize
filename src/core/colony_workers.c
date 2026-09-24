#include "core/colony.h"

/*
 * Sections:
 *  - School/workplace chrome popups & field/building work assignment (colonies_emit_noteacher_chrome .. colonies_clear_field)
 *  - Colonist admission, seating & auto-assignment of idle workers (colonies_admit_unit_w .. colonies_auto_assign_idle)
 *  - Colonist eject/dismiss flow & gear seizure on release (colonies_eject_role_name .. colonies_eject_colonist)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_popup.h"
#include "core/ai_euro.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/font.h"
#include "core/founding_fathers.h"
#include "core/ai_diplo.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/europe.h"
#include "core/ss.h"
#include "core/strutil.h"
#include "core/unit_chrome.h"
#include "core/units_cargo.h"
#include "platform/diagnostics.h"

#include "core/colony_internal.h"

/* ===================== School/workplace chrome popups & field/building work assignment (colonies_emit_noteacher_chrome .. colonies_clear_field) ===================== */
void colonies_emit_noteacher_chrome(
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!ai_popups) {
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    messages,
    "NOTEACHER",
    NULL,
    "",
    body,
    sizeof(body)
  );
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

/*
 * @SCHOOL1 / @COLLEGE2 / @UNIV3 (GAME.TXT:474-487) — the faculty cap refusal,
 * overlays.c:60459-60472 cases 9 / 8 / 7 (tag ids 0xc29 / 0xc20 / 0xc1a).
 * `owned_tier` is colonies_school_owned_tier's 1/2/3.
 */
void colonies_emit_school_faculty_chrome(
  int owned_tier,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!ai_popups || owned_tier < 1 || owned_tier > 3) {
    return;
  }
  static const char* const k_sections[3] = {"SCHOOL1", "COLLEGE2", "UNIV3"};
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(messages, k_sections[owned_tier - 1], NULL, "", body, sizeof(body));
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

void colonies_emit_need_school_chrome(
  int profession,
  int building_tier,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!ai_popups) {
    return;
  }
  const int shortfall = colonies_school_tier_shortfall(profession, building_tier);
  if (shortfall != 2 && shortfall != 3) {
    return;
  }
  const char* section = (shortfall == 3) ? "NEEDUNIVERSITY" : "NEEDCOLLEGE";
  const char* pname = colonies_profession_name(profession);
  char body[AI_POPUP_BODY_LEN];
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = pname;
  popup_msg_fill(messages, section, &tok, "", body, sizeof(body));
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

int colonies_building_worker_count(const ColonizeColony* colony, int building_type) {
  if (!colony) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < colony->colonist_count; ++i) {
    const ColonizeColonist* c = &colony->colonists[i];
    if (c->active && c->building_type == building_type) {
      n++;
    }
  }
  return n;
}

/*
 * bugs.md: only buildings with a real @JOB worker slot accept colonists.
 * Passive/structural buildings (fortifications, docks chain, Warehouse,
 * Stable, Custom House, Printing Press/Newspaper, Capitol) have no crew —
 * assigning there must be refused BEFORE any admit side effect.
 */
bool colonies_building_workable(const ColonizeColonyPool* pool, int building_type) {
  if (!pool || building_type < 0 || building_type >= pool->building_type_count) {
    return false;
  }
  /* Chains with no crew: fortifications, the docks line, Warehouse (+
   * Expansion), Stable, Custom House, Printing Press/Newspaper, Capitol. */
  switch (colonies_building_row_chain(colonies_building_type_row(pool, building_type))) {
    case COLONIES_CHAIN_FORTIFICATION:
    case COLONIES_CHAIN_DOCKS:
    case COLONIES_CHAIN_WAREHOUSE:
    case COLONIES_CHAIN_STABLE:
    case COLONIES_CHAIN_CUSTOM_HOUSE:
    case COLONIES_CHAIN_PRESS:
    case COLONIES_CHAIN_CAPITOL:
      return false;
    default:
      break;
  }
  return true;
}

/* debug.logs: colonist "#2 Master Carpenter" style tag for assign lines. */
static void colony_log_colonist(
  const ColonizeColony* col,
  int colonist_index,
  char* out,
  size_t out_size
) {
  if (!col || colonist_index < 0 || colonist_index >= col->colonist_count) {
    snprintf(out, out_size, "#%d", colonist_index);
    return;
  }
  snprintf(out, out_size, "#%d job=%d", colonist_index, col->colonists[colonist_index].profession);
}

/*
 * DS:0x2f4 (FUN_15eb_0aec raw 10087-10093) — @JOB -> required @BUILDING row,
 * read straight out of VICEROY.EXE (file offset 121248 + 0x2f4), 19 bytes:
 *   -1 x9 (the field jobs 0..8 need no building), then
 *   9 Distiller 27, 10 Tobacconist 24, 11 Weaver 21, 12 Fur Trader 32,
 *  13 Carpenter 35, 14 Blacksmith 39, 15 Gunsmith 3, 16 Preacher 37,
 *  17 Statesman 9, 18 Teacher 12.
 * DOS returns -1 for any job >= 0x13 (those are the leave-as rows, gated by
 * colonies_eject_row_offered instead).
 */
int colonies_job_required_building_row(int job) {
  static const int16_t k_job_building[COLONIES_JOB_TEACHER + 1] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1,
    COLONY_BUILDING_RUM_DISTILLERS_HOUSE,
    COLONY_BUILDING_TOBACCONISTS_HOUSE,
    COLONY_BUILDING_WEAVERS_HOUSE,
    COLONY_BUILDING_FUR_TRADERS_HOUSE,
    COLONY_BUILDING_CARPENTERS_SHOP,
    COLONY_BUILDING_BLACKSMITHS_HOUSE,
    COLONY_BUILDING_ARMORY,
    COLONY_BUILDING_CHURCH,
    COLONY_BUILDING_TOWN_HALL,
    COLONY_BUILDING_SCHOOLHOUSE
  };
  if (job < 0 || job > COLONIES_JOB_TEACHER) {
    return -1;
  }
  return (int)k_job_building[job];
}

/*
 * DOS-LITERAL FUN_15eb_3454 raw 13534-13554 — the `param_1 < 0x13` arm of the
 * jobs-menu row gate (FUN_281f_0bb4), whose answer FUN_2f2b_348c's row loop
 * (raw 50753 `if (local_e != 0)`) reads as "list this row at all".
 *   raw 13536-13539: `iVar2 = FUN_15eb_0aec(job); if (-1 < iVar2 &&
 *     FUN_15eb_038e(iVar2) == 0) local_4 = 0` — the required @BUILDING ROW
 *     itself must be owned, not merely some tier of its chain. So the nine
 *     field jobs are always listed and an indoor job appears only once its
 *     base building stands.
 *   raw 13540-13553 (job 0x12, Teacher): the colonist's own specialty level
 *     (@JOB column 2, 0x1c remapped to 0x19) decides — level 4 drops the row,
 *     level 3 needs @BUILDING 0xe (University), level 2 needs 0xd (College).
 *     The same OWNED-tier reading as colonies_school_owned_tier (#580/#589);
 *     colonies_school_tier_shortfall is that test, already shared with the
 *     seating validator.
 * Note DOS returns 0 (drop) here, never -1 (grey): greying is the leave-as
 * short-stock answer only.
 */
bool colonies_job_row_offered(
  const ColonizeColonyPool* pool, const ColonizeColony* col, int job, int profession
) {
  if (!pool || !col || job < 0 || job > COLONIES_JOB_TEACHER) {
    return false;
  }
  const int row = colonies_job_required_building_row(job);
  if (row >= 0 && !colonies_has_building_row(pool, col, (ColonizeBuildingRow)row)) {
    return false;
  }
  if (job == COLONIES_JOB_TEACHER) {
    if (!colonies_profession_may_teach(profession)) {
      return false;
    }
    if (colonies_school_tier_shortfall(profession, colonies_school_owned_tier(pool, col)) != 0) {
      return false;
    }
  }
  return true;
}

/*
 * The workplace an indoor jobs-menu pick lands in. DOS stores the OCCUPATION
 * (thunk_FUN_291f_054c -> FUN_281f_0cb8) and derives the building from the
 * chain; the port stores the building slot, so resolve the highest tier of
 * that chain the colony owns (FUN_281f_0ab0 -> FUN_15eb_039e's "count owned
 * along the parent chain" answer). -1 when nothing is owned.
 */
int colonies_job_workplace_building(
  const ColonizeColonyPool* pool, const ColonizeColony* col, int job
) {
  const int row = colonies_job_required_building_row(job);
  if (!pool || !col || row < 0) {
    return -1;
  }
  const int chain = colonies_building_row_chain(row);
  const int* rows = colonies_building_chain_rows(chain);
  int best = -1;
  if (!rows) {
    return colonies_has_building_row(pool, col, (ColonizeBuildingRow)row)
             ? colonies_building_row(pool, (ColonizeBuildingRow)row)
             : -1;
  }
  for (int i = 0; rows[i] >= 0; ++i) {
    if (colonies_has_building_row(pool, col, (ColonizeBuildingRow)rows[i])) {
      const int slot = colonies_building_row(pool, (ColonizeBuildingRow)rows[i]);
      if (slot >= 0) {
        best = slot;
      }
    }
  }
  return best;
}

bool colonies_assign_workplace(
  ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  int building_type
) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !pool) {
    return false;
  }
  if (colonist_index < 0 || colonist_index >= col->colonist_count) {
    return false;
  }
  ColonizeColonist* c = &col->colonists[colonist_index];
  if (!c->active) {
    return false;
  }
  if (building_type < 0 || building_type >= pool->building_type_count) {
    return false;
  }
  if (!col->has_building[building_type]) {
    return false;
  }
  if (!colonies_building_workable(pool, building_type)) {
    return false;
  }
  /*
   * DOS work-assign validator thunk_FUN_1000_9808 (overlays.c:60412-60498),
   * in its own order. What it validates is an @JOB OCCUPATION, not a building:
   * `iStack_10 = FUN_1000_8dfe(colonist); if (iStack_10 == param_2) return 0`
   * lets a no-op reassignment through before any cap runs.
   * bugs.md #580 / #589 / #590.
   */
  const int occupation = colonies_building_occupation(pool, building_type);
  const int cur_occupation =
    (c->building_type >= 0) ? colonies_building_occupation(pool, c->building_type) : -1;
  if (occupation >= 0 && occupation != cur_occupation) {
    if (occupation == COLONIES_JOB_TEACHER) {
      /* overlays.c:60459-60472: faculty cap = the best school OWNED —
       * University 3 (@UNIV3), College 2 (@COLLEGE2), Schoolhouse 1
       * (@SCHOOL1) teachers, counted over the whole colony. */
      const int cap = colonies_school_owned_tier(pool, col);
      if (cap > 0 &&
          colonies_occupation_worker_count(pool, col, COLONIES_JOB_TEACHER, -1) >= cap) {
        return false;
      }
      /* overlays.c:60474-60484: @JOB school level > 3 → @NOTEACHER; level 3
       * without a University (@BUILDING 0xe) → @NEEDUNIVERSITY; level 2
       * without a College (0xd) → @NEEDCOLLEGE. Again the test is what the
       * colony OWNS, never the tier of the clicked school row. */
      if (!colonies_profession_may_teach(c->profession)) {
        return false;
      }
      if (colonies_school_tier_shortfall(c->profession, cap) != 0) {
        return false;
      }
    }
    /*
     * DOS-LITERAL overlays.c:60486-60496 @MORETHANTHREE: DOS tallies every
     * OTHER colonist by occupation and refuses a fourth with
     * `if ((2 < aiStack_42[param_2]) && (9 < param_2)) return 0x16;` — the
     * `9 < param_2` half means @JOB 9 (Rum Distiller) has no cap at all.
     * Verbatim, oddity included.
     */
    if (occupation > 9 &&
        colonies_occupation_worker_count(pool, col, occupation, colonist_index) > 2) {
      return false;
    }
  }
  colonies_clear_colonist_tile(col, colonist_index);
  /* FUN_15eb_1068 (raw 11256-11258): `if (param_2 != current_job)
   * FUN_15eb_0cbc(colonist, 0)` — a real job change zeroes the +0x60
   * education nibble; a no-op reassignment keeps it. */
  if (c->building_type != building_type || c->field_job >= 0) {
    c->turns_in_job = 0;
  }
  c->field_job = -1;
  c->building_type = building_type;
  if (diag_info_enabled()) {
    char who[48];
    colony_log_colonist(col, colonist_index, who, sizeof(who));
    diag_info(
      "COLONY %s: colonist %s -> %s",
      col->name[0] ? col->name : "colony", who, pool->building_types[building_type].name
    );
  }
  return true;
}

bool colonies_toggle_custom_house_cargo(ColonizeColonyPool* pool, int colony_id, int cargo_type) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !pool) {
    return false;
  }
  if (cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT) {
    return false;
  }
  const int ch = colonies_building_row(pool, COLONY_BUILDING_CUSTOM_HOUSE);
  if (ch < 0 || !col->has_building[ch]) {
    return false;
  }
  col->custom_house_bits = (uint16_t)(col->custom_house_bits ^ (1u << cargo_type));
  return true;
}

bool colonies_assign_field(
  ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  int tile_index,
  int field_job
) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !pool) {
    return false;
  }
  if (colonist_index < 0 || colonist_index >= col->colonist_count) {
    return false;
  }
  /* Only the colony's own ring can be seated (DS:0x329[FUN_15eb_0470()],
   * bugs.md #593) — slots beyond it are outside the work radius. */
  if (tile_index < 0 || tile_index >= colonies_work_plot_count(pool, col)) {
    return false;
  }
  if (field_job < 0 || field_job >= COLONIZE_FIELD_JOB_COUNT) {
    return false;
  }
  ColonizeColonist* c = &col->colonists[colonist_index];
  if (!c->active) {
    return false;
  }
  /* Evict prior worker on this tile. */
  const int prev = (int)col->tiles[tile_index];
  if (prev >= 0 && prev < col->colonist_count && prev != colonist_index) {
    col->colonists[prev].field_job = -1;
  }
  colonies_clear_colonist_tile(col, colonist_index);
  col->tiles[tile_index] = (int8_t)colonist_index;
  if (c->building_type >= 0 || c->field_job != field_job) {
    c->turns_in_job = 0; /* FUN_15eb_1068 raw 11256-11258, as assign_workplace */
  }
  c->building_type = -1;
  c->field_job = field_job;
  if (diag_info_enabled()) {
    char who[48];
    colony_log_colonist(col, colonist_index, who, sizeof(who));
    diag_info(
      "COLONY %s: colonist %s -> field tile %d as %s",
      col->name[0] ? col->name : "colony", who, tile_index,
      colony_yield_job_name(field_job)
    );
  }
  return true;
}

bool colonies_clear_field(ColonizeColonyPool* pool, int colony_id, int tile_index) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col) {
    return false;
  }
  if (tile_index < 0 || tile_index >= COLONIZE_COLONY_FIELD_TILES_MAX) {
    return false;
  }
  const int who = (int)col->tiles[tile_index];
  col->tiles[tile_index] = -1;
  if (who >= 0 && who < col->colonist_count) {
    col->colonists[who].field_job = -1;
    diag_info(
      "COLONY %s: colonist #%d off field tile %d",
      col->name[0] ? col->name : "colony", who, tile_index
    );
  }
  return true;
}

/* ===================== Colonist admission, seating & auto-assignment of idle workers (colonies_admit_unit_w .. colonies_auto_assign_idle) ===================== */
int colonies_admit_unit_w(
  const ColonizeWorld* w,
  int colony_id,
  int unit_id
) {
  ColonizeColonyPool* pool = w->colonies;
  ColonizeUnitPool* units = w->units;
  const ColonizeCol1Save* col1 = w->col1;

  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  const ColonizeUnit* unit = units_get_const(units, unit_id);
  if (!col || !units || !unit || !unit->active) {
    return -1;
  }
  if (!units_is_on_map(unit) || unit->x != col->x || unit->y != col->y) {
    return -1;
  }
  if (unit->nation_id != col->nation_id) {
    return -1;
  }
  if (units_is_sea(units, unit_id) || units_is_transport(units, unit_id)) {
    return -1;
  }
  /* DOS-LITERAL FUN_2b5a_0b34 raw 42189: Join gated on DS:0x30e default-profession
   * table (FUN_281f_0b78 >= 0); Treasure/Artillery/Wagon/Regulars/Cavalry have no
   * slot and cannot Join. */
  if (!units_type_has_profession_slot(unit->type_index)) {
    return -1;
  }
  if (col->colonist_count >= COLONIZE_COLONY_POP_MAX) {
    return -1;
  }
  const int profession = unit->profession;
  int work_type = units_kind_type_index(units, UNITS_KIND_COLONIST);
  if (work_type < 0) {
    work_type = unit->type_index;
  }
  int tools = 0;
  int muskets = 0;
  int horses = 0;
  units_founder_loot(units, unit_id, &tools, &muskets, &horses);
  if (!units_despawn(units, unit_id)) {
    return -1;
  }
  if (tools > 0) {
    col->stock[COLONIZE_CARGO_TOOLS] += tools;
  }
  if (muskets > 0) {
    col->stock[COLONIZE_CARGO_MUSKETS] += muskets;
  }
  if (horses > 0) {
    col->stock[COLONIZE_CARGO_HORSES] += horses;
  }
  ColonizeColonist* c = &col->colonists[col->colonist_count];
  memset(c, 0, sizeof(*c));
  c->active = true;
  c->unit_type_index = work_type;
  c->profession = profession;
  c->building_type = -1;
  c->field_job = -1;
  const int idx = col->colonist_count++;
  col->population = col->colonist_count;
  colonies_col1_rebel_divisor_adjust(g_colonies_col1, col->x, col->y, 100);
  /* La Salle: this join may have just crossed pop 3 — grant the free
   * Stockade the same moment, not next turn (see founding_fathers.h). */
  (void)founding_fathers_la_salle_check(pool, col1, col->nation_id);
  /* Col1 +0x8e / +0x1e: LABOR join co-decrements demand counters (~87701). */
  if (col->labor_shortage > 0) {
    col->labor_shortage--;
  }
  if (col->garrison_quota > 0) {
    col->garrison_quota--;
  }
  /*
   * Early Isabella TURN4→5: beachhead soldier join cancels unused Stockade
   * auto-start (hammers still 0) → COL1 bip 0xFF. Cite: test-saves-ai/TURN5.
   */
  if (col->hammers == 0 && col->building_in_production >= 0) {
    const int stockade = colonies_building_row(pool, COLONY_BUILDING_STOCKADE);
    if (stockade >= 0 && col->building_in_production == stockade) {
      col->building_in_production = -1;
    }
  }
  /* bugs.md #256: every admit path (AI joins, capture, save import) puts the
   * newcomer to work immediately — DOS has no idle colonists, and an idle
   * one made the head count disagree with the visible workers. */
  colonies_seat_new_colonist(pool, colony_id, idx);
  return idx;
}

/*
 * DOS `FUN_15eb_1068(slot, 0xd)` — the auto-assign fallback both
 * FUN_15eb_2ea0 (raw 13189-13192) and FUN_15eb_28c8 (raw 13152) use when no
 * work plot scores: @JOB row 13, Carpenter. DOS sets the job unconditionally;
 * the port needs a workplace to put the colonist in, so it seats him in the
 * Carpenter chain when the colony owns one and only then falls back to any
 * other non-school building (bugs.md #6/#256/#408 keep their intent: no idle
 * colonist, and never a silent re-seat at a school).
 */
void colonies_assign_carpenter_fallback(ColonizeColonyPool* pool, int colony_id, int colonist_index) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!pool || !col) {
    return;
  }
  const char* const* chain = colonies_building_chain(COLONIES_CHAIN_CARPENTER);
  for (int i = 0; chain && chain[i]; ++i) {
    const int bi = colonies_find_building(pool, chain[i]);
    if (bi >= 0 && bi < COLONIZE_BUILDING_TYPES_MAX && col->has_building[bi] &&
        colonies_assign_workplace(pool, colony_id, colonist_index, bi)) {
      return;
    }
  }
  for (int bi = 0; bi < pool->building_type_count; ++bi) {
    if (!col->has_building[bi] || colonies_school_building_tier(pool, bi) > 0) {
      continue;
    }
    if (colonies_assign_workplace(pool, colony_id, colonist_index, bi)) {
      return;
    }
  }
}

/*
 * bugs.md #562: DOS's join path is FUN_15eb_3930 -> FUN_15eb_2ea0 ->
 * FUN_15eb_28c8 — the newcomer takes the best-scoring WORK PLOT, and only when
 * nothing scores does he become a Carpenter (`1068(slot, 0xd)`). The port used
 * to seat every joiner in the Town Hall, which has no DOS counterpart at all,
 * so an Expert Farmer joining a colony with a free Plains plot made 0 food.
 */
void colonies_seat_new_colonist(ColonizeColonyPool* pool, int colony_id, int colonist_index) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!pool || !col || colonist_index < 0 || colonist_index >= col->colonist_count) {
    return;
  }
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.colonies = pool;
  ctx.map = g_colonies_occupancy_map;
  ctx.col1 = g_colonies_col1;
  ctx.col1_ok = g_colonies_col1 != NULL;
  /* bVar1 (raw 12952-12958) is `colony+0x1a < 4 && player[nation].control
   * == 0` — a per-colony test, so feeding the scorer this colony's own nation
   * when it is human-controlled answers it exactly (and keeps colony.c free of
   * a save-layer call the slim test targets do not link). */
  ctx.human_nation = -1;
  if (g_colonies_col1 && col->nation_id >= 0 && col->nation_id < 4 &&
      g_colonies_col1->player[col->nation_id].control == 0) {
    ctx.human_nation = col->nation_id;
  }
  if (!ctx.map) {
    /* No map bound (headless import / unit tests): DOS's no-plot outcome. */
    colonies_assign_carpenter_fallback(pool, colony_id, colonist_index);
    return;
  }
  ai_euro_28c8_auto_assign_plots(&ctx, colony_id, colonist_index);
}

/*
 * Stale-save sweep, NOT a DOS routine: a colonist imported with DOS
 * occupation 0x13 (@JOB row 19, plain "Colonist") is genuinely idle in DOS —
 * FUN_15eb_0e18 returns 19, so FUN_15eb_2ea0's `< 9` gate skips him, and the
 * colony_prod01 DOS capture shows he stays unproductive. The port still needs
 * him visible on the settlement grid (bugs.md #6/#256), so he is parked in a
 * building; new joiners go through colonies_seat_new_colonist instead.
 */
void colonies_auto_assign_idle(ColonizeColonyPool* pool, int colony_id) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!pool || !col) {
    return;
  }
  const int town_hall = colonies_building_row(pool, COLONY_BUILDING_TOWN_HALL);
  for (int i = 0; i < col->colonist_count; ++i) {
    ColonizeColonist* c = &col->colonists[i];
    if (!c->active || c->field_job >= 0 || c->building_type >= 0) {
      continue;
    }
    if (town_hall >= 0 && colonies_assign_workplace(pool, colony_id, i, town_hall)) {
      continue;
    }
    /* bugs.md #408: never quietly seat a specialist back at a school. */
    colonies_assign_carpenter_fallback(pool, colony_id, i);
  }
}

/* ===================== Colonist eject/dismiss flow & gear seizure on release (colonies_eject_role_name .. colonies_eject_colonist) ===================== */
const char* colonies_eject_role_name(int role) {
  /* NAMES.TXT @JOB column 0: rows 20..24 are the equipped roles, 19 Colonist. */
  switch (role) {
  case COLONIZE_EJECT_PIONEER:
    return reports_job_short_name(UNITS_JOB_PIONEER);
  case COLONIZE_EJECT_SOLDIER:
    return reports_job_short_name(UNITS_JOB_SOLDIER);
  case COLONIZE_EJECT_SCOUT:
    return reports_job_short_name(UNITS_JOB_SCOUT);
  case COLONIZE_EJECT_DRAGOON:
    return reports_job_short_name(UNITS_JOB_DRAGOON);
  case COLONIZE_EJECT_MISSIONARY:
    return reports_job_short_name(UNITS_JOB_MISSIONARY);
  case COLONIZE_EJECT_COLONIST:
  default:
    return reports_job_short_name(19);
  }
}

int colonies_has_church_or_cathedral(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col
) {
  if (!pool || !col) {
    return 0;
  }
  const int church = colonies_building_row(pool, COLONY_BUILDING_CHURCH);
  const int cath = colonies_building_row(pool, COLONY_BUILDING_CATHEDRAL);
  if (church >= 0 && church < COLONIZE_BUILDING_TYPES_MAX && col->has_building[church]) {
    return 1;
  }
  if (cath >= 0 && cath < COLONIZE_BUILDING_TYPES_MAX && col->has_building[cath]) {
    return 1;
  }
  return 0;
}

/*
 * Tools handed to a body being equipped as a Pioneer: whole 20-tool steps,
 * capped at 100. DOS-LITERAL, not a port convenience — verified 2026-09-10
 * (seventh wave, second-wave lead 7, which suspected a deviation):
 *   FUN_15eb_1068, the equip/leave-as applier (viceroy_unpacked.c 11250-11253):
 *     local_8 = colony stock[TOOLS] (+0xb6) / 0x14;
 *     iVar6   = local_8 * 0x14;  if (100 < iVar6) iVar6 = 100;
 *   then `param_2 == 0x14` (profession Pioneer) writes that byte to the unit's
 *   tools field +0x3159 (raw 11274 and 11288, the re-type and the new-unit
 *   arms), and the tail at raw 11322-11331 charges the colony the same amount.
 *   The four dialog builders recompute it the same way for their row text
 *   (raw 50570-50571, 53455, 62541, 67774), clamped to [0x14, 100].
 * FUN_15eb_35d0's `min(stock, 100, req)` is a different path — the cargo-hold
 * loader, whose `req` is FUN_15eb_3208's free-hold count * 100 (raw 13375) —
 * i.e. loading 100-lots into a ship/wagon, never equipping a colonist.
 * bugs.md #356 (a Pioneer legitimately walks with 20/40/60/80/100) is the
 * behaviour this reproduces.
 */
int colonies_equip_tools_take(int available) {
  if (available < UNITS_EQUIP_TOOLS_STEP) {
    return 0;
  }
  int take = (available / UNITS_EQUIP_TOOLS_STEP) * UNITS_EQUIP_TOOLS_STEP;
  if (take > UNITS_EQUIP_TOOLS_MAX) {
    take = UNITS_EQUIP_TOOLS_MAX;
  }
  return take;
}

/*
 * The "Leave as" row list, DOS's three states.
 *
 * This list is FUN_2f2b_348c's leave-as mode, rows = professions 0x13..0x18
 * (`local_fa = 0x13, local_146 = 6`), each asked through FUN_281f_0bb4 →
 * FUN_15eb_3454 (viceroy_unpacked.c 13518-13590), whose answer has THREE
 * values, not two:
 *   0       — row not offered at all; the dialog loop skips it outright
 *             (raw 50753 `if (local_e != 0)`, and the row counter at 50575).
 *   0xffff  — row offered but DISABLED: raw 50805 `if (local_e == -1)` calls
 *             the greyed-row draw FUN_291f_01b6. This is the short-stock
 *             answer: for each cargo in the row's FUN_15eb_0d8e list, colony
 *             stock < required (tools 0x14 = 20, muskets/horses 0x32 = 50)
 *             sets local_4 = 0xffff (raw 13580-13585).
 *   0xfffe  — ordinary enabled row.
 * Return 0 comes from exactly three places for these rows: an Indian Convert
 * (@JOB 0x1b) gets nothing but Colonist — raw 13557-13560,
 * `if (0x13 < param_1 && cur_prof == 0x1b) return 0` — the Missionary row
 * 0x18 needs the Church bit UNLESS the body is already a Jesuit
 * (FUN_15eb_038e(0x25) && cur_prof != 0x18, raw 13567-13569), and the
 * Colonist row 0x13 disappears for a Jesuit body under the DS:0x8dc6 test
 * (raw 13561-13565, see colonies_eject_row_offered below).
 *
 * The port used to OMIT short-stock gear rows instead of greying them, and
 * offered all six to a Convert. Both fixed 2026-09-10 (seventh wave,
 * second-wave lead 4). out_enabled (optional) carries the 0xfffe/0xffff
 * distinction; callers that pass NULL get the row list only.
 *
 * The same DOS function serves a unit standing on the fence (a band index at
 * or past the colonist count forces leave-as mode), so game_loop.c's
 * game_colony_list_outside_roles is a twin of this list and must stay
 * row-for-row identical, greying included.
 *
 * Earlier cites for the bless row: Colonization.pdf Establishing a Mission /
 * Church; building_production Missionary.
 */
/* 0-based pool slot of a colony record, DOS's colony index (DS:0x8dc6 is set
 * from the same kind of index by FUN_15eb_002c raw 9318). -1 if unknown. */
static int colonies_pool_slot_index(const ColonizeColonyPool* pool, const ColonizeColony* col) {
  if (!pool || !col) {
    return -1;
  }
  const ptrdiff_t slot = col - &pool->colonies[0];
  if (slot < 0 || slot >= (ptrdiff_t)COLONIZE_COLONIES_MAX) {
    return -1;
  }
  return (int)slot;
}

/*
 * DOS-LITERAL FUN_15eb_3454 raw 13556-13570 — the "is this leave-as row
 * offered at all" test (return 0 vs 0xfffe/0xffff), shared by the two row
 * builders (here and game_loop's outside twin) and by both appliers, so a
 * click cannot take a row the list never drew.
 *
 *   raw 13557-13560: rows above 0x13 return 0 for an Indian Convert (0x1b).
 *   raw 13561-13565: row 0x13 (Colonist) returns 0 when the body is already a
 *     Jesuit Missionary (0x18) AND `*(int*)0x8dc6 < 4` AND
 *     `*(char*)(*(int*)0x8dc6 * 0x34 + 0x543f) == 0`. DS:0x8dc6 is the ACTIVE
 *     COLONY INDEX (FUN_15eb_002c raw 9318 writes it from the colony record
 *     index), while 0x543f + n*0x34 is the NATION table whose byte 0 is the
 *     control flag (0 = human). DOS indexes the nation table with a colony
 *     index: a genuine DOS bug, ported literally per docs/project_goals.md —
 *     whether a Jesuit may be un-blessed back to a plain Colonist depends on
 *     which pool slot his colony happens to occupy.
 *   raw 13567-13569: row 0x18 (Missionary) returns 0 only when the colony has
 *     no Church AND the body is not already 0x18 — a Jesuit is always offered
 *     the Missionary row, churchless colony or not.
 */
bool colonies_eject_row_offered(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  int profession,
  int role
) {
  if (!col) {
    return false;
  }
  if (role != COLONIZE_EJECT_COLONIST && profession == COLONIZE_PROF_CONVERT) {
    return false;
  }
  if (role == COLONIZE_EJECT_COLONIST) {
    if (profession == UNITS_JOB_MISSIONARY) {
      const int slot = colonies_pool_slot_index(pool, col);
      if (slot >= 0 && slot < 4 && g_colonies_col1 &&
          g_colonies_col1->player[slot].control == 0) {
        return false;
      }
    }
    return true;
  }
  if (role == COLONIZE_EJECT_MISSIONARY) {
    return colonies_has_church_or_cathedral(pool, col) || profession == UNITS_JOB_MISSIONARY;
  }
  return true;
}

int colonies_list_eject_roles_gear(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  int add_tools,
  int add_muskets,
  int add_horses,
  int profession,
  int* out_roles,
  bool* out_enabled,
  int out_max
) {
  if (!col || !out_roles || out_max <= 0) {
    return 0;
  }
  const bool convert = (profession == COLONIZE_PROF_CONVERT);
  const int tools = col->stock[COLONIZE_CARGO_TOOLS] + add_tools;
  const int muskets = col->stock[COLONIZE_CARGO_MUSKETS] + add_muskets;
  const int horses = col->stock[COLONIZE_CARGO_HORSES] + add_horses;

  int n = 0;
  if (colonies_eject_row_offered(pool, col, profession, COLONIZE_EJECT_COLONIST)) {
    out_roles[n] = COLONIZE_EJECT_COLONIST;
    if (out_enabled) {
      out_enabled[n] = true;
    }
    ++n;
  }
  if (!convert) {
    const struct {
      int role;
      bool enabled;
    } k_gear[] = {
      {COLONIZE_EJECT_PIONEER, tools >= UNITS_EQUIP_TOOLS_STEP},
      {COLONIZE_EJECT_SOLDIER, muskets >= UNITS_EQUIP_MUSKETS},
      {COLONIZE_EJECT_SCOUT, horses >= UNITS_EQUIP_HORSES},
      {COLONIZE_EJECT_DRAGOON, muskets >= UNITS_EQUIP_MUSKETS && horses >= UNITS_EQUIP_HORSES}
    };
    for (size_t i = 0; i < sizeof(k_gear) / sizeof(k_gear[0]) && n < out_max; ++i) {
      out_roles[n] = k_gear[i].role;
      if (out_enabled) {
        out_enabled[n] = k_gear[i].enabled;
      }
      ++n;
    }
    /* Bless costs no cargo, so the row is never the greyed kind
     * (FUN_15eb_3454 row 0x18). */
    if (n < out_max &&
        colonies_eject_row_offered(pool, col, profession, COLONIZE_EJECT_MISSIONARY)) {
      out_roles[n] = COLONIZE_EJECT_MISSIONARY;
      if (out_enabled) {
        out_enabled[n] = true;
      }
      ++n;
    }
  }
  return n;
}

int colonies_list_eject_roles_ex(
  const ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  int* out_roles,
  bool* out_enabled,
  int out_max
) {
  const ColonizeColony* col = colonies_get(pool, colony_id);
  if (!col || !out_roles || out_max <= 0) {
    return 0;
  }
  if (colonist_index < 0 || colonist_index >= col->colonist_count ||
      !col->colonists[colonist_index].active) {
    return 0;
  }
  /* raw 13556: every >= 0x13 row gate reads the body's own @JOB. */
  const int profession = col->colonists[colonist_index].profession;
  return colonies_list_eject_roles_gear(
    pool, col, 0, 0, 0, profession, out_roles, out_enabled, out_max
  );
}

bool colonies_eject_role_gear(
  int role,
  int stock_tools,
  int* out_tools,
  int* out_muskets,
  int* out_horses
) {
  int tools_take = 0;
  int muskets_take = 0;
  int horses_take = 0;
  switch (role) {
  case COLONIZE_EJECT_PIONEER:
    tools_take = colonies_equip_tools_take(stock_tools);
    break;
  case COLONIZE_EJECT_SOLDIER:
    muskets_take = UNITS_EQUIP_MUSKETS;
    break;
  case COLONIZE_EJECT_SCOUT:
    horses_take = UNITS_EQUIP_HORSES;
    break;
  case COLONIZE_EJECT_DRAGOON:
    muskets_take = UNITS_EQUIP_MUSKETS;
    horses_take = UNITS_EQUIP_HORSES;
    break;
  case COLONIZE_EJECT_COLONIST:
  case COLONIZE_EJECT_MISSIONARY:
    break;
  default:
    return false;
  }
  if (out_tools) {
    *out_tools = tools_take;
  }
  if (out_muskets) {
    *out_muskets = muskets_take;
  }
  if (out_horses) {
    *out_horses = horses_take;
  }
  return true;
}

int colonies_list_eject_roles(
  const ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  int* out_roles,
  int out_max
) {
  return colonies_list_eject_roles_ex(pool, colony_id, colonist_index, out_roles, NULL, out_max);
}

int colonies_eject_colonist(
  ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  ColonizeUnitPool* units,
  int role
) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !units) {
    return -1;
  }
  if (colonist_index < 0 || colonist_index >= col->colonist_count) {
    return -1;
  }
  ColonizeColonist* c = &col->colonists[colonist_index];
  if (!c->active) {
    return -1;
  }

  int tools_take = 0;
  int muskets_take = 0;
  int horses_take = 0;
  ColonizeUnitKind type_kind = UNITS_KIND_COLONIST;
  /* Unknown rows fall through as a plain Colonist (the old `default:` arm). */
  (void)colonies_eject_role_gear(
    role, col->stock[COLONIZE_CARGO_TOOLS], &tools_take, &muskets_take, &horses_take
  );
  switch (role) {
  case COLONIZE_EJECT_PIONEER:
    if (tools_take <= 0) {
      return -1;
    }
    type_kind = UNITS_KIND_PIONEER;
    break;
  case COLONIZE_EJECT_SOLDIER:
    type_kind = UNITS_KIND_SOLDIER;
    break;
  case COLONIZE_EJECT_SCOUT:
    type_kind = UNITS_KIND_SCOUT;
    break;
  case COLONIZE_EJECT_DRAGOON:
    type_kind = UNITS_KIND_DRAGOON;
    break;
  case COLONIZE_EJECT_MISSIONARY:
    /* Row gate re-tested (Church bit, or a body that is already a Jesuit —
     * FUN_15eb_3454 raw 13567-13569). */
    if (!colonies_eject_row_offered(pool, col, c->profession, role)) {
      return -1;
    }
    type_kind = UNITS_KIND_MISSIONARY;
    break;
  case COLONIZE_EJECT_COLONIST:
    /* raw 13561-13565: the Colonist row can be missing for a Jesuit. */
    if (!colonies_eject_row_offered(pool, col, c->profession, role)) {
      return -1;
    }
    break;
  default:
    break;
  }
  if (col->stock[COLONIZE_CARGO_MUSKETS] < muskets_take ||
      col->stock[COLONIZE_CARGO_HORSES] < horses_take) {
    return -1;
  }

  int type_index = units_kind_type_index(units, type_kind);
  if (type_index < 0) {
    type_index = c->unit_type_index;
  }
  /* bugs.md #556: DOS's colony eject applier (overlays.c:10225-10245) writes
   * only the TYPE byte from table 0x2f5 and copies the profession byte
   * verbatim — `*(0x315b) = FUN_0000_6d02(param_1)` — for every row, bless
   * included. A blessed Expert Farmer stays an Expert Farmer under type 3.
   * The port used to wipe it to NONE here; the outside twin
   * (game_loop game_colony_apply_outside_role) never did. */
  const int profession = c->profession;

  colonies_clear_colonist_tile(col, colonist_index);
  for (int i = colonist_index; i < col->colonist_count - 1; ++i) {
    col->colonists[i] = col->colonists[i + 1];
  }
  col->colonist_count--;
  col->population = col->colonist_count;
  colonies_col1_rebel_divisor_adjust(g_colonies_col1, col->x, col->y, -100);
  if (col->colonist_count >= 0 && col->colonist_count < COLONIZE_COLONY_POP_MAX) {
    memset(&col->colonists[col->colonist_count], 0, sizeof(col->colonists[0]));
  }
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES_MAX; ++t) {
    const int who = (int)col->tiles[t];
    if (who == colonist_index) {
      col->tiles[t] = -1;
    } else if (who > colonist_index) {
      col->tiles[t] = (int8_t)(who - 1);
    }
  }

  col->stock[COLONIZE_CARGO_TOOLS] -= tools_take;
  col->stock[COLONIZE_CARGO_MUSKETS] -= muskets_take;
  col->stock[COLONIZE_CARGO_HORSES] -= horses_take;

  const int uid = units_spawn_allow_stack(units, type_index, col->x, col->y);
  if (uid < 0) {
    /* Refund gear if spawn fails (colonist already removed — best-effort). */
    col->stock[COLONIZE_CARGO_TOOLS] += tools_take;
    col->stock[COLONIZE_CARGO_MUSKETS] += muskets_take;
    col->stock[COLONIZE_CARGO_HORSES] += horses_take;
    return -1;
  }
  ColonizeUnit* u = units_get(units, uid);
  if (u) {
    units_set_nation(u, col->nation_id);
    u->profession = profession;
    u->tools = tools_take;
    u->muskets = muskets_take;
    u->horses = horses_take;
    /* bugs.md: a freshly ejected/armed/horsed unit starts with no moves —
     * it acts from next turn's refresh. */
    u->moves = 0;
  }
  return uid;
}

