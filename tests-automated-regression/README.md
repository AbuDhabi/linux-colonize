# Input-replay regressions

One subdirectory per test: `input.sav`, `input.jsonl`, `output.sav`.
Run by `regression_replay` (ctest); format and recording recipe in
[tests/README.md](../tests/README.md#input-replay-regressions).

Every test without a `turns` file stays inside one turn (the harness enforces
it), so AI and end-of-turn changes never move its expected save. The `turns_*`
tests end the number of turns their `turns` file names (tests/README.md
"Multi-turn tests"); AI and end-of-turn fixes may move them on purpose.

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
| `colony_jobs_more_leave` | Quebec: the jobs menu's "(More)" flips page 0 (16 job rows) to page 1 (the leave-as rows, greyed Pioneer..Dragoon) and back; a greyed row click does nothing; Colonist leaves the colony (port_bugs.md #1224) |
| `colony_remove_roles` | Montreal (+tools, muskets, horses, Church): colonists leave as Colonist, Soldier, Pioneer, Scout, Dragoon, Missionary (also dropped onto units on the fence); greyed rows; abandon prompt declined |
| `colony_fence_to_jobs` | Quebec: colonists on the colony tile join a building and a tile; the Dragoon joins (gear back to stock); Artillery cannot |
| `colony_change_construction` | Quebec (+Armory): CHANGE / `c` picker by mouse and keyboard, building, unit, cancel, (No Production) |
| `colony_buy_construction` | Quebec (hammers 20, gold 716): BUY declined, bought, a tools top-up buy, a refused buy |
| `colony_build_queue` | Quebec: Shift-click / Shift+Enter queue (port-only `BQUE` chunk), refusals, wipe by plain pick |
| `colony_build_queue_complete` | Starts from `colony_build_queue`'s output (cheat bit set): the loaded queue is numbered, Schoolhouse and Stable queued behind it; each project bought and completed by the colony-screen Space cheat tick, so the head starts (buildings, then the Wagon Train unit); three colonists leave (pop 3), so the Schoolhouse head is dropped and Stable starts |
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
| `map_first_contact_refused` | France made unmet with the Tupi (pre-contact alarm 40), go-to units sentried: Preacher #131 steps beside a Tupi village, @INDIANWELCOME answered No, @INDIANSHUN dismissed. MET both ways, contact_state 2, alarm 20 + 50 = 70, no PEACE bit, no village word, no RNG draw (port_bugs.md #1228) |
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

## Multi-turn tests

Each `turns_*` test has a `turns` file (the number of turns it ends); the start state and every
step are commented in its `input.steps`.

| Test | Turns | Does |
|------|-------|------|
| `turns_spring_1600_events` | 1 | From `map_unit_orders`' output (autumn 1599): the last unit sentried, Space ends the turn; Spring 1600 events (Spanish Succession: Spain withdraws, REF Regulars, rebel 50%, Coats price), a go-to Scout reaches an Iroquois village and speaks with the chief |
| `turns_colony_birth_and_horses` | 2 | Autumn 1599 start (go-to units sentried, combat analysis off; the English take Guadeloupe in turn 2, its four popups answered), Quebec food 195: at 205 the birth takes 200, a Free Colonist is born (@NEWCOLONIST); horses breed in Quebec and Montreal both turns |
| `turns_colony_build_complete` | 2 | Same start; Quebec Lumber Mill hammers 50/52 completes at end of turn; Montreal Artillery at 192 hammers with 34 tools raises @NEEDTOOLS, completes next turn once tools reach 40 |
| `turns_colony_starvation` | 2 | Same start; Quebec's food workers made statesmen, food 3: @FOOD1 stores depleted, next turn a colonist starves (@STARVE2) |
| `turns_warehouse_spoilage` | 2 | Same start; Quebec (no Warehouse) cotton 130 / furs 120 spoil to 100 (@SPOIL2), Montreal (+Warehouse) lumber 230 to 200 (@SPOIL1) while furs 150 stay; @CARGOREADY1 for tobacco reaching 100 |
| `turns_custom_house_exports` | 2 | Same start; Quebec + Custom House: 100 Furs sold at end of turn 1, 52 Tobacco at turn 2 (stock sold down to 50), tax deducted, gold rises |
| `turns_pioneer_jobs_complete` | 2 | Spring 1600 start (England and France at peace, the Europe-bound Caravel held at sea, French units fortified); Pioneers' job counters near done: road (+10 hammers to Quebec) and clear forest (@CLEARCUT, lumber to Guadeloupe) finish in autumn 1600, plow in spring 1601; tools 100 to 80 each; freed Pioneers fortified |
| `turns_goto_long_land` | 2 | Same start; a Free Colonist with Go To Quebec walks the roads from Guadeloupe and arrives next turn (order cleared, asks for orders); a Caravel with Go To Port Montreal docks in spring 1601 |
| `turns_fortify_sentry` | 2 | Same start; Fortify (5) becomes Fortified (6) next turn; a sentried colonist boards the departing Caravel, rides to Montreal and is woken there; a sentried Scout stays sentried, another is woken by a neighbour and re-sentried |
| `turns_school_training` | 2 | Same start; Quebec + Schoolhouse, an Expert Fisherman teacher at counter 2: the counter reaches 4, a Free Colonist student becomes Expert Fisherman (@TRAINPROFESSION), the counter resets |
| `turns_pioneer_automate` | 3 | Same start, `divergences.pioneer_plan`; Hardy Pioneer plans Plow (43,25) and Road (44,24), Automate: plow done autumn 1600, walks to (44,24) and builds the road autumn 1601 (tools 100 to 60) |
| `turns_founding_father_join` | 2 | Spring 1600 start (French units sentried), next father Pocahontas, bell pool 1000 of 1009: she joins (notice, congress, Colonizopedia), Franklin picked next, tension and alarm with every tribe zeroed; the Caravel's Europe arrival screen closed, woken units re-sentried |
| `turns_declare_independence_ref` | 2 | From `map_declare_independence`'s start (units sentried): declares, the REF lands near Montreal (@INVASION, intervention, Continental Army popups), a second wave takes Montreal and captures rebels |
| `turns_hotseat_rounds` | 2 | From `hotseat_handoff`'s start: two full hot-seat rounds 1492 to 1494, England and Spain handing over through @MULTINEXT with France and the Netherlands (AI) between |
| `turns_war_combat` | 2 | Spring 1600 start, England at war: Quebec's Dragoon routs a fortified English Dragoon twice (Dragoon to Soldier to Colonist), England takes Guadeloupe, offers a demarcation treaty, refused (@WARMEEK) |
| `turns_native_mission_trade` | 2 | Spring 1600 start: a Missionary walks to the Tupi camp and establishes a mission (tension zeroed); the Iroquois bring 16 Ore to Port au Prince (@INDIANGIVESTUFF) |
| `turns_ship_europe_roundtrip` | 4 | Spring 1600 start (other units fortified, England at peace, Caravel #148 parked): a loaded Caravel on the high seas sails for Europe (`r`), docks in turn 110, cargo sold, a colonist recruited, sails back with two colonists and reappears at (50,29) in turn 112 |
| `turns_trade_route_wagon` | 3 | Same start, routes from `map_trade_route_assign`: a Wagon Train runs Guadeloupe to Quebec to Montreal and back (100 Lumber, 12 Cotton moved) |
| `turns_trade_route_ship_europe` | 6 | Same start: a Caravel route with a Europe stop (sell Tobacco/Furs, buy Tools) and Quebec (unload Tools): sails out, trades in Europe, returns, unloads 100 Tools, aims at Europe again |
| `turns_immigration_crosses` | 1 | Same start, crosses one tick past the threshold (2 drained per waiting dock colonist): Brewster's @RECRUITCHOOSE pick, Master Distiller chosen lands on the docks, pool slot refilled |
| `turns_europe_price_drift` | 2 | Same start, two Galleons in Europe with 600 Furs each: the first unload drops the Furs price (notice), the second sells next turn at the lower price; the market tick runs both turn ends |
| `turns_idle_1492_start` | 6 | `early-game-seed-100/TURN1.SAV` re-saved: England's Caravel sentried at sea, 1492 to 1498; France, Spain and the Dutch land and found their first colonies |
| `turns_idle_1599_france` | 8 | `turns_spring_1600_events`' output, idle units sentried: spring 1600 to 1604, English attacks and captures (Guadeloupe, Montreal), native gifts, English diplomacy, a new Dutch colony |
| `turns_idle_dutch_campaign` | 5 | `dutch-campaign/COLONY03.SAV` (1669), idle units sentried: late Euro AI, a native raid beaten off, REF additions, Jan de Witt joins |
| `turns_idle_lategame` | 5 | `valid-lategame-saves/COLONY06.SAV` (1680), idle units sentried: native raids, buildings completed, Juan de Sepulveda joins, a new Spanish colony |
| `turns_idle_independence` | 5 | `independence-seed-100/000002.SAV` (spring 1681, declared), idle units sentried: REF landings, Tories take Bahia, Quebec and St. Louis, fort fire, Continental Army, mercenary offer declined |
