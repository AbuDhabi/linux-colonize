# Indian trade / meet decision (`FUN_4d56_2820` + nest)

Layer D map for the **trade shell** and nested buy/haggle/demand helpers.
Linux keeps thin auto-trade / gift-demand / hard-bargain stand-ins in
`ai_contact_*` — deep body **PARKED** for port; **mapped** here.

Related: [`indian_contact.md`](indian_contact.md). Stubs:
[`indian_trade_helpers.c`](indian_trade_helpers.c).

## Correction (2026-08-13) — clean re-disassembly recovered, size mismatch flagged

`FUN_4d56_2820` carried a Ghidra disassembly-fault warning in the canonical
export (`Instruction at (ram,0x00040af8) overlaps instruction at
(ram,0x00040af7)` — see `docs/decomp_inventory.md`), the same defect class
as `FUN_4d56_4528` (`indian_settlement_4528.md`). Re-disassembled directly
via the overlay-addressing project (`docs/rtlink_decode_v2_gap.md`,
`tools/address_mapping.csv` → `OVL13_L0000:2820`): a clean, self-contained,
**3439-byte / 595-line** function, no warnings, ends with a real return,
never leaves `OVL13_L0000`. Calls only resident helpers (`FUN_1000_*`) and
near-thunks — **no calls to any of the `2aac`/`2b92`/`2bbc`/`2e92`/`2f96`/
`306c`/`311e`/`3582`/`2af6`/`2a9b`/`2a78` labels** this doc's call graph
below describes as `2820`'s own nest.

**Resolved: the "nest" isn't separate functions.** `2aac`, `2b92`, `2bbc`,
`2e92`, `2e86`, `2e89`, `3582` all show up in the clean decompile as
**internal `goto` labels inside this one function** (`LAB_OVL13_L0000__002e92`
etc.) — a state-machine-style control flow, not calls to sibling functions.
The call-graph section below, which models them as separate
`FUN_4d56_2aac(...)`/`FUN_4d56_2b92(...)` etc. with their own call chains,
has the right *concepts* (auto-trade vs. player buy-offer vs. haggle vs.
hard-bargain are all really in here) but the wrong *shape* — there's one
function, not a shell dispatching to a nest. The line-range table's "shell
~219 + nest ~1171 across separate functions" was very likely read off the
same corrupted disassembly that broke `4528` — once merged back into one
function the two pieces are this single 595-line body. `2f96`/`306c`/`311e`
from the call graph below weren't confirmed as labels in this recovery;
worth checking if they're used at all or were part of the same corrupted
inference.

Full clean recovery (Ghidra decompile, `OVL13_L0000::2820`–`0x359a`, zero
warnings, ends in a real `return`):

```c
void FUN_4d56_2820(undefined2 param_1, int param_2, undefined2 param_3, int param_4, undefined2 param_5)
{
  char *pcVar1;
  uint *puVar2;
  int *piVar3;
  char cVar4;
  int iVar5;
  uint uVar6;
  int iVar7;
  int iVar8;
  undefined2 unaff_SI;
  int iVar9;
  int iVar10;
  undefined2 unaff_DI;
  undefined2 unaff_CS;
  undefined2 uVar11;
  undefined2 uVar12;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  undefined4 uVar13;
  undefined2 in_stack_0000ff1c;
  undefined2 in_stack_0000ff1e;
  undefined2 in_stack_0000ff20;
  undefined2 in_stack_0000ff26;
  int aiStack_d6 [4];
  int iStack_ce;
  uint uStack_cc;
  uint uStack_ca;
  int iStack_c8;
  int iStack_c6;
  int iStack_c4;
  int iStack_c2;
  undefined1 auStack_c0 [3];
  char cStack_bd;
  char cStack_bb;
  char acStack_98 [15];
  char cStack_89;
  int iStack_88;
  int iStack_86;
  int iStack_84;
  int iStack_82;
  int iStack_80;
  int iStack_7e;
  char acStack_7c [13];
  char cStack_6f;
  char cStack_6e;
  char cStack_6d;
  int iStack_6c;
  int iStack_6a;
  int iStack_68;
  undefined4 uStack_66;
  uint uStack_62;
  int iStack_60;
  int iStack_5e;
  int iStack_5c;
  int iStack_5a;
  undefined1 auStack_58 [80];
  int iStack_8;
  int iStack_6;
  int iStack_4;

  iStack_c8 = -1;
  iStack_c6 = 1;
  if (((param_4 < 0) || (3 < param_4)) || (*(char *)(param_4 * 0x34 + 0x543f) != '\0')) {
    iStack_8 = 0;
  }
  else {
    iStack_8 = 1;
  }
  uVar11 = unaff_CS;
  if (iStack_8 != 0) {
    in_stack_0000ff20 = 3;
    in_stack_0000ff1e = 0;
    uVar11 = 0x181f;
    iVar5 = FUN_1000_86c4();
    in_stack_0000ff1c = unaff_CS;
    if (iVar5 == 0) {
      in_stack_0000ff1e = 5;
      in_stack_0000ff1c = 0x181f;
      FUN_1000_8688(0x181f,5,in_stack_0000ff20);
      if (*(int *)0x8d52 == 0) {
        in_stack_0000ff1c = 7;
        FUN_1000_8688(0x181f,7);
      }
      uVar11 = 0x181f;
      if (*(int *)0x8d52 == 1) {
        uVar11 = 0x181f;
        FUN_1000_8688(0x181f,6);
      }
    }
  }
  if ((*(byte *)(param_2 * 0x1c + 0x3146) < 0xd) || (0x12 < *(byte *)(param_2 * 0x1c + 0x3146))) {
    *(undefined1 *)(param_2 * 0x1c + 0x3158) = 0;
  }
  iStack_c2 = 0;
  do {
    acStack_7c[iStack_c2] = (char)iStack_c2;
    acStack_98[iStack_c2] = (char)iStack_c2;
    iStack_c2 = iStack_c2 + 1;
  } while (iStack_c2 < 0x10);
  uVar12 = *(undefined2 *)0x83a6;
  FUN_1000_86ba(uVar11,uVar12);
  uVar11 = 0x181f;
  thunk_FUN_1000_a624(uVar12,in_stack_0000ff1c,in_stack_0000ff1e,in_stack_0000ff20,unaff_SI,unaff_DI
                      ,in_stack_0000ff26);
  iStack_68 = *(int *)0x9e78;
  *(undefined2 *)0x9e78 = 0;
  if (*(int *)0x9e92 < iStack_68) {
    *(undefined2 *)0x9e58 = 0;
  }
  uVar12 = 0x191f;
  FUN_1000_a0c0(0x9e78,unaff_DS,acStack_98,unaff_SS);
  iStack_c2 = 1;
  do {
    cVar4 = *(char *)((int)&iStack_88 - iStack_c2);
    *(undefined2 *)(cVar4 * 2 + -25000) = 0;
    if (cVar4 == '\0') {
      *(char *)((int)&iStack_88 - iStack_c2) = '\f';
    }
    iStack_c2 = iStack_c2 + 1;
  } while (iStack_c2 < 4);
  if (param_4 < 0) {
    return;
  }
  iVar5 = param_2 * 0x1c;
  if (*(char *)(iVar5 + 0x3150) != '\0') {
    iStack_7e = 0;
    if (1 < *(byte *)(iVar5 + 0x3150)) {
      if (iStack_8 == 0) {
        uVar12 = 0x181f;
        iStack_7e = FUN_1000_86c4(0x191f,0,*(byte *)(iVar5 + 0x3150) - 1);
      }
      else {
        uStack_66 = func_0x00019372(0x191f);
        if (uStack_66 == 0) goto LAB_003582;
        for (iStack_c2 = 0; iStack_c2 < (int)(uint)*(byte *)(param_2 * 0x1c + 0x3150);
            iStack_c2 = iStack_c2 + 1) {
          iStack_c8 = FUN_1000_8dd6(0x191f,param_2,iStack_c2);
          iStack_6a = FUN_1000_8e58(0x181f,param_2,iStack_c2,auStack_58,10);
          FUN_0000_daca(iStack_6a);
          FUN_1000_8368(0xd1d,auStack_58);
          FUN_1000_835e(0x181f,auStack_58,*(undefined2 *)(iStack_c8 * 2 + -0x6840));
          func_0x00019366(0x181f,uStack_66,auStack_58);
        }
        uVar13 = FUN_1000_8212(0x191f,*(undefined2 *)0x2dfa,99);
        uVar11 = 0x2a43;
        func_0x00019366(0x181f,uStack_66,uVar13);
        iStack_5e = FUN_1000_935a(uStack_66);
        uVar12 = 0x191f;
        func_0x00019398(0x191f,uStack_66);
        if ((iStack_5e == 0) || (iStack_5e == 99)) goto LAB_003582;
        iStack_7e = iStack_5e + -1;
      }
    }
    iStack_c8 = FUN_1000_8dd6(uVar12,param_2,iStack_7e);
    uVar12 = 0x181f;
    iStack_6a = FUN_1000_8e58(0x181f,param_2,iStack_7e);
  }
  iVar5 = 0x2aa7;
  aiStack_d6[0] = FUN_1000_84fc(uVar12,*(undefined2 *)0x8d52,param_4);
  if (iStack_c8 < 0) {
LAB_002e92:
    if ((int)((uint)*(byte *)((uint)*(byte *)(param_2 * 0x1c + 0x3146) * 0xe + 0x5237) -
             (uint)*(byte *)(param_2 * 0x1c + 0x3150)) < 1) {
      iStack_c6 = 0;
    }
    if (iStack_c6 == 0) goto LAB_003582;
    if (-1 < *(char *)(*(int *)0x8d4a + 8)) {
      *(undefined2 *)(*(char *)(*(int *)0x8d4a + 8) * 2 + -25000) = 0;
    }
    if (-1 < *(char *)(*(int *)0x8d4a + 9)) {
      *(undefined2 *)(*(char *)(*(int *)0x8d4a + 9) * 2 + -25000) = 0;
    }
    uVar12 = 0x191f;
    FUN_1000_a0c0(0x9e58,unaff_DS,acStack_7c,unaff_SS);
    if ((cStack_6d != iStack_c8) && (cStack_6e != iStack_c8)) {
      FUN_1000_8628(0,*(undefined2 *)(cStack_6d * 2 + -0x6840),uVar11,iVar5);
      FUN_1000_8628(1,*(undefined2 *)(cStack_6e * 2 + -0x6840));
      uVar12 = 0x181f;
      FUN_1000_8628(2,*(undefined2 *)(cStack_6f * 2 + -0x6840));
      if (iStack_8 != 0) {
        uVar12 = 0x191f;
        func_0x0001938c(0x181f,0x1587,*(undefined2 *)0x8d52);
      }
    }
    if ((iStack_8 == 0) || (*(char *)(*(int *)0x8d4a + 7) != -2)) {
      if (-1 < iStack_c8) {
        iStack_5a = 0;
        for (iStack_c2 = 0; (iStack_5a < 3 && (iStack_c2 < 0x10)); iStack_c2 = iStack_c2 + 1) {
          iStack_c8 = (int)(&cStack_89)[-iStack_c2];
          if ((iStack_c8 != 0xf) && (((iStack_c8 != 0 && (iStack_c8 != 0xe)) && (iStack_c8 != 0xd)))
             ) {
            uVar12 = 0x181f;
            FUN_1000_8628(iStack_5a,*(undefined2 *)(iStack_c8 * 2 + -0x6840));
            aiStack_d6[iStack_5a + 1] = 0xf - iStack_c2;
            iStack_5a = iStack_5a + 1;
          }
        }
        if (iStack_8 == 0) {
          iStack_5e = 1;
          uStack_cc = 0xd8f1;
          iStack_c2 = 0;
          do {
            iStack_60 = (int)*(char *)((int)&iStack_88 - iStack_c2);
            uStack_ca = (uint)*(byte *)(param_4 * 0x10 + (int)*(char *)((int)&iStack_88 - iStack_c2)
                                       + -0x7b44);
            if ((int)uStack_cc < (int)uStack_ca) {
              uStack_cc = uStack_ca;
              iStack_5e = iStack_c2 + 1;
            }
            iStack_c2 = iStack_c2 + 1;
          } while (iStack_c2 < 3);
        }
        else {
          iStack_5e = func_0x0001938c(uVar12,0x15a0,*(undefined2 *)0x8d52);
          uVar12 = 0x191f;
        }
        if ((0 < iStack_5e) && (iStack_5e < 4)) {
          iStack_86 = aiStack_d6[iStack_5e];
          iStack_c8 = (int)acStack_98[aiStack_d6[iStack_5e]];
          if ((0xc < *(byte *)(param_2 * 0x1c + 0x3146)) &&
             (*(byte *)(param_2 * 0x1c + 0x3146) < 0x13)) {
            *(int *)0x8dc4 = *(int *)0x8dc4 >> 2;
          }
          uStack_62 = 200;
          if (7 < iStack_c8) {
            uStack_62 = (*(byte *)(*(int *)0x8d4e + 2) - 8) * -0x32;
          }
          if (6 < iStack_c8) {
            uStack_62 = uStack_62 +
                        (uint)*(byte *)(iStack_c8 + param_4 * 0x10 + -0x7b44) *
                        ((uint)*(byte *)0x53a6 * 2 + 0xf);
          }
          iVar5 = FUN_1000_86c4(uVar12,0,uStack_62);
          uStack_62 = uStack_62 + iVar5 + *(int *)(iStack_86 * 2 + -0x6188) * -4;
          iVar5 = FUN_1000_84fc(0x181f,*(undefined2 *)0x8d52,param_4);
          uStack_62 = uStack_62 + iVar5 * 4;
          uStack_62 = FUN_0000_e096((long)*(int *)0x8dc4 * (long)(int)uStack_62,100,0);
          iStack_82 = FUN_1000_86c4(0xd1d,0,2);
          uStack_62 = uStack_62 + ((uint)*(byte *)0x53a6 + iStack_82) * 10;
          if ((int)uStack_62 < 0x32) {
            uStack_62 = 0x32;
          }
          FUN_1000_8628(0,*(undefined2 *)(iStack_c8 * 2 + -0x6840));
          iVar5 = *(int *)0x8dc4;
          *(int *)0x9cb8 = iVar5;
          *(int *)0x9cba = iVar5 >> 0xf;
          FUN_1000_8b9e(3,*(undefined2 *)(param_4 * 0x13c + -0x77ce),
                        *(undefined2 *)(param_4 * 0x13c + -0x77cc));
          if ((*(byte *)(param_2 * 0x1c + 0x3146) < 0xd) ||
             (0x12 < *(byte *)(param_2 * 0x1c + 0x3146))) {
            uVar11 = *(undefined2 *)0x2e0c;
          }
          else {
            uVar11 = *(undefined2 *)0x2e0e;
          }
          FUN_1000_8628(1,uVar11);
          uVar11 = 0xd1d;
          FUN_0000_d9b4(auStack_c0,0x15a9);
          iStack_88 = 0;
          do {
            iStack_6c = 0;
            iStack_ce = (int)uStack_62 >> 1;
            if (iStack_ce < 10) {
              iStack_ce = 10;
            }
            iStack_4 = (int)uStack_62 >> 2;
            if (iStack_4 < 1) {
              iStack_4 = 1;
            }
            *(uint *)0x9cb0 = uStack_62;
            *(int *)0x9cb2 = (int)uStack_62 >> 0xf;
            *(int *)0x9cb4 = iStack_ce;
            *(int *)0x9cb6 = iStack_ce >> 0xf;
            cStack_bd = (char)iStack_88 + '0';
            iStack_5e = 1;
            if (iStack_8 != 0) {
              iStack_5e = func_0x0001938c(uVar11,auStack_c0,*(undefined2 *)0x8d52);
              uVar11 = 0x191f;
            }
            if (iStack_5e == 1) {
              iVar5 = (int)uStack_62 >> 0xf;
              iVar8 = param_4 * 0x13c;
              if ((*(int *)(iVar8 + -0x77cc) < iVar5) ||
                 ((*(int *)(iVar8 + -0x77cc) <= iVar5 && (*(uint *)(iVar8 + -0x77ce) < uStack_62))))
              {
                uVar12 = *(undefined2 *)(param_4 * 0x13c + -0x77cc);
                *(undefined2 *)0x9cb0 = *(undefined2 *)(param_4 * 0x13c + -0x77ce);
                *(undefined2 *)0x9cb2 = uVar12;
                if (iStack_8 != 0) {
                  func_0x0001938c(uVar11,0x15ae,*(undefined2 *)0x8d52);
                }
                iVar5 = 0;
                iVar8 = 1;
              }
              else {
                puVar2 = (uint *)(iVar8 + -0x77ce);
                uVar6 = *puVar2;
                *puVar2 = *puVar2 - uStack_62;
                *(int *)(iVar8 + -0x77cc) =
                     (*(int *)(iVar8 + -0x77cc) - iVar5) - (uint)(uVar6 < uStack_62);
                iVar5 = *(int *)0x8d4a;
                *(undefined1 *)(iVar5 + 9) = (undefined1)iStack_c8;
                if (iStack_c8 == 9) {
                  *(undefined1 *)(iVar5 + 9) = 0xff;
                }
                iVar5 = *(int *)0x8dc4;
                piVar3 = (int *)(*(int *)0x8d4e + iStack_c8 * 2 + 0xe);
                *piVar3 = *piVar3 - iVar5;
                FUN_1000_8f48(uVar11,param_2,iStack_c8,iVar5);
                iVar8 = (int)uStack_62 / 0x19 + 1;
                iStack_c4 = FUN_1000_86c4(0x181f,0,iVar8);
              }
              uVar11 = 0x181f;
              FUN_1000_8f5c(*(undefined2 *)0x8d52,param_4,iVar8,iVar5);
            }
            else if (iStack_5e == 2) {
              iStack_82 = FUN_1000_86c4(uVar11,0,*(int *)(iStack_86 * 2 + -0x6188) / 0x19 + 8);
              if (((int)uStack_62 < 0xb) || (iStack_82 <= (int)(*(byte *)0x53a6 + 1))) {
                FUN_1000_8f5c(*(undefined2 *)0x8d52,param_4,2,0);
                uVar11 = 0x181f;
                uVar6 = FUN_1000_8c28(0x181f,param_4,param_5);
                if ((uVar6 & 0x40) != 0) {
                  *(undefined1 *)(*(int *)0x8d4a + 7) = 0xfe;
                  uVar11 = 0x191f;
                  func_0x0001938c(0x181f,(undefined1 *)&LAB_0015b8,
                                  *(undefined2 *)0x8d52);
                }
              }
              else {
                uStack_62 = uStack_62 - iStack_4;
                if ((int)uStack_62 < 10) {
                  uStack_62 = 10;
                }
                iVar5 = FUN_1000_86c4(0x181f,1,-(*(byte *)0x53a6 - 8));
                if (iVar5 == 1) {
                  FUN_1000_8f5c(*(undefined2 *)0x8d52,param_4,1,0);
                }
                uVar11 = 0x181f;
                uVar6 = FUN_1000_8c28(0x181f,param_4,param_5);
                if ((uVar6 & 0x40) != 0) {
                  iStack_88 = 1;
                  iStack_6c = 1;
                }
              }
            }
          } while (iStack_6c != 0);
        }
        goto LAB_003582;
      }
      if (iStack_8 == 0) goto LAB_003582;
    }
  }
  else {
    if (iStack_8 == 0) {
LAB_002bbc:
      iStack_82 = FUN_1000_86c4(0x181f,1,5);
      iStack_5c = 6;
      if (8 < iStack_c8) {
        iStack_5c = 7;
      }
      if (iStack_c8 == 0xd) {
        iVar5 = FUN_1000_86c4(0x181f,0,7);
        iStack_5c = iStack_5c - iVar5;
      }
      if (iStack_c8 == 0xf) {
        iStack_5c = iStack_5c - (*(char *)(*(int *)0x8d4e + 7) + -0xc);
      }
      if (iStack_c8 == 8) {
        iStack_5c = iStack_5c - (*(char *)(*(int *)0x8d4e + 8) + -10);
      }
      if (iStack_c8 == 0xe) {
        iStack_5c = iStack_5c + 1;
      }
      iVar5 = aiStack_d6[0];
      iStack_80 = FUN_1000_8c50(0x181f,aiStack_d6[0]);
      iStack_80 = iStack_80 << 1;
      if ((iStack_c8 == 0xf) || (iStack_c8 == 8)) {
        iStack_80 = 0;
      }
      if (0x13 < *(int *)(iStack_c8 * 2 + -25000)) {
        iStack_80 = iStack_80 >> 1;
      }
      iVar7 = iStack_c8 * 2;
      iVar8 = (((iStack_5c - (uint)*(byte *)0x53a6) - iStack_80) + iStack_82 + 4) * 2 *
              *(int *)(iVar7 + -25000);
      if (iVar8 < 0) {
        iVar8 = 0;
      }
      iVar8 = FUN_0000_e096((long)(iStack_82 * 5 + iVar8) * (long)iStack_6a,100,0);
      uStack_62 = iVar8 / 2;
      if ((int)uStack_62 < 1) {
        uStack_62 = 1;
      }
      iVar8 = (*(int *)(iVar7 + -25000) - iStack_80) + 4;
      uStack_ca = iVar8 / 10;
      if (3 < (int)uStack_ca) {
        uStack_ca = 3;
      }
      uVar11 = 1;
      iStack_c4 = FUN_1000_86c4(0xd1d,0,1);
      iStack_c4 = iStack_c4 + (iVar8 >> 2);
      iStack_6 = iStack_6a;
      iStack_ce = (*(int *)(iVar7 + -25000) + 1) * 4 + uStack_62;
      FUN_1000_8628(0,*(undefined2 *)(uStack_ca * 2 + -0x6cc0));
      FUN_1000_8628(1,*(undefined2 *)(iVar7 + -0x6840));
      iStack_88 = 0;
      uVar12 = 0xd1d;
      FUN_0000_d9b4(auStack_c0,0x1575);
      do {
        iStack_6c = 0;
        *(uint *)0x9cb0 = uStack_62;
        *(int *)0x9cb2 = (int)uStack_62 >> 0xf;
        *(int *)0x9cb4 = iStack_ce;
        *(int *)0x9cb6 = iStack_ce >> 0xf;
        cStack_bb = (char)iStack_88 + '0';
        if (iStack_8 == 0) {
          iStack_5e = 1;
          if (0x31 < aiStack_d6[0]) {
            iStack_5e = 3;
          }
        }
        else {
          iStack_5e = func_0x0001938c(uVar12,auStack_c0,*(undefined2 *)0x8d52);
          uVar12 = 0x191f;
        }
        if (iStack_5e == 1) {
          FUN_1000_8cdc(uVar12,param_2,iStack_7e);
          puVar2 = (uint *)(param_4 * 0x13c + -0x77ce);
          uVar6 = *puVar2;
          *puVar2 = *puVar2 + uStack_62;
          piVar3 = (int *)(param_4 * 0x13c + -0x77cc);
          *piVar3 = *piVar3 + ((int)uStack_62 >> 0xf) + (uint)CARRY2(uVar6,uStack_62);
          piVar3 = (int *)(*(int *)0x8d4e + iStack_c8 * 2 + 0xe);
          *piVar3 = *piVar3 + *(int *)0x8dc4;
          *(undefined1 *)(*(int *)0x8d4a + 7) = 0xff;
          if (0 < iStack_c4) {
            FUN_1000_8f5c(*(undefined2 *)0x8d52,param_4,iStack_c4 * -2,0);
            iVar8 = *(int *)0x8dc4;
            iVar9 = param_4 * 2;
            iVar7 = *(int *)0x8d4a;
            piVar3 = (int *)(iVar7 + iVar9 + 10);
            *piVar3 = *piVar3 - iVar8;
            iVar10 = *(int *)(iVar7 + iVar9 + 10);
            if (iVar10 < 0) {
              iVar10 = 0;
            }
            *(int *)(iVar7 + iVar9 + 10) = iVar10;
            if (iVar8 == 100) {
              *(undefined2 *)(iVar7 + iVar9 + 10) = 0;
            }
          }
          uVar12 = 0x181f;
          if ((param_2 == 0xf) || (param_2 == 8)) {
            *(undefined1 *)(*(int *)0x8d4a + 8) = 0xff;
          }
          else {
            *(undefined1 *)(*(int *)0x8d4a + 8) = (undefined1)iStack_c8;
          }
          if (iStack_c8 == 0xf) {
            if (0x18 < iStack_6a) {
              *(char *)(*(int *)0x8d4e + 7) = *(char *)(*(int *)0x8d4e + 7) + '\x01';
            }
            if (0x31 < iStack_6a) {
              *(char *)(*(int *)0x8d4e + 7) = *(char *)(*(int *)0x8d4e + 7) + '\x01';
            }
          }
          if (iStack_c8 == 8) {
            iVar8 = *(int *)0x8d4e;
            piVar3 = (int *)(iVar8 + 10);
            *piVar3 = *piVar3 + (iStack_6a >> 2);
            if (0x18 < iStack_6a) {
              pcVar1 = (char *)(iVar8 + 8);
              *pcVar1 = *pcVar1 + '\x01';
            }
            if (0x31 < iStack_6a) {
              iVar8 = *(int *)0x8d4e;
LAB_002e86:
              uVar12 = 0x181f;
              *(char *)(iVar8 + 8) = *(char *)(iVar8 + 8) + '\x01';
            }
          }
        }
        else {
          if (iStack_5e == 2) {
            iStack_6 = iStack_6 >> 1;
            if (0 < iStack_c4) {
              iVar8 = FUN_1000_86c4(uVar12,1,iStack_c4 << 3);
              if ((int)(uint)*(byte *)0x53a6 < iVar8) {
                iStack_c4 = iStack_c4 + -1;
                iVar8 = *(int *)(iStack_c8 * 2 + -25000);
                uVar12 = 0x181f;
                iVar8 = FUN_1000_86c4(0x181f,(iVar8 >> 1) + 1,iVar8 * 2 + 1);
                iStack_84 = (iVar8 * iStack_6a) / 100;
                if (iStack_84 < 1) {
                  iStack_84 = 1;
                }
                uStack_62 = uStack_62 + iStack_84;
                if (iStack_ce <= (int)uStack_62) {
                  iStack_ce = uStack_62 + 10;
                }
                iStack_88 = 1;
                iStack_6c = 1;
                goto LAB_002e89;
              }
            }
            *(undefined1 *)(*(int *)0x8d4a + 7) = (undefined1)iStack_c8;
            FUN_1000_8f5c(*(undefined2 *)0x8d52,param_4,(iStack_80 >> 1) + 1,0);
            uVar12 = 0x181f;
            uVar6 = FUN_1000_8c28(0x181f,param_4,param_5);
            if ((uVar6 & 0x40) != 0) {
              uVar12 = 0x191f;
              func_0x0001938c(0x181f,0x157c,*(undefined2 *)0x8d52);
            }
          }
          else if ((iStack_5e == 3) && (iStack_88 == 0)) {
            FUN_1000_8cdc(uVar12,param_2,iStack_7e);
            iVar8 = *(int *)0x8d4a;
            *(undefined1 *)(iVar8 + 7) = 0xff;
            if ((param_2 == 0xf) || (param_2 == 8)) {
              *(undefined1 *)(*(int *)0x8d4a + 8) = 0xff;
            }
            else {
              *(undefined1 *)(iVar8 + 8) = (undefined1)iStack_c8;
            }
            if (-1 < iStack_c4) {
              iStack_c4 = iStack_c4 + 1;
              FUN_1000_8f5c(*(undefined2 *)0x8d52,param_4,iStack_c4 * -4,0);
              iVar10 = param_4 * 2;
              iVar8 = *(int *)0x8d4a;
              piVar3 = (int *)(iVar8 + iVar10 + 10);
              *piVar3 = *piVar3 + *(int *)0x8dc4 * -2;
              iVar7 = *(int *)(iVar8 + iVar10 + 10);
              if (iVar7 < 0) {
                iVar7 = 0;
              }
              *(int *)(iVar8 + iVar10 + 10) = iVar7;
              if (*(int *)0x8dc4 == 100) {
                *(undefined2 *)(iVar8 + iVar10 + 10) = 0;
              }
            }
            uVar12 = 0x181f;
            if (iStack_c8 == 0xf) {
              *(char *)(*(int *)0x8d4e + 7) = *(char *)(*(int *)0x8d4e + 7) + '\x01';
            }
            if (iStack_c8 == 8) {
              iVar8 = *(int *)0x8d4e;
              *(int *)(iVar8 + 10) = *(int *)(iVar8 + 10) + (*(int *)0x8dc4 >> 2);
              goto LAB_002e86;
            }
            goto LAB_002e89;
          }
          iStack_c6 = 0;
        }
LAB_002e89:
      } while (iStack_6c != 0);
      goto LAB_002e92;
    }
    if (((*(char *)(*(int *)0x8d4a + 8) == iStack_c8) ||
        (*(char *)(*(int *)0x8d4a + 9) == iStack_c8)) || (*(int *)(iStack_c8 * 2 + -25000) == 0)) {
      FUN_1000_8628(0,*(undefined2 *)(iStack_c8 * 2 + -0x6840));
      if (-1 < *(char *)(*(int *)0x8d4a + 8)) {
        *(undefined2 *)(*(char *)(*(int *)0x8d4a + 8) * 2 + -25000) = 0;
      }
      if (-1 < *(char *)(*(int *)0x8d4a + 9)) {
        *(undefined2 *)(*(char *)(*(int *)0x8d4a + 9) * 2 + -25000) = 0;
      }
      FUN_1000_a0c0(0x9e58,unaff_DS,acStack_7c,unaff_SS);
      FUN_1000_8628(1,*(undefined2 *)(cStack_6d * 2 + -0x6840));
      FUN_1000_8628(2,*(undefined2 *)(cStack_6e * 2 + -0x6840));
      uVar12 = 0x181f;
      FUN_1000_8628(3,*(undefined2 *)(cStack_6f * 2 + -0x6840));
    }
    else {
      if (*(char *)(*(int *)0x8d4a + 7) != iStack_c8) goto LAB_002bbc;
      uVar12 = 0x181f;
      FUN_1000_8628(0,*(undefined2 *)(*(char *)(*(int *)0x8d4a + 7) * 2 + -0x6840));
    }
  }
  func_0x0001938c(uVar12);
LAB_003582:
  iVar5 = *(int *)(*(int *)0x8d4a + param_4 * 2 + 10);
  if (iVar5 < 0) {
    iVar5 = 0;
  }
  *(int *)(*(int *)0x8d4a + param_4 * 2 + 10) = iVar5;
  return;
}
```

## Line ranges

| Piece | Decomp lines | Size |
|-------|--------------|------|
| `FUN_4d56_2820` shell | 82064–82282 | ~219 |
| Nest `2aac`…`311e` | 82286–83456 | ~1171 |
| `FUN_4d56_3582` closer | 83460–83476 | ~17 |
| Catalog “~1396” | shell + nest | — |

Sibling (not caller): `FUN_4d56_2154` @81743–82057 — meet economics scorer
([`indian_meet_scoring_2154.md`](indian_meet_scoring_2154.md)), thunk `2a1f_0434`.
(raid-adjacent action). `2820` thunk: `2a1f_044c`. Mid-turn `1b3a` does **not**
call either.

## Call graph

```
thunk 2a1f_044c → FUN_4d56_2820(unit, ?, euro_nation)
  ├─ human Euro gate (control==0) → LCG chrome / BGM 5|6|7
  ├─ clear wagon flag 0x3158 if land unit
  ├─ init cargo-index shuffle arrays; reseed; tribe price table 291f_0ed0
  ├─ param_3 < 0 → LAB_3f41_16ea abort chrome
  ├─ empty cargo → LAB_4d56_2a9b → 3582
  ├─ multi-hold: pick trade good (local_c8) via dialog / AI
  │    LAB_4d56_2a78 cargo pick → common LAB_4d56_2a9b
  └─ dispatch → FUN_4d56_2aac
         ├─ selected_good < 0 → 2e92 (no-deal)
         ├─ AI / non-human (BP−6==0) → 2bbc
         ├─ last-goods conflict → 2b92 (player buy) or refuse subst 0x1561 → 3582
         └─ else refuse path → 3582

FUN_4d56_2b92  player buy-offer loop
  ├─ sticky last-good (tribe+7) → string 0x156a
  ├─ else price from LCG + cargo-type tables (−0x5a base 6/7…)
  ├─ accept → apply gold/goods → maybe 311e demand
  ├─ choice 2 → 2f96 haggle
  └─ choice 3 → 306c hard-bargain

FUN_4d56_2bbc  AI buy-offer (same pricing; auto choices)
FUN_4d56_2e92  no-deal → 311e or 3582
FUN_4d56_2f96  haggle: bump offer/tension; resume loop
FUN_4d56_306c  hard-bargain: worse terms + tension; resume
FUN_4d56_311e  counter-demand tribute goods + buy-back dialog
FUN_4d56_3582  friction / alarm floor helper (post-trade close)
FUN_4d56_2af6  last-goods clear + refuse dialog 0x1561 → 3582
```

## Dialog / string IDs (shell + nest)

| ID | Site | Role |
|----|------|------|
| `0x1561` | `2aac` / `2af6` | Refuse / “not interested” (with tribe name slots) |
| `0x156a` | `2b92` sticky good | Already-trading-that-good line |
| (others in nest) | `2b92`/`2bbc`/`311e` | Buy / haggle / demand bodies — see catalog one-liners; full string census PARKED for VGA |

Subst slots: `281f_0438` slots 0..3 load cargo-name ptrs from table `−0x6840`.
`291f_019c(msg, DS:0x8d52)` presents Indian dialog with nation voice index.

## Shell phases (`2820`)

1. **Peer gate** — `param_3` Euro nation 0..3 with `control==0` → `local_8=1` (human trade UI); else AI silent path.
2. **Chrome** — if human: `range(0,3)==0` may queue BGM events 5/6/7 via `0498`.
3. **Land clear** — non-ship clears `0x3158` (wagon/trade flag).
4. **Tables** — fill 0..15 index arrays; `ai_reseed_from_timer`; `291f_0ed0` builds tribe price vector @ `0x9e78`.
5. **Abort** — `param_3 < 0` → `LAB_3f41_16ea` (UI abort).
6. **Cargo** — empty holds → close via `2a9b`/`3582`; else pick `local_c8` good (human dialog or AI).
7. **Hand off** — `2aac` dispatch.

## Linux thin vs PARKED

| Behavior | Linux (`ai_contact`) | This map |
|----------|----------------------|----------|
| Auto-trade / gift | Real gold debit (2bbc/2820 price formula) | Full `2bbc` / `2b92` pricing |
| Human buy-offer CHOICE (`LAB_002e92` human branch) | `ai_popup` Accept/Decline Done (2026-08-19), locked price shown then charged | Deep Haggle (`2f96`) / hard-bargain counter-offer (`306c`) sub-loops still PARKED; multi-good cargo-select CHOICE (`0x15a0`) not ported (TRADE_GOODS only, matching the AI path's scope) |
| Hard-bargain mid-alarm | Thin Done; primary extra TG for all non-`0xff` teach primaries (Series M) | Full `306c` loop |
| Gift-amount CHOICE | `ai_popup` Done | Deep nest still PARKED |
| VGA wood dialog | PARKED | `291f_019c` / `0438` subst |

## AI buy-offer price formula — resolved (2026-08-13); human CHOICE gate ported (2026-08-19)

The `2b92`/`2bbc` price-formula blocker below is now resolved for the
**AI-controlled Euro peer path** (`LAB_002bbc`, `iStack_8==0` — the one
`ai_contact_auto_trade` actually needs). The human `CHOICE`-dialog path
(`LAB_002e92`, `iStack_8 != 0`) now reuses this same formula/gold-debit for
its own Accept/Decline CHOICE (`ai_contact_enqueue_trade_price_choice` /
`ai_contact_apply_trade_offer` in `ai_contact.c`, wired from the Meet CHOICE
Trade arm) — previously a human picking Trade silently ran the AI's blind
auto-accept with no price shown or player agency at all; now the player
sees the locked price and Accepts/Declines before any gold or goods move.
This reuses the AI formula as the closest already-verified structural
template rather than re-deriving `LAB_002e92`'s own distinct byte-level
price table (see "Open RE" below) — same general shape, not a byte-exact
transcription of the human branch's own constants.

**Key unlock: the Ask/Bid tables `2820` reads (`DS:0x9e58`/`0x9e78`,
`-25000`/`-0x6188`) are not missing data — they're written by
`FUN_4d56_2154`, which is already fully ported** as
`ai_contact_meet_economics_2154` in `ai_contact.c` (`ask[16]`/`bid[16]`,
tested — see `indian_meet_scoring_2154.md`, status "Done"). `2820`'s price
formula is a *consumer* of already-working Linux state, not a fresh
extraction target.

Formula (`iStack_c8` = cargo type, `iStack_6a` = quantity, `aiStack_d6[0]`
= relation via `FUN_1000_84fc` ≈ `ai_diplo_indian_relation`):

```
rng = RNG(1,5)                                           -- dos_rng_range(rng,1,5)
base = (cargo_type > 8) ? 7 : 6
if cargo_type == 0xd (TRADE_GOODS): base -= RNG(0,7)
if cargo_type == 0xf (MUSKETS):     base -= (indian_state[7] - 0xc)
if cargo_type == 8   (HORSES):      base -= (indian_state[8] - 10)
if cargo_type == 0xe (TOOLS):       base += 1

relation_component = f_8c50(relation) << 1     -- FUN_1000_8c50, exact shape not yet traced
if cargo_type in {0xf, 8}: relation_component = 0
if ask[cargo_type] > 19:   relation_component >>= 1

raw = ((base - difficulty) - relation_component + rng + 4) * 2 * ask[cargo_type]
raw = max(raw, 0)
price = (rng*5 + raw) * quantity / 200          -- FUN_0000_e096 signed mul/div by 100, then /2
price = max(price, 1)
```

Then **debits `price` from the Euro nation's gold**
(`col1->nation[e].gold -= price` — confirmed via `param_4*0x13c-0x77cc`/
`-0x77ce`, which `FUN_15eb_0544` already documents as the per-nation
treasury dword) — the AI Euro nation *pays* to buy the cargo. Natives don't
track a numeric gold resource; nothing is credited to the Indian side
beyond relation/production bookkeeping (not yet fully traced — see below).

**Real gap this surfaced:** `ai_contact_auto_trade` currently does
goods/relation bookkeeping only — **no gold ever changes hands**. Not a
conscious deferral in the existing thin stand-in's comments; a genuine
missing behavior this investigation found.

### `indian_state+7`/`+8` — resolved: difficulty-seeded musket/horse throttle

Cargo type 8 = `COLONIZE_CARGO_HORSES`, cargo type 0xf =
`COLONIZE_CARGO_MUSKETS` (`colony.h`) — i.e. the two cargo IDs this formula
special-cases are exactly the two goods Colonization famously restricts
native demand for. Traced `indian_state+7`/`+8` (`DS:0x8d4e+7`/`+8`) to
their owner, `FUN_4d56_1816` (the Indian nation's per-turn tick):

- **Init, once per Euro nation, gated by the same "contact prelude fired"
  flag `ColonizeCol1Indian.unknown31_flags` bit `0x20` already documents**
  (DOS: `indian_state+3` bit `0x20`) — on first real contact (relation
  check + RNG gate), seeds `indian_state[7] = min(prior, difficulty) << 2`,
  `indian_state[8] = min(prior, difficulty)`, `indian_state[10..13]
  (int32) = difficulty * 25`.
- **Decay, every tick**: `if (indian_state[7] > 0) { RNG-gated (probability
  scales with difficulty) decrement by 1 }` — a replenishing-but-draining
  "how many muskets/horses will natives still buy this stretch" throttle,
  not a simple counter.

`ColonizeCol1Indian.unknown33[8]` (currently "opaque in DOS") is unused
padding large enough to host both new counters (`musket_sell_throttle`,
`horse_sell_throttle`, 1 byte each, or the int32-at-+10 companion if that
turns out to matter for the price formula too) without a save-format
break. **Not yet implemented** — this doc records the finding; adding the
fields + wiring init/decay/consume is the next concrete step (needs to
locate `FUN_4d56_1816`'s existing Linux port to hook the same cadence, not
yet located from this pass).

## Open RE

- ~~`FUN_1000_8c50` (relation → price-discount shape) not yet traced~~
  **Resolved 2026-08-14**: thunk → `FUN_15dc_00a2`, a plain quartile
  bucketer on a 0-100 DOS-native scale (`<25→0, <50→1, <75→2, else 3`,
  `viceroy_unpacked.c:9271-9284`), already independently catalogued
  ("Bucket integer into quartile 0..3"). **Wired into
  `ai_contact_2820_ai_buy_price`** (`ai_contact.c`), replacing an
  unverified `(relation>>2)<<1` bit-shift approximation that was also
  missing the 0-255→0-100 rescale entirely (operated on
  `ai_diplo_indian_relation`'s raw 0-255 Linux scale with no conversion —
  a real magnitude bug, not just an approximation, now fixed). `ctest`
  42/43 unchanged (only the known pre-existing unrelated failure), all
  goldens green.
- Human `CHOICE`-dialog buy-offer **gate** (`LAB_002e92` human `iStack_8 != 0`
  branch) is **ported (2026-08-19)**: `ai_contact_enqueue_trade_price_choice`
  / `ai_contact_apply_trade_offer` in `ai_contact.c` replace the old blind
  auto-accept with a real Accept/Decline CHOICE for humans, using the
  already-verified `2bbc`/`ai_contact_2820_ai_buy_price` formula (TRADE_GOODS
  only, matching `ai_contact_auto_trade`'s existing scope). What's still
  genuinely unresolved and **not invented**, honestly left out rather than
  guessed:
  - `LAB_002e92`'s own distinct byte-level price table for a *newly offered*
    good (as opposed to `2bbc`'s sticky-good re-offer table): `*(int*)0x8d4e+2`
    (an indian_state field distinct from the already-resolved `+7`/`+8`
    musket/horse throttle), a per-(Euro-nation, cargo) throttle array at
    absolute `-0x7b44`, and string/format IDs `0x15a9`/`0x2e0c`/`0x2e0e` —
    none of these are captured anywhere in this project (address_mapping.csv
    only maps function entry points, not these DS data offsets); no live
    DOSBox-X session available this pass to trace them
  - The deeper Haggle (`2f96`, "bump offer/tension; resume loop") and
    hard-bargain counter-offer (`306c`) sub-loops that would let a *human*
    push back for more gold instead of a flat Accept/Decline — PARKED, same
    scope discipline as the already-thin Gift/Demand amount CHOICEs
  - The multi-good cargo-select CHOICE (`0x15a0`, picking among up to 3
    tribe-priced goods) — PARKED; the ported path stays TRADE_GOODS-only,
    same as the AI path it reuses
- Where the Indian side's own bookkeeping (production counters at
  `indian_state + cargo*2 + 0xe`) gets updated post-sale — visible in the
  decompile but not yet semantically mapped
- Full string ID list for haggle / demand beyond `0x1561`/`0x156a`
- Second entry into `2820` after `4528` blob (~86762) — confirm args
