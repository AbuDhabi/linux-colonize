# First-colony DOS trace (#530)

Status 2026-10-04: the `AI_SHIP_DOS` switch is gone; the DOS hull/land path
is the only path. The whole fitted first-colony layer (#530 S5, #1035, both
#971 arms, the seed-100 landfall table, ship FOUND producer, legacy
`ai_euro_act_ship` band, dispatcher 0-MP carve-out) is deleted. The last arm
(#971 Pioneer re-aim to (47,40)) was standing in for the 20e6 explore ring:
DOS scores only coastal tiles there (the best-site compare, raw 89257, sits
inside the coastal branch), the port let inland tiles win with nib 0 and so
never committed. With that fixed the French pioneer's TURN4->5 move is DOS's
own (order 0x0b, plan '2', goto (47,40)). No fitted opening code remains.
ctest 96/96, golden_ai_turns 6/6, `make golden` clean.
`AI_SHIP_DOS=1` in the sections below is historical; drop it from commands.

## Evidence and reproduction

Live DOSBox-X, private copy of `COLONIZE/VR_SEED.EXE` named VICEROY.EXE,
`test-saves-ai/TURN2.SAV` copied to COLONY00.SAV. Load that save, then press
Space to finish the active human ship's move and advance the turn. Enter
alone with an active ship did not advance it. No RNG or instruction patches.

Headless debugger: SDL_VIDEODRIVER=dummy, SDL_AUDIODRIVER=dummy, TERM=dumb,
pexpect terminal 24x80. Launch `debugbox viceroy.exe`; debugger command `RUN`
continues, `BPINT 16` catches keyboard polling. Keyboard buffer injection at
0040:001a sets head=001e, tail=001e+byte count, followed by ASCII/scancode
pairs (Space=20 39, Enter=0d 1c). Do not queue repeated RUN commands when
collecting a breakpoint: they can skip the state being measured.

For this launch DS=SS=237e, resident nation-turn thunk=2043:0638, loaded
521d overlay=D39E. Discover the latter from the far-jump operand at
2043:063d; relocation addresses are session-specific. Break at overlay
20ea (after ENTER in 20e6) and 3afd (after unload-mask scan). Dump DS:0,
10000 bytes with MEMDUMPBIN. Unit slot is word SS:[BP+6]; only at 3afd
are the mask word [BP-9a], pioneer count [BP-48], military count [BP-46],
and passenger count [BP-a6] valid. Unit records start DS:3144, stride 1c;
fields +0/+1=xy, +5=spent MP, +8=order, +9/+a=goto, +b=facing.
Do not read a mask from the entry breakpoint's uninitialized locals.

Observed Spanish ship (DOS slot 6; zero-based), all mask rows have one
pioneer, one military passenger, two total:

| Stop | xy | order | goto | facing | spent | mask |
|---|---|---|---|---|---|---|
| 3afd, before movement | 50,53 | 0 | 4,13 | 0 | 0 | 0000 |
| 3afd, after NW step | 49,52 | 0c | 49,52 | 7 | 3 | 0000 |
| 3afd, after arrival query | 49,52 | 0 | 49,52 | ff | 3 | 0000 |
| 3afd, after SW step | 48,53 | 0c | 48,53 | 5 | 6 | 0050 |
| next nation entry | 48,53 | 0 | 48,53 | ff | 12 | — |

This disproves the older #530 hypothesis that Spain takes W,W and must
suppress unloading at (49,53): DOS never visits that tile. The actual cause
was retained facing after an already-arrived path query. FUN_6662_0f74
LAB_1599 (raw 104758) writes ff even when returning no direction. Correcting
that state also fixes France's landing without the previously proposed
three-draw RNG adjustment.

At Spanish nation entry, France's soldier (slot 5) is already ashore at
(50,38), order **1**, goto (56,42), spent 3. The final TURN3 save has order
0: that change happens after the French unload. Native steps bypassed the
port's sentry wake scan (FUN_465b_0000 -> FUN_5bfb_3180 raw 98628-98646).
Adding the scan wakes the French soldier and Spanish landers. Automatic
wake must preserve their goto bytes: DOS writes only +314c, whereas the
port's explicit `units_wake` path also clears the destination.

## Mechanisms corrected

- Unload mask resets on each empty/own shore **before** stance/latitude
  rejection (20e6 raw 89470). Only order 0b enables all-cargo goto-continent
  unloading; order 0c does not.
- Hull goal walk records the pathfinder's returned facing, including ff
  on arrival (6662:1599).
- 5b66 dispatches the resulting order even if unloading exhausted MP
  (overlay asm 139925, 5bda..5c0e). This clears Spain's final step order.
- Native committed movement runs the shared sentry wake scan; automatic
  wake preserves saved goto coordinates.

Focused proof: `make test T=unit_regress_ai_tables`, and
`AI_SHIP_DOS=1 COLONIZE_TEST_ONLY=TURN2_to_3 ./build/debug/golden_ai_turns`.
No fixture coordinates, RNG burns, or expected saves were changed.


## TURN3 to TURN4 (2026-09-28)

Loaded TURN3 through the same debugger setup. At France's first hull 3afd
breakpoint, DS:173e=fff6, DS:173c=0000, and the unload mask is 0040. The
port's 0a60 producers already compute exactly fff6 but discarded it;
20e6 reconstructed the mask from primary-goal coordinates. Those are ocean
tiles, whose continent is not the land continent recorded by the producer.
Persisting the real per-nation masks fixes France's delayed unload.

Observed nation-entry / subsequent 20e6 entries, zero-based DOS slots:

| Unit | Before act | After act |
|---|---|---|
| French soldier 5 | (50,38), order 0, spent 0 | (50,37), order 0, goto (50,37), facing 0, spent 3 |
| French pioneer 4 | aboard (51,39), order 1, spent 0 | still aboard, order 1, spent 3 |
| French hull 3 | (51,39), order 0, facing 6, spent 0 | same xy, order 0c, goto (50,39), facing 6, spent 15 |
| French pioneer after hull | aboard (51,39) | (50,38), order 1, goto (56,42), spent 3 |
| Spanish soldier 8 | (47,54), order 0, spent 0 | (46,54), order 0c, goto (46,54), facing 6, spent 9 |
| Spanish pioneer 7 | (47,53), order 0, spent 0 | (46,52), order 0b, plan 32, goto (45,52), facing 7, spent 6 |
| Dutch soldier 11 | (48,14), order 0, spent 0 | (49,14), order 0, goto (49,14), spent 3 |
| Dutch pioneer 10 | (49,14), order 0, spent 0 | founds the colony on this turn |

Further corrections under the optional DOS path:

- Bypass the fitted first-colony land approach and local-settle override;
  use the existing 2912 founding-site scan and 5b66/479b goal walker.
- Restore an overnight Sentry unit's parked allotment when 0a60 clears
  its order. DOS refreshes every spent byte at day top (raw 6357); the port
  parks sleeping units at zero remaining MP. Preserve the saved goto.
- The scan binds order 0b, plan 32 (overlay 2e3a..2e46 -> 20c6). It must not
  call the port movement scorer before the actual pathfinder: that extra
  query consumes RNG and changes subsequent ship directions.
- The village penalty reads native nation **tech**, not village population:
  raw 89200 dereferences DS:8d4e+2; FUN_15dc_0006 (raw 9229) binds 8d4e to
  the Indian nation record. This corrects Spain's (45,51) vs (45,52) target.

Verification: 95/95 ctest; default six-turn golden passes; `make golden`
now also runs DOS TURN2_to_3 and TURN3_to_4. The play smoke now waits for
human-owned colonies: the previous all-nation count mistook an early Dutch
colony for the player's and failed its automatic colony-screen assertion.

Remaining measured differences start at TURN4_to_5: Spanish soldier
(46,53) instead of (46,55), Dutch hull (43,21) instead of (39,18), plus one
native position. Later transitions still fail. A separate static lead:
`ai_euro_land_explore_scan_target` recomputes the explorer flag/counter
already evaluated by its caller; removing that duplicate alone did not
change the six-turn outcomes, so it was left for a separate focused proof.

## TURN4–7 follow-up (2026-09-28)

Live DOS TURN4 proves the Spanish soldier's facing must come from saved
unit +0x314f, not the zero-initialized per-unit scratch mirror. Reading the
real field corrects its final (46,55), including the subsequent native wake.

Dutch TURN4 exposed two independent defects:

- DOS enters the first hull act with RNG 00000064; the port entered with
  016d2d97. A debugger backtrace located the extra draw in the 5d04 artillery
  check (raw 92569–92578). DOS absorbs the colony's soldier before planning,
  adding 50 muskets; the port planned with stock zero. Under DOS mode the
  5952 counters/origin binding, threat/flags/absorption, tools/improvement,
  placement, build cascade, and specialist arms now run together before
  inventory and planning. This is the #964 prerequisite extraction, not a
  hoist of the old placement-only call. Default phase order is retained.
- FUN_1427_09dc's two presence probes were reversed in the ship helper:
  137f_03e4 reads settlement bit 02; 137f_0314 reads unit bit 01. Outside a
  settlement, a ship ignores neighbouring land units. Coastal Braves wrongly
  interrupted order-0c arrival cleanup, retaining facing instead of ff.

DOS Dutch hull trace: (43,16) -> (42,17) -> (41,18) -> (41,19) ->
(40,18) -> (39,18). Between steps, order 0c is queried once at its arrived
coordinate, becoming order 0 with facing ff. After the first three moves
RNG states are 06a641ac, b2e257f4, bdc2652d; after the fourth, 2c9146c0.

TURN5 French pioneer enters 20e6 at (48,39), RNG c1408068, and returns to
its colony (50,37), order 0, spent 3. The patrol arm's LAB_27f5 -> 20c6
bind is order 0b, not the port's 0c. Its partial-MP movement invokes the
465b reseed (raw 75649): the following ship enters with RNG 016d2d97.
The shared 479b walker now supplies `ai_turn_seed` to movement. Both world
constructors also initialize all members: newly added reseed fields were
otherwise indeterminate, causing inconsistent repeated runs.

TURN6 French ship remains (52,43), order 0b, goal (50,37). The 4393 haul
pick goes through LAB_4567 -> 27f5 (raw 89927–89929): it targets the colony,
not the port helper's neighbouring water tile (51,38).

Mutation proof: individually reverting saved facing, colony phase order, the
coastal-unit probe, patrol goal order, partial-MP reseed, or haul destination
makes its focused regression fail; all mutations were restored.

Focused proof: arrival cleanup beside a coastal Brave, a partial-MP goal
arrival after prior planning draws, and all six unchanged DOS opening
fixtures. The joint golden target additionally guards TURN4_to_5,
TURN5_to_6 and TURN6_to_7. These fixtures compare their declared fields;
they do not prove byte-for-byte equality of all saved state.

### Follow-up — 2026-10-03

The broader DOS path now retains the deferred Treasure/Wagon/Missionary
handlers. The full 5952 colony tick has its own entry point, preserving the
placement-only seam. Europe dock demand is wired before delivery/haul;
its fixtures now carry a consistent census and no unintended export surplus.
Haul assertions distinguish DOS colony-coordinate goals from legacy water goals.

Further source-backed corrections:

- Land and ship callers now share the corrected FUN_1427_09dc settlement/unit
  probes and terrain-domain comparison (raw 7927–7968). A regression covers
  a coastal foreign unit, a land unit across different continent labels, and
  a coastal settlement. The old land implementation fails it.
- FUN_479b_0972 arrival clears a pioneer's hop countdown and slot
  (raw 77099–77103). The partial-MP arrival regression now seeds both latches
  and checks their reset as well as the RNG reseed; it failed before the fix.
- FUN_1427_10be assembles passengers sharing the ship's coordinate bucket.
  The handler called `units_board`, whose adjacency check rejects identical
  coordinates. It now uses capacity-checked `units_board_stacked` for that
  case. Boarding tests put the hull on the colony tile and inspect the berth
  band directly, including a repeated act with an already full hull.
- The ship LAB_5a78 tail now reads PEACE (bit 0x40), matching raw 90406–90420
  and the land tail. The old code read WAR. A border-hull regression covers
  peace, war, contact-only, and zero relations.

The former boarding assertions inferred boarding from end-of-turn positions.
Tracing showed both passengers boarded, then disembarked at their original
colony when the hull entered it: those final positions cannot prove boarding.
The direct berth assertions require both passengers aboard instead. Likewise,
the sale-ledger test now isolates the trade band: a treasury watchpoint showed
two legitimate 140-gold recruitment debits before the correct 200-gold sale.
Its exact sale credit and double-book ledger assertions are unchanged.

Validation: default ctest **96/96**, joint golden target passes, DOS opening
fixtures unchanged. DOS-mode ctest **88/96**. Logic-map check: 13 graphs,
179 nodes, 230 edges, no errors or warnings.

### Remaining default-switch blockers

`AI_SHIP_DOS=1 ctest --preset debug --output-on-failure` still fails:
`unit_ai`, `unit_ai_euro_expand_purchase`, `unit_ai_euro_expand_build`,
`unit_ai_euro_expand_settle`, `unit_ai_euro_war_land`,
`unit_ai_euro_war_transport`, `unit_ai_euro_20e6`, `unit_ai_euro_5d04_hire`.

These cover opening expectations, construction/recruitment, land combat,
settlement, and transport landing positions. Separate actual DOS omissions
from assertions inherited from the fitted path before enabling the switch.
Do not selectively enable DOS behavior for fixture nations, coordinates,
or early turns. The duplicate explorer-counter evaluation noted above also
remains. #530 stays OPEN and `AI_SHIP_DOS` remains opt-in.

## Fresh-map founding (2026-10-03b)

The opening goldens all start from DOS saves, so they never exercised an
OpenCol-generated new game. A headless new-game driver (new game on seeds
5/77/4242/9001/31337/100, then `ai_replay`) showed the DOS path founding
**zero** colonies on every port-made map, while it founded on DOS TURN1.SAV
of the very same seed-100 map. Three start-state defects, all fixed:

- **Site-score nibble.** FUN_682a_000c (raw 105147, called once by the
  new-game bootstrap FUN_75c2_235c raw 121595) writes the colony-site score
  into the low nibble of the seen plane (DS:0x168). `map_gen` never did, so
  the 20e6 2912 ring scan (`best_nib > 0`) found nothing once tiles were
  seen. Ported as `ai_goals_write_site_scores` (asm-read; ring tables DS:0xc8
  / 0xde, site column DS:0x2f79, @RESOURCE column DS:0x97b2, FUN_137f_000a
  inset bounds, specials scored as before villages exist). Byte-exact against
  all 4176 tiles of SEED100.SAV / TURN1.SAV; `golden_mapgen_seed100` now
  checks it. Old port saves (no nibble anywhere) are scored once on load
  (`ai_goals_repair_site_scores`).
- **Nation landfall bytes.** Nation +0x32/+0x33 (-0x77c6/-0x77c5) were never
  written for AI nations, so every Europe sailing (`ai_euro_europe.c`) aimed
  at (0,0) and parked fleets on the west map edge. `ai_init_new_game` stamps
  them; loaded records still at (0,0) get the High Seas tile nearest the
  nation's first colony or unit (`ai_repair_nation_landfalls`, port repair).
- **Starter hull order byte.** FUN_75c2_235c raw 121624 writes orders 0, not
  GOTO; only the goto bytes carry the landfall.

Result: the DOS path founds a first colony for every AI nation on every map
tried; on the live campaign4 save (turn 377) England goes from 0 to 3
colonies within 60 turns. Remaining weakness is economic, not founding: by
turn 150 AI nations hold 1-3 colonies with few colonists, buying mostly
military while gold stays near zero.

The remaining DOS-mode test failures were fixtures written against the old
phase order or the fitted path (all fixture-only, no src change): 5952-tick
flag rebuild ahead of 5d04 (ocean ring), turn%8 lumber buy (lumber stock),
asm-22da Docks arm (landlocked colony), founding stimulus = act state 7,
unnamed-kind expert types (kind_plus1), 06ae landing tile within 2 of the
colony, and Europe-lane hulls in the AMERICA distance sum. With those, the
switch was flipped on by default.
