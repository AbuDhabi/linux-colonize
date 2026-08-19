# `FUN_5bfb_3180` — encounter/adjacency handler, full clean recovery (2026-08-14)

## What it is

Found as `153e`'s only caller (per last pass's investigation). **Real role
is broader than "calls 153e"**: this is the **adjacent-unit encounter
resolver** — scans all 8 tiles around `(param_2, param_3)` for a foreign-
nation unit, and for each found:

1. **Naval ambush roll** (ships only, type ∈ Man-O-War/Frigate/Privateer):
   combines both units' combat-power thunks (`thunk_FUN_2a1f_0626` =
   `FUN_5bfb_312e`, not traced) into an RNG roll (`FUN_281f_04d4`); on a
   hit, fires one of two message popups (`0x1a49` win / implied loss path)
   with unit-name substitutions, and adds to the encountering unit's
   `unit+0x3149` "spent" field (already-known MP-spend byte).
2. **A second, distinct check** slightly further down (message `0x1a5a`) —
   **disentangled this pass, correctly stays PARKED**: unlike (1) it is
   *not* a random-roll ambush. It's gated on `FUN_281f_09fc(1)` /
   `FUN_281f_09fc(2)` (two boolean queries against some flag/state) and,
   on a hit, adds a **flat** constant (`+2` or `+50`, literal, not
   type-scaled) to the encountering unit's `0x3149` spend byte, then reads
   `*(int*)0x8f8e` or `*(int*)0x8f9a` purely as a **message-format
   parameter** for popup `0x1a5a` (passed to `FUN_281f_0438` arg-slot 2,
   the same call pattern every other popup here uses for a numeric fill-
   in) — not a further state mutation. So semantically this is "a flavor-
   text encounter penalty gated by some game/diplomatic flag," not combat.
   **Traced `FUN_281f_09fc` and confirmed it's a dead end for now**: both
   its call-site alias (`281f:09fc`) and its RAM alias
   (`FUN_1000_8bec`/`18bec`, `address_mapping.csv` "exact") decompile
   identically to a bare 2-call trampoline (`FUN_1000_1e61(); FUN_0000_623e();`,
   both effectively no-arg/1-arg stubs) despite every call site passing 1-2
   real arguments — a naked-asm calling-convention thunk Ghidra can't
   recover params for, same class as the unit-capability-bitmask dead end
   from earlier sessions. Real behavior would need raw `.asm` register
   tracing of the callee chain, for a payoff that's just flavor text +
   a small fixed MP penalty. Not worth it this pass — left PARKED.
3. **Records the encounter direction**: `neighbor_unit+0x314f = local_30`
   (the 0-7 loop index) — **confirms and extends** the `unit+0x314f` finding
   from `20e6`/last pass: it's written here too, on the *other* unit in an
   encounter, most likely as "which direction did the encounter come from"
   rather than purely "last movement direction" — the two uses (movement
   momentum in `20e6`, encounter direction here) may be the same DOS field
   doing double duty, or this write may get overwritten by the unit's own
   next `20e6` call anyway. Not fully reconciled — flagged, not assumed.
4. **Diplomatic dispatch by nation-pair type**:
   - Euro-vs-Euro (`local_46 < 4`, WoI flag clear): calls `thunk_FUN_2a1f_05fc`
     = **`FUN_5bfb_153e`** (the war-declare deep body mapped last pass).
   - Euro-vs-Indian: calls `thunk_FUN_2a1f_066c` = **`FUN_5bfb_022e`**,
     the Indian meet/contact dispatcher **already fully ported**
     (`ai_contact.c`, ai_popup `AI_POPUP_TAG_CONTACT_*` chain).
   - Indian-vs-Indian: skips both, instead syncs a small per-tribe-pair
     byte table (`DS:0x5ade`, stride `0x4e` — matches the already-known
     Indian nation-record stride from `FUN_15b3_0004`'s `param*0x4e+23000`
     addressing, though `0x5ade` itself is a different, unnamed table) to
     the max of the two tribes' current values — not resolved further.
   - On a "successful" diplomatic outcome, calls `switchD_2000:da9f::caseD_10`
     with bit `0x20`. **Corrected 2026-08-14** (was mischaracterized as "a
     generic cross-overlay event dispatch, case 0x20, not traced"):
     `caseD_10` is not a dispatcher at all — it's a Ghidra-named single-case
     thunk straight to `FUN_15b3_0066` (or-diplomacy-bit-both-directions,
     already known per `FUNCTION_CATALOG.md`, already ported as
     `ai_diplo_or_both`). So this call is simply
     `ai_diplo_or_both(uVar7, local_4, 0x20)` — OR bit `0x20` between the two
     encountering units' nations. `0x20` is the same unmapped bit seen in
     `FUN_43f7_0108` (see `king_ref.md`'s 2026-08-14 note) — two independent
     sites now OR the same bit on "positive outcome," which is suggestive
     but still not enough to name it (no bit-read site found yet that would
     pin down its effect). Still correctly PARKED alongside the rest of
     `3180`'s diplomatic-dispatch branches, just for a narrower reason now.

**This resolves last pass's open question cleanly**: `153e` is not a
periodic turn-based "should I declare war" check — it's triggered by
**physical adjacency between units of different nations**, alongside the
*already-fully-ported* Indian meet/contact path as its structural sibling.
Confirms `153e` is a real, missing piece of Linux's Euro-Euro first-contact
handling, not a duplicate of anything already covered.

## Recovery notes

Two independent recoveries (canonical `viceroy_unpacked.c:98457-98695`,
239 lines, zero warnings; fresh `GhidraDecompileAt OVL16_L0040:3580`, 238
lines, zero warnings) **agree on content** — an earlier same-pass panic
("427 vs 239 lines, is one truncated?") was a **false alarm caused by a
bad boundary check on my end**: I searched for the next
`__cdecl16far FUN_` declaration to bound canonical's search, but a
different-signature helper sits between `3180`'s real end (line 98695,
found by searching for the next column-0 `}`) and that next matching
declaration (line 98884) — those in-between lines are unrelated content
(likely the small helpers `address_mapping.csv`'s ~1.6KB post-`3180` gap
already flagged), not part of `3180` at all. Lesson: bound canonical
function searches by the nearest closing `^}`, not by the next differently-
patterned declaration — the two aren't always adjacent.

## Two of the three helpers resolved, same day

- **`FUN_5bfb_312e` (ambush combat-power proxy) — resolved, trivial.**
  `movement_points(unit) + 3, ×2 if Man-O-War, +3 if Frigate, −4×cargo_
  holds_occupied (already-known unit+0x3150), floor 1`. Movement points
  itself comes from `FUN_1427_065a` = unit-type table `type*0xe+0x5234`
  (the movement field, already identified during the unit-type-loader
  investigation two passes ago) **+3 if the nation has a specific
  Founding Father** (`FUN_15eb_3960(nation, 5)` — index 5, not identified,
  but the shape matches the already-documented "Magellan +1 sea MP"
  manual-aligned effect; could be that FF or a different one, not
  confirmed). So: naval ambush odds are **speed/maneuverability-based**,
  not raw attack/defense — faster, less-laden ships get better ambush
  odds. Genuinely simple, no remaining unknowns in this specific chain.
- **`FUN_281f_0366` (attacker/defender role resolver) — resolved,
  trivial.** Thunks to `FUN_124c_002a`, a two-line pointer swap
  (`*a, *b = *b, *a`). So the "resolver" is just "swap self/other nation
  id" for whichever branch needed the opposite ordering — not a scoring
  function at all.
- `switchD_2000:da9f::caseD_10` (generic cross-overlay event dispatch,
  case `0x20`) — **not traced**, would need the jump-table resolution
  method from earlier in this session (`a6e4`/`4528` case-thunks); lower
  priority, fires only as a side effect after a successful outcome, not
  part of the core decision logic.
- The `unit+0x314f` double-duty question (movement-momentum vs encounter-
  direction) — **not resolved**.
- `DS:0x5ade` (Indian-vs-Indian sync table) — **not named**.
- **Doc-sync 2026-08-19: the ambush sub-mechanic (item 1 above) IS ported**
  — `ai_euro_naval_try_ambush`/`ai_euro_naval_ambush_power` (`ai_euro.c`),
  confirmed live and cited in `docs/ai_transcription.md`'s `3180` row.
  This paragraph's "Not ported" verdict was stale; only the surrounding
  **diplo-dispatch** (item 4: Euro-vs-Euro → `153e`, Indian-vs-Indian
  `0x5ade` sync, the `0x20`-bit OR outcome) remains genuinely PARKED —
  blocked on `153e`'s own still-unresolved selector/worthiness-score logic
  (see `euro_diplo_153e_full.md`), not on anything in this function. The
  Indian-vs-Indian `0x5ade` sync is separately low-value to wire blind:
  self-contained (no `153e` dependency) but has zero known reader in any
  export, so it would be a write-only stub with no confirmed effect.

## Raw recovered C (canonical version, 239 lines, zero warnings)

```c
void __cdecl16far FUN_5bfb_3180(int param_1,int param_2,int param_3)

{
  char *pcVar1;
  int iVar2;
  int iVar3;
  uint uVar4;
  undefined2 uVar5;
  uint uVar6;
  uint uVar7;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  uint uVar8;
  uint local_46;
  int local_44;
  uint local_42;
  int local_40;
  uint local_3e;
  int local_3c;
  undefined2 local_3a;
  int local_38;
  int local_36;
  uint local_34;
  int local_32;
  int local_30;
  int local_2e;
  int local_2c;
  int local_2a;
  int local_28;
  int local_26;
  int local_24;
  char local_22;
  int aiStack_20 [12];
  uint local_8;
  int local_6;
  uint local_4;
  
  local_3a = 0;
  for (local_38 = 0; local_38 < 0xc; local_38 = local_38 + 1) {
    aiStack_20[local_38] = 0;
  }
  local_44 = param_1;
  uVar7 = *(byte *)(param_1 * 0x1c + 0x3147) & 0xf;
  local_3e = FUN_281f_090c(0x5bfb,param_1);
  local_3e = local_3e & 0xff;
  local_36 = FUN_281f_0768(0x281f,param_2,param_3);
  iVar2 = FUN_281f_0696(0x281f,param_2,param_3);
  local_34 = (uint)(-1 < iVar2);
  local_30 = 0;
  do {
    if (7 < local_30) {
      return;
    }
    uVar6 = *(char *)(local_30 + 0xbe) + param_3;
    local_40 = *(char *)(local_30 + 0xb4) + param_2;
    local_42 = FUN_281f_06be(0x281f,local_40,uVar6);
    uVar8 = 0xf616;
    local_44 = FUN_281f_07e0(0x281f);
    local_46 = local_42;
    if (-1 < local_44) {
      local_46 = *(byte *)(local_44 * 0x1c + 0x3147) & 0xf;
    }
    local_8 = uVar7;
    local_4 = local_46;
    if ((-1 < (int)local_46) && (uVar7 != local_46)) {
      iVar2 = param_1 * 0x1c;
      if ((0xc < *(byte *)(iVar2 + 0x3146)) &&
         (((((*(byte *)(iVar2 + 0x3146) < 0x13 && (local_36 != 0)) &&
            (uVar8 = uVar6, iVar3 = FUN_281f_0768(0x281f,local_40,uVar6), iVar3 != 0)) &&
           (-1 < local_44)) &&
          ((uVar4 = FUN_281f_0a38(0x281f,local_8,local_46), (uVar4 & 0x40) == 0 ||
           (*(char *)(iVar2 + 0x3146) == '\x10')))))) {
        local_2e = local_44;
        while ((-1 < local_44 && (*(byte *)(param_1 * 0x1c + 0x3149) < (byte)local_3e))) {
          if ((0xc < *(byte *)(local_44 * 0x1c + 0x3146)) &&
             (*(byte *)(local_44 * 0x1c + 0x3146) < 0x13)) {
            local_2c = 0;
            if (*(char *)(local_44 * 0x1c + 0x3146) == '\x10') {
              local_2c = 4;
            }
            if (*(char *)(local_44 * 0x1c + 0x3146) == '\x11') {
              local_2c = 6;
            }
            if (*(char *)(local_44 * 0x1c + 0x3146) == '\x12') {
              local_2c = 8;
            }
            if (local_2c != 0) {
              local_6 = thunk_FUN_2a1f_0626(0x281f,param_1);
              local_2a = thunk_FUN_2a1f_0626(0x281f,local_44);
              local_2a = local_2a + 2;
              local_28 = local_2a + local_6;
              uVar8 = 1;
              local_26 = FUN_281f_04d4(0x281f,1,local_28);
              if (local_26 < local_6) {
                local_2c = 0;
              }
              else if (local_26 == local_6) {
                local_2c = local_2c >> 1;
              }
              if ((local_2c == 0) &&
                 ((((int)local_8 < 4 && (*(char *)(local_8 * 0x34 + 0x543f) == '\0')) ||
                  (((int)local_46 < 4 && (*(char *)(local_46 * 0x34 + 0x543f) == '\0')))))) {
                FUN_281f_0438(0x281f,0,*(undefined2 *)
                                        ((uint)*(byte *)(param_1 * 0x1c + 0x3146) * 0xe + 0x5230));
                uVar8 = local_46;
                uVar5 = FUN_281f_09a4(0x281f,local_46);
                FUN_281f_0438(0x281f,1,uVar5);
                uVar5 = FUN_281f_09a4(0x281f,local_8);
                FUN_281f_0438(0x281f,2,uVar5);
                FUN_281f_0438(0x281f,3,*(undefined2 *)
                                        ((uint)*(byte *)(local_44 * 0x1c + 0x3146) * 0xe + 0x5230));
                FUN_281f_0652(0x281f,0x1a49,0);
              }
            }
            if (local_2c != 0) {
              pcVar1 = (char *)(param_1 * 0x1c + 0x3149);
              *pcVar1 = *pcVar1 + (char)local_2c;
              FUN_281f_0438(0x281f,0,*(undefined2 *)
                                      ((uint)*(byte *)(param_1 * 0x1c + 0x3146) * 0xe + 0x5230));
              uVar8 = local_4;
              uVar5 = FUN_281f_09a4(0x281f,local_4);
              FUN_281f_0438(0x281f,1,uVar5);
              FUN_281f_0438(0x281f,2,*(undefined2 *)
                                      ((uint)*(byte *)(local_44 * 0x1c + 0x3146) * 0xe + 0x5230));
              if (((int)local_8 < 4) && (*(char *)(local_8 * 0x34 + 0x543f) == '\0')) {
                uVar8 = 0;
                FUN_281f_0652(0x281f,0x1a51,0);
              }
            }
          }
          local_44 = FUN_281f_02e4(uVar8,0x281f);
        }
        local_44 = local_2e;
      }
      local_42 = (uint)(-1 < (int)local_42);
      iVar2 = param_1 * 0x1c;
      if (((((0xc < *(byte *)(iVar2 + 0x3146)) && (*(byte *)(iVar2 + 0x3146) < 0x13)) &&
           (local_42 != 0)) &&
          ((*(byte *)(iVar2 + 0x3149) < (byte)local_3e &&
           ((uVar8 = local_46, uVar4 = FUN_281f_0a38(0x281f,local_8,local_46), (uVar4 & 0x40) == 0
            || (*(char *)(iVar2 + 0x3146) == '\x10')))))) &&
         (local_3c = FUN_281f_07be(0x281f,local_40,uVar6), -1 < local_3c)) {
        FUN_281f_09e6(0x281f,local_3c);
        local_2c = -1;
        iVar2 = FUN_281f_09fc(0x281f,2);
        if (iVar2 == 0) {
          iVar2 = FUN_281f_09fc(0x281f,1);
          if (iVar2 != 0) {
            pcVar1 = (char *)(param_1 * 0x1c + 0x3149);
            *pcVar1 = *pcVar1 + '\x02';
            local_2c = *(int *)0x8f8e;
          }
        }
        else {
          pcVar1 = (char *)(param_1 * 0x1c + 0x3149);
          *pcVar1 = *pcVar1 + '2';
          local_2c = *(int *)0x8f9a;
        }
        if (-1 < local_2c) {
          FUN_281f_0438(0x281f,0,*(undefined2 *)
                                  ((uint)*(byte *)(param_1 * 0x1c + 0x3146) * 0xe + 0x5230));
          uVar8 = local_4;
          uVar5 = FUN_281f_09a4(0x281f,local_4);
          FUN_281f_0438(0x281f,1,uVar5);
          FUN_281f_0438(0x281f,2,local_2c);
          if (((int)local_8 < 4) && (*(char *)(local_8 * 0x34 + 0x543f) == '\0')) {
            uVar8 = 0;
            FUN_281f_0652(0x281f,0x1a5a,0);
          }
        }
      }
      if ((((local_34 != 0) || (local_42 != 0)) ||
          (uVar8 = uVar6, iVar2 = FUN_281f_0768(0x281f,local_40,uVar6), iVar2 == local_36)) &&
         (-1 < local_44)) {
        local_2e = local_44;
        while (-1 < local_44) {
          iVar2 = local_44 * 0x1c;
          if (*(char *)(iVar2 + 0x314c) == '\x01') {
            if ((*(byte *)(iVar2 + 0x3146) < 0xd) || (0x12 < *(byte *)(iVar2 + 0x3146))) {
              uVar8 = (uint)*(byte *)(local_44 * 0x1c + 0x3145);
              iVar2 = FUN_281f_0768(0x281f,*(undefined1 *)(local_44 * 0x1c + 0x3144),uVar8);
              if (iVar2 != 0) goto LAB_5bfb_360e;
            }
            *(undefined1 *)(local_44 * 0x1c + 0x314c) = 0;
          }
LAB_5bfb_360e:
          local_44 = FUN_281f_02e4(uVar8,0x281f);
        }
        local_44 = local_2e;
      }
    }
    if (((local_36 == 0) && (iVar2 = FUN_281f_0768(0x281f,local_40,uVar6), iVar2 == 0)) &&
       ((-1 < (int)local_4 && ((uVar7 != local_4 && (aiStack_20[local_4] == 0)))))) {
      if (-1 < local_44) {
        *(undefined1 *)(local_44 * 0x1c + 0x314f) = (undefined1)local_30;
      }
      if ((uVar7 < 4) || ((int)local_4 < 4)) {
        if ((uVar7 < 4) && (*(char *)(uVar7 * 0x34 + 0x543f) == '\0')) {
          local_3a = 1;
        }
        else {
          if (((int)local_4 < 4) && (*(char *)(local_4 * 0x34 + 0x543f) == '\0')) {
            local_3a = 1;
          }
          else if ((uVar7 < 4) || (3 < (int)local_4)) goto LAB_5bfb_3752;
          FUN_281f_0366(0x281f,&local_8,&local_46);
        }
LAB_5bfb_3752:
        if ((int)local_46 < 4) {
          if ((*(byte *)0x5382 & 1) == 0) {
            iVar2 = thunk_FUN_2a1f_05fc(0x281f,local_8,local_46,param_1,local_30,0);
            goto LAB_5bfb_3798;
          }
        }
        else {
          iVar2 = thunk_FUN_2a1f_066c(0x281f,local_8,local_46,param_1,local_30);
LAB_5bfb_3798:
          aiStack_20[local_4] = iVar2;
        }
        if (aiStack_20[local_4] != 0) {
          switchD_2000:da9f::caseD_10(0x281f,uVar7,local_4,0x20);
        }
      }
      else {
        local_32 = local_4 - 4;
        iVar2 = local_32 * 0x4e;
        local_24 = uVar7 - 4;
        local_22 = *(char *)(local_24 * 0x4e + 0x5ade);
        if (local_22 < *(char *)(iVar2 + 0x5ade)) {
          local_22 = *(char *)(iVar2 + 0x5ade);
        }
        *(char *)(local_24 * 0x4e + 0x5ade) = local_22;
        *(char *)(iVar2 + 0x5ade) = local_22;
        aiStack_20[local_4] = 1;
      }
    }
    local_30 = local_30 + 1;
  } while( true );
}
```

## Implementation status (2026-08-14)

**Naval ambush sub-mechanic (item 1): Done.** Ported as
`ai_euro_naval_ambush_power`/`ai_euro_naval_try_ambush` in `ai_euro.c`,
called from `ai_euro_unit_act`'s `is_ship` block, right before that
block's `return;`. Own type gated to Man-O-War/Frigate/Privateer; scans
8 neighbors for the first foreign-nation ship; power = `movement*3+3`
(×2 for MoW, +3 for Frigate), minus `4*cargo_count`, floored at 1; rolls
`dos_rng_range(1, self_power+foe_power+2)`; a roll over self_power drains
`moves_left` by a type constant (4/6/8 for MoW/Frigate/Privateer), floored
at 0. Non-destructive (no combat), matches DOS's MP-spend semantics.

**Placement bug (found and fixed this pass):** the call was first wired
at the very tail of `ai_euro_unit_act`, after the land-only "Case 0x0b
land" section — but every ship path in that function `return;`s at the
end of its own `is_ship` block, well before that tail. Result: the ambush
check was dead code for every ship, silently never reached (moves_left
never changed, no crash, no test failure — the dedicated test originally
accepted "5→5 unchanged" as a valid outcome, which masked it for 11
different RNG seeds in a row). Moved the call inside the `is_ship` block;
confirmed fixed by sweeping seeds 1–12, now seeing both outcomes (~8/12
ambush, ~4/12 no-ambush, consistent with the ~64% hit rate the formula
predicts for the test's Frigate-vs-Man-O-War setup).

**Approximated, not byte-exact:** DOS's real RNG call site
(`FUN_281f_04d4`) and the tie/partial-drain case (`local_2c >> 1`) are not
reproduced; ties here resolve to "no ambush" instead of a half-drain.

**Explicitly parked, out of scope for this pass:** item 2 (the second,
undisentangled roll), item 3 (`unit+0x314f` double-write — encounter
direction here vs. movement momentum in `20e6`, not reconciled), and all
of item 4 (diplomatic dispatch: Euro-vs-Euro → `153e` war-declare is
itself parked — see `euro_diplo_153e_full.md`; Euro-vs-Indian already
covered by the existing `ai_contact.c` meet dispatcher independent of this
path; Indian-vs-Indian `DS:0x5ade` sync table; the `caseD_10`/`0x20` bit —
identified 2026-08-14 as `ai_diplo_or_both(.., 0x20)`, unmapped bit, see
above).
