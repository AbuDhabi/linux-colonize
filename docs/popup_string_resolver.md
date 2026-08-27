# Numeric popup message-id resolver — investigation (2026-08-14)

## Why this matters

Several AI functions are permanently blocked on "which outcome branch is
accept vs. decline" or similar sign questions, because the DOS code
displays its answer via a numeric message id (`0x1866`, `0x1340`,
`0x14c8`, `0x181c`, `0x1858`, `0x1871`, …) rather than a `GAME.TXT` `@TAG`
string. Every prior doc touching this (`settlement_record_8d4a.md`,
`indian_trade_2820.md`, `indian_settlement_4528.md`, `king_ref.md`'s
`2244`/`2022` section) calls these "unrecoverable binary popup strings" and
stops there. Resolving the mechanism once would potentially unblock *all*
of them at once — worth checking whether "unrecoverable" really means
"impossible" or just "nobody traced the resolver chain yet."

**Answer: it's the latter, at least partially.** The chain traces
cleanly through named, real functions for several hops — this is not a
dead end, it's an unfinished trace. Genuinely stops short of the final
"numeric id → raw text bytes" step, which needs either more RE or (more
practically) a live capture.

## Traced chain, real functions only (each cross-checked against
`address_mapping.csv` / `FUNCTION_CATALOG.md`, not decompiler-name-trusted
blind — see the retracted false lead in `euro_diplo_153e_full.md` for why
that check matters)

Starting from `022e`'s own real call
(`viceroy_unpacked.c:96882`, `FUN_291f_019c(0x281f,0x1866,*0x8d52)`):

1. `FUN_291f_019c` → thunk → `FUN_6f74_3760(param_1, param_2)`:
   `*(u16*)0x1f5c = param_2` (stores the numeric id, here `0x1866`, into a
   fixed DS slot) → `thunk_FUN_281f_0998`. Sibling thunks `FUN_6f74_37a2`/
   `FUN_6f74_37cc` store into neighbor slots `0x1f5e`/`0x1f60` — this
   looks like a 3-slot dialog-request record (id + two extra params).
2. `thunk_FUN_281f_0998` → `FUN_1000_8b88` (exact-mapped) → `FUN_6f74_36ca`:
   "Parse+run+free dialog; return choice" (catalog's own gloss, matches
   behavior). Calls `thunk_FUN_291f_0182` to get a far pointer
   (`lVar1`), then `thunk_FUN_291f_016a(lVar1, ...)` to run it and get a
   choice, then frees via `FUN_291f_01a8`.
3. `thunk_FUN_291f_0182` → `FUN_6f74_32a4` ("Parse @-directive dialog
   script into box"): a genuine `@`-directive text parser — reads lines
   one at a time via `FUN_291f_091c` (catalog: "next line from **open
   string resource**" — real evidence a resource-open concept exists),
   checks `*local_10=='@'`, and matches known directive keywords
   (`TITLE`/`OPT`/etc. — small literal strings at fixed addresses
   `0x1fc7`/`0x1fcf`/`0x1fd6`/`0x1fdb`/`0x1fe5`/`0x1fe7`/`0x1fe9`/`0x1fef`)
   to build title/options/attributes. This confirms the eventual payload
   *is* `@`-directive-formatted text, same shape as `GAME.TXT` — not some
   opaque binary blob.
4. Before that: `FUN_6f74_32a4` opens with `thunk_FUN_291f_023c` → `FUN_6f74_06d0`
   ("Alloc+init dialog box record from flags/script attrs") — allocates the
   box structure the parsed directives populate. Not the id→text step
   either.

**Stopped here.** The actual "numeric id → text buffer/pointer" resolution
happens somewhere between step 1 (id stored at `0x1f5c`) and step 3 (a
line-iterable text resource already open) — not yet located. Likely
inside `thunk_FUN_291f_016a` (the "run" step, not traced this pass) or an
earlier "open resource by id" call folded into `FUN_6f74_36ca` that this
pass's reading didn't isolate.

## Hypothesis tested and ruled out

Tried: are these numeric ids simple offsets/ordinals into the *raw*
`GAME.TXT` file, just never cross-referenced? Checked the one
already-confirmed mapping (`0x1340` → `@MERCENARIES`, cited in
`king_ref.md`) against `COLONIZE/GAME.TXT` directly:
- Byte offset of `@MERCENARIES` in the file: `0x1215b` (74075) — doesn't
  match `0x1340` (4928).
- Ordinal position among the file's 1045 `@`-tags: 894th — doesn't match
  either, and `GAME.TXT` only has 1045 tags total, while ids like `0x1866`
  (6246) exceed that count anyway.

**Ruled out**: not a direct byte offset or tag-ordinal index into
`GAME.TXT` as shipped. Whatever the actual resource is, it's either a
different (larger, or differently-encoded) table, or the ids index into
something computed/compiled at load time rather than the raw file.

## Practical recommendation: live capture, not further static RE

Continuing the static trace could still work, but the next hop
(`FUN_1000_...` targets of `thunk_FUN_291f_016a`, not yet resolved) is
resident-space and may need the same overlay-recovery effort as `153e`
did — real but uncertain-sized additional work.

**A live DOSBox-X capture is comparatively cheap and already
proven for exactly this class of problem** — this is the identical
technique that fully resolved `417e`'s (Incite Indians) live-parameter
ambiguity earlier this session (`indian_incite_417e.md`). Two concrete
options, in order of effort:

1. **Cheapest, most generalizable**: set a breakpoint (not a self-loop
   trap — a real breakpoint that lets execution continue) at
   `thunk_FUN_281f_0998`'s resident target (`FUN_1000_8b88`, or the
   `FUN_1000_...` target of `thunk_FUN_291f_016a` once found) and log
   `[0x1f5c, 0x1f5e, 0x1f60]` (the id + 2 extra params) to the DOSBox-X
   debugger console every time it's hit, **while playing normally** — no
   need to engineer a scenario-specific trap. Cross-referencing logged
   ids against on-screen dialog text as they naturally occur during play
   would build a reusable id→text table incrementally, unblocking this
   and any future numeric-id question at once, not just one branch.
2. **Targeted**: same `VR417E.EXE`-style self-loop trap technique, built
   specifically to catch a village-adjacency accept/decline moment (the
   `settlement_record_8d4a.md` question) — faster to a single answer, but
   doesn't generalize the way option 1 does.

Neither was attempted this pass — both need the user's own hands-on
DOSBox-X session (building/running a patched `.EXE`, reading debugger
output), the same as every previous live-capture round this session.
Flagging as the concrete, actionable next step if this gets picked up —
this doc gives the exact function names and memory slots to target,
so no re-tracing is needed first.

## What this does NOT unblock by itself

Even a full id→text resolution only settles the sign/wording ambiguity —
`022e`'s deep adjacency body, `2820`/`4528`, and `153e`'s `12d0` branch
would still need their own porting effort afterward. This is a
prerequisite-unblock, not a finished port.

## Outcome, 2026-08-14 — the live capture happened

The user ran the `BPM`-based capture recommended above. The specific
mystery ids (`0x1866` etc.) were never recovered — the breakpoint mostly
surfaced an already-mapped dialog family — but the session resolved the
actual blocker anyway, by a different route: the user recognized live
gameplay text (`@INDIANBEGFOOD`) from direct experience and gave decisive
testimony on the accept/decline sign question, unblocking
`settlement_record_8d4a.md`'s parked mechanic (now ported, see that doc).
See `decomp_inventory.md`'s "Method: asking the user to identify things
is a legitimate RE tool" for the general lesson — recognize-from-
gameplay is often a faster path than a full id→text table, worth trying
before or alongside a longer capture session.


## 2026-08-27 — RESOLVED: the numeric ids are DS addresses of tag-name strings

The "numeric message id" pushed into `DS:0x1f5c` is simply the **DS
address of a NUL-terminated GAME.TXT tag name** living in the EXE's data
segment. EXE file offset = `121248 + id` (same DS→file formula the
2026-08-27 mysteries sweep established). Checked directly against
`COLONIZE/VICEROY.EXE`:

| id | string | id | string |
|---|---|---|---|
| `0x1866` | `INDIANCITY` | `0x1340` | `MERCENARIES` |
| `0x14c8` | `INDIANBURN` | `0x181c` | `INDIANBEGFOOD` |
| `0x1858` | `INDIANCOMMENT` | `0x1871` | `INDIANWAGONS` |
| `0x13cb` | `CANCELPEACE` | `0x13d7` | `DECLAREWAR` |
| `0x156a` | `BADHAGGLE1` | `0x157c` | `BADHAGGLE0` |
| `0x1575` | `TRADE0` | `0x1bed` | `KINGGALLEON` (+`0x1bf9` `"3"`, `0x1bfb` `"2"`) |

So the dialog engine opens GAME.TXT and looks the `@TAG` up by name; the
"resource open" seen at step 3 above is that lookup. The full table of
every tag-shaped string in `DS:0x1000..0x2000` (357 strings, 250 of them
present as `@TAG`s in the shipped GAME.TXT; the rest are NAMES.TXT
sections, directive keywords, etc.) is in
[`popup_tag_ids.md`](popup_tag_ids.md). Two consequences:

- every "unrecoverable binary popup string" note in the AI docs can now
  be resolved by a lookup — no live capture needed;
- tags can be **assembled at runtime** (`KINGGALLEON` + `"2"`/`"3"`),
  so a grep for the full tag name can miss a real site; grep for the
  base-name address instead.
