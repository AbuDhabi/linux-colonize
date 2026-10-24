# Popup inventory

Inventory of every player-facing modal in original Colonization, cross-checked
against the port. Canonical copy identity is the `@SECTION` name in
[`COLONIZE/GAME.TXT`](../COLONIZE/GAME.TXT) (499 sections). Decomp call sites
are secondary citations.

Feature-level status also lives in [manual_gap.md](manual_gap.md) and
[port_plan.md](port_plan.md). This file is the **popup checklist**.
Authenticity audit (closed): [archive/popup_audit.md](archive/popup_audit.md).

## Legend

| Status | Meaning |
|--------|---------|
| **Done** | Playable wood/list modal with correct choices/behavior (VGA-identical chrome may still be parked) |
| **Partial** | Thin OK, status line, simplified choices, incomplete GAME.TXT fidelity, or PARKED FA stand-in |
| **Missing** | No user-facing modal in the port yet |
| **n/a** | Catalog sentinel / not a player modal |

**Partial** no longer means “invented English OK is fine.” Invented wood INFO OKs are demoted to status; real `@SECTION`s should be wired via `popup_msg_*`.

Structural Done (choices work) is enough per [project_goals.md](project_goals.md);
pixel VGA / portraits are end-game polish.

## Architecture

Original DOS uses one dialog compositor plus a thin message-box helper. Triggers
are scattered; presentation is centralized.

```mermaid
flowchart LR
  GameTxt["GAME.TXT @SECTION"]
  Decomp["FUN_6f74 / FUN_7aa1"]
  PortChrome["popup_draw WOODTILE/OPENTILE"]
  Dedicated["Dedicated dialogs"]
  AiQueue["ai_popup queue"]
  User["Player"]

  GameTxt --> Decomp
  GameTxt --> Dedicated
  GameTxt --> AiQueue
  Decomp -.->|"port mirrors"| PortChrome
  PortChrome --> Dedicated
  PortChrome --> AiQueue
  Dedicated --> User
  AiQueue --> User
```

| Layer | Original | Port |
|-------|----------|------|
| Script source | `GAME.TXT` (+ `DEBUG.TXT` cheats) | `assets_msg_find` / hardcoded snippets |
| Compositor | segment `6f74` (`FUN_6f74_36ca` / `3760` / `3848` …) | [`popup.c`](../src/core/popup.c) chrome |
| Thin message box | `FUN_7aa1_003a` | often status line or `ai_popup` KIND_OK (body only; no invent “OK” button — click/key dismiss) |
| Map event queue | flush immediately from AI/turn | [`ai_popup.c`](../src/core/ai_popup.c) (`AI_POPUP_QUEUE_MAX`) |
| Dedicated UIs | colony `2f2b`, Europe `38fd`, save `7562`, … | `colony_screen`, Europe menus in `game_loop`, `save_load_dialog`, `pick_music`, `unit_stack`, `cheat_list_dialog`, `new_game` |

### The map status line is not a popup (port_bugs.md #369)

DOS has a second, lighter channel that the port kept mistaking for a dialog.
`DS:0x2d54` is a text buffer; `FUN_0000_035c` repaints the map's top strip
every frame and, whenever that buffer is non-empty, draws **its** text there
instead of the strip's normal content (wood pattern id 0x22, ink 0x95 from
`FUN_1009_0004`). Composition is a small append API, all of it via
`FUN_1009_017e` (each append also leaves one trailing space):

| Resident | Thunk | Does |
|----------|-------|------|
| `FUN_1009_00b4(1)` | `281f_0056` | **flush**: wait out the pending line, then clear `0x4a` + the buffer |
| `FUN_1009_017e` | `281f_006a` | append a `char*` |
| `FUN_1009_01a2` | `281f_0074` | append a pooled string by index (`FUN_1000_0062`) |
| `FUN_1009_01b8` | `281f_007e` | append an integer |
| `FUN_1009_0222` | `281f_0088` | drop the last character (kills the trailing space before punctuation) |
| `FUN_1009_0244(kind, ticks, hi)` | `281f_0092` | arm it: `0x4a = 1`, deadline = now + ticks |

The wait (`FUN_1009_0036`) runs until `min(armed deadline, now + 30 ticks)` on
the 60.877 Hz clock, broken early by any key or mouse button — so in practice
each line owns the strip for about half a second and a run of them plays as a
sequence. It **blocks** like a dialog does, but nothing is clicked away.

The **ink** comes from the arm kind (`FUN_1009_0244`'s first argument, kept in
`DS:0x4c`) via `FUN_1009_0004` / `FUN_0000_0094`:

| kind | ink | used by |
|------|-----|---------|
| 1, 2 | `0x95` @COLORS hilite (gold) | every success line — Custom House sale, Europe sale |
| 3 | `0x0c` bright red | Europe refusals (boycott, not enough gold, no room) |
| other | `0x44` @COLORS basic (green) | — |

Port: a ring on `AiPopupState` (`ai_popup_enqueue_bar_message` /
`_kind`, `ai_popup_bar_service`, `ai_popup_bar_message_color`),
`map_menu_set_message(bar, text, ink)` painting the strip in place of the
pull-down titles, and `game_service_bar_message` holding the turn pipeline
while a line is up (parked, not ticked, while any screen covers the map).
Dwell follows DOS's `min(deadline, now + 30 ticks)`: a line with more behind it
holds ~0.5 s, the last of a run lives out the full 0x78 ticks (1971 ms).

Users:

- Custom House sales (`FUN_364b_0688`, one line per cargo) — map strip.
- **Europe sales** (`FUN_38fd_23c4`, arm `FUN_38fd_19d8(1, 0x78, 0)`) — the
  European Status screen's own top strip, `<amount> <Cargo>` + `@CMESSAGE 1`
  "sold for" + gross + `DS:0xfef` "." + tax rate + `@CMESSAGE 0x11` + tax paid
  + `@CMESSAGE 0x12` + net. This one does **not** block: the screen keeps
  taking input and `FUN_1009_0270` retires the line on its deadline. Port:
  `EuropeScreen.bar_event` + `europe_push_sale_status`, drained by
  `game_service_europe_bar`, painted by `render_europe_screen`.

### Dialog script parser — blank lines are the body/choice separator

`FUN_6f74_32a4` (EXE 117247) is a **state machine keyed on blank lines**, not a
keyword matcher. `local_6` starts at 1 and is incremented by every empty line;
the loop runs `while (local_6 < 3)`:

| state | meaning |
|---|---|
| 1 | body prose (`291f_08c6` append text line) |
| 2 | choice rows (`291f_0176` add option, ordinal in `local_164`) |
| 3 | stop |

`@`-directives inside a section: `@TEXT` forces state 1, `@OPTIONS` / `@PROMPT`
force state 2, `@SMALLFONT` / `@X` / `@Y` / `@WIDTH` / `@LENGTH` / `@CHECKBOX` /
`@DEFAULT` set box fields, and **any unrecognised `@` sets state 3** — that is
how the next `@SECTION` ends the parse. `@DEFAULT=N` pre-selects the option
whose ordinal is N (`local_164 == local_16c` → box `+0x4c`).

Port: `assets_msg_load_file` records a `blank_before[]` flag per stored line
(blank lines are still not stored — ordinal indexing depends on that), and
`popup_msg_section_body` / `popup_msg_choices` run the same state machine.
`@default` rides the same side-channel as `@width`
(`popup_msg_take_pending_default` → `AiPopupRequest.default_choice` →
`ai_popup` initial selection). `unit_popup_msg` sweeps all 499 GAME.TXT
sections asserting no choice row appears in a body.

**Missing sections show nothing.** `FUN_7314_001a` scans the file for `@TAG`
and returns non-zero at EOF; `32a4` then never allocates a box and `36ca`
returns 0 without drawing. So a tag DOS pushes but GAME.TXT does not ship
(`@CANCELTREATY`, `0x1898`) is a **silent no-op**, not a fallback popup.
The port keeps that rule; the one exception is `@CANCELTREATY`, which gets a
port-authored notice ([dos_bugs.md](dos_bugs.md) DB-52).

### `{}` emphasis markup

GAME.TXT colors braced spans (`{far too crowded}`, `{%STRING0}`) with the
@COLORS **hilite** ink (149, gold on the in-game / WOODPANL / EUROPE
palettes) instead of **basic** (68 green). DOS mechanism (dialog text writer
`FUN_6f74_0538`, file offset 0x6C388): `{` sets DS:`0x1f62` = 1, `}` clears
it, both zero-width; the per-char color helper (`0xc346`) picks struct color
+6 (emphasis) over +2 (normal), with the +4 grey/disabled ink overriding
both; `~` draws the next char as a plain glyph in the other ink (hilite outside
`{}`, base inside: the option hotkey letters) and so also escapes it, `|` ends the
string. `0x1f62` starts 0 per popup and carries across every drawn line — title,
body, then choice rows.

Port: `popup_msg_apply_tokens` **keeps** `{}` (it used to strip); renderers
eat the braces at draw time via `popup_markup_text_width` /
`popup_draw_text_markup` ([`popup.c`](../src/core/popup.c)). Markup-aware:
`ai_popup_render` (title/body/choices, state carried DOS-style, new
`hilite_color` arg), `options_dialog` rows (@COLONYOPTIONS), Europe dock
menu rows (@ARMOPTIONS; greyed rows stay all-grey like DOS), colony-screen
custom-house/dock-orders titles, plus the pre-existing brace-aware paths
(title menu, `europe_draw_prose`, pedia, new_game). Plain-text sinks strip
via `popup_msg_strip_markup` (window-title status via `game_status_text`,
`colony_screen_set_status`, ai_contact/ai_diplo/ai_euro status lines).

### `%NUMBERn$` — the `$` is literal

GAME.TXT writes gold amounts as `{%NUMBER0$}`. The DOS expander
`FUN_6f74_309c` matches the 8-byte `%NUMBERn` token, formats the number and
resumes copying at `+8`, so the `$` that follows is copied verbatim: DOS shows
`500$`, and `popup_msg_apply_tokens` does the same. Not a bug.

### Wood frame recipe — `FUN_6f74_2278`

The dialog frame is **not** ornate art: it is four drawn layers plus a tiled
brush, and `popup.c`'s `popup_draw` already reproduces all of it. `FUN_6f74_2278`
(6f74:2278, called from `FUN_6f74_248e` via `thunk_FUN_2a1f_0710` with the box's
own `+0x10/+0x12/+0x14/+0x16`) emits, for rect `(x, y, w, h)`:

| # | Run | Colour |
|---|-----|--------|
| 1 | outline `(x, y)`–`(x+w-1, y+h-1)` | `0` black |
| 2 | outline `(x+1, y+1)`–`(x+w-2, y+h-2)` | `DS:0x1f44` = @COLORS **border0** (134) |
| 3 | vline `x+2`, `y+2`..`y+h-3` | `DS:0x1f48` = **border2** (138, dark) |
| 4 | vline `x+w-3`, same rows | `DS:0x1f46` = **border1** (128, light) |
| 5 | hline `y+2`, `x+2`..`x+w-3` | border1 |
| 6 | hline `y+h-3`, same span | border2 |
| 7 | fill `(x+3, y+3, w-6, h-6)` | brush `DS:0x1f3c/0x1f3e` |

**Trap — the bevel corners are decided by order, not by rule.** The two
verticals go down first and the horizontals paint over them, so the shared
top-left pixel is *light* and the bottom-right *dark*.

`DS:0x1f44/46/48` are copied from the NAMES.TXT `@COLORS` bytes at
`DS:0x837/0x838/0x839` by `FUN_75c2_0204` — the same border0/1/2 the port
already uses. `FUN_75c2_024c` is the title-menu variant (mid `0x2e`, border1
`0xfd`, border2 `0x37`, literal indices, no `@COLORS` read).

The brush is real wood, not a dither. Layer 7 goes through `FUN_6f74_033c`
(Ghidra mislabels the wrapped near call as `FUN_7b29_47ec`): when the colour
argument is `7` *and* `DS:0x1f6c` is set it tiles the 32×24 bitmap that
pointer names, else it is a solid `FUN_281f_00ba` rect. `FUN_75c2_0204` builds
that bitmap from **WOODTILE.SS sprite 1** (1-based → the port's sprite 0; the
sheet holds exactly one 32×24 sprite) and points `DS:0x1f6c` at it; the
title-menu set points at the OPENTILE.SS copy, and `DS:0x82e` holds a third
built from PARCH.SS. Brush phase is anchored on the box origin, which is what
`popup_tile_rect` tiling from `(x, y)` reproduces. `TEXTCOLR.TXT`, the only
other writer of `DS:0x1f3c/0x1f3e`, does not ship — those slots keep their EXE
defaults, which is why the `7` arm always wins.

Box flag `0x10` (armed by `FUN_6f74_37f6`, `DS:0x1f56 |= 0x18`) drops layers
1–6 **and** the 3px inset, so the fill covers the whole rect. The King audience
`FUN_75c2_20e2` arms it — its `@KINGLOSE`/`@KINGWIN` text is frameless over the
throne art, which `new_game_render_throne_audience` already matches.

### Popup decorations (MSS/MYR/IND/KING sheets)

DOS decorates many popups with a figure sprite. Three latches, all cleared
after every popup show (`LAB_6f74_3018`):

- `DS:0x1f5c` chief/king portrait (`IND{tribe}A{tier}.SS` / `KING.SS`,
  `FUN_6f74_0042`) — ported earlier (P8.6).
- `DS:0x1f5e` = MSS index → `MSS{0-5}.SS` (`FUN_6f74_00c2`, set via far stub
  `FUN_281f_0652(tag, index)` or direct writes): 0 admiral (ships/sea),
  1 continental soldier (war/king/revolution), 2 courtier (gold/prices/court),
  3 frontiersman (terrain work/scout/LCR), 4 friar (religion/unrest),
  5 nun (colony stores/raids).
- `DS:0x1f60` = MYR index → `MYR{0-3}.SS` (`FUN_6f74_00ec`, stub
  `FUN_2a1f_0688(tag, nation)`): Euro ruler by nation id, used by the
  `5bfb` diplomacy dialogs (13b0/153e).

Placement is data-driven from each sheet's 0x98-byte section-0 header
(bytes 0x0e/0x10/0x12 → `ss.h` `place_offset_y`/`place_mode`/`place_offset_x`):
the sprite stands **above** the dialog, bottom overlapping the dialog top by
`place_offset_y`; mode 0 = left edge (horizontal overlap `place_offset_x`),
1 = centred, 2 = right edge; the pair is centred vertically as a unit, and the
sprite is dropped when the combined height reaches 200 (compositor
`viceroy_unpacked.c:116068`).

Draw order matters and depends on which latch is set. `FUN_6f74_248e`
(`viceroy_unpacked.c:116519`) calls the sprite blit `thunk_FUN_2a1f_0ab6`
(→ `FUN_6f74_1ae8`) **before** the frame when `DS:0x1f5c >= 0` and **after**
everything when `DS:0x1f5c < 0`: a chief/King portrait sits *under* the wood,
an MSS/MYR figure *over* it (`ai_popup_render`).

Port wiring: the (tag → index) pairs were lifted from every constant
`FUN_281f_0652` PUSH pair in the VICEROY.EXE asm and keyed by section name in
`popup_msg.c` (`k_popup_msg_mss`); `popup_msg_fill` records the section's index
in a side-channel (same pattern as `@width`) and the next `ai_popup` enqueue
takes it, so all existing fill→enqueue sites decorate without signature
changes. Explicit overrides where DOS latches without a constant tag:
Fountain-of-Youth picks (MSS3) and the Brewster pick (MSS4) in `units.c`
(`FUN_38fd_4884` writes), MYR via `ai_popup_set_last_graphic_myr` in
`ai_diplo.c`'s talk helpers (skipped when the section carries its own MSS
figure, e.g. `@DECLAREWAR`). MYR wins when both are set (DOS loads it last);
a chief portrait suppresses the MSS/MYR sprite in the port. Every DOS writer of the three
latches (shared-tail 0652 pairs, digit-patched tags, `FUN_364b_0000`'s param_6, the direct
writes, the 39 chief-portrait sites) is listed in [popups_catalog.md](popups_catalog.md)
Appendix C (port_bugs.md #1220); the figure goes on after the text as well (`ai_popup_render`,
`popup_list_render` `PopupListMetrics.figure`); @SIEGE, @INDIANFOREST, @CREATE, @CSHIP,
@FOREIGN, @SETVIEW and @SETHUMAN match the DOSBox-X captures outside the mouse pointer.
Tests: `smoke_ai_popup case_siege_figure_is_dos`, `unit_popup_msg
case_dialog_figure_scan`, `smoke_popup_dialogs cheat_create_unit_is_dos`.

#### Europe in-screen menus + the King helper

The Europe Recruit/Purchase menus are drawn by `game_loop`, not `ai_popup`, so
they missed the side-channel. DOS builds them through the *same* generic list
dialog (`FUN_291f_0182` → `FUN_6f74_32a4`, shown by `FUN_291f_016a` →
`FUN_6f74_2580`, which is what reads all three latches and calls
`FUN_6f74_14c6`), latching immediately before it. Asm PUSH/LEA pairs, since
Ghidra drops these args (`LEA BX,[0x87c]` = `GAME`, `LEA AX,[tag]`):

| DOS site | asm | tag | latch |
|---|---|---|---|
| `FUN_38fd_4884(1,0)` FoY pick | `38fd:4910` | `0x10f1` `@LOSTCITY0` | MSS3 — already wired (`units.c`) |
| `FUN_38fd_4884(0,1)` Brewster | `38fd:4924` | `0x10fb` `@RECRUITCHOOSE` | MSS4 — already wired (`units.c`) |
| `FUN_38fd_4884(0,0)` Recruit menu | `38fd:4948` | `0x1109` `@RECRUIT` | **MSS2 — wired now** |
| `FUN_38fd_4b50` Purchase menu | `38fd:4b5d` | `0x1111` `@PURCHASE` | **MSS2 — wired now** |
| `FUN_38fd_2a92` sail confirm | `38fd:2aba` | `0xffc` `@SAILAWAY` | **MSS0 — wired now** (table row) |
| `FUN_2b5a_001e` Land Ho name prompt | `2b5a:0028` (`MOV [0x1f5e],0`) | `0x8a2` `@LANDHO` | MSS0 — `k_popup_msg_mss` row, drawn by `popup_field_frame` (port_bugs.md #1183) |
| `FUN_479b_076e` colony name prompt (human only) | `479b:0823` (`MOV [0x1f5e],5`) | `0x146f` `@COLONY` | MSS5 — same |

`@KINGRECRUIT` (Train) and the dock `@ARMOPTIONS` menu latch nothing, and DOS
shows them bare — `original_screenshots/europe/train.png` confirms.

The text-entry prompts (`FUN_291f_0120` → `FUN_6f74_2580`, the same show) take
the same latch: `name_entry` keeps it (`mss`, from `popup_msg_take_pending_graphic`)
and `popup_field_frame` re-places the frame around the figure with the
`FUN_6f74_14c6` arithmetic (`6f74:18ae-1a0c`: total `t = sprite_h - min(sprite_h, oy) + h`,
figure y `100 - (t >> 1)`, frame y `gy - oy + sprite_h`, mode 0 frame x
`gx - ox + sprite_w`, mode 1 frame x unmoved) and blits it last. `@RENAMECOLONY`,
`@TRADENAME` and `@HOWMUCH1-5` latch nothing in DOS and stay bare.

Placement is the same header-driven rule as `ai_popup_render`, now also in
`europe_menu_layout` (`game_loop_render.c`): both call `popup_place_dos`
(`popup.c`), the `FUN_6f74_14c6` placement (plain arm `6f74:16c8-1735`, figure
arm `18ae-1a0c`): `160 - (w >> 1)` / `100 - (h >> 1)` as arithmetic shifts, right
edge pulled back to 320 and bottom to 200, no menu-bar floor, and with a figure
the assembly total `t = sprite_h - min(sprite_h, oy) + h` (dropped at `t >= 200`),
figure y `100 - (t >> 1)`, frame y `fig_y - oy + sprite_h`. MSS2.SS is 122×84 with
`place_offset_y=6`, `place_mode=1` (centred), `place_offset_x=0`. The DOS shots
fix the arithmetic: `recruit.png`'s dialog is 103 px (black top rule row 88,
bottom 190), `t = 84 - 6 + 103 = 181`, figure y `100 - 90 = 10`, dialog y 88;
`purchase.png`'s 119 px dialog gives `t = 197`, figure y 2, dialog y 80;
`train.png`'s undecorated 180 px dialog lands at y=10. Both odd totals sit 1 px
below the halve-the-margin form `(200 - t) / 2`, which is therefore wrong
(port_bugs.md #1186). `160`/`100`/`320`/`200` are `fb_w / 2`, `fb_h / 2`, `fb_w`,
`fb_h` (port-only: the overland map view in a window larger than 320×200 centres
in its own frame; every other screen, screens-on-map included, is the 320×200
frame and gets the DOS values). `popup_field_frame` uses the same helper.
Tests: `unit_popup_prompt place_dos`, `smoke_ai_popup case_placement_is_dos`.
The sprite blits **after** the frame (`FUN_6f74_248e`: sprite-last whenever
`DS:0x1f5c < 0`), and `EUROPE.PIK`'s black 120..251 block takes MSS2's own
palette entries via `ai_popup_sheet_palette_merge` (merge, never remap).

Size and rows (port_bugs.md #1187; `europe_menu_layout`, `popup_dos_list_height` in
`popup.c`, `EuropeMenuLayout` in `game_loop_internal.h`). The height is the
`FUN_6f74_14c6` block `6f74:1540-16c8`: `h = 3 + 6 (frame) + prose_h + (glyph_h + 3) * rows
+ 3 + footer`, prose_h = lines * (glyph_h + 1) (`FUN_6f74_1198`, wrapped to
content - 4), footer = the `DS:0x1f66` "(F1 for Help)" line, +6 when the dialog font
is FONTTINY (`DS:0x89e`, `@smallfont`), else +3, 0 when the flag is clear; frame width
= `@width` (0x50 without one) + 6. `glyph_h` is FONTINTR 9 / FONTTINY 5 (a 6-row font counts
as 5). Recruit = 4 lines + 4 rows = 103, Purchase = 2 + 7 = 119, Train (FONTTINY) =
3 + 18 = 180, all x 62, w 196. Rows are `FUN_6f74_1b7c`: row k's selection bar tops
at `frame_y + 9 + prose_h` + k * (glyph_h + 3), is `glyph_h + 2` tall and spans frame
+ 4 .. frame + w - 5, and its text sits one row lower at `frame_x + 5 + box[+0x22]`
(default 4, the Recruit / Purchase / Train callers `FUN_38fd_4884` / `4b50` / `41ce` set 8;
the dock `@EUROPEARM` list `FUN_38fd_3746` sets neither 8 nor the footer flag, port_bugs.md
#1190); prose is at `(frame_x + 5, frame_y + 6)`. The footer is
`FUN_6f74_1a78`: FONTTINY face, text top `frame_bottom - 7` (`- 9` in a FONTTINY
dialog), right edge `frame_x + w - 2`, highlight ink, no drop shadow. The @REALLYBUY
confirm is a plain `FUN_281f_0652` popup (+0x22 stays 4, and `FUN_6f74_2580` clears
`DS:0x1f66` on every exit), so it has no footer (h = 12 + prose + rows). Ink is the
generic `COLONIZE_COL_BASIC` / `COLONIZE_COL_HILITE` / grey (85,150,52 / 199,162,32 /
85,85,85 in the shots). With the same fixture content (gold 1000) purchase.png and train.png
reproduce pixel for pixel over the whole dialog rect outside the mouse pointer.
The cost column's right edge (advance end at `frame_x + w - 13`) is measured off the
shots, not read from the asm. Tests: `unit_popup_prompt list_height_dos`,
`smoke_popup_dialogs europe_menu_frames_are_dos`.
The footer is live: with `DS:0x1f66` armed, F1 (barred row) or a right-click (row under the
pointer) returns the row with `DS:0x1f68` set. The None row still leaves; a Recruit / Train
row opens the skill article of its `@JOB` (`FUN_291f_08de`, the pool's 0x1c read as 0x13), a
Purchase row the unit article (`FUN_291f_0942`), and the Europe screen comes back with the
list up (`game_europe_menu_help`, `pedia_return_europe`). The Fountain of Youth and Brewster
picks (same `FUN_38fd_4884` body) open the pool slot's skill article and are re-shown unspent
(`ai_popup_requeue_current`, `game_dialogs.c`); DOS also redraws the Europe screen on that
path although both run from the map, which the port does not copy.

Moved to [popups_dialog_engine.md](popups_dialog_engine.md): dialog text shading, the
non-dialog `FUN_104b_*` writers, the new-game wizard and its dialog layout, in-game wrap /
width / centring, list rows, the Colonizopedia article body, centring users, "Sizes of the
remaining compositor dialogs", Up / Down, greyed `ai_popup` rows, "Custom House keys", "Units Present".

**King portrait (KING.SS, `DS:0x1f5c = 8`)** is wired at every DOS site: the
tax audience (`ai_king.c`, KING.SS base + KING2.SS frames + palette merge),
`FUN_38fd_3dc8`'s message-only arm (`38fd:402a MOV word [0x1f5c],0x8`, a tax
cut or no cargo eligible for a tea party), and the King-flair helper
`FUN_291f_0ad4` (→ `FUN_6f74_378a`, latches 8 then shows), whose only two call
sites are `38fd:5b6e PUSH 0x1134` = `@KINGNEWWAR` (`ai_king.c`) and
`3844:04e1 PUSH 0xf09` = `@LOSENOCOLONIES` (`turn.c`). Trap: the five extra
`0ad4` calls Ghidra shows in `FUN_2f2b_628a`/`6372`/`2f3e` and
`FUN_38fd_3694`/`4f6e` are the `5930` body re-decoded into neighbouring switch
tables (all carry `0x1134`), not real sites.

Still not wired: `FUN_479b_076e`'s idx-5 WoI popup.

Modal input (`game_dialogs.c` `game_handle_modal_input`): early gate before
parent hotkeys (E/Q/etc.) — pick_music → save_load → options →
combat_analysis → name_entry → howmuch → cheat_list → **ai_popups** →
trade_screen → unit_stack (opening/closing screens gate before all of these).
Letters typed in name/howmuch do not switch views. Every arm in this chain is
also a `GAME_MODAL_LIST` member, so `game_modal_open()` sees it structurally —
new modals only need to join that macro list (`game_loop.c`). The open map
pull-down (`map_menu.open_index >= 0`) is the one blocker outside the list:
`game_modal_open` tests it after the list (port_bugs.md #1091) while its input
still reaches `game_update_map_menu_bar`.

**Blocking invariant:** every modal blocks all simulation, including mid-EOT;
see architecture.md "Blocking-popup invariant".

**Colony-event choices:** every colony EOT message DOS routes
through `FUN_364b_0000` (`thunk_FUN_291f_09dc`) carries two choice rows —
LABELS.TXT `@MISC` 34/35 "Continue turn." (id 1) / "Zoom to colony." (id 2,
DS:0x2dfe/0x2e00) — while the per-colony latch DS:0xa898 is clear. Picking
zoom sets the latch (rest of that colony's batch presents optionless) and the
colony screen opens after the batch drains (`FUN_364b_0688` tail
`FUN_281f_0608`). Port: `ai_popup_enqueue_colony_event` (tag
`AI_POPUP_TAG_COLONY_EVENT`, payload = colony id) on the whole family —
BUILT / NOMOREWAREHOUSE / NOMOREWAGONS / DEPLETION / SoL (SONS*/TORY*/REBEL*)
/ EFFICIENT / INEFFICIENT / TRAIN* / NEWCOLONIST / FOOD1-2 / FOODLOW /
STARVE1-2 / ALREADYHAVE / NEEDTOOLS* / raw-out crumbs (LUMBER…TOOLS) /
SPOIL* / CARGOREADY*. `@VANISH` stays optionless (DOS passes the choice flag
false). `game_loop` latches the election (`ai_popup_colony_zoom_elect`) and
opens the colony screen via `ai_popup_take_colony_zoom` once no popups for
that colony remain — mid-EOT too, like DOS. Not in the family (DOS
`FUN_281f_0652` / ticker): TUTORIAL6, UNREST, WARN1/2, Custom House sale line.

**Out of main tables:** MAPEDIT.EXE ([`MAPEDIT.TXT`](../COLONIZE/MAPEDIT.TXT) —
19 sections), Colonizopedia articles, F2–F10 report *plates* (unless a nested
confirm), pulldown chrome from `MENU.TXT`. Woodcut discovery captions
(`WOODCUT.TXT`) are noted under Discovery.

---

## Inventory by system

Each row is one presentation site (what actually opens), not one `@SECTION`
fragment. Related sections are listed in the first column.

### 1. Shell / title / new game

| Popup / `@SECTION`s | When | Status | Port |
|---------------------|------|--------|------|
| `@BEGINMENU` | Title: New World / America / Customize / Load / Exit / HoF | Done | [`game_loop.c`](../src/core/game_loop.c) |
| `@AMERICA`, map pick | America vs generated / load `.MP` | Done | [`new_game.c`](../src/core/new_game.c) |
| `@DIFFICULTY` | Difficulty pick | Done | Image regions on `DIFFICUL.PIK` |
| `@PICKNATION` | Nation pick | Done | Image regions on `NATIONS.PIK` |
| `@LEADERNAME` | Leader name entry | Done | Wood name entry |
| `@VICEROY` / `@VICEROY2` | King audience intro | Done | `KINGLSS.PIK` phase |
| `@NATION0A`…`@NATION3B` | Nation lore | Done | Lore pages |
| `@BUILD1`…`@BUILD10` | Sail-away captions | Done | Over `LEVN*.PIK` |
| `@CUSTOM` / `@CLAND`…`@CCLIM` | Customize land/climate | Done | Image grid + `@MISC` labels |
| `@GAMEOPTIONS` / `@COLONYOPTIONS` / `@SOUNDOPTIONS` | Options checkboxes | Done | [`options_dialog.c`](../src/core/options_dialog.c) |
| `@DOS` / `@DOSYES` | Quit confirm | Done | Map/title confirm via `AI_POPUP_TAG_MAP_CONFIRM` |
| `@RETIRE` | Retire confirm | Done | Confirm then F10 score |
| `@MULTI*` | Multiplayer setup | Missing | — |

### 2. Save / load / music

| Popup / `@SECTION`s | When | Status | Port |
|---------------------|------|--------|------|
| `@SAVEGAME` (+ good/error) | Manual save slots | Done | [`save_load_dialog.c`](../src/core/save_load_dialog.c); GAME→Save / **S** |
| `@LOADGAME` (+ variants) | Manual / title load | Done | Same; slots 0–9; title LOAD / **L** |
| `@PICKMUSIC` + Independence/Military/Indian | GAME→Pick Music | Done | [`pick_music.c`](../src/core/pick_music.c) |

### 3. Map / unit orders

| Popup / `@SECTION`s | When | Status | Port |
|---------------------|------|--------|------|
| `@LANDFALL` / `@LANDFALL2` | Ship→bare land with passengers | Done | `AI_POPUP_TAG_LANDFALL` + `popup_msg_fill` |
| Stack picker | Multi-unit tile click | Done | [`unit_stack.c`](../src/core/unit_stack.c) |
| `@SUREDISBAND` / `@DISBANDSHIP` | Disband confirm / ship cargo error | Done | Confirm `@SUREDISBAND`; `@DISBANDSHIP` OK when units aboard |
| `@FINDCITY` / `@NOCITY` | VIEW Find Colony text entry | Done | `name_entry` FIND_COLONY (`game_open_find_colony`, DOS 2b5a:2028 `FUN_291f_0120(0x17)`); `game_find_colony`: first colony of any nation whose name equals the text case-blind and that the view may show (view >= 4, seen bit, or show_entire_map) → View Pieces + cursor there, no colony screen; else `@NOCITY` %STRING0 = the text (port_bugs.md #1197) |
| `@SAILPORT` | Unit ORDERS "Go to Port" (ships) | Done | `game_open_goto_port_picker`/`cheat_list` GOTO_PORT, DOS `FUN_647e_01c6`/`FUN_2b5a_1dfc`; Europe row first, rows filtered by `travel_dest_ok` (docs/unit_orders.md); paged (port_bugs.md #1194) by `cheat_list_open_dest_pages` (n <= 10 one page of n + 1 slots, else (n + 9) / 10 pages of 10, Europe = slot 0; LABELS @CTITLE 6 "(More)" as Previous / More; the DOS last-colony gap at Europe + 20/30/40 colonies not reproduced, dos_bugs.md DB-53), Europe row `@HOMEPORT (@COUNTRY)`, the ship's own colony greyed (`disabled_mask`, Up/Down wrap and skip it as `FUN_6f74_2580`), empty list shows nothing. No DOS screenshot of this box |
| `@TRAVELPLACE` | Unit ORDERS "Go to Place" (land units) | Done | port_bugs.md #1208: the same `FUN_2b5a_1dfc` / `FUN_647e_01c6` list as Go to Port (ORDERS 0x320 and 0x321 share case 2b5a:28d4), titled `@TRAVELPLACE`: own colonies on the unit's continent, no Europe row, the colony he stands on left out (its slot still counts); replaces the port's invented click-a-tile mode (DOS has none; the mouse drag `FUN_2b5a_3462` stays). No DOS screenshot of this box |
| `@OVERBOARD` | Which cargo to dump | Done | Hold picker (`AI_POPUP_TAG_OVERBOARD_WHICH`), ids = hold+1, 99 = cancel |
| Order gates (`@NOPLOW`, `@NOROAD`, `@NEEDTOOLS`, …) | Illegal order | Done thin | `@NOPLOW`/`@NOROAD` (`units.c` `units_pioneer_emit_order_gate`) — `@ONLYPIO` is dead text (port_bugs.md #621: the literal is absent from VICEROY.EXE; `0b34` raw 42211-42215 greys the menu rows instead), EOT `@NEEDTOOLS`/`@NEEDTOOLS0` (`turn.c`), `@SEACOLONY`/`@TOOMOUNTAIN`/`@NOPORT` (`game_loop.c`) all call `popup_msg_fill` with the real GAME.TXT body at the DOS condition; see Appendix A |

### 4. Colony screen

| Popup / `@SECTION`s | When | Status | Port |
|---------------------|------|--------|------|
| Construction CHANGE | CHANGE / **C** | Done | [`colony_screen.c`](../src/core/colony_screen.c); the built project's "(COMPLETE)" row is kept silently, `@ALREADYHAVE` / `@NOMOREWAREHOUSE` are EOT only (port_bugs.md #1223) |
| Field jobs | Assign colonist to field | Done | Job list popup |
| Leave-as (eject) | Fence with colonist | Done | Role list |
| `@ABANDON` / `@ABANDON2` | Last colonist leave | Done | Yes/No confirm from GAME.TXT |
| `@KEEPSTOCKADE` | Stockade min pop | Done | OK from `@KEEPSTOCKADE` |
| `@MORETHANTHREE` | Assign 4th colonist to a full building | Done | `colonies_assign_workplace` caps at `COLONIZE_BUILDING_MAX_WORKERS` (3); `colonies_emit_more_than_three_chrome` OK. Was previously misdocumented as a `@KEEPSTOCKADE` alias — the two sections are unrelated |
| `@COLONY` / `@RENAMECOLONY` | Found / rename | Done | Name entry after found (`@COLONY` wears the MSS5 nun, `479b:0823`); rename (bare, DOS latches nothing) = click colony title bar (DOS zone 10) or **R** (port extra) |
| `@LANDHO` | Human move with land in the destination 3x3, `named_new_world` clear (`FUN_4720_049e` tail) | Done | Name New World (`colony_region`); the field opens on the live `player[turn].country_name` (DOS passes that buffer, so `@default=America` is dead; a new game stores NAMES `@COLONYNAME` there). Text-entry layout: `popup_field_frame` (port_bugs.md #1181); MSS0 admiral (`2b5a:0028`, #1183) |
| `@HOWMUCH1`… | Cargo amount | Done | [`howmuch_dialog.c`](../src/core/howmuch_dialog.c) (`=` colony / Europe **L**) |
| `@WAREHOUSEFULL` | Warehouse overflow | Done thin | Unload full → `ai_popup` OK; spoilage still `@SPOIL*` |
| Train fails (`@NOTEACHER`, `@TRAINFAIL`, …) | School train | Done thin | EOT `@TRAINFAIL` (`turn.c`), assign-time `@NOTEACHER`/`@NEEDCOLLEGE`/`@NEEDUNIVERSITY` (`colony.c`) all `popup_msg_fill` real body at the DOS condition; see Appendix A |
| `@FULL` | Join at population cap | n/a | port_bugs.md #690: DOS join bails silently at 32; no popup |
| Spoil / starve (`@SPOIL*`, `@STARVE*`, …) | EOT production | Done | `ai_popup` OK from turn production |
| Docked unit orders | `@COLONYUNIT` + `@SHIPOPTIONS`/`@UNITOPTIONS`, 2nd click on selected dock icon | Done thin | `colony_screen_open_dock_orders`; ineligible rows omitted (`2f2b_5746`); VGA chrome PARKED |
| `@CARGOREADY*` | Century tip | Done thin | EOT century `@CARGOREADY0`–`2` |

### 5. Europe

| Popup / `@SECTION`s | When | Status | Port |
|---------------------|------|--------|------|
| `@RECRUIT*` | Recruit pool | Done | Wood menu **R** |
| `@PURCHASE` / `@REALLYBUY` | Buy ship/artillery | Done | ~PURCHASE |
| `@SCHOOL1` / `@COLLEGE2` / `@UNIV3` | Train expert | Done | ~TRAIN |
| Europe dock orders | Don’t board / Board / Move front | Done | `EUROPE_MENU_DOCK` |
| `@HOWMUCH4`/`5` | Buy/sell amount | Done | Europe **L**/**U** → howmuch |
| `@PRICEUP` / `@PRICEDOWN` | Price change notice | Done | OK popup (EOT + immediate buy/sell) |
| `@KINGTAX` / `@TAXOPTIONS` / `@TEAPARTY` | Tax audience (also map queue) | Done | `@KINGTAX` body + `@TAXOPTIONS` Kiss/Party; `@TEAPARTY` Done thin (stock dump + tokens; VGA PARKED) |
| `@KISSUP` / `@KISSSORRY` | Click a boycotted market cell | Done | `AI_POPUP_TAG_EUROPE_KISSUP` CHOICE (Unfair / Pay) from `game_europe_ask_boycott_buyback`; Pay then tests the purse and shows `@KISSSORRY` OK, exactly `FUN_38fd_2dfe`'s order |

### 6. Indian contact / trade / raids

| Popup / `@SECTION`s | When | Status | Port |
|---------------------|------|--------|------|
| `@INDIANWELCOME` → peace/shun | First contact Yes/No | Done | `CONTACT_WELCOME` |
| Meet Trade/Gift/Demand/Teach/Leave | Village meet | Done | `CONTACT_MEET` |
| Gift amount | Gift gold | Done | Small/Large/Generous CHOICE |
| Demand tools/gold | Tribute | Done | `CONTACT_DEMAND` |
| Teach (`@LEARN*`) | Teach skill result | Done | `ai_contact_live_among_natives` (menu path, `thunk_FUN_1000_a618`) renders the full script — `@LEARNMAD`/`@LEARNCRIMINAL`/`@LEARNMASTER`/`@LEARNALREADY`/`@LEARNSLOW`/`@LEARNSTAY` CHOICE → `@LEARNDONE`/`@LEARNLATER` — each with the real GAME.TXT body via `popup_msg_fill`; `ai_contact_teach_skill` is only a defensive fallback when no menu unit/village is present. `ai_contact_speak_with_chief` gives Scout→Seasoned `@WELLSEASONED`. AI-only auto-pulse (non-menu) path still skips silently — noted in Appendix A, tracked as a design tension, not a regression |
| Mission / convert (`@MISSION*`, `@INDIANSCONVERT`) | Missionary | Done | `CONTACT_CONVERT` with the real `@MISSION0-3` / `@HERESY0-1` / `@INDIANSCONVERT` bodies |
| `@RAID*` outcomes | Raid / ambush | Done | `CONTACT_RAID` + real `GAME.TXT` bodies via `popup_msg_fill` for 6 of 7 kinds; `4528` ported. `@RAIDWREAK` is deliberately thin: DOS raw `5fef:126a` does call `FUN_281f_0652(0x1b8a, mode=3)` from `FUN_5fef_0f14`, but with a different mode/loop shape than the other 5 kinds' `mode=5` victim calls, and the GAME.TXT `@RAIDWREAK` text is a third-person "Spies report: … the {adjective} colony of {name}" frame, not a victim status line — see `indian_raid_outcomes.md` |
| Village attitude / HELLO | Enter settlement | Done | `@VILLAGEHAPPY/SAVAGE/MEDIUM/BAD/WAR` band body via `popup_msg_fill`; `@INDIANHELLO1/2` are dead GAME.TXT (no DS tag string) |
| `@CHIEF*` | Scout speaks with chief | Done | `ai_contact_speak_with_chief` (`thunk_FUN_1000_a60c`): `@CHIEFHOWDY` / `@CHIEFGUIDES` / `@CHIEFAREA` / `@CHIEFGIFT` / `@CHIEFBORED` / `@CHIEFKILL` via `popup_msg_fill` |
| `@EXTORT*` | Demand tribute from a village | Done | `ai_contact_demand_tribute` (`thunk_FUN_1000_a5f4`): `@EXTORTSTUFF` / `@EXTORTPOOR` / `@EXTORTNO` / `@EXTORTLAUGH` |
| `@KILLWAGONS` / `@MADATWAGONS` / `@GRUDGEWAGONS` | Wagon / ship enters a village at alarm >= 75 | Done | `ai_contact_enter_hostile_village` (`thunk_FUN_1000_a5e8`) |
| `@TRADE0`/`1`, haggle `@BADHAGGLE*` | Deep village trade | Done (structural) | `2820` LAB_002bbc/002e92 human loops ported (`ai_contact_2820_sell_haggle`, `ai_contact_2e92_haggle`); VGA PARKED |
| Bribe / encroachment CHOICE | Pioneer clear / road, colony plot on tribal land | Done | `@INDIANFOREST` / `@INDIANROAD` / `@INDIANLAND` via `game_request_indian_land_choice` (`AI_POPUP_TAG_INDIAN_LAND`); "offer" greyed when gold < price (port_bugs.md #1214), `@INDIANBRIBE` after paying |

### 7. Euro diplomacy

| Popup / `@SECTION`s | When | Status | Port |
|---------------------|------|--------|------|
| War / Peace / Break | Rival offers | Done | `DIPLO_WAR` / `PEACE` / `BREAK` (Euro alliances are not DOS; `DIPLO_ALLIANCE` retired) |
| Boycott | War embargo / Tools lift | n/a | no DOS popup: war/peace OKs are `@DECLAREWAR` / `@SIGNTREATY` (invented "boycott imposed" / "embargo lifted" bodies no longer replace them; the Tools-lift line is status-only) |
| FA `@HELLO*` / `@PEACE*` / `@TRIBUTE*` … | Euro encounter negotiation | Done | `FUN_5bfb_153e` talk machine (`ai_diplo.c`, tag `DIPLO_TALK`); `3f41` is the F2-F9 adviser-report overlay, not diplomacy. `…USA` variants are the one delta |
| Military Assistance / mercenary hire (`@MILITARY`, `@UNFORTUNATE`, `@MERCENARY`) | PEACEMENU choice 4 | Done | `ai_diplo.c` `AI_TALK_ST_ALLY_PICK`/`ALLY_PAY` matches raw :98330-98395 (affordability gate, war-bit set both directions incl. Indian side), `@MILITARY` intro line included |
| Scout enters foreign colony (`@SCOUTCOLONY`, `@LOSTOURSCOUTS`, `@LOSTTHEIRSCOUTS`, `@NOMAYORSDURINGREV`) | Human scout steps onto Euro colony | Done for the human side | `AI_POPUP_TAG_SCOUT_COLONY` (`game_loop.c`); DOS's `@LOSTTHEIRSCOUTS`/`@NOWARSDURINGREV` branches (AI scout vs. human colony; any unit vs. any foreign colony during WoI) live in the shared `FUN_465b_0000`→`FUN_5f7a_0662` dispatcher, which the port never calls for AI-driven unit movement; both arms are dead code in DOS too (`FUN_5f7a_000e` head raw 98822-98834 returns unless the mover is human; port_bugs.md #500) — see Appendix A |

### 8. King / REF / independence / FF

| Popup / `@SECTION`s | When | Status | Port |
|---------------------|------|--------|------|
| Tax audience Accept/Refuse | King tax hike | Done | `KING_AUDIENCE` |
| Dump-goods cargo | Refuse → boycott pick | Done | `KING_DUMP_GOODS` |
| Tax/boycott follow-up | After audience | Done | `KING_TAX` OK from `@TEAPARTY` / `@KINGTAX` / rung sections (`@KINGNAVACT` …) via `popup_msg_fill` |
| Audience flavour sections | Which text the tax audience shows | Done | `FUN_38fd_5be8` names a section per ladder rung: `@KINGVICTORY` (cut) / `@KINGWIFE` (+1) / `@KINGWAR` (+2) / `@KINGNAVACT` (+3-4) / `@KINGSTAMPACT` (+5-8), with `%STRING2` from `@COUNTRIES` / `@ORDINAL` / the player's New World name (`ai_king.c` `AiKingAudienceFlavor`) |
| King's audience chamber | `@KINGWELCOME0` / `@AUDIENCE` menu / `@KINGBLESS` / `@KINGLAUGH` / `@KINGNO` / `@KINGFUND` / `@KINGLOWER` / `@KINGRAISE` / `@KINGNOTHING` | n/a | Cut feature: the chamber is `FUN_38fd_462e`, which nothing calls in `viceroy_unpacked.asm` or `viceroy_overlays.asm`, and its menu text (`@AUDIENCE` `0x10d4`) and refusal (`@KINGGOAWAY` `0x10c9`) were removed from the shipped `GAME.TXT`. Its five actions (`38fd:44a4`, `38fd:4590`, three thunks) have no other caller either |
| Merc Hire/Decline (`@MERCENARIES`) | Continental mercs | Done | `KING_MERC` |
| Declare (`@DECLARE`) | Independence confirm | Done | `KING_CONGRESS` body+choices via `popup_msg_fill` / `popup_msg_choices`; VGA PARKED |
| `@INDEPENDENCE` letter | Rename / letter | Done | `KING_LETTER` from `@INDEPENDENCE`; signing cinematic (`declaration.c`) |
| `@INVASION` / intervene | REF / ally arrival | Done thin | REF `@INVASION`; ally `@INTERVENTION`+`@INTERVENE` |
| Crown capture | REF takes colony | Done thin | `@CAPTURED3` via `units_combat_notify_colony_captured` (`COMBAT_COLONY`; the reserved `KING_CAPTURE` tag has no producer) |
| Revolution win/lose | End WoI | Done thin | `@WINNING` / `@LOSING1` / `@LOSING2` / `@RETIRING2` via `popup_msg_fill`; latches; VGA PARKED |
| Mid-war warn | WoI | Done | ONE `@WARN%d` per turn from DOS's digit-patch selector (raw 58506-58534): colonies<3 → `@WARN2` > share ≥80% → `@WARN3` > ports<3 → `@WARN1`; skipped on a turn that wins or loses the war; port-side episode latches `market_demand_pool_raw[6]/[7]/[10]`. `@LOSING3` takes over at share ≥90% |
| `@CONTINENTAL` FF elect | Founding Father debate | Done | `FF_CONGRESS` |
| 1800 peacetime end | Auto-end | Done thin | `@SCORED` CHOICE; That's all → `@RETIRING` + retire score; latch `market_demand_pool_raw[4]=3` |
| Anniversary soon-retire | Calendar | Done thin | `@SOONRETIRING0` Spring 1790 peacetime; `@SOONRETIRING1` 1840 WoI |

### 9. Combat / loot / capture

Deep mechanics: [combat.md](combat.md).

| Popup / `@SECTION`s | When | Status | Port |
|---------------------|------|--------|------|
| Combat Analysis (options bit) | After strengths, before roll; human side | Done | DOS `636c` layout: 214-wide frame, per-column header (chrome + type name + baseline right-aligned), label/±N% split rows at 20px pitch; gated by `combat_analysis` |
| `@LOOT*` / `@LOOTCAPTURE` / `@LOOTCASH` | Combat loot / treasure capture | Done | `@LOOT` treasure + `@LOOT2` burn Done; `@LOOTCAPTURE` treasure capture-alive (`FUN_5fef_0352` raw 99392-99413) Done; `@LOOTCASH` Europe fleet cash-in (`units_king_galleon_credit` / `europe_cash_treasure_passengers`) Done — see section 9 |
| `@CAPTURED*` / `@BURNED*` / `@SHIPDAMAGE` / `@SHIPSUNK` | Capture / burn / naval | Done (`@BURNED*` Missing) | Colony `@CAPTURED*`; ship damage/sunk. `@BURNED` / `2` / `3` (the Euro-vs-Euro burn arm of `FUN_5fef_1b0e`) are never raised: no port path (known_divergences.md #1082) |
| `@COLONISTCAPTURE*` / `@WAGONCAPTURE` / `@CARGOCAPTURE` | Unit / wagon capture | Done | Structural combat popups; treasure uses `@LOOTCAPTURE` |
| `@EUROPEWIN` / `@EUROPELOSE` | Euro combat outcome | Done | `{atk} defeat {def nation unit} near {place}!` / reverse; LABELS defeat/defeats |
| `@DEMOTE` | Specialty strip / demote | Done | `@DEMOTE` with nation/unit/status tokens |
| `@SEIZURE*` | Privateer / seizure | Done | Privateer custom body; Crown `@SEIZURESEA` Royal Navy |
| Ambush WIN/LOSE (`@INDIANWIN*`) | Indian ambush | Done | `{tribe} ambush {nation unit} near {place}!` (+ seized by tribe braves); LOSE `{nation unit} {defeat} {tribe} near {place}!` |

### 10. Year-end / victory / HoF / retire

| Popup / `@SECTION`s | When | Status | Port |
|---------------------|------|--------|------|
| `@LOSENOCOLONIES` / defeat | No colonies | Done thin | DOS `3844_0442` Section B (year≥1600, peacetime, zero human colonies): `@LOSENOCOLONIES` OK (`%STRING0` difficulty title, `%STRING1` leader name) then `AI_KING_ENDGAME_LOST` → retire-score/HoF chain (`turn_run_year_end_chrome`) |
| `@SCORE` / `@SCORED` / retire | Retire / F10 | Done thin | `@SCORED` + `@RETIRING` on peacetime 1800 That's all; F10 score; HoF stub `HOF.TXT` |
| `@LOSING*` / `@WARN*` / `@WINNING` | WoI end / anniversary | Done thin | `@WINNING`/`@LOSING1`–`3`/`@RETIRING`/`@RETIRING2`/`@WARN1`–`3`/`@SOONRETIRING0`/`1` Done thin |
| `@TIMECHANGE` | Calendar help | Done thin | Fires once at year 1600 / season 0 (`FUN_130d_0290`, asm-recovered `LEA BX,[0x141]`) via `popup_chrome_ok` in `turn_processor_advance` |
| 1850 WoI win | Year≥1850 + no crown | Done thin | `@WINNING` latch + INFO |
| 1850 WoI stalemate | Year≥1850 + crown alive | Done thin | `@RETIRING2` latch lost + INFO |

See also [sons_of_liberty.md](sons_of_liberty.md),
[`year_end_chrome.md`](../original_sources_annotated/turn/year_end_chrome.md).

### 10a. Game Options

The `GAMEOPTIONS` dialog persists all eight Col1 flags. End of Turn,
Autosave, and Combat Analysis are active. Show Indian Moves and Show
Foreign Moves now watch nearby AI tile steps during interactive end-turn
processing; Fast Piece Slide shortens the tile-step interval. Water Color
Cycling and Tutorial Hints remain persisted flags only until final polish.

### 11. Discovery / tutorial / woodcuts

| Popup / `@SECTION`s | When | Status | Port |
|---------------------|------|--------|------|
| `@LOSTCITY1`…`9`, `@BURIAL*`, `@SCREWED` | Lost City / ruins | Done thin | `units_resolve_lcr_rumour`; @LOSTCITY4 Search/Stay-clear CHOICE live for the human (`AI_POPUP_TAG_LCR_BURIAL`, port_bugs.md #956) and the payout block runs from its result; native-attack combat resolve on `@SCREWED` PARKED (50/50 despawn stand-in) |
| `@TUTORIAL1`…`19`, `@TUT*` | Tutorial hints option | Partial | Wired: `@TUTORIAL5` (first dock immigrant, `turn_colony.c`), `@TUTORIAL6` (first cargo-ready, `turn_production.c`), `@TUTORIAL19` (first Convert selected, `game_loop_orders.c`), `@TUTNOLUMBER` / `@TUTNOSPACES` (Build Colony, `game_dialogs.c`); the other `@TUTORIAL*` are Missing |
| `WOODCUT.TXT` captions | Discovery woodcut scenes | Done | [`woodcut.c`](../src/core/woodcut.c) — `FUN_12fd_006c` once-only gate (DS:`0x540a` = `event` + `unknown05`) + tune table, `FUN_6f30_0062` presenter (WOODFRAM.SS frame, WDCUT`nn`.SS art, NAMEPLAT.SS 3-piece plate, FONT-NP.FF caption). Wired ids: 1 new world (land sighted, `FUN_4720_049e`), 2 building a colony (`479b:0950`), 3/4/5 natives/Aztec/Inca (`5bfb:038a`, by tribe tech class), 6 Pacific (`13f1:0280`), 7 entering a village (`4d56:478a`), 8 Fountain of Youth (`65dd:04a9`), 9 cargo from the New World (`48d3:08bf`, a ship docks in Europe with goods), 10 meeting fellow Europeans (`5bfb:15cf`, MET bit still clear), 11 colony burning + 13 Indian raid (`5fef`). Id 12 COLONY DESTROYED has art but **no** call site in the shipped EXE; ids 14-16 have captions but no art; 17-25 are demo-autoplay only |

### 12. Cheats (`DEBUG.TXT`)

| Popup | When | Status | Port |
|-------|------|--------|------|
| `@SETVIEW` Reveal Map | DEBUG → Reveal | Done (Esc resets; 5 rows unless a view is active) | [`cheat_list_dialog.c`](../src/core/cheat_list_dialog.c) |
| Kill Indians tribe list | CHEAT Shift-F6 | Done (non-extinct tribes only, DOS case 0x67; port_bugs.md #1173) | Same + `units_destroy_tribe_nation_w` |
| `@SETHUMAN` Set Human | CHEAT Shift-F5 | Done; "None" (`@SETAUTO`) parked | `game_cheat_set_human` (`game_loop_saveload.c`) |
| `@FORCED` | CHEAT Shift-F7, last stage | Done | `game_cheat_advance_revolution` (`game_loop_menus.c`) |
| `@CREATE` / other DEBUG | Other cheats | Create Unit done (Indian rows 9-12 own the nearest village's tribe); the rest per docs/assets.md CHEAT | |
| Colony-screen cheat keys (T S `$` `%` `^` `!` Space) | CHEAT bit set, bare colony screen | Done, no popup of their own (`FUN_2f2b_6372` tail, port_bugs.md #1173; Space's tick may queue the usual production chrome); docs/colony.md "Colony-screen cheat keys" | `game_colony_cheat_key` / `game_colony_cheat_produce` (`game_loop_colony.c`) |

DEBUG sections: `MOTD`, `MOTD2`, `MEMORY`, `CREATE`, `CREATE2`, `CSHIP`,
`FOREIGN`, `FOREIGN2`, `SETVIEW`, `SETHUMAN`, `SETAUTO`, `SETREPORT`,
`SETEUROPE`, `DANGER`, `BADGUYS`, `SOUND`, `OPTIONS`, `FORCED`, `TEST`, `END`.

### 13. Trade routes

DOS module = segment `647e` (mislabelled "colony" in the catalog — records are
0x4a routes, not 0xca colonies); create wizard = OVL19_L0040; Begin =
`FUN_2b5a_1e66`; stop service = `FUN_479b_0bd0`.

| Popup / `@SECTION`s | When | Status | Port |
|---------------------|------|--------|------|
| `@TRADEMANY` | Create at 12-route cap | Done | ai_popup OK, %NUMBER0=12 |
| `@TRADESTART` / `@SAILPORT` / `@TRAVELPLACE` | Create / editor destination pickers | Done | `game_trade_open_dest_picker` → `game_open_dest_pager` (TRADE_DEST, paged as Go to Port, port_bugs.md #1194). Wizard pickers: `@TRADESTART` %NUMBER0 = 1 / 2, every own colony, no Europe row (DOS asks `FUN_647e_01c6` with unit -1). Editor stop rows: titled by the phantom unit `FUN_478c_007e` (type 0xd sea / 0 land), so `@SAILPORT` / `@TRAVELPLACE` (static read of 647e:0227-025c, no DOS shot); Europe first (sea route), then colonies reachable from the route's first stop (`travel_dest_ok`); editing preselects the stop's destination, appending stop 0's; "(Delete Destination)" on the last page when editing a stop of a multi-stop route, and always when appending (picking it there does nothing, 647e:0e55) |
| `@TRADETYPE` | Create: first colony coastal | Done | AI_POPUP_TAG_TRADE_TYPE CHOICE (1=Sea) |
| `@TRADENAMES` + `@TRADENAME` | Create: default name + entry | Done | "<Colony> <word>", the word = `FUN_281f_04ca` reseed + `FUN_281f_04d4(1, n)` draw on the game RNG (`move_rng`; no private LCG), " A"-suffix dedupe; name_entry TRADE_NAME (31 chars). Regression tests type names, never rely on the default |
| `@TRADENONE` / `@TRADENONE2` | Begin/Edit with no (matching) routes | Done | ai_popup OK; %STRING0 = @ROUTE Sea/Land |
| `@TRADESELECT` route picker | Begin (sea/land filtered) / Edit | Done | `cheat_list` TRADE_SELECT, "N. NAME" rows |
| `@SAILPORT` / `@TRAVELPLACE` | Begin: multi-stop route starting-stop picker | Done | `game_trade_open_stop_picker` (`game_loop_menus.c`), title by `r->sea` (DOS `FUN_647e_090a`), "N. stop" rows, preselect = unit's current stop or 0 |
| `@CARGOLOAD` / `@CARGOUNLOAD` | Editor cargo append | Done | `cheat_list` TRADE_CARGO_ONE, %STRING0 = stop; 190px frame fits the prompt |
| `@TRADEDELETE` / `@SUREDELETE` | Delete confirm | Done | Route picker + Yes/No; DOS unit fixup + array compaction |
| `@ROUTELOOP` | Route with one distinct port | Done | ai_popup OK at stop service; unit parked |
| VGA TRADE chrome | EDIT TRADE ROUTE screen | Done | [`trade_screen.c`](../src/core/trade_screen.c) — `647e_09da` layout (LABELS `@ROUTE` title and table headers, 4 stop rows y 61+20i, unload x 125 / load x 208, ICONS.SS #22+ icons), `1064`/`10d2` click zones (rename band, dest/cargo columns, exit ≥ y 169), "(Delete Destination)" row |

---

## Section Index

Detailed popup catalog, with exhaustive `@SECTION` reference and DOS site citations: **See [popups_catalog.md](popups_catalog.md)**
