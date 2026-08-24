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
| `unknown05[2]` | head pad, no gameplay cite — only touch is the 4-byte memset that also clears `event` at new game (`FUN_1d1d_0dae(0x540a,0,4)`); no standalone R/W found |
| `unknown06_lo` (6 bits, player struct) | bits0-5 @ player+0x30 — no reader/writer cite in either decompiled export. **Was 7 bits; bit6 resolved 2026-08-19** as `lcr_case5_bonus_used`, a per-nation one-shot in `FUN_65dd_0004` (the still-PARKED LCR/native-encounter result table, see `indian_contact.md`'s de Soto note) that upgrades a first-time case-5 roll to case 4; which named `@LOSTCITY`/`@BURIAL` outcome cases 4/5 correspond to is still unresolved |
| `unknown13_pad[4]` (+0xbe) → **writer located 2026-08-19, live DOSBox-X trace + follow-up static dig** | founding=0 confirmed; lategame non-zero now explained. Live memory-watchpoint trace (played colony, DS:8542 colony-pointer anchor) caught the byte flip firing from a **periodic timer-tick interrupt handler**, not founding/player action. Live trace gave segment `124C`/tick counter `DS:0x8338`; static follow-up located it exactly in the raw disasm at **`ram:0000:a294`** (`OUT 0x20,AL` EOI + `IRET` — genuine INT8 ISR, never decompiled to a named C function by Ghidra, hence total invisibility to prior grep-only search). ISR dispatches via a `DS:0x376` countdown (resets to 5) to `FUN_1000_05f3` (~every 5th tick) and `FUN_1000_05f7` (every tick, gated by `DS:0x92f4`) — both decompiled (`viceroy_overlays.c:21487`/`21500`) but are themselves just indirect calls through function-pointer cells **`DS:0xa660`/`0xa664`** ("could not recover jumptable" per Ghidra). So the real per-tick handler is installed dynamically (callback/hook pattern); its install site isn't in any of the 3 decompiled exports or the raw `.asm` (checked). **Next-session lead**: grep/live-watch writes to `0xa660`/`0xa664` to find what installs the colony-screen tick handler — much narrower target than before. `visible_to_euro` (`+0xba`) sibling likely shares this path, not separately re-checked |
| `unknown15_lo` (7 bits @ 0x3148) → **7 named single-bit fields, resolved 2026-08-19** | exhaustive walk of every literal `+0x3148` access across all 3 exports: bit0 confirmed dead (never touched); bit1 `roam_reeval_pending` (mirrors order-state∈{5,6}); bit2/bit3 `stack_has_founders_or_military`/`stack_has_military` (ship cargo — already resolved 2026-08-18 in `ai_euro.c`, just not propagated to the struct); bit4 `wander_dest_chosen` (explore-destination latch, less certain — traced inside a still-unnamed AI move-scorer); bit5 `garrison_request_pending` (matches `euro_goal_orders_0a60_full.md`'s own "garrison-check 0x3148 flags" note); bit6 `bound_in_transit` (ship en route to a colony, matches `nation_eot_ship_spawn.md`). All 4 of bits1/2/3/5 are reset to 0 every AI tick by `FUN_521d_0a60` (`&=0xd1`) then re-derived same-pass — per-tick scratch, not persistent history, despite round-tripping through the save |
| `unknown21` (+0xb) → **`unknown21_pad`, resolved 2026-08-19** | confirmed dead — no touch by literal offset in any of the 3 decompiled DOS exports; provably the one gap `FUN_38fd_6024`'s new-game zero-init skips (clears `+7..+0xa` then `+0xc..` separately), so on a fresh game it's plain uninitialized garbage |
| `unknown22` (+0x10) → **`king_audience_tax_delta`, resolved 2026-08-19** | signed tax-rate delta rolled by the King's-audience event (`FUN_38fd_5be8`): favor-score ladder picks a cut (RNG 2..5, capped, negated) or a raise (+1/+2/+3-4/+5-8); applied same-call via `FUN_38fd_3dc8(delta)` → `tax_rate += delta` (clamped 75). Save copy has no found DOS reader — write-through of an already-consumed value, not a pending slot. **Ported same day** into `src/core/ai_king.c` (`ai_king_audience_roll`/`ai_king_audience_apply_delta`/`ai_king_tax_event`), replacing the earlier invented Accept/Refuse-gates-the-hike design; see `original_sources_annotated/ai/king_ref.md` "Tax audience" section. |
| `unknown23_pad[4]` → **byte0 = `rebellion_pct_last_notified`, resolved 2026-08-19; bytes1-3 stay `unknown23_pad[3]`, confirmed dead** | byte0 (+0x1a): per-nation independence-progress news dedup latch — caches the last rebellion % the "rising/falling" popup reported (msg `0xf5e`/`0xf69`); crossing the difficulty threshold instead sets `nation_flags` bit `0x04` (**named this pass too: "nation achieved independence"**) and broadcasts a diplomatic update to the other 3 nations. Bytes1-3 (+0x1b..+0x1d): never touched anywhere |
| `unknown24_pad[4]` → **confirmed dead 2026-08-19** | no reader/writer anywhere outside new-game init (`FUN_38fd_6024` zeroes it alongside `royal_money`) — starts at 0, stays untouched all game |
| `unknown26[12]` (nation +area) | Linux repurposed as diplo stand-ins (treaty_timer/diplo_flag/etc.) — **exact DOS DS layout PARKED**, current field split is a Linux invention, not a decoded original |
| `unknown28_pad` (+1) → **`sticky_trade_good`, resolved 2026-08-19** | cargo good index a tribe is mid-haggle over with the human trader (`FUN_4d56_2820`); 0xff=idle (sale closed), 0xfe=last visit refused outright, else=the good, read back next visit to resume the standoff. Already found and named in `indian_trade_2820.md`'s call-graph notes — just never propagated to the struct/catalog |
| `unknown31_lo_pad` (bits 0-4) | no reader cite |
| `unknown31b`, `unknown31c` | no reader cite (bit 0x20 of the sibling `unknown31_flags` byte *is* resolved — contact-prelude-fired) |
| `unknown31d[2]` → **`hill_silver_bid_bonus` (int16), resolved 2026-08-19** | write confirmed: map-gen tribe placement (`FUN_6a09_0006`) adds nation `tech` per nearby Hill tile found around each new capital; read confirmed: trade-meet economics (`FUN_4d56_2154`) divides by difficulty and feeds the tribe's Silver bid. Full write→read loop traced in DOS source, not just inferred from the Linux port's prior guess |
| `unknown33[8]` (+0x3e) | opaque in DOS; Linux formerly parked peace bookkeeping here |
| `unknown34[12]` → **`unknown34_pad[12]`, confirmed dead 2026-08-19** | DS:0x9566 — exhaustively checked across all 3 decompiled exports; only touches are the bulk save-block R/W calls, zero semantic reader/writer anywhere. Genuinely vestigial, content unrecoverable |
| `unknown36[577]` region (file offset 140..716) | DS-named save chunks per `FUN_75c2_0288`, not further decoded |
| `unknown45_pad[8]` → **4 named int16 slots, resolved 2026-08-19** | `crown_nation_id` (DS:0x53d2, already named elsewhere as "Crown nation" — `combat.md`/`king_ref.md`/`ai_king_crown_nation`, just never in `col1_save.h`); `rival_nation_slot_1`/`_2` (0x53d4/0x53d6, year-end chrome's lazily-picked "rival nation" for SoL-pressure reports — `year_end_chrome.md`'s "crown name via 0x53d4" is this same cache, not a second crown); `sol_pct_last_notified` (0x53d8, human-colony SoL-pressure dedup latch — same shape as `rebellion_pct_last_notified`). All 4 reset together (`-1/-1/-1/0`) at new game (`FUN_75c2_235c`) |
| `unknown46[32]` / `price_group_state[16]` → **formula traced 2026-08-19** | despite the name, not the displayed price — each of the 16 slots is a hidden per-cargo market **saturation/demand pool** (`FUN_38fd_0058`), seeded `RNG(600,1000)` at new game, topped up each call by summing a per-Euro-nation demand table. Index 0 (Food) gets an extra decay rule (turn1/nation0 only); indices 9-12 (Rum/Cigars/Cloth/Coats) get a floor-at-1 clamp — DOS *code* confirmation of the field's own long-standing "only these 4 form a live price group" smcol/experimental guess. `ColonizeCol1NationTrade.euro_price[16]` is the actual shown price; the formula connecting the two wasn't traced this pass. Still collides with the live king-stand-ins union on bytes 0-5 — not touched, same reasoning as the `unknown26` meta-mystery |
| DS:0x54f6 table `[origin*9+nation]`, int16 elements → **relabeled "grudge/tension", corrected 2026-08-19; write-side formula pinned down 2026-08-19** | previous "wealth/tribute" guess was wrong. `origin` = a unit's home-settlement id (`unit+6`, stable settlement id, not the live tribe array index); `nation` = Euro nation. Confirmed: incremented by hostile acts (RMW sites, clamp-floor-at-0); read as a `>0x7f` hostility gate in AI move-scoring (`FUN_521d_0896`); read `>>5` clamped 0-3 as a 4-tier relations-report rating icon. The "capped/reset-to-0 on relation improvement" write site is `FUN_4cc6_00f2` (relation-delta, already ported thin as `ai_diplo_indian_relation_delta`) on a negative-delta tier-crossing: for every tribe of the affected Indian nation, if the nation's and Indian's own `FUN_281f_0a60`-rated combat strengths are within one tier of each other, *clamp* the existing value to `0x20` (roughly-equal strength) or `0x60` (Indian much weaker); if the strength gap is ≥2 tiers, *hard-reset to 0* instead — full read in `indian_euro_23000_matrix.md`'s new "max-relation branch" section. Meaning now understood, including the write formula; **wired 2026-08-24** — `ColonizeCol1Save.indian_tension` (runtime-only, not DOS save-file layout) + `ai_diplo_indian_tension_tier_update`, called from `ai_diplo_indian_relation_delta`. Correction from that pass: `iVar3`/`iVar6` are `FUN_281f_0a60` (quartile bucketer, `ai_indian_152e_quartile`) applied to the OLD/NEW *relation* value, not a separate combat-strength stat as previously written above — so the gap is always bounded to `{-1,0,1}` and the "hard-reset to 0 on ≥2-tier gap" `else` branch is **dead code** in the shipped binary; only the clamp-to-`0x20`/`0x60` arm is real. Still not read anywhere in Linux (the two DOS read sites — `FUN_521d_0896` hostility gate, `>>5` report-icon tier — are Euro-AI/reports territory, separate items). See `indian_euro_23000_matrix.md`'s 2026-08-24 update for the full trace |

Pattern across most of these: writer confirmed (byte position + size right
for save-file round-trip), but *semantic* reader/effect never traced in
DOS code — so Linux either leaves them opaque pad or repurposes the space
for its own bookkeeping, which is a Linux invention wearing the old
field's address, not a decode of the original.

## B. Semantic-RE dead ends (named function, meaning still guessed/unfound)

- **`FUN_4d56_417e` (Incite Indians) — two price-table terms.** Formula
  traced (`table[-0x69d6]*8 + (table[-0x6e7c]>>2&0xfe - 2*table[-0x69d6])`),
  and on 2026-08-14 both tables got *identified* (village-count-by-type,
  Σ combat-strength-by-type) — but this is a structural identification,
  not captured DOS values; `ai_contact_incite_price()` in `ai_contact.c`
  approximates both with `indian.tech` rather than the real per-type
  tables. **Caller never found** despite three static methods + two live
  DOSBox-X hang-dump captures (trampoline at resident `0x1261f` is
  RTLink's generic overlay dispatcher, not game logic — doesn't pin the
  trigger).
  - **Stale-doc mismatch found this pass**: `ai_contact.c:2930-2933`'s own
    comment still says "not wired in — 2 price-table values unnamed,
    caller unfound," but `AI_POPUP_TAG_CONTACT_INCITE` *is* wired
    (`ai_contact.c:1625,1803,1835,1872,1900,5722`). Comment is out of date
    relative to the code sitting right below it.

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
- `+0x40-0x43` ("`treaty_timer[4]`"): still unconfirmed either way.

---
Not included: `PARKED` markers (~90+ across `ai_diplo.c`/`ai_king.c`) —
those are known DOS mechanics with deferred *UI/dialog* ports, not
unresolved meaning. See `docs/ai_transcription.md` status tables instead.
