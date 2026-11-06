# Input-replay regressions

One subdirectory per test: `input.sav`, `input.jsonl`, `output.sav`.
Run by `regression_replay` (ctest); format and recording recipe in
[tests/README.md](../tests/README.md#input-replay-regressions).

Every test without a `turns` file stays inside one turn (the harness enforces
it), so AI and end-of-turn changes never move its expected save. The `turns_*`
tests end the number of turns their `turns` file names (tests/README.md
"Multi-turn tests"); AI and end-of-turn fixes may move them on purpose. The
`turns_idle_*` tests have no input log: an `idle` file makes the harness play
their turns (tests/README.md "Idle tests"), so a fix moves their save but never
desyncs them.

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
| `colony_board_ship_units` | Quebec (fixture: empty Caravel A given full moves; Caravel B full of goods): a fence colonist and a Town Hall worker dragged onto a ship icon or its hold board nothing (DOS FUN_2f2b_628a: unit drags never drop on the transport pane); three fence units sentried from Units Present, one moved to front; full Caravel B sails out empty-handed, Caravel A takes the front unit plus one more (2 holds), the third stays sentried |
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
| `village_scout_demand_tribute` | Fixture: Tupi braves and their census rows handed to the Sioux (Tupi roll side 0), village 62 tribute_paid, Arawak alarm 80, a Scout beside the Arawak capital: Demand Tribute gives @EXTORTPOOR (rand(0,62) draws 0, no win), @EXTORTPOOR (already paid) and @EXTORTLAUGH (alarm >= 75, +2) |
| `village_trade_wagon_and_ship` | Caravel (Cloth/Rum) beside Tupi camp 69 and Wagon Train (Trade Goods/Cloth) beside camp 31: the ship gives its Rum as a gift and turns down 12 Furs (ships buy qty>>2); the wagon haggles Trade Goods 86 to 114 and sells, then a buy haggle exhausts their patience (sticky 0xfe, alarm +1) |
| `village_dragoon_attack` | Three Dragoons use Attack Village on Tupi camp 31 (combat analysis on): two wins (population 3 to 1), the third loses and is demoted to Soldier; the attack bit sets euro_diplo 100, alarm +9 per attack |
| `village_scout_chief_unscouted` | Speak With Chief at three unscouted Tupi camps: @CHIEFHOWDY, then @CHIEFGUIDES + @WELLSEASONED (Scouts promoted) twice and @CHIEFAREA (radius-6 reveal) once; villages marked scouted |
| `village_destroy_treasure` | `village_dragoon_attack`'s start, camp 31 and camp 69 (made the Tupi capital) at population 1 with French missions, Tupi muskets 0: one win razes each; camp 31 @LOOT2 (no treasure), the capital @INDIANBOW then @LOOT (600 Treasure train); tribe records compacted, home braves die, Missionaries return, villages_burned 2, alarm clamped to 15 |
| `map_rumour_colonist_outcomes` | Rumours restored at (30,62), (39,22), (36,25) (path owner nibble 0xf), de Soto removed, case-5 latch used, RNG state 2293: Seasoned Scout #77 small treasure 300 (3d8*10 doubled), Preacher #131 vanishes (@LOSTCITY5, despawned), colonist #134 finds Cibola on forest class 9 (Treasure 3500) |
| `map_rumour_de_soto_fountain` | Same start with de Soto kept, Fountain woodcut bit cleared, RNG state 17: the Scout's Fountain of Youth on mixed forest (de Soto skips the terrain gate), woodcut, eight recruit picks (rows 3, 3, 2, 1...); the Preacher still vanishes (de Soto helps Scouts only), the colonist gets the chief's gift of 50 |
| `map_rumour_burial_search` | Same start, no de Soto, latch clear, RNG state 13888: Scout's @LOSTCITY4 answered "Let us search", claim roll RNG(1,24) misses, @BURIAL3 Treasure 4200; then nothing (Preacher) and 110 gold (colonist) |
| `map_rumour_burial_stay_clear` | Same start: "Stay clear" draws nothing, so the Preacher gets survivors (@LOSTCITY9, Free Colonist joins) and the colonist gets the Tupi trespass (@LOSTCITY8, alarm 0 to 13 after the French halving) |
| `map_attack_soldier_loss` | Spring 1600 start (`turns_war_combat`'s input: England at war, French units sentried; Quebec's Dragoon made a plain Soldier): Combat Analysis on, the Soldier attacks the fortified veteran English Dragoon on hills (45,23) and loses, demoted to Colonists (@DEMOTE), moves spent |
| `map_attack_wagon_capture_artillery_damaged` | Same start, Combat Analysis off, English Wagon Train moved to (44,23) and a Colonist to (46,25), Quebec's Artillery woken: the Dragoon captures the wagon (@WAGONCAPTURE, it joins Quebec's tile); the Artillery attacks the Colonist in the open (strength cut to a quarter), loses and is damaged (@ARTILLERY) |
| `map_attack_colony_capture` | Same start, two veteran Dragoons at (39,23) beside English Cayenne (pop 2, no defender), an English Colonist at (38,22): Combat Analysis on; the first Dragoon captures the Colonist (@COLONISTCAPTURE), the second beats the militia phantom and marches into Cayenne (270 plundered, colony flips, census and relation updates), the captured colony's screen is closed |
| `map_attack_colony_militia_loss` | `map_attack_colony_capture`'s start: Dragoon #131 attacks Cayenne first and loses to the militia phantom, demoted to Soldiers with the whole Dragoon allotment spent (12, 1b0e entry FUN_281f_0934; port_bugs.md #1375); #87 is selected next |
| `map_attack_naval_privateer` | Same start, a French Privateer at (47,24) and a Frigate at (47,23), an empty English Caravel at (48,24): the Privateer loses (gunless winner, so damaged, not sunk: @SHIPDAMAGE, hull to the Europe Expected lane with a repair timer); the Frigate's attack is evaded (@EVASIVE), moves spent |
| `map_attack_naval_win` | `map_attack_naval_privateer`'s start, the English Caravel given 100 Furs + 100 Tools and an English Merchantman (four full holds) moved to (48,22), RNG state 134623: the Privateer wins, takes both holds (@CARGOCAPTURE twice) and sinks the Caravel (@SHIPSUNK: an unarmed Caravel after turn 0x4f always sinks); the Frigate wins, @PICKACARGO row 100 Muskets fills its one free hold (a human pick raises no @CARGOCAPTURE), the Merchantman's holds are emptied and it limps to London damaged (@SHIPDAMAGE); winners move onto the vacated tiles |
| `map_attack_brave_field` | Same start, Tupi braves moved onto open land around Quebec (Armed Braves (44,23), Braves (46,25) and (43,24)), Quebec colonist #142 and Pioneers #126 on (44,25) made Soldiers, RNG state 839414: @WHACKINDIANS asked once (Yes: euro_diplo 0x60 to 0x64); the Dragoon kills the Armed Braves (gear back: tribe muskets 2 to 3), a Soldier kills the Braves (no promotion), the outside Soldier loses (@DEMOTE to Colonists; a defending brave takes no gear); alarm 0 to 12, both home villages need_colonist |
| `map_contact_spain_first` | Spanish Dragoon moved to (44,24), France and Spain unmet: Preacher #131 steps beside it; FUN_5bfb_153e @HELLOFIRST, @APOSTATES "Never", @WORTHY "Yes" (PEACE \| MET both ways, `nation_relation[2]` = turn + 16, treaty timer 4), @OLDPEACEMEEK "Go in peace" |
| `map_contact_spain_refused` | Same start: @APOSTATES joined (Iroquois alarm +50), @WORTHY "No", @GIVECASH 400 refused, @WARMEEK; the pair left MET only, no peace menu |
| `map_contact_scout_colony` | Two Seasoned Scouts beside Dutch Fort Orange: @SCOUTCOLONY "Nothing" (allotment spent), W skips a colonist, "Meet With Mayor" (forced 153e: @HELLOMEEK, peace menu), "We suggest an alliance" against the Iroquois, @SMITEINDIANS 3250 paid with 429 gold: refused, nothing changes |
| `map_scout_spy_colony` | Same scouts, RNG state edited so the roll is 12: @SCOUTCOLONY "Infiltrate Colony" beats the Seasoned threshold 9 and opens Fort Orange's screen on Units Present, look only (a Dragoon click, a worker drag and `c` do nothing, port_bugs.md #1382; closed with Escape; only the scout's moves and one RNG draw change); the second scout's menu Escaped = Nothing (moves spent) |
| `map_stack_select_orders` | Three sentried colonists on (43,25): the stack picker's two-step (cancel, then activate) by click; Fortify, Sentry, No orders (Space) from it; View Pieces moves the cursor only, Center View changes nothing |
| `map_wagon_colony_haul` | Wagon Train #82 in Quebec: load 12 Cotton and 25 of 38 Horses (Shift amount prompt), road to Montreal (arrival spends all moves), unload Cotton and 10 Horses (15 stay aboard) |
| `map_wagon_foreign_trade` | England at peace, France with Jan de Witt: a Wagon Train (Ore 50 / Lumber 20) bumps English Cayenne, "Which cargo" picker, @TRADEWITH 50 Furs or 206$, the Furs taken (hold 0 swapped, Cayenne Ore +50 / Furs -50) |
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
| `game_retire` | Base save: GAME menu Retire, @RETIRE Yes, F10 score, exploits screen, Hall of Fame, title menu (`ends_on_title`; `output.hallfame` pins the record) |

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
| `europe_board_dock_units` | `europe_equip_units`' start + the empty harbor Caravel: a dock colonist dragged onto the ship only opens its dock menu (DOS has no unit drag in Europe), slot 0 set "Don't get on next ship", 100 Food bought, the Food hold dropped on empty dock space and on a dock colonist (nothing), the Caravel sails: one colonist boards the free hold, slot 0 and four sentried colonists stay on the dock |
| `combat_attack_colony_fortifications` | `map_attack_colony_capture`'s start; Cayenne + Stockade/Fort with an English Soldier, St. Louis + Fortress with an English Soldier, Port Royal + Stockade with a fortified Soldier (local_1a 4+2), three French attackers, Combat Analysis on: the Fort's defender is beaten (@DEMOTE, colony not entered); the attacks on the fortified Stockade and the Fortress lose (@DEMOTE, moves spent); the Fortress defender is promoted (profession 28 to 21, FUN_5fef_172c, no popup for an AI unit) |
| `combat_artillery_attack_colony` | `map_attack_colony_capture`'s start, an English Soldier in Cayenne (Stockade), both Dragoons at (39,23) made Artillery (#87 already damaged), RNG state 9001: on a settlement tile neither gun takes the open-field >>2; the sound gun loses and is damaged (@ARTILLERY), the damaged gun loses and is destroyed |
| `combat_landfirst_refused` | `map_attack_naval_privateer`'s start, Dragoon #131 aboard Frigate #121 beside English Dragoon #114: the Dragoon's step onto the English tile is refused with @LANDFIRST (FUN_4720_015c reason 9, no roll); it then lands on an empty tile with its whole allotment spent |
| `combat_naval_frigate_drydock` | `map_attack_naval_privateer`'s start, Quebec and Plymouth + Docks/Drydock, two English Frigates, RNG state 271828: the Privateer loses (a stronger defender never evades), @SHIPDAMAGE to Quebec; the Frigate wins and the English Frigate (census Frigates 0, FUN_5fef_0352 "own count < 2" clause) is damaged to Plymouth; the winner takes the vacated tile |
| `combat_naval_frigate_sunk` | `combat_naval_frigate_drydock`'s start with England's census at 2 Frigates: Frigate-vs-Frigate, the winner's nation counts fewer Frigates than the loser's, so the English Frigate is sunk (@SHIPSUNK, FUN_5fef_0352 raw 99527-99570); England's census tallies drop |
| `combat_attack_veteran_promotion` | `map_attack_colony_capture`'s start, a Free Colonist Soldier attacks an English Soldier in the open, RNG state 123457: wins (@DEMOTE on the loser) and is promoted (FUN_5fef_172c, profession 28 to 21, @VETERAN) |
| `ff_washington_promotion` | `map_attack_brave_field`'s start + Washington (FF 11): same three attacks; both Free Colonist Soldiers win and become Veteran Soldiers (0x15) with no promotion roll (FUN_5fef_172c), so the third fight is won rather than lost |
| `ff_la_salle_admit_stockade` | `map_join_colony`'s start, France owns La Salle (FF 9), no Stockades: B joins #128 to Quebec (pop 7), only Quebec gets its Stockade (FUN_15eb_1068 raw 11311; Montreal 7 / Guadeloupe 4 stay bare, port_bugs.md #1329) |
| `ff_stuyvesant_custom_house` | `colony_change_construction`'s start (Stuyvesant owned): C picker, six rows up from Lumber Mill, Custom House (row 18) becomes Quebec's project |
| `ff_stuyvesant_custom_house_refused` | Same keys with Stuyvesant cleared: no Custom House row (FUN_15eb_3650 raw 13691), the pick lands on Stable (row 17) |
| `ff_adam_smith_fur_factory` | Same start, Quebec + Fur Trading Post, Adam Smith (FF 0) owned: the row above Lumber Mill is Fur Factory (row 34), picked |
| `ff_adam_smith_fur_factory_refused` | Same without Adam Smith: no factory rows (raw 13707), the same keys pick Rum Distillery (row 28) |
| `bldg_change_list_gating` | Spring 1600 start, Montreal + Docks/Drydock/Shipyard: the C picker in Quebec (pop 6, no Press, no Stockade, no Adam Smith) lists 16 rows: no Fort, College / Magazine / Shipyard (min pop 8), Newspaper (no Press), factories or ships; Printing Press picked. Montreal (pop 7) lists 21 rows ending Caravel..Frigate (ships need only the Shipyard bit, FUN_15eb_3650); Galleon picked |
| `bldg_docks_fishing_gate` | Spring 1600 start, no edits: Guadeloupe (no Docks) refuses a statesman dragged onto its ocean plot (@NODOCKS); the field-jobs menu still lists Fisherman (rows not docks-gated, FUN_15eb_3454), picking it gives @NODOCKS again. Save unchanged |
| `bldg_school_teacher_gate` | Spring 1600 start, Quebec + College, Montreal + University, an Elder Statesman in each: Quebec's College refuses the Statesman (@NEEDUNIVERSITY) and takes a Master Blacksmith as second teacher (counter zeroed); Montreal's University takes its Elder Statesman |

## Divergence tests

Each opts into one `divergences` switch (docs/settings.md) through its own
`settings.json`; `map_current_vision` is render-only, so it also pins the final
frame in `output.frame` (its `output.sav` equals `map_move_land_tile_by_tile`'s).

| Test | Does |
|------|------|
| `pioneer_auto_goto` | `divergences.pioneer_plan`; Hardy Pioneer plans a road two tiles away (VIEW Plan Pioneer), ORDERS Automate walks it there (Go To, order 3) until its moves run out |
| `pioneer_auto_start_work` | `divergences.pioneer_plan`; Hardy Pioneer plans Clear Forest / Plow on its own tile, Automate starts the job on its own tile (order 8, progress 1, moves spent) |
| `pioneer_auto_road_instant` | `divergences.pioneer_plan`; Hardy Pioneer plans a road on its own unroaded plains tile, Automate finishes it at once (Hardy plains threshold 1; tools 100 to 80, plan bit cleared) |
| `map_scout_autoexplore` | `divergences.scout_autoexplore`; from `map_rumour_colonist_outcomes`' start (Seasoned Scout #77 on (29,62), full moves): `q` Automate picks the next-door Iroquois village over the (30,62) rumour (both points of interest, scan order) and walks in: woodcut + @ACTIONS menu, Cancel Action (Escape) spends its moves, the village joins `SAUT`'s entered-settlement set, Preacher #131 selected |
| `map_scout_autoexplore_unreachable` | `divergences.scout_autoexplore`; campaign 20 port save (England 1595), Scout #54 on (21,29) with full moves: `q` Automate's first pick, the (18,26) rumour behind the Arawak village, misses the Go To probe without spending the moves (port_bugs.md #1449), and the Scout walks to (18,28) the same turn |
| `map_ship_autoexplore` | `divergences.ship_autoexplore`; from `map_move_ship_tile_by_tile`'s start (Caravel on (47,29), 4 moves): `q` Automate heads for (52,28), the seen ocean tile that lifts the most fog per tile sailed, and sails until its moves run out at (51,28) (orders 3); fog lifts, `SAUT` saved |
| `map_modern_prng` | `divergences.modern_prng` (`settings.json` seed 1234); `map_rumour_colonist_outcomes`' start, the same three rumours roll from the seeded xoshiro256** stream: Scout finds nothing (LCG: 300 treasure), Preacher #131 gets a small tribe's gift, gold 429 to 467 (LCG: despawned), colonist #134 finds nothing (LCG: Cibola); the save gains RTST record 13 |
| `map_unlimited_units` | `divergences.unlimited_units`; `map_cheat_create_unit` start plus 145 sentried French Colonists on Quebec (45,24), 298 unit records: CHEAT Create Unit (Shift-F1, Colonists) four times; all four appear on (44,22), 302 records, the two past 300 saved in the `UNXT` chunk |
| `map_unit_limit_refused` | Same start, divergences off: the first two Creates appear (records 299, 300), the next two are refused with @TOOMANYUNITS (FUN_1427_06b4); 300 records, no `UNXT` chunk |
| `map_current_vision` | `divergences.current_vision`; `map_move_land_tile_by_tile`'s steps: the save is unchanged, `output.frame` pins the dithered veil over tiles no unit or colony sees after the moves |

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
| `turns_goto_long_land` | 2 | Same start; a Free Colonist with Go To Quebec walks the roads from Guadeloupe and arrives next turn (order cleared, asks for orders); a Caravel with Go To Port Montreal sails out the first turn (keys pressed mid-route are dropped) and docks in autumn 1600 |
| `turns_fortify_sentry` | 2 | Same start; Fortify (5) becomes Fortified (6) next turn; a sentried colonist boards the departing Caravel, rides to Montreal and is woken there; a sentried Scout stays sentried, another is woken by a neighbour and re-sentried |
| `turns_school_training` | 2 | Same start; Quebec + Schoolhouse, an Expert Fisherman teacher at counter 2: the counter reaches 4, a Free Colonist student becomes Expert Fisherman (@TRAINPROFESSION), the counter resets |
| `turns_pioneer_automate` | 3 | Same start, `divergences.pioneer_plan`; Hardy Pioneer plans Plow (43,25) and Road (44,24), Automate: plow done autumn 1600, walks to (44,24) and builds the road autumn 1601 (tools 100 to 60) |
| `turns_hard_king` | 2 | `turns_declare_independence_ref`'s start and declaration with `divergences.hard_king`: five REF waves a turn (Montreal x3, Quebec, Port au Prince); the Tories take Montreal, Port au Prince and Quebec, sink two Caravels and capture rebels ("all but 2 of our colonies"); REF pool 49/25/14/26 to 31/15/4/18 |
| `turns_founding_father_join` | 2 | Spring 1600 start (French units sentried), next father Pocahontas, bell pool 1000 of 1009: she joins (notice, congress, Colonizopedia), Franklin picked next, tension and alarm with every tribe zeroed; the Caravel's Europe arrival screen closed, woken units re-sentried |
| `turns_declare_independence_ref` | 2 | From `map_declare_independence`'s start (units sentried): declares, the REF lands near Montreal (@INVASION, intervention, Continental Army popups), a second wave takes Montreal and captures rebels |
| `turns_hotseat_rounds` | 2 | From `hotseat_handoff`'s start: two full hot-seat rounds 1492 to 1494, England and Spain handing over through @MULTINEXT with France and the Netherlands (AI) between |
| `turns_war_combat` | 2 | Spring 1600 start, England at war: Quebec's Dragoon routs a fortified English Dragoon twice (Dragoon to Soldier to Colonist), England takes Guadeloupe; on its turn-109 move England offers a demarcation treaty, refused (@WARMEEK) |
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
| `turns_king_tax_tea_party` | 2 | `turns_school_training`'s start verbatim: turn 109's +1% tax raise refused through @TAXOPTIONS row 2 ('Quebec Tobacco Party'): @TEAPARTY dumps the 12 Tobacco held before production, tax stays 26%, Tobacco boycotted |
| `turns_sol_membership_latches` | 2 | Same start, colony rebel pairs and latches edited: Quebec @TORYMINORITY (100 latch, 94%), Montreal @REBELMAJORITY (50%), Guadeloupe @TORYMAJORITY (48%), Martinique @REBELUNANIMOUS (100%); turn 2 runs with the new production bonuses, Guadeloupe re-crosses 50% |
| `turns_ship_drydock_repair` | 2 | Same start, Quebec + Drydock, its two ships damaged (Caravel timer 0, a Merchantman timer 2): out of the orders rotation, timer +2 a turn in port, @REFIT for the Caravel in turn 1 and the Merchantman in turn 2, each then asks for orders |
| `turns_native_convert_joins` | 3 | Same start, French Jesuit missions in the three Iroquois villages by Port au Prince: turn 109's Brave visit sends an Indian Convert instead of 16 Ore (@INDIANSCONVERT); it joins Port au Prince (B) and works a land tile in turn 110 |
| `turns_king_tax_accept` | 2 | `turns_king_tax_tea_party`'s start: turn 109's +1% tax raise (@KINGTAX) accepted through @TAXOPTIONS row 0 ('Kiss pinky ring'): tax 26% to 27%, no @TEAPARTY, no boycott, Quebec keeps its Tobacco |
| `turns_native_raid_colony` | 1 | Same start, Iroquois alarm toward France 100, village words 200, Brave #29 beside Port au Prince: the Brave attacks and loses (@INDIANLOSE), 1b0e's colony limb raids (@RAIDSTORES, 10 of 20 Furs stolen, village word zeroed); a second Brave ambushes the French Scouts (@INDIANWIN2, Horses seized) |
| `turns_woi_victory` | 1 | `map_declare_independence`'s output (declared) with the REF pools at [0,0,1,0]: end of turn 107 wins the war (@WINNING, @KINGLOSE, closing cinematic, score, exploits, Hall of Fame), @SCORED "That's all." ends on the title menu |
| `turns_woi_victory_keep_playing` | 1 | Same, @SCORED "Keep playing anyway." returns to the main map in turn 108 (same save as the title-menu end) |
| `turns_woi_defeat` | 1 | `turns_declare_independence_ref`'s output with Quebec, Guadeloupe and Martinique handed to the Crown: a REF wave and Tory fights, then @LOSING3 (Crown >= 90% of the population), @KINGWIN, score, exploits, Hall of Fame, title menu |
| `turns_ff_fugger_boycotts` | 1 | `turns_pioneer_jobs_complete`'s output, next father Fugger, bell pool 1000 of 1009, Furs + Cloth boycotted: he joins, boycott_bitmap 0x0810 to 0 (FUN_4345_0342 case 1); Adam Smith picked next |
| `turns_ff_la_salle_stockades` | 1 | Same start, next father La Salle: Stockades for Quebec (pop 6), Montreal (7), Guadeloupe (4); Martinique and Port au Prince (pop 1) get none (case 9) |
| `turns_ff_coronado_reveal` | 1 | Same start, next father Coronado: France's seen bit set within 5 tiles of all 14 colonies, foreign included, nowhere else (case 6 to FUN_13f1_00a6) |
| `turns_ff_brebeuf_missions` | 1 | Same start, next father Brebeuf: French plain missions (villages 7, 61, 68) go 1 to 17 (Jesuit bit); the French Jesuit and Dutch missions untouched (case 0x16) |
| `turns_ff_las_casas_converts` | 1 | Same start, next father Las Casas, a map Colonist and a Soldier given Convert profession: Quebec's Convert and the map Colonist become Free Colonists; the Soldier keeps 27 (type-0 test, case 0x18) |
| `turns_ff_bolivar_sentiment` | 1 | Same start, next father Bolivar: rebel sentiment 53 to 75 (55 without him), Montreal's SoL notice at 66%, @REBELUP50 at 75%; Franklin replaces him in the picker |
| `turns_ff_hudson_furs` | 1 | Same start, France owns Hudson, Quebec's farmer made a Fur Trapper on the N plot: Furs 0 to 6 at end of turn (3 without Hudson; FUN_15eb_18ec raw 11970) |
| `turns_combat_coastal_fort_fire` | 1 | `turns_war_combat`'s start, English Jamestown + Stockade/Fort, the empty French Caravel #79 moored beside it, Quebec + Drydock: England's colony EOT opens with @FORTFIRE, a hit (@SHIPDAMAGE, to Quebec); Guadeloupe falls as in `turns_war_combat`; the French repair tick fixes the Caravel and it asks for orders |
| `turns_bldg_factory_tiers` | 1 | `turns_school_training`'s input, one statesman per colony moved into a building: Quebec Iron Works 10 tools from 6 ore (factory (G*2)/3), Montreal Armory 3 muskets from 3 tools, Martinique Magazine 6 from 6, Guadeloupe Arsenal 10 from 10 (muskets pair 1:1, port_bugs.md #910); SoL +1 before the tier step (FUN_15eb_1d4c) |
| `turns_bldg_cathedral_crosses` | 1 | Same start, Quebec Church + Cathedral with a Preacher, Montreal Church (empty): crosses = base 1 + Church 1 + Cathedral 1 + worker (3+1) doubled by the Cathedral; current_crosses 5 to 16 (the dock drain clamps the old 5 to 0) |
| `turns_bldg_printing_press_bells` | 1 | Same start, Quebec + Printing Press, Montreal + Press and Newspaper: Quebec bells 19 to 28 (Press v += v>>1), Montreal 15 to 30 (Newspaper v <<= 1 only, not both) |
| `turns_bldg_shipyard_caravel` | 1 | Same start, both colonies + Shipyard: Montreal Merchantman at 192 hammers with 50 tools raises @NEEDTOOLS; Quebec Caravel (128 hammers, 40 tools) completes on the colony tile (hammers/tools to 0, project kept), the new Caravel asks for orders |
| `turns_bldg_college_training` | 1 | Same start, Quebec College (Master Blacksmith teacher, counter 5), Montreal University (Elder Statesman, counter 7): both graduate a Free Colonist (@TRAINPROFESSION, Montreal first: colonies run highest index down), counters reset |
| `turns_end_soon_retiring` | 1 | `turns_king_tax_accept`'s start moved to Autumn 1789: Spring 1790 shows @SOONRETIRING0 (FUN_3844_0442 raw 58617-58628, season 0, no WoI); the game goes on |
| `turns_end_retiring_1800` | 1 | Same start moved to Autumn 1799: Spring 1800 peacetime end, @RETIRING (estate = most populous colony, last of ties: Montreal), F10 score, exploits, Hall of Fame, @SCORED "That's all." ends on the title menu (popup placement is port_bugs.md #1387) |
| `turns_end_retiring_keep_playing` | 2 | Same, @SCORED "Keep playing anyway.": calendar_latch set, back on the map; Autumn 1800 plays on with no second @RETIRING |
| `turns_end_soon_retiring_war` | 1 | `turns_declare_independence_ref`'s output (declared) moved to Autumn 1839, English Roanoke handed to France (3 open-sea ports), units fortified: REF wave and Tory fights, then Spring 1840 @SOONRETIRING1 |
| `turns_end_retiring2_keep_playing` | 2 | Same fixture at Autumn 1849: Spring 1850 wartime end, @RETIRING2 (estate Quebec), score, exploits, Hall of Fame, @SCORED "Keep playing anyway." (port_bugs.md #1232); the war goes on through Autumn 1850 with no second @RETIRING2 |
| `turns_end_woi_warn1` | 1 | Declared start (Autumn 1600), Guadeloupe handed to the Crown: 3 colonies, 2 ports, crown share 57%: @WARN1 |
| `turns_end_woi_warn2` | 1 | Declared start, Martinique and Port au Prince handed to the Crown: 2 colonies (overrides ports < 3): @WARN2 |
| `turns_end_woi_warn3` | 1 | Declared start, Roanoke to France, Quebec, Guadeloupe and Cayenne to the Crown: 3 colonies, 2 ports, share 83% overrides the ports test: @WARN3 |
| `turns_end_woi_losing1` | 1 | Declared start, St. Louis to France, Quebec, Guadeloupe and Port au Prince to the Crown: no open-sea port left, share < 90%: @LOSING1, @KINGWIN, score, exploits, Hall of Fame, title menu |
| `turns_king_merc_hire` | 1 | Declared start, Roanoke to France, gold 10000: FUN_43f7_2022 offers 2 Regulars + Artillery for 7200 (@MERCENARIES); "Pay": gold to 2800, @MERCS lands 2 Continental Army + 1 Artillery in Quebec |
| `turns_king_buy_ref` | 1 | `turns_king_tax_accept`'s start, royal_money 1790, REF pools [95,25,14,26]: FUN_43f7_1d42 adds 84, purse >= 1800 buys one unit, last write wins (Cavalry then Man-O-War): pools [95,25,15,26], @KINGBUY, purse 74 (DB-33 bells bump not reproduced) |
