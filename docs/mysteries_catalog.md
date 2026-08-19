# Mysteries Catalog

Fields, flags, and DS globals whose real meaning was never pinned down —
distinct from `PARKED` (mechanic known, port deferred). Entries here are
"we don't fully know what this represents." Compiled 2026-08-19 by grep
sweep of `col1_save.h`, `src/core/*`, `original_sources_annotated/`,
`docs/`. Cross-ref: [[ai-transcription-fulldraft]].

## A. Save-format opaque bytes (`src/core/col1_save.h`)

Byte ranges read/written for save round-trip fidelity, no confirmed
gameplay meaning:

| Field | Note |
|---|---|
| `tut1.unused06`/`unused08` (was `unknown01`/`unknown02`, DS:0x5380 bit2/bit6) | **confirmed dead, renamed off "unknown"** — all 3 decompiled sources test/set bits 0x01/02/08/10/20/80 on 0x5380 (nr13-nr19), never 0x04/0x40; only touch is word-clear `*(u16*)0x5380=0` at init |
| `unknown05[2]` | head pad, no gameplay cite — only touch is the 4-byte memset that also clears `event` at new game (`FUN_1d1d_0dae(0x540a,0,4)`); no standalone R/W found |
| `unknown06_lo` (7 bits, player struct) | unnamed, sibling bit7 (`named_new_world`) is named |
| `unknown13_pad[4]` (+0xbe) | founding=0, lategame fixtures non-zero — pattern seen, not explained |
| `unknown15_lo` (7 bits @ 0x3148) | "live AI/cargo/orders latches" — vague, not resolved to specific bits |
| `unknown21` (+0xb) | no reader cite — opaque |
| `unknown22` (+0x10) | written by `FUN_38fd_5be8` — writer known, role thin (reader/effect not traced) |
| `unknown23_pad[4]`, `unknown24_pad[4]` | opaque |
| `unknown26[12]` (nation +area) | Linux repurposed as diplo stand-ins (treaty_timer/diplo_flag/etc.) — **exact DOS DS layout PARKED**, current field split is a Linux invention, not a decoded original |
| `unknown28_pad` (+1) | "unproven" |
| `unknown31_lo_pad` (bits 0-4) | no reader cite |
| `unknown31b`, `unknown31c`, `unknown31d[2]` | no reader cite (bit 0x20 of the sibling `unknown31_flags` byte *is* resolved — contact-prelude-fired) |
| `unknown33[8]` (+0x3e) | opaque in DOS; Linux formerly parked peace bookkeeping here |
| `unknown34[12]` | DS:0x9566 — save R/W only, called "vestigial" |
| `unknown36[577]` region (file offset 140..716) | DS-named save chunks per `FUN_75c2_0288`, not further decoded |
| `unknown45_pad[8]`, `unknown46[32]` | opaque |
| DS:0x54f6 wealth/tribute table `[origin*9+nation]` | no Linux accessor at all — table exists, never wired |

Pattern across most of these: writer confirmed (byte position + size right
for save-file round-trip), but *semantic* reader/effect never traced in
DOS code — so Linux either leaves them opaque pad or repurposes the space
for its own bookkeeping, which is a Linux invention wearing the old
field's address, not a decode of the original.

## B. Semantic-RE dead ends (named function, meaning still guessed/unfound)

- **`FUN_4d56_417e` (Incite Indians) — two price-table terms.** Formula
  traced (`table[-0x69d6]*8 + (table[-0x6e7c]>>2&0xfe - 2*table[-0x69d6])`),
  and on 2026-08-14 both tables got *identified* (village-count-by-type,
  Σ combat-strength-by-type) — but this is a structural identification,
  not captured DOS values; `ai_contact_incite_price()` in `ai_contact.c`
  approximates both with `indian.tech` rather than the real per-type
  tables. **Caller never found** despite three static methods + two live
  DOSBox-X hang-dump captures (trampoline at resident `0x1261f` is
  RTLink's generic overlay dispatcher, not game logic — doesn't pin the
  trigger).
  - **Stale-doc mismatch found this pass**: `ai_contact.c:2930-2933`'s own
    comment still says "not wired in — 2 price-table values unnamed,
    caller unfound," but `AI_POPUP_TAG_CONTACT_INCITE` *is* wired
    (`ai_contact.c:1625,1803,1835,1872,1900,5722`). Comment is out of date
    relative to the code sitting right below it.

- **`FUN_5fef_0000`** (best-defender-unit-at-tile scoring walk, resident,
  99111/98). Hand-transcribed from clean disassembly (decompiler pcode
  bug blocked normal recovery) — but "call targets and a few field
  cutoffs [are] not independently named." Structural read only.

- **`FUN_521d_5b66`**: the switch-case body this function was long
  documented as owning (~1815 lines) turned out to actually belong to a
  *different, still-unfound* function reached through a local thunk.
  `5b66` itself was misattributed for an unknown span of project history
  before this was caught — the real owner of that body has never been
  located.

- **Nation-slot table ambiguity (resolved, but the resolution itself
  reveals a live confusion risk).** Pass 17 of the `417e` investigation
  briefly concluded Indian tribes have real entries (`nation[11]`) in the
  same 12-slot table `difficulty.md` calls the "Europe block" — later
  retracted (pass 18: a stack-offset mis-decode, real value was Euro
  nation 0). The retraction is solid, but it means the boundary of that
  12-slot table (is it strictly 4 Euro nations, or does it extend to
  cover tribes too under some other code path?) was never independently
  re-confirmed after the correction — just no longer *contradicted*.

## C. Doc-flagged "Open RE" items still open

- `original_sources_annotated/ai/indian_settlement_4528.md` — ASM-faithful
  map of byte range 84216→end not done; string table XREF
  `0x1710`…`0x172e` never resolved against `GAME.TXT`.
- `original_sources_annotated/ai/indian_trade_2820.md` — human `CHOICE`
  dialog buy-offer path (`LAB_002e92`) price formula: same general shape
  as the AI path, different RNG/UI gating, never traced.
- `original_sources_annotated/turn/between_turns.md` — `FUN_4d56_1816`'s
  dispatcher forge-edge narrowed to overlay `0x0C`/`VR_2A02` but **not
  proven** to be the `130d` edge specifically (best current guess, not
  confirmed).

## D. Meta-mystery

`ColonizeCol1Nation.unknown26[12]`'s Linux-side reinterpretation
(treaty_timer/diplo_flag/indian_hostility_sticky/privateer_spawn_mask) is
functional and tested, but is explicitly **not** a decode of what those
12 DOS bytes actually held — anyone reading the struct casually could
mistake "has named sub-fields" for "meaning resolved." Worth a stronger
comment than "exact DS PARKED" if this keeps tripping people up.

---
Not included: `PARKED` markers (~90+ across `ai_diplo.c`/`ai_king.c`) —
those are known DOS mechanics with deferred *UI/dialog* ports, not
unresolved meaning. See `docs/ai_transcription.md` status tables instead.
