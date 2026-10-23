# Second Americas idle campaign fix log (2026-10-21 .. 2026-10-23)

STATUS: archived 2026-10-23, when the campaign completed (saves 1 -> 12 byte-identical, continuous
and with a reload before every turn). Living doc: [docs/idle_america2.md](../idle_america2.md). This
is the triage and divergence log split out of it, verbatim; grep by `FUN_` name, save or resync pair
number, do not read whole. Saves 000014..000406 were removed in commit d87711a1 and 000013 on
2026-10-23 (all are in git history at `d87711a1^`), so "save N" and "resync N" rows past 12 cannot be
re-run against the checked-in archive; the DOS behaviour they record stands. The e7c4 oracle
(`GOLDEN_IDLE_E7C4_ORACLE`, `GOLDEN_IDLE_E7C4_FORCE`, `k_america2_gap_parks`, the
`ai_euro_e7c4_oracle` hook in `src/core/ai_euro_land.c`) was deleted with the trim; it is in
`tests/golden/test_idle_campaign.c` at commit 38359f3f.

## The e7c4 oracle (triage only)

The DB-4 border park (FUN_521d_20e6's 0x46 arm reading the uninitialised SS:e7c4) decides park or
wander for a combat land unit alone beside a foreign colony. In this archive it first changes the
outcome at save 13 (England's Dragoon beside Quebec parks in DOS), and dozens of later pairs carry
it. `GOLDEN_IDLE_E7C4_ORACLE=1` makes that gate (and only that gate, after its three pure tests
pass) follow the archive: park when the next save shows a unit of that nation on that tile with
plan 0x46 and no orders (`ai_euro_e7c4_oracle`, `tests/golden/test_idle_campaign.c`). It keeps the
residue from hiding real bugs; it is not a gate.

How often it matters (resync pass with the oracle, 2026-10-22): the gate is reached 22544 times in
405 turns (mostly the same camped units every turn). The modelled deterministic writers predict
the archive in 97.4%: 99% when one of them ran earlier in that nation's turn, 81% when none did
(port value 0, DOS parks 19% of those). The 587 misses fall in 232 turns with no period; a
nation's first gate hit of the turn misses 10.5%, later ones 1.7%.

A DOSBox replay of the archive is no oracle either: a continuous DOSBox session from a load of
000001 already parks a different Dragoon at save 2 (overlay history differs from the recording's
session).

## Restored-load DOSBox traces

A from-load DOSBox session starts with the unsaved tables at zero (stance DS:0x9870, goals
DS:0x98b0 + nation*0x100, rank DS:0x917c), so its first turn is not the archive's: from-load 17
registers New Amsterdam on a zero stance and sends an empty Merchantman on a work-queue haul the
archive never made. Write the port's tables (the RTST chunk of a resync start) back after
`load_slot` before tracing.

## Fixed so far (2026-10-21)

- Save 16: FUN_1427_06b4 stamps `+0x3159 = 100` on every new Pioneer, also in a slot no
  compaction reached; the export no longer zeroes a non-Pioneer's hold[5] <= 100 (DOS keeps the
  byte a unit created over a compaction copy inherits).
- Resync 76, 90, 106, 130 class: units off the map export their live visibility nibble. 007a's
  08da (FUN_1427_0968) clears it for a hull sailing to Europe and its passengers; 06b4 sets none
  off the map; a hull 5fef_0352 parked for repair keeps its map mask through the repair and the
  voyage back.
- Resync 52, 55, 62, 70, 83 class: on a Europe lane tile a hull (with its passengers) and loose
  land units are one tile chain, linked in arrival order (export, `col1_bridge_capture_unit_chains`).
- Save 37: the FUN_521d_6d8e prelude runs each colony's 5952 tick and then reads specialty,
  stocks and the wanting-colonists bit through DS:0x8542, which a road connect inside the tick
  leaves on another colony (`ai_euro_6d8e_prelude_read`); the a0cc table and DS:0xa0b8 are
  snapshots from that loop, and the WAGON_TRAIN bit is cleared on the bound record.
- Save 39: naval plunder is FUN_5fef_0352's loop (room check, FUN_5fef_016c's most valuable hold
  for a computer winner, 317c out, 30b8 in), and the 1b0e winner advance reveals nothing.
- Save 45: FUN_4962_0018's blockade probe (0906 floods) runs in every nation's 00f2 census, before
  6d8e's colony ticks, not inside each 5952 prelude; its tallies are not reset in 0a60.
- Resync 43, 58, 100, 127 class: DOS's dock chain has no "aboard"; FUN_1427_10be (the 5d04
  departure, `ai_euro_5d04_cb_unit_exhaust`) also takes a sentried rider of another docked hull,
  e.g. a Pioneer docked beside a Privateer under repair (`units_unboard`).
- Resync 203: a hull that cowers at a colony with a frigate nearby (raw 90373) writes plan 'C' and
  the 589e stay tail, which 5a78 leaves alone.

## Gap turns and the oracle

The oracle reads the next save, so it cannot see an e7c4 park in a turn the archive skipped.
Those parks are found by divergence minimization (force a candidate from the `[46-gate]` lines of
`AI_EA_TRACE`, keep it when the pair's diff shrinks) and listed in `k_america2_gap_parks`
(tests/golden/test_idle_campaign.c); `GOLDEN_IDLE_E7C4_FORCE=nation:x:y[:step[:value]],...` tries
more. So far: save 18 Dutch (44,57); save 46 Dutch (49,41) and English (51,42). With the oracle and
the table the continuous run matches saves 1..45, also with `GOLDEN_IDLE_RELOAD=1` (2026-10-21).
A restored-load DOSBox trace has residues of its own (its England parked a Dragoon at (51,42) that
the archive moved), so its draws are an oracle only up to its first e7c4 gate.

## End-state comparison

`python3 tools/save_compare.py DOS=.../000406.SAV port=SIM.sav` compares two ends per nation.
Plain continuous run (no oracle, 2026-10-22): same shape (army size and mix, colony count, food,
muskets, building tiers), different wars. DOS: France takes three Dutch colonies, England takes
New Amsterdam and burns 10 Tupi villages. Port: England takes four Dutch colonies and burns one
village, and the Dutch collapse (33 units vs 72, building tiers 59 vs 87). The port's AI nations end
34-61% richer in gold.

England and the Tupi (2026-10-22, both Americas archives): in DOS, England burns 10 Tupi villages
(war from turn 274 here, 245 in the first archive). An English Soldier, Dragoon or Artillery
entering a village sets bit 0x04 both ways (FUN_4d56_4528 arm), and alarm climbs to 100. The port
burns none in either continuous run because its England elects Pocahontas (turn 192 here, 263 in
the first archive). Pocahontas zeroes alarm with every tribe and halves every later rise, so
+1 steps round to 0 and alarm stays at 0 for good. DOS shows the same thing for France and the
Dutch, which elect her in both archives. DOS England never does: in the 1600s its political pick
rolled Paine (Pocahontas weight 5 of 16 with Franklin owned). Resync (each turn from the DOS
save, oracle on) reproduces all 95 AI father picks and every alarm delta except a few cells in
pairs that already diverge for other reasons, and it reproduces the war and the burns at 184-187.
So this is history divergence after the e7c4 split, not a port bug.

## Open

- Save 46 (35 fields): France's Merchantman arriving from Europe (233 -> 229 -> placed at (47,61))
  carries its Pioneer in the port and rolls the far-roam arm (plan 'D', (10,36)); in DOS (resident
  BPs 2243:04f4 = 20e6 thunk, 2143:02ea = 48d3_015e thunk, restored-load 45) both French
  Merchantmen act alone in T129 and sail home, and the Pioneer reaches the eastbound lane with the
  hull that entered Europe from (47,61) (its goto byte). Where DOS keeps the Pioneer between the
  arrival and that crossing is not traced yet (064e/10be on the arrival lane).
- Resync classes not chased yet: 354 (an explorer's ring20 hop slot roll, one RNG value), 300
  (history-dependent: a from-load DOS run captures Curacao, the archive demotes the attacker),
  105/113 (French hull goals after Europe), cargo_hold[5] tail guesses (resync artifacts).
