# Indian tribe WoI defection (`FUN_4d56_1816`, item 2 gap) — 2026-08-14

## Summary

While chasing an unrelated caller-resolution question (`FUN_OVL12_L0000_0`),
found and read `FUN_4d56_1816` (Indian nation turn — `viceroy_unpacked.c:
81543-81680`, 137 lines / 799 bytes, matches the clean re-disassembly
`indian_contact.md` already confirmed) in full for the first time. Items
1 and 3-8 of `indian_contact.md`'s existing phase checklist all check out
byte-for-byte against this canonical copy. **Item 2 ("select indian
context + chrome" → Linux "no-op / turn cursor") is not a no-op** — it
contains a real, previously undocumented mechanic: **a tribe may defect to
support the rebel side during the War of Independence**, receiving a
one-time musket/horse windfall. Genuinely clean code (bounded by a real
`^}` at line 81680, no corruption-fault warnings on this section), all
touched fields already exist in Linux's `ColonizeCol1Indian` struct with
confirmed exact offsets — this is a real gap, not a stale-doc mirage like
the last two leads this session.

## Raw logic (`viceroy_unpacked.c:81558-81599`)

```c
FUN_281f_0a42(0x281f, param_1);                  /* select indian context */
FUN_281f_0590(0x281f, *(byte*)(param_1 + 0x84c)); /* per-nation chrome byte, cosmetic */
if ((*(byte*)0x5382 & 1) != 0 &&                  /* WoI declared (head.unknown46[0] stand-in) */
    (*(byte*)(indian_rec + 3) & 0x20) == 0) {     /* tribe hasn't already resolved this check */
  iVar5 = FUN_281f_030c(tribe, declaring_nation);  /* current relation vs the rebel nation */
  bVar1 = !(iVar5 < 0x19 || dos_rng_range(1,400) < iVar5); /* base eligibility */
  if ((indian_rec + 3) & 0x40) { bVar1 = true; }   /* force-eligible override bit */
  if (bVar1) {
    if (dos_rng_range(0, (difficulty - 5) * -2) == 0) {  /* final defect roll */
      /* status message: "<tribe> ... <declaring nation> ..." (exact wording unfound) */
      ai_diplo_indian_relation_delta(tribe, declaring_nation, +100); /* maxes out */
      ai_diplo_indian_relation_delta(tribe, crown_nation,    -100); /* bottoms out */
      FUN_2a1f_0398(tribe, declaring_nation); /* "mission clear" thunk — FUN_4cc6_0000, see below */
      /* musket/horse windfall: */
      /* 2026-09-06d: the clamp operand is the tribe's VILLAGE COUNT, not
       * its tech. DOS: `cVar3 = *(char *)(*(int *)0x8d52 - 0x69d6)`, and
       * 0x962a = tribe_village_counts (save_format_map.md file offset 581,
       * written by FUN_4962_06b6 — which 1816 itself calls a few lines
       * later via FUN_2a1f_0270). */
      villages     = tribe_village_counts[slot];
      muskets      = min(muskets, villages);
      muskets     *= 4;                       /* << 2 */
      horse_herds  = min(horse_herds, villages);
      horse_breeding = horse_herds * 25;
      indian_rec.unknown31_flags |= 0x20;      /* one-shot latch */
    }
  }
}
```

`difficulty` = `DS:0x53a6`, already a named/known global (matches
`col1.head.difficulty`, cited e.g. in `euro_diplo_153e_full.md`). Roll
range shrinks with difficulty (10/8/6/4/2 for difficulty 0-4), so higher
difficulty makes defection strictly *more* likely per eligible tribe per
turn (uniform 1-in-N chance, N = that range+1).

**`FUN_2a1f_0398` resolved this pass too** (thunk to `FUN_4cc6_0000`,
`viceroy_unpacked.c:80774-80802`, itself fully clean/uncorrupted in the
canonical export — no Ghidra struggle needed at all, a clean copy was
sitting right there under a name nobody had cross-referenced). Scans the
settlement-record array (`settlement_record_8d4a.md`'s struct: `+2`=type,
`+5`=owner) for records of `type == param_1+4` owned by `param_2`, and
clears them (owner byte → `0xff`, matching that doc's "record
invalid/inactive" sentinel), i.e. **"missions of a given type belonging
to a nation get pulled" for the just-departed(?) nation** — plausible
reading given the context (tribe switching allegiance would need its
existing Euro-owned mission/asset ties to that specific type severed),
though the exact `type == param_1+4` selector's real-world meaning (which
settlement archetype) isn't independently confirmed this pass.

**This also resolves last pass's unresolved `007308`/`a6e4` contradiction
— it was chasing the wrong address entirely.** `FUN_OVL12_L0000_0`
(hand-transcribed, blocked on "unlabeled globals") is exactly
`FUN_4cc6_0000` above, byte-for-byte, and a clean canonical copy already
existed (`4cc6:0000`, confirmed via `address_mapping.csv`'s `"before-
first-function"` entry for that segment). `FUN_OVL14_L0000__007308` (the
address the old "a6e4" thread was staring at) is a real, `"exact"`-mapped
function that decompiles as the giant move-scoring gate, matching the
phase-outline's own "`r = 2a1f_04f4 → FUN_521d_20e6`" description — it
was simply never the right lead. `euro_unit_act.md`'s note updated to
point here instead of speculating further.

## Fields — all already in `ColonizeCol1Indian` (`col1_save.h`), offsets
confirmed exact:

| DOS offset | Linux field | Current Linux semantics |
|---|---|---|
| `+3` bits `0x20`/`0x40` | `unknown31_lo` (7 bits, currently fully opaque) | Would need two new named bits: one-shot "resolved this WoI" latch, and a "guaranteed defect" override |
| `+7` | `muskets` | Already read/written elsewhere |
| `+8` | `horse_herds` | Already read/written elsewhere |
| `+10` | `horse_breeding` (uint16) | Already read/written elsewhere ("±0x32 on acquire/tick") |

`unknown31_lo`'s clamp at item 4 (`viceroy_unpacked.c:81603-81607`,
`indian_contact.md`'s existing "clamp alarm byte ≥0") is actually clamping
**`+7`/muskets**, not "alarm" — the checklist's item-4 label predates any
byte-offset verification and looks like a guess that was never checked
against the struct. Worth a small correction alongside this, not chased
further this pass.

## Implemented (2026-08-14, same day, user-greenlit)

Ported as `ai_contact_indian_woi_defect` in `ai_contact.c`, wired into
`ai_indian_nation_turn` (item §2, right after reseed, before the alarm
prelude — matches DOS phase order). New Linux-side pieces:

- `ColonizeCol1Indian.unknown31_lo` (`col1_save.h`, offset `+3`, previously
  fully opaque) split into `woi_defect_resolved` (bit `0x20`, one-shot
  latch) and `woi_defect_forced` (bit `0x40`, override — no known DOS
  setter found this pass, so nothing sets it from Linux either; reserved).
- `ai_king_crown_nation` / `ai_king_independence_declared` made public
  (were `static` in `ai_king.c`) — both already existed for other King
  features, just needed exposing across the module boundary.
- Eligibility (`relation ≥ 25` then `dos_rng_range(1,400) ≥ relation`,
  or the forced-override bit), then the final `dos_rng_range(0,
  (5-difficulty)*2) == 0` hit roll, then `+100`/`-100` relation deltas
  (human/crown) via the already-existing `ai_diplo_indian_relation_delta`,
  then the musket×4 / horse-herds-capped-at-tech / horse_breeding=herds×25
  windfall, then the one-shot latch and a status line.
- **Resolved 2026-09-06d** (was "approximated"): the windfall clamp is
  `DS:0x962a[slot]` = `stuff.tribe_village_counts`, not `ind->tech` — a
  different field, not "the same conceptual quantity". A large low-tech
  tribe was being disarmed on defection and a small high-tech one
  over-armed. `ai_contact_indian_woi_defect` now reads the census array
  (also reads muskets/herds as signed bytes, as DOS does). DOS's exact
  status wording is still not reproduced — Linux invents its own.
- **`FUN_2a1f_0398` "mission clear" side-effect — wired 2026-08-14**, same
  day, once its target (`FUN_4cc6_0000`) was fully read (`viceroy_unpacked.c:
  80774-80802`, clean/uncorrupted). Byte-exact: `param_1` at the call site
  is the defecting tribe's own *type* (0-7), and the DOS condition
  `tribe.nation_id == param_1+4` collapses to exactly this tribe's own
  `nation_id` — i.e. **every village of this same Indian nation currently
  hosting a mission from the declaring (human) side loses it** when one of
  its villages defects, using the identical `col1->tribe[]` 18-byte-stride
  `+2`=`nation_id`/`+5`=`mission` fields the Incite discount loop
  (`indian_incite_417e.md`) already established this same day. DOS then
  shows the human an informational popup (string id `0x14c8`, exact
  wording unrecoverable without a live capture — same "invent our own
  text" gap as the rest of this mechanic) only when something was actually
  cleared; folded into the existing status line instead of a separate
  popup. Test: `test_ai_contact.c`'s WoI sweep re-arms a same-tribe human
  mission each iteration and asserts it's cleared on a defect hit.

**Real bug caught during testing, not a naval-ambush-style placement bug
this time — a test-harness bug.** First test sweep reseeded the RNG with
sequential small seeds (1..30) per iteration and got zero hits across all
30 — traced it to a genuine `dos_rng` property, not a port bug: a tiny
seed's *first* `dos_rng_next()` output is dominated by the LCG's additive
constant before the multiplicative term has "warmed up," so seeds 1..30
all produce the *same* first `dos_rng_range(1,400)` result (verified with
an isolated probe: literally `roll=1` for all of seeds 1-10). Fixed by
seeding once and letting the stream advance naturally across iterations
(also more faithful to real gameplay, which never reseeds per-check).
Second bug, this one in the test's own assertions: mixed up
`nation[euro_index].relation_by_indian[indian_index]`'s two axes when
checking the crown-side relation. Both fixed; `unit_ai_contact` now proves
both hit and miss are reachable over a 60-iteration continuous-stream
sweep, plus the resolved-latch and no-WoI no-op cases. Full `ctest` green
(42/43, same pre-existing unrelated baseline failure).
