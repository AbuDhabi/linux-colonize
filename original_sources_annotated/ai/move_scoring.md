# Move scoring (`FUN_521d_20e6`) — phase 2 notes

Phase 1 does **not** annotate the full ~3995-line scorer. This note pins the
quiet Brave entry so the next slice can be extracted coherently.

## Symbol

| Ghidra | Role |
|--------|------|
| `FUN_521d_20e6` | Direction / move scoring for all unit kinds |
| Nested `FUN_521d_5b66` | Large helper **inside** the `20e6` span (not a separate far export) |
| Quiet NEW WORLD Brave | Entry around ASM `521d:4ea9` (type 19, flags `0x38`) |

## Quiet Brave (type 19) — ASM intent vs Linux port

From [`.context/seed100-brave.md`](../../.context/seed100-brave.md):

| Piece | ASM quiet (`521d:4ea9`) | Linux today (`ai_native_pick_dir`) |
|-------|-------------------------|-------------------------------------|
| Base | `range(1,3)` | Empirical **200** |
| Terrain | river-cardinal / fa → **+1**, else **−2f76[terr]** | Facing / home / roll empiricism |
| Facing | **−diff²×2** | Empirical +4 / −6 / +3 |
| Extras | neighbor fog/explore (`bVar20`), colony-pull `52aa` | Skipped (no colonies) |

**Important:** partial ports of the ASM quiet formula (facing-only, or
facing+−2f76 on empirical base) **regress** `smoke_mapgen_seed100` / T1.
Keep empirical bridges until a **coherent** quiet slice lands in
[`indian_nation_turn.c`](indian_nation_turn.c) and then in `src/core/ai.c`.

## Phase 2 plan

1. Annotate the quiet Brave-only path inside `20e6` end-to-end (no Euro ships).
2. Name fog / explore / colony-pull helpers it calls.
3. Drive Linux off the annotated slice; empty residual overlays.
4. Only then expand to ocean / Euro land / combat branches.

## Related

- [`docs/ai_transcription.md`](../../docs/ai_transcription.md) — inventory + R0/R5
- [`SYMBOL_MAP.md`](../SYMBOL_MAP.md)
- Linux: `ai_native_pick_dir`, `ai_dos_move_spent` in `src/core/ai.c`
