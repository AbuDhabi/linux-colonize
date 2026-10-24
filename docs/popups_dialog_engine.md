# Dialog engine metrics (6f74 compositor)

Split out of [popups.md](popups.md) on 2026-10-23 (doc-length policy). Text
shading, wrap / width / centring, per-builder sizes, list rows and list keys of
the DOS `6f74` dialog compositor (`FUN_6f74_14c6` layout, `FUN_6f74_0538` text,
`FUN_6f74_2580` input) and how the port matches them. The base `FUN_6f74_14c6`
size formula, the Europe menus and the figure placement stay in
[popups.md](popups.md#europe-in-screen-menus--the-king-helper);
the popup inventory is [popups.md](popups.md), the exhaustive `@SECTION`
reference [popups_catalog.md](popups_catalog.md).

Dialog text shading (port_bugs.md #1191). DOS has no drop-shadow pass. `FUN_6f74_0538`
(Ghidra shows its near calls as `CALL DATA:FUN_7b29_49e8`; callers `FUN_6f74_116c` prose,
`1b7c` option rows and the text after `|`, `1e14` edit field + `_` caret, `201e` icon rows)
loads a 4-entry map via `FUN_6f74_04f6` -> `FUN_1c28_000a` into `DS:0x269e`: shade 0 skip,
1 ink (style+2; style+6 inside `{}`; style+4 when caller BX&1, the disabled rows; `~`
flips the next char between +6 and +2), 2 style+8, 3 style+0xa. `FUN_1c11_000c` blits each
2-bpp glyph once through it. The style (box+0x74) is copied at box init from
`DS:0x1f4a..0x1f52`, which `FUN_75c2_0204` loads from `@COLORS` 68 / 8 / 149 / 128 / 47, so
shade 2 = 128 (121,73,52) and shade 3 = 47 (12,12,12); the title-menu set `FUN_75c2_024c`
has shade 3 = 0. FONTINTR / FONT-NP carry shades 2 and 3, FONTTINY / FONTSMAL only shade
1 (no edges), FONTKING draws its body in shade 3. Port: `popup_draw_text_markup` with
`ColonizePopupColors.enhance` / `.shadow` (remapped by RGB with the rest of the title set,
shadow forced to 0); `text_edit_render` the same map. Test: `unit_popup_prompt
dialog_text_is_dos_shaded`. `~` (6f74:05be, port_bugs.md #1199): the map is reloaded with
DX = (0x1f62 == 0), the next byte drawn as a glyph whatever it is, the map restored, so the
character after `~` is 149 outside `{}`, 68 inside, grey on a disabled row (callers pass grey
as both inks); the measure `FUN_6f74_0486` deletes every `~{}|` first
(`popup_markup_text_width`). Test: `unit_popup_prompt dialog_tilde_is_dos`.
A char whose face width byte is 0 draws nothing and advances 0 (`FUN_1c11_000c` raw
17468-17471, port_bugs.md #1225): FONTTINY has no `*<=>@\^_~{|}` or backquote, FONTINTR no
`\^{|}~` or backquote, so the
jobs rows' DS:0xca8 ` = ` reads as two spaces. The compositor (`popup_draw_text_markup`,
`popup_markup_text_width`) drops those chars; `font.c`'s 5x7 fallback glyph is for the
port's other text only. Test: `unit_popup_prompt dialog_lacking_glyph_is_dos`.

Screens that are not compositor dialogs write through the resident `FUN_104b_*` writers
instead: `FUN_281f_013c` / `0150` / `0100` -> `FUN_104b_0288` / `02c2` (right-aligned) /
`0318` (centred), FONTTINY (DS:0x89e), the map `{skip, c, c, c}` with the caller's ink, one
blit, no shadow (the FONTINTR family `035c` / `039a` / `03d2` / `0430` maps
`{skip, c1, c2, 0}`). Combat Analysis (`FUN_636c_0000`): title, names and labels @COLORS
basic (DS:0x830), values and base strengths hilite (DS:0x831). Trade route
(`FUN_647e_09da`): ink literal 0x0f on a 0x22 fill, grid and OK outline colour 0, the DOS
positions in `trade_screen.c`. Port: `font_draw_text_shaded` with `{0, c, c, c}`
(port_bugs.md #1201). Test: `unit_popup_prompt stat_screens_text_is_one_pass`.

New-game wizard (port_bugs.md #1198, `new_game.c`). Difficulty / nation / customize
(`FUN_733a_06a4` / `0b3e` / `01a4`) draw their titles through `FUN_104b_0430`
(FONTINTR `{skip, 0xfe, 0xfd, 0}`) and the finished line through `0318` (FONTTINY 0xfe);
the selected cell's two lines (`FUN_733a_0512` / `09c6` / `002c`) are each drawn twice,
ink 0 at x + 1 then the cell ink at x (a real DOS pass, not a shadow); cell inks are
10/9/14/13/12, the @COUNTRY colour column, and 0x0a / 0x0e. @AMERICA, the map picker,
@LEADERNAME and the @NATION lore use the @COLORS map; @AMERICA and the picker show over the
game palette (WOODPANL family) with the WOODTILE frame, settled by a DOSBox-X capture
(every visible colour is WOODPANL.PIK's, the frame is WOODTILE.SS through it, not
OPENMENU / OPENTILE). The king's audience (`FUN_75c2_20e2`, also the war-end throne)
swaps the style to ink 0xf2, enhance 0x2f, shadow 0; the sail captions (`FUN_1262_0152`)
to ink 0x0e, enhance 0x36, shadow 47. Difficulty, nation, customize and @BUILD1 match the
capture pixel for pixel. Tests: `unit_new_game wizard_picture_text_is_dos`,
`wizard_prose_text_is_one_pass`.

Wizard dialog layout (port_bugs.md #1210, `new_game_render_dialog`). @AMERICA (`FUN_281f_03fe`),
the map picker (`FUN_75c2_0000`: GAME @MAPTOLOAD + one row per `*.MP`), @NATIONnA/B and the
king's @VICEROY are `FUN_6f74_14c6` boxes parsed like `FUN_6f74_32a4` (prose until a blank or
@OPTIONS, `_` read as a space by `FUN_7314_0106`, @WIDTH taken as given, no 0x50 floor once set):
content = max(@width, widest row + 8), prose wrapped to content - 2 * margin by
`popup_wrap_text`, `^^` rows at + (wrap_w >> 1) - (width >> 1) (`popup_dos_center_dx`,
6f74:1263), h = sp + 2 * frame + prose + rows * (glyph_h + sp) + sp. @LEADERNAME, the lore pages
and the king's audience are frameless: `FUN_281f_040a` -> `FUN_6f74_37f6` sets DS:0x1f56 |= 0x18
before the box is built, so `FUN_6f74_06d0` zeroes box[+0x46] / [+0x48] and 14c6 box[+0x2a]
(no wood, margin, spacing): the lore is x 10 w 300 over WOODPANL, the leader prompt is
`popup_field_frame_frameless` (field length 0x17, label = the @OPTIONS row of underscores, so the
box starts at x 10 + 23 spaces = 79), the king text sits at @X 232 / @Y 21 in FONTKING. The king's
banners and KING1 (and the throne's KINGWIN / KINGLOSE) are `FUN_281f_02f8` -> `FUN_1c56_0004`
at 100% on the sprite's header anchor (`ss_blit_anchored`: ENGLND1 (32, 0), KING1 (0, 12)).
`FUN_6f74_1198` charges a continuation line's first word its leading space (6f74:13fd: local_8
restarts at the measured " word"), so @NATION0A breaks "...had neither" before "the";
`popup_wrap_text` does that for every dialog since port_bugs.md #1216 (next paragraph). Every wizard screen (@AMERICA, map picker, difficulty, nation, @LEADERNAME,
@NATION0A / 0B, king) matches the DOSBox-X captures outside the 13x19 mouse pointer. Test:
`unit_new_game wizard_dialog_layout_is_dos`.

In-game wrap, width and centring (port_bugs.md #1216). Every 6f74 dialog (ai_popup prose,
`popup_list_render` prompts, `popup_field_frame` name / amount prompts, the colony sub-dialogs,
the Europe menus, the wizard) wraps through `popup_wrap_text` = `FUN_6f74_1198`: a
continuation line's first word is charged its space (6f74:13fd). Width: box[+0x28] is the
section's @width as given (`FUN_6f74_0c22`, no floor) or the `FUN_6f74_06d0` default 0x50 when the
section has none (`popup_msg_section_width`; @DOS / @DOSYES / @RETIRE, which the port used to
draw 190 wide); 0 from the side-channel means no GAME.TXT section (port text, ai_popup keeps its
190). `FUN_6f74_14c6` stores max(box[+0x28], widest row) back into box[+0x28] before it calls
1198, so rows wider than @width widen the prose too (`ai_popup_render` used to wrap at @width -
4 first); a list prompt wraps instead of widening the box. `^^` rows (in-game only @TIMECHANGE)
sit at + (wrap_w >> 1) - (width >> 1). Checked against DOSBox-X captures (VR_SEED.EXE, the
`map_pioneer_native_land` / `colony_eject_siege_refused` input saves). Test:
`smoke_ai_popup case_dos_wrap_and_width`.

The same rows drive `ai_popup_render` and the `popup_list_metrics_dos6f74` list
(save/load picker) since port_bugs.md #1189: row 0's bar top is `popup_dos_list_y0`
(`frame_y + 6`, + 3 + prose_h with prose; prose = title + body for an ai_popup, the
prompt for save/load), text one px lower at `frame_x + 5 + box[+0x22]`, and the hit
tests read the same list_y0 / pitch. `box[+0x22]` is 4 except the Fountain of Youth /
Brewster picks (`FUN_38fd_4884`) and the Congress debate (`FUN_4345_06d2`), which set 8;
`FUN_6f74_0a00` widens the box to text + 4 + `box[+0x22]`. The save/load height is
`popup_dos_list_height` (12 + prompt + rows). Test: `smoke_ai_popup case_rows_are_dos`.

6f74 box fields, row pitch and selection bar (OVL24 ndisasm, offsets = 6f74 segment offsets;
port_bugs.md #1189, #1192). Box `+0x10` / `+0x12` = x / y, `+0x14` / `+0x16` = width / height
(`FUN_6f74_14c6` 0x16cb-0x16f6: x = 160 - (w >> 1), y = 100 - (h >> 1), clamped to 320 / 200).
`FUN_6f74_06d0` defaults: `box[+0x22]` 4 (0x075c), `box[+0x46]` = (flags & 0x10) ? 0 : 3
(0x078a), `box[+0x48]` 2 (0x079d). `FUN_6f74_0f16` returns `font[0]` (6 -> 5 unless
`DS:0x1f8a`) = `glyph_h`. Pitches: option rows (`+0x54` list) `glyph_h + box[+0x46]` (0x1611),
edit-field rows (`+0x60` list) `glyph_h + box[+0x46] + 5` (0x1649), prose lines `glyph_h + 1`
(`FUN_6f74_1198` 0x1234), so a framed dialog has glyph_h + 3 / + 8 / + 1. `FUN_6f74_1b7c` builds
the label x as `box[+0x24] + box[+0x48] + box[+0x22]` (0x1b86) and the bar x as that minus
`box[+0x22]` minus 1 (0x1c8e), so the bar sits at `box[+0x24] + box[+0x48] - 1` = content origin
+ 1 whatever `+0x22` is; width `box[+0x20] + 2 * (1 - box[+0x48])` = content - 2, height
`glyph_h + 2`. Its only other draws are two `FUN_6f74_033c` fills: a wood-brush erase of the
whole option column (`box[+0x3c]` / `[+0x3e]`, height `(glyph_h + box[+0x46]) * box[+0x2]`) and
the solid highlight (`box[+0x40]` / `[+0x42]`) behind the selected row. The box font is the
`FUN_6f74_06d0` text record `box+0x74` (font word at `+0x80`), taken from the parser's
dialog-font slot (`FUN_291f_023c`): FONTINTR (`DS:0x268a`) except inside the FONTTINY swaps of
`FUN_7562_0052` / `FUN_647e_0e80`; the `DS:0x1f66` footer's FONTTINY test reads
`box[+0x80]` / `[+0x82]` against `DS:0x89e` / `0x8a0`. Trap: `+0x54` and `+0x60` are different
lists. `FUN_291f_0176` -> `FUN_6f74_0a00` appends option rows to `+0x54` (every `ai_popup`
CHOICE row, the Congress debate, the `FUN_7562_0052` save / load slots: 8 / 10); only
`FUN_6f74_0d44` (the parser's `DS:0x2008 != 0` arm) adds `+0x60` text-entry fields, and those
are the only rows `FUN_6f74_1e14` boxes in an outlined wood rect (height `glyph_h + 5`, 0x1ee7).
Option rows get no per-row button chrome.

Colonizopedia article body (port_bugs.md #1222): `FUN_6cb2_03bc` makes the box frameless
(`FUN_281f_040a`) | 0x20, switches the dialog font to FONTTINY and shows the section through
`FUN_281f_0998` at y = DS:0x1f5a, so `pedia_body_render_text` is `popup_wrap_text` at @width
(300), x 160 - (w >> 1), `popup_dos_center_dx` for `^^` rows, a 7 px pitch (font[0] + 1, from
the capture); the "(name: category)" titles centre as `FUN_104b_0318`. Two caret lines in a row
no longer leave an empty row between them (`popup_msg_section_body`; @FATHER1, also @TIMECHANGE).
Matches the DOSBox-X captures of @FATHER0 / 1 / 12 / 20 outside the mouse pointer. Test:
`unit_pedia case_article_body_wrap_is_dos`.

Centring users (port_bugs.md #1188). Every dialog in the port that is a 6f74
compositor dialog without `@X`/`@Y` is centred by `popup_place_dos`: `popup_center_frame`
(save/load `FUN_7562_0052`, `@GAMEOPTIONS` / `@COLONYOPTIONS` / `@SOUNDOPTIONS`
`@CHECKBOX` lists, `@PICKMUSIC`, the cheat and Go-to-Port lists `FUN_647e_01c6` & co,
and the "Units Present" picker `FUN_2b5a_1b5a` via `FUN_291f_023c`; no menu-bar
floor any more) and `colony_screen_open_dialog_frame` (construction list
`FUN_2f2b_5bd2`, jobs / leave-as `FUN_2f2b_348c`, custom house, dock orders, messages: x
and y, replacing the fixed y of 20 / 24 / 28 / 36). The parser `FUN_6f74_32a4` honours
`@X`/`@Y`; in GAME.TXT only `@BEGINMENU`, `@VICEROY` / `@VICEROY2` and the TUTORIAL sections
carry them.

Sizes of the remaining compositor dialogs (port_bugs.md #1190, #1192, #1193). Every one now
takes the `FUN_6f74_14c6` formula with its own builder's box fields:

| Dialog | DOS builder | font | content width | `box[+0x22]` | footer |
|---|---|---|---|---|---|
| save / load | `FUN_7562_0052` | FONTTINY (`FUN_2a1f_07c4(DS:0x89e)`) | `@width` 190 | 4 | no |
| game / colony / sound options | parser (`FUN_281f_03fe`) | FONTINTR | `@width` 190 | 4 | no |
| pick music | parser (`2b5a:253c`) | FONTTINY (`@smallfont`) | `@width` 220 | 4 | no |
| DEBUG.TXT cheat lists | parser | FONTINTR | `@width` or 0x50 (Create Unit rows widen it, #1221) | 4 | no |
| Go to Port, route, stop pickers | `FUN_647e_01c6` / `0796` / `090a` | FONTINTR | `@width` 190 | 10 | no |
| route cargo picker | `FUN_647e_0e80` | FONTTINY | `@width` | 10 | no |
| Units Present | `FUN_2b5a_1b5a` | FONTINTR | widest text + 26 | icon rows | no |
| construction | `FUN_2f2b_5bd2` | FONTTINY | max(0x50, rows) | 4 | yes |
| jobs / leave-as | `FUN_2f2b_348c` | FONTTINY | 0xbe = 190 | 8 | yes |
| custom house | `FUN_2f2b_4424` (`@CUSTOM`) | FONTTINY | `@width` 190 | 4 | no |
| colony dock orders | `FUN_2f2b_51ec` (`@COLONYUNIT`) | FONTINTR | `@width` 190 + 22 icon column | 4 | no |
| Fountain of Youth / Brewster / Congress | `FUN_38fd_4884` / `FUN_4345_06d2` | FONTINTR | `@width` | 8 | yes |

Widths: content = max(`box[+0x28]` (`@width`, default 0x50), the widest row as
`FUN_6f74_0a00` measures it = text + 2 * `box[+0x48]` + `box[+0x22]`, again + `box[+0x22]`
for a row with the `|` column tab), frame = content + 6. Prose wraps to content - 4. The only
`DS:0x1f66` writers in the image (asm scan) are `FUN_2f2b_348c`, `5bd2`, `FUN_38fd_41ce`,
`4884`, `4b50` and `FUN_4345_06d2`; `FUN_647e_01c6` and the Europe dock list do not set it.
The footer is `popup_dos_draw_help_footer` (`FUN_6f74_1a78`). `@checkbox` rows (options,
custom house, the DEBUG.TXT `@OPTIONS` flags) are the DOS row text: `FUN_6f74_0a00` prefixes
DS:0x1f91 `"~[ "` and `FUN_6f74_1b7c` sets its second byte to `]` (on) or `[` (off); those
two glyphs are the font's hollow / filled check circles, so the port draws them as text, in
the hilite ink through the `~`. Hotkeys: `FUN_6f74_0642` (6f74:0b72, on the raw label) stores
the char after the last `~`, upper-folded ("~F~n" gives an F-key); in `FUN_6f74_2580` a key
matching a row (6f74:2df6) selects it and, in a @checkbox box, toggles it and stays open
(6f74:2dcd), else returns it (`popup_hotkey_of`, `popup_input_dos_key`; options dialogs and
the cheat lists). Test: `smoke_popup_dialogs checkbox_hotkeys`.

Up / Down (port_bugs.md #1209): `FUN_6f74_2580`'s list arm (raw 116905-116940) moves to the
previous / next row, wraps last <-> first (box+0x70 / box+0x54) and steps past disabled rows
(option bit 0, `FUN_291f_01b6`), giving up after two wraps; its icon arm (Units Present,
raw 116787-116847) wraps the same way without disabled rows. One helper, `popup_list_key_step`
(`popup.c`), serves every 2580 list in the port: the cheat_list kinds, options, Pick Music,
`ai_popup` choice boxes, the title `@BEGINMENU`, Units Present, the Europe Recruit / Train /
Purchase / dock menus (greyed rows skipped), and the colony jobs / leave-as / dock-orders /
construction / Custom House lists (`colony_screen_list_key`: a paged jobs / construction list's
"(More)" row is a row like the others, `ColonyScreenView.more_selected`; Enter on it turns the
page; page 1 of the jobs list opens on its "(More)" row unless the occupation is listed there).
`save_load_dialog.c` keeps its own wrap; the new-game @AMERICA / map picker rows wrap too
(port_bugs.md #1212). Not wrapped: the colony message popup has no DOS builder.
Tests: `smoke_popup_dialogs list_keys_wrap`, `smoke_ai_popup case_choice_keys_wrap`,
`unit_stage_seams test_colony_list_keys_wrap`.

Greyed `ai_popup` rows (port_bugs.md #1214): `AiPopupRequest.disabled_mask` (bit i = row i, set
by `ai_popup_set_last_disabled` after the enqueue) is `FUN_291f_01b6` -> `FUN_6f74_08fa`'s option
bit 0: the row draws in `COLONIZE_COL_GREY` as both inks (the `{}` state still carries), Up / Down
step past it, a click on it does nothing (2580's press arm, raw 116747-116748). The Enter arm
(6f74:2cfc) has no greyed test, so a greyed row that starts with the bar would be returned; no
builder does that. The image has nine `FUN_291f_01b6` call sites (asm scan): the three
encroachment boxes `FUN_2b5a_123e` / `1454` / `FUN_2f2b_2f3e` (gold < price greys "offer", id 2;
all three are `game_request_indian_land_choice`, which used to drop the row), the leave-as list
`FUN_2f2b_348c`, the Europe dock / Train / Recruit / Purchase lists `FUN_38fd_3746` / `41ce` /
`4884` / `4b50` and Go to Port `FUN_647e_01c6`; the last six are not ai_popups and already grey.
Tests: `smoke_ai_popup case_choice_greyed_row`, `unit_stage_seams
test_indian_land_offer_row_greyed`.

Custom House keys (port_bugs.md #1213): `@CUSTOM` (`FUN_2f2b_4424`) is a 2580 @checkbox box
(`FUN_6f74_32a4` flags | 5). The bar opens on row 0 (`FUN_6f74_0a00`'s first row), Up / Down
wrap, Space and F1 (6f74:2cfc, raw 116852-116864) flip the barred row and leave the box up, Enter
and Esc close it, a click puts the bar on the row and flips it, and a key matching a row's hotkey
(taken from the raw @CARGO label, so none in the shipped NAMES.TXT) moves the bar there and flips
it (6f74:2df6). Every other key does nothing while the box is up. Test: `unit_stage_seams
test_colony_custom_house_keys`.

"Units Present" (`FUN_2b5a_1b5a`): `box[+0x46]` 0, `box[+0x4a]` 2, flags 0x82, one
`FUN_6f74_0f3c` icon row per unit (the first 10 on the tile chain; the rest are not listed
and cannot be picked), no prose, no option rows. Row k spans `frame_y + 5 + 16k` .. + 16, the
icon at `frame_x + 5`, the text at `frame_x + 25`, `frame_y + 3 + e + 8 - (glyph_h >> 1)`;
h = 12 + 16n, w = widest text + 32; the selected row gets a colour-15 rectangle round its
icon (`FUN_6f74_201e`) and no bar. The colony dock-orders box carries one such icon entry
(no text), which adds the 22 px icon column to its width and shifts prose and rows right.
Tests: `unit_popup_prompt units_present_is_dos`, `unit_colony_screen unit_sub_dialogs_are_dos`,
`smoke_ai_popup case_help_footer_is_dos`, `smoke_popup_dialogs save_load_is_fonttiny`.
