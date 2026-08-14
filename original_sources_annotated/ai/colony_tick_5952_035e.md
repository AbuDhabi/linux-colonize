# Colony per-turn tick (`FUN_5952_035e`) — clean recovery

`FUN_5952_035e` is the **colony production/buildings/AI-hint tick**, cited as
the source of truth in `save_format_map.md` and `colony.h` for 8 already-
`mapped` fields: `garrison_quota` (+0x1e), `specialty_cargo` (+0x8d),
`cargo_idle_turns` (+0x8f), `improve_timer` (+0x8c), `labor_shortage` (+0x8e),
`warehouse_level` (+0x95), `capitol_level` (+0x96), plus the `ai_flags`/
`build_ai_flags` bit writes (+0x1b/+0x1d).

## Corruption found and fixed (2026-08-14)

The canonical export (`viceroy_unpacked.c:93790`) carries the exact
disassembly-fault signature already known from `4528`/`1816`/etc:
`// WARNING: Instruction at (ram,0x0005a676) overlaps instruction at
(ram,0x0005a675)` + 2× `Removing unreachable block`, immediately above the
declaration. Its recovered signature (4 params) doesn't even match this
clean recovery's real one (2 params) — a real mismatch, not just noise.

Re-disassembled via the overlay-addressing project (`docs/rtlink_decode_v2_gap.md`,
`address_mapping.csv` → `OVL15_L0000:35e`): **complete, zero-warning,
1577-line recovery below.** Renamed `FUN_OVL15_L0000__00035e` →
`FUN_5952_035e` (its canonical name) for readability; no other edits.

**Self-correction this same pass:** an earlier draft of `move_scoring_land.md`
claimed a read site for DS `0x94e6` (`= −0x6b1a` in `FUN_521d_20e6`) "inside
`FUN_5952_035e` itself" — traced to canonical-export line ~95043-95062, past
this real function's true end. That line range belongs to the *corrupted*
export's garbled tail, not to this function at all: **the clean 1577-line
body never references `0x94e6`, `0x9572`, `−0x6b1a`, or `−0x6a8e` anywhere.**
Corrected in `move_scoring_land.md` and `save_format_map.md` — the real
owner of that code is still unidentified.

## Confirms (not corrects) the 8 already-mapped fields

Spot-checked against `save_format_map.md`'s existing claims — all consistent,
no discrepancies found:

- `+0x1e` (`garrison_quota`) = `threat >> 3` (`iStack_9a >> 3`, the function's
  own local threat accumulator) — exact match.
- `+0x8c`/`+0x8f` (`improve_timer`/`cargo_idle_turns`): both INC, both capped
  at `0x7f` (`< '\x7f'` gate) — exact match; both cleared (`= 0`) at several
  points downstream (construction-queue-done-shaped conditions).
- `+0x8e` (`labor_shortage`): written from the garrison-threat calculation
  (`iStack_76`), read back for a defense-quota comparison — matches role.
  `+0x8d` (`specialty_cargo`): compared against small cargo-type constants
  (`0x0e`/`0x0f`) in a construction-pick branch — matches "index, 0xff none"
  role (0xff never appears as a literal compare here, consistent with the
  existing "not this value" framing).
- `+0x95`/`+0x96` (`warehouse_level`/`capitol_level`): both read into
  construction-scoring formulas — matches "level, used in production math"
  role.
- `+0x1b`/`+0x1c`/`+0x1d` (`ai_flags`/`colony_flags`/`build_ai_flags`): bit
  writes throughout (garrison bit `0x40`, starvation-shaped `0xa0`/`0x80`,
  `build_ai_flags` bit7 `0x80` wants-construction gate at the very end) —
  match existing bit-role documentation.

## Decompiler-noise caveat

`*(undefined2 *)(code *)FUN_5952_035e = 1;` / `= 0;` appears twice (function
entry-ish and just before `return`) — this is **not** a real self-write.
Ghidra's decompiler occasionally renders an unresolved far-call return-value
write as a dereference through the containing function's own symbol when it
can't otherwise name the target address; same noise class as the project's
existing `unaff_DS`/`unaff_CS` flood. Read both as "set/clear an internal
result flag," not as literal self-modifying code.

## Not attempted this pass

This function is now clean and available, but a full field-by-field semantic
audit (every one of its ~40 locals, its construction-priority scoring
formula, its `aiStack_68`/`aiStack_12e` build-queue bookkeeping) was **not**
done — only the 8 already-mapped fields were spot-checked. Real next step for
whoever continues: trace the still-unnamed globals this function touches
(`0x8dc6`, `0x8d72`, `0x8e5a`, `0x8e64`, `0x8de4`/`0x8dd4`, the `-0x71xx`/
`-0x72xx`/`-0x7bxx` construction-cost/priority tables) — none attempted here.

## Raw recovered C

```c
void FUN_5952_035e(undefined2 param_1,undefined2 param_2)

{
  char *pcVar1;
  byte *pbVar2;
  int *piVar3;
  uint *puVar4;
  byte bVar5;
  char *pcVar6;
  byte *pbVar7;
  char cVar8;
  byte bVar9;
  undefined1 uVar10;
  int iVar11;
  int iVar12;
  int iVar13;
  uint uVar14;
  uint uVar15;
  uint uVar16;
  undefined2 uVar17;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  bool bVar18;
  undefined2 uVar19;
  uint uVar20;
  undefined2 uVar21;
  undefined2 uStack_1c2;
  undefined2 uStack_1c0;
  undefined2 uStack_1be;
  uint uStack_1bc;
  int iStack_1b6;
  int iStack_1b4;
  uint uStack_1b0;
  int aiStack_1ae [32];
  uint uStack_16e;
  int iStack_16c;
  int aiStack_16a [19];
  uint uStack_144;
  int iStack_142;
  int iStack_140;
  uint uStack_13e;
  int iStack_13c;
  int iStack_13a;
  int iStack_138;
  int iStack_136;
  int iStack_134;
  uint uStack_132;
  uint uStack_130;
  int aiStack_12e [32];
  int iStack_ee;
  uint uStack_ec;
  uint uStack_ea;
  int iStack_e8;
  int iStack_e6;
  int aiStack_e4 [13];
  int iStack_ca;
  int iStack_c4;
  undefined2 uStack_b2;
  int iStack_b0;
  uint uStack_ae;
  int iStack_ac;
  int iStack_aa;
  uint uStack_a8;
  uint uStack_a6;
  uint uStack_a4;
  uint uStack_a2;
  int iStack_a0;
  int iStack_9e;
  uint uStack_9c;
  int iStack_9a;
  int iStack_98;
  uint uStack_96;
  int iStack_94;
  int iStack_92;
  int iStack_90;
  int iStack_8e;
  int iStack_8c;
  int iStack_8a;
  uint uStack_88;
  uint uStack_86;
  uint uStack_84;
  int iStack_82;
  uint uStack_80;
  int iStack_7e;
  int iStack_7c;
  int iStack_7a;
  int iStack_78;
  int iStack_76;
  int iStack_74;
  int iStack_72;
  int iStack_70;
  int iStack_6e;
  int iStack_6c;
  int iStack_6a;
  int aiStack_68 [13];
  int iStack_4e;
  int iStack_4a;
  int iStack_48;
  int iStack_42;
  int iStack_3e;
  int iStack_36;
  int iStack_34;
  int iStack_32;
  int iStack_30;
  int iStack_2e;
  int iStack_2c;
  uint uStack_2a;
  int iStack_28;
  int iStack_26;
  undefined2 uStack_24;
  int iStack_22;
  int iStack_20;
  int iStack_1e;
  int iStack_1c;
  int iStack_1a;
  int iStack_18;
  uint uStack_16;
  int iStack_14;
  int iStack_12;
  int iStack_10;
  int iStack_e;
  int iStack_c;
  int iStack_a;
  int iStack_8;
  int iStack_6;
  undefined2 uStack_4;
  
  uStack_24 = 0;
  iStack_8c = 0;
  iStack_82 = 0;
  iStack_92 = 0;
  FUN_1000_8bd6();
  pcVar6 = (char *)*(undefined2 *)0x8542;
  if ((*pcVar6 == '.') && (pcVar6[1] == '\x14')) {
    uStack_4 = 1;
  }
  else {
    uStack_4 = 0;
  }
  FUN_0000_df7e(pcVar6 + 0x8a,0,2,param_2);
  FUN_1000_8e62(0xd1d);
  FUN_1000_8e12();
  iStack_36 = FUN_1000_8f2a(0x181f);
  iStack_70 = FUN_1000_8e4e(0x181f);
  uStack_a2 = (uint)*(byte *)(iStack_70 + 0x329);
  if (*(char *)(*(int *)0x8542 + 0x8c) < '\x7f') {
    pcVar1 = (char *)(*(int *)0x8542 + 0x8c);
    *pcVar1 = *pcVar1 + '\x01';
  }
  if (*(char *)(*(int *)0x8542 + 0x8f) < '\x7f') {
    pcVar1 = (char *)(*(int *)0x8542 + 0x8f);
    *pcVar1 = *pcVar1 + '\x01';
  }
  pbVar7 = (byte *)*(undefined2 *)0x8542;
  uStack_16 = (uint)(0x13 < *(int *)(pbVar7 + 0xb6));
  uStack_a6 = (uint)*pbVar7;
  uStack_ae = (uint)pbVar7[1];
  bVar9 = pbVar7[0x1a];
  uStack_1b0 = (uint)bVar9;
  iStack_6c = FUN_1000_8912(0x181f,uStack_a6,uStack_ae);
  uStack_2a = (uint)(byte)((undefined1 *)&LAB_0000_9870)[uStack_1b0 * 0x10 + iStack_6c];
  iStack_76 = 0;
  iStack_9a = 0;
  iStack_22 = 0;
  iStack_b0 = FUN_1000_8e76(0x181f);
  iStack_7c = ((int)*(char *)(*(int *)0x8542 + 0x1f) * (100 - iStack_b0) + 0x32) / 100;
  if ((*(byte *)0x5382 & 1) != 0) {
    iStack_7c = 0;
  }
  iStack_e8 = FUN_1000_8f74(uStack_a6,uStack_ae,0xffff,iStack_6c);
  uStack_b2 = *(undefined2 *)0x8db8;
  if (-1 < iStack_e8) {
    FUN_1000_84fc(0x181f,*(undefined2 *)0x8d52,uStack_1b0);
  }
  for (uStack_130 = 0xfffb; (int)uStack_130 < 6; uStack_130 = uStack_130 + 1) {
    for (uStack_a4 = 0xfffb; (int)uStack_a4 < 6; uStack_a4 = uStack_a4 + 1) {
      iStack_28 = uStack_130 + uStack_ae;
      iStack_12 = uStack_a4 + uStack_a6;
      iVar11 = FUN_1000_84f2(0x181f,iStack_12,iStack_28);
      if (((iVar11 != 0) && (uStack_13e = FUN_1000_89d0(0x181f), -1 < (int)uStack_13e)) &&
         ((*(byte *)(uStack_13e * 0x1c + 0x3147) & 0xf) != bVar9)) {
        while (-1 < (int)uStack_13e) {
          iStack_98 = func_0x00018bb8(0x181f,uStack_13e,1);
          if ((*(byte *)(uStack_13e * 0x1c + 0x3146) < 0xd) ||
             (0x12 < *(byte *)(uStack_13e * 0x1c + 0x3146))) {
            if ((*(byte *)(uStack_13e * 0x1c + 0x3147) & 0xf) < 4) {
              if (iStack_98 < 2) {
                iStack_98 = 0;
              }
              bVar5 = *(byte *)(uStack_13e * 0x1c + 0x3147);
              if (((bVar5 & 0xf) < 4) && (*(char *)((bVar5 & 0xf) * 0x34 + 0x543f) == '\0')) {
                iStack_98 = iStack_98 + (iStack_98 >> 1);
              }
              if (iStack_98 != 0) {
                uVar14 = uStack_a4;
                if ((int)uStack_a4 < 1) {
                  uVar14 = ~uStack_a4 + 1;
                }
                if ((int)uVar14 < 2) {
                  uVar14 = uStack_130;
                  if ((int)uStack_130 < 1) {
                    uVar14 = ~uStack_130 + 1;
                  }
                  if ((int)uVar14 < 2) {
                    iStack_22 = iStack_22 + 1;
                  }
                }
              }
            }
            else {
              iVar11 = FUN_1000_84fc(0x181f,(*(byte *)(uStack_13e * 0x1c + 0x3147) & 0xf) - 4,
                                     uStack_1b0);
              if (iVar11 < 0x19) {
                iStack_98 = 0;
              }
              if (*(int *)((*(char *)(uStack_13e * 0x1c + 0x314a) * 9 + uStack_1b0) * 2 + 0x54f6) <
                  0x80) {
                iStack_98 = 0;
              }
            }
            iVar11 = FUN_1000_88ae(0x181f,iStack_12,iStack_28);
            if (-1 < iVar11) {
              iStack_98 = iStack_98 >> 1;
            }
            iVar11 = FUN_1000_8560(0x181f,uStack_a4,uStack_130);
            iStack_98 = -(iVar11 + -8) * iStack_98 >> 3;
            iStack_9a = iStack_9a + iStack_98;
          }
          uStack_13e = FUN_1000_84d4(0x181f);
        }
      }
    }
  }
  iStack_1c = iStack_9a;
  if (0x10 < iStack_9a) {
    iStack_1c = 0x10;
  }
  iVar11 = FUN_1000_8ca0(0x181f,0);
  iStack_9a = iStack_9a / (iVar11 + 1);
  if (iStack_9a < iStack_1c) {
    iStack_9a = iStack_1c;
  }
  iVar12 = iStack_9a >> 3;
  iVar11 = *(int *)0x8542;
  *(undefined1 *)(iVar11 + 0x1e) = (char)iVar12;
  iVar11 = (int)*(char *)(iVar11 + 0x1f) + *(int *)0x8d72;
  iVar13 = iVar11 >> 1;
  iStack_76 = (iVar11 + -1) / 2;
  if (iStack_76 < iVar12) {
    iStack_76 = iVar12;
  }
  if (iVar13 < iStack_76) {
    iStack_76 = iVar13;
  }
  if ((*(byte *)0x5382 & 1) != 0) {
    iStack_76 = iStack_76 + 1;
  }
  if (((iStack_22 != 0) && (1 < (int)*(char *)(*(int *)0x8542 + 0x1f) + *(int *)0x8d72)) &&
     (iStack_76 < 1)) {
    iStack_76 = 1;
  }
  *(undefined1 *)(*(int *)0x8542 + 0x8e) = (undefined1)iStack_76;
  uVar21 = 0x728;
  uStack_13e = FUN_1000_89d0(0x181f);
  while (-1 < (int)uStack_13e) {
    if ((*(byte *)(uStack_13e * 0x1c + 0x3146) < 0xd) ||
       (0x12 < *(byte *)(uStack_13e * 0x1c + 0x3146))) {
      if (*(char *)(uStack_13e * 0x1c + 0x314a) < '\0') {
        *(undefined1 *)(uStack_13e * 0x1c + 0x314a) = *(undefined1 *)0x8dc6;
      }
      if ((1 < *(byte *)((uint)*(byte *)(uStack_13e * 0x1c + 0x3146) * 0xe + 0x5236)) &&
         (iStack_76 != 0)) {
        iStack_76 = iStack_76 + -1;
      }
    }
    uStack_13e = FUN_1000_84d4(uVar21,0x181f);
  }
  for (uStack_13e = 0; (int)uStack_13e < *(int *)0x539c; uStack_13e = uStack_13e + 1) {
    if (((((*(byte *)(uStack_13e * 0x1c + 0x3147) & 0xf) == bVar9) &&
         (*(char *)(uStack_13e * 0x1c + 0x314a) == *(char *)0x8dc6)) &&
        ((*(byte *)(uStack_13e * 0x1c + 0x3146) < 0xd ||
         (0x12 < *(byte *)(uStack_13e * 0x1c + 0x3146))))) &&
       (1 < *(byte *)((uint)*(byte *)(uStack_13e * 0x1c + 0x3146) * 0xe + 0x5236))) {
      iStack_82 = iStack_82 + 1;
    }
  }
  iStack_82 = iStack_82 - (*(char *)(*(int *)0x8542 + 0x8e) - iStack_76);
  uStack_144 = 0;
  iStack_142 = 0;
  iStack_1a = 0;
  iStack_aa = 0;
  iStack_134 = 0;
  iStack_6e = 0;
  iStack_e = 0;
  iStack_c = 0;
  iStack_26 = 0;
  iStack_140 = 0;
  for (iStack_ac = 0; iStack_ac < (int)uStack_a2; iStack_ac = iStack_ac + 1) {
    uStack_ae = (int)*(char *)(iStack_ac + 0xde) + (uint)((byte *)*(undefined2 *)0x8542)[1];
    uStack_a6 = (int)*(char *)(iStack_ac + 200) + (uint)*(byte *)*(undefined2 *)0x8542;
    iVar11 = FUN_1000_84f2(0x181f,uStack_a6,uStack_ae);
    if (iVar11 == 0) {
      iStack_142 = iStack_142 + 1;
      uStack_144 = uStack_144 + 1;
    }
    else {
      uStack_ea = func_0x0001897c(0x181f,uStack_a6,uStack_ae);
      if ((uStack_ea == 0x19) || (uStack_ea == 0x1a)) {
        uStack_144 = uStack_144 + 1;
        iStack_1a = iStack_1a + 1;
        iStack_142 = iStack_142 + 1;
        iStack_e = iStack_e + 1;
      }
      if ((((int)uStack_ea < 8) || (0xf < (int)uStack_ea)) &&
         (((int)uStack_ea < 0x10 || (0x17 < (int)uStack_ea)))) {
        if (*(byte *)(uStack_ea * 0x10 + 0x2f7b) < 3) {
          if (*(byte *)(uStack_ea * 0x10 + 0x2f7b) < 2) {
            uStack_144 = uStack_144 + 1;
          }
        }
        else {
          iStack_e = iStack_e + 1;
        }
      }
      else {
        iStack_134 = iStack_134 + 1;
        uStack_144 = uStack_144 + 1;
        if (2 < *(byte *)((uStack_ea & 7) * 0x10 + 0x2f7b)) {
          iStack_c = iStack_c + 1;
        }
      }
      uStack_86 = FUN_1000_8944(0x181f,uStack_a6,uStack_ae);
      uStack_86 = uStack_86 & 10;
      if (-1 < *(char *)(*(int *)0x8542 + iStack_ac + 0x70)) {
        if (uStack_86 == 0) {
          iStack_6e = iStack_6e + 1;
        }
        else {
          iStack_aa = iStack_aa + 1;
        }
        if ((int)uStack_ea < 8) {
          uVar14 = FUN_1000_8944(0x181f,uStack_a6,uStack_ae);
          if ((uVar14 & 0x40) == 0) {
            iStack_140 = iStack_140 + 1;
          }
          else {
            iStack_26 = iStack_26 + 1;
          }
        }
      }
    }
  }
  iVar11 = FUN_1000_8bec(0x181f,6);
  if (iVar11 != 0) {
    iStack_142 = 0;
  }
  iVar11 = *(int *)0x8542;
  *(byte *)(iVar11 + 0x1b) = *(byte *)(iVar11 + 0x1b) & 7;
  if (((*(byte *)(iVar11 + 0x1c) & 0x10) != 0) && (*(char *)(iVar11 + 0x1f) < ' ')) {
    *(byte *)(iVar11 + 0x1b) = *(byte *)(iVar11 + 0x1b) | 0x10;
    *(byte *)(iVar11 + 0x1c) = *(byte *)(iVar11 + 0x1c) & 0xef;
  }
  if (0 < iStack_76) {
    *(byte *)(*(int *)0x8542 + 0x1b) = *(byte *)(*(int *)0x8542 + 0x1b) | 0x40;
  }
  iVar11 = (int)*(char *)(uStack_1b0 * 3 + -0x6a98);
  uStack_84 = ((((int)*(char *)(*(int *)0x8542 + 0x1f) + *(int *)0x8d72) * 3 >> 1) - iVar11) -
              (*(int *)0x538e >> 7);
  uStack_1bc = iVar11 + 5;
  if (iStack_76 != 0) {
    uStack_1bc = iVar11 + 6;
  }
  if (uStack_2a == 4) {
    uStack_1bc = uStack_1bc + -1;
  }
  if (uStack_2a == 0) {
    uStack_84 = uStack_84 + 2;
  }
  if (uStack_2a == 3) {
    uStack_84 = uStack_84 + 1;
  }
  iStack_74 = (int)uStack_84 / (int)uStack_1bc;
  if ((*(char *)(iStack_6c + -0x6a0e) == '\0') && (uStack_2a != 0)) {
    iStack_74 = 0;
  }
  if ((*(byte *)(iStack_6c + -0x6a0e) & 1) != 0) {
    if (((*(byte *)(iStack_6c + -0x6a0e) & 6) == 0) || (uStack_1b0 == 2)) {
      if (1 < *(byte *)(iStack_6c + uStack_1b0 * 0x10 + -0x6a4e)) {
        iVar11 = *(int *)(uStack_1b0 * 2 + -0x6be4);
        uStack_84 = iVar11 << 1;
        if (uStack_1b0 == 2) {
          uStack_84 = iVar11 << 2;
        }
        uVar14 = (uint)*(byte *)(iStack_6c + uStack_1b0 * 0x10 + -0x6a4e);
        uStack_9c = uVar14 << 2;
        if (uStack_1b0 == 2) {
          uStack_9c = uVar14 << 3;
        }
        iVar11 = *(int *)0x8d52;
        if ((((int)(uint)*(byte *)(iVar11 + -0x6e7c) <= (int)uStack_84) &&
            (*(byte *)(iStack_6c + iVar11 * 0x10 + -0x6e34) < uStack_9c)) &&
           ((iVar11 = FUN_1000_84fc(0x181f,iVar11,uStack_1b0), 0x19 < iVar11 || (uStack_1b0 == 2))))
        {
          FUN_1000_8bf6(uStack_1b0,*(undefined2 *)(code *)FUN_0000_8d50,2);
        }
      }
    }
    else if (1 < iStack_74) {
      iStack_74 = 1;
    }
  }
  if (iStack_82 < iStack_74) {
    *(byte *)(*(int *)0x8542 + 0x1b) = *(byte *)(*(int *)0x8542 + 0x1b) | 8;
  }
  if ((int)((uint)(1 < iStack_74) + iStack_74) < iStack_82) {
    *(byte *)(*(int *)0x8542 + 0x1b) = *(byte *)(*(int *)0x8542 + 0x1b) | 4;
  }
  if (((int)(uStack_a2 - 1) <= (int)uStack_144) && (1 < iStack_134)) {
    *(byte *)(*(int *)0x8542 + 0x1b) = *(byte *)(*(int *)0x8542 + 0x1b) | 0xa0;
  }
  if (((iStack_e < *(char *)(*(int *)0x8542 + 0x1f) + 3 >> 2) && (iStack_c != 0)) &&
     (1 < iStack_134)) {
    pbVar2 = (byte *)(*(int *)0x8542 + 0x1b);
    *pbVar2 = *pbVar2 | 0xa0;
  }
  if ((iStack_6e != 0) || (iStack_140 != 0)) {
    *(byte *)(*(int *)0x8542 + 0x1b) = *(byte *)(*(int *)0x8542 + 0x1b) | 0x80;
  }
  iStack_10 = func_0x0001a684(0x181f,*(undefined1 *)(*(int *)0x8542 + 0x1a));
  if (*(char *)(*(int *)0x8542 + 0x1f) < ' ') {
    cVar8 = FUN_1000_8e6c(0x1a1f);
    iVar11 = *(int *)0x8542;
    if ((*(char *)(iVar11 + 0x1f) < (char)(cVar8 + (char)iStack_70 * '\x02')) &&
       ((int)*(char *)(iVar11 + 0x1f) + iStack_70 * -2 < (int)(uStack_a2 - iStack_142))) {
      *(byte *)(iVar11 + 0x1b) = *(byte *)(iVar11 + 0x1b) | 0x10;
    }
  }
  FUN_0000_df7e(aiStack_68,0,0x32);
  iVar11 = 0xd1d;
  FUN_0000_df7e(aiStack_e4,0,0x32);
  for (iStack_ac = 0; iStack_ac < *(char *)(*(int *)0x8542 + 0x1f); iStack_ac = iStack_ac + 1) {
    iStack_18 = FUN_1000_8e44(iVar11,iStack_ac);
    iVar11 = 0x181f;
    iVar12 = FUN_1000_8e8a(0x181f,iStack_18);
    if (iVar12 == 0) {
      iStack_18 = 0x13;
    }
    aiStack_68[iStack_18] = aiStack_68[iStack_18] + 1;
  }
  FUN_1000_8ebc(iVar11);
  if ((*(int *)0x8d72 != 0) && ((*(byte *)(*(int *)0x8542 + 0x1b) & 0x10) != 0)) {
    do {
      iStack_32 = 0;
      iStack_ac = (int)*(char *)(*(int *)0x8542 + 0x1f);
      while (((iStack_32 == 0 &&
              (iStack_ac < (int)*(char *)(*(int *)0x8542 + 0x1f) + *(int *)0x8d72)) &&
             (*(char *)(*(int *)0x8542 + 0x1f) < ' '))) {
        iStack_ee = FUN_1000_8dfe(0x181f,iStack_ac);
        iStack_18 = FUN_1000_8e44(0x181f,iStack_ac);
        if (((iStack_ee == 0x15) || (iStack_ee == 0x17)) &&
           ((((iStack_76 < 0 && ((*(byte *)(*(int *)0x8542 + 0x1b) & 8) == 0)) ||
             (((iVar12 = FUN_1000_8e8a(0x181f,iStack_18), iVar12 != 0 && (iStack_18 != 0x15)) &&
              ((iStack_42 != 0 || (iStack_3e != 0)))))) ||
            ((*(byte *)(*(int *)0x8542 + 0x1b) & 4) != 0)))) {
          FUN_1000_8e26(0x181f,iStack_ac,0x12);
          iStack_76 = iStack_76 + 1;
          iStack_32 = 1;
          *(byte *)(*(int *)0x8542 + 0x1b) = *(byte *)(*(int *)0x8542 + 0x1b) & 0xfb;
          if (iStack_3e == 0) {
            if (iStack_42 != 0) {
              iStack_42 = iStack_42 + -1;
            }
          }
          else {
            iStack_3e = iStack_3e + -1;
          }
        }
        if ((iStack_ee == 0x14) &&
           ((((*(byte *)(*(int *)0x8542 + 0x1b) & 0x80) != 0 && (uStack_16 == 0)) ||
            (uStack_2a == 0)))) {
          FUN_1000_8e26(0x181f,iStack_ac,0x12);
          uStack_16 = 1;
          iStack_32 = 1;
        }
        if ((iStack_ee == 0x16) && ((uStack_2a == 0 || (*(int *)(*(int *)0x8542 + 0xaa) < 0x34)))) {
          FUN_1000_8e26(0x181f,iStack_ac,0x12);
          iStack_32 = 1;
        }
        if (iStack_ee == 0x13) {
          FUN_1000_8e26(0x181f,iStack_ac,0x12);
          iStack_32 = 1;
        }
        iStack_ac = iStack_ac + 1;
      }
    } while (iStack_32 != 0);
  }
  iVar12 = *(int *)0x8542;
  if ('\x01' < *(char *)(iVar12 + 0x1f)) {
    iStack_8e = 0x13;
    iStack_136 = 0;
    if (0x65 < *(int *)(iVar12 + 0xaa)) {
      if (*(char *)(iVar12 + 0x1f) < '\n') {
        cVar8 = FUN_1000_8e6c(0x181f);
        iVar12 = *(int *)0x8542;
        if (*(char *)(iVar12 + 0x1f) < cVar8) goto LAB_OVL15_L0000__000de5;
      }
      if ((*(byte *)(iVar12 + 0x1b) & 0x10) == 0) {
        iStack_8e = 0x16;
        iStack_136 = 1;
      }
    }
LAB_OVL15_L0000__000de5:
    if (((uStack_2a == 0) && ('\n' < *(char *)(iVar12 + 0x1f))) &&
       ((iVar12 = FUN_1000_86c4(0x181f,0,3), iVar12 == 0 &&
        ((*(byte *)(*(int *)0x8542 + 0x1b) & 0x10) == 0)))) {
      iStack_90 = 1;
    }
    else {
      iStack_90 = 0;
    }
    if ((((iStack_90 != 0) && (*(char *)(uStack_1b0 * 0x13 + -0x6db2) == '\0')) && (iStack_10 != 0))
       && (0x13 < *(int *)(*(int *)0x8542 + 0xb6))) {
      iStack_8e = 0x14;
      iStack_136 = 1;
    }
    iVar12 = *(int *)0x8542;
    if ((((*(byte *)(iVar12 + 0x1b) & 0x48) != 0) || (iStack_90 != 0)) &&
       (0x31 < *(int *)(iVar12 + 0xb8))) {
      iStack_8e = 0x15;
      if (0x33 < *(int *)(iVar12 + 0xaa)) {
        iStack_8e = 0x17;
      }
      iStack_136 = 1;
    }
    if (iStack_136 != 0) {
      iStack_1b4 = iStack_8e;
      if (iStack_8e == 0x17) {
        iStack_1b4 = 0x15;
      }
      uStack_16e = 0xffff;
      iStack_34 = -1;
      for (iStack_ac = 0; iStack_ac < *(char *)(*(int *)0x8542 + 0x1f); iStack_ac = iStack_ac + 1) {
        uStack_144 = 0;
        iStack_18 = FUN_1000_8e44(0x181f,iStack_ac);
        if (iStack_18 == iStack_1b4) {
          uStack_144 = 4;
        }
        else {
          iVar12 = FUN_1000_8e8a(0x181f,iStack_18);
          if (iVar12 == 0) {
            uStack_144 = uStack_144 + 1;
          }
          else if ((iStack_1b4 == 0x15) && ((*(byte *)(*(int *)0x8542 + 0x1b) & 0x40) == 0)) {
            uStack_144 = 0xff9d;
          }
        }
        if (iStack_18 == 0x19) {
          uStack_144 = uStack_144 + 1;
        }
        if (iStack_18 == 0x1a) {
          uStack_144 = uStack_144 + 2;
        }
        if ((iStack_18 != 0x1b) && ((int)uStack_16e <= (int)uStack_144)) {
          uStack_16e = uStack_144;
          iStack_34 = iStack_ac;
        }
      }
      if (-1 < iStack_34) {
        iVar11 = FUN_1000_8e8a(0x181f,iStack_18);
        if ((iVar11 != 0) && (iStack_1b4 != iStack_18)) {
          FUN_1000_8e9e(0x181f,iStack_34,0x1c);
        }
        iVar11 = iStack_8e;
        FUN_1000_8e26(0x181f,iStack_34,iStack_8e);
      }
    }
  }
  uVar21 = 0x181f;
  FUN_1000_8ebc(0x181f);
  iVar12 = (*(char *)(uStack_1b0 * 3 + -0x6a9a) + 2) * 0x32;
  FUN_OVL15_L0000__002a82
            (0x181f,0xf,
             iVar12 - *(int *)(*(int *)0x8542 + 0xb8) != 0 &&
             *(int *)(*(int *)0x8542 + 0xb8) <= iVar12);
  iVar12 = *(int *)0x8542;
  uStack_96 = (uint)(*(char *)(iVar12 + 0x8d) == '\x0f');
  if (((*(int *)(iVar12 + 0xb4) < 100) && (*(byte *)(uStack_1b0 * 0x10 + -0x7b37) < 4)) &&
     ((*(byte *)(iVar12 + 0x1c) & 0x20) != 0)) {
    uStack_1c0 = 1;
  }
  else {
    uStack_1c0 = 0;
  }
  FUN_OVL15_L0000__002a82(0x181f,0xd,uStack_1c0);
  FUN_OVL15_L0000__002a82(0x181f,8,*(int *)(*(int *)0x8542 + 0xaa) < 0x32);
  if ((uStack_16 == 0) && ((*(byte *)(*(int *)0x8542 + 0x1b) & 0x80) != 0)) {
    uStack_1be = 1;
  }
  else {
    uStack_1be = 0;
  }
  FUN_OVL15_L0000__002a82(0x181f,0xe,uStack_1be);
  if (((((iStack_76 < 1) || (0x31 < *(int *)(*(int *)0x8542 + 0xb8))) &&
       ((iVar12 = *(int *)0x8542, *(char *)(iVar12 + 0x8e) != '\x01' ||
        ((0x31 < *(int *)(iVar12 + 0xb8) || (*(char *)(iVar12 + 0x8d) == '\x0e')))))) &&
      (((*(byte *)(iVar12 + 0x1b) & 8) == 0 ||
       ((0x31 < *(int *)(iVar12 + 0xb8) || (*(char *)(iVar12 + 0x8d) == '\x0e')))))) &&
     ((uStack_96 == 0 || (*(char *)(iVar12 + 0x8d) != '\x0f')))) {
    uStack_1c2 = 0;
  }
  else {
    uStack_1c2 = 1;
  }
  FUN_OVL15_L0000__002a82(0x181f,0xf,uStack_1c2);
  if (((iStack_76 == 0) && (iVar12 = *(int *)0x8542, 199 < *(int *)(iVar12 + 0xb8))) &&
     (*(byte *)(*(int *)0x84fc + 0x49) < 0x14)) {
    pcVar1 = (char *)(*(int *)0x84fc + 0x49);
    *pcVar1 = *pcVar1 + '\x01';
    piVar3 = (int *)(iVar12 + 0xb8);
    *piVar3 = *piVar3 + -0x32;
  }
  *(undefined1 *)0x34c = 0;
  uVar19 = 0x181f;
  uVar17 = 0x181f;
  FUN_1000_8dc2(0x181f);
  if ((((*(byte *)(*(int *)0x8542 + 0x1b) & 0x80) != 0) || (*(int *)0x538e % 10 == 0)) &&
     (uStack_16 == 0)) {
    uVar14 = (uint)*(byte *)((uint)*(byte *)(*(int *)0x8542 + 0x1a) * 0x10 + -0x7b36);
    uStack_a8 = uVar14 * 0x14;
    iVar12 = *(int *)0x84fc;
    if ((-1 < *(int *)(iVar12 + 0x2c)) &&
       ((0 < *(int *)(iVar12 + 0x2c) ||
        (uStack_a8 < *(uint *)(iVar12 + 0x2a) || uStack_a8 - *(uint *)(iVar12 + 0x2a) == 0)))) {
      puVar4 = (uint *)(iVar12 + 0x2a);
      uVar15 = *puVar4;
      *puVar4 = *puVar4 + uVar14 * -0x14;
      *(int *)(iVar12 + 0x2c) = *(int *)(iVar12 + 0x2c) - (uint)(uVar15 < uStack_a8);
      uVar17 = 0x191f;
      FUN_1000_9e04(0xe,0x14,uVar19,uVar21,iVar11);
      *(int *)(*(int *)0x8542 + 0xb6) = *(int *)(*(int *)0x8542 + 0xb6) + 0x14;
      uStack_16 = 1;
    }
  }
  iStack_13c = 0;
  if (((uStack_16 != 0) && (*(int *)0x538e % 7 == 0)) &&
     (iVar11 = FUN_OVL15_L0000__002a7d(uVar17), iVar11 != 0)) {
    iVar11 = *(int *)0x8542;
    iVar12 = *(int *)(iVar11 + 0xb6);
    if (0x14 < iVar12) {
      iVar12 = 0x14;
    }
    *(int *)(iVar11 + 0xb6) = *(int *)(iVar11 + 0xb6) - iVar12;
    *(undefined1 *)(iVar11 + 0x8c) = 0;
    iStack_13c = 1;
  }
  if ((((*(byte *)(*(int *)0x8542 + 0x1b) & 0x80) != 0) && (uStack_16 != 0)) &&
     (*(int *)0x538e % 7 != 0)) {
    iStack_34 = -1;
    uStack_16e = 0xffff;
    for (iStack_7a = 0; iStack_7a < (int)uStack_a2; iStack_7a = iStack_7a + 1) {
      uStack_ae = (int)*(char *)(iStack_7a + 0xde) + (uint)((byte *)*(undefined2 *)0x8542)[1];
      iStack_12 = *(char *)(iStack_7a + 200) + 2;
      iStack_28 = *(char *)(iStack_7a + 0xde) + 2;
      uStack_a6 = (int)*(char *)(iStack_7a + 200) + (uint)*(byte *)*(undefined2 *)0x8542;
      iVar11 = FUN_1000_84f2(uVar17,uStack_a6,uStack_ae);
      if (((iVar11 != 0) && (iVar11 = FUN_1000_8958(0x181f,uStack_a6,uStack_ae), iVar11 == 0)) &&
         (*(char *)(iStack_28 + iStack_12 * 5 + -0x7210) == '\0')) {
        uStack_ea = func_0x0001897c(0x181f,uStack_a6,uStack_ae);
        iStack_9e = FUN_1000_8908(0x181f,uStack_a6,uStack_ae);
        bVar9 = *(byte *)(uStack_ea * 0x10 + 0x2f79);
        if (iStack_9e != -1) {
          bVar9 = *(byte *)(iStack_9e + -0x684e);
        }
        uStack_132 = (uint)bVar9;
        iVar11 = *(int *)0x8542;
        if ((((*(byte *)(iVar11 + 0x1b) & 0x20) == 0) || ((int)uStack_ea < 8)) ||
           (0x17 < (int)uStack_ea)) {
          if (-1 < *(char *)(iVar11 + iStack_7a + 0x70)) {
            iStack_ee = FUN_1000_8dfe(0x181f,(int)*(char *)(iVar11 + iStack_7a + 0x70));
            if (iStack_ee < 4) {
              uVar14 = FUN_1000_8944(0x181f,uStack_a6,uStack_ae);
              uVar14 = uVar14 & 0x40;
            }
            else {
              uVar14 = FUN_1000_8944(0x181f,uStack_a6,uStack_ae);
              uVar14 = uVar14 & 10;
            }
            if (uVar14 == 0) {
              uStack_132 = uStack_132 << 1;
            }
          }
          uVar14 = FUN_1000_8944(0x181f,uStack_a6,uStack_ae);
          if (((uVar14 & 10) != 0) &&
             (uVar14 = FUN_1000_8944(0x181f,uStack_a6,uStack_ae), (uVar14 & 0x40) != 0))
          goto LAB_OVL15_L0000__00122c;
        }
        else {
          uStack_132 = uStack_132 << 1;
        }
        iStack_6 = (int)*(char *)(iStack_28 + iStack_12 * 5 + -0x7262);
        if (-1 < iStack_6) {
          iVar11 = FUN_1000_84fc(0x181f,iStack_6,uStack_1b0);
          iStack_1b6 = -(iVar11 + -4);
          if (iStack_9e != -1) {
            iStack_1b6 = (iVar11 + -4) * -2;
          }
          if (((*(byte *)(iStack_6c + -0x6a0e) & 1) != 0) &&
             ((*(byte *)(*(int *)0x8542 + 0x1c) & 0x20) == 0)) {
            if ((*(byte *)(*(int *)0x8542 + 0x1b) & 0x20) == 0) goto LAB_OVL15_L0000__00122c;
            iStack_1b6 = iStack_1b6 << 1;
          }
          iVar11 = *(int *)(*(int *)0x84fc + 0x2c);
          if ((iVar11 < 1) && ((iVar11 < 0 || (*(uint *)(*(int *)0x84fc + 0x2a) < 2000)))) {
            iStack_1b6 = iStack_1b6 << 1;
          }
          else {
            iStack_1b6 = iStack_1b6 >> 1;
          }
          uStack_132 = uStack_132 - iStack_1b6;
        }
        if ((int)uStack_16e < (int)uStack_132) {
          uStack_16e = uStack_132;
          iStack_34 = iStack_7a;
        }
      }
LAB_OVL15_L0000__00122c:
      uVar17 = 0x181f;
    }
    if (-1 < iStack_34) {
      uStack_a6 = (int)*(char *)(iStack_34 + 200) + (uint)*(byte *)*(undefined2 *)0x8542;
      uStack_ae = (int)*(char *)(iStack_34 + 0xde) + (uint)((byte *)*(undefined2 *)0x8542)[1];
      iStack_8 = 1;
      iStack_7a = 0;
      do {
        iStack_28 = (int)*(char *)(iStack_7a + 0xbe) + uStack_ae;
        iStack_12 = (int)*(char *)(iStack_7a + 0xb4) + uStack_a6;
        uStack_1b0 = FUN_1000_88c2(uVar17,iStack_12,iStack_28);
        if (((-1 < (int)uStack_1b0) && ((int)uStack_1b0 < 4)) &&
           (*(char *)(uStack_1b0 * 0x34 + 0x543f) == '\0')) {
          iStack_8 = 0;
        }
        iStack_7a = iStack_7a + 1;
        uVar17 = 0x181f;
      } while (iStack_7a < 8);
      uVar17 = 0x181f;
      uVar14 = FUN_1000_88c2(0x181f,uStack_a6,uStack_ae);
      if (*(byte *)(*(int *)0x8542 + 0x1a) == uVar14) {
        iStack_8 = 1;
      }
      if (iStack_8 != 0) {
        uVar17 = 0x181f;
        uStack_ea = func_0x0001897c(0x181f,uStack_a6,uStack_ae);
        uVar14 = (uint)*(byte *)(uStack_ea * 0x10 + 0x2f78);
        uStack_144 = uVar14 + 2;
        if (((((int)uStack_ea < 8) || (0xf < (int)uStack_ea)) &&
            (((int)uStack_ea < 0x10 || (0x17 < (int)uStack_ea)))) ||
           ((*(byte *)(*(int *)0x8542 + 0x1b) & 0x20) == 0)) {
          iStack_2e = 0;
        }
        else {
          iStack_2e = 1;
        }
        if (iStack_2e != 0) {
          uStack_144 = uVar14 + 4;
        }
        if ((char)uStack_144 <= *(char *)(*(int *)0x8542 + 0x8c)) {
          uVar20 = 0;
          uVar15 = (uint)*(byte *)(*(int *)0x8542 + 0x1a);
          uVar21 = 0x191f;
          uVar14 = uStack_ae;
          uVar16 = func_0x00019c10(0x181f,*(undefined1 *)0x524e,uStack_a6,uStack_ae,uVar15,0);
          *(undefined1 *)(uVar16 * 0x1c + 0x315a) = 99;
          uStack_13e = uVar16;
          if (iStack_2e == 0) {
            iStack_ee = -1;
            if (-1 < *(char *)(*(int *)0x8542 + iStack_34 + 0x70)) {
              uVar20 = (uint)*(char *)(*(int *)0x8542 + iStack_34 + 0x70);
              uVar21 = 0x181f;
              iStack_ee = FUN_1000_8dfe(0x191f,uVar20);
            }
            uVar16 = uVar20;
            uVar17 = uVar21;
            if ((-1 < iStack_ee) && (iStack_ee < 4)) {
              uVar17 = 0x181f;
              uVar15 = uStack_ae;
              uVar14 = FUN_1000_8944(uVar21,uStack_a6,uStack_ae);
              if ((uVar14 & 0x40) != 0) goto LAB_OVL15_L0000__00155c;
LAB_OVL15_L0000__001556:
              uVar21 = 0x181f;
              uVar14 = uStack_13e;
              goto LAB_OVL15_L0000__001508;
            }
LAB_OVL15_L0000__00155c:
            uVar21 = uVar17;
            if (3 < iStack_ee) {
              uVar21 = 0x181f;
              uVar14 = FUN_1000_8944(uVar17,uStack_a6,uStack_ae);
              if ((uVar14 & 10) != 0) goto LAB_OVL15_L0000__001577;
LAB_OVL15_L0000__0015b0:
              FUN_1000_9406(uStack_13e);
              goto LAB_OVL15_L0000__0015b9;
            }
LAB_OVL15_L0000__001577:
            if (((int)uStack_ea < 2) || (7 < (int)uStack_ea)) {
              uVar19 = 0x181f;
              uVar14 = FUN_1000_8944(uVar21,uStack_a6,uStack_ae);
              if ((uVar14 & 10) == 0) goto LAB_OVL15_L0000__0015b0;
            }
            else {
              uVar19 = 0x181f;
              uVar15 = uStack_ae;
              uVar14 = FUN_1000_8944(uVar21,uStack_a6,uStack_ae);
              if ((uVar14 & 0x40) == 0) goto LAB_OVL15_L0000__001556;
            }
          }
          else {
LAB_OVL15_L0000__001508:
            func_0x000193b2(uVar21,uVar14,uVar15,uVar16);
LAB_OVL15_L0000__0015b9:
            uVar19 = 0x191f;
            iStack_13c = 1;
          }
          uVar17 = 0x191f;
          func_0x00019bf6(uVar19);
          if (iStack_13c != 0) {
            iVar11 = *(int *)0x8542;
            iVar12 = *(int *)(iVar11 + 0xb6);
            if (0x14 < iVar12) {
              iVar12 = 0x14;
            }
            *(int *)(iVar11 + 0xb6) = *(int *)(iVar11 + 0xb6) - iVar12;
            *(undefined1 *)(iVar11 + 0x8c) = 0;
          }
        }
      }
    }
  }
  for (iStack_ac = 0; iStack_ac < *(char *)(*(int *)0x8542 + 0x1f); iStack_ac = iStack_ac + 1) {
    iVar12 = FUN_1000_8e44(uVar17,iStack_ac);
    iVar11 = iStack_ac;
    aiStack_12e[iStack_ac] = iVar12;
    aiStack_1ae[iVar11] = 0;
    FUN_1000_8c6e(0x181f,iStack_ac,0);
    uVar17 = 0x181f;
    FUN_1000_8e26(0x181f,iStack_ac,0x12);
  }
  iStack_ac = 0;
  do {
    *(undefined1 *)(*(int *)0x8542 + iStack_ac + 0x70) = 0xff;
    iStack_ac = iStack_ac + 1;
  } while (iStack_ac < 0x14);
  iStack_ac = 0;
  do {
    iVar11 = FUN_1000_8da4(uVar17,iStack_ac);
    aiStack_16a[iStack_ac] = iVar11;
    iStack_ac = iStack_ac + 1;
    uVar17 = 0x181f;
  } while (iStack_ac < 0x13);
  FUN_0000_df7e(*(int *)0x8542 + 0x70,0xffff,0x14);
  FUN_1000_8df4(0xd1d);
  iVar11 = *(int *)0x8542;
  uStack_80 = (uint)((int)*(char *)(iVar11 + 0x1f) < (int)(uStack_a2 * 2));
  if ((*(int *)(iVar11 + 0xaa) < 2) || (iStack_36 <= *(int *)(iVar11 + 0xaa))) {
    iStack_1e = 0;
  }
  else {
    iStack_1e = 1;
  }
  if ((uStack_80 == 0) && (iStack_1e == 0)) {
    iStack_7e = 0;
  }
  else {
    iStack_7e = 1;
  }
  if ((iStack_7e != 0) || (*(int *)(iVar11 + 0x9a) < 0x4b)) {
    iStack_ac = 0;
    while ((iStack_ac < *(char *)(*(int *)0x8542 + 0x1f) &&
           (((uStack_80 != 0 || (*(int *)0x8dc8 <= *(int *)0x8e0a)) || (iStack_1e != 0))))) {
      if ((aiStack_1ae[iStack_ac] == 0) &&
         (((aiStack_12e[iStack_ac] == 0 ||
           ((aiStack_12e[iStack_ac] == 8 && (iVar11 = FUN_1000_8bec(0x181f,6), iVar11 != 0)))) &&
          (iVar11 = iStack_ac, iVar12 = FUN_1000_8d5e(0x181f,iStack_ac,aiStack_12e[iStack_ac]),
          iVar12 == 0)))) {
        if (*(int *)0x8dbe < 3) goto LAB_OVL15_L0000__00178f;
        aiStack_1ae[iStack_ac] = 1;
        FUN_1000_8df4(iVar11,0x181f);
      }
      iStack_ac = iStack_ac + 1;
    }
    for (iStack_e6 = 0; iStack_e6 < 2; iStack_e6 = iStack_e6 + 1) {
      iStack_ac = 0;
      while ((iVar11 = *(int *)0x8542, iStack_ac < *(char *)(iVar11 + 0x1f) &&
             (((*(int *)0x8e32 != 0 && (*(int *)(iVar11 + 0x9a) <= *(int *)0x8e32 * 0x10)) ||
              (iStack_7e != 0))))) {
        if (aiStack_1ae[iStack_ac] == 0) {
          uStack_84 = (uint)(*(int *)0x8e32 * 0x10 < *(int *)(iVar11 + 0x9a));
          iVar11 = FUN_1000_8e8a(0x181f,aiStack_12e[iStack_ac]);
          if ((((iVar11 == 0) || ((uStack_84 == 0 && (iStack_e6 != 0)))) &&
              ((uStack_84 == 0 || ((iStack_e6 != 0 || (aiStack_12e[iStack_ac] == 0x1b)))))) &&
             (iVar11 = iStack_ac, iVar12 = FUN_1000_8d5e(0x181f,iStack_ac,0xffff), iVar12 == 0)) {
            if ((*(int *)0x8dbe < 3) || ((uStack_84 != 0 && (*(int *)0x8dbe < 5))))
            goto LAB_OVL15_L0000__00178f;
            aiStack_1ae[iStack_ac] = 1;
            FUN_1000_8df4(iVar11,0x181f);
          }
        }
        iStack_ac = iStack_ac + 1;
      }
    }
  }
LAB_OVL15_L0000__0017a9:
  *(undefined2 *)(code *)FUN_5952_035e = 1;
  if (((iStack_142 == 0) || ((int)*(char *)(*(int *)0x8542 + 0x1f) < (int)(uStack_a2 - iStack_142)))
     && (cVar8 = FUN_1000_8e6c(0x181f), *(char *)(*(int *)0x8542 + 0x1f) < cVar8)) {
    uStack_16e = 0xffff;
    iStack_34 = -1;
    for (iStack_ac = 0; iStack_ac < *(char *)(*(int *)0x8542 + 0x1f); iStack_ac = iStack_ac + 1) {
      if ((aiStack_1ae[iStack_ac] == 0) &&
         (iVar11 = FUN_1000_8d5e(0x181f,iStack_ac,0xfffe), iVar11 == 0)) {
        *(int *)0x8dc0 = *(int *)0x8dc0 << 2;
        iVar11 = FUN_1000_8e8a(0x181f,aiStack_12e[iStack_ac]);
        if (iVar11 == 0) {
          *(int *)0x8dc0 = *(int *)0x8dc0 + 1;
        }
        if (aiStack_12e[iStack_ac] == *(int *)0x8dc2) {
          *(int *)0x8dc0 = *(int *)0x8dc0 + 2;
        }
        if (((int)uStack_16e < *(int *)0x8dc0) && (1 < *(int *)0x8dbe)) {
          uStack_16e = *(uint *)0x8dc0;
          iStack_34 = iStack_ac;
        }
      }
    }
    if ((-1 < iStack_34) && (iVar11 = FUN_1000_8d5e(0x181f,iStack_34,0xffff), iVar11 == 0)) {
      aiStack_1ae[iStack_34] = 1;
      FUN_1000_8df4(0x181f);
      iVar11 = FUN_1000_8dfe(0x181f,iStack_34);
      if (iVar11 == 5) {
        iStack_8c = 1;
      }
    }
  }
  iStack_14 = 0;
  if ((*(byte *)(*(int *)0x8542 + 0x1d) & 0x80) == 0) {
    iStack_14 = 1;
    if ((iStack_8c == 0) && (*(int *)(*(int *)0x8542 + 0xa4) < 10)) {
      for (iStack_e6 = 0; iStack_e6 < 3; iStack_e6 = iStack_e6 + 1) {
        iStack_ac = 0;
        while ((iStack_ac < *(char *)(*(int *)0x8542 + 0x1f) && (*(int *)0x8dd2 == 0))) {
          if (aiStack_1ae[iStack_ac] == 0) {
            if (iStack_e6 == 0) {
              bVar18 = aiStack_12e[iStack_ac] == 5;
LAB_OVL15_L0000__0019ef:
              if (!bVar18) goto LAB_OVL15_L0000__0019f1;
            }
            else if (iStack_e6 == 1) {
              iVar11 = FUN_1000_8e8a(0x181f,aiStack_12e[iStack_ac]);
              bVar18 = iVar11 == 0;
              goto LAB_OVL15_L0000__0019ef;
            }
            iVar11 = FUN_1000_8d5e(0x181f,iStack_ac,5);
            if (iVar11 == 0) {
              aiStack_1ae[iStack_ac] = 1;
              iStack_8c = 1;
              FUN_1000_8df4(0x181f);
            }
          }
LAB_OVL15_L0000__0019f1:
          iStack_ac = iStack_ac + 1;
        }
      }
    }
    if (((iStack_8c == 0) && (*(int *)(*(int *)0x8542 + 0xa4) < 2)) && ((*(byte *)0x538e & 7) == 0))
    {
      piVar3 = (int *)(*(int *)0x8542 + 0xa4);
      *piVar3 = *piVar3 + 100;
      iVar11 = *(int *)0x84fc;
      if ((-1 < *(int *)(iVar11 + 0x2c)) &&
         ((0 < *(int *)(iVar11 + 0x2c) || (199 < *(uint *)(iVar11 + 0x2a))))) {
        puVar4 = (uint *)(iVar11 + 0x2a);
        uVar14 = *puVar4;
        *puVar4 = *puVar4 - 200;
        *(int *)(iVar11 + 0x2c) = *(int *)(iVar11 + 0x2c) - (uint)(uVar14 < 200);
      }
    }
    iStack_6a = *(int *)(*(int *)0x8542 + 0xa4) + *(int *)0x8dd2;
    if (1 < iStack_6a) {
      for (iStack_e6 = 0; iStack_e6 < 4; iStack_e6 = iStack_e6 + 1) {
        iStack_ac = 0;
        while ((iStack_ac < *(char *)(*(int *)0x8542 + 0x1f) && (*(int *)0x8de8 == 0))) {
          if (aiStack_1ae[iStack_ac] == 0) {
            if (iStack_e6 == 0) {
              bVar18 = aiStack_12e[iStack_ac] == 0xd;
LAB_OVL15_L0000__001adb:
              if (!bVar18) goto LAB_OVL15_L0000__001add;
            }
            else {
              if (iStack_e6 == 1) {
                bVar18 = aiStack_12e[iStack_ac] == 0x1c;
                goto LAB_OVL15_L0000__001adb;
              }
              if (iStack_e6 == 2) {
                bVar18 = aiStack_12e[iStack_ac] == 0x19;
                goto LAB_OVL15_L0000__001adb;
              }
            }
            if ((aiStack_12e[iStack_ac] == 0x1a) || (aiStack_12e[iStack_ac] == 0x19)) {
              FUN_1000_8e9e(0x181f,iStack_ac,0x1c);
            }
            uVar21 = 0x1b4e;
            iVar11 = FUN_1000_8e8a(0x181f,aiStack_12e[iStack_ac]);
            if (((iVar11 == 0) && (iStack_4e == 0)) && ('\x05' < *(char *)(*(int *)0x8542 + 0x1f)))
            {
              uVar21 = 0;
              iVar11 = FUN_1000_86c4(0x181f,0,-(*(byte *)0x53a6 - 0x10));
              if (iVar11 == 0) {
                uVar21 = 0xd;
                FUN_1000_8e9e(0x181f,iStack_ac,0xd);
              }
            }
            FUN_1000_8e26(0x181f,iStack_ac,0xd,uVar21);
            iStack_ca = iStack_ca + 1;
            aiStack_1ae[iStack_ac] = 1;
            FUN_1000_8df4(0x181f);
          }
LAB_OVL15_L0000__001add:
          iStack_ac = iStack_ac + 1;
        }
      }
    }
  }
  for (iStack_ac = 0; iVar11 = iStack_ac, iStack_ac < *(char *)(*(int *)0x8542 + 0x1f);
      iStack_ac = iStack_ac + 1) {
    if (aiStack_1ae[iStack_ac] == 0) {
      iVar13 = FUN_1000_8e8a(0x181f,aiStack_12e[iStack_ac]);
      iVar12 = iStack_ac;
      if (((iVar13 != 0) && (aiStack_12e[iVar11] < 9)) &&
         ((aiStack_12e[iStack_ac] != 0 && (aiStack_12e[iStack_ac] != 8)))) {
        if (aiStack_12e[iStack_ac] == 5) {
          if (iStack_14 == 0) {
            iStack_14 = 1;
          }
          else if (((*(byte *)(*(int *)0x8542 + 0x1b) & 0x20) == 0) && (*(int *)0x8e64 == 0))
          goto LAB_OVL15_L0000__001bfe;
        }
        if (*(int *)(*(int *)0x8542 + aiStack_12e[iStack_ac] * 2 + 0x9a) <= iStack_36) {
          iVar11 = FUN_1000_8d5e(0x181f,iStack_ac,aiStack_12e[iStack_ac]);
          if (iVar11 == 0) {
            aiStack_1ae[iVar12] = 1;
            FUN_1000_8df4(0x181f);
          }
        }
      }
    }
LAB_OVL15_L0000__001bfe:
  }
  uVar21 = 0x181f;
  for (iStack_ac = 0; iVar11 = iStack_ac, iStack_ac < *(char *)(*(int *)0x8542 + 0x1f);
      iStack_ac = iStack_ac + 1) {
    uVar17 = uVar21;
    if (aiStack_1ae[iStack_ac] == 0) {
      uVar17 = 0x181f;
      iVar13 = FUN_1000_8e8a(uVar21,aiStack_12e[iStack_ac]);
      iVar12 = iStack_ac;
      if ((((iVar13 != 0) && (8 < aiStack_12e[iVar11])) && (aiStack_12e[iVar11] < 0x13)) &&
         ((aiStack_16a[aiStack_12e[iStack_ac]] != 0 && (aiStack_e4[aiStack_12e[iStack_ac]] < 3)))) {
        uStack_88 = 0;
        uVar21 = 0;
        switch(aiStack_12e[iStack_ac]) {
        case 9:
        case 10:
        case 0xb:
        case 0xc:
        case 0xe:
        case 0xf:
          uStack_84 = (int)*(char *)(aiStack_12e[iStack_ac] + 0x2a2);
          iVar11 = *(char *)(aiStack_12e[iStack_ac] + 0x2a2) * 2;
          uVar17 = uVar21;
          if (*(int *)(iVar11 + -0x71a6) == 0) {
            if (*(int *)(*(int *)0x8542 + iVar11 + 0x9a) < 1) {
              iVar11 = *(int *)(iVar11 + -0x7238);
LAB_OVL15_L0000__001d71:
              uVar17 = uVar21;
              if (iVar11 == 0) break;
            }
            uStack_88 = 1;
            uVar17 = uVar21;
          }
          break;
        case 0xd:
          if (((*(byte *)(*(int *)0x8542 + 0x1d) & 0x80) != 0) && (1 < iStack_4e)) {
            aiStack_12e[iStack_ac] = 0x1c;
            uVar17 = 0x181f;
            FUN_1000_8e9e(0,iStack_ac,0x1c);
            iStack_4e = iStack_4e + -1;
            goto LAB_OVL15_L0000__001ccd;
          }
          uVar17 = uVar21;
          if (*(int *)0x8e64 == 0) {
            iVar11 = *(int *)(*(int *)0x8542 + 0xa4) + *(int *)0x8dd2;
            goto LAB_OVL15_L0000__001d71;
          }
          break;
        case 0x10:
          uStack_88 = (uint)((*(byte *)0x5382 & 1) == 0);
          uVar17 = 0;
          if ((*(byte *)0x5382 & 1) != 0) {
            aiStack_12e[iStack_ac] = 0x1c;
            uVar17 = 0x181f;
            FUN_1000_8e9e(0,iStack_ac,0x1c);
            iStack_48 = iStack_48 + -1;
            goto LAB_OVL15_L0000__001ccd;
          }
          break;
        case 0x11:
          uStack_88 = (uint)((*(byte *)0x5382 & 1) == 0);
          uVar17 = 0;
        }
        if (uStack_88 != 0) {
          FUN_1000_8e26(uVar17,iStack_ac,aiStack_12e[iStack_ac]);
          aiStack_e4[aiStack_12e[iVar12]] = aiStack_e4[aiStack_12e[iVar12]] + 1;
          aiStack_1ae[iVar12] = 1;
          uVar17 = 0x181f;
          FUN_1000_8df4(0x181f);
        }
      }
    }
LAB_OVL15_L0000__001ccd:
    uVar21 = uVar17;
  }
  for (iStack_ac = 0; iStack_ac < *(char *)(*(int *)0x8542 + 0x1f); iStack_ac = iStack_ac + 1) {
    if (aiStack_1ae[iStack_ac] == 0) {
      FUN_1000_8d5e(uVar21,iStack_ac,0xfffe);
      uStack_16e = 0;
      iStack_ee = 0xd;
      for (iStack_7a = 9; iStack_7a < 0x13; iStack_7a = iStack_7a + 1) {
        if (((aiStack_16a[iStack_7a] != 0) && (iStack_7a != 0x12)) && (aiStack_e4[iStack_7a] < 3)) {
          FUN_1000_8e26(0x181f,iStack_ac,iStack_7a);
          iStack_13a = (int)*(char *)(iStack_7a + 0x2b6);
          if (iStack_7a == 0xf) {
            iStack_13a = 0xe;
          }
          if (iStack_7a == 0xd) {
            iStack_13a = 5;
          }
          if (-1 < iStack_13a) {
            iVar11 = iStack_13a * 2;
            iStack_78 = (*(int *)(*(int *)0x8542 + iVar11 + 0x9a) - *(int *)(iVar11 + -0x71f6)) +
                        *(int *)(iVar11 + -0x7238);
            if (iStack_78 < 0) goto LAB_OVL15_L0000__001ef4;
            if (iStack_78 == 0) {
              iStack_78 = 1;
            }
          }
          uStack_144 = FUN_1000_8ec6(0x181f,iStack_ac,&iStack_34);
          if (iStack_78 < (int)uStack_144) {
            uStack_144 = iStack_78;
          }
          if (iStack_34 < 0x10) {
            iVar11 = (uint)*(byte *)(*(int *)0x8542 + 0x1a) * 0x10 + iStack_34;
            uStack_1bc = (uint)*(byte *)(iVar11 + -0x7b44);
            if ((iStack_34 != 0xf) && (iStack_34 != 0xe)) {
              uStack_1bc = uStack_1bc - *(byte *)(iVar11 + -0x7b4c);
            }
            if ((((iStack_34 == 0xe) || (iStack_34 == 0xf)) &&
                (uStack_1bc = uStack_1bc + 4, 0x31 < *(int *)0x538e)) &&
               (*(byte *)(*(int *)0x5398 + -0x6e84) <= *(byte *)(uStack_1b0 + 0x917c))) {
              uStack_1bc = uStack_1bc * 2;
            }
          }
          else {
            uStack_1bc = 3;
            if (iStack_7a == 0x11) {
              iVar11 = FUN_1000_8ca0(0x181f,0x13);
              uStack_1bc = iVar11 * 4 + iStack_7c + 7 + (uint)*(byte *)(*(int *)0x8542 + 0x96) * 4;
              if (9 < iStack_7c) {
                uStack_1bc = uStack_1bc * 2;
              }
              iVar11 = FUN_1000_89a4(0x181f,*(undefined1 *)(*(int *)0x8542 + 0x1a),0xf);
              if (iVar11 != 0) {
                uStack_1bc = uStack_1bc << 1;
              }
              if (*(int *)0x538a < 0x604) {
                uStack_1bc = 0;
              }
              if (0x640 < *(int *)0x538a) {
                uStack_1bc = uStack_1bc << 1;
              }
              if (0x6a4 < *(int *)0x538a) {
                uStack_1bc = uStack_1bc << 1;
              }
              if ((*(byte *)0x5382 & 1) != 0) {
                uStack_1bc = 0;
              }
              if (*(char *)(*(int *)0x8542 + 0x1f) < '\x04') {
                uStack_1bc = (int)uStack_1bc >> 1;
              }
              if (*(char *)(*(int *)0x8542 + 0x1f) < '\x06') {
                uStack_1bc = (int)uStack_1bc >> 1;
              }
              if (*(byte *)(*(int *)0x5398 + -0x6e84) < *(byte *)(uStack_1b0 + 0x917c)) {
                uStack_1bc = (int)uStack_1bc >> 1;
              }
              if (*(byte *)(uStack_1b0 + 0x917c) < *(byte *)(*(int *)0x5398 + -0x6e84)) {
                uStack_1bc = uStack_1bc << 1;
              }
              if ((*(byte *)*(undefined2 *)0x84fc & 4) != 0) {
                uStack_1bc = (int)uStack_1bc >> 1;
              }
              uStack_1bc = func_0x0001854c(0x181f,uStack_1bc - *(int *)(iStack_34 * 2 + -0x7238),1,
                                           100);
            }
            if (iStack_7a == 0xd) {
              uStack_1bc = -(*(uint *)(iStack_34 * 2 + -0x7238) / 3 - 5);
              if ((*(byte *)(*(int *)0x8542 + 0x1d) & 0x80) != 0) {
                uStack_1bc = (int)uStack_1bc >> 1;
              }
              if ((int)uStack_1bc < 1) {
                uStack_1bc = 1;
              }
            }
            if ((iStack_7a == 0x10) &&
               (uStack_1bc = uStack_1bc -
                             ((*(uint *)(iStack_34 * 2 + -0x7238) >> 1) + *(int *)0x538e / 100 + -6)
               , (int)uStack_1bc < 1)) {
              uStack_1bc = 1;
            }
          }
          uStack_144 = (uStack_144 * 8 + 5) * uStack_1bc;
          if (uStack_144 - uStack_16e != 0 && (int)uStack_16e <= (int)uStack_144) {
            uStack_16e = uStack_144;
            iStack_ee = iStack_7a;
          }
        }
LAB_OVL15_L0000__001ef4:
      }
      uVar21 = 0x2121;
      FUN_1000_8e26(0x181f,iStack_ac,0x12);
      if (*(int *)0x8dc0 < (int)uStack_16e) {
LAB_OVL15_L0000__002174:
        iVar11 = iStack_ee;
        FUN_1000_8e26(0x181f,iStack_ac,iStack_ee,uVar21);
        aiStack_e4[iStack_ee] = aiStack_e4[iStack_ee] + 1;
      }
      else {
        iVar11 = 0x2139;
        FUN_1000_8d5e(0x181f,iStack_ac,0xffff);
        if (*(int *)0x8dbe == 0) {
          uVar21 = 0x181f;
          iVar11 = FUN_1000_8bec(0x181f,0x25);
          if (iVar11 != 0) {
            uVar21 = 5;
            iVar11 = FUN_1000_8ef8(0x181f,5);
            if ((iVar11 != 0) && (iStack_c4 < 3)) {
              iStack_ee = 0x10;
              goto LAB_OVL15_L0000__002174;
            }
          }
          iStack_ee = 0xd;
          goto LAB_OVL15_L0000__002174;
        }
      }
      uVar21 = 0x181f;
      FUN_1000_8df4(0x181f,iVar11);
    }
  }
  iVar12 = FUN_1000_8d72(uVar21,0);
  iVar11 = FUN_1000_8d72(0x181f,8);
  iVar12 = iVar12 + iVar11;
  iVar11 = FUN_1000_8de0(0);
  iVar13 = FUN_1000_8de0(8);
  iStack_a = iVar11 + iVar13;
  iVar11 = *(int *)0x8542;
  pbVar2 = (byte *)(iVar11 + 0x1d);
  *pbVar2 = *pbVar2 & 0x7f;
  *(undefined1 *)(iVar11 + 0x94) = 0xff;
  iStack_a0 = 0;
  uStack_ec = 0;
  iStack_138 = 0;
  iStack_ac = 0;
  while( true ) {
    iVar11 = *(int *)0x8542;
    if ((int)*(char *)(iVar11 + 0x1f) + *(int *)0x8d72 <= iStack_ac) break;
    if (iStack_ac < *(char *)(iVar11 + 0x1f)) {
      iStack_18 = aiStack_12e[iStack_ac];
    }
    else {
      iStack_18 = FUN_1000_8e44(0x181f,iStack_ac);
    }
    iVar11 = FUN_1000_8e8a(0x181f,iStack_18);
    if (iVar11 != 0) {
      uStack_ec = uStack_ec + 1;
      iVar11 = *(int *)(iStack_18 * 8 + -0x715a) % 4;
      if (iVar11 < iStack_138) {
        iVar11 = iStack_138;
      }
      iStack_138 = iVar11;
    }
    iStack_ac = iStack_ac + 1;
  }
  if ((*(char *)(iVar11 + 0x1f) >> 1 < iVar12) && (1 < iVar12)) {
    iStack_a0 = 1;
  }
  if (*(int *)0x8e5a != 0) {
    iStack_a0 = 1;
  }
  if ((((int)(uStack_a2 - iStack_142) <= (int)*(char *)(iVar11 + 0x1f)) ||
      ((iStack_142 != 0 && (iStack_a0 != 0)))) &&
     (iVar11 = FUN_OVL15_L0000__002a6e(0x181f), iVar11 == 0)) {
    iStack_a0 = 0;
    goto LAB_OVL15_L0000__00274b;
  }
  iVar11 = FUN_OVL15_L0000__002a6e(0x181f);
  if ((iVar11 == 0) ||
     ((1 < *(int *)(*(int *)0x8542 + 0xaa) &&
      (iVar11 = FUN_OVL15_L0000__002a6e(0x181f), iVar11 == 0)))) goto LAB_OVL15_L0000__00274b;
  iVar11 = *(int *)0x8542;
  if (*(char *)(iVar11 + 0x1f) < '\x04') {
LAB_OVL15_L0000__002747:
    *(byte *)(iVar11 + 0x1d) = *(byte *)(iVar11 + 0x1d) | 0x80;
  }
  else {
    bVar9 = *(char *)(iVar11 + 0x1f) / '\x06';
    iStack_16c = (int)(char)bVar9;
    if ((((*(byte *)(iVar11 + 0x95) < bVar9) && (*(char *)(iVar11 + 0x95) == '\0')) &&
        (iVar11 = FUN_OVL15_L0000__002a6e(0x181f), iVar11 == 0)) ||
       ((iVar11 = *(int *)0x8542, '\x05' < *(char *)(iVar11 + 0x1f) &&
        (((((*(byte *)(iVar11 + 0x1b) & 3) != 0 ||
           ((int)(uint)*(byte *)(uStack_1b0 + 0x9424) <
            (int)(*(byte *)(*(int *)0x5398 + -0x6bdc) - 2))) || ('\v' < *(char *)(iVar11 + 0x1f)))
         && (iVar11 = FUN_OVL15_L0000__002a6e(0x181f), iVar11 == 0))))))
    goto LAB_OVL15_L0000__00274b;
    if ((((*(byte *)(*(int *)0x8542 + 0x1c) & 0x20) == 0) && (*(int *)0x538a < 0x640)) &&
       (((*(byte *)(iStack_6c + -0x6a0e) & 1) != 0 &&
        ((iVar11 = FUN_1000_84fc(0x181f,*(undefined2 *)0x8d52,uStack_1b0), iVar11 < 0x32 &&
         (iStack_22 == 0)))))) {
      uVar21 = 0xc;
    }
    else {
      if ((0 < iStack_138) &&
         ((3 < (int)((int)*(char *)(*(int *)0x8542 + 0x1f) + uStack_ec) &&
          (iVar11 = FUN_OVL15_L0000__002a6e(0x181f), iVar11 == 0)))) goto LAB_OVL15_L0000__00274b;
      if (*(char *)(*(int *)0x8542 + 0x1f) < '\x06') {
        pbVar2 = (byte *)(*(int *)0x8542 + 0x1d);
        *pbVar2 = *pbVar2 | 0x80;
      }
      if (((((((((3 < (uint)*(byte *)(uStack_1b0 * 0x10 + -0x7b35) + (uint)(*(byte *)0x53a6 >> 1))
                || (0x50 < *(int *)0x538e)) && ('\x05' < *(char *)(*(int *)0x8542 + 0x1f))) &&
              ((0x27 < *(int *)(*(int *)0x8542 + 0xb6) || (*(int *)0x8de4 != 0)))) ||
             (iStack_4a != 0)) && (iVar11 = FUN_OVL15_L0000__002a6e(0x181f), iVar11 == 0)) ||
           (((iStack_48 != 0 && (iVar11 = FUN_OVL15_L0000__002a6e(0x181f), iVar11 == 0)) ||
            ((iVar11 = FUN_OVL15_L0000__002a6e(0x181f), iVar11 == 0 ||
             (iVar11 = FUN_OVL15_L0000__002a6e(0x181f), iVar11 == 0)))))) ||
          (((((3 < *(byte *)(uStack_1b0 * 0x10 + -0x7b35) &&
              ('\x03' < *(char *)(*(int *)0x8542 + 0x1f))) &&
             ((0x27 < *(int *)(*(int *)0x8542 + 0xa6) || (*(int *)0x8dd4 != 0)))) &&
            (iVar11 = FUN_OVL15_L0000__002a6e(0x181f), iVar11 == 0)) ||
           ((((*(byte *)(*(int *)0x8542 + 0x95) < (byte)iStack_16c &&
              (iVar11 = FUN_OVL15_L0000__002a6e(0x181f), iVar11 == 0)) ||
             ((0x17 < *(uint *)(code *)FUN_0000_8dec &&
              (iVar11 = FUN_OVL15_L0000__002a6e(0x181f), iVar11 == 0)))) ||
            ((3 < *(uint *)(code *)FUN_0000_8dec &&
             (iVar11 = FUN_OVL15_L0000__002a6e(0x181f), iVar11 == 0)))))))) ||
         ((1 < iStack_138 &&
          ((9 < (int)((int)*(char *)(*(int *)0x8542 + 0x1f) + uStack_ec) &&
           (iVar11 = FUN_OVL15_L0000__002a6e(0x181f), iVar11 == 0))))))
      goto LAB_OVL15_L0000__00274b;
      iVar11 = *(int *)0x8542;
      if (*(char *)(iVar11 + 0x1f) < '\b') goto LAB_OVL15_L0000__002747;
      if ((((2 < iStack_138) && (0xf < (int)((int)*(char *)(iVar11 + 0x1f) + uStack_ec))) &&
          (iVar11 = FUN_OVL15_L0000__002a6e(0x181f), iVar11 == 0)) ||
         ((iVar11 = FUN_OVL15_L0000__002a6e(0x181f), iVar11 == 0 ||
          (('\t' < *(char *)(*(int *)0x8542 + 0x1f) &&
           (iVar11 = FUN_OVL15_L0000__002a6e(0x181f), iVar11 == 0))))))
      goto LAB_OVL15_L0000__00274b;
      iStack_2c = 0;
      iStack_ac = 5;
      do {
        iVar11 = FUN_1000_8ca0(0x181f,*(undefined1 *)(iStack_ac * 4 + 0x864));
        if (iVar11 == 3) {
          iStack_2c = 1;
        }
        iStack_ac = iStack_ac + -1;
      } while (-1 < iStack_ac);
      if ((iStack_2c == 0) || ((*(byte *)(*(int *)0x8542 + 0x1c) & 0x40) == 0)) {
LAB_OVL15_L0000__00262c:
        iStack_92 = 0;
        uVar21 = 0x181f;
        uStack_13e = FUN_1000_89d0(0x181f);
        while (-1 < (int)uStack_13e) {
          if (*(char *)(uStack_13e * 0x1c + 0x3146) == '\v') {
            iStack_92 = iStack_92 + 1;
          }
          uStack_13e = FUN_1000_84d4(uVar21,0x181f);
        }
        if ((iStack_2c == 0) || ((iStack_92 != 0 && (iStack_76 != 0)))) {
          iStack_ac = 5;
          do {
            iVar11 = FUN_OVL15_L0000__002a73(0x181f,iStack_ac);
            if (iVar11 != 0) {
              FUN_1000_8d90(0x181f,*(undefined1 *)(iStack_ac * 4 + 0x864));
              iVar11 = FUN_OVL15_L0000__002a6e(0x181f);
              if (iVar11 == 0) goto LAB_OVL15_L0000__00274b;
            }
            iStack_ac = iStack_ac + -1;
          } while (-1 < iStack_ac);
          iVar11 = FUN_OVL15_L0000__002a6e(0x181f);
          if (iVar11 == 0) goto LAB_OVL15_L0000__00274b;
          iVar11 = FUN_1000_8bec(0x181f,8);
          if (iVar11 != 0) {
            if (*(byte *)(uStack_1b0 * 0x13 + -0x6da5) <
                (byte)((*(byte *)(uStack_1b0 + 0x9424) - 8 & -(*(byte *)(uStack_1b0 + 0x9424) < 8))
                      + 8)) goto LAB_OVL15_L0000__0025f1;
            if (*(byte *)(uStack_1b0 + 0x9424) < 8) goto LAB_OVL15_L0000__002626;
          }
          if (*(char *)(*(int *)0x8542 + 0x1f) < '\n') {
            pbVar2 = (byte *)(*(int *)0x8542 + 0x1c);
            *pbVar2 = *pbVar2 | 0x10;
          }
          if (2 < iStack_92) {
            iVar11 = *(int *)0x8542;
            *(undefined1 *)(iVar11 + 0x94) = 0xff;
            goto LAB_OVL15_L0000__002747;
          }
          iVar11 = FUN_OVL15_L0000__002a6e(0x181f);
          if (iVar11 == 0) goto LAB_OVL15_L0000__00274b;
          iVar11 = *(int *)0x8de6;
        }
        else {
          iVar11 = *(int *)(*(int *)0x8542 + 0xb6);
        }
        if ((iVar11 != 0) && (iVar11 = FUN_OVL15_L0000__002a6e(0x181f), iVar11 == 0))
        goto LAB_OVL15_L0000__00274b;
        uVar21 = 0xb;
      }
      else {
        iVar11 = FUN_OVL15_L0000__002a6e(0x181f);
        if (iVar11 == 0) goto LAB_OVL15_L0000__00274b;
        iVar11 = FUN_1000_8bec(0x181f,8);
        if (iVar11 == 0) goto LAB_OVL15_L0000__00262c;
        if ((uint)(*(byte *)(uStack_1b0 + 0x9410) >> 1) + (uint)*(byte *)(uStack_1b0 + 0x9298) <
            (uint)*(byte *)(uStack_1b0 + 0x9414)) {
          if (((*(char *)(uStack_1b0 * 0x13 + -0x6da3) != '\0') ||
              (*(char *)(uStack_1b0 + 0x9424) == '\0')) && (*(byte *)(uStack_1b0 + 0x9424) < 4)) {
            uVar21 = 0x10;
            goto LAB_OVL15_L0000__0023be;
          }
          if (*(char *)(uStack_1b0 * 0x13 + -0x6da3) != '\0') goto LAB_OVL15_L0000__00262c;
LAB_OVL15_L0000__002626:
          uVar21 = 0x11;
          goto LAB_OVL15_L0000__0023be;
        }
LAB_OVL15_L0000__0025f1:
        uVar21 = 0xf;
      }
    }
LAB_OVL15_L0000__0023be:
    uVar10 = FUN_OVL15_L0000__002a78(0x181f,uVar21);
    *(undefined1 *)(*(int *)0x8542 + 0x94) = uVar10;
  }
LAB_OVL15_L0000__00274b:
  iStack_94 = FUN_1000_8ca0(0x181f,0xc);
  if ((iStack_94 != 0) &&
     ((char)(((iStack_94 == 3) + (char)iStack_94) * '\x04') <= *(char *)(*(int *)0x8542 + 0x8c))) {
    iStack_8a = -1;
    if ((*(char *)(*(int *)0x8542 + 0x1f) < '\n') && (iStack_a <= iVar12)) {
      iVar11 = FUN_1000_8de0(8);
      if (iVar11 < iStack_1a) {
        iStack_8a = 8;
      }
      else {
        iStack_8a = 0;
      }
    }
    for (iStack_ac = 0; iStack_ac < 6; iStack_ac = iStack_ac + 1) {
      cVar8 = *(char *)(iStack_ac * 4 + 0x864);
      iVar11 = FUN_1000_8ca0(0x181f,cVar8);
      if (cVar8 == '\x03') {
        iVar12 = 1;
      }
      else {
        iVar12 = 2;
      }
      if (((iVar12 <= iVar11) &&
          (*(int *)((uint)*(byte *)(iStack_ac * 4 + 0x866) * 2 + -0x7238) != 0)) &&
         (aiStack_68[*(byte *)(iStack_ac * 4 + 0x865)] == 0)) {
        iStack_8a = iStack_ac;
      }
    }
    iStack_20 = 0;
    for (iStack_ac = 0; iStack_ac < (int)*(char *)(*(int *)0x8542 + 0x1f) + *(int *)0x8d72;
        iStack_ac = iStack_ac + 1) {
      iStack_ee = FUN_1000_8dfe(0x181f,iStack_ac);
      iStack_18 = FUN_1000_8e44(0x181f,iStack_ac);
      iVar11 = FUN_1000_8e8a(0x181f,iStack_18);
      if ((((iVar11 == 0) || ((iStack_18 != iStack_ee && (iStack_ee != 0x13)))) &&
          (iStack_18 != 0x1b)) && (iStack_20 < 0x19)) {
        aiStack_68[iStack_20] = iStack_ac;
        iStack_20 = iStack_20 + 1;
      }
    }
    if (iStack_20 != 0) {
      iStack_72 = FUN_1000_86c4(0x181f,0,iStack_20 + -1);
      iStack_30 = aiStack_68[iStack_72];
      iVar11 = iStack_8a;
      if (iStack_8a < 0) {
        iVar11 = FUN_1000_8dfe(0x181f,iStack_30);
      }
      iStack_ee = iVar11;
      FUN_1000_8e9e(0x181f,iStack_30,iVar11);
      *(undefined1 *)(*(int *)0x8542 + 0x8c) = 0;
    }
  }
  uVar21 = 0x181f;
  if ((iStack_a0 != 0) && (iVar11 = *(int *)0x84fc, *(char *)(iVar11 + 1) < '\x1a')) {
    iVar12 = (int)*(uint *)0x8ea8 >> 0xf;
    if ((iVar12 <= *(int *)(iVar11 + 0x2c)) &&
       ((iVar12 < *(int *)(iVar11 + 0x2c) || (*(uint *)0x8ea8 <= *(uint *)(iVar11 + 0x2a))))) {
      iStack_30 = -1;
      uStack_ec = 0;
      for (iStack_ac = 0; iStack_ac < *(char *)(*(int *)0x8542 + 0x1f); iStack_ac = iStack_ac + 1) {
        iStack_ee = FUN_1000_8e44(0x181f,iStack_ac);
        if (iStack_ee == 0) {
          uStack_ec = uStack_ec | 1;
        }
        if (iStack_ee == 8) {
          uStack_ec = uStack_ec | 2;
        }
        iVar11 = FUN_1000_8e8a(0x181f,iStack_ee);
        if ((iVar11 == 0) && (iStack_ee != 0x1b)) {
          iStack_30 = iStack_ac;
        }
      }
      iStack_ee = 0x1c;
      if (-1 < iStack_30) {
        if (((uStack_ec & 2) == 0) && (iVar11 = FUN_1000_8bec(0x181f,6), iVar11 != 0)) {
          iStack_ee = 8;
        }
        else if ((uStack_ec & 1) == 0) {
          iStack_ee = 0;
        }
      }
      uVar21 = 0x181f;
      if (iStack_ee != 0x1c) {
        FUN_1000_8e9e(0x181f,iStack_30,iStack_ee);
        uVar15 = *(uint *)(iStack_30 * 8 + -0x7158);
        iVar11 = *(int *)0x84fc;
        puVar4 = (uint *)(iVar11 + 0x2a);
        uVar14 = *puVar4;
        *puVar4 = *puVar4 - uVar15;
        piVar3 = (int *)(iVar11 + 0x2c);
        *piVar3 = (*piVar3 - ((int)uVar15 >> 0xf)) - (uint)(uVar14 < uVar15);
        uVar21 = 0x191f;
        func_0x00019cd0(0x181f,0x17e0,1);
      }
    }
  }
  if (((*(int *)(*(int *)0x8542 + 0xaa) < 2) && (0x27 < *(int *)0x538e)) &&
     (iVar11 = FUN_1000_89d0(uVar21),
     *(char *)((uint)*(byte *)(iVar11 * 0x1c + 0x3146) * 0xe + 0x5237) != '\0')) {
    iVar11 = *(int *)0x84fc;
    if ((-1 < *(int *)(iVar11 + 0x2c)) &&
       ((0 < *(int *)(iVar11 + 0x2c) || (9 < *(uint *)(iVar11 + 0x2a))))) {
      puVar4 = (uint *)(iVar11 + 0x2a);
      uVar14 = *puVar4;
      *puVar4 = *puVar4 - 10;
      *(int *)(iVar11 + 0x2c) = *(int *)(iVar11 + 0x2c) - (uint)(uVar14 < 10);
      *(undefined2 *)(*(int *)0x8542 + 0xaa) = 2;
    }
  }
  *(undefined2 *)(code *)FUN_5952_035e = 0;
  return;
LAB_OVL15_L0000__00178f:
  FUN_1000_8c96(iStack_ac);
  FUN_1000_8e26(0x181f,iStack_ac,0x12);
  goto LAB_OVL15_L0000__0017a9;
}
```
