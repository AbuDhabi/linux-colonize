# `0x8d4a` — the DOS settlement-record array (struct map, 2026-08-14)

## Why this exists

Flagged in [`move_scoring_land.md`](move_scoring_land.md) as the blocker for
the missionary `0x4c` orders-write inside `FUN_521d_20e6`: that gate reads
`*(int*)0x8d4a + <offset>` and was assumed (without checking) to be a small
per-nation cache. It is not — it is DOS's **persistent settlement index**,
a fixed array of 18-byte records covering every colony *and* native village
in the game, selected via a shared "current record" pointer. This doc is the
full field map, gathered via a broad grep + read sweep across
`viceroy_unpacked.c` / `viceroy_overlays.c` (no Ghidra recovery needed — the
canonical decomp is clean at every site cited below).

**Status: mapped, not ported.** See "Relationship to Linux" and "Open
question" at the end before attempting to port any of this.

## Storage and selection

- Array base: DS `0x54ec`. Stride: `0x12` (**18 bytes**/record). Capacity:
  `0x54` (84) records, live count tracked by `*(int*)0x539a`.
- `*(int*)0x8d4a` is **not a heap pointer** — it's `record_index*0x12 + 0x54ec`,
  i.e. the address of the *currently selected* record. Set (with clamping)
  by `FUN_15dc_0032` (`viceroy_unpacked.c:9236-9247`):
  ```c
  *(int *)0x8d4c = param_1;                         /* unclamped selector, sibling global */
  if (-1 < param_1) {
    if ((param_1 < 0) || (*(int *)0x539a <= param_1)) param_1 = 0;
    *(int *)0x8d4a = param_1 * 0x12 + 0x54ec;
    FUN_15dc_0006(*(byte *)(param_1 * 0x12 + 0x54ee) - 4);
  }
  ```
  `0x8d4c` (sibling global) holds the unclamped selector, `-1` = "nothing
  selected" — read as a validity guard in several places (e.g.
  `viceroy_unpacked.c:90144`).
- Created by `FUN_4d56_0038` (`viceroy_unpacked.c:81249-81280`, full-field
  initializer — the only site that writes every offset in one place) and
  `FUN_6a09_0006` (`viceroy_unpacked.c:107840-108020`, placement-search
  founding). Deleted/reindexed by `FUN_4d56_00e0`
  (`viceroy_unpacked.c:81282-81332`) — shifts the array down and reindexes
  unit `home_colony` references, and is the site that establishes **owner
  nibble ≤3 = European, >3 = native/other** as a real cutoff DOS itself relies
  on, i.e. **this one array indexes Euro colonies and Indian villages
  together.**

## Record layout (18 bytes)

```c
struct Settlement {            /* 0x12 = 18 bytes, array @0x54ec, up to 84 */
  uint8_t x;                   /* +0  founding X */
  uint8_t y;                   /* +1  founding Y */
  uint8_t type;                 /* +2  index (raw-4, clamped 0..7) into an 8-entry,
                                        78-byte "type profile" table @0x5ad6
                                        (ptr cached at *(int*)0x8d4e) */
  uint8_t flags;                 /* +3  scratch AI flags, see below */
  uint8_t value;                  /* +4  one-time founding "worth" (FUN_41f2_0294 survey) */
  uint8_t owner_flags;            /* +5  nibble=owner nation 0-3 / 0xf=none; bit0x10=capital;
                                          bit7=valid sentinel */
  uint8_t unknown6;                /* +6  write-only (init 0 at creation), no read found */
  int8_t  target_primary;          /* +7  -1 none, -2 "resolved", else native-settlement id */
  int8_t  target_secondary;        /* +8  same domain as +7 */
  int8_t  target_tertiary;         /* +9  same domain as +7 */
  int16_t attitude[4];             /* +10,+12,+14,+16 — per-EURO-NATION (0-3) score */
};
```

### `+0`/`+1` — coordinates
Read-only after creation, always as arguments to tile-lookup/distance helpers
(`FUN_281f_0722` tile-owner, `FUN_1000_856a`/`FUN_281f_037a` distance).
Representative sites: `viceroy_unpacked.c:78309, 81201, 87994, 88440,
89971-89974, 90052`.

### `+2` — type/class index
Selects an entry in the 78-byte type-profile table at `0x5ad6` (not itself
resolved this pass — next thing to map if this struct gets ported). Also
read directly as a raw priority weight in `FUN_15eb_26e4`
(`viceroy_unpacked.c:12851`). Sites: `9243-9244` (creation), `12851`,
`83842-83891` (`FUN_4d56_4528`), `83659` (`FUN_65dd_0004`), `83561-83567`
(`FUN_4d56_417e`).

### `+3` — scratch flags (bits `0x1,0x2,0x4,0x8,0x10` all independently used)
Mostly **one-shot "already handled this AI pass" latches**, not persistent
settlement attributes:
- bit `0x1`: cleared in `FUN_4d56_152e` (`81448`) right after a founding
  colonist assignment succeeds — "needs first colonist".
- bit `0x2`: set in `FUN_1000_a618`(overlay, `77809`) after issuing a
  build/upgrade order; tested in `FUN_521d_20e6` (`89368`) as an
  already-ordered guard.
- bit `0x4`: **read-only** bonus/multiplier flag at half a dozen scoring
  sites (`75619, 81231, 81891, 83584, 89065, 89219, 89976, 85183, 85233`);
  **set** at `FUN_6a09_0006` (`107891`) immediately after founding a new
  settlement — reads as "newly founded / capital-class" scoring multiplier.
- bit `0x8`: one-shot "already produced this AI suggestion" latch
  (`89065` test, `77558-77559`/`78346` set).
- bit `0x10`: one-shot "already doubled the weight this pass" latch
  (`77413-77415` set, `83584` test).

### `+4` — founding "worth" stat
Written once at creation from `FUN_41f2_0294` (a terrain-survey function,
`viceroy_unpacked.c:72085+`). Read as a scoring-table bonus term
(`87998, 95380`).

### `+5` — owner + persistent flags
- Low nibble: owner nation 0-3 (European), `0xf` = none/unowned. This is the
  field `FUN_4d56_00e0` uses to decide "European (≤3) vs native (>3)"
  when reindexing on delete.
- Set at colony founding: `FUN_1000_a5dc` (overlay, `77303-77306`).
- Reassigned wholesale on nation-merge/absorption in `FUN_43f7_0218`
  (`73699-73703`, "every record owned by X becomes owned by Y").
- Bit `0x10` = **persistent capital flag** — set in `FUN_4345_0342` case
  `0x16` (`73125-73132`, "for every colony owned by nation N, set capital
  bit") and used as a real weight multiplier (1000 vs 250) in
  `FUN_4d56_417e` (`83562-83568`).
- Bit 7 (sign bit) used as a separate "valid/active" sentinel from the
  nibble (`73127`, `83952`, `86614-86615`).

### `+6` — unresolved
Only touched at creation (`= 0`, raw-address write, not through `0x8d4a`).
No read site found in either decompiled file. Flag: **low confidence,
possibly padding/reserved** — don't assume dead without a second pass if
this struct ever gets ported.

### `+7`/`+8`/`+9` — up to 3 pending "target" slots
Signed bytes, `-1`=none, `-2` (only seen on `+7`)="already resolved this
pass". Used as an index into two parallel word tables — a score table at
`idx*2 + (-25000)` and a label table at `idx*2 + (-0x6840)` — and compared
against loop bounds up to `0x4a` (74), so the index domain is a **native-
settlement id** (missionary/scout AI target), not this array's own index.
Pattern repeated verbatim across `FUN_4720_015c` (`76280-76330`),
`FUN_4d56_2820/2aac/2af6/2bbc/2e92/2f96/306c` (`82263-83220`),
`FUN_5952_035e` (`96005-96009`) and overlay twins — always "is this
candidate already claimed by slot 8 or 9? if not, and its score table entry
is non-zero, proceed; else invalidate the stale slot's score entry and
fall through to reassignment." Reads as **two-to-three concurrently
tracked pending missions per settlement** (e.g. scout/missionary targets).

### `+10,+12,+14,+16` — `int16_t attitude[4]`, indexed by Euro nation id
Confirmed stride 2, confirmed index domain nation 0-3 (matches struct's
remaining 8 bytes exactly) at every site checked:
- Bulk-zeroed per nation across **all** records in `FUN_4345_0342` case
  `0x10` (`73112-73118`, "nation eliminated/reset" event).
- Read in `FUN_521d_20e6` itself (`88444`, `89065`) as
  `*(int*)(*(int*)0x8d4a + owner_nibble*2 + 10)` where `owner_nibble` comes
  from the **evaluated unit's own owner nation** (`unit+0x3147 & 0xf`) — "this
  colony's attitude toward the unit's nation," gating a missionary/return
  order when `==0`.
- Same read/clear pattern in `FUN_5bfb_022e` (`96700-97000`, the **Indian
  meet/contact dispatcher — already fully ported in Linux**, see
  `indian_contact.md`), compared against `0x7f`/`0x80` and zeroed on some
  outcomes.
- Also touched in `FUN_5952_035e`, `FUN_4d56_4528`, `FUN_4d56_311e/3582`.
- A handful of sites decompile the same expression as 4-byte `*(int*)`
  instead of 2-byte `*(undefined2*)` (`83444-83474, 84185-84189, 96711`) —
  all immediately clamp to ≥0 and compare against small constants (`0x7f`,
  `0x13`, `0x3f`); almost certainly a Ghidra 16-bit-register artifact, not a
  real 4-byte field. Treat as `int16_t` for any future port; re-verify
  against raw disassembly first if it matters.

## Init pattern (matters for porting effort)

Only `FUN_4d56_0038` writes the whole record at once, and only at colony
*creation* — there is **no per-AI-turn full rebuild**. The `+10` attitude
array gets bulk-reset **per nation** across all records on a nation-
elimination event (`FUN_4345_0342` case `0x10`), not recomputed each turn.
Everything else is scattered single-field accessor reads/writes from
whichever AI routine currently holds the record selected. Net: this is a
**persistent per-settlement game-state record**, like a lightweight parallel
"colony" object — not a scratch/temporary structure. Porting it faithfully
means a new persistent array, not a per-turn-computed cache.

## Full function list

`FUN_15dc_0032`(select) · `FUN_15eb_26e4`(+2 as priority weight) ·
`FUN_2f2b_2f3e`/`51ec`, `FUN_38fd_4e8e`, `FUN_3f41_010a`(count-by-owner) ·
`FUN_4345_0342`(event dispatch: zero attitude / set capital bit) ·
`FUN_43f7_0218`(owner reassign on merge) · `FUN_465b_0000`(attitude
increment + `+3` bit4 multiplier) · `FUN_4720_015c`(`+8`/`+9` slot reuse) ·
`FUN_4962_0018`/`06b6`(coord reads) · `FUN_4cc6_07c2`(coord + `+3` bit4) ·
`FUN_4d56_0038`(**create**) · `FUN_4d56_00e0`(**delete**/reindex) ·
`FUN_4d56_152e`(colonist assign, clears `+3` bit0) · `FUN_4d56_2154`(coord
delta) · `FUN_4d56_2820/2aac/2af6/2b92/2bbc/2e92/2f96/306c`(target-slot
family) · `FUN_4d56_311e/3582`(attitude floor-clamp) · `FUN_4d56_417e`
(Incite-Indians price — **already ported**, `+2`/`+5` scan) ·
`FUN_4d56_4528`(settlement enter/raid — **mapped, body PARKED**) ·
`FUN_521d_0a60`(**already ported** — coord/value lookup) ·
`FUN_521d_20e6`(the function that started this investigation) ·
`FUN_5952_035e`(colony tick) · `FUN_5bfb_022e`(**already ported** — Indian
meet/contact) · `FUN_5fef_1b0e`, `FUN_65dd_0004`, `FUN_684c_08c0`,
`FUN_6a09_0006`(**create**, placement search). Overlay-file twins exist
under `FUN_1000_a1xx`-`FUN_1000_a7xx` names, same logic, not separately
detailed.

**False positive to avoid re-chasing:** `FUN_1000_8d4a` / `FUN_1000_8db8`
in `viceroy_overlays.c` are functions whose own *code address* happens to be
`1000:8d4a`/`1000:8db8` — pure Ghidra auto-naming coincidence, unrelated to
these data globals.

## `0x8db8` (separate, plain int — briefer)

Read at `+2` above (`FUN_15eb_26e4`'s aggression-tier gate). Not a stable
single-purpose global: it's written by a generic "nearest-match search"
idiom (`local_4=9999` sentinel, minimized in a scan loop, stored on exit) in
at least four places (`FUN_15eb_0142` @`9399`, `FUN_4cc6_07c2`'s helper
@`80961`, and two overlay twins @`39184`/`44954`) — looks like a shared
"last search result" slot reused by an options/difficulty-select menu
helper. Read sites gate on small thresholds (`<2`, `<3`, `==1`, `!=0`,
`/5-1`) consistent with **game difficulty (0-4)**, which is the best
functional interpretation for a port, but confirm no non-menu code path
reuses the same writer before relying on it.

## Relationship to Linux — RESOLVED (2026-08-14)

**This is the *Indian* half of the array, not the Euro colony half — and
for that half, no new struct is needed.** Confusingly, this one DOS array
(`0x54ec`, stride `0x12`) covers *both* Euro colonies and native villages
(owner nibble ≤3 vs >3, per `FUN_4d56_00e0`), but Linux already models
those with two separate, already-existing structs — `ColonizeColony`
(`colony.h`, unrelated to this array) for the Euro side, and
`ColonizeCol1Tribe`/`ColonizeCol1Indian` (`col1_save.h`) for the native
side. Checked `FUN_5bfb_022e` (Indian meet/contact) field-by-field against
`col1_save.h` and the offsets line up almost exactly:

| DOS (via `0x8d4a`/`0x8d4e` selectors) | Linux (`col1_save.h`) | Match |
|---|---|---|
| `0x8d4a` record `+10..+17` (`int16_t attitude[4]`, per Euro nation) | `ColonizeCol1Tribe.alarm[4]` = `{uint8_t friction; uint8_t attacks;}`, **byte offset +10** in that struct (x+y+nation_id+state+population+mission+growth_accum+pad+last_bought+last_sold = 10 bytes before `alarm[4]`) | **Exact offset match.** DOS's 16-bit word = Linux's packed `{friction, attacks}` pair (low byte friction, high byte attacks) — explains why a few sites decompiled it as 4-byte `*(int*)`: `attacks` dominates once non-zero (`attacks*256`), so the `iVar16 > 0x7f` gate at `viceroy_unpacked.c:96712` is mostly "has this village recorded any attacks from this nation," not a friction threshold. |
| `0x8d4e` record `+0x2e` (word, 0/1/2 state) | `ColonizeCol1Indian.contact_state[4]`, cited in `col1_save.h:466` as "**+0x2e** — per-euro contact FSM 0/1/2 (`FUN_5bfb_*`)" | **Exact, already documented.** |
| `0x8d4e` record `+0x46` (clamped ≤20) | `ColonizeCol1Indian.alarm_by_player[4]`, `col1_save.h:477`, offset **+0x46** (`unknown33[8]` at +0x3e, +8 = 0x46) | **Exact offset match.** |

So `0x8d4a`/`0x8d4e` are simply DOS's runtime "currently selected village" /
"currently selected native-nation" pointers into data Linux already has,
fully save-backed, already read/written in dozens of places in `ai_contact.c`
(`t->alarm[e].friction`, `ind->alarm_by_player[e]`, etc.). **No new struct
or field is needed to port any of this.**

**The real, narrower, confirmed gap** is behavioral, not structural: read
`FUN_5bfb_022e` end to end (`viceroy_unpacked.c:96565-97101`) and compared
it against `ai_contact_indian_meet_trade` (`ai_contact.c:3604`, the actual
Linux port of this function). DOS's body has two parts:
1. **First contact** (`96614-96676`) — **Done** in Linux
   (`ai_contact_try_first_welcome` + friction-decay/mission-offer stand-in).
2. **Already-met Brave/Euro adjacency** (`96677-97101`, ~420 lines) — calls
   the already-ported `FUN_4d56_2154` scorer (`96750`, "Done scorer" per
   `indian_meet_scoring_2154.md`) to fill gift/demand tables at `0x9e58`/
   `0x9e78`, then (`96751-96827`) uses those scores to roll a probabilistic
   accept/refuse outcome that **resets** the village's `friction` to 0 on
   refusal or **scales it up ×1.5** plus a computed positive tribe-relation
   delta on acceptance. **This part is not ported at all** — Linux's
   already-met path (`ai_contact.c:3673-3703`) skips straight to unrelated,
   much simpler stand-ins: a Trade/Gift/Demand/Teach/Incite/Leave CHOICE
   menu for human Euros, silent `ai_contact_auto_trade`/
   `ai_contact_gift_or_demand` stand-ins for AI Euros. Neither resolves
   through DOS's real accept/refuse roll or touches `friction`/`attacks`
   the way `022e` does.

**2026-08-14, later same day — correcting the estimate above after a full
end-to-end read (attempted implementation, aborted):** "~75-line block
built on mostly-existing infra" was too optimistic, on two counts.

**Scope**: `96677-97101` is not one block — it's *two parallel scenarios*
sharing plumbing (Brave-initiates-adjacency-to-Euro, using `local_42`,
`96677-96988`; Euro-initiates-adjacency-to-village, using `local_4c`,
`96989-97101`), each with its own which-good-to-gift/trade selection loop,
a free-colonist chance (`FUN_281f_095c`, `97009-97013`), and cash-gift
sizing (`97016-97089`) — closer to `2820`/`4528` in size and shape than to
a bounded slice. `FUN_281f_030c`/`0d6c` *are* now identified with
confidence — signature and sign convention match `ai_diplo_indian_read`/
`ai_diplo_indian_relation_delta` exactly (both take indian-nation id +
euro-nation id; both use negative-delta-for-hostility, confirmed against
`ai_diplo.c`'s existing war-hit call sites) — so that part of the
uncertainty from the first pass is resolved. But that's not the blocker.

**The real blocker: sign-convention ambiguity on which branch is "accept"
vs "decline."** The one genuinely bounded, resolved-looking piece
(`96751-96827`, gated on the already-ported `2154` scorer's `ask[0]>bid[0]`
output) still splits into two branches whose real-world meaning can't be
pinned down:
- `local_c==2`: gives away half the colony's stock, resets village
  `attitude` to 0, applies a **negative** relation delta (floored at 71).
- `local_c!=2`: keeps the stock, scales `attitude` **up ×1.5**, applies a
  **positive** relation delta.

Which of `local_c==2`/`!=2` corresponds to "AI/human agreed to help the
village" depends on the actual text of message ids `0x1866`/`0x1858`/
`0x181c` etc. — binary-resource popup strings, same unrecoverable-resource
class as the unit-capability bitmask (not in any `GAME.TXT` `@TAG`, no
string-table extraction tool exists for them). Looked for a second,
unambiguous site touching the same `attitude` field to cross-check the
sign independently (a clear "raid/attack → worse" narrative would settle
it) — the only other site is `FUN_4d56_4528`, itself the already-known
corrupted decompile ("decomp soup," variable names lost) — not usable as
corroboration.

**Conclusion: correctly stays PARKED, same treatment and same underlying
reason as `2820`/`4528`** (unrecoverable binary string resources + no
corroborating site) — implementing either branch's polarity as a guess
risks the AI silently doing the *opposite* of DOS on every already-met
adjacency, which is worse than an inert/unreached mechanic. Revisit only
if a way to recover the popup string table surfaces (would also unblock
`2820`/`4528`/the second `3180` roll, all blocked on the same wall).

The missionary `0x4c` gate and the `+7/8/9` target-slot family (Euro-side,
`0x8d4a`'s *other* half) are unaffected by any of this and stay PARKED
under `move_scoring_land.md`'s original reasoning.
