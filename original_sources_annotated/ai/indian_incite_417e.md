# `FUN_4d56_417e` — Incite Indians (WARPATH) price + gold deduct

Disassembly-clean, structure fully clarified, **transaction identified**
(2026-08-13, task #5). **Not yet ported to Linux** — two of the formula's
four price terms are unnamed data (need a live memory dump), the caller
is still unfound, and wiring this up is a real new player-facing feature
(a `@ACTIONS` menu item + two-stage dialog), not a small pulse tweak. But
the mechanic itself is now confidently known.

**History on this doc, same day:** first guess was "skill-teach price"
(wrong — teaching is free, user-corrected; see `indians.md`). Then a full
control-flow re-read surfaced a second, named Euro nation in the
transaction. The user recognized the shape immediately: **this is
"Incite Indians" — the Missionary `@ACTIONS` order that bribes a tribe to
go to war with a rival Euro nation.** Confirmed against `GAME.TXT`:

```
@INDIANWARPATH
"The {%STRING0} tribe is ready to go on the warpath.
Whom would you like us to attack?"

@INDIANWARPATH2
"We will gladly drive the {%STRING0} from our ancestral
lands in exchange for {%NUMBER0$}."
Pay {%NUMBER0$}.  /  Never mind.
```

`WARPATH` = the "pick a nation" menu (matches this function's Mode-1
3-nation-menu step exactly). `WARPATH2` = the pay/decline confirm
(matches the final gold-charge dialog). This closes a gap independently
flagged **"incite/WARPATH gold PARKED"** in four separate places already
in this project (`docs/indians.md`, `docs/manual_gap.md`,
`docs/ai_transcription.md`, `original_sources_annotated/ai/indian_contact.md`,
`src/core/ai_contact.c`'s own missionary-pulse comment) — this function
is that missing piece.

## Disassembly status

`OVL13_L0000::417e`, 933 bytes, self-contained, ends right where `4528`
begins (same segment, same clean recovery pass that fixed `4528`/`2820`).
One mild `Removing unreachable block` warning — the ordinary decompiler
noise class, not corruption. Canonical export's `address_mapping.csv` row
is `before-first-function` (no boundary there originally); the overlay
project's own auto-analysis had already recovered a correct boundary
(`FUN_OVL13_L0000__417e_FIXED`) from an earlier pass.

## Caller: still not found

Tried three independent methods this session, same result as the original
investigation:

1. Ghidra reference manager (`getReferencesTo`) on the overlay project:
   zero xrefs.
2. `rtlink_decode VICEROY.EXE`'s own jump-table (the method that resolved
   `a6e4`'s and `4528`'s case-dispatch thunks): no jump-table entry targets
   `(segment 13, offset 0x417e)` — segment 13's only cross-overlay entry
   points are all at offset 0, called from 6 places elsewhere. A resident
   stub (`1000:a5bd JMPF 0x0000:417e`) and an `OVL14` near jump
   (`JMP 0x00417e`) both contain the literal bytes `417e` but are
   confirmed false leads: the first is an unpatched RTLink placeholder
   whose real target (via file-offset lookup) doesn't match; the second
   is `OVL14`'s own *local* offset 0x417e, a different segment entirely —
   numeric coincidence, not a real reference.
3. Whole-block byte scan of `OVL13_L0000` for the raw word `0x417e` (in
   case of a data dispatch table, the same pattern `FUN_15eb_1d4c`'s
   switch used): zero hits.

Conclusion: `417e` is reached only via a plain intra-segment near `CALL`
from somewhere inside `OVL13_L0000` that Ghidra's sweep never disassembled
(a coverage gap), or via a computed/indirect call neither static method
above can resolve. Not chased further — would need a live hang-dump or a
much deeper manual byte walk of `OVL13_L0000`'s undisassembled gaps.

## Parameter / global semantics (cross-referenced against `4528`)

`4528` (already fully ported, `indian_settlement_4528.md`) uses the exact
same resident-utility call idioms this function does. Reading `417e`
against that Rosetta stone:

```c
void FUN_4d56_417e(undefined2 param_1, int param_2, uint param_3, undefined2 param_4)
```

| Symbol | Reading | Evidence |
|--------|---------|----------|
| `param_2` | acting **unit index** — the Missionary performing Incite | `*(char *)(param_2 * 0x1c + 0x315b)` — same `unit*0x1c`-stride record `4528` indexes at `+0x3146`/`+0x3147`/etc. State byte `== 0x18` gives a 1500 discount — plausibly a Jesuit/Expert Missionary flag (Jesuits are canonically better with natives) |
| `param_3` | **`nation_A`** — the inciting nation, charged the gold (0-3) | Indexes `nation*0x13c` tables throughout; final write target `nation[param_3].gold` (see below) |
| `param_4` | still uncertain — not clearly present in `@INDIANWARPATH2`'s text (only one `%STRING0`/`%NUMBER0`) | Fed to `func_0x00018b94(0x181f, param_4)` then `FUN_1000_8628(...)`, same idiom `4528` uses for dialog text — but may be an unused/internal slot for this template. Best current guess: the tribe's own civilization/class, reused by the discount loop to find *other* same-class tribes already favoring `nation_A` (a "you're already trusted by tribes like this one" discount), not something shown to the player |
| `*(int*)0x8d4a` (`VICEROY_DS_CUR_TRIBE_PTR`) | current tribe record | Already named in `viceroy_globals.h`; discount loop reads `+2` (type-ish byte) and `+5&0xf` (nation nibble) — same fields `4528` reads from the identical pointer |
| `*(int*)0x8d4e` (`VICEROY_DS_INDIAN_STATE_PTR`) | Indian-nation state block | Already documented as "signed bytes at +7/+8" — read here (`*2` each) as two of the four additive price terms |
| `*(int*)0x8d52` (`VICEROY_DS_CUR_INDIAN_ALT`) | holds a pointer, dereferenced with `-0x69d6`/`-0x6e7c` | Two more price terms, byte-table lookups by whatever `CUR_INDIAN_ALT` currently points to (likely tribe civilization/sophistication class) |
| `nation[param_3].gold` at `param_3*0x13c - 0x77cc` / `-0x77ce` | the actual charge target | Matches `difficulty.md`'s already-documented `nation.gold (+0x2a/+0x2c)` field exactly — same `nation*0x13c` stride, 2-byte offset gap between the two halves of the 32-bit value. Linux home: `col1->nation[n].gold` (`europe.h`) |
| `FUN_1000_84fc(dialog, a, b)` | relation/eligibility **score** (0-100ish) | Same function `4528` uses, compared there against 75/50/25 thresholds; here used once as the price *divisor* (`iVar3 + 0x4b`) and again later as a straight eligibility gate (`< 0x4b` → reject) |
| `FUN_1000_8c28(dialog, a, b)` | diplomacy/relation **flags** byte | Same function `4528` uses; `& 0x20` bit gates both — "peaceful enough" |
| `FUN_1000_935a` | affordability/gold check | Called on the computed price; non-affordable path takes the "reject, no dialog" branch |
| `FUN_1000_8842(dialog, id, 1)` | show CHOICE popup | Same call shape `2820`/`4528` use for their trade/raid dialogs |

## Full control flow (re-derived 2026-08-13, complete walkthrough)

This is a **three-party** transaction, not just "nation pays tribe" —
re-reading the whole function end to end surfaced a second Euro nation
(`uStack_14`) that earlier passes hadn't clearly separated out.

**1. Price (always computed first)**, `nation_A` = `param_3`:

```
base  = table[CUR_INDIAN_ALT deref, -0x69d6] * 8
      + ((table[CUR_INDIAN_ALT deref, -0x6e7c] >> 2 & 0xfe) - 2*table[-0x69d6])
      + INDIAN_STATE.signed_byte[7] * 2
      + INDIAN_STATE.signed_byte[8] * 2
price = base / (relation_score(nation_A) + 0x4b)          // FUN_0000_e130, signed long div
if param_3 == 1: price = rescale(price, factor 3)          // FUN_0000_e096, param_3==1 special-case
for each tribe record matching (type == param_4, owner_nibble == nation_A):
    discount = (tribe.flags & 0x10) ? 1000 : 250            // x2 if road/river flag (&4) set
    price -= discount
if unit(param_2)'s own state byte == 0x18: price -= 1500
if INDIAN_STATE flags & 4:                 price -= 500
price = max(price, 500)                                     // explicit floor
```

**2. Pick `nation_B` and gate**, mode selected by
`param_3 < 4 AND nation_A's per-nation flag byte == 0`:

- **Mode 1** (the "normal" path): if NEW-WORLD-scenario map, **build a
  menu of the other 3 Euro nations** (skipping `nation_A` and the
  crown/peer nation) and something selects one → `nation_B`. If
  AMERICA-scenario map, `nation_B` is just fixed to the crown/peer
  nation, no menu. Then: diplomacy-flags gate vs `nation_B`
  (`FUN_1000_8c28 & 0x20`) — fail → one informational message
  (string `0x16b7`), stop, no charge. Pass → show a **confirm dialog**
  (string `0x16c1`); decline → stop. Then affordability check
  (`nation_A.gold >= price`, plain 32-bit compare — not a separate
  threshold table, just re-reading current gold) — fail → different
  informational message (`0x16d0`). Pass → step 3.
- **Mode 2** (`param_3>=4` or the flag byte is set — reads as an
  AI-nation-only / no-menu path): `nation_B` fixed to **FOCUS_NATION**.
  Diplomacy-flags gate vs `nation_B`, then a plain relation-score gate
  (`>= 0x4b`/74), then the same affordability check. No menu, no confirm
  dialog — straight through on pass.

**3. Execute (both modes converge here):** CHOICE dialog, string id
`0x16e9` — almost certainly `@INDIANWARPATH2` ("We will gladly drive the
{nation} from our ancestral lands in exchange for {price}." / Pay / Never
mind). Four dialog-field slots get filled before the call
(`param_4`'s name, `nation_A`'s name, `param_4`'s formatted value,
`nation_B`'s name), but the template only references one `%STRING0` and
one `%NUMBER0$` — likely `nation_B`'s name and the price, with the other
two slots unused by this particular template (common in a shared
dialog-building routine). Then a call shaped
`apply(CUR_INDIAN_ALT, nation_B, 100, 0)` — the actual **incite**: almost
certainly the alarm/war-relation push that sets the tribe against
`nation_B`, magnitude 100. Then: **`nation_A.gold -= price`** (32-bit
subtract with borrow, confirmed in the raw disassembly's tail).
`nation_B`'s gold is never touched — B is the *target*, not a payer.

## Confirmed: this is "Incite Indians" (WARPATH)

Player-facing Missionary `@ACTIONS` order (full list, `docs/indians.md`):
*Trade With Village, Enter Hostile Village, Establish Mission, Denounce
Heresy, Live Among The Natives, Speak With Chief, **Incite Indians**,
Demand Tribute, Attack Village.* A missionary at/adjacent to a tribe
chooses Incite; the tribe (`WARPATH`) asks which of the other Euro
nations to attack; accepting and paying (`WARPATH2`) sets that tribe
against the chosen rival. Maps cleanly onto every structural finding:
three parties (payer/inciter, target rival, tribe), a nation-picker menu
(`WARPATH`'s "whom would you like us to attack?"), a pay/decline confirm
(`WARPATH2`), gold leaving only the inciter's treasury, and a relational
effect landing on the target nation rather than a payment to it.

**Two modes now read as**: Mode 1 (menu-driven) = the normal player
path on a NEW WORLD map, where any of the 3 other Euro nations can be
targeted. Mode 2 (fixed target, relation-score-gated, no confirm dialog)
= plausibly the **AI-nation** version of the same order — an AI Euro
nation deciding to incite against whichever nation is currently in
focus, with a straight relation-score threshold instead of a human
confirm click. Fits `param_3>=4`/flag-byte gating being an
"AI shortcut" path. `param_1`'s exact role (still unconfirmed) is a
reasonable place to look for a human-vs-AI selector if anyone traces
this further.

**Remaining unknowns:**
- Exact byte values in the two price-formula lookup tables
  (`CUR_INDIAN_ALT`-relative `-0x69d6`/`-0x6e7c`, tribe civilization/
  sophistication class) — need a live memory dump, not just static
  disassembly.
- `param_4`'s precise role — best guess above (tribe's own class, for
  the same-class discount), not confirmed against the dialog text.
- The caller — still not found (3 independent static methods tried, see
  above); would confirm exact trigger conditions and `param_1`.
- Whether the `+100` relation push is toward war/alarm specifically vs.
  some other effect — not traced past the call site.

**Not yet wired into Linux.** This is a real, currently-**absent**
feature (no `AI_POPUP_TAG_CONTACT_INCITE` or equivalent exists in
`ai_popup.h`, confirmed 2026-08-13) — every other `AI_POPUP_TAG_CONTACT_*`
mechanic (Meet/Teach/Gift/Demand/Raid/Convert/Welcome/Refuse) has one,
Incite doesn't. Unlike the teach-skill fix earlier this session, this
isn't editing working behavior — it's filling a total gap — so a
reasonable first-draft implementation (documented approximations for the
two unknown table values) is lower-risk than usual and worth attempting
once the player-facing unit-order integration point is scoped.
