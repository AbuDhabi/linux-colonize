# Mysteries Catalog

Fields, flags, and DS globals whose real meaning was never pinned down —
distinct from `PARKED` (mechanic known, port deferred). Entries here are
"we don't fully know what this represents." Compiled 2026-08-19 by grep
sweep of `col1_save.h`, `src/core/*`, `original_sources_annotated/`,
`docs/`. Cross-ref: [[ai-transcription-fulldraft]].

## A. Save-format opaque bytes (`src/core/col1_save.h`)

Byte ranges read/written for save round-trip fidelity, no confirmed
gameplay meaning:

| Field | Note |
|---|---|
| `tut1.unused06`/`unused08` (was `unknown01`/`unknown02`, DS:0x5380 bit2/bit6) | **confirmed dead, renamed off "unknown"** — all 3 decompiled sources test/set bits 0x01/02/08/10/20/80 on 0x5380 (nr13-nr19), never 0x04/0x40; only touch is word-clear `*(u16*)0x5380=0` at init |
| `unknown05[2]` | head pad, no gameplay cite. **Narrowed 2026-08-24, not closed**: `0x540a` (the memset target) is also the base of a real 4-byte bit-array accessed by two small resident helpers, `FUN_12fd_000e(id,set)`/`FUN_12fd_0048(id)` (1-based bit-index set/clear/test — `*(byte*)((id-1)>>3 + 0x540a)`), so `unknown05` sits inside the *addressable range* of that accessor (bits 17-32 of a 32-bit space), not necessarily outside it. But the only caller found (`FUN_12fd_006c`, `viceroy_unpacked.c:5113+`) is a large, `WARNING`-flagged (unreachable-block-removed) function whose own switch/loop shape reads like cargo/inventory bit-twiddling, not event flags, and its own literal `id` argument is itself stack-passed and not recovered by the decompiler at any call site (`FUN_281f_051a`/`0524` thunks forward whatever `id` their own callers pass, also not recovered). So: real accessor confirmed, but neither "is `unknown05` actually used through it" nor "what would it mean if so" is resolved — the corrupted wrapper and lost literal args are a genuine dead end for this pass, not under-searching. Next step if resumed: raw-`.asm` PUSH-immediate reads at `FUN_12fd_006c`'s and the `051a`/`0524` thunks' call sites (same method that unstuck `T1.1`'s case dispatch) to recover the literal `id` values in play. |
| `unknown06_lo` (6 bits, player struct) | bits0-5 @ player+0x30 — no reader/writer cite in either decompiled export. **Was 7 bits; bit6 resolved 2026-08-19** as `lcr_case5_bonus_used`, a per-nation one-shot in `FUN_65dd_0004` (the still-PARKED LCR/native-encounter result table, see `indian_contact.md`'s de Soto note) that upgrades a first-time case-5 roll to case 4. **Case 4/5 correspondence resolved 2026-08-24** (see catalog section B's new `FUN_65dd_0004` entry) — case 4 is the burial-mounds/"search for treasure" event (`@LOSTCITY4`+`@SCREWED`+`@BURIAL1`/`2`/`3`); case 5 is *not* an independently-displayed result at all, it's a transient internal roll bucket that always converts to case 4 (first LCR ever, this bonus) or case 6 ("nothing", no text) before the display dispatch runs — so the bit's own description ("upgrades case 5 to case 4") was already accurate, just case 5 never had a `@LOSTCITY5`/`@BURIAL` text of its own to begin with. |
| `unknown13_pad[4]` (+0xbe) → **writer located 2026-08-19, live DOSBox-X trace + follow-up static dig** | founding=0 confirmed; lategame non-zero now explained. Live memory-watchpoint trace (played colony, DS:8542 colony-pointer anchor) caught the byte flip firing from a **periodic timer-tick interrupt handler**, not founding/player action. Live trace gave segment `124C`/tick counter `DS:0x8338`; static follow-up located it exactly in the raw disasm at **`ram:0000:a294`** (`OUT 0x20,AL` EOI + `IRET` — genuine INT8 ISR, never decompiled to a named C function by Ghidra, hence total invisibility to prior grep-only search). ISR dispatches via a `DS:0x376` countdown (resets to 5) to `FUN_1000_05f3` (~every 5th tick) and `FUN_1000_05f7` (every tick, gated by `DS:0x92f4`) — both decompiled (`viceroy_overlays.c:21487`/`21500`) but are themselves just indirect calls through function-pointer cells **`DS:0xa660`/`0xa664`** ("could not recover jumptable" per Ghidra). So the real per-tick handler is installed dynamically (callback/hook pattern); its install site isn't in any of the 3 decompiled exports or the raw `.asm` (checked). **2026-08-24: lead followed, still open, more precisely bounded.** Confirmed via Ghidra's own reference index (`tools/GhidraListXRefs.java`, `OverlayTest` project, `0000:a660`/`0000:a664` — the flat-linear resident space these cells actually live in, verified against the raw `viceroy_overlays.asm` disassembly at `ram:0000:a660`/`a664` which shows Ghidra mis-disassembling the pointer's own stored bytes as junk instructions, confirming it never resolved a code target there): **zero XREFs into either cell** — no absolute-address `mov [0xa660],...`/`[0xa664],...` writer exists anywhere in the analyzed project, not just in grepped text. So the writer, if findable statically at all, must use indexed/computed addressing (same "could not recover jumptable" class of problem as the readers themselves), not a plain absolute store — ruling out an easy grep-harder fix. Filed as `ai_port_plan.md`/`port_plan.md` **W4.4**, needs a live write-watch. `visible_to_euro` (`+0xba`) sibling likely shares this path, not separately re-checked |
| `unknown15_lo` (7 bits @ 0x3148) → **7 named single-bit fields, resolved 2026-08-19** | exhaustive walk of every literal `+0x3148` access across all 3 exports: bit0 confirmed dead (never touched); bit1 `roam_reeval_pending` (mirrors order-state∈{5,6}); bit2/bit3 `stack_has_founders_or_military`/`stack_has_military` (ship cargo — already resolved 2026-08-18 in `ai_euro.c`, just not propagated to the struct); bit4 `wander_dest_chosen` (explore-destination latch, less certain — traced inside a still-unnamed AI move-scorer); bit5 `garrison_request_pending` (matches `euro_goal_orders_0a60_full.md`'s own "garrison-check 0x3148 flags" note); bit6 `bound_in_transit` (ship en route to a colony, matches `nation_eot_ship_spawn.md`). All 4 of bits1/2/3/5 are reset to 0 every AI tick by `FUN_521d_0a60` (`&=0xd1`) then re-derived same-pass — per-tick scratch, not persistent history, despite round-tripping through the save |
| `unknown21` (+0xb) → **`unknown21_pad`, resolved 2026-08-19** | confirmed dead — no touch by literal offset in any of the 3 decompiled DOS exports; provably the one gap `FUN_38fd_6024`'s new-game zero-init skips (clears `+7..+0xa` then `+0xc..` separately), so on a fresh game it's plain uninitialized garbage |
| `unknown22` (+0x10) → **`king_audience_tax_delta`, resolved 2026-08-19** | signed tax-rate delta rolled by the King's-audience event (`FUN_38fd_5be8`): favor-score ladder picks a cut (RNG 2..5, capped, negated) or a raise (+1/+2/+3-4/+5-8); applied same-call via `FUN_38fd_3dc8(delta)` → `tax_rate += delta` (clamped 75). Save copy has no found DOS reader — write-through of an already-consumed value, not a pending slot. **Ported same day** into `src/core/ai_king.c` (`ai_king_audience_roll`/`ai_king_audience_apply_delta`/`ai_king_tax_event`), replacing the earlier invented Accept/Refuse-gates-the-hike design; see `original_sources_annotated/ai/king_ref.md` "Tax audience" section. |
| `unknown23_pad[4]` → **byte0 = `rebellion_pct_last_notified`, resolved 2026-08-19; bytes1-3 stay `unknown23_pad[3]`, confirmed dead** | byte0 (+0x1a): per-nation independence-progress news dedup latch — caches the last rebellion % the "rising/falling" popup reported (msg `0xf5e`/`0xf69`); crossing the difficulty threshold instead sets `nation_flags` bit `0x04` (**named this pass too: "nation achieved independence"**) and broadcasts a diplomatic update to the other 3 nations. Bytes1-3 (+0x1b..+0x1d): never touched anywhere |
| `unknown24_pad[4]` → **confirmed dead 2026-08-19** | no reader/writer anywhere outside new-game init (`FUN_38fd_6024` zeroes it alongside `royal_money`) — starts at 0, stays untouched all game |
| `unknown26[12]` (nation +area) | Linux repurposed as diplo stand-ins (treaty_timer/diplo_flag/etc.) — **exact DOS DS layout PARKED**, current field split is a Linux invention, not a decoded original. **`+0x40-0x43` sub-range narrowed 2026-08-24** — see section D | 
| `unknown28_pad` (+1) → **`sticky_trade_good`, resolved 2026-08-19** | cargo good index a tribe is mid-haggle over with the human trader (`FUN_4d56_2820`); 0xff=idle (sale closed), 0xfe=last visit refused outright, else=the good, read back next visit to resume the standoff. Already found and named in `indian_trade_2820.md`'s call-graph notes — just never propagated to the struct/catalog |
| `unknown31_lo_pad` (bits 0-4) → **confirmed dead 2026-08-24** | base found: DOS selects this record via `*(int*)0x8d4e = nation*0x4e + 0x5ad6` (`settlement_record_8d4a.md`'s sibling selector; `0x4e`=78=`sizeof(ColonizeCol1Indian)`, exact stride match). `unknown31_lo_pad` is bits 0-4 of `0x8d4e+3`; grepped every literal `*(int*)0x8d4e + 3` access across all 3 decompiled DOS exports (24 hits) — only masks `0x20`/`0x40`/`0x80` ever appear (the three already-named bits: `woi_defect_resolved`/`woi_defect_forced`/`extinct`), masks `0x01`/`0x02`/`0x04`/`0x08`/`0x10` never do |
| `unknown31b_pad`, `unknown31c_pad` (was `unknown31b`/`unknown31c`) → **confirmed dead 2026-08-24** | same `0x8d4e` base (see above): `unknown31b_pad` = `0x8d4e+4`, `unknown31c_pad` = `0x8d4e+9` — zero literal-offset touches for either across all 3 decompiled DOS exports. (Bit `0x20` of the sibling `unknown31_flags` byte *is* separately resolved — contact-prelude-fired.) `col1_save.h`/`tools/col1_json.c` renamed with `_pad` suffix, JSON key strings kept stable |
| `unknown31d[2]` → **`hill_silver_bid_bonus` (int16), resolved 2026-08-19, write side wired 2026-08-24** | write confirmed: map-gen tribe placement (`FUN_6a09_0006`) adds nation `tech` per nearby terrain-class-`0x1b` tile found in a 5×5 window around **every** placed tribe (capitals *and* satellites, not "capitals only" as first written); read confirmed: trade-meet economics (`FUN_4d56_2154`) divides by difficulty and feeds the tribe's Silver bid. Full write→read loop traced in DOS source. **Correction 2026-08-24: class `0x1b` is Mountains, not Hills** — verified via raw asm of `FUN_13e4_000e` and cross-checked against `map.c`'s independent `map_byte_is_mountain` convention; the field name stays as-is (no rename of a live field), but its real meaning is "mountain," not "hill" — flag this if the name is ever revisited. Write side was previously unported (field always read as 0 in procedural games) — now wired in `ai_place_tribes_procedural` (`ai.c`). Same pass found and fixed a matching inverted `0x1b`/`0x1c` ternary bug in `ai.c`'s `ai_decoded_type`. |
| `unknown33_pad[8]` (was `unknown33[8]`, +0x3e) → **confirmed dead 2026-08-24** | same `0x8d4e` base as `unknown31_lo_pad` above (`0x8d4e+0x3e`..`+0x45`) — zero literal-offset touches across the whole 8-byte range in any of the 3 decompiled DOS exports (checked every offset `0x3e`-`0x45`, not just the range endpoints). Opaque in DOS; Linux formerly parked peace bookkeeping here, since moved to `euro_diplo[4]`. Renamed with `_pad` suffix in `col1_save.h`/`tools/col1_json.c`, JSON key string kept stable |
| `unknown34[12]` → **`unknown34_pad[12]`, confirmed dead 2026-08-19** | DS:0x9566 — exhaustively checked across all 3 decompiled exports; only touches are the bulk save-block R/W calls, zero semantic reader/writer anywhere. Genuinely vestigial, content unrecoverable |
| `unknown36[577]` region (file offset 140..716) → **stale entry, region already characterized elsewhere; catalog just never synced** | This isn't one opaque blob any more (and per the col1_save.h comment there since renamed, hasn't been for a while) — it's `ColonizeCol1Stuff`'s 33 DS-named chunks, and `docs/save_format_map.md`'s "Stuff (727)" table (built 2026-08-14) already gives per-chunk stride + confirmed semantics for the great majority of them via `FUN_4962_0018`/`FUN_4962_06b6` raw-`.asm` register traces: e.g. `0x947e`=village-counts-by-continent, `0x95f2`=continent-presence-flags, `0x94a6`/`0x94e6`/`0x95b2`/`0x9526`/`0x918c`/`0x9572`=per-continent×nation unit/colony/combat/skill tallies, `0x9622`/`0x962a`=tribe population/village-count-by-type (the two `417e` Incite-price terms), `0x91cc`=brave-combat-value-by-tribe-type×continent (misnamed `tribe_dwellings_91cc`, real role confirmed, rename deferred as its own careful pass — save-format-critical). **Renamed 2026-08-26** (was: "8 of these chunks still carry generic `unknown_ds_XXXX` names"): `947e`→`village_counts_by_continent`, `94a6`→`land_unit_counts_by_continent`, `95b2`→`field_combat_strength_by_continent`, `9526`→`skilled_unit_counts_by_continent`, `918c`→`unit_value_sum_by_continent`, `9572`→`combat_value_sum_by_continent`, `9622`→`tribe_population_totals`, `962a`→`tribe_village_counts` (`col1_save.h`, `col1_stuff_census.c`, `tools/col1_json.c` struct refs; JSON key strings kept stable per the existing `unknown31b/31c_pad` precedent). `ctest`: 42/42, no regressions — pure rename, no behavior change. `95f2`/`94e6`/`944e` intentionally left generic (lower-confidence/placeholder per their own rows above); `91cc` stays deferred as its own careful pass, unchanged |
| `unknown45_pad[8]` → **4 named int16 slots, resolved 2026-08-19** | `crown_nation_id` (DS:0x53d2, already named elsewhere as "Crown nation" — `combat.md`/`king_ref.md`/`ai_king_crown_nation`, just never in `col1_save.h`); `rival_nation_slot_1`/`_2` (0x53d4/0x53d6, year-end chrome's lazily-picked "rival nation" for SoL-pressure reports — `year_end_chrome.md`'s "crown name via 0x53d4" is this same cache, not a second crown); `sol_pct_last_notified` (0x53d8, human-colony SoL-pressure dedup latch — same shape as `rebellion_pct_last_notified`). All 4 reset together (`-1/-1/-1/0`) at new game (`FUN_75c2_235c`) |
| `unknown46[32]` / `price_group_state[16]` → **formula traced 2026-08-19** | despite the name, not the displayed price — each of the 16 slots is a hidden per-cargo market **saturation/demand pool** (`FUN_38fd_0058`), seeded `RNG(600,1000)` at new game, topped up each call by summing a per-Euro-nation demand table. Index 0 (Food) gets an extra decay rule (turn1/nation0 only); indices 9-12 (Rum/Cigars/Cloth/Coats) get a floor-at-1 clamp — DOS *code* confirmation of the field's own long-standing "only these 4 form a live price group" smcol/experimental guess. `ColonizeCol1NationTrade.euro_price[16]` is the actual shown price; the formula connecting the two wasn't traced this pass. Still collides with the live king-stand-ins union on bytes 0-5 — not touched, same reasoning as the `unknown26` meta-mystery |
| DS:0x54f6 table `[origin*9+nation]`, int16 elements → **relabeled "grudge/tension", corrected 2026-08-19; write-side formula pinned down 2026-08-19** | previous "wealth/tribute" guess was wrong. `origin` = a unit's home-settlement id (`unit+6`, stable settlement id, not the live tribe array index); `nation` = Euro nation. Confirmed: incremented by hostile acts (RMW sites, clamp-floor-at-0); read as a `>0x7f` hostility gate in AI move-scoring (`FUN_521d_0896`); read `>>5` clamped 0-3 as a 4-tier relations-report rating icon. The "capped/reset-to-0 on relation improvement" write site is `FUN_4cc6_00f2` (relation-delta, already ported thin as `ai_diplo_indian_relation_delta`) on a negative-delta tier-crossing: for every tribe of the affected Indian nation, if the nation's and Indian's own `FUN_281f_0a60`-rated combat strengths are within one tier of each other, *clamp* the existing value to `0x20` (roughly-equal strength) or `0x60` (Indian much weaker); if the strength gap is ≥2 tiers, *hard-reset to 0* instead — full read in `indian_euro_23000_matrix.md`'s new "max-relation branch" section. Meaning now understood, including the write formula; **wired 2026-08-24** — `ColonizeCol1Save.indian_tension` (runtime-only, not DOS save-file layout) + `ai_diplo_indian_tension_tier_update`, called from `ai_diplo_indian_relation_delta`. Correction from that pass: `iVar3`/`iVar6` are `FUN_281f_0a60` (quartile bucketer, `ai_indian_152e_quartile`) applied to the OLD/NEW *relation* value, not a separate combat-strength stat as previously written above — so the gap is always bounded to `{-1,0,1}` and the "hard-reset to 0 on ≥2-tier gap" `else` branch is **dead code** in the shipped binary; only the clamp-to-`0x20`/`0x60` arm is real. Still not read anywhere in Linux (the two DOS read sites — `FUN_521d_0896` hostility gate, `>>5` report-icon tier — are Euro-AI/reports territory, separate items). **Second write site found and wired same day (round 3):** `FUN_5fef_0f14` (colony raid loot, `viceroy_unpacked.c:100034`) unconditionally zeroes the raiding tribe's tension slot on every raid outcome including total wipe — raiding itself discharges tension, win or lose. Wired into `ai_contact_indian_raids` (`ai_contact.c`), keyed by `ColonizeUnit.home_tribe_id`. Two more `0x54f6`-touching sites (`FUN_5fef_1b0e` empty-tile attack, `FUN_5fef_31ea` Cortes/Sepulveda fallout — the latter a corrupted `unaff_BP` decompile) remain unwired, both living in `units.c`, out of the Indian/Contact domain. See `indian_euro_23000_matrix.md`'s 2026-08-24 update and `indian_raid_loot.md` for the full trace |

Pattern across most of these: writer confirmed (byte position + size right
for save-file round-trip), but *semantic* reader/effect never traced in
DOS code — so Linux either leaves them opaque pad or repurposes the space
for its own bookkeeping, which is a Linux invention wearing the old
field's address, not a decode of the original.

## B. Semantic-RE dead ends (named function, meaning still guessed/unfound)

- **`FUN_4d56_417e` (Incite Indians) — RESOLVED 2026-08-27, static-only.**
  Caller is `FUN_4d56_4528`'s tail `switch(uStack_56)` `case 7`
  (`OVL13::4b80 → thunk OVL13::4c36 → FUN_1000_a5b8 → JMPF 0000:417e`);
  the `1000:a5bd JMPF 0x0000:417e` stub previously dismissed as a "false
  lead" was the real resident thunk (`0000` = RTLink load-time-patched
  segment, same shape as the trusted `1000:a641 → 2820` stub). Push order
  matches the live capture exactly. Mode 1 = human picks the Missionary
  "Incite Indians" menu entry (`*0x9336`); Mode 2 = **real**: AI
  Missionary entering a village, gated on tribe-relation-to-human <75,
  human MET bit, AI poorer than human (`0x917c` rank), AI gold ≥1500,
  RNG(0,4)≠0 or no mission. Side effects: the 4528 doc's 2026-08-21
  polarity flip (`0`=AI) and its "all 8 tail thunks resolve to one OVL11
  utility" claim were both wrong — retracted in
  `indian_settlement_4528.md` (2026-08-27 section, full 9-case target
  table). Mode 2 still unported (`ai_port_plan.md` T4.5).

- **`FUN_5fef_0000`** (best-defender-unit-at-tile scoring walk, resident,
  99111/98). Hand-transcribed from clean disassembly (decompiler pcode
  bug blocked normal recovery). **Fully resolved 2026-08-19**: all 7
  previously-unnamed call targets now trace via `tools/address_mapping.csv`'s
  resident-thunk chain — 3 are already-named `accessors.c` helpers
  (`euro_settlement_owner`/`map_tile_in_bounds`/`ocean_or_high_seas`), 2 are
  the standard unit-list first/next iterator pair, and the last 2
  (`FUN_1000_8bb8`/`8bcc`) both land on the already-authoritative combat
  pair `FUN_157e_004a`/`015e` (`combat_unit_base_x8` / `combat_engagement_strength`,
  `docs/combat.md`) — `8bb8` via `FUN_281f_09c8` (a "gap" row in
  `address_mapping.csv`: no decompiled `FUN_1000_8bb8` stub, but its
  canonical target `FUN_281f_09c8` is a plain 2-instruction thunk straight to
  `FUN_157e_004a`, confirming the doc's own prior guess and closing the "no
  resident thunk stub" dead end without a raw-offset lookup); `8bcc` via
  `FUN_281f_09dc` → `FUN_157e_015e`, matching the 2026-08-19 pass already on
  record. So the scoring loop's `local = (byte<<8) - prior - 1` is confirmed
  a real attack-base-vs-defense-site combat differential, not a distance
  metric — both terms are combat-strength calls, not merely "same segment as."
  See `indian_raid_loot.md` for the full chain. Still not ported (structural
  read only), but every callee is now a named, already-documented function —
  no remaining "wall of unnamed callees" blocker.

- **`FUN_521d_5b66` — resolved 2026-08-19 (doc-sync, not fresh RE):
  `euro_unit_act.md` already located the real owner on 2026-08-13/14,
  just never made it back to this catalog.** `5b66` itself is a clean,
  tiny 44-line per-unit dispatcher (confirmed via `tools/address_mapping.csv`
  re-disassembly, no corruption): it calls `FUN_521d_20e6` — the giant
  2170-line move-scoring function, already separately documented in
  `move_scoring.md` — and if that returns non-zero (unit already acted),
  aborts; otherwise falls into its own small `switch(orders_state)` with 5
  real handlers (`FUN_479b_076e`/`01a6`/`0526`/`0972`, `FUN_1427_155e`).
  Two independently-written docs cross-confirm the call direction
  (`euro_unit_act.md`'s trace of `5b66`'s own body vs. `move_scoring.md`'s
  caller list for `20e6`). The ~1815-line estimate was corrupted-decompile
  artifact, not real inline content — no separate "still-unfound" function
  exists. A related false lead (an `a6e4`/"12-byte pattern" chase that
  looked like a data table) is also closed: it was an unpatched RTLink
  call-thunk placeholder, and its real target (`FUN_4cc6_0000`, WoI
  tribe-defection) is unrelated to `5b66` — see `euro_unit_act.md`'s own
  "Method note" on this failure mode (naive placeholder-byte reads = false
  leads) before chasing another one of these.

- **`FUN_65dd_0004` LCR case 4/5 ↔ `@LOSTCITY`/`@BURIAL` naming — narrowed 2026-08-24, not a symmetric two-case mapping after all.** Read the roll/dispatch body directly (`viceroy_unpacked.c:103463-103757`, clean, no `WARNING`s). Findings:
  - **Case 4 = the burial-mounds "search for treasure" event** (`@LOSTCITY4` prompt / `@SCREWED` hostile branch), confirmed via its own dedicated post-loop block (`:103653-103715`): a `local_3a` variant selector (1/2/3, rolled from the same `local_c` magnitude byte used for every other case) picks a payout size — `local_3a==1`→no gold, `local_3a==2`→modest 3d8×10 gold, `local_3a==3`→writes a map treasure marker requiring pickup — passed straight into the display call `FUN_281f_0652(0x281f, local_2c, 3, local_3a)` against string context `0x1db7`. Variant shape matches `@BURIAL1` (empty)/`@BURIAL2` (trinkets worth `%NUMBER0`)/`@BURIAL3` (incredible treasure worth `%NUMBER1`, **"It will take a Galleon to get this treasure back"** — exactly the treasure-marker-requiring-pickup branch) one-to-one. High confidence.
  - **Case 5 has no independent display of its own — it isn't a second named result.** Every code path that produces the internal roll bucket `local_8==5` reassigns it *before* the loop's break/dispatch: the per-nation first-time check (`*(byte*)(nation*0x34+0x543e)&0x40`, the same byte `lcr_case5_bonus_used` already documents) either sets it to **4** (this is the "upgrade" the bit already describes) or, if already used this game, leaves it to fall to **6** ("nothing", no text, silently reroll/exit) on the very next lines. So `lcr_case5_bonus_used`'s own description was already accurate; there is no missing `@LOSTCITY5`("vanished without a trace")/`@BURIAL` pairing to resolve — `@LOSTCITY5` is a *different*, separately-triggered bad-outcome text, not case 5's own display (not chased further this pass, out of scope).
  - **Not resolved this pass, and flagged as a real dead end, not under-searching**: the numeric "message id" arguments used throughout this function (`FUN_281f_048e(0x281f, 0x37/0x3c/0x33)` for cases 1/2/4's dialog headers, `FUN_1d1d_07e4(local_2c, 0x1dae/0x1db7)` for the two string contexts) don't correspond to GAME.TXT line numbers or any documented index scheme found in this project — there's no compiled `.MSG` resource alongside `GAME.TXT`, so these are presumably indices into a table the game builds at load time by scanning `@TAG` blocks in file order, but no tool in `tools/` recovers that mapping, and hand-counting `@TAG`s to match a 4-digit id is unreliable enough not to guess. If a future pass needs the exact ids (cases 1/2's own message text, or 0x1dae's fallback text), building that index-recovery tool is the real next step, not another grep.
  - **RESOLVED 2026-08-27 (static only, no live session).** They aren't message ids at all: `FUN_281f_048e` → `FUN_129f_02cc` is the *sound/event* queue (FUNCTION_CATALOG), so `0x37/0x3c/0x33/0x24/0x32` are sound ids (FoY / Cibola / burial prompt / burial treasure / SCREWED = the same `0x32` military sting `units_combat_music_sting` already plays). The "string contexts" `0x1dae/0x1db7/0x1dbe` are plain DS strings — `strcpy` (`FUN_1d1d_07e4`) of `"LOSTCITY"`, `"BURIAL"`, `"SCREWED"` (VICEROY.EXE byte offsets 128846/128855/128862, spacings 9 and 7 = string lengths incl. NUL) with the case / variant number appended by `FUN_281f_0182` — so **case N == `@LOSTCITYN` literally**, no index table exists. That also fixes the P7.1 case identity: case 2 spawns unit type 10 (Treasure, `+0x315b` = value/100) = Cibola; case 9 spawns type 0 (Colonist) = survivors — the Linux port had those two swapped; fixed. The other P7.1 unknowns fell the same way: `FUN_281f_078c` is `terrain_class_at` (SYMBOL_MAP, pedia class) — case 1 needs class<24 && (class&7)>3 (grassland/savannah/marsh/swamp ± forest), case 2 needs mountain/hill or (class&7)==1 (desert/scrub); `-0x6d68` = DS:0x9298 `colony_counts`, `-0x6bf0` = DS:0x9410 `census_pop_proxy` (both already named in `col1_save.h`); `0x1dc6/0x1dc7` are unsaved session counters (rumours explored / Cibolas found, bytes right after "SCREWED") — ported as statics. All now live in `units_lcr_roll_outcome`; ctest 44/44.

- **Nation-slot table ambiguity (resolved, but the resolution itself
  reveals a live confusion risk).** Pass 17 of the `417e` investigation
  briefly concluded Indian tribes have real entries (`nation[11]`) in the
  same 12-slot table `difficulty.md` calls the "Europe block" — later
  retracted (pass 18: a stack-offset mis-decode, real value was Euro
  nation 0). The retraction is solid, but it means the boundary of that
  12-slot table (is it strictly 4 Euro nations, or does it extend to
  cover tribes too under some other code path?) was never independently
  re-confirmed after the correction — just no longer *contradicted*.

## C. Doc-flagged "Open RE" items still open

- `original_sources_annotated/ai/indian_settlement_4528.md` — ASM-faithful
  map of byte range 84216→end not done; string table XREF
  `0x1710`…`0x172e` never resolved against `GAME.TXT`.
- `original_sources_annotated/ai/indian_trade_2820.md` — human `CHOICE`
  dialog buy-offer path (`LAB_002e92`) price formula: same general shape
  as the AI path, different RNG/UI gating, never traced.
- `original_sources_annotated/turn/between_turns.md` — `FUN_4d56_1816`'s
  dispatcher forge-edge narrowed to overlay `0x0C`/`VR_2A02` but **not
  proven** to be the `130d` edge specifically (best current guess, not
  confirmed).

## D. Meta-mystery

`ColonizeCol1Nation.unknown26[12]`'s Linux-side reinterpretation
(treaty_timer/diplo_flag/indian_hostility_sticky/privateer_spawn_mask) is
functional and tested, but is explicitly **not** a decode of what those
12 DOS bytes actually held — anyone reading the struct casually could
mistake "has named sub-fields" for "meaning resolved." Worth a stronger
comment than "exact DS PARKED" if this keeps tripping people up.

**Stronger comment added 2026-08-19** (in `col1_save.h`, not a rename —
this union is live gameplay code, deliberately left alone). Two of the
three sub-ranges now have concrete DOS answers, both confirming the Linux
names are stand-ins:
- `+0x44/+0x45` ("`diplo_flag[0..1]`"): real content is per-nation
  **recruit-type RNG cycling state** (`FUN_38fd_46d4`/`FUN_38fd_6024`) —
  seeded at nation creation, mutated every Europe-dock recruit pick.
  Nothing to do with diplomacy.
- `+0x48/+0x49/+0x4a` ("`indian_hostility_sticky`/`privateer_spawn_mask`/
  `unknown26_pad`"): a **crosses/hammers-pool carry-normalize accumulator**
  (`FUN_4d56_4528`/`FUN_4d56_5d04`) — already independently found and kept
  as separate scratch in `ai_euro.c`'s `Ai5d04HireScratch`, for exactly
  this reason; confirmed again this pass from a second call site.
- `+0x40-0x43` ("`treaty_timer[4]`"): **resolved 2026-08-24 (round 2) — this
  is a genuine numeric cooldown/countdown, not a boolean, and the second
  writer's formula, gate, and readers are all now pinned down.**
  Base confirmed via struct-offset math (`nation_flags` at DS `-0x77f8`
  unsigned `0x8808`, `+0x40` → `-0x77b8` unsigned `0x8848`) and cross-checked
  against a literal-offset grep across all 3 decompiled DOS exports. Two
  confirmed writers, both indexed `[nation_a][nation_b]` via
  `param_b + param_a*0x13c + -0x77b8` (row stride `0x13c` = `sizeof`
  the DOS per-nation record, symmetric — always written both directions):
  - `FUN_5bfb_13b0` (form/break alliance, already ported as
    `ai_diplo_form_alliance_ctx`/`ai_diplo_break_alliance_ctx`) writes
    literal **`1`** on forming an alliance and **`0`** on breaking one
    (`viceroy_unpacked.c:97291-97292`/`97308-97309`).
  - `FUN_5bfb_153e` itself, at its single common exit (`LAB_..._0034de`,
    reached from nearly every internal branch — this is effectively the
    function's tail, not a rare edge case): re-read directly off the
    canonical export (`viceroy_unpacked.c:98423-98426`), cross-confirmed
    byte-for-byte against a second, independently decompiled export of the
    same tail (`viceroy_overlays.c:84013-84025`, a register-based
    `unaff_BP` variant of the identical code — not a decompiler artifact).
    Gate: `FUN_1000_8c28(self,target)&0x40` — already-resolved
    `AI_DIPLO_MET` (`ai_diplo.h`). Formula, exact now (the earlier "-ish"/
    "difficulty" framing was a correct hedge, no longer needed):
    `value = (DS:0x53a6 - 6) * -2`, where `DS:0x53a6` is the already-named
    `VICEROY_DS_DIFFICULTY` global (`viceroy_globals.h:68`), then
    **halved** (`>>1`) if `FUN_1000_89a4(self,0x13)` — a per-nation
    FF/feature-bit test whose specific bit `0x13` selects is still
    unresolved elsewhere in this project (`euro_diplo_153e_full.md`'s
    worthiness-score writeup already stubs this same accessor to "absent").
    So the unhalved range is `12`(difficulty 0)..`4`(difficulty 4), halved
    to `6..2` when that FF bit is set — always a small positive magnitude,
    never the literal `0`/`1` `13b0` writes.
  **Readers found this pass** (grepped every `-0x77b8`/`0x77b8` literal
  touch across all 3 decompiled exports — the write list above is now
  known-complete, and so is this read list):
  - `FUN_521d_6d8e` (`viceroy_unpacked.c:93172-93187`, the Euro-AI
    per-nation-turn dispatcher) unconditionally decrements the cell by 1
    every turn once nonzero, and — only in the turn(s) the cell reads `0`,
    gated on a *different* diplo-flag bit (`&8`, not yet named) plus a
    1-in-3 RNG roll — tweaks the `-0x77c4` relation byte (clears bits
    `0x08`/`0x40`, sets bit `0x01`). This is **already ported**, and the
    decrement half already matches: `ai_diplo.c`'s `ai_diplo_treaty_timers`
    carries the comment "6d8e step 4: decrement per-rival treaty timer
    bytes before planning" and does exactly that (decrement, floor 0,
    re-checked every turn while at 0) — confirming the Linux name/shape
    was right all along. The zero-tick expiry *action* itself is a
    different, simpler Linux invention (break alliance if `AI_DIPLO_ALLY`,
    else a 1-in-8 reset to plain `PEACE|MET`) rather than a decode of
    DOS's own bit-`8`+RNG(1-in-3)+relation-mask tweak — not revisited this
    pass, out of this task's scope.
  - `FUN_465b_0000` (`viceroy_unpacked.c:75417` on, the "moving onto a
    foreign-owned tile" handler) reads the cell as a plain **nonzero
    gate**: `if (met && timer != 0) skip further war/reaction escalation
    on this encounter` (`viceroy_unpacked.c:75551-75556`). This whole body
    is Linux's `move_spent_foreign_combat_parked` — explicitly **PARKED**,
    not ported (`original_sources_annotated/ai/move_spent.c:191-206`).
  **Semantics, now settled**: DS `-0x77b8` is a genuine per-nation-pair
  **diplomatic cooldown counter** ("don't re-trigger a war/reaction
  encounter, or the AI's own war-fatigue-flip roll, against this rival for
  N more turns"), ticked down once per turn by `6d8e` and read as a guard
  by both `6d8e`'s own zero-tick branch and `465b_0000`'s encounter gate.
  `13b0`'s plain `1`/`0` writes are just the low/degenerate end of that
  same numeric range (alliance just formed → minimal 1-turn cooldown;
  alliance just broken → 0, immediately eligible again), not a separate
  boolean field — the field really is one thing throughout, just with a
  wide write-value range. Confirms Linux's own `treaty_timer[4]` naming was
  apt in spirit even before this pass.
  **Not ported this round**: `153e`'s own difficulty-scaled reset formula.
  `153e` is not wired live anywhere in Linux (`ai_diplo.c`'s `153e`
  comments: its outcome-table dispatch and worthiness-score phase are
  structural references only, deliberately not called) and its only DOS
  caller (`FUN_5bfb_3180`, the adjacent-unit encounter resolver) isn't
  wired for the Euro×Euro case either — so there is no live call site to
  attach this reset to without porting the rest of `153e` first, which is
  out of scope here. Same "documented, not ported" outcome as the
  already-closed `+0x44/+0x45`/`+0x48-0x4a` sub-ranges. Genuinely still
  open, low-value: the `0x13` FF/feature-bit meaning (`FUN_1000_89a4`) and
  `6d8e`'s own `&8` gate bit — neither blocks this writeup, both already
  tracked as open elsewhere in this project.

---
Not included: `PARKED` markers (~90+ across `ai_diplo.c`/`ai_king.c`) —
those are known DOS mechanics with deferred *UI/dialog* ports, not
unresolved meaning. See `docs/ai_transcription.md` status tables instead.
