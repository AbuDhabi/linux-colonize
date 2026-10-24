# Founding Fathers doc history (archived 2026-10-24)

STATUS: archived 2026-10-24 from docs/founding_fathers.md — history, not a spec.
Live facts: docs/founding_fathers.md (per-FF rows, Congress debate widget) and
docs/popups_dialog_engine.md ("6f74 box fields, row pitch and selection bar").

## Open items found while writing this table

P9.2 closed 2026-08-28. Items below are resolved or parked elsewhere; none
are open Father-effect work.

1. **Magellan is wired in 2 places, not "turn.c only"** as an early P9 draft
   stated: `turn.c` `turn_refresh_moves_for_nation` (ongoing per-turn +1)
   **and** `col1_bridge.c` ~991 (Col1 save-load path re-derives ship `moves`
   from raw `moves` with the same +1). **2026-09-17 correction:** there is
   no elect-time one-shot bump any more — `FUN_4345_0342` has no
   `param_2 == 5` case, so that third site was removed (port_bugs.md #476); the
   two ongoing-refresh sites are the whole story.
2. **Corrected 2026-09-17 (port_bugs.md #476).** The per-tick re-sweep added
   2026-08-26 had no DOS site and is removed. La Salle's ongoing coverage
   is the synchronous admit-time/birth-time hook (`founding_fathers_la_salle_check`,
   DOS raw ~11312) plus the elect-time sweep — not a per-turn pool scan.
3. **Resolved 2026-10-18 (port_bugs.md #1105).** Drake lives in one place:
   `combat_unit_base_x8` (combat_strength.c, raw defense → ×8 → veteran →
   Drake → holds penalty). `units_fort_vs_ship` (units_combat_resolve.c,
   FUN_5fef_1b0e on 03f6's temporary attacker) takes the defending ship's
   strength from `combat_unit_base_x8` too, so it gets Drake there. The
   old separate `units_drake_scale_strength` had no caller and was deleted.
4. **Confirmed 2026-08-26 — not a bug, matches DOS.** Read
   `FUN_4345_0342` (`viceroy_unpacked.c` ~73044, `param_2==0xe` branch,
   the FF-apply dispatch's Jones case) directly: it calls
   `FUN_281f_095c(0x11, param_1, …)` (spawn Frigate, type `0x11`) with no
   `param_1==0`/human-only guard anywhere in that branch or the
   surrounding function — same shared dispatch every other FF case uses,
   unconditional on which nation is electing. DOS itself grants Jones's
   free Frigate to whichever nation (human or AI) elects him; the port's
   `effect_jones_frigate` matches this exactly. P9.2's "for the player"
   task wording was simply imprecise, not a spec the code was failing to
   meet — no fix needed.
5. **Fixed 2026-08-28.** Brewster's pick-among-pool was ported from the
   `5e52` Brewster branch (`FUN_38fd_4884(0,1)` / `@RECRUITCHOOSE`) — see
   row 20. Same pass: de Soto's ongoing sight radius (`13f1_02f8`),
   Magellan's voyage roll (`48d3_0002`, plus a polarity bug in the
   immigrant-ship landfall) — rows 5 and 7.
6. **Stuyvesant's Custom House per-cargo toggle** is **Done**
   (`colony_screen_open_custom_house` / `colonies_toggle_custom_house_cargo`).
   Auto-sell itself (stock>99→leave 50 + denylist) is wired. VGA chrome of
   that checklist is D4 if it still looks off.

## Cross-check against port_plan.md P9's own claims

- **Wired list** (Bolivar, Brebeuf, Brewster, Cortes, de Soto, de Witt,
  Drake, Franklin, Hudson, Jefferson, Paine, Penn, Pocahontas, Revere,
  Sepulveda, Washington, Las Casas) — **verified accurate** against code;
  all 17 have a real gate + effect site as tabulated above (de Soto sight +
  LCR and Brewster pick-among-pool are **Done**, 2026-08-28 — see their rows).
- **"Not found outside core files" list** (Fugger, Coronado, La Salle,
  Magellan (turn.c only), Jones (ai_ only)) — **partially confirmed,
  partially corrected**:
  - (2026-08-28: de Soto and Magellan now also have `FF_*` references
    outside the core files — `units_sight_radius` in `units.c`,
    `game_voyage_turns` in `game_loop.c` / `turn.c`.)
  - Fugger, Coronado, La Salle, Jones: confirmed — `grep` for their
    `FF_*` symbols outside `founding_fathers.c`/`founding_fathers.h`
    returns nothing (Jones has no `ai_*` reference at all, correcting
    the parenthetical "ai_ only" — there is no such reference anywhere
    outside `founding_fathers.c`). This is expected, not a gap: all four
    are one-shot elect-time effects with no ongoing per-turn gate needed.
  - Magellan: **not** "turn.c only" — see Open item 1 above.

## Congress debate widget — `FUN_4345_06d2` (T5.1 slice, 2026-09-07)

The "which Founding Father shall we appoint" picker is not bespoke chrome:
`FUN_4345_06d2` drives the shared `6f74` dialog engine (the same one behind
the Save/Load dialog), so only its **script + row format + right-click arm**
were missing from the port. Recovered from `OVL07_L0040` raw bytes
(`tools/rtlink_overlay_extract.py` → `ndisasm -b16 -o 0x400`,
`FUN_4345_06d2` = overlay `0xad2`) because Ghidra drops every one of this
function's register/immediate arguments.

- **Script (0xc2b–0xc35):** `FUN_291f_0182` (→ `FUN_6f74_32a4`) is called with
  `AX = DS:0x1262 = "WHICHFREEDOM"`, `BX = DS:0x087c = "GAME"`, `DX = 0` —
  i.e. `GAME.TXT` `@WHICHFREEDOM`, whose only directive is `@width=190`.
  The port already fills the body through `popup_msg_fill("WHICHFREEDOM", …)`;
  its hardcoded fallback now matches the asset verbatim.
- **Row format (0xc72–0xcfb):** per category with a candidate,
  `281f_016e` @FATHERS name → `281f_0178` (`DS:0x50` = `" "`) →
  `281f_011e` (`DS:0x5e` = `"("`) → `281f_016e` `DS:0x96e8[type]`
  (`NAMES.TXT` `@FOUNDING` word) → `281f_0178` → `281f_016e` `DS:0x2e88`
  (`LABELS.TXT` `@MISC` 103 = `"Adviser"`, pointer base `0x2dba`) →
  `281f_0128` (`DS:0x60` = `")"`). So each row reads
  **`Adam Smith (Trade Adviser)`**, not a bare name. Live via
  `reports_ff_category_display_name` / `reports_misc_display_word(103, …)`.
- **Widen-to-widest-row:** those rows are wider than `@width=190`, and
  `FUN_6f74_14c6` grows the box to the widest emitted option. `ai_popup_render`
  now applies that rule (it was already in `save_load_dialog.c`); it is a
  no-op for every dialog whose rows already fit, so no golden moved.
- **Right-click = explain the row:** `06d2` sets `DS:0x1f66 = 1` before the
  modal loop, so `FUN_6f74_2580`'s right-button arm returns the hovered row
  with `DS:0x1f68 = 1`; `06d2` then calls `FUN_2a1f_0062` → `FUN_6cb2_1f28`
  (the Colonizopedia founding-father article) and **loops back to rebuild the
  same dialog**. Wired in `game_loop.c` alongside the port's existing F1
  keyboard equivalent.
- **Not cancellable:** a result `≤ 0` jumps back to the rebuild label (0xc2b)
  with the same slate — matching the port's existing "escape re-presents the
  identical slate" behaviour.
- `FUN_281f_04ac(3)` at 0xc21 is **not** portrait art: the catalog resolves it
  to `FUN_129f_0318`, "set BGM track id + gate play". port_plan T5.1's
  "chief portrait `FUN_281f_04ac`" is a misattribution — the chief portrait
  path is `FUN_6f74_0042` / `FUN_6f74_14c6` (already cited in `ai_popup.c`).
- **`DS:0x1f66` height bump — RESOLVED, ported (port_bugs.md #1192).** `+0x14` is the
  box **width** and `+0x16` the **height**, pinned at `FUN_6f74_14c6` OVL24
  `0x16cb`–`0x16f6`: auto-placement computes `160 − box[0x14]/2` into `+0x10`
  and `100 − box[0x16]/2` into `+0x12`, then clamps `+0x14 + 0x10` against
  `0x140` (320) and `+0x16 + 0x12` against `0xc8` (200). So the `0x1f66` block
  at `0x169d`–`0x16c3` grows the **height** — the earlier "axis ambiguous"
  note is withdrawn. The `+6` arm needs `box[+0x80]/[+0x82] == DS:0x89e/0x8a0`
  (FONTTINY); `@WHICHFREEDOM` declares no `@SMALLFONT`, and the box font is the
  `FUN_6f74_06d0` text record `box+0x74` (font at `+0x80`) filled from the
  parser's `FUN_291f_023c(DS:0x1fa2, DS:0x1f9e, DS:0x1fa0)` dialog-font slot,
  which holds FONTINTR (`DS:0x268a`) outside the FONTTINY swaps of
  `FUN_7562_0052` / `FUN_647e_0e80`. So the Congress takes `+3`, and
  `FUN_6f74_1a78` draws LABELS `@MISC` 189 bottom-right (`ai_popup_render`,
  docs/popups_dialog_engine.md "Sizes of the remaining compositor dialogs").
- `box[+0x22] = 8` (default 4, `FUN_6f74_06d0` `0x075c`–`0x0763`) shifts only
  the option-row **text** start: `FUN_6f74_1b7c` builds `box[+0x24] +
  box[+0x48] + box[+0x22]` as the label x, and the selection bar subtracts
  `box[+0x22]` straight back off, so the bar is unaffected. Ported
  (port_bugs.md #1189): the Europe list shots confirm `frame_x + 5 + box[+0x22]`,
  so the Congress row labels sit at `inner_x + pad_x + 8`.
- No golden covers the dialog — the repo's goldens are all simulation-state,
  not pixels. Capturing one would mean a `render_*_main.c`-style harness that
  opens `AI_POPUP_TAG_FF_CONGRESS` on a fixture save and dumps the
  framebuffer, the way `tools/render_report_main.c` does for F2–F10.

### `6f74` row-pitch and selection-bar audit (2026-09-07, handoff items)

Three claims handed over from the wood-frame slice, checked against
`ndisasm` of OVL24 (`seg_24_load0000.bin`, origin 0 — offsets are the 6f74
segment offsets directly):

1. **Two row pitches — CONFIRMED and ported.** `FUN_6f74_0f16` returns
   `font[0]` (6 → 5 unless `DS:0x1f8a`), i.e. the port's `glyph_h`. Then:
   *option* rows (`+0x54` list) advance by `glyph_h + box[+0x46]`
   (`0x1611`–`0x1628`), *edit-field* rows (`+0x60` list) by
   `glyph_h + box[+0x46] + 5` (`0x1649`–`0x1653`), and *prompt/body* lines by
   `glyph_h + 1` (`FUN_6f74_1198` `0x1234`/`0x124c`, `call 0xf16` + `inc ax`).
   `box[+0x46] = (flags & 0x10) ? 0 : 3` (`FUN_6f74_06d0` `0x078a`–`0x0799`),
   so framed dialogs get **glyph_h + 3** and **glyph_h + 8**. `ai_popup_render`
   used `glyph_h + 1` for options too — every option row sat 2 px tight; fixed,
   with hit-testing (`st->line_h`) moved onto the option pitch. Body lines were
   already right.
2. **Per-row button chrome — REFUTED for this widget.** The outlined,
   wood-filled per-row box in `FUN_6f74_1e14` is drawn only for the `+0x60`
   list, and that list is a **text-entry field** column (label + buffered
   input, box height `glyph_h + 5` at `0x1ee7`), not option buttons — hence
   the catalog's "Paint edit-field column". Rows reach it only through
   `FUN_6f74_0d44` (`thunk_FUN_2a1f_0ace`), whose sole caller is the script
   parser's `DS:0x2008 != 0` arm (`FUN_6f74_32a4`, decomp ~117418); the
   `DS:0x2008 == 0` arm calls `FUN_291f_0176` → `FUN_6f74_0a00`, which appends
   to `+0x54` (`0x0a08`/`0x0a8c`). `FUN_291f_0176` is what every `ai_popup`
   CHOICE row — the Congress debate included — goes through, so those are
   `+0x54` rows and DOS never draws a button around them. Painting one would
   be invented chrome, so nothing was ported. What `FUN_6f74_1b7c` actually
   emits for `+0x54` is two fills through `FUN_6f74_033c`: one wood-brush
   erase over the whole option column (`box[+0x3c]/[+0x3e]`, height
   `(glyph_h + box[+0x46]) · box[+0x2]`) and one solid highlight
   (`box[+0x40]/[+0x42]`) behind the selected row — which is what
   `popup_draw`'s fill plus `ai_popup_fill_row` already produce.
3. **Selection-bar x — RESOLVED, it never leaves the frame.** `0x1b86` builds
   `[bp-0x8] = box[+0x24] + box[+0x48] + box[+0x22]`; the bar call at
   `0x1c8e`–`0x1c98` then does `ax = [bp-0x8]`, `sub ax,[es:si+0x22]`,
   `dec ax`. The two `+0x22` terms cancel **exactly**, leaving
   `x = box[+0x24] + box[+0x48] − 1` — with `box[+0x48] = 2`
   (`FUN_6f74_06d0` `0x079d`–`0x07a5`) that is `content_origin + 1`, inside
   the frame, with no dependence on the `+0x5c` header list. The "box+0x24 − 5"
   reading came from substituting the default `box[+0x22] = 4` into one term
   and not the other. Width is `box[+0x20] + 2·(1 − box[+0x48])` = content − 2
   (`0x1c81`–`0x1c9b`) and height `glyph_h + 2` (`0x1c5b`–`0x1c62`, `call
   0xf16` then two `inc ax`). Ported into `ai_popup_render`. The *y* anchoring
   followed in port_bugs.md #1189 (bar at `box[+0x26]` = `popup_dos_list_y0`,
   text one px lower, as the Europe shots show; docs/popups_dialog_engine.md).

4. **`save_load_dialog.c` — CONFIRMED `+0x54`, ported 2026-09-07.** The
   handover left this open pending a read of the bespoke `7562_0052` builder.
   Read: `FUN_7562_0052` emits each slot row with `FUN_291f_0176`
   (`viceroy_unpacked.c` 119680), and `FUN_291f_0176` is a bare thunk to
   `FUN_6f74_0a00` (`viceroy_unpacked_2.c` 33285–33289) — the `+0x54` option
   list — so the slot rows are option rows, not body lines. The box itself
   comes from `FUN_291f_0182` → `FUN_6f74_32a4` (the script parser) over
   `GAME.TXT` `@SAVEGAME` / `@LOADGAME`, which declare nothing but
   `@width=190`, so no `flags & 0x10` and `box[+0x46] = 3`. Pitch is
   `glyph_h + 3`; the port's `glyph_h + 1` was the same 2 px tight as
   `ai_popup` had been. Fixed, with hit-testing (`dlg->line_h`) moved onto the
   option pitch and the selection bar switched to the resolved `1b7c` rect
   (`x = inner_x + 1`, width `content − 2`, height `glyph_h + 2`) it had been
   approximating. Same read also re-confirms the slot counts: the two callers
   pass the row count as the second arg — `030a` → `(0x20f6, 8)` = save,
   `04e8` → `(0x211a, 10)` = load, matching the port's 8/10.
