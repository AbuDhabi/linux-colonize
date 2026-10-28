# DOS bug candidates, sweep 2026-10-22

STATUS: open review list for the user, not a spec. Only C-63..C-65 and C-67..C-71
(section C below) are still undecided (C-66 decided 2026-10-24); an agent must not file them in
[dos_bugs.md](dos_bugs.md) or [tolerated_dos_bugs.md](tolerated_dos_bugs.md)
alone. Archive this file once they are decided. Every other item is decided;
the full review text (sections A, B, C-61/C-62, D, E, F) is verbatim in
[archive/dos_bug_candidates_2026-10-22_decided.md](archive/dos_bug_candidates_2026-10-22_decided.md),
where the "Came from dos_bug_candidates_2026-10-22.md C-N" cites resolve.

`raw NNNNN` = line in `original_sources_decompiled/viceroy_unpacked.c`.
Confidence is that the DOS code is a programmer error, not design.

## Outcome of sections A and B (2026-10-22)

Each fix was simulated alone (user rule: goldens pass -> dos_bugs.md, any
golden fails -> tolerated_dos_bugs.md). Entries carry the full evidence.

- Not reproduced any more, [dos_bugs.md](dos_bugs.md): C-1 DB-12, C-2 DB-13,
  C-3 DB-14, C-4 DB-15, C-11 DB-22, C-14 walk-origin half DB-23, C-15
  founding half DB-24, C-22 DB-25, C-24 DB-26, C-26 DB-27, C-27 DB-28, C-31
  DB-29, C-33 DB-30, C-34 DB-31, C-39 DB-32, C-46 DB-33, C-49 DB-34, C-53 DB-36 + DB-37, C-55 DB-38, C-57 DB-39, C-58 DB-40.
- Tolerated, [tolerated_dos_bugs.md](tolerated_dos_bugs.md) and
  [tolerated_dos_bugs_ai.md](tolerated_dos_bugs_ai.md) (TB-9..30): C-8b TB-9,
  C-9 TB-10, C-10 TB-11, C-12 TB-12, C-13 TB-13, C-14 road-connect payer TB-14,
  C-14 AND test TB-15, C-15 tick TB-16, C-15 local_78 TB-17, C-16 TB-18, C-17
  TB-19, C-18 TB-20, C-19 TB-21 + TB-22, C-20 TB-23, C-21 TB-24, C-23 TB-25,
  C-25 TB-26, C-28 TB-27, C-29 TB-28, C-30 TB-29, C-32 TB-30, C-35 TB-31, C-36
  TB-32, C-38 TB-33, C-40 ship count TB-34, C-41 TB-35, C-42 TB-36, C-43 TB-37
  + TB-38, C-44 TB-39, C-45 TB-40, C-47 TB-41, C-48 TB-42, C-50 TB-43, C-54
  TB-44, C-59 TB-46, C-6 TB-47 (all five unit-cap sites; fix passed the goldens
  but the user kept DOS for the late-game AI saves; DB-17..DB-21 withdrawn).
- No fix without inventing a rule (left as they are): C-8a, C-8c, C-8d, C-37,
  C-40 capacity term (20e6 raw 88557-88566 has the same shape: a heuristic),
  C-51, C-60 (byte save fields).
- Judged intended by the user, entry withdrawn: C-5 (DB-16, @KINGMERCY cut
  content), C-52 (DB-35, follow DOS), C-56 (TB-45, Man-O-War radius 1).
- Section C, port already differs, recorded as not reproduced: C-61 DB-41,
  C-62 DB-42.
- C-7 refuted: DOS closes the bracket; the port's missing ")" was a port bug,
  fixed as port_bugs.md #1147.

## Outcome of section D (2026-10-23)

C-72..C-81 judged DOS bugs by the user; simulated the same way, then applied
together (full ctest + `golden_ai_joint` green).

- Not reproduced any more, [dos_bugs.md](dos_bugs.md): C-72 DB-46, C-73 DB-47,
  C-74 DB-48, C-75 DB-49, C-76 DB-50, C-78 DB-51, C-79 DB-52.
- Tolerated, [tolerated_dos_bugs.md](tolerated_dos_bugs.md): C-77 TB-49, C-81
  TB-50.
- C-80 left as it is (user): no fix without inventing which units post goal 6.
- Low-confidence leftovers, reviewed 2026-10-23: none filed, all left as they
  are (port reproduces each literally). 20e6 `milvet == 0`: dead (mode 6
  counts everything mode 4 counts) and the intended variable is not
  recoverable. 5d04 `|| type == 0x0b`: the shared awaiting-orders idiom, dead
  inside the hull range; damaged hulls do leave once the repair tick clears
  bit 0x80. DS:0x1734 loop: an all-or-nothing gate with the same >= 25
  threshold as raw 89616; looks deliberate. Recruit prices: AI difficulty
  discount by design (two paths, two surcharges). `rival_navy_strength`: a
  zero test in Frigate units, not an average. +0x315a sharing: forced by the
  record layout (voyage, failed-move tail, repair, cower). The port comments
  for the last two were wrong and are rewritten (`ai_euro_europe.c`
  `ai_euro_5d04_compute_flags`, `ai_euro_ship.c` cower block).

## Left as they are (do not re-file)

The port reproduces each literally; a fix would invent a rule.

- C-8a. @KINGNEWWAR strength compare is always `0 <= 0` (FUN_38fd_5930 raw 68352-68362; port `ai_king_war.c`).
- C-8c. 5d04 tail reads `unit[-1].flags & 0x20` (raw 92589, DS:0x312c static byte); port uses constant 0 (`ai_euro_europe.c`).
- C-8d. Colony name list exhausted: FUN_479b_0000 returns row 0 forever, so every later colony gets the same name (port `colony.c`).
- C-37. Attacker Artillery-in-open penalty tests the defender's fortify orders and Euro-ness (FUN_5fef_1b0e raw 100472-100493); may be a deliberate "bombard fortified targets" rule (port `combat_strength.c` `def_shielded`).
- C-40 capacity term. FUN_5fef_0352's unarmed-hull gate (raw 99538-99560) multiplies Frigates by capacity but not Privateers and omits Man-O-War; 20e6 raw 88557-88566 has the same shape, a heuristic. (The ship-count half is TB-34.)
- C-51. Mine depletion suppresses every worked Minerals/Silver plot with a miner on each counter wrap, not just the depleted one (FUN_364b_033a raw 56977-57012; port `turn_production.c`).
- C-60. Census counters: some saturate at 255, some wrap (FUN_4962_0018 raw 78111-78324 with FUN_4962_0006); inherent to the u8 save fields (port `col1_stuff_census.c`).
- C-80. Explorer goal 6 is never posted (raw 89270-89280 else arm unreachable), so the +0x10 explore-goal bonus is inert; no fix without inventing which units post goal 6.

---

## C. Port already does not reproduce; undocumented

Each needs a decision: add a dos_bugs entry (keep the port as is), or port the
DOS behaviour literally. C-61 and C-62 are decided (DB-41, DB-42).

### C-63. Military Assistance passes the nation id to the tribe selector
- DOS: FUN_5bfb_153e raw 98271, 98335 (OVL16 0x2da4, 0x2ec6): FUN_281f_0a42 gets 4..11; all 17 other callers subtract 4. Extinct test and hire price read the wrong tribe. Confidence: high.
- Port: `ai_diplo.c` ~1962 (row filter) and ~2013 (hire price) use `p - 4`; the row filter tests the right tribe's extinct bit (re-checked 2026-10-28).

### C-64. Euro diplomacy score and withdraw cost overflow int16
- DOS: FUN_5bfb_153e raw 97530-97577, 98150-98156 (OVL16 0x1990-0x1a02): `(diff+8)*delta*10` wraps past 32767 (delta > 273 at Viceroy), making the score 0 or random; withdraw cost wraps and is raised to 100. Confidence: high on the mechanism.
- Port: `ai_diplo.c:1223, 1243, 2183` use 32-bit int.

### C-65. Denounce Heresy weight compares two loop invariants
- DOS: thunk_FUN_1000_a594, overlays.c:75807 `if (uVar3 == param_2)` (asm OVL13 ~0x1f3f-0x1f5e): both are constant in the loop, so every village's mission weight goes to one side; the per-village owner is computed into `[BP-4]` and never read. Confidence: high.
- Port: `ai_contact_actions.c:1457` uses the per-village owner. (DB-7 is a different slip in the same function.)

### C-66. @INDIANCOME follow-up gate calls the alarm reader with swapped arguments
Decided 2026-10-24: ported literally (port_bugs.md #1230, CLOSED).
- DOS: FUN_5bfb_0182 raw 96551 (asm 5bfb:01e0-01f5): `030c(euro - 4, indian)` reads before the Indian records. Confidence: high.
- Port: reproduces it since 2026-10-24 (port_bugs.md #1230): `ai_contact_apply_welcome_accept` reads the same word through `ai_euro_5952_alarm_word(col1, e - 4, nation)` (dwelling slots 71..80 for euro 0..2, the Inca record for euro 3). The intended (slot, euro) read is the alarm 022e just clamped to 20, so the fix would show @INDIANCOME every time.

### C-67. Missionary revisit gate passes a tribe index as a nation id, and the human
- DOS: FUN_521d_20e6 raw 90057-90063 (overlays asm 138096-138101): `8c28([0x5398], [0x8d52])`, missing `+ 4`. Confidence: medium.
- Port: `ai_euro_land.c:2953-2960` uses the AI nation and the right record.

### C-68. Wander scorer's Euro-foe arm with owner nibble -1
- DOS: FUN_521d_20e6 raw 88877-88880 (asm 138877 signed `CMP [BP-0xe],4`): owner -1 passes and reads nation+0x33 (`return_from_europe_y`) as a relation byte, so the PEACE test is effectively ignored on unclaimed land. Confidence: high.
- Port: `ai_euro_land.c:2342` stand-in uses the occupant's nation.

### C-69. Homeless Brave despawn returns -1 and the caller steps the freed slot
- DOS: FUN_4d56_021a asm 0x337-0x365 returns -1 after destroying the unit; FUN_4d56_14fe 0x1502-0x1521 then steps it with dir -1. Use-after-free; corrupt/edited saves only.
- Port: `ai_brave.c` ~407 stops cleanly (#826).

### C-70. 5d04 Artillery price unset before turn 40
- DOS: FUN_521d_5d04 raw 92834: `local_38` assigned only when turn >= 40, otherwise stale or uninitialised. Port uses 0 (`ai_euro_europe.c:1427-1431`). DB-5 class.

### C-71. 5d04 Pioneer conversion leaves tools stale
- DOS: the unconditional Colonist->Pioneer conversion never sets +0x3159, and FUN_479b_0158 then wraps it to 236.. (#628, #636). Port substitutes tools = 100.
