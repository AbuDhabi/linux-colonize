# Colony governor

Port-only, `mods.colony_governor` (docs/settings.md), `colony_governor.c`.
`ColonizeColony.governor` is a `ColonyGovernor` (none, Farming, Cash,
Industry, Politics, Military), set from the colony screen's CHANGE button above the
area view (`colony_screen_draw_governor`, `AI_POPUP_TAG_COLONY_GOVERNOR`) and
saved in RTST record 18. `game_do_end_turn` runs
`colony_governor_run_nation_w` for the human ending the turn, before the
turn processor starts, so the changes land as that human's own last moves.
The Colony Advisor report (both pages) lifts a governed colony's name by 4 px
and prints "Governor: <setting>" in white under it (`reports_render_colony_sidebar`);
ungoverned rows keep the DOS layout.

Autobuy tools: a checkbox line (@MODGOVERNOR row 7, FONTTINY's ']' / '['
glyphs) in the wood under the area view, mirroring the governor line, shown
whenever the mod is on, governor or not; `ColonizeColony.governor_autobuy`,
bit 7 of the RTST record 18 byte. At EOT, where Phase L would raise
@NEEDTOOLS (hammers in, tools short; the Colony Report option does not
matter), a checked human colony that can pay the BUY price
(`colonies_construction_gold_cost_ex`) skips the notice and, after that
tick's completion attempts, buys the tools through
`colonies_buy_construction_ex`, so the project completes next turn as after
the player's BUY. The status strip says "<colony> buys <n> tools for <gold>."
(@MODGOVERNOR row 8, @MISC 48), like a Custom House sale. Short of gold, the
notice fires as before (`turn_autobuy_tools`, turn_production.c).

Order: the project is picked first (below), then the colonists are seated
to serve it.

Seating: a hill climb on a copy of the colony. Each colonist tries every free
unblocked plot (`colonies_plot_blocked_mask` 0, and no tribal land unless the
colony already works it) with every field job that yields there, and every
indoor job 9..18 the jobs menu offers (`colonies_job_row_offered`, three per
building, the faculty cap for teachers); then every pair tries trading seats;
then every pair tries a chain: one onto the best free plot for a raw job
(sugar .. ore), the other into the workplace that processes it (@JOB 9..14),
since a lumberjack without a carpenter, or a carpenter without lumber, is
worth little alone. A change is kept when the score rises. Tribal land is a
plot the @INDIANLAND table claims, or any unbought plot inside a village's
radius (`colonies_indian_land_purchase_gold` > 0) even before the tribe is
met, where DOS seats without asking and stamps the plot bought: clearing it
(or Peter Minuit) is the player's call.

Score: three turns (`GOV_HORIZON`) of `colony_preview_compute_w` on a scratch
copy, each turn's net goods stocked into its warehouse before the next, so a
stored input runs out and a good stops counting once it reaches capacity
(none for Food, none for a good this colony's Custom House exports). Each
turn adds the goods that went in times per-governor weights, plus bells and
crosses, plus 0.01 per unit of any output (goods gained, bells, crosses,
hammers), so a worker idle in a building with no input moves to anything
that makes something, even output weighted 0. Food: the net counts at its weight (2.5, a colonist per 200),
deficits included; a stock below zero (starvation), or a shortfall with
under four turns of it left in store (`turn_produce_food`'s @FOODLOW, unless
the Colony Report option hides it), costs 1000 a unit, so a stocked colony may
run a deficit but never earns the popup. Hammers count while there is a
project: fully up to its and the queue's remaining cost, half beyond (the
surplus carries over, TB-57; none under `dos_buggy_behavior`). Every governor
weights hammers (2, Industry 4), listed project or not. A tool the project
still lacks is worth a tool plus a hammer. A convert seated indoors costs 3 a
turn (a third of the output there, +1 in the field). Sale value
(`gov_sale_value`) is the Europe sell price after the nation's tax, 0 for a
boycotted good or during the War of Independence unless one of the nation's
colonies has a Custom House. Cash weights goods at sale value; the others at
a quarter of it plus their own goods (horses always 1.5x the governor's food weight, so breeding pays): Farming food, Industry
hammers, tools, muskets, ore, lumber and a bonus per added tool matched by an
added musket (so blacksmiths feed a gunsmith rather than either working
alone); tools and muskets past 100 in store (a hold) weigh only their sale
share, and the bonus stops there, so the project's hammers come first; Politics bells and crosses (bells 1, Farming 0.75, Politics 6; crosses 0.5, Politics 5, 0 during the War of Independence, when Europe sends no immigrants), Military food 3.5 (growth, so horses 5.25), muskets, the tools and ore for them, hammers 3. Bells are weighted by SoL: x1.5 under
50%, x1 from 50%. At 100%, the 2 x pop a turn that keeps the colony there
weigh x1.5; at less, SoL slipped to 99% and called the statesman back every
other turn (seat churn only: the +2 latch holds down to 95%,
`colony_prod_refresh_sol_flags`). Bells past that upkeep
only feed the nation's pool (DOS clamps the rebel dividend to the divisor,
FUN_364b_0688), so they get half weight. They get none once every Father is
elected before the declaration, or once intervention is announced and the
pool's signed score word is full. There is no specialist bonus: a specialist out-produces
anyone on his own job, so the swaps seat him there unless his output is
unwanted or has no room, and then he works elsewhere.

Stickiness: a free colonist, servant or criminal already on a sugar, tobacco,
cotton or fur plot whose expert the nation lacks (`turn_produce_discover`'s
on-the-job roll, census by `turn_tally_professions_w`) scores a bonus for
staying. Teaching: a teacher scores per turn for each teacher up to the
number of free colonists, servants and criminals in the colony, more for each
non-expert working his expert job, and more the dearer his expert is on
the Europe Train list (@JOB column 3, `colonies_job_train_cost`; untrainable
rows count 0, and the widest price gap is worth less than one such
non-expert), so with no real need the school takes the dearest expert to spare.
A field expert teacher (farmer, fisherman, ...) also scores his graduates'
future surplus: his yield over a non-expert's on the best open plot for the
job, at the governor's weight for it, times 2.5 (`GOV_TEACH_FUTURE`), while
the colony has more such plots than experts (a fisherman needs the Docks); a seated teacher is not moved while there
is a student, and is free to work once none is left. Unseated workers
(`idle_job1`) are not moved; no one enters or leaves the colony. The result is
committed through `colonies_assign_field_w` / `colonies_assign_workplace`, the
player's own calls, for the colonists that move.

Construction: only when no project is queued and the current one is "(No
Production)", a built building, or no longer startable. When a good has no
room left, or fills within three turns at its current rate, Warehouse and
Warehouse Expansion go first; then the next unbuilt tier of the school chain
(Schoolhouse, College, University) and of the church chain (Church,
Cathedral), each when the seating, rerun as if it stood, scores over 2 a turn
more (`gov_probe_rows`: teachers for spare students, preachers for hands whose
best job is worth little); then the governor's list (Cash first tries every
crop chain that pays, most valuable first, each through all its tiers, house,
shop and factory, the Fur Trading Post included: a chain is worth the crop
the colony grows now, or its best seatable plot if more, at the processed
good's sale value, so a boycotted good with no Custom House drops out; Industry ends with Artillery and ships; Military puts Stockade, Armory, Fort, Stable, Fortress first and ends with Artillery).
The first startable row is picked; with none, "(No Production)", and the
hammers bank for a later turn. A unit project (Artillery, Wagon Train, ships)
repeats after each unit; when one completes and no queued plan takes over,
`colonies_try_complete_unit_construction` sets `governor_rethink` (bit 6 of
the record 18 byte), and the next governor run picks anew once, as if the
slot were empty, keeping the unit when nothing listed is wanted. Kept, it
then runs on until the next unit completes. A capture (`colonies_capture_show_ex`) resets
the governor to none. Covered by `unit_colony_governor`.
