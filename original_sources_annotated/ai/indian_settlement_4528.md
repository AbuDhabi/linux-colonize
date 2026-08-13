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
**Done** thin; full body now recovered clean (see below) — VGA chrome still open.

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

## Full body recovered (2026-08-13, via the overlay-addressing project)

Root cause of the desync, precisely located: `(ram,0x000586c7)` in the
canonical flattened project maps (via `tools/address_mapping.csv`) to file
offset `0x53AC7` in `VICEROY_OUT_2.EXE` — which falls in the *next* RTLink
segment over (`OVL14_L0000`, original DOS segment 16), not the segment this
function actually lives in (`OVL13_L0000`, DOS segment 15, file offset
`0x4528` within it). The flattened file places these two unrelated overlay
segments back-to-back; Ghidra's linear disassembly, once desynced inside
`FUN_4d56_4528`, walked straight across that seam and kept decoding OVL14's
unrelated bytes as if they were still part of this function — which is
exactly why later labels look foreign (`LAB_521d_*` etc: real code, just
not *this* function's code).

Re-disassembled `OVL13_L0000::4528` directly in the clean per-overlay Ghidra
project (`~/projects/ghidra_overlay_scratch/OverlayTest.gpr` —
`docs/rtlink_decode_v2_gap.md`), bypassing the flattened file entirely. Result:
a complete, self-contained, **312-line** function (`OVL13_L0000::4528` –
`0x4c20`, right up against that segment's own local thunk table at `0x4c22`
— never leaves `OVL13_L0000`) — vs. the canonical export's corrupted
~3073-line sprawl across foreign segments. No Ghidra `WARNING:`s, no
`halt_baddata`, no foreign labels. Matches the manually-reasoned "authentic
head" above point for point once decompiled: same dialog-string band values
(`0x1718`/`0x171c`/`0x1720`/`0x1727`/`0x172e`) and the same relation
thresholds (`0x4b`=75, `0x32`=50, `0x19`=25) fall right out of the clean
decompile, not inferred.

Full corrected decompile (Ghidra output, resident-space calls like
`FUN_1000_8628` are the same functions the canonical export already names
correctly — only this function's own body was affected):

```c
int FUN_4d56_4528(undefined2 param_1, int param_2, undefined2 param_3, undefined2 param_4)
{
  byte bVar1;
  uint uVar2;
  int iVar3;
  uint uVar4;
  undefined2 uVar5;
  undefined1 *puVar6;
  int iVar7;
  int iVar8;
  undefined2 uVar9;
  undefined2 unaff_DS;
  undefined4 uVar10;
  undefined2 uVar11;
  undefined1 auStack_ba [80];
  uint uStack_6a;
  undefined2 uStack_68;
  int iStack_66;
  undefined2 uStack_64;
  int iStack_62;
  int iStack_60;
  undefined4 uStack_5e;
  int iStack_5a;
  int iStack_58;
  undefined2 uStack_56;
  undefined1 auStack_54 [80];
  uint uStack_4;

  iStack_5a = 1;
  iStack_60 = 0;
  uStack_5e._2_2_ = 0;
  uStack_5e._0_2_ = 0;
  iStack_58 = *(int *)0x539c;
  uStack_64 = FUN_1000_8be0(param_3,param_4);
  FUN_1000_8c3c(0x181f,uStack_64);
  uVar2 = *(byte *)(param_2 * 0x1c + 0x3147) & 0xf;
  uStack_4 = (uint)*(byte *)(*(int *)0x8d4a + 2);
  iVar7 = uStack_4 - 4;
  uStack_6a = uVar2;
  iStack_62 = iVar7;
  iVar3 = FUN_1000_84fc(0x181f,iVar7,uVar2);
  iStack_66 = *(int *)(*(int *)0x8d4a + uStack_6a * 2 + 10);
  if ((((int)uStack_6a < 4) && (*(char *)(uStack_6a * 0x34 + 0x543f) == '\0')) &&
     (*(int *)0xa2 == 0)) {
    if (*(int *)0x8d52 == 0) {
      iVar7 = 7;
    }
    else if (*(int *)0x8d52 == 1) {
      iVar7 = 6;
    }
    else {
      iVar7 = 5;
    }
    FUN_1000_869c(0x181f,iVar7);
  }
  if ((0xc < *(byte *)(param_2 * 0x1c + 0x3146)) && (*(byte *)(param_2 * 0x1c + 0x3146) < 0x13)) {
    uVar4 = FUN_1000_8c28(0x181f,uStack_6a,uStack_4);
    if ((uVar4 & 0x20) == 0) {
      FUN_1000_85ee(0x181f);
      uVar10 = CONCAT22(uStack_5e._2_2_,(undefined2)uStack_5e);
      goto switchD_004bdb_default;
    }
    if ((0x4a < iVar3) || (0x3f < iStack_66)) {
      uVar5 = func_0x00018b94(0x181f,uStack_4);
      FUN_1000_8628(0,uVar5);
      func_0x0001938c(0x181f,0x1705,*(undefined2 *)0x8d52);
      uVar10 = CONCAT22(uStack_5e._2_2_,(undefined2)uStack_5e);
      goto switchD_004bdb_default;
    }
  }
  if (((int)uStack_6a < 4) && (*(char *)(uStack_6a * 0x34 + 0x543f) == '\0')) {
    FUN_1000_8714(0x181f,7);
    FUN_1000_8628(0,*(undefined2 *)((uint)*(byte *)(*(int *)0x8d4e + 2) * 6 + -0x69cc));
    uVar5 = FUN_1000_8c0a(uStack_4);
    FUN_1000_8628(1,uVar5);
    FUN_0000_d9b4(auStack_54,0x1710);
    if (iVar3 < 0x4b) {
      if (iVar3 < 0x32) {
        if ((iVar3 < 0x19) && (*(int *)(*(int *)0x8d4a + uStack_6a * 2 + 10) < 0x80)) {
          if (*(int *)0x8d52 == 2) { uVar5 = 0x1727; } else { uVar5 = 0x172e; }
        } else { uVar5 = 0x1720; }
      } else { uVar5 = 0x171c; }
    } else { uVar5 = 0x1718; }
    uVar9 = 0xd1d;
    FUN_0000_d974(auStack_54,uVar5);
    if (0x31 < iVar3) {
      if (*(int *)0x8d52 == 0) { uVar5 = 7; }
      else if (*(int *)0x8d52 == 1) { uVar5 = 6; }
      else { uVar5 = 5; }
      uVar9 = 0x181f;
      FUN_1000_869c(0xd1d,uVar5);
    }
    uStack_5e = func_0x00019372(uVar9);
    uVar10 = 0;
    if (uStack_5e == 0) goto switchD_004bdb_default;
    iVar8 = param_2 * 0x1c;
    if ((*(char *)(iVar8 + 0x3146) == '\f') ||
       ((0xc < *(byte *)(iVar8 + 0x3146) && (*(byte *)(iVar8 + 0x3146) < 0x13)))) {
      if (iVar3 < 0x4b) { uVar9 = 1; uVar5 = *(undefined2 *)0x932a; }
      else { uVar9 = 2; uVar5 = *(undefined2 *)0x932c; }
      uVar10 = FUN_1000_8212(0x191f,uVar5,uVar9);
      func_0x00019366(0x181f,uStack_5e,uVar10);
    }
    if (*(char *)(param_2 * 0x1c + 0x3146) == '\x05') {
      uVar10 = FUN_1000_8212(0x191f,*(undefined2 *)0x9334,6);
      func_0x00019366(0x181f,uStack_5e,uVar10);
    }
    if (((*(byte *)(param_2 * 0x1c + 0x3146) < 0xd) || (0x12 < *(byte *)(param_2 * 0x1c + 0x3146)))
       && (1 < *(byte *)((uint)*(byte *)(param_2 * 0x1c + 0x3146) * 0xe + 0x5236))) {
      uVar10 = FUN_1000_8212(0x191f,*(undefined2 *)0x933a,9);
      func_0x00019366(0x181f,uStack_5e,uVar10);
      iStack_60 = 1;
    }
    uVar5 = 0x181f;
    uVar4 = FUN_1000_8c28(0x191f,uStack_6a,uStack_4);
    if ((uVar4 & 0x40) != 0) {
      if (*(char *)(param_2 * 0x1c + 0x3146) == '\x03') {
        if (*(char *)(*(int *)0x8d4a + 5) < '\0') {
          uVar9 = 0x181f;
          puVar6 = (undefined1 *)FUN_1000_8212(0x181f,*(undefined2 *)0x932e,3);
LAB_0049c8:
          uVar5 = 0x191f;
          func_0x00019366(uVar9,uStack_5e,puVar6);
        }
        else if ((*(byte *)(*(int *)0x8d4a + 5) & 0xf) != uStack_6a) {
          FUN_1000_8212(0x181f,*(undefined2 *)0x9330);
          FUN_0000_e34e(auStack_54);
          uVar5 = FUN_1000_8c0a(*(byte *)(*(int *)0x8d4a + 5) & 0xf);
          uVar10 = FUN_1000_8212(0x181f,uVar5);
          uVar9 = 0xd1d;
          FUN_0000_dd18(auStack_ba,auStack_54,uVar10);
          puVar6 = auStack_ba;
          goto LAB_0049c8;
        }
        uVar11 = 7;
        uVar9 = *(undefined2 *)0x9336;
      } else {
        uVar5 = 0x181f;
        iVar3 = FUN_1000_8d68(0x181f,param_2);
        if (((-1 < iVar3) &&
            (bVar1 = *(byte *)(param_2 * 0x1c + 0x3146), *(byte *)((uint)bVar1 * 0xe + 0x5236) < 2))
           && (bVar1 != 5)) {
          uVar5 = 0x181f;
          iVar3 = FUN_1000_8d68(0x181f,param_2);
          if (iVar3 != 0x1b) {
            uVar10 = FUN_1000_8212(0x181f,*(undefined2 *)0x9332,5);
            uVar5 = 0x191f;
            func_0x00019366(0x181f,uStack_5e,uVar10);
          }
        }
        bVar1 = *(byte *)(param_2 * 0x1c + 0x3146);
        if ((*(char *)((uint)bVar1 * 0xe + 0x5236) == '\0') || ((0xc < bVar1 && (bVar1 < 0x13))))
        goto LAB_004a85;
        uVar11 = 8;
        uVar9 = *(undefined2 *)0x9338;
      }
      uVar10 = FUN_1000_8212(uVar5,uVar9,uVar11);
      uVar5 = 0x191f;
      func_0x00019366(0x181f,uStack_5e,uVar10);
    }
LAB_004a85:
    if (((iStack_60 == 0) &&
        (bVar1 = *(byte *)(param_2 * 0x1c + 0x3146), *(char *)((uint)bVar1 * 0xe + 0x5236) != '\0'))
       && ((bVar1 < 0xd || (0x12 < bVar1)))) {
      uVar10 = FUN_1000_8212(uVar5,*(undefined2 *)0x933a,9);
      uVar5 = 0x191f;
      func_0x00019366(0x181f,uStack_5e,uVar10);
    }
    uVar10 = FUN_1000_8212(uVar5,*(undefined2 *)0x933c,10);
    func_0x00019366(0x181f,uStack_5e,uVar10);
    uStack_56 = FUN_1000_935a(uStack_5e);
    func_0x00019398(0x191f,uStack_5e);
    uStack_5e._2_2_ = 0;
    uStack_5e._0_2_ = 0;
  }
  else {
    switch(*(undefined1 *)(param_2 * 0x1c + 0x3146)) {
    case 1: case 4: case 0xb:
      uStack_68 = FUN_1000_8912(0,((undefined1 *)&LAB_003144)[param_2 * 0x1c],
                                *(undefined1 *)(param_2 * 0x1c + 0x3145));
      uStack_56 = 9;
      break;
    default:
      iVar8 = FUN_1000_8d68(0,param_2);
      uVar10 = CONCAT22(uStack_5e._2_2_,(undefined2)uStack_5e);
      if (((iVar8 < 0) ||
          ((*(char *)(param_2 * 0x1c + 0x315b) != '\x1c' &&
           (uVar10 = CONCAT22(uStack_5e._2_2_,(undefined2)uStack_5e),
           *(char *)(param_2 * 0x1c + 0x315b) != '\x19')))) ||
         (uVar10 = CONCAT22(uStack_5e._2_2_,(undefined2)uStack_5e), 0x4a < iVar3))
      goto switchD_004bdb_default;
      uStack_56 = 5;
      break;
    case 3:
      iVar3 = FUN_1000_84fc(0,*(undefined2 *)0x8d52,*(undefined2 *)0x5398);
      if (((iVar3 < 0x4b) &&
          (uVar4 = FUN_1000_8c28(0x181f,*(undefined2 *)0x5398,uStack_4), (uVar4 & 0x20) != 0)) &&
         (*(byte *)(uStack_6a + 0x917c) < *(byte *)(*(int *)0x5398 + -0x6e84))) {
        iVar3 = *(int *)(uStack_6a * 0x13c + -0x77cc);
        if (((-1 < iVar3) && ((0 < iVar3 || (0x5db < *(uint *)(uStack_6a * 0x13c + -0x77ce))))) &&
           ((iVar3 = FUN_1000_86c4(0x181f,0,4), iVar3 != 0 || (-1 < *(char *)(*(int *)0x8d4a + 5))))
           ) {
          uStack_56 = 7;
          break;
        }
      }
      if (*(char *)(*(int *)0x8d4a + 5) < '\0') {
        uStack_56 = 3;
      } else {
        uVar10 = CONCAT22(uStack_5e._2_2_,(undefined2)uStack_5e);
        if ((*(byte *)(*(int *)0x8d4a + 5) & 0xf) == uStack_6a)
        goto switchD_004bdb_default;
        uStack_56 = 4;
      }
      break;
    case 5:
      uStack_56 = 6;
      break;
    case 0xc:
      uStack_56 = 1;
    }
  }
  uVar10 = CONCAT22(uStack_5e._2_2_,(undefined2)uStack_5e);
  switch(uStack_56) {
  case 1:
    thunk_FUN_1000_a63c(param_2,uStack_64,uStack_6a,uStack_4);
    uVar10 = CONCAT22(uStack_5e._2_2_,(undefined2)uStack_5e);
    break;
  case 2:
    iVar3 = thunk_FUN_1000_a5e8(0,param_2,uStack_64,uStack_6a,uStack_4);
    goto LAB_004b3f;
  case 3:
    iStack_5a = 2;
    thunk_FUN_1000_a5dc(param_2,uStack_6a,uStack_4,iVar7,uVar2);
    uVar10 = CONCAT22(uStack_5e._2_2_,(undefined2)uStack_5e);
    break;
  case 4:
    thunk_FUN_1000_a594(param_2,uStack_6a,uStack_4,iVar7,uVar2);
    uVar10 = CONCAT22(uStack_5e._2_2_,(undefined2)uStack_5e);
    break;
  case 5:
    thunk_FUN_1000_a618(0,param_2,uStack_6a,uStack_4,0);
    uVar10 = CONCAT22(uStack_5e._2_2_,(undefined2)uStack_5e);
    break;
  case 6:
    iVar3 = thunk_FUN_1000_a60c(param_2,uStack_6a,uStack_4);
LAB_004b3f:
    uVar10 = CONCAT22(uStack_5e._2_2_,(undefined2)uStack_5e);
    if (iVar3 != 0) {
      iStack_5a = 2;
      uVar10 = CONCAT22(uStack_5e._2_2_,(undefined2)uStack_5e);
    }
    break;
  case 7:
    thunk_FUN_1000_a5b8(0,param_2);
    uVar10 = CONCAT22(uStack_5e._2_2_,(undefined2)uStack_5e);
    break;
  case 8:
    thunk_FUN_1000_a5f4(param_2,uStack_6a,uStack_4);
    uVar10 = CONCAT22(uStack_5e._2_2_,(undefined2)uStack_5e);
    break;
  case 9:
    FUN_1000_8bf6(uStack_6a,uStack_4,4);
    iStack_5a = 0;
    uVar10 = CONCAT22(uStack_5e._2_2_,(undefined2)uStack_5e);
  }
switchD_004bdb_default:
  uStack_5e = uVar10;
  FUN_1000_900c(1);
  if (iStack_58 != *(int *)0x539c) {
    return 2;
  }
  if (iStack_5a == 1) {
    FUN_1000_8b24(0x181f,param_2);
  }
  return iStack_5a;
}
```

`thunk_FUN_1000_a5xx`/`a6xx` calls are this segment's own local RTLink thunk
stubs (5-byte `JMPF` trampolines at `OVL13_L0000::4c22`+, right after this
function) into resident dispatch handlers — not further mystery functions,
just the normal overlay-to-resident call mechanism (see
`docs/rtlink_decode_v2_gap.md`). `unaff_DS`-style near-data refs (none
actually surfaced here — this function turned out to only use absolute/far
addressing) would be the one remaining pre-existing Ghidra limitation if
they had; they didn't block this recovery.

Reproduce: `tools/address_mapping.csv` → `FUN_4d56_4528` → `OVL13_L0000:4528`
→ open `~/projects/ghidra_overlay_scratch/OverlayTest.gpr`, create a function
at that address (Ghidra didn't auto-create one — cross-overlay call target,
same reason covered in `docs/rtlink_decode_v2_gap.md`), decompile.

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
