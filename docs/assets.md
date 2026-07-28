# Colonization Asset Notes

Runtime data lives under `COLONIZE/` (override with `--data-dir`).

## Text Encoding

- Encoding: legacy DOS OEM / ASCII with printable 7-bit English.
- Line endings: CR/LF (`\r\n`). Parsers must accept both CR/LF and LF.
- Comment lines start with `;`.
- Section headers start with `@NAME` (e.g. `@BEGINMENU`, `@GAME`).
- Hotkeys are marked with `~` before the hotkey character (e.g. `~Save Game`).
- Directive lines inside sections also start with `@` (`@width=160`, `@default=2`, `@options`).

Key text catalogs:

| File | Role |
|------|------|
| `GAME.TXT` | Dialogs, main menu (`@BEGINMENU`), prompts |
| `MENU.TXT` | In-game menu bar structure |
| `LABELS.TXT` | Short UI labels |
| `COLONY.TXT` | Colony name lists by nation |
| `ERRORS.DB` | Error / series name strings |
| `MODULES.DB` | Module tags |

## Binary / Packed Graphics

Many `.PIK` and `.SS` files begin with the ASCII header:

```text
MADSPACK 2.0\x1a
```

followed by:

| Offset | Type | Meaning |
|--------|------|---------|
| 0 | 12 bytes | `"MADSPACK 2.0"` |
| 12 | u16 | `0x001A` |
| 14 | u16 | section count (max 16) |
| 16 | 160 bytes | section headers (16 × 10) |
| 176 | ... | section payloads |

Each section header is `flags:u16`, `uncompressed_size:u32`, `compressed_size:u32`.
If `flags & 1`, the payload is FAB-compressed (`FAB` + shift byte + bitstream).

Colonization `.PIK` layout (after MADSPACK explode):

| Section | Content |
|---------|---------|
| 0 | Header: height, width, unk, unk (u16 each) |
| 1 | Indexed 8-bit pixels (`width * height`) |
| 2 | Optional VGA palette (768 bytes = 256×RGB, 6-bit DAC values) |

The Linux port decompresses MADSPACK/FAB and blits `.PIK` images. The main menu uses `OPENMENU.PIK` with its embedded palette. `CCBKGD.PIK` is the Continental Congress / Founding Fathers background.

`.SS` sprite sheets (e.g. `TERRAIN.SS`, `CURSOR.SS`) use four MADSPACK sections: header, per-sprite metadata, palette, and linemode-compressed pixel data. Sprites are blitted with transparency at index `0xFD`.

`.FF` fonts (e.g. `FONTSMAL.FF`, `FONTTINY.FF`) are single-section MADSPACK files. After decompression:

| Offset | Size | Content |
|--------|------|---------|
| 0 | 1 | `max_height` |
| 1 | 1 | `max_width` |
| 2 | 128 | glyph widths for code points 1–127 |
| 130 | 256 | glyph offsets (u16 for code points 1–127, plus padding) |
| 386 | ... | 2-bit-per-pixel glyph data (4 pixels per byte) |

Palette indices used by glyphs: `0` = transparent, `1` = `0x0F`, `2` = `0x07`, `3` = `0x08`.

## World Map (`.MP`)

Colonization scenario maps (e.g. `AMER2.MP`) are raw binary files:

| Offset | Size | Content |
|--------|------|---------|
| 0 | 1 | map width |
| 1 | 1 | unknown (always 0) |
| 2 | 1 | map height |
| 3 | 3 | unknown header padding |
| 6 | W×H | terrain layer |
| 6+W×H | W×H | layer 2 (unused in shipped maps) |
| 6+2×W×H | W×H | layer 3 (fog / visibility in-game) |

Each terrain byte:

- bits 0–4: terrain index 0–26 (tundra … high seas; see FreeCol `ColonizationMapLoader`)
- bits 5–7: overlay (0=none, 1=hill, 2=minor river, 5=mountain, 6=major river, …)

The Linux port maps terrain indices to `TERRAIN.SS` sprites (12 base tiles). Hill/river/mountain overlays from `PHYS0.SS` are not yet drawn.

| Extension | Typical use |
|-----------|-------------|
| `.PIK` | Packed pictures / backgrounds |
| `.SS` | Sprite sheets / animation frames |
| `.PAL` | VGA palette (`VICEROY.PAL`, 1024 bytes = 256×RGBA, 6-bit VGA RGB) |
| `.COL` | Sound / music related blobs |
| `.MOV` | Short motion / script tables |
| `.MP` | Map data |
| `.DAT` | Tables / path data |
| `.BIN` | Large binary blobs (e.g. `COLDIG.BIN`) |

## Discovery Order

1. Explicit `--data-dir`
2. `<executable-dir>/COLONIZE`
3. `./COLONIZE` (working directory)
