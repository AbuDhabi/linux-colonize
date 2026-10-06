# Seed-100 idle campaign gate (`golden_idle_campaign`)

STATUS: working doc, opened 2026-10-06. Gate and harness: [tests/README.md](../tests/README.md).
Fixtures: `original_saves/1492-1600-seed-100/year_*.sav` (VR_SEED.EXE autosaves, human idle in
View mode, never moves).

## How to measure

Each transition is one `turn_end` from DOS save Y compared with DOS save Y+1, so years are
independent. Field-level diff (byte offsets are useless here):

```
GOLDEN_IDLE_DUMP=/tmp/x/sim.sav ./build/debug/golden_idle_campaign Y Y+1
./build/debug/sav_json /tmp/x/sim.sav /tmp/x/sim.json      # explicit output path!
./build/debug/sav_json original_saves/1492-1600-seed-100/year_<Y+1>.sav /tmp/x/dos.json
```

then a recursive JSON diff (show `*_hex` blobs as differing byte offsets). The target is
`EXCLUDE_FROM_ALL`: rebuild it explicitly (`cmake --build build/debug --target golden_idle_campaign`).

Progress (sum of differing JSON leaves over all 77 transitions): 18,869 → 16,546.
1493→1494 is down to the tile-chain order below; 1492→1493 to the human's first-turn UI.

## What the autosave is (DOS year loop, raw 6330-6470)

Day top: `FUN_281f_0550`, `+0x3149 = 0` for every unit (spent MP), `0676` Indian mid-pass.
Then slot by slot: `0644` (= `FUN_3844_00f2` EOT, ending in 06ba lanes + `4962_0018` census),
for the human `FUN_130d_0172` autosave **before** Move Pieces (`FUN_2b5a_3b68`, which sets
`DS:0x5392 = 0xffff`). The calendar step is at the loop **end**. So with human slot 0 the
autosave holds: AI slots of year Y (old date), day top, human EOT of Y+1, nothing of the
human's Move Pieces. Head UI words at that point: `map_modal_active 0`, `no_unit_selected 0`,
`map_mode` = whatever the player left, `active_unit` = last AI hull that sailed for Europe
(raw 76484/77099 set 0x5392 before `291f_0208`), else 0xffff.

## Fixed in the 2026-10-06 pass

- Europe arrivals: `FUN_48d3_06ba` tail (064e→048e) places hulls in the nation's own lane tick,
  before 0a60; the fitted "first leg + west-explore (4,13)" exit course is deleted (the (4,13)
  goto is the 20e6 far roam, which now also writes plan `'D'`, 521d:4e86). 048e's ±e pair loop:
  the +e hit wins.
- Day-top spent clear moved to the Indian mid-pass entry and now restores runtime MP; the
  calendar advance moved there too (slots above the human act under the old turn; market
  ticks use each slot's own turn).
- AI landers reveal on landing (465b_0000 07a0); first contact stamps `contact_state = 2`
  (raw 96669); the invented WELCOME land grant (purchased bit) is deleted; AI moves no longer
  set `named_new_world` (bugs.md #1057a reversed: 049e is the human UI mover).
- `FUN_4962_0018` ported per nation (`col1_stuff_census_4962_w`), called at each nation's 00f2;
  `FUN_1427_0824` / `FUN_364b_1ba8` / colony delete keep `all_unit_counts` / `colony_counts`
  live in between.
- Colony founding mints its COL1 record (1ba8): custom-house export bits, `visible_to_euro`
  all 1, founder's +100 `rebel_divisor`; `last_colony_founded_turn` is the nation record field.
- Capture keeps DOS's stale hold bytes past `holds_occupied` (DOS remover `FUN_15eb_317c`
  never clears); autosaves stamp the 130d_0172 head state; Move Pieces entry resets 0x5392.

## Open leads (most transitions first)

- **AI colony tick vs DOS**: colony worker/tile choice, `specialty_cargo`, `building_in_production`
  255, AI-colony custom-house bits cleared one turn after founding (no static writer of
  colony+0x8a found besides 1ba8 and the human dialog — needs a DOSBox watch).
  Downstream: purchased bit on worked tiles, alarm/friction, recruit pool RNG phase.
- **Tile chain order** (`transport_chain`): after a LAB_3558 unload DOS chains the remaining
  passenger before the hull (4→3); capture emits hull first.
- **Native unit `vis_mask`** stays 0 in the port where DOS sets the seeing Euro bit.
- **Land-unit hold bytes** (`cargo_hold[0]` 1/2 on soldiers/pioneers) and plan codes
  `'9'` vs `'?'` after landing.
- `nations[].indian_hostility_sticky` is a port stand-in stored in DOS-dead byte +0x4b;
  DOS saves always carry 0 there. Needs a port-only home (COLNXEXT) before the byte can be 0.
- 1492→1493 only: `tut2.nr1`, `rival_nation_slot_2`, `stuff.x/y`, `map_mode` come from the
  human's first Move Pieces (tutorial popup, View key); not reachable headless.
