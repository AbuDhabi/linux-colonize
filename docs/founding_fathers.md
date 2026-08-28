# Founding Fathers — per-FF status

Owner doc for port_plan.md **P9**. One row per Founding Father: DOS effect
text, DOS FUN address(es), port symbol(s), and unit-test coverage.

**Authority order** (per fandom_col1994.md's own rubric): the **Effect**
column below quotes `COLONIZE/PEDIA.TXT` `@FATHER0`–`@FATHER24` verbatim —
this is a real shipped DOS asset (Tier 1), not wiki paraphrase.
`docs/fandom_col1994.md` was cross-checked against it: the two agree on
every Father, so fandom's summaries are safe to keep citing in code
comments. The `@FATHERS` type/weight table in `COLONIZE/NAMES.TXT` was
also diffed byte-for-byte against `founding_fathers.c`'s
`k_ff_type`/`k_ff_weight` — **exact match**, all 25 rows.

FF index order below is DOS/NAMES.TXT/`FF_*` enum order (0–24), grouped
into the 5 `@FATHERS` categories the game itself uses.

Election mechanics (Congress debate, century-weighted pick, bells
threshold `FUN_4345_0982`, apply dispatch `FUN_4345_0342`) are **Done** —
see `founding_fathers.c` header comment and `founding_fathers_tick`. Not
re-tabulated per-row here; only per-effect wiring is.

Legend: **Done** = ownership gate + effect wired at the real consumption
site. **Done (elect-only)** = one-shot mutation at election, correctly has
no ongoing gate (nothing else needs one). **Thin** = gate exists, effect
partial/stand-in. **PARK** = known DOS behavior not portable yet, explicit
comment naming why.

---

## Trade

| # | Father | PEDIA.TXT effect (Tier 1) | DOS FUN | Port symbol(s) | Test | Status |
|---|--------|---------------------------|---------|-----------------|------|--------|
| 0 | Adam Smith | "Allows factory level buildings... Factories allow the production of 1 and 1/2 units of manufactured goods for each unit of raw materials." | `FUN_4345_0342` apply; factory-tier math `FUN_15eb_1d4c` (`manufacturing_worker_calc_1d4c.md`) | Gate: `ColoniesBuildableOpts.has_adam_smith` (`colony.h`), set from `game_nation_has_ff(...,0)` (`game_loop.c` `game_colony_buildable_opts`) and `ai_euro.c` (3 call sites: 2118, 2429, 2603). Throughput: `colony_prod_manufacturing_output` (`colony_production.c` ~166, `out += out>>1` when `tier==FACTORY`) — factory tier is itself unreachable without the build gate, so the ×1.5 is correctly conditioned on ownership. | `test_founding_fathers.c` ~154 (elect, no gold invent); `test_ai_euro_expand.c` 16238/16963/17930/18040/18157/18272 (AI factory-build gating) | **Done** — both build-gate and the 1.5× throughput are wired, not just the gate. (P9.2's "verify wired in production, not just build gating" is resolved — confirmed done, this doc closes that question.) |
| 1 | Jakob Fugger | "When Fugger joins the Congress, all boycotts currently in effect are forgiven, without back taxes." | `FUN_4345_0342` apply | `founding_fathers.c` `apply_effect` case `FF_JAKOB_FUGGER`: `nat->boycott_bitmap = 0` + clears `col1->head.unknown46[2]` (king boycott-refuse byte) for the human. Elect-only — no ongoing gate needed (a *future* boycott after Fugger owned still boycotts normally; DOS doesn't grant immunity). | `test_founding_fathers.c` ~173-190, ~873-882 (AI elect path) | **Done (elect-only)** |
| 2 | Peter Minuit | "Once Peter Minuit joins the Continental Congress, the Indians no longer demand payment for their land." | `FUN_4cc6_07c2` (land-cost formula), FF index 2 zero-cost branch | `colony.c` ~533 `colonies_indian_land_purchase_gold`/land-purchase cost path: `founding_fathers_nation_has(col1, nation_id, FF_PETER_MINUIT)` → return 0. Ongoing gate (correct — applies to every future purchase, not just elect-time). | `test_founding_fathers.c` ~1478-1543; `test_ai_euro_expand.c` ~13170 | **Done** |
| 3 | Peter Stuyvesant | "Allows construction of the Custom House... which can streamline trade with Europe and allows European trade during the Revolution." | `FUN_4345_0342` apply (gate); `FUN_364b_0688`/`FUN_364b_0636` (Custom House EOT auto-sell + per-cargo denylist) | Gate: `ColoniesBuildableOpts.has_peter_stuyvesant` (`colony.h`), set from `game_nation_has_ff(...,3)` (`game_loop.c`) and `ai_euro.c` 1536/1547. Auto-sell: `europe_custom_house_autosell` (turn.c, thin structural per code comments — stock>99 leave 50 + denylist). Per-cargo UI chrome still PARK (P4.4 / P11). | `test_ai_euro_expand.c` ~15215-15283 (AI Custom-House prefer) | **Done** structural gate + autosell; per-cargo UI **PARK** (tracked P4.4, not this track) |
| 4 | Jan de Witt | "Trade with foreign colonies is allowed. In addition, your Foreign Affairs report becomes more revealing." | `FUN_4345_0342` apply (gate) | `founding_fathers_de_witt_allows_foreign_colony_trade` → `colonies_de_witt_transfer_*` (cargo transfer) + `ai_euro.c` wagon/ship trade-act gate; FA detail: `reports.c` peeks `head.founding_father[4]` directly for revealing strength numbers. | `test_founding_fathers.c` ~1786-1831 (both direct gate scenarios); `test_ai_euro_expand.c` ~2954-2956 | **Done** |

## Exploration

| # | Father | PEDIA.TXT effect (Tier 1) | DOS FUN | Port symbol(s) | Test | Status |
|---|--------|---------------------------|---------|-----------------|------|--------|
| 5 | Ferdinand Magellan | "The movement allowance of all naval vessels is increased by one, and the time to sail from the west map edge to Europe is shortened considerably." | `FUN_4345_0342` apply (elect bump); `FUN_1427_065a` (+3 moves = +1 MP on ship types 0xd..0x12); `FUN_48d3_0002` voyage roll via `291f_0aee` from `48d3_007a` (sail to Europe) and `48d3_0346` (sail from Europe) | Elect: `effect_magellan_sea_moves`. Ongoing +1: `turn.c` `turn_refresh_moves_for_nation`, `col1_bridge.c` save re-derive. **Voyage (2026-08-28):** `europe_voyage_turns_roll` — every crossing is 1 turn, or 2 when `RNG(1,100)>89 && ship_counts[nation]>2 && !Magellan`; used by `europe_enqueue_expected` / `europe_set_sail_from_harbor` (both directions) and the rare immigrant Merchantman in `turn.c` (which had the polarity inverted — Magellan *caused* the 2-turn delay — and gated on dock count instead of ship count). The invented 2-turn-east / 4-turn-west voyage is gone. DOS's `x<3` west-edge branch (`48d3:0089`) burns `RNG(0,1)` + an FF test and discards both (asm-confirmed: `mov ax,[bp-4]; leave; retf`), so there is no separate west-edge sail code — PEDIA's wording describes this same 10% delay Magellan removes. | `test_founding_fathers.c` ~592-622 (moves); `test_europe.c` voyage block (roll gates: none with Magellan, none with ≤2 ships, some without) | **Done** — +1 movement + voyage roll; "west-edge shortcut" resolved as the same roll, no PARK left |
| 6 | Francisco de Coronado | "When he joins the Congress, all existing colonies and the area around them become visible on the map." | `FUN_4345_0342` apply | `effect_coronado_reveal` (`founding_fathers.c`): `map_reveal_radius` around every owned colony, radius `FF_CORONADO_REVEAL_RADIUS=2`. Elect-only, correctly — no per-turn ongoing effect in DOS beyond the initial reveal. | `test_founding_fathers.c` ~399-409 (no-map fallback), ~447-589 (deep reveal + radius bound) | **Done (elect-only)** |
| 7 | Hernando de Soto | "Results of exploring Lost City Rumors are always positive, and all units have an extended sighting radius." | `FUN_4345_0342` apply (elect reveal); sight: `FUN_13f1_02f8` (radius) → `FUN_13f1_02b4` → `FUN_13f1_0158` (reveal loop); LCR: `FUN_65dd_0004` | **Sight (2026-08-28):** `units_sight_radius` = 1; Galleon/Privateer/Frigate 2; **de Soto: every non-ship unit 2**; Scouts +1 — straight from the `13f1:02f8` asm (`CODE_17` listing, Ghidra had dropped the `3960` result). `map_reveal_sight` mirrors `0158`: |dx|,|dy|<2 always, the outer ring only for tiles of the unit's domain (water for ships, same-continent land for land units). Wired at every unit reveal site (`turn_reveal_fog_for_nation`, game_loop load/after-move, LCR tail); colonies stay radius 2. LCR: `units_resolve_lcr_rumour` → `units_lcr_roll_outcome` = full `65dd` port (2026-08-27, `de_soto_reroll` = FF-7 loop at `65dd:00a6`). | `test_founding_fathers.c` (elect reveal, LCR); `test_units.c` LCR cases | **Done** — both halves; the old "full 65dd table PARK" is stale (ported 2026-08-27) |
| 8 | Henry Hudson | "Hudson increases the output of all Fur trappers by 100%." | `FUN_4345_0342` apply (gate); fur-trapper yield doubling in colony harvest | `colony_preview.c` ~135 and `turn.c` ~634 and `colony_screen.c` ~1626/3668: `if (yld>0 && field_job==FUR_TRAPPER && has(HUDSON)) yld *= 2;` — 4 call sites, all consistent. | `test_founding_fathers.c` ~641 (deep hook); `test_turn.c` ~1858 | **Done** |
| 9 | Sieur De La Salle | "La Salle gives all existing and future colonies a stockade when the population of the colony reaches 3." | `FUN_4345_0342` apply | `effect_la_salle_stockades` (`founding_fathers.c`): elect sweep grants Stockade to every owned colony at pop≥3 (`FF_LA_SALLE_STOCKADE_POP=3`). **2026-08-26:** also re-swept every `founding_fathers_tick` while owned (same shape as Las Casas), so a colony founded after election, or one that grows into pop 3 later, gets the free Stockade too. **2026-08-26 (same day, follow-up):** the tick-only version still read as "next turn" from the player's seat — user confirmed DOS shows it the instant the colony hits pop 3, no wait. Added `founding_fathers_la_salle_check` (public one-colony-pool sweep, gated on ownership) called directly from `colonies_admit_unit` (`colony.c`, all 10 call sites in `ai.c`/`ai_euro.c`/`game_loop.c` now pass `col1`) right after the population bump, so walking a colonist into a colony grants the Stockade synchronously, same input. EOT-driven growth (the food-surplus birth in `turn_produce_one_colony`) was already effectively instant — it and `founding_fathers_tick` both run inside the same `turn_processor_start` pass, before the player ever sees the next turn, so no separate fix was needed there. | `test_founding_fathers.c` ("La Salle ownership tick", "La Salle immediate grant") | **Done** — elect sweep + per-turn ownership tick + synchronous admit-time grant all wired |

## Military

| # | Father | PEDIA.TXT effect (Tier 1) | DOS FUN | Port symbol(s) | Test | Status |
|---|--------|---------------------------|---------|-----------------|------|--------|
| 10 | Hernan Cortes | "Conquered native settlements always yield treasure, in greater abundance, and the king's galleons transport the treasure free of charge." | `FUN_4345_0342` apply (gate); `FUN_5fef_31ea` (post-win Indian settlement fallout: treasure spawn) | `founding_fathers_cortes_guarantees_conquest_treasure` + `units_cortes_conquest_treasure_gold` (`FUN_5fef_31ea` peel) + `units_spawn_treasure_train`, wired from `units_resolve_land_combat_ff` via fallout context (`units_set_native_fallout_context`). Free transport: `founding_fathers_cortes_free_king_galleon` → `units_cortes_cash_coastal_treasures` (tax-share-only `@KINGGALLEON3` path). Non-Cortes `@KINGGALLEON2` share: resolved 2026-08-27 — `FUN_5fef_1908` (`units_king_galleon_offer_coastal_treasures`, share `max((difficulty+10)*5, 2*tax)` cap 90), no longer PARK. | `test_founding_fathers.c` ~1732-1734, ~1991-1993; `test_ai_euro_expand.c` ~9935-9937; `test_units.c` ~2707-2709 | **Done** for conquest-treasure guarantee + free transport; `KINGGALLEON2` non-Cortes share also **Done** (P7.4, 2026-08-27) |
| 11 | George Washington | "Every non-veteran soldier or dragoon who wins a combat is automatically upgraded." | `FUN_4345_0342` apply (gate); promote-on-win in combat resolve | `units_washington_promote_on_win` (`units.c` ~1382), called unconditionally instead of the chance-based `units_chance_promote_on_win` (~1552) when owned — `if (has(WASHINGTON)) return 0;` short-circuits the RNG path so Washington's promote is *always*, not chance. Wired from `units_resolve_land_combat_ff` (AI/king path passes `g_units_ff_col1` set by `units_set_ff_col1`). | `test_founding_fathers.c` ~891-899 (ownership bit set alongside Drake/Revere) | **Done** |
| 12 | Paul Revere | "When a colony with no standing soldiers is attacked, a colonist automatically takes up any stockpiled muskets in defense of the colony." | `FUN_4345_0342` apply (gate); auto-arm on attack | `founding_fathers_revere_should_auto_arm` (gate: no soldier defender + `muskets_stock >= UNITS_EQUIP_MUSKETS`) + `founding_fathers_revere_auto_arm` (eject first active colonist as Soldier). Wired from `units_try_move` when the attacker steps onto an empty foreign colony tile, FF col1 context set via `turn_refresh_moves_for_nation` → `units_set_ff_col1`. | `test_founding_fathers.c` ~891-899 | **Done** |
| 13 | Francis Drake | "The combat strengths of all your privateers are increased by 50%." | `FUN_4345_0342` apply (gate); Privateer strength term | `combat_strength.c` ~171 (`local_4 += local_4>>1` when `combat_type_is_privateer` + owned) — this is the **live combat-strength** path. `units.c` ~1961 (`units_apply_drake_privateer_bonus`, `strength*3/2`) is a **second**, narrower helper gated the same way — confirm both are reached on the same code path or one is dead (see Open items). | `test_ai_euro_war.c` ~3046-3048; `test_founding_fathers.c` ~891-899 | **Done** (two call sites doing the same ×1.5 — worth a follow-up dedup pass, not a correctness bug) |
| 14 | John Paul Jones | "A Frigate is added to your colonial navy, without cost." | `FUN_4345_0342` apply | `effect_jones_frigate` (`founding_fathers.c`): spawns Frigate (fallback Man-O-War if type missing) at nearest owned coastal water tile or an existing ship's location; nation-stamped. Elect-only, correct (one-shot grant). | `test_founding_fathers.c` ~413-423 (no-fallback-gold check), ~668-687 (deep spawn) | **Done (elect-only)** — player-nation only per port_plan P9.2 note (`effect_jones_frigate` runs for whichever nation elects, not player-restricted in code — see Open items) |

## Political

| # | Father | PEDIA.TXT effect (Tier 1) | DOS FUN | Port symbol(s) | Test | Status |
|---|--------|---------------------------|---------|-----------------|------|--------|
| 15 | Thomas Jefferson | "Jefferson's presence in the Congress increases Liberty Bell production of statesmen by 50%." | `FUN_4345_0342` apply (gate); `FUN_15eb_1d4c` Statesman body | `colony_prod_colony_bells_ff` (`colony_production.c` ~415, `colony_preview.c` ~245, `turn.c` ~1791, `reports.c` ~1795): `statesmen_pct = has(JEFFERSON) ? 50 : 0`, threaded into `colony_prod_carpenter_preacher_shape`. | `test_founding_fathers.c` ~272, ~1403-1435; `test_turn.c` ~2736-2743 | **Done** |
| 16 | Pocahontas | "All tension levels between you and the natives are reduced to content, and all Indian alarm is generated half as fast." | `FUN_4345_0342` apply (elect reset); ongoing half-rate | `effect_pocahontas_reset_alarm` (elect: zero `tribe.alarm[nation].friction/attacks` + `indian.alarm_by_player[nation]`). Ongoing half-rate: `ai_contact_alarm_bump_amount` (`ai_contact.c` ~1083), gated on ownership for encroachment/prelude/raid bumps. | `test_ai_contact.c` (11 scenarios listed, ~525-4389) | **Done** — both elect reset and ongoing half-rate wired |
| 17 | Thomas Paine | "Liberty Bell production in all colonies is increased by value of the current tax rate." | `FUN_4345_0342` apply (gate); `FUN_15eb_1d4c`/`1f72` | Same `colony_prod_colony_bells_ff` call sites as Jefferson: `paine_tax_pct = has(PAINE) ? nation.tax_rate : 0`. | `test_founding_fathers.c` ~1439-1441; `test_turn.c` | **Done** |
| 18 | Simon Bolivar | "Sons of Liberty membership in all your colonies is increased by 20%." | `FUN_15eb_0274` (SoL display read, +20 when human+owned) | `founding_fathers_bolivar_sol_bonus` — **display-time only**, no storage bump (matches DOS: `FUN_15eb_0274` re-adds +20 on every read, doesn't mutate stored SoL). Consumed wherever colony SoL % is displayed/used. | `test_colonies.c` ~1229-1236 | **Done** |
| 19 | Benjamin Franklin | "The King's European Wars have no further effect on the relations between the powers in the New World, and Europeans in the New World always offer peace in negotiations." | `FUN_4345_0342` apply (elect make-peace); ongoing gate | Elect: `effect_franklin_nw_peace` (make_peace with all 4 Euro peers if at war). Ongoing: `founding_fathers_franklin_keeps_nw_peace` → `ai_diplo` declare/`euro_balance`/war-hit side effects. FA `3f41` UI display of the effect **PARK**. | `test_ai_diplo.c` ~3307-3445 | **Done** for the mechanic; FA UI chrome **PARK** (P11 territory) |
| 20 | William Brewster | "No more criminals or servants appear on the docks, and you select which immigrant in the Recruitment Pool will move to the docks." | `FUN_4345_0342` apply (gate); arrival: `FUN_38fd_5e52` ~68577 FF-0x14 test → `FUN_38fd_4884(0,1)` (mode 4 = `@RECRUITCHOOSE`, passage 0, human picks / AI takes slot 1) | `effect_brewster_filter_pool` (no criminals/servants; the immigration tick now also latches `brewster_no_criminals` from the FF bit so a loaded save filters too). **Pick (2026-08-28):** `europe_tick_immigration_pressure` returns 2 when Brewster is owned (no random pick, crosses kept), `turn.c` enqueues `AI_POPUP_TAG_BREWSTER_PICK` (`units_brewster_enqueue_pick`, `@RECRUITCHOOSE` + 3 pool names), `game_loop` applies via `units_brewster_apply_popup` → `europe_brewster_pick_from_pool` (free dock transfer, crosses zeroed, no +6 recruit bump — `4884` tail with `param_1==0, param_2!=0`) + Europe-map mirror unit. Cancel keeps crosses so DOS re-asks next turn. | `test_turn.c` "brewster pick-among-pool" (tick→2, cancel keeps, pick docks + zeroes) | **Done** — both PEDIA halves |

## Religious

| # | Father | PEDIA.TXT effect (Tier 1) | DOS FUN | Port symbol(s) | Test | Status |
|---|--------|---------------------------|---------|-----------------|------|--------|
| 21 | William Penn | "With Penn, cross production in all colonies is increased by 50%." | `FUN_4345_0342` apply (gate); `FUN_15eb_1d4c` Preacher body (flag 0x15=21=FF_WILLIAM_PENN, confirmed via decomp table match) | `colony_prod_carpenter_preacher_shape` (`colony_production.c` ~124-135): `nation_has_penn` param, Preacher-only (Carpenter always passes false), `v += v>>1` **after** the Cathedral ×2 fold (stacks multiplicatively, confirmed asm-exact, not the port's old flat ×1.5-on-total bug which this file's own history notes as fixed). | `test_founding_fathers.c` ~1445-1447 | **Done** |
| 22 | Jean de Brebeuf | "With de Brebeuf in the Congress, all missionaries function as experts." | `FUN_4345_0342` apply (gate) | `founding_fathers_brebeuf_missionaries_are_experts` → `ai_contact.c` mid-band convert pulse: plain Missionary treated as Jesuit-grade (`@JOB24`). Ownership bit only, no elect-time crosses fiction (correct — DOS effect is purely a lookup-time reclassification). | `test_ai_contact.c` ~936-970 | **Done** |
| 23 | Juan de Sepulveda | "His presence in the Congress increases the chance that subjugated Indians will 'convert' and join a colony." | `FUN_4345_0342` apply (gate); `FUN_5fef_31ea` convert-join threshold | `founding_fathers_sepulveda_convert_join_bonus` → `units_try_native_settlement_fallout` / `units.c` ~2163 (`thr += 4` when owned, on top of the base `thr` and the separate Spanish-nation-id `+4`). | `test_units.c` ~2923-2925 | **Done** |
| 24 | Bartolome de las Casas | "With Las Casas, all currently existing Indian converts are assimilated into the colonies as free colonists." | `FUN_4345_0342` apply (elect + ongoing) | `effect_las_casas_assimilate` (Convert→Free Colonist profession swap on owned colony colonists + map units), called both at elect **and** every `founding_fathers_tick` while owned (catches late converts — the only Father with an explicit re-tick, per its own code comment: "PEDIA elect is one-shot; tick catches late joins"). A **second** effect not mentioned in the header doc comment: `units.c` ~2163 also lowers the Sepulveda-style convert-join threshold by `-4` when Las Casas is owned (fewer *new* subjugated-convert spawns, since existing ones assimilate instead) — worth cross-referencing in the header comment. | `test_founding_fathers.c` ~1605-1680 | **Done** — two effect sites, both consistent with PEDIA intent |

---

## Open items found while writing this table (not fixed — P9.1 is static RE + doc only)

These are candidates for **P9.2** (a separate track) or a documentation
follow-up; none were touched here.

1. **Magellan is wired in 3 places, not "turn.c only"** as port_plan.md's
   P9 "Now" summary states: `founding_fathers.c` (elect one-shot bump),
   `turn.c` `turn_refresh_moves_for_nation` (ongoing per-turn +1), **and**
   `col1_bridge.c` ~991 (Col1 save-load path re-derives ship `moves_left`
   from raw `moves` with the same +1). All three are consistent with each
   other; this is a doc-accuracy note, not a bug.
2. **Fixed 2026-08-26.** La Salle now re-sweeps every `founding_fathers_tick`
   while owned (added alongside the existing Las Casas re-tick), so future
   colonies and colonies growing into pop 3 later also get the free
   Stockade — was previously elect-time-only.
3. **Checked 2026-08-26 — not a dedup candidate after all, confirmed both
   needed.** `combat_strength.c` ~171 (`combat_unit_base_x8`) is the
   general ship-to-ship engine: raw defense → ×8 scale → veteran → Drake
   → holds penalty, used for regular naval combat. `units.c` ~1948
   (`units_drake_scale_strength`), called once from `units_fort_vs_ship`
   (~3274), is a **separate, self-contained coastal-fort-fire formula**
   that never routes through `combat_unit_base_x8` at all — it scales the
   bare type `defense` value directly, no ×8/veteran step. Different
   combat path, different formula shape, same Drake ×1.5 semantics
   applied independently to each — not duplicate code, no merge needed.
4. **Confirmed 2026-08-26 — not a bug, matches DOS.** Read
   `FUN_4345_0342` (`viceroy_unpacked.c` ~73044, `param_2==0xe` branch,
   the FF-apply dispatch's Jones case) directly: it calls
   `FUN_281f_095c(0x11, param_1, …)` (spawn Frigate, type `0x11`) with no
   `param_1==0`/human-only guard anywhere in that branch or the
   surrounding function — same shared dispatch every other FF case uses,
   unconditional on which nation is electing. DOS itself grants Jones's
   free Frigate to whichever nation (human or AI) elects him; the port's
   `effect_jones_frigate` matches this exactly. P9.2's "for the player"
   task wording was simply imprecise, not a spec the code was failing to
   meet — no fix needed.
5. **Fixed 2026-08-28.** Brewster's pick-among-pool was ported from the
   `5e52` Brewster branch (`FUN_38fd_4884(0,1)` / `@RECRUITCHOOSE`) — see
   row 20. Same pass: de Soto's ongoing sight radius (`13f1_02f8`),
   Magellan's voyage roll (`48d3_0002`, plus a polarity bug in the
   immigrant-ship landfall) — rows 5 and 7.
6. **Stuyvesant's Custom House per-cargo sell-configuration UI** is PARK
   (P4.4) — auto-sell itself (stock>99→leave 50 + denylist) is wired.

## Cross-check against port_plan.md P9's own claims

- **Wired list** (Bolivar, Brebeuf, Brewster, Cortes, de Soto, de Witt,
  Drake, Franklin, Hudson, Jefferson, Paine, Penn, Pocahontas, Revere,
  Sepulveda, Washington, Las Casas) — **verified accurate** against code;
  all 17 have a real gate + effect site as tabulated above (de Soto and
  Brewster are correctly "wired" but thin/partial, not fully DOS-complete
  — see their rows).
- **"Not found outside core files" list** (Fugger, Coronado, La Salle,
  Magellan (turn.c only), Jones (ai_ only)) — **partially confirmed,
  partially corrected**:
  - (2026-08-28: de Soto and Magellan now also have `FF_*` references
    outside the core files — `units_sight_radius` in `units.c`,
    `game_voyage_turns` in `game_loop.c` / `turn.c`.)
  - Fugger, Coronado, La Salle, Jones: confirmed — `grep` for their
    `FF_*` symbols outside `founding_fathers.c`/`founding_fathers.h`
    returns nothing (Jones has no `ai_*` reference at all, correcting
    the parenthetical "ai_ only" — there is no such reference anywhere
    outside `founding_fathers.c`). This is expected, not a gap: all four
    are one-shot elect-time effects with no ongoing per-turn gate needed.
  - Magellan: **not** "turn.c only" — see Open item 1 above.

## Method notes

- `docs/fandom_col1994.md` (Tier 3) was **not** used as the sourcing
  authority here — `COLONIZE/PEDIA.TXT` (Tier 1, shipped DOS asset) was
  read directly for every Father's effect text. fandom's paraphrases were
  cross-checked against it and found accurate in every case, so existing
  code comments citing fandom remain trustworthy.
- `COLONIZE/PEDIA.TXT` is byte-flagged as binary by `grep` on this system
  (ISO-8859 CRLF content trips grep's binary heuristic) — use `grep -a` or
  `sed`/`Read`, not a plain `grep`, or matches silently return nothing.
- `COLONIZE/NAMES.TXT` `@FATHERS` (type + 3 century-weight columns) was
  diffed against `founding_fathers.c`'s `k_ff_type`/`k_ff_weight` tables —
  exact match, confirms the election-weight port is byte-faithful.
