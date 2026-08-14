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
- `FUN_4cc6_0092` (Indian-nation-*type* elimination, distinct from the
  already-ported *nation*-elimination `FUN_43f7_0108`) unconditionally
  clears the met bit (`0x40` in its own reading — wait, re-examine with
  the corrected bit layout: **this function's own `0x40` argument is
  almost certainly the *peace* bit under the now-confirmed Indian-branch
  layout, not met** — worth re-deriving its effect with the corrected
  bit meaning before porting, not just relabeling the old "clears MET"
  read). Caller context (when does a settlement "type" get eliminated?)
  is still unresolved from two passes ago.

Net: no architecture extension needed for the part that matters most —
that's done. The remaining piece is small (one function, one caller
question, one bit-relabel to redo) rather than a systemic gap.

**Caller search retried this same pass, still unresolved**: grepped both
`viceroy_unpacked.c` and `viceroy_unpacked_2.c` for any call to
`FUN_4cc6_0092` (its thunk sibling `FUN_281f_0d6c` → `FUN_4cc6_00f2` was
found this way immediately, confirming the method works) — zero call
sites in either file, only the two definitions. Whatever calls it isn't
resolvable from either canonical export; would need the `.asm`
register-tracing approach `euro_g_table_0a60.md` used for its own blocked
calls. Not worth that for one elimination-handler function — leaving
`FUN_4cc6_0092` correctly parked, caller unknown, same status as before
this pass (the redundancy question around it is resolved; the caller
question isn't).
