/*
 * Quiet NEW WORLD Brave move scoring — ASM LAB_521d_4ea9 / flags 0x10|0x20.
 *
 * Recovered from original_sources_decompiled/viceroy_unpacked.c FUN_521d_20e6
 * and viceroy_unpacked.asm around CODE_125:521d:4ea9.
 *
 * Applies only when unit type table flags at DS:0x523d include 0x10 or 0x20
 * (Brave type 19 → flags 0x38). Other unit kinds use different bases in the
 * same scorer and are out of scope here.
 *
 * Reference only — not compiled into the Linux binary.
 *
 * ============================================================================
 * MAJOR FINDING (2026-08-13) — a whole branch of this formula is missing.
 * See docs/seed100_brave.md "Root cause candidate" section for the full
 * writeup; short version here.
 *
 * FUN_521d_20e6 failed to decompile entirely in the canonical export
 * ("Unable to decompile ... process: timeout") — this file was necessarily
 * written from the raw .asm only. Re-disassembled clean via the
 * overlay-addressing project (tools/address_mapping.csv -> OVL14_L0000:20e6,
 * see docs/rtlink_decode_v2_gap.md): decompiled successfully in 27s (vs.
 * never finishing before), 2219 lines, zero warnings.
 *
 * The clean decompile confirms base/terrain/facing below ARE correct for the
 * gate this file already scopes to ("type flags have 0x10 or 0x20 set") —
 * but that gate is only the FIRST of TWO outer branches in the real DOS
 * code, and this file (and quiet_score_base/quiet_score_terrain below) only
 * ever implements the first one:
 *
 *   if ( (unit+0x3147 high nibble == 0) && (dest tile is not ocean/HS) ) {
 *     // "unit not yet SEEN by any Euro nation" — this file's existing formula.
 *     ... quiet_score_base() / quiet_score_terrain() as below ...
 *   } else {
 *     // "unit HAS been seen by at least one Euro nation" (or dest is
 *     // ocean/HS) — a COMPLETELY DIFFERENT, UNPORTED formula:
 *     base = RNG(1,5);                                    // not RNG(1,3)!
 *     if (<some contact/diplomacy condition, not fully traced>) {
 *       score = base + terr_cost_table2[terr] * 4;         // ADD, table+1, x4
 *     }
 *     // else: score stays just the RNG(1,5) roll, no terrain term at all.
 *   }
 *
 * unit+0x3147's high nibble is a per-Euro-nation "has nation N seen this
 * unit" bitmask (bit = 0x10 << nation_id) — same convention already
 * documented for tile/colony visibility in docs/save_format_map.md ("0x10
 * <<euro" fog-of-war bit), confirmed by cross-reference at
 * viceroy_unpacked_2.c:53706 (`0x10 << (human_nation & 0x1f) & unit+0x3147`).
 * quiet_lab_54f5_gate below does NOT implement or even reference this check.
 *
 * All Braves are genuinely unseen at spawn, so this doesn't obviously
 * explain the very-first-move (turn 0/1, docs/seed100_brave.md "13 init
 * peels") mismatch by itself — but it's the natural explanation for why
 * MID-turn peels (113, ~9x more) so vastly outnumber init peels: every turn
 * a Brave becomes visible to some Euro nation, its scoring should switch to
 * the RNG(1,5) formula and currently never does. Not yet implemented here —
 * this is the concrete next step for whoever picks up the peel-elimination
 * work, not a full fix. The exact "some contact/diplomacy condition" gate
 * for the ADD-vs-bare-roll split inside the RNG(1,5) branch also needs
 * tracing before this can be ported (not done this pass).
 * ============================================================================
 */

#include <stdint.h>

#include "../include/viceroy_types.h"
#include "../include/viceroy_globals.h"

extern int rng_range(int lo, int hi_inclusive);
extern int tile_has_minor_river(int x, int y);
extern int tile_fa_flags(int x, int y);
extern int ocean_or_high_seas(int x, int y);
extern int owner_nibble(int x, int y);
extern uint8_t terrain_byte(int x, int y);
extern int decode_terrain_class(uint8_t terrain);
extern int map_tile_in_bounds(int x, int y);
extern int coarse_fog_unseen(int x, int y);
extern int tile_explore_mask(int x, int y);
extern int tile_owner_or_presence(int x, int y);
extern int tile_tribe_or_presence(int x, int y);
extern int unit_index_on_tile(int x, int y);
extern int diplomacy_flags(int self_nation, int other_nation);
extern int unit_type_combat_byte(int unit_type);

/* Direction deltas at DS:0xbe / 0xb4 — same order as Linux k_ai_dir8_*. */
static const int k_dir8_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int k_dir8_dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

/* ====================================================================== */
/* Score terms                                                            */
/* ====================================================================== */

/* Ghidra: LAB_521d_4fb4 | quiet_score_base — FUN_281f_04d4(1,3). */
int quiet_score_base(void) {
  return rng_range(1, 3); /* burns LCG */
}

/*
 * Ghidra: LAB_521d_4fc8..500a | quiet_score_terrain
 *
 * If (unit has minor-river and dest has river and dir is cardinal)
 *    OR (unit has fa-mask and dest has fa-mask):
 *      score += 1
 * Else:
 *      score -= terr_cost_table[terr_class]   // NOT ×3
 */
int quiet_score_terrain(int score, int unit_x, int unit_y, int nx, int ny, int dir) {
  int unit_river = tile_has_minor_river(unit_x, unit_y);
  int unit_fa = tile_fa_flags(unit_x, unit_y) != 0;
  int dest_river = tile_has_minor_river(nx, ny);
  int dest_fa = tile_fa_flags(nx, ny) != 0;
  int cardinal = (dir & 1) == 0;

  if ((unit_river && dest_river && cardinal) || (unit_fa && dest_fa)) {
    return score + 1; /* LAB_521d_4ffa */
  }
  int terr = decode_terrain_class(terrain_byte(nx, ny)) & 31;
  static const uint8_t k_terr_cost[32] = {
      1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 2, 2, 2, 2, 3, 3,
      2, 1, 2, 2, 2, 2, 3, 3, 2, 1, 1, 3, 2, 13, 255, 255};
  return score - (int)k_terr_cost[terr];
}

/*
 * Ghidra: LAB_521d_54f5 entry | quiet_lab_54f5_gate
 *
 * Enter facing / military −10 / bVar20 fog only when:
 *   (unit_index_on_tile(dest) < 0 && tile_tribe_or_presence(dest) < 0)
 *   || owner_nibble(dest) == self_nation
 *
 * Callees: FUN_281f_07e0 (empty), FUN_281f_06d2 (tribe/presence),
 * FUN_281f_06dc (owner).
 */
int quiet_lab_54f5_gate(int dest_x, int dest_y, int nation_id) {
  int own = owner_nibble(dest_x, dest_y);
  if (own == nation_id) {
    return 1;
  }
  if (unit_index_on_tile(dest_x, dest_y) < 0 &&
      tile_tribe_or_presence(dest_x, dest_y) < 0) {
    return 1;
  }
  return 0;
}

/*
 * Ghidra: LAB_521d_54f5 facing | quiet_score_facing
 *
 * diff = circular |last_dir - dir| clamped to 0..4
 * score += -diff * diff * 2
 */
int quiet_score_facing(int score, int dir, int last_dir) {
  if (last_dir < 0 || last_dir > 7) {
    return score;
  }
  int diff = last_dir - dir;
  if (diff < 1) {
    diff = ~diff + 1; /* decomp two's-complement abs when diff < 1 */
  }
  if (diff > 4) {
    diff = -(diff - 8); /* decomp: -(local_6e + -8) */
  }
  return score + diff * diff * -2;
}

/*
 * Ghidra: LAB_521d_54f5 military neighbor loop | quiet_score_military_minus10
 *
 * For each of 8 neighbors of dest:
 *   presence = 0682; if foreign && (0a38 & 0x60)==0x20 && mover combat==0:
 *     for each unit on nbr with type-table combat != 0: score -= 10
 * Brave type 19 has combat byte 0 → enters the path; early NEW WORLD usually
 * finds no war-flagged foreign combat units → often a no-op.
 * Annotated: diplomacy_flags stub returns 0 → −10 never fires here.
 * Linux cutover walks the live unit pool when diplomacy is wired.
 */
int quiet_score_military_minus10(
  int score,
  int dest_x,
  int dest_y,
  int nation_id,
  int mover_type
) {
  if (unit_type_combat_byte(mover_type) != 0) {
    return score;
  }
  for (int n = 0; n < 8; ++n) {
    int nx = dest_x + k_dir8_dx[n];
    int ny = dest_y + k_dir8_dy[n];
    int presence = tile_owner_or_presence(nx, ny);
    if (presence < 0 || presence == nation_id) {
      continue;
    }
    if ((diplomacy_flags(nation_id, presence) & 0x60) != 0x20) {
      continue;
    }
    /* DOS: FUN_281f_07e0 / 02e4 unit walk; subtract 10 per combat≠0 unit. */
    (void)nx;
    (void)ny;
  }
  return score;
}

/*
 * Ghidra: bVar20 block after facing/−10 | quiet_score_fog_explore
 *
 * bVar20 init (Brave type 19): starts true (type != wagon 0x12); may clear
 * later for some Euro paths. NEW WORLD Indian quiet: treat as enabled.
 *
 * Far probe = unit + 4×dir.
 *   +8 if explore-index byte==0 (y>>2)+(x>>2)*18 && !ocean(far) && inset(far)
 *       — not the tribe /5 index; see accessors.c coarse_fog_* 
 *   +4 ship west-bias — skipped (local_34 ship flag; Braves N/A)
 * Neighbor loop around far (8 dirs):
 *   +2 explore-mask clear — ONLY if nation_id < 4 (Europeans); Indians skip
 *   −2 if tile_owner_or_presence(nbr) >= 0
 *   +2f79[terr] if local_6a — NEW WORLD forces local_6a=0; skip
 */
int quiet_score_fog_explore(
  int score,
  int unit_x,
  int unit_y,
  int dir,
  int nation_id,
  int enable_fog /* bVar20 */
) {
  if (!enable_fog || dir < 0 || dir > 7) {
    return score;
  }

  int far_x = unit_x + k_dir8_dx[dir] * 4;
  int far_y = unit_y + k_dir8_dy[dir] * 4;

  if (coarse_fog_unseen(far_x, far_y) && !ocean_or_high_seas(far_x, far_y) &&
      map_tile_in_bounds(far_x, far_y)) {
    score += 8;
  }

  for (int n = 0; n < 8; ++n) {
    int nx = far_x + k_dir8_dx[n];
    int ny = far_y + k_dir8_dy[n];
    if (!map_tile_in_bounds(nx, ny)) {
      continue;
    }
    if (nation_id >= 0 && nation_id < 4) {
      int mask = tile_explore_mask(nx, ny);
      int euro_bit = 0x10 << (nation_id & 3);
      if ((mask & euro_bit) == 0 && !ocean_or_high_seas(nx, ny)) {
        score += 2;
      }
    }
    if (tile_owner_or_presence(nx, ny) >= 0) {
      score -= 2;
    }
  }
  return score;
}

/*
 * Ghidra: LAB_521d_52aa colony / capital pull | quiet_score_colony_pull
 *
 * Combat-capable units near foreign colonies. Brave type 19 has combat
 * strength byte DS:0x5236 == 0 → path rejects early (JMP 5183).
 * When colony_count==0 (NEW WORLD early), this is a documented no-op.
 */
int quiet_score_colony_pull(int score, int colony_count) {
  if (colony_count == 0) {
    return score; /* no-op — DOS condition preserved */
  }
  /*
   * Checked 2026-08-14 whether this is a quick win like 417e/1816 turned
   * out to be — confirmed genuinely blocked then. **2026-08-20 re-check
   * (T1.7): the "wrong function boundary, needs 2244-style overlay
   * recovery" diagnosis was wrong — retracted, not just refined.**
   *
   * The 6-arg call site cited above (`viceroy_unpacked.c:57109` at the
   * time) is a stale line reference from an earlier decompile pass —
   * that line now belongs to the unrelated, already-ported
   * `FUN_364b_03f6` (coastal fort fire, `turn_run_coastal_fort_fire`).
   * The REAL call site for *this* quiet-scoring block is the zero-arg
   * one, `iVar20 = FUN_291f_0a14();` at `viceroy_unpacked.c:85601`,
   * directly inside `LAB_521d_52aa` (confirmed via the raw body — this
   * is the actual colony/capital-pull section). Its canonical decompile
   * (`FUN_210d_0dab(0x291f); FUN_5fef_1b0e(); return;`) is a completely
   * normal RTLink call-thunk, rendered `(void)`-signature with no-arg
   * inner calls — verified against a known-good sibling thunk
   * (`FUN_291f_0996` → `FUN_364b_1b76`, same two-line shape, same
   * `(void)` rendering) that nobody doubts is correct. Ghidra renders
   * *every* thunk in this whole `291f_XXXX` family this way regardless
   * of the real target's signature — the arg-count "mismatch" was never
   * a corruption signal, just this rendering style plus a stale line
   * cite pointing at an unrelated function's real (non-thunk) call.
   *
   * **Real target, already known**: `FUN_5fef_1b0e` — Linux's own
   * `combat_apply_1b0e_peels` (`combat.md`), the open-field combat-
   * strength formula `atk = ((terrain_stash+4)*atk>>2)*3>>1`. Its
   * return here (`iVar20`) feeds directly into this block's own score
   * arithmetic (`iStack_e8 = ((iVar18+1)/iVar14)*iVar20 / ...`) — reads
   * as reusing the same combat-strength estimate as a scoring *input*
   * (not resolving a real fight), consistent with `1b0e` computing a
   * pure strength number rather than only being invoked for actual
   * combat.
   *
   * **Not fully unblocked — real remaining work, just correctly scoped
   * now**: the surrounding formula still touches several genuinely
   * unnamed pieces (`DS:0x5239` stride-0xe table, `iVar14`/`iVar18`'s own
   * source calls `FUN_281f_08bc` — the same widely-reused generic field
   * accessor as before, `DS:0x523d` bitmask reuse, `DS:0x53d2`
   * comparison) that would need their own semantic pass before a
   * faithful port — a real formula-mapping task, comparable in size to
   * `euro_g_table_0a60.md`'s own G-table dig, not a quick finish. Still
   * no golden exercises `colony_count>0` for a Brave, so still nothing
   * to verify a port against even once mapped. **Downgraded from
   * "corruption-blocked" to "known-clean, needs a dedicated formula-
   * mapping pass"** — a real, if partial, unblock. See `ai_port_plan.md`
   * T1.7 and `ai_transcription.md`'s R2 section for the up-to-date note.
   *
   * **2026-08-20 (T1.9 pass) — two more pieces resolved, one genuine new
   * wall found.**
   * - `func_0x00019c04(0x181f,param_2,uStack_18,uStack_1c,0,0)` (the
   *   6-arg call right before `iVar15=FUN_1000_8aac(0x191f)`, result
   *   discarded): force-disassembled its raw target (`ram:0x19c04`,
   *   `tools/GhidraDisasmExact.java`) — a clean 2-instruction stub,
   *   `CALLF FUN_1000_1e7b` (the usual loader guard) `; JMPF 0x0000:1b0e`.
   *   `0x1b0e` is the *same* `FUN_5fef_1b0e`/`combat_apply_1b0e_peels`
   *   this block's zero-arg `FUN_291f_0a14()` call already resolves to
   *   (see above) — this call applies/primes that same combat-strength
   *   computation for `(param_2, dest x/y)` before the cheap zero-arg
   *   call reads a cached result from it; not a separate mystery
   *   function, the same one invoked twice with different argument
   *   shapes.
   * - `DS:0x53d2` (the `*(int*)0x53d2==2` gate before the `>>1` halving):
   *   already named elsewhere in this project — `king_ref.md`'s
   *   `crown_nation_id` (the non-human Euro nation slot). Reads as "halve
   *   the pull score if the crown-controlled nation is a specific slot
   *   (2) and two other flags are clear" — mechanically clear now, still
   *   not independently confirmed *why* slot 2 specifically matters.
   * - `DS:0x5239` (and the whole `0x5235..0x523d` per-unit-type family
   *   this formula reads): **attempted a raw-byte read via the resident
   *   project (same method that resolved the T1.1/`4fa8` family) and hit
   *   a real methodological wall, not a quick win.** Reading flat
   *   `0000:5235` onward returns unambiguous *code* bytes (a real,
   *   structured function body — `CALL`/`PUSH`/`MOV` patterns, not
   *   table-shaped data), not the expected per-unit-type stat table.
   *   Every other DS-relative address this project has successfully read
   *   this way (`0x8542`, `0x92c0`, `0x9e12`, etc.) sits well above
   *   `0x8000`; this table's addresses (`0x5235`-`0x523d`) sit well
   *   below it, in what turns out to be the code region. **This means
   *   the "DS-relative address == flat resident offset" identity this
   *   session has otherwise relied on successfully does NOT hold
   *   uniformly** — either DS's real runtime base differs from CS's for
   *   addresses in this range, or this specific table lives somewhere
   *   this flat single-blob resident extraction doesn't reach. Not
   *   pursued further this pass — flagging as a real, new, precisely-
   *   stated blocker rather than either trusting a wrong byte read or
   *   guessing a value. Needs either resolving the actual DS segment
   *   base for this address range (a static-tooling question, possibly
   *   answerable without a live session) or a live DOSBox-X read.
   *
   * **2026-08-21 — resolved from existing `dosbox-x-dumps/*` saves, no
   * live session needed.** The "flat resident offset == DS-relative
   * address" identity really doesn't hold for this range (confirmed),
   * but the fix is the same one `terrain_yields.md`'s `DS:0x2f76` dig
   * used: locate the table by **byte-pattern content search** across a
   * captured `Memory` blob instead of computing a flat address, then
   * (separately) calibrate `HDR + DS_segment*16 + ds_relative_offset`
   * against a byte offset already found this way, using the real `DS`
   * register value read straight from the save's own `CPU` record (same
   * technique `tools/brave_dump/parse_0e52_dump.py` already uses to
   * print `DS=`). Search target: the unit-type table's own flags byte
   * (offset `+7` from `0x5236`, i.e. `DS:0x523d`) for Braves/Armed
   * Braves/Mtd. Braves/Mtd. Warriors (indices 19-22) — `0x38` four times
   * at stride `0xe`, an unambiguous signature. Found at the same file
   * offset in every save checked (`dump_1816`, `vr4528`, `vr_2a02` —
   * despite `vr_2a02` capturing a *different* `DS` register value,
   * `0x2042` vs. the other saves' `0x237d`, confirming the table's file
   * position tracks physical/linear memory, not the momentary `DS` value
   * at capture time — the same `DS≠A000` caveat `parse_0e52_dump.py`
   * already flags for IRQ-context captures applies here too, so byte-
   * pattern search is the robust method, segment arithmetic is a
   * cross-check only). Decoded the whole stride-`0xe` table (23 unit
   * types, indices `0..22`) at that offset and diffed **three separate
   * byte columns against `NAMES.TXT` `@UNIT` and `ColonizeUnitType`,
   * 23-for-23 exact matches each, replicated in a second independent
   * save**:
   * - `DS:0x5236` (the `LAB_521d_52aa` entry gate, `!= 0`) = **`attack`**
   *   (the same field already loaded into `ColonizeUnitType.attack`,
   *   `units.c:129`). Braves' attack is `1` (nonzero) — gate open,
   *   consistent with this formula being scoped to Braves.
   * - `DS:0x5239` (`cVar9`, the clamped divisor) = **`cost`**
   *   (`ColonizeUnitType.cost`, `units.c:132` — the Europe-purchase-price
   *   `@UNIT` column, not a coordinate or distance value). Braves' cost
   *   is `1`, so the clamp-to-≥1 divisor is trivially `1` for the
   *   in-scope case — the `(cVar9-1U & ~-(cVar9==0))+1` clamp exists for
   *   *other* unit types this same table/formula also runs for (out of
   *   this file's stated scope), not for Braves specifically.
   * - `DS:0x523d & 0x10`/`0x20` — already known (this file's own header,
   *   "Brave type 19 → flags `0x38`" = `0b00111000`, bits `0x08/0x10/0x20`
   *   set); now independently re-confirmed via the same calibrated read.
   * **Net: for the Brave-scoped case this file documents, all three
   * `DS:0x5235..0x523d`-family unknowns are resolved and trivial**
   * (gate open, divisor `1`, flags already known) — this specific
   * literal-address table family is fully closed for Braves. **Does not
   * resolve `iVar14`/`iVar18`** (the two `FUN_281f_08bc()` calls feeding
   * `iStack_e8 = ((iVar18+1)/iVar14)*iVar20/divisor`) — those are calls
   * into the *generic field-index accessor*, same family as
   * `FUN_1000_8aac` (**T1.1**'s own still-open blocker), not a literal
   * `DS:0x52xx` address read; a different, separately-blocked piece of
   * this same formula, untouched by this table-location fix. The table's
   * *other* byte offsets (whatever sits at, e.g., `+8`/`+9`/`+10`) were
   * not decoded — they didn't cross-match `NAMES.TXT`'s remaining
   * columns cleanly at this same stride, likely different data
   * interleaved nearby; out of scope since nothing in this formula reads
   * them. Method note for reuse elsewhere: computing a `DS`-relative
   * address by segment arithmetic is fragile (the `DS` register at
   * capture time is not reliable — differs across saves even for
   * genuinely static data); prefer content-based byte-pattern search
   * first, use segment arithmetic only to sanity-check a hit already
   * found that way.
   */
  return score;
}

/* ====================================================================== */
/* Dir pick                                                               */
/* ====================================================================== */

/*
 * Ghidra: quiet Brave path through FUN_521d_20e6 | quiet_brave_pick_dir_asm
 *
 * Dir loop 0..7 (stay handled by caller for LCG). Reject ocean/HS / foreign.
 * Score = base + terrain; then if LAB_521d_54f5 gate: facing + −10 + fog;
 * else colony_pull (no-op early). Keep max.
 */
int quiet_brave_pick_dir_asm(
  int x,
  int y,
  int nation_id,
  int last_dir,
  int colony_count,
  int enable_fog,
  int mover_type /* Brave 19 */
) {
  int best_dir = VICEROY_DIR_STAY;
  int best_score = -0x3e7; /* ASM init local_e2 = 0xfc19 (−999) */

  for (int d = 0; d < 8; ++d) {
    int nx = x + k_dir8_dx[d];
    int ny = y + k_dir8_dy[d];

    uint8_t terr = (uint8_t)(terrain_byte(nx, ny) & VICEROY_TERRAIN_TYPE_MASK);
    if (terr == VICEROY_TERRAIN_OCEAN || terr == VICEROY_TERRAIN_HIGH_SEAS ||
        terr >= 0x18) {
      continue;
    }
    if (ocean_or_high_seas(nx, ny)) {
      continue;
    }
    int own = owner_nibble(nx, ny);
    if (own >= 0 && own != nation_id) {
      continue; /* foreign → combat path, not quiet */
    }

    int score = quiet_score_base();
    score = quiet_score_terrain(score, x, y, nx, ny, d);
    if (terr == VICEROY_TERRAIN_HIGH_SEAS) {
      score -= 0x10; /* LAB_521d_5070 — usually unreachable after reject */
    }

    if (quiet_lab_54f5_gate(nx, ny, nation_id)) {
      score = quiet_score_facing(score, d, last_dir);
      score = quiet_score_military_minus10(score, nx, ny, nation_id, mover_type);
      score = quiet_score_fog_explore(score, x, y, d, nation_id, enable_fog);
    } else {
      score = quiet_score_colony_pull(score, colony_count);
    }

    if (score > best_score) {
      best_score = score;
      best_dir = d;
    }
  }
  return best_dir;
}
