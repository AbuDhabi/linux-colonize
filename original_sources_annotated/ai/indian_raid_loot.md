# Colony raid loot + plunder pick (`FUN_5fef_0f14` / `016c`)

Combat siblings used by Indian raid / naval plunder. Port stand-ins live in
`ai_contact_indian_raids` / `units_plunder_*` — see
[`indian_raid_outcomes.md`](indian_raid_outcomes.md).

**Disassembly verified clean (2026-08-13).** `FUN_5fef_0f14` carried a
Ghidra disassembly-fault warning in the canonical export (`Removing
unreachable block (ram,0x0006125d)` — same defect class as `4528`/`2820`/
`417e`/`5b66`, see `docs/decomp_inventory.md`). Re-disassembled via the
overlay-addressing project (`docs/rtlink_decode_v2_gap.md`,
`tools/address_mapping.csv` → `OVL17_L0000:f14`): clean, self-contained,
309 lines, ends in a real return, one unrelated minor "unreachable block"
warning at a different address (ordinary decompiler noise, not the
corruption class). Line count (309) is close to this doc's existing
298-line estimate — unlike `4528`, this function wasn't severely
desynced, just untrusted pending verification. Confirms the `iStack_6`
variable documented below as the `@RAID*` kind selector is real and the
existing line-range docs below are trustworthy as written; not re-ported
here (existing Linux `ai_contact_indian_raids` phase coverage already
matches the documented shape).

---

## `FUN_5fef_0000` — candidate-unit search/scoring (pcode-blocked, hand-read)

Segment's first function, offset 0. Same disassembly-fault-lookalike as
the rest of this file's warnings, but the actual blocker is different:
Ghidra's decompiler throws `Offset must be between 0x0 and 0x10ffef, got
0xffffffff` on this one (root-caused 2026-08-13 alongside `OVL12_L0000:0`
- see `euro_unit_act.md`'s writeup there - as a decompiler bug in
`CALLF 0x1000:XXXX` far-call resolution local to these two functions, not
a disassembly corruption - the raw bytes are clean, no overlap/bad-
instruction warnings, no bad pcode varnodes at the per-instruction level).

Hand-read from the raw disassembly (362 bytes, `OVL17_L0000:0000`-`016b`,
`ENTER 0x14,0x0` / `PUSH SI` / ... / `POP SI` / `LEAVE` / `RETF`):
a **best-candidate search loop** over the unit table (record stride
`0x1c` = 28 bytes, matching the same `unit+0x3144..0x314c` field group
`ai_native_pick_dir_asm`'s new RNG(1,5) visibility work this session
already uses `unit+0x3147` from - same table, different sub-fields here:
`+0x3144`/`+0x3145` position-ish bytes fed to `FUN_1000_8886`/`84f2`/
`8958` "in range" tests, `+0x3146` a type/class byte checked against
`0xb` and a `0xd..0x12` band, `+0x314c` a status byte checked against
`5`/`6`). Two entry modes selected by `param_1 < 0`: a direct single-index
fast path (`param_1 >= 0`) vs. a full linear scan picking the
best-scoring candidate by a distance-like metric (`FUN_1000_8bb8`/`8bcc`
combined into `local = (byte<<8) - prior - 1`, then min-tracked against
`param_1`'s position via `FUN_1000_84d4`/`84de`). Returns the best index
found, or the `0xffff` sentinel local it's initialized to if the scan
exhausts without a hit.

Consistent with this file's "colony raid loot" family - reads as a
"find nearest/best-scoring eligible unit near this position" helper
(candidate for a raid target or loot-recipient picker), but the call
targets (`FUN_1000_8886`/`84f2`/`8958`/`8bb8`/`8bcc`/`84d4`/`84de`) and
several of the byte-field cutoffs aren't independently named — not
semantically confirmed enough to port. Not re-derived into full C here;
raw disassembly is in the 2026-08-13 investigation log if anyone resumes
this. Same "needs unlabeled DS globals/callees named first" gate as
`FUN_4d56_417e` (task #5).

---

## `FUN_5fef_016c` — pick cargo slot to plunder

| Item | Value |
|------|-------|
| Lines | **99209–99286** (~78) |
| Thunk | `FUN_2a1f_06b0` |
| Args | `param_1` = victim unit (holds); `param_2` = attacker (nation) |

### Algorithm

1. `slot_count = victim+0x3150`; if 0 → return −1
2. If attacker hold capacity (`2a1f_01a0`) ≥ count → can take; else skip
3. **Human** Euro attacker (`0x543f==0`): build CHOICE menu of cargos
   (`0be6` type / `0c68` qty / name `@CARGO`); cancel → −1; pick → 0-based slot;
   sets `DS:0xa154` busy flag
4. **AI**: score each slot = `euro_price[type][nation] * qty` (`-0x7b44` table);
   sort (`291f_0ed0`); pick best index

**Linux:** `units_plunder_ship_holds` / raid STORES arm — goods-value sort shape;
human CHOICE **thin/PARKED**.

---

## `FUN_5fef_0f14` — Indian raid colony loot + tension

| Item | Value |
|------|-------|
| Lines | **99738–100035** (~298) |
| Thunk | `FUN_2a1f_06c8` |
| Args | Indian nation-ish `param_1`, colony index `param_2`, … `param_4` force flag |

### Setup

Bind Indian (`0a42`), colony (`09e6` → `0x8542`); tribe name subst; RNG reseed;
roll difficulty-adjusted threshold vs colony defense probe `0ab0`.

### Kind roll `local_6` ∈ {0..4}

| Kind | Meaning | Fail → nothing |
|------|---------|----------------|
| 0 | Nothing / scare chrome only | early `LAB_5fef_123a` |
| 1 | **Goods** from warehouse (`+0x9a` stocks) | empty after 100 tries |
| 2 | **Building** destroy (`0bbe`) | no valid building after filters |
| 3 | **Unit** at colony (ship type 0xd..0x12 walk) | no unit / `088a` fail |
| 4 | **Gold** drain from Euro treasury | insufficient gold |

Difficulty / year / building-present gates can demote 2/3/4 → 1 or 0.
`param_4!=0` can skip the soft “nothing” early out.

### Apply + tension

| Kind | Effect | Tension delta arg to `0d6c` |
|------|--------|------------------------------|
| 1 | Halve-ish stock (clamp 1..10 roll); horses/tools side effects on tribe | `0xfffc` (−4) |
| 2 | Remove building; reassign jobs if needed | `0xfff4` (−12) |
| 3 | Combat apply `2a1f_06e0`→`5fef_0352` vs unit | `0xfff0` (−16) |
| 4 | Subtract rolled gold from nation ledger | `0xfff8` (−8) |
| 0 | Scare dialog only | skip `0d6c` |

Then clear war/chrome word at `(param_3*9 + euro)*2 + 0x54f6`.

Human: sounds + side-art strings `0x1b8a`…`0x1bba` by kind.

**Linux:** `@RAID*` kind picker in `ai_contact_indian_raids` — structural.
STORES half-stock clamp **Done** thin; GOLD drain peel **Done** thin. Kind-scaled
friction/alarm escalate (Series J: STORES +4, BURN/WREAK +12, SCALP +16, GOLD/SHIP
+8; Pocahontas/France half) **Done** thin. Full `0f14` RNG ladders still **PARKED**.
