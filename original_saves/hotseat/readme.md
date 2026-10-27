DOS-written hot-seat save (docs/hotseat.md), made under DOSBox-X on 2026-10-26 with
VR_SOUND.EXE (tools/dosbox_trace.py).

- `hotseat_1522.sav`: Spring 1522. England (at the controls, DS:0x5394 = 0x5396 = 0) and
  France (DS:0x5398, the fixed power) have control byte 0; Spain and the Netherlands are AI.
  DS:0x5381 bit 0x80 is set.

How it was made:
1. Source: `original_saves/1492-1600-seed-100/year_1521.sav` (England human), with three
   bytes poked before the DOS load, as `@MULTI` with England and France ticked would leave
   them: France's control byte (file 0x103, DS:0x543f + 0x34) = 0, file 0x11 (DS:0x5381)
   |= 0x80, file 0x28 (DS:0x5398) = 1.
2. DOS loaded it and resumed at France, the DS:0x5398 slot. France ended its turn, Spain,
   the Dutch and the Indians moved, the calendar stepped to 1522, and slot 0 showed
   `@MULTINEXT` over a blank screen (`dos_multinext_english.png`).
3. On England's map: GAME, Save Game, slot COLONY01 (`dos_saved.png`). That file is
   `hotseat_1522.sav`.

Round trip: the port loads it at France with `@MULTINEXT` (pixel-identical to DOS's
French box except the mouse pointer), and its re-save keeps the control table, DS:0x5398
and bit 0x80. DOS loaded that re-save back as hot-seat at France
(`dos_load_port_resave.png`). Test: `unit_stage_seams` `test_hotseat_dos_save_round_trip`.
