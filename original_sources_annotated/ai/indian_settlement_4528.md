# Indian settlement enter / raid (`FUN_4d56_4528` authentic head)

Layer D map for the **reliable head** of `FUN_4d56_4528` only. Full body is
~3073 lines (83699–~86768); after ~84216 Ghidra injects foreign `LAB_521d_*` /
`LAB_5bfb_*` / `LAB_5fef_*` labels — **do not** invent mid-body sections from
that soup. Mid-body deepen needs ASM.

Thunk: `2a1f_016c`. Callers: move-foreign / contact (`move_spent` §3) — **not**
quiet `14fe`.

Loot outcomes (sibling path, not direct callees inside `4528`):
[`indian_raid_outcomes.md`](indian_raid_outcomes.md) (`FUN_5fef_0f14` / `016c` /
`0352` / `0ec0`).

**Port status:** head warn CHOICE + ship abort + Linux raid/fallout arms
**Done** thin; deep mid-body / VGA **PARKED**.

## Authentic head (~83699–84215)

```
FUN_4d56_4528(unit, ?, ?, ?)
  local_5a=1; bind tribe (09f0 / 0a4c)
  euro_nibble = unit.nation&0xf
  tribe_nation = tribe.type − 4
  relation = 030c(tribe, euro) → iVar20
  friction = tribe[euro*2+10] → local_66

  if human Euro (control==0) && DS:0xa2==0:
    BGM 04ac

  if unit is ship (type 0xd..0x12):
    if !(euro_diplo & 0x20): @DONTKNOWSHIPS → LAB_4d56_4bf2 (abort)
    if relation≥0x4b (75) || friction≥0x40 (64):
      @MADATSHIPS → 4bf2 abort
    else: fall through toward village meet (narrow mid-relation window;
      peace floor 96 normally hits MADAT)

  if human Euro:
    discovery 0524
    compose warn string base 0x1710 + band:
      relation ≥0x4b → 0x1718
      ≥0x32 → 0x171c
      ≥0x19 (or high friction) → 0x1720 / 0x1727 / 0x172e by DS:0x8d52
      else → 0x1718 path above
    if relation>0x31: BGM 5|6|7 by 0x8d52
    CHOICE 291f_0182 — cancel → 4bf2
    strip cargo chrome for wagon/ship/missionary/combat types (0022/0176)
    flag local_60 if combat-capable land

  … (further authentic LABs 4a6d / 4a85 / 4b0f / 319d before soup) …
```

### Dialog string band (human warn)

| Relation `iVar20` | String id | Notes |
|-------------------|-----------|-------|
| ≥ `0x4b` (75) | `0x1718` | Friendliest warn band |
| ≥ `0x32` (50) | `0x171c` | Mid |
| ≥ `0x19` (25) | `0x1720` | Low; or `0x1727`/`0x172e` if friction&lt;`0x80` and voice `0x8d52` |
| (base copy) | `0x1710` | Prefixed into buffer before append |

Exact `@INDIAN*` GAME.TXT mapping not fully XREF’d here — treat as DOS resource
ids until string table peel.

### Early exits → `LAB_4d56_4bf2`

- Ship without met bit `0x20` → `@DONTKNOWSHIPS`
- Ship with relation ≥ 75 or friction ≥ 64 → `@MADATSHIPS`
- Human cancels CHOICE (`291f_0182==0`)

Linux: `COLONIZE_ENTER_VILLAGE_SHIP` + `ai_contact_try_ship_village` (no landfall).

## Decomp contamination note (ASM pass, 2026-08-13 — correction of a same-day
## over-claim, see below)

**Root cause found, and it's stronger than either prior guess.** The raw
decompiled source carries Ghidra's own warning immediately before this
function's declaration (`viceroy_unpacked.c` line 83695-83697):

```
// WARNING: Instruction at (ram,0x000586cb) overlaps instruction at (ram,0x000586c7)
// WARNING: Control flow encountered bad instruction data
```

Ghidra is stating outright that its **disassembly** (not just the C
decompiler's higher-level reconstruction) went wrong at/near this function —
overlapping instructions is the classic symptom of a byte-offset
misalignment (decoding started one or more bytes into a real instruction, so
everything downstream is partly garbage until the stream re-syncs by luck).
That alone is sufficient explanation for messy labels/branches appearing
later in the function; no overlay or tail-jump theory is needed to explain
it, and none should be assumed without further evidence.

An earlier pass on the same day *thought* it had confirmed a real cross-segment
`JMPF` tail-jump explanation (citing that `LAB_5fef_004a` is a genuine label
that really does exist in segment `5fef`, ASM line 167275, `FUN_5fef_0000`'s
own body). That fact is true but doesn't prove the mechanism: a direct ASM
search for `JMPF` instructions inside `FUN_4d56_4528`'s own instruction range
(`viceroy_unpacked.asm` 143410–144409, all `CODE_125:4d56...`-tagged) found
**none** crossing to another segment. So the tail-jump claim is retracted —
it was an unconfirmed inference dressed up as a finding. The
`LAB_5fef_004a`-exists-elsewhere fact is real but doesn't by itself connect to
this function.

**Net, revised:** the original caution stands, now on firmer ground (a
Ghidra-acknowledged disassembly fault, not a vague "overlay collision"
guess). Do not section-map mid-body until someone manually re-disassembles
the bytes around `(ram,0x000586c7)` to find the correct instruction boundary
and re-derives the true control flow from there — that's raw byte-level work,
a different and much slower task than reading either the C or the ASM as
Ghidra already segmented them.

## Linux phase arms vs head

| DOS head idea | Linux `ai_contact_indian_raids` |
|---------------|----------------------------------|
| Relation / friction gates | Alarm/friction ≥40 (Spain ≥35); war prefer |
| Human warn CHOICE | `ai_contact_try_village_raid_warn` Attack/Leave; apply opens hostilities + deferred move |
| Ship abort | `ai_contact_try_ship_village` (`@DONTKNOWSHIPS` / `@MADATSHIPS`; mid-band 50..74 wary + Meet, Series T) |
| Post-head combat / loot | Adjacent combat + `@RAID*` kinds + fallout `@LOOT`/`@LOOT2` |
| Capture / burn | High band + tiny pop → `colonies_capture` |

## Related authentic LABs (named in early span)

| LAB | Role (from catalog / skim) |
|-----|----------------------------|
| `4a6d` / `4a85` / `4b0f` | Post-CHOICE contact / mission / combat branches (needs ASM confirm) |
| `4bf2` | Common abort / return |
| `319d` | Later authentic label before soup |

## Open RE

- ASM-faithful map 84216→end
- Wire `4528` return codes into move-foreign caller
- String table XREF `0x1710`…`0x172e` → `GAME.TXT`
