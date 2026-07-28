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

### TERRAIN.SS sprite index (`terrain & 0x1f`)

| Sprite | Terrain type |
|--------|----------------|
| 0 | Tundra |
| 1 | Desert |
| 2 | Plains |
| 3 | Prairie (cotton) |
| 4 | Grassland (tobacco) |
| 5 | Savannah (sugar) |
| 6 | Marsh |
| 7 | Swamp |
| 8 | Scrub forest |
| 9 | Arctic |
| 10 | Ocean |
| 11 | High seas |

Map indices 24/25/26 map to arctic/ocean/high-seas sprites. Indices 12–23 reuse land sprites modulo 12 until cleared/forested variants are modeled.

### PHYS0.SS sprite atlas (press `` ` `` in-game)

| Sprites | Content |
|---------|---------|
| 0, 16 | Blank |
| 1–15 | Major rivers |
| 17–31 | Minor rivers |
| 32–47 | Mountains |
| 48–63 | Hills |
| 64–79 | Mixed forests |
| 80–88 | Roads |
| 89 | Depleted silver |
| 90 | Oasis |
| 91–95 | Wheat, cotton, tobacco, sugar, ore (flat) |
| 96–99 | Fish, beaver, deer, timber |
| 100 | Empty |
| 101–103 | Silver, ore (hill), rumours |
| 104–111 | Fog-of-war edges (approx.) |
| 112–127 | Coastline 8×8 fragments (edge variants; not yet placed) |
| 128–131 | Coastline 8×8 diagonal (checkerboard 2×2) |
| 132–139 | Coastline 8×8 fragments (not yet placed) |
| 140–147 | Coastline 16×16 animation frames (approx.) |
| 148 | Ocean overlay |
| 149 | Plowed |
| 150–153 | Coastal ocean corners (NW/NE/SW/SE land → 150/151/152/153) |

### Map overlay compositing

The Linux port draws cleared terrain from `TERRAIN.SS` (using FreeCol-style decoding of bits 0–4), then composites `.MP` overlays from `PHYS0.SS`.

**Terrain byte decode (layer 1):**

- Bits 0–2: cleared land type 0–7
- Bit 3 only (`0x08`…): forest terrain index `8 + (bits 0–2)` → TERRAIN sprite 8 for now
- Bit 4 only (`0x10`…): cleared base type in bits 0–2 (overlay bit 4 is not part of the terrain index)
- Index 24/25/26 → arctic / ocean / high seas sprites 9/10/11
- Row `y=0` land tiles display as cleared tundra (sprite 0) with PHYS0 forest sprite 65

**Overlay rules (bits 5–7):**

| Overlay | PHYS0 range | Notes |
|---------|-------------|-------|
| 0, 4 | (none) | — |
| 1 hill | 48–63 | Unless bit 4 is set → mountain (below) |
| 2 minor river | 17–31 | `17 + (mask % 15)` |
| 3 hill + minor river | mountain or hill, then river | Bit 4 selects mountain vs hill |
| 5 mountain | 32–47 | Isolated tile uses sprite **36** |
| 6 major river | 1–15 | `1 + (mask % 15)` |
| 7 mountain + major river | mountain then river | — |

When overlay is 1/3 **and** bit 4 is set in the terrain byte, the tile uses mountain art (e.g. AMER2 `(1,1)` → PHYS0 36 on tundra).

Forests on other rows, roads, resources, and fog overlays are not drawn from static `.MP` data yet.

**Coastal ocean** (fixture-backed bring-up; not yet matched to recovered DOS compositor code):

| Pattern | PHYS0 | Size |
|---------|-------|------|
| Only sea in 2×2 (3 land) SE/SW/NE/NW | 153 / 152 / 151 / 150 | 16×16 |
| Diagonal checkerboard (2 land on anti-diagonal, far corner sea) SE/SW/NE/NW | 130 / 131 / 129 / 128 | 8×8 in that quadrant |

(Sheet artwork sits opposite the land corner — equivalent to flipping each piece on both axes.) Corners stack. AMER2 `(6,14)` → **153+152+151+150**. Diagonal examples: `(33,6)`→130, `(9,25)`→131, `(8,26)`→129, `(34,7)`→128. Other 8×8 coastline bands (112–127, 132–139) and animation frames (140–147) are not drawn yet.

Tile compositing tables extracted from `VICEROY.EXE` live in `src/data/viceroy_tables.{h,c}`; see [viceroy_tables.md](viceroy_tables.md). **Deferred work** on recovering the real DOS compositor is tracked in [decomp_inventory.md](decomp_inventory.md).

### Colonizopedia terrain preview

Press **P** from the menu or map to open a terrain Colonizopedia page (`PEDIA.TXT` `@TERRAIN0`–`@TERRAIN28`). Left/Right cycles types; Esc or P exits. Each page shows a 3×3 sample of the TERRAIN/PHYS0 composite plus the title and body text.

### Europe (home port) bring-up

Press **E** from the map to open the European Status screen (`EUROPE.PIK`). Esc or E returns to the map.

| Key | Action |
|-----|--------|
| R | Recruit cheapest `@CLASS` immigrant onto the docks |
| T | Train (stub) |
| `]` | Cheat: +1000 gold |
| `[` | Cheat: −1% tax |

Market bid/ask prices come from `NAMES.TXT` `@CARGO` (ask = bid + burden + 1). Ship cargo holds, drag-trade, and sailing to the New World are not implemented yet.

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
