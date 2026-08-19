# The `23000` table — a nation-wide bilateral relation matrix, not a struct

2026-08-14. Mapped via `FUN_15b3_0004`/`FUN_15b3_0032` (`viceroy_unpacked.c:
9056-9082`), the dual-mode accessor `euro_diplo.md` already cites for the
Indian branch (`nation ≥ 4`): `*(peer + nation*0x4e + 23000)`.

## Headline finding

**This is not a record with named fields — it's a `[12][12]` (or `[8][12]`
in practice) relation matrix, one byte per (nation, peer) pair, covering
the full 0-11 nation space (4 Euro + 8 Indian).** Every confirmed call
site passes a *runtime nation-id value* as the "offset" argument — never a
compile-time constant — and every one resolves to the range 0-11. There is
no evidence of any use of offsets 12-77 anywhere in either decompiled
file. So of the 78-byte-per-nation record `euro_diplo.md` flagged as
"unclear which bytes are meaningful," the real answer is: **bytes 0-11 are
one bit-flag byte per other nation (Euro or Indian); bytes 12-77 have zero
evidence of ever being touched** — either genuinely unused, or fields this
game's Indian AI never exercises (the Euro record's treasury-dword/treaty-
timer/privateer-mask equivalents live *outside* this offset-as-nation-id
convention entirely, at fixed offsets like `-0x77cc`/`-0x77ce`).

This mirrors the already-known Euro-Euro table exactly in shape
(`euro_relation[b]` = `nation*0x13c - 0x77c4 + peer`, one byte per peer) —
the `23000` table is the same concept, just addressed differently and
extended to include Indian nations as either side of the pair.

## Confirmed touch sites (all Indian-branch, nation ≥ 4 independently verified)

| Function | Lines | Nation side | Peer/offset side | Bits touched |
|---|---|---|---|---|
| `FUN_4cc6_00f2` (`ai_diplo_indian_relation_delta`'s DOS original, already partially ported) | `80858,80861,80865` | `param_1+4` (explicit Indian normalization) | `param_2` (Euro id) | clears `0x4`, `0x2`; reads `0x40` as a gate |
| `FUN_4cc6_0092` (Indian-nation-elimination handler, sibling of the already-ported `FUN_43f7_0108`, itself unported) | `80819` | `param_2+4` | `param_3` (Euro id) | clears `0x40` (MET) |
| `FUN_5fef_0f14` (raid outcome — `indian_raid_outcomes.md`/`indian_settlement_4528.md`/`indian_raid_loot.md`) | `99948,99991,100005,100028` | raided settlement's Indian owner (`*(u2*)0x8d50`) | attacking unit's owner (0-11) | reads `0x2` four times as an outcome gate |
| `FUN_521d_0a60` (Euro G-table, already ported — `euro_g_table_0a60.md`) | `87583` | this pass's own nation (`param_1`, Indian when ≥4) | another settlement's owner nibble | reads `(byte & 0x60) == 0x20` |
| `FUN_5bfb_022e` (Indian meet/contact — already ported for first-contact; deep body PARKED) | `96619` | Euro id (`param_1`) — via the or-both wrapper's internal reciprocal write | — | ORs `0x20` |
| unnamed helper, direct unthunked call | `12866` | acting unit's owner (`*(int*)0x8542+0x1a`) | a settlement's owner (`local_8`) | reads `0x20` |

**Bits actually observed on the Indian branch: `0x02`, `0x04`, `0x20`,
`0x40`, `0x60`.** `0x40`(MET) and `0x02`/`0x04`(PEACE/ALLY-shaped) match
the already-known Euro convention (`euro_diplo.md`'s bit table). **`0x20`
does not** — and this is not a new mystery, it's the *same* open
discrepancy `euro_g_table_0a60.md:221-233` already flagged and left
"not reconciled this pass" for its own read of this exact table. Two
independent investigations (that pass's and this one) now corroborate the
same unexplained bit rather than resolving it.

**No bulk-init site found** (unlike `0x8d4a`'s single creation function
that writes every field at once) — every touch here is a single
opportunistic set/clear/read tied to a specific event (contact, raid,
elimination, relation-delta). This supports the "lazy per-peer relation
byte" reading over "a record with a real init routine."

## Real limitation, not chased further

Roughly 40+ of the ~120 total calls to the three accessor thunks
(`FUN_281f_0a38` read, `FUN_281f_0a10` clear-both,
`switchD_2000:da9f::caseD_10` or-both — the last one is a distinct thunk
from `FUN_281f_0a10` despite similar Ghidra auto-naming, confirmed by
reading both bodies directly) show up with **no recoverable arguments at
all** in the canonical decompile — the decompiler lost the real
register-passed params. Those sites can't be classified Euro vs. Indian
from this source. A handful of functions that superficially matched the
grep were checked and confirmed **Euro-only** instead (`FUN_465b_0000`,
`FUN_38fd_5930`, `FUN_43f7_0108` — all explicitly `<4`-guarded). If a
fully exhaustive map is ever needed, the unresolvable ~40 sites would need
`.asm` register tracing (the technique `euro_g_table_0a60.md` used
successfully for its own blocked calls), not another decompile pass.

## Redundancy check (2026-08-14, same day) — resolved, not redundant, but not fully missing either

User asked to check whether DOS's boolean flags and Linux's numeric
threshold model actually drive the same decisions before extending
anything. They don't need reconciling — **for the bits that matter most
(met, peace), Linux already ported this exact table, just without ever
connecting it to the "23000 table" identity.**

`ai_contact.c:594` (`ai_contact_try_first_welcome`, the Linux port of
`FUN_5bfb_022e`'s first-contact arm) carries the comment "DOS OR bit 0x20
before dialog; accept ORs PEACE 0x40 → euro_diplo 0x60" — and that is
*exactly* `FUN_5bfb_022e`'s own literal code at `viceroy_unpacked.c:
96615-96619` (already read in full two passes ago): `FUN_281f_0a38`
(read, → `FUN_15b3_0004`) gates on bit `0x20`, then
`switchD_2000:da9f::caseD_10(...,0x20)` (or-both, → `FUN_15b3_0066`) sets
it — the *identical* accessor family this doc maps, on the Indian side of
the pair. So `ColonizeCol1Indian.euro_diplo[euro_nation]` **is** the
already-ported Linux mirror of (at least) the 23000 table's met/peace
bits — `col1_save.h`'s existing comment ("DOS: bit0x20 met, bit0x40
peace") already had the right bit *values*, just not the "this is the
23000 matrix" cross-reference. That also resolves the "`0x20` unreconciled"
puzzle both this pass and `euro_g_table_0a60.md` flagged: **on the Indian
branch, `0x20`=met and `0x40`=peace — the opposite pairing from the
Euro-Euro branch's `0x40`=MET convention.** Not a mystery, just a
different bit layout per branch that nobody had cross-checked before.

**So the "PORT DEBT" framing in `euro_diplo.md` was stale for the met/
peace bits — same class of correction as the `caseD_10` mischaracterization
two passes ago.** What's left, genuinely:
- `FUN_4cc6_00f2`'s `0x02`/`0x04` clears (relation-delta's own bit-clear
  side effect, alongside the already-ported numeric delta) — practical
  necessity unclear; DOS clears them defensively when relation drops, but
  the *numeric* relation drop (already ported) may already drive every
  behavior that matters, making these bits internal bookkeeping with no
  independent effect. Not confirmed either way.
- `FUN_4cc6_0092` — **re-derived in full 2026-08-19, old "elimination
  handler" framing dropped.** Full body (`viceroy_unpacked.c:80806-80822`):
  ```c
  void FUN_4cc6_0092(int param_1, int param_2, int param_3) {
    uVar1 = 0x4cc6;
    if (param_3 < 4 && *(char*)(param_3*0x34+0x543f) == 0 && param_1 != 0) {
      uVar1 = FUN_281f_0a1a(0x4cc6, param_2+4);   // nation-name string (subst slot 0)
      FUN_281f_0438(0x281f, 0, uVar1);
      uVar1 = 0x281f;
      FUN_281f_0998(0x281f);                      // "run pending dialog" — traced
                                                    // in popup_string_resolver.md
                                                    // (id→text step itself unresolved)
    }
    FUN_281f_0a10(uVar1, param_2+4, param_3, 0x40); // clear-both bit 0x40
    thunk_FUN_2a1f_0398(0x281f, param_2, param_3);  // FUN_4cc6_0000 mission clear
    return;
  }
  ```
  `FUN_281f_0a1a` is independently catalogued (`FUNCTION_CATALOG.md:1434`)
  as "nation name string ptr thunk → dialog subst," confirming the gated
  branch is a **human-only notification**, not an elimination check. So
  with the corrected Indian-branch layout (`0x20`=met, `0x40`=peace, see
  above) this function is: **break peace** between Indian nation
  `param_2+4` and Euro nation `param_3` — clear the peace bit
  unconditionally, sever every mission that Euro nation holds among that
  Indian nation's villages (`thunk_FUN_2a1f_0398` → `FUN_4cc6_0000`,
  already fully read and ported for the WoI-defection caller — see
  `indian_woi_defect_1816.md`), and, only if the Euro side is human and
  `param_1!= 0`, show a nation-name-substituted notice. Not an
  elimination handler at all — no settlement/tribe record is touched here
  (that was `FUN_4cc6_0000`'s own body, misattributed to this function by
  an earlier pass that hadn't yet separated the two). `param_1`'s role is
  now just "notify the player" boolean, most plausibly set by the caller
  to distinguish a player-visible peace break from a silent AI-vs-AI one.
  Caller context (still unresolved — see below) no longer blocks
  understanding what the function itself does.

Net: no architecture extension needed for the part that matters most —
that's done. `FUN_4cc6_0092` itself is now fully read (peace-break +
mission-clear + human notice); only its caller remains open.

**Caller search retried this same pass, still unresolved**: grepped both
`viceroy_unpacked.c` and `viceroy_unpacked_2.c` for any call to
`FUN_4cc6_0092` (its thunk sibling `FUN_281f_0d6c` → `FUN_4cc6_00f2` was
found this way immediately, confirming the method works) — zero call
sites in either file, only the two definitions. Whatever calls it isn't
resolvable from either canonical export; would need the `.asm`
register-tracing approach `euro_g_table_0a60.md` used for its own blocked
calls. Given the function's now-clear semantics ("break peace with an
Indian nation"), the likeliest caller is somewhere in the human/AI war-
declare or `153e` diplomacy flow against an Indian nation, or a colony-
destroyed/tribe-wiped-out path — not chased this pass (2026-08-19), still
correctly parked, caller unknown (the redundancy question around it is
resolved; the caller question isn't). **Not wired into Linux** on
account of the unresolved caller — porting the mechanical effect without
knowing when DOS actually fires it risks inventing a trigger condition,
so this stays documentation-only for now.

## `FUN_4cc6_00f2` max-relation branch — new finding, 2026-08-19

Full body of `FUN_4cc6_00f2` (DOS original of the already-ported
`ai_diplo_indian_relation_delta`, `viceroy_unpacked.c:80826-80917`) read
in full for the first time this pass (prior passes only looked at the
`0x02`/`0x04` clear lines cited above). Two previously-undocumented
pieces:

1. **Tier-crossed tension update** (main branch, `iVar5<100 || peace bit
   clear`): when the new relation value crosses a 5-point tier boundary
   (`iVar5/-5 != iVar2/-5`, i.e. old and new relation land in different
   20-tier bands) and the delta was negative, updates the `0x54f6`
   grudge/tension table (already relabeled in `docs/mysteries_catalog.md`)
   for every tribe of this Indian nation, clamped to `0x20` or `0x60`
   depending on how close nation and Indian's own combat-strength ratings
   are (`iVar3`/`iVar6`, both from `FUN_281f_0a60`). Human-nation name
   substitution (`FUN_281f_09a4` × 2) feeds a status line whose text isn't
   captured. Not ported — needs the `0x54f6` field wired into
   `col1_save.h` first (mysteries catalog: "still no Linux accessor").
2. **Max-relation mission-clear branch** (else branch, `iVar5>=100 &&`
   peace bit set): `local_66 = difficulty` if the Euro side is human else
   `1`; roll `RNG(0,10)`; if `roll <= local_66+1`, call
   `thunk_FUN_2a1f_0398` (→ `FUN_4cc6_0000`, the same mission-clear body
   the WoI-defection port already uses) for this Indian/Euro pair and
   return early — i.e. **once relation with an Indian nation is pinned at
   the 100 cap while still at peace, every further positive relation-delta
   call has a small chance (`(difficulty+2)/11` for human, `2/11` for AI)
   to sever that Euro nation's mission(s) among the tribe's villages.**
   Reads like "the tribe has nothing left to gain from missionary
   contact and lets it lapse" flavor, though not confirmed against any
   manual/fandom text. **Not ported** — the currently-live
   `ai_diplo_indian_relation_delta` clamps to `[0,255]` (byte range), not
   DOS's `[0,100]`, so a literal `v==100` check would almost never fire in
   Linux the way it reliably does in DOS once the original hits its hard
   cap; porting this branch correctly needs the relation clamp itself
   re-scoped to 0-100 first (separate, slightly bigger PORT DEBT item, not
   done this pass to avoid touching a shared clamp blind).
