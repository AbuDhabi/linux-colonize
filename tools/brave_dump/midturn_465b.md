# Mid-turn `465b` spent hang

## Config warning

`tools/brave_dump/dosbox-midturn-dump.conf` previously caused a black,
unresponsive window (bad SDL/autosave combo). Prefer launching hang EXEs from a
**normal** DOSBox-X session with `COLONIZE` as the mounted C: drive.

## `dump_b465` (VR_B465, first ADD of EOT)

Hang at spent ADD (`EB FE` replacing ADD). Registers:

| Item | Value | Meaning |
|------|-------|---------|
| `AL` | **3** | `local_40` step cost already 3 |
| `BX` | `0x54` (=3×`0x1c`) | unit index 3 |
| Unit | n=1 type=13 `(54,38)` spent=0 | Euro (not Sioux yet) |
| Sioux `(49,40)` | still spent=9 | Prior-turn spent; Indian pulse not reached |

So the first EOT spend is a **Euro** unit with cost 3. Sioux T2 is later.

## `dosbox_465n` (VR_B465N, post-EOT — no native hang)

Startup froze ~minutes (overlay/reloc disruption), then recovered. LOAD 0 + EOT
completed **without** hanging on Indian spends; human regained control.

Post-turn Memory (matches `TURN3` golden):

| Unit | Tile | spent |
|------|------|-------|
| Sioux n=10 | `(49,39)` | **3** |
| Apache n=7 | `(46,53)` | **3** |
| Arawak n=6 | `(47,16)` | **3** |

### Why the native hang never fired

1. **Reloc corrupted the stub.** Patch site overwrote a far-call whose segment
   bytes are still in the overlay reloc table. File stub
   `8A 87 47 31` (`MOV AL,[BX+0x3147]`) became `8A 87 6A 39` in Memory → nation
   test read garbage and usually `RET`’d.
2. **Second live ADD.** Unpatched `00 87 49 31` remains at file `368087`
   (Memory `0x8f9ef`). Natives likely spent through that path:
   `MOV AL,[BP-2A]` / `IMUL BX,[BP+6],1C` / `ADD [BX+3149],AL`.
3. Force-to-max `88 84 49 31` was JMP-skipped on the patched copy only; not
   found anywhere in this dump’s Memory.

Golden spent=3 is confirmed; mid-ADD `local_40` for Sioux still needed.

## `dosbox_b465s` (VR_B465S — no hang)

Patch **was** loaded (`EB FE` at second ADD, mem `0x8f9ef`) but **never executed**.
Turn completed; Sioux/Apache/Arawak spent **3** again. Braves spend via the
**first** ADD (`258771` / mem `0x9f25b`), not the second path.

So B465N’s “natives used ADD2” guess was wrong: they used ADD1’s stub (ADD
still ran; reloc made the nation check usually fall through).

## `dosbox_dump_465r2` (Brave type hang — still early)

Same picture as `freeze_465r`: French euros spent; **all Braves still at TURN2
xy/spent**; IRQ-clobbered regs. So `CMP [BX+3146],19` also false-fired during
euro phase (BX not a clean unit frame, or unaligned `19` in memory).

## `dump_b465l` / `dump_b465r` (broken builds — froze)

Both used `EB 44` at the ocean check → **JMP JOIN**, skipping the 53-byte
**continue** block (`258862`–`258914`). That soft-locks AI mid-turn.

| Dump | Scratch `DS:7000` | Why froze |
|------|-------------------|-----------|
| `dump_b465l` | cost=`03` BX=`0054` (unit 3 FR ship) | Continue skipped; human ship corrupted to `(25,232)` |
| `dump_b465r` | cost=`03` BX=`08d2` | Intentional `EB FE`, but `BX >= 0x150` false-fired on non-unit BX |

Braves still at TURN2 xy/spent in both — Indian pulse never reached.

## Phase 8 note

Far tiles for `(47,53)` fog flip **agree** Linux ↔ `SEED100.SAV` (probe
`tools/probe_far_ocean_4753`). Fog/dir hang is **parked** — see
[`init_20e6_4753.md`](init_20e6_4753.md). Prefer coarse-fog port (phase 9).
Spent hang unchanged (`VR_B465R`) and also parked.

## Phase 7 note

Score dump: fog `+8` on W far land vs ocean far NW flips init `(47,53)`.
Spent hang unchanged (`VR_B465R`).

## Phase 6 note

Quiet miss isolated to init n=7 Brave `(47,53)` (emp NW vs ASM W). Spent hang
target unchanged: `VR_B465R` → AL.

## Phase 5 note (scoring vs spent)

Quiet cutover remains blocked after LCG alignment (scoring-term gap). Spent
work stays independent: get **AL = local_40** at ADD1 for Sioux/Apache.

## Phase 4 status (Linux scoring cutover still blocked)

Quiet scoring is a separate track. Spent work here: get **AL = local_40** at
ADD1 for Sioux (`BX=0x1F8`) and Apache (`BX=0x1A4`). Until that byte is known,
do not change `ai_dos_move_spent` empirically.

**Sharper next hang target:** run `VR_B465R` first (Sioux). If AL reads cleanly
as 3, the Linux bug is upstream of the ADD (cost already wrong before add). If
AL is 9 (or 6), DOS computed the high cost and something after ADD (or a
different path) still forces golden 3 — chase force-to-max / second write.

Cross-check tiles with `tools/probe_sioux_spent.c` on `test-saves-ai/TURN2.SAV`.

## `dump_b465r3` (VR_B465R force-max stub — **success**)

Hang fired on Sioux ADD. Reloc wrote trash `B3 98` into the `EB 02` skip
slots; `CMP BX,1F8` survived in Memory.

| Item | Value | Meaning |
|------|-------|---------|
| `AL` / `BP-3E` | **9** | `local_40` at ADD |
| `BX` | `0x1F8` | Sioux unit 18 |
| Unit | n=10 t=19 `(49,40)` spent=**9** | ADD already applied (0+9) |
| Apache `[15]` | `(46,53)` spent=**3** | Already stepped; force-max disabled on this EXE → Apache **AL was 3** |
| Arawak `[14]` | `(47,16)` spent=3 | Prior Indian spends OK |

**Verdict:** DOS Sioux T2 cost head is **9**, same as Linux `class*3`. Golden
end-of-turn spent=3 is **not** a pre-ADD cost-head bug — chase **post-ADD**.
Apache on this dump already spent=3 with force stubbed ⇒ **AL was 3** at ADD.

## `dump_b465f3` (VR_B465F — force-max did **not** fire)

Turn completed (no hang). Probe stub present in Memory (`7D 0F 81 FB F8 01 74 FE`
at `0x9f2a5`). Sioux ended `(49,39)` **spent=3**.

| Outcome | Result |
|---------|--------|
| Hang on Brave | **No** |
| Sioux end spent | **3** (golden) |
| Stock E4D2 | Yes (same as VICEROY) — still felt slow |

**Verdict:** Stock force-max body is **not** entered for Sioux T2 land step.
Something else turns ADD of 9 into end spent 3. Do **not** chase ocean/0696
gate fixes for this row. Widen RE past `465b:0628` (0934 exhaust is Brave-skipped
via 088a; check post-`465b` act bookkeeping / other 3149 writers).

## Phase 17 — dump-free spent search exhausted

Re-parsed `dump_b465r3` / `dump_b465f3` / `dump_vrb465x2` without new hangs:

- Sioux ADD AL=9; force-max not entered; x2 shows spent=9 without XY commit.
- Apache SAV head=6; “AL≈3” inference retired (post-ADD can clamp either).
- Map predicates (ocean-adj dest, capital dist≤1) break T1 spent=9.
- Keep `k_quiet_brave_t2` overlays. Oracle: `./build/probe_sioux_spent`.

**Still needed for a real fix:** successful `VR_B465X` → `dump_b465x3` (table
below). Do not invent cost-head caps.

Call-graph annotation (`original_sources_annotated/ai/brave_spent_callgraph.md`):
post-ADD chrome (`0916`/`0948`/`08da`/`084e`/`07fe`/…) → `FUN_1427_*` bodies
do **not** write `0x3149`; in-`465b` `0934` paths cannot fire for lone Brave.
Hang X remains the named localizer; Linux keeps `k_quiet_brave_t2`.

## Phase 16 — `VR_B465X` (hang at `465b` exit)

Localize whether spent is already 3 **inside** `465b` or only **after** return.

### `dump_vrb465x` / `dump_vrb465x2` (broken X builds)

| Dump | Failure |
|------|---------|
| `dump_vrb465x` | Force-window stub: reloc ate `74 FE` — no hang |
| `dump_vrb465x2` | Reloc-safe stub in force window overwrote ocean **JGE** → every unit `RETF` after ADD without xy commit; hang intact but never reached Sioux; control returned, nothing moved |

| File offset | Role |
|-------------|------|
| `0x3F2D3` | ADD1 **stock** |
| `0x3F31D` | Force-max **stock** (must keep `JGE`) |
| `0xE4D2` | **Stock** |
| `0x3F909` | `LAB_465b_0c19` → `E9 02 00 90` JMP stub |
| `0x3F90E` | Stub (14 B): `IMUL BX,[BP+6],1C`; `CMP BX,1F8`; hang / RETF |

```bash
python3 tools/brave_dump/patch_b465_hang.py
```

### Hang run (`VR_B465X`) — Sioux at `465b` RETF

1. `TURN2.SAV` → `COLONY00.SAV`
2. **Latest** `VR_B465X.EXE` (force-max stock; stub after RETF) → LOAD 0 → EOT until freeze
3. Save → `dosbox-x-dumps/dump_vrb465x3`
4. Parse: `python3 tools/brave_dump/parse_465b_dump.py dosbox-x-dumps/dump_vrb465x3`
5. Confirm Memory has `6B 5E 06 1C 81 FB F8 01 74 FE` near exit; force window still `7D 0F…`; `BX=0x1F8`; read **spent**

| Hang spent | Meaning | Next |
|------------|---------|------|
| **9** | Writer is **after** `465b` returns | `VR_B465E` (`--with-e`); dump `dump_b465e3` |
| **3** | Writer still **inside** `465b` after ADD | Unlabeled site — static chrome table already says no 3149; re-diff ASM |
| No hang; units move | Filter / JMP missed | Report |

Optional Apache ADD confirm: **`VR_B465A.EXE`** → `dump_b465a3` (expect `BX=0x1A4`, **AL=3**).

### `VR_B465E` (only if X shows spent=9)

```bash
python3 tools/brave_dump/patch_b465_hang.py --with-e
```

Hang at `FUN_1427_155e` IMUL (`0x7BDD`): `CMP BX,1F8` / hang. Dump →
`dump_b465e3`. Hang ⇒ `0934` is the 9→3 writer — backtrace caller.

### Slowness note

Trampoline was **not** the only cause: `VR_B465F` uses stock `E4D2` and is still
slow. Likely any patch in the `465b` overlay window (reloc/EMS thrash).

## `VR_B465F` — force-max entry probe (done)

Stock ADD1 left intact. Force-max window (`0x3F31D`): keep `JGE` skip; if the
body would run, `CMP BX,1F8` / hang.

| Outcome | Meaning |
|---------|---------|
| **Hangs**, `BX=0x1F8` | Stock force-max **fires** for Sioux — fix Linux ocean/0696 inputs |
| **No hang**; turn ends Sioux spent=**3** | **Observed (`dump_b465f3`)** — other post-ADD writer |
| **No hang**; spent stays **9** | This EXE path differed — report, do not invent caps |

Optional Apache confirm: **`VR_B465A.EXE`** → `dump_b465a3` (expect `BX=0x1A4`, **AL=3**).

## Rebuild (2026-08-06) — force-max reloc-safe stub (post-evm0015)

### Cave attempt failed (`dos_dump.sav` / `dosbox_dump.sav`)

Cave stub at `0x3ECD0` + ADD1 `CALL` (`E8 FA F9 90`) loaded correctly in
Memory, but fatals:

`Fatal Error evm0015: vectoring error: interrupt handler calls a Virtual Page.
Vectored call to Page 0018H … near address E646:00B8.`

Near CALL from ADD1 to the cave crosses an EMS overlay page boundary even
though both regions appear in a full Memory dump. **Do not use the cave.**

### Current layout (same overlay page as ADD1)

Rebuild from stock `VICEROY.EXE` via [`patch_b465_hang.py`](patch_b465_hang.py).
Stub sits in the force-max window with reloc trash skipped:

| File offset | Role |
|-------------|------|
| `0x3F2D3` | ADD1 → `E8 49 00 90` (CALL stub ADD + NOP) |
| `0x3F31D` | `EB 0F` skip for ocean fallthrough |
| `0x3F31F` | Stub: `ADD`; `EB 02`; trash; `CMP BX,imm`; `JZ` hang; `RET` |
| `0xE4D2` | Launch trampoline |
| `0x3ECD0` | Must stay zeros (no executable cave) |

| EXE | Filter | Behavior |
|-----|--------|----------|
| `VR_B465R.EXE` | `CMP BX,1F8` | Sioux unit 18; **AL = local_40** |
| `VR_B465A.EXE` | `CMP BX,1A4` | Apache unit 15; **AL = local_40** |
| `VR_B465L.EXE` | log BX/AL → `DS:7000` | No hang; last ADD logged |

```bash
python3 tools/brave_dump/patch_b465_hang.py   # regenerate from VICEROY.EXE
```

### Verify before trusting a dump

**File:** `VR_B465R.EXE` has `81 FB F8 01` at `0x3F327` (inside force stub).  
**Memory (after hang):** same `81 FB F8 01` still present; `BX == 0x1F8`; **AL** is cost.
If Memory lost the `CMP`, reloc beat the `EB 02` skip — stop and report.

### Logger run (`VR_B465L`)

1. `TURN2.SAV` → `COLONY00.SAV`
2. `VR_B465L.EXE` → LOAD 0 → EOT → wait until **you** control again
3. Save as `dosbox-x-dumps/dump_b465l3`
4. Read `DS:7000` (BX word) / `DS:7002` (AL)

### Hang run (`VR_B465R`) — Sioux mid-ADD cost

1. `TURN2.SAV` → `COLONY00.SAV`
2. `VR_B465R.EXE` → LOAD 0 → EOT until freeze (Indian pulse)
3. DOSBox-X save → `dosbox-x-dumps/dump_b465r3`
4. Confirm Memory still has `CMP BX,1F8`; unit `BX=0x1F8`; **AL = local_40**
5. Optional: `VR_B465A` → `dump_b465a3` for Apache (`BX=0x1A4`)

`VR_B465D` (`EB FE` on ADD1) still hangs on the first Euro spend — avoid for
Sioux/Apache AL.

## Bad trampoline note

Do **not** put stubs in the FAR-pointer null table at file `285914` (launch freeze).
Do **not** `JMP JOIN` (`EB 44`) over continue. Do **not** filter `BX >= 0x150`.
Do **not** CALL a distant zero cave (`0x3ECD0`) — EMS `evm0015`.
Naive force-max stubs without `EB 02` reloc skip lose `CMP BX` in Memory.
`VR_B465S` uses zero padding at file `372566` (near the second ADD; natives use ADD1).
