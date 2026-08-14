# `FUN_4d56_417e` — Incite Indians (WARPATH) price + gold deduct

Disassembly-clean, structure fully clarified, transaction identified and
**ported to Linux** (2026-08-13, task #5, closed). First-draft quality —
see "Ported to Linux" below for exactly what's faithful vs. approximated.

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
  nation, no menu. Then: diplomacy-flags gate — **`FUN_1000_8c28(dialog,
  param_4, nation_B)`, correction 2026-08-13: this is `param_4` paired
  against `nation_B`, not `nation_A`** (verified against both call sites
  in the raw decompile; earlier draft of this doc mis-stated it) — fail →
  one informational message (string `0x16b7`), stop, no charge. Pass →
  show a **confirm dialog** (string `0x16c1`); decline → stop. Then
  affordability check (`nation_A.gold >= price`, plain 32-bit compare —
  not a separate threshold table, just re-reading current gold) — fail →
  different informational message (`0x16d0`). Pass → step 3.
- **Mode 2** (`param_3>=4` or the flag byte is set): `nation_B` fixed to
  **FOCUS_NATION**. Same `FUN_1000_8c28(dialog, param_4, nation_B)`
  diplomacy gate, then a plain relation-score gate (`>= 0x4b`/74), then
  the same affordability check against `nation[param_3]`. No menu, no
  confirm dialog — straight through on pass. **Live-captured 2026-08-13
  (see "Live debugger capture" below): confirms `param_4` pairs with a
  nation argument, not a flat "type" — strengthens the "`param_4` = the
  acting tribe's own identity" reading over "a skill/good type".**

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
  the same-class discount and the diplomacy check), not confirmed
  against the dialog text.
- The caller — still not found via static methods (see below); a live
  capture traced it to RTLink's own generic overlay-call trampoline
  (infrastructure, not game logic), so the real trigger context is
  still open.
- Whether the `+100` relation push is toward war/alarm specifically vs.
  some other effect — not traced past the call site.

## Live debugger capture (2026-08-13)

User ran a patched `VR417E.EXE` (`EB FE` self-loop trap at the entry,
same technique as the `VR4528.EXE` control build) under DOSBox-X's live
debugger and got a genuine hit — `CS:IP = E2AF:417E` exactly. Recovered
the stack (`SS:SP = 237D:E75E`) via a second debugger text capture and
decoded the pushed args by hand.

**Correction (still 2026-08-13) — the first parameter-order read below
was wrong, fixed after a second capture caught the same call again from
a different trap point and let it be cross-checked against the
decompile's own body code.** Ghidra's `param_1` is not the first stack
slot for this far-call signature — cross-referencing `[BP+0x6]` against
`*(char *)(param_2 * 0x1c + 0x315b)` in the body (the unit-state check),
`[BP+0x8]` against `param_3 < 4` / `param_3 * 0x34 + 0x543f` (the mode
gate, two independent hits), and `[BP+0xA]` against
`func_...8b94(0x181f, param_4)` (confirmed identically in *both*
captures) pins the real layout: **`param_2`=`[BP+0x6]`,
`param_3`=`[BP+0x8]`, `param_4`=`[BP+0xA]`**. `param_1` doesn't appear
anywhere in the decompiled body at all — likely register-passed (`AX`)
rather than a 4th stack slot; `[BP+0xC]` reads as `0xF` (15) in both
captures but is more likely leftover caller-stack content than a real
argument.

A second capture (`VR417E1.EXE`, trapping the Mode-1 entry specifically)
landed on **`param_2=38, param_3=0, param_4=11`, return address
`1930:1554`** — bit-for-bit identical to the first capture's corrected
values. Same real call, replayed from a reloaded save/state and caught
at two different points, not two different events. Real, confirmed data
for one genuine "player picks Incite Indians" action:

```
param_2 (unit)   = 38   -- the missionary performing the order
param_3 (nation) = 0    -- English (a real Euro nation, 0-3 — NOT Indian
                            nation 11 as the uncorrected first read said)
param_4          = 11   -- consistent both times; still read as the
                            acting tribe's own identity/Indian-nation id
```

Since `param_3=0 < 4` and the second trap only fires past the Mode-1
gate, **this positively confirms Mode 1 (menu + confirm dialog) as the
real path for a player-initiated Incite** — not the Mode 2/AI-shortcut
reading the first (uncorrected) capture suggested. **This walks back the
earlier "checked `nation[11]`, found zero, explains the whole mechanic"
conclusion** — that checked the wrong nation's memory (`param_3` was
never 11); the zero-value observation at that address is still a true
fact, it just doesn't explain *this* call. Whether Mode 2's "AI nation
auto-incites" reading is even real is now unconfirmed again — no capture
has actually landed there yet.

**Chased the caller for both captures.** Both times: the immediate
return address points into a `CALLF 0000:0000` (unpatched RTLink
placeholder, same class as `a6e4`/`4528`'s case-thunks) in resident
memory at `ram:0x1261f`, identified via a distinctive
`"<Return Vector>"` debug string sitting next to it in the static
`resident.bin` (cross-referenced live memory bytes against the
extracted per-segment `.bin` files rather than trying to decode
DOSBox-X's `.sav` `CPU` component, which turned out to not be a simple
flat physical dump). Disassembled the surrounding code: heavy
register-save / stack-segment-switch pattern — this is **RTLink's own
generic overlay-call trampoline**, confirmed reused across both captures
(same static call site, dynamically re-patched to a different target
segment each session — `E2AF` first capture, `D6A2` second, exact same
`ram:0x1261f` origin and identical `1930:1554` return address both
times, since resident itself loads at a stable address session to
session while overlays don't). Not game-specific logic — every overlay
call in the game likely funnels through this exact spot, so finding it
doesn't pin the gameplay trigger the way a real caller would. Caller
identity for a *specific* trigger context (one level further up, whoever
calls the trampoline) remains open.

**Caller hunt closed out (2026-08-13).** A third patched build
(`VR417ET.EXE`, trapping the trampoline's own `CALLF` one level earlier)
never fired — most likely because `OVL13` was already warm-loaded by the
time Incite was tried, so RTLink skipped the "ensure loaded" trampoline
entirely for that call. Widening the stack dump from the working Mode-1
trap (`VR417E1.EXE`) out ~130 bytes found no second far-pointer either —
past the params it's all leftover garbage from `417e`'s own internal
calls (the menu-building loop, several repeated copies of the same
trampoline-return template) and then straight into static data
(environment strings, `GAME.TXT` text). DOSBox-X has no call-stack
feature to fall back on. Stopping here — the exact calling function's
name doesn't block the port below; everything it needs (trigger
condition, live params, full formula shape) is already confirmed.

## Ported to Linux (2026-08-13, task #5 closed)

`src/core/ai_contact.c`: `ai_contact_incite_target_choice` (menu step) +
`ai_contact_incite_price` (formula) + `ai_contact_apply_incite` (pay +
alarm push), wired into the existing village-meet `AI_POPUP_TAG_CONTACT_MEET`
CHOICE chain as a 6th option alongside Trade/Gift/Demand/Teach/Leave.
New tag `AI_POPUP_TAG_CONTACT_INCITE` in `ai_popup.h`. Two-stage flow
matching `@INDIANWARPATH`/`@INDIANWARPATH2`: pick a target from the
other Euro nations you can afford, then pay-and-commit.

**Faithful to the confirmed structure:**
- Menu → pick target → pay flow, matching Mode 1 exactly.
- Gold debited only from the inciter; target nation's own gold untouched.
- Relation-scaled component (via `ai_diplo_indian_relation`) and a floor
  of 500.
- Discount for other tribes of the same Indian nation already favoring
  the inciter (approximating the DOS "matching type" discount loop —
  exact tribe-type-match field still unconfirmed, substituted with
  "same Indian nation").

**Price formula wired for real, 2026-08-14 (was approximated with
`ind->tech` since 2026-08-13).** Both previously-unnamed DOS tables were
identified while tracing the deep Euro G-table (`euro_g_table_0a60.md` /
`FUN_4962_06b6`, same DS neighborhood) — neither is a static lookup
constant, both are live per-turn sums over the tribe *type*
(`nation_id - 4`):
- `-0x69d6[type]` = count of villages of that tribe type.
- `-0x6e7c[type]` = Σ `combat_unit_base_x8(brave, mode=1)` (attack-mode
  value, matching the traced `FUN_281f_09c8`/`FUN_157e_004a` call) over
  every Brave of that tribe type, byte-clamped to match DOS's saturating
  `FUN_4962_0006` accumulator.

`ai_contact_incite_price` now recomputes both directly (loop `col1->tribe`
for the count, loop units for the combat sum) instead of reading a stand-in
field. The other two DOS terms, `INDIAN_STATE.signed_byte[7]`/`[8]`, turned
out to be already-named, already-real fields that were just never wired in
— `ind->muskets` / `ind->horse_herds` (`col1_save.h`) — not new unknowns at
all. **Base price formula (all four additive terms + the relation-scaled
division) is now the real DOS formula, not an approximation.**

**Discount loop — byte-exact, 2026-08-14 (was a Linux-invented "100 gold if
relation>128" stand-in).** Re-read the raw disassembly directly (not just
the earlier prose pseudocode) and found the loop is a real, fully-decodable
match against fields this project already has named on `ColonizeCol1Tribe`:
for every tribe record where `tribe.nation_id(+2)` == this village's own
`nation_id` AND `tribe.mission(+5)&0xf` == the inciter (another village of
the same tribe already hosts a mission from the inciting Euro power),
subtract 250 gold, or 1000 if it's a Jesuit-grade mission
(`mission&0x10`), doubled again if that other village is the tribe
capital (`state.capital`, DOS `state(+3)&4`). No unnamed globals needed —
`mission` and `state.capital` were already real, wired fields (used
elsewhere for convert odds / the teach one-shot exemption). Floor (500) is
now applied once at the very end, matching DOS order, not also
mid-formula before the loop.

**Real regression caught and reverted from the same day's earlier "bonus
fix."** That earlier pass diagnosed the discount loop's `t->nation_id`
comparison as a "0-7 vs 4-11 range mismatch" and changed it to compare
against `nation_id - 4` — but `colony.c` and `units.c` both independently
confirm (via their own `tribe.nation_id - 4` indexing into
`col1->indian[8]`) the field is genuinely 4-11, the same range as this
function's own `nation_id` parameter — so the *original* direct
comparison was correct all along, and the "fix" broke it a second, subtly
different way (still never matching). Reverted to comparing directly
against `nation_id`. The exact same range bug, independently present in
`village_count`'s own loop (part of the base-price formula wired the same
day), meant `village_count` was silently always 0 — fixed identically.
Lesson: a plausible-sounding range-mismatch diagnosis still needs
cross-checking against how the *same field* is used elsewhere in the
already-shipped Linux code before trusting it, not just re-reading the DOS
side once more.

**Base-combine op resolved byte-exact, 2026-08-14 (same day, later in the
pass) — the multiply-vs-divide ambiguity above is closed.** Read the
actual decompiled bodies of both helpers directly
(`viceroy_unpacked.c:20742-20829`, both `address_mapping.csv` `"exact"`-kind)
instead of trusting `FUNCTION_CATALOG.md`'s inferred labels secondhand:
`FUN_1d1d_0f60` (`FUN_0000_e130`) really is a plain 32-bit multiply
(`return (ulong)param_1*(ulong)param_3;` for the common small-operand
case — confirmed line-by-line, not just by name); `FUN_1d1d_0ec6`
(`FUN_0000_e096`) really is a full signed-division routine (restoring
long division, `uVar1 = CONCAT22(...)/(ulong)uVar5` core step). So the
catalog's inferred labels were right both times, and the earlier-shipped
`base * 100 / (relation+75)` here was backwards in *both* operation
(divide instead of multiply) and shape (an invented `*100` fudge with no
DOS basis) — the real line is `price = base * (relation_raw + 75)`, a
plain multiply, no fudge factor. **Now wired**, `ai_contact_incite_price`
(`ai_contact.c`).

**French `nation_A==1` rescale also now wired**, same resolution: since
`FUN_1d1d_0ec6` really is the divide helper, the `if (param_3==1)` branch
really is `price = price*2/3` (~33% off) — a real, independently-sensible
DOS price break for the French (matches actual Colonization lore: French
have the best native relations of the four powers). Implemented as
`inciter==1` (this project's own English/French/Spanish/Dutch = 0/1/2/3
ordering, `ai_contact_euro_name`).

**Missionary -1500 and target-village-capital -500 discounts — now wired,
2026-08-14 (same day, third pass on this function).** Turned out not to
need the unit-id/village-index threading originally assumed: both
`game_loop.c` trigger sites (unit-enters-village, unit-already-adjacent)
already have the exact right values in scope at the trigger point —
`selected`/`u` (the specific acting `ColonizeUnit*`, checked against
`UNITS_JOB_MISSIONARY`) and `t` (the specific matched `ColonizeCol1Tribe*`
for that tile, `state.capital`) — no lookup/approximation needed at all.
`ai_contact_try_village_meet` gained two new params
(`is_missionary`/`is_capital`); the ship-contact call site
(`ai_contact_try_village_meet`'s third caller, `ai_contact_try_ship_village`)
already had its own resolved `tribe` pointer too (`is_missionary` forced 0
— ships never carry one). Both booleans are captured once at Meet-CHOICE
offer time and packed into that CHOICE's own `payload` (bit0/bit1, the
same offer-time-capture discipline as `ai_king_merc`'s landing tile and
this same function's earlier price-formula work), then re-packed into the
Incite target-picker CHOICE's own payload for the second round-trip, so
neither the acting unit nor the specific village record needs to still
exist or be re-derivable at apply time.

New test in `test_ai_contact.c`'s Incite block: two full CHOICE round
trips with muskets/horse_herds boosted so `base` clears the 500 floor,
payload `0` vs `3` (both bits set), asserts the discounted price is ~2000
gold cheaper. Caught one real bug while writing it — the test's own
simulated round trip needs to manually copy the enqueued CHOICE's
`.payload` field into `.result_payload` before simulating the answer,
matching what the real harness does live (`ai_popup.c:156`,
`st->result_payload = st->current.payload`) — the actual gameplay wiring
was correct from the first pass; only the synthetic test needed the fix,
confirmed by checking the real harness code before concluding either way.

**Still approximated / open, documented in code comments:**
- The DOS `apply(CUR_INDIAN_ALT, nation_B, 100, 0)` relation-push call
  (exact semantics/magnitude unconfirmed) — implemented as a flat +10
  `alarm_by_player[target]` bump.
- Only the "Mode 1" (human, menu-driven) path is wired. Mode 2's
  AI-nation-shortcut reading (`param_3>=4` or a per-nation flag set) was
  never actually confirmed by a live capture — not ported; AI nations
  don't autonomously incite in this port yet.

Verified: `unit_ai_contact` (existing dedicated test: menu enqueues target
choices for an affordable inciter, picking one drains ≥500 gold from the
inciter only and bumps the target's alarm by 10, a broke inciter gets no
target choices — all still pass with the real formula's different price
outputs, since the test only asserts the ≥500 floor and gold-isolation, not
an exact price) + full `ctest` (42/43, same pre-existing unrelated
`unit_ai_euro_expand` baseline failure, no regression). Clean build, no
warnings.
