# First-colony DOS trace (#530)

Status: partial fix, 2026-09-27. `AI_SHIP_DOS=1` now passes TURN2_to_3;
TURN3_to_4 through TURN6_to_7 still fail. Keep the alternative disabled until
those transitions and the removal of the fitted opening pass.

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
