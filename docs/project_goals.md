# Project goals

Modern port of **Sid Meier's Colonization** (MicroProse, 1994 DOS).

The port should play and feel like the original: same rules, same assets, same
saves, same inputs. Implementation may diverge from DOS for technological and
architectural reasons; fidelity is judged by player-visible and
save/interop-visible behavior, not by cloning the DOS codebase structure.

---

## Visuals

- Look and feel match the original.
- Use the original textures / art assets provided with the game data.
- Same resolution and layout as the DOS UI.
- Need not be a pixel-for-pixel blit clone of the DOS renderer when tech or
  architecture makes that impractical.
- **Working drafts OK until late:** same functionality and matching text, with
  approximate layout/style, are acceptable until the polish phase. Pixel-exact
  layout and style are an end-game target, not a day-one gate.

## User interface

- Same menus, hotkeys, and mouse interactions as the original.
- Wire UI in as gameplay and screens come online; do not invent alternate
  control schemes when the original already defines one.

## Gameplay loop

- Same turn-based loop as the original (natives → colonial powers, calendar,
  end-of-turn resolution, win/retire/revolution paths as in the manual).
- Rules and systems aim at original behavior; stubs and incomplete logic are
  temporary and tracked toward removal (see Determinism).

## Interoperability

- Consume the same original data files the DOS game uses (no proprietary
  replacement asset pack required to play).
- Save and load are **100% interoperable** with the original:
  - Start in DOS → load in the port → continue.
  - Start in the port → load in DOS → continue.
  - Default save/data directory is `<exe>/COLONIZE` (DOS-style layout); override with `--save-dir` / `--data-dir`.
- Round-trips preserve Col1 layout and unknown regions as documented in
  [savegame.md](savegame.md).
- Path to a complete decomp-backed field map (opaque regions, connectivity,
  export rebuild): [save_format_map.md](save_format_map.md).

## Determinism

- Ideal bar: with a fixed pseudo-random seed, the same game state and the same
  inputs produce the same outputs in DOS and in the port.
- **Working drafts OK early:** incomplete or divergent logic is acceptable
  while systems are being brought up.
- By project end, draft gaps should be reduced to nothing for gameplay-affecting
  logic (RNG, combat, AI decisions, production, diplomacy, etc.).

## Rules accuracy bar

Prefer real rules from `Colonization.pdf`, [fandom_col1994.md](fandom_col1994.md),
and NAMES/decomp before inventing stand-ins.

- "Rough" means incomplete wiring/UI, **not** made-up powers.
- If the real effect needs a missing hook: implement the closest real behavior,
  or PARK with a comment naming the intended effect — no gold/crosses fiction.
- Cite the source effect in the code comment for each Founding Father / rule
  change.

Authority order for effect semantics: original code / NAMES / decomp →
`Colonization.pdf` → [fandom_col1994.md](fandom_col1994.md) → documented
temporary stub only if those give no usable rule.

Forbidden: inventing gold/tools/moves/crosses stand-ins when the manual/wiki
already names the real power (e.g. Smith factory gate, Drake privateer +50%,
Brewster dock filter).

---

## Acceptance order (when goals conflict)

1. **Save / data interoperability** — never break Col1 or original data use.
2. **Gameplay / determinism** — correct rules and reproducible outcomes.
3. **UI parity** — hotkeys, menus, mouse paths match the original.
4. **Visual polish** — pixel-exact layout and style last.

Related: [roadmap.md](roadmap.md) (whole-project phases),
[manual_gap.md](manual_gap.md) (feature checklist),
[savegame.md](savegame.md) (Col1 layout / interop),
[save_format_map.md](save_format_map.md) (field atlas / RE roadmap),
[assets.md](assets.md) (data files).
