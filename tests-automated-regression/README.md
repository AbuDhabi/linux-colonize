# Input-replay regressions

One subdirectory per test: `input.sav`, `input.jsonl`, `output.sav`.
Run by `regression_replay` (ctest); format and recording recipe in
[tests/README.md](../tests/README.md#input-replay-regressions).

Every test stays inside one turn (the harness enforces it), so AI and
end-of-turn changes never move an expected save.

The `colony_*`, `map_*` and `village_*` tests were authored from `input.steps` scripts
(`tools/regression_script.py`; each step is commented there). They start from
`original_saves/french-campaign/COLONY04.SAV` (France, 1599) re-saved once by
the port, with fixture edits through `sav_json` where a test needs them
(JSON `moves` is moves *spent*: 0 = full allotment):

| Test | Does |
|------|------|
| `colony_cargo_load_unload` | Quebec: load / unload goods between warehouse and two docked Caravels, ship-to-ship, Shift+drag partial amounts |
| `colony_unload_best_hold` | Quebec (fixture: Caravel A = Silver + Muskets, tobacco stock 90): `u` unloads the lowest-key hold (FUN_479b_0f60 mode 0) with its status line, red bar on an empty ship, the @WAREHOUSEFULL confirm (Never mind, then anyway) |
| `colony_load_best_cargo` | Quebec (fixture: Caravel A = Tobacco 60 + a free hold, ship B = empty Galleon, stock Tobacco 90 / Cotton 12 / Ore 4 / Horses 38 / Coats 7 / Muskets 60): `l` loads the highest-key cargo (FUN_479b_11a4) with its "Loading ... onto" status, tops up the partial Tobacco hold, red bar on a ship with no free hold, Muskets (key 60) before Horses (key 0), Horses as the last stock, red bar on an empty warehouse |
| `colony_building_assignment` | Quebec (+Church, Armory, Schoolhouse, Warehouse, Printing Press): drag colonists into buildings; 3-per-building cap, teacher rules, buildings that take no workers |
| `colony_tile_assignment` | Quebec: building worker to land and water tiles, tile to tile (click and drag), plot worker dragged into a building, field jobs menu, refused drops |
| `colony_remove_roles` | Montreal (+tools, muskets, horses, Church): colonists leave as Colonist, Soldier, Pioneer, Scout, Dragoon, Missionary (also dropped onto units on the fence); greyed rows; abandon prompt declined |
| `colony_fence_to_jobs` | Quebec: colonists on the colony tile join a building and a tile; the Dragoon joins (gear back to stock); Artillery cannot |
| `colony_change_construction` | Quebec (+Armory): CHANGE / `c` picker by mouse and keyboard, building, unit, cancel, (No Production) |
| `colony_buy_construction` | Quebec (hammers 20, gold 716): BUY declined, bought, a tools top-up buy, a refused buy |
| `colony_build_queue` | Quebec: Shift-click / Shift+Enter queue (port-only `BQUE` chunk), refusals, wipe by plain pick |
| `map_move_land_tile_by_tile` | Dragoon, Wagon Train, Colonist moved by keypad over mixed terrain and roads into colonies until out of moves |
| `map_move_ship_tile_by_tile` | Two Caravels moved by keypad at sea, a blocked move into land, docking in Quebec |
| `map_goto_orders` | Go-to by mouse drag (land unit, ships), ORDERS Go to Place and Go to Port; units move under the order |
| `map_unit_orders` | Pioneers plow, clear forest (@CLEARCUT), build road; Fortify and Sentry |
| `map_board_and_ferry` | Colonist sentried in Quebec boards Caravel A on departure, a shore unit walks aboard, the full ship sails out |
| `map_ship_with_passengers_into_colony` | Starts from `map_board_and_ferry`'s output: the loaded Caravel sails into Quebec, passengers go ashore |
| `map_goto_colony_and_europe` | Go to Place (`g`) by land with a cancel; ship Go to Port picker (reachable ports only, Europe first; cancel, own port, Europe row = go-to the nearest High Seas); Return to Europe greyed off the high seas, by menu and `r` on them |
| `map_trade_route_edit` | TRADE menu: create wizard (land and sea, typed names, cancel), stop cargo lists (add, remove, 6 cap), Europe stop, rename, change and delete stops, delete a route (Esc, No, Yes) |
| `map_trade_route_assign` | From `map_trade_route_edit`'s output: Begin Trade Route with both pickers cancelled; a Wagon Train and a Caravel start their routes, unload, load and move |
| `colony_custom_house_options` | Quebec (+Custom House): toggle every checklist row on, off and mixed; close by Enter and outside click; reopen shows the choice kept |
| `village_scout_visit` | Scouts at Tupi villages: menu Escape (moves forfeited, same as the Cancel row); Speak with Chief at scouted, calm villages (outcome fixed: HOWDY then BORED) |
| `village_missionary_visit` | Missionaries: Establish Mission (plain and Jesuit), menu Escape and Cancel row (moves forfeited), Incite Indians declined, unaffordable (notice dismissed, no further quote) and paid |
| `village_learn_skill` | Free Colonists and Indentured Servants Live Among the Natives: No (LEARNLATER), Yes (skill learned, village marked), capital teaches again, LEARNALREADY |
| `village_criminal_learn_refused` | Petty Criminals refused (LEARNCRIMINAL) at taught, untaught and capital villages |
| `map_found_colony` | Colonist #131 on free coastal forest (44,22): B, name prompt accepted (Trois Rivieres), colony screen closed |
| `map_found_colony_mountain` | Colonist #131 on the mountain (42,22): B and ORDERS > Build Colony both refused with @TOOMOUNTAIN |
| `map_found_colony_native_land` | Peter Minuit removed, gold 1500: three colonists found colonies on Tupi (peace) land, nothing asked, nothing charged (gold 1500, lands_bought 0, no 0x10 stamp; port_bugs.md #1175) |
| `colony_tile_native_land` | Quebec, Minuit removed, Tupi at peace, plot S bought-bit cleared: a statesman dragged to S raises @INDIANLAND (respect = nothing changes), a fence colonist respects (stays outside) then offers 422 (paid, lands_bought +1, @INDIANBRIBE, joins and works S), a statesman takes SE (seated, alarm raised, gold kept); RNG record unchanged (port_bugs.md #1175) |
| `map_join_colony` | Free colonist #128 on the Quebec tile: B joins the colony from the map |
| `colony_abandon` | Port au Prince (pop 1, view via the Scout #95 selected): last colonist leaves, @ABANDON answered Yes |
| `colony_abandon_stockade_refused` | Guadeloupe (pop 4, + Stockade): one colonist leaves, the next is refused with @KEEPSTOCKADE (pop stays 3) |
| `colony_abandon_trade_route` | `colony_abandon` with Port au Prince moved to colony index 7 and three trade routes naming it (land, sea with a Europe stop, land) plus a Wagon Train and a Caravel on routes: the abandon drops the stops at it, shifts later stops down and walks the units' cursors (port_bugs.md #1179) |
| `colony_eject_siege_refused` | Guadeloupe besieged by three English Dragoons (at war): leaving as Colonist or Scout is refused with @SIEGE, as Soldier and Dragoon allowed (port_bugs.md #1165) |
| `map_landho_on_move` | From `map_move_ship_tile_by_tile`'s start with France's `named_new_world` and the discovery event bit cleared: loading prompts nothing, three ship moves with no land in the destination 3x3 prompt nothing, the fourth raises woodcut 1 then @LANDHO, a typed name lands in `country_name` (port_bugs.md #1178) |
| `map_ship_landfall` | From `map_board_and_ferry`'s output: @LANDFALL "Stay With Ships", then "Make Landfall" twice (one passenger ashore per confirm) |
| `map_board_ship_from_land` | Scout #95 on the shore steps onto empty Caravel #2 at sea and boards |
| `map_board_ship_full_refused` | Same, Caravel #2 with both holds full: the step is refused silently, the Scout stays ashore |
| `map_game_options_enable_all` | GAME > Game Options with every option off: all eight rows turned on (mouse and keyboard), reopened |
| `map_game_options_disable_all` | Base save: all eight rows turned off, reopened, closed by an outside click |

The `europe_*` tests were recorded live and start from `port_saves/carpentry.SAV`. The `europe_*` ones use a copy
edited with `sav_json` to 60000 gold and a recruit pool of Fishermen /
Lumberjacks / Veteran Soldiers, and open the European Status screen with E:

| Test | Does |
|------|------|
| `europe_buy_artillery_twice` | PURCHASE Artillery twice (500, then 600) |
| `europe_buy_each_ship` | PURCHASE each ship type once |
| `europe_buy_each_good` | Buy two Galleons and a Merchantman, drag 100 of every good aboard (price popups dismissed; the ship click's menu closed with Escape, hand-inserted in the log) |
| `europe_recruit_each_immigrant` | RECRUIT pool rows 1, 2, 3 in turn |
| `europe_train_each_profession` | TRAIN every Royal University row once |
| `europe_buy_ship_and_sail` | PURCHASE a Caravel and sail it for the New World (ship click's menu closed, then S and the menu's Set sail row, @SAILAWAY Yes; now an `input.steps` script) |
| `europe_reload_dock_artillery` | Loads `europe_buy_artillery_twice`'s output, opens and closes Europe, saves: the reload must be byte-identical |
| `europe_reload_dock_trained` | Same for `europe_train_each_profession`'s output (17 trained dock units, Pioneer tools) |

Step-script Europe tests (same base save, fixture edits noted in each `input.steps`):

| Test | Does |
|------|------|
| `europe_reverse_sailing_ship` | Drag ships between Expected Soon and Bound for the New World both ways, refused drops, harbor ship onto Bound answered No |
| `europe_equip_units` | Dock @ARMOPTIONS: Soldier, Scout, Pioneer, Missionary, Dragoon both ways; market drags that do nothing |
| `europe_unequip_units` | Equipped dock fixture + a Jesuit: every way back to Colonist, Dragoon to Soldier / Scout, Jesuit has no Unbless |
| `europe_reload_dock_equipped` | Loads `europe_equip_units`' output, opens and closes Europe: the reload keeps the dock order byte-identical |
| `europe_equip_units_poor` | 120 gold: unaffordable rows refused, muskets bought and sold, blessing free |
| `europe_ship_menu` | Galleon (Cotton 40 / Rum 20 / Furs 10) + empty Caravel in the harbor: a ship-icon release opens the ship menu (@EUROPESHIPCLICK / @EUROPESHIPOPTIONS, 3 rows when empty, 4 with goods), Move to front reorders the harbor, No changes / Esc / empty box space do nothing, S / Space / Enter open it, Unload all cargo sells every hold (port_bugs.md #1176) |
| `europe_sell_goods` | Drag-sells, `-` (whole first hold, then the "No Cotton in cargo hold!" red bar), `_` and Shift+drag amount prompts, `U` one lowest-key hold per press, the ship menu's Unload all, `U` on an empty ship (red bar) |
