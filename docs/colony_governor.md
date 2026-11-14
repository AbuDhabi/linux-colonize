# Colony governor

Port-only, `mods.colony_governor` (docs/settings.md), `colony_governor.c`.
`ColonizeColony.governor` is a `ColonyGovernor` (none, Farming, Cash,
Industry, Politics, Military), set from the colony screen's CHANGE button above the
area view (`colony_screen_draw_governor`, `AI_POPUP_TAG_COLONY_GOVERNOR`) and
saved in RTST record 18. `game_do_end_turn` runs
`colony_governor_run_nation_w` for the human ending the turn, before the
turn processor starts, so the changes land as that human's own last moves.

Order: the project is picked first (below), then the colonists are seated
to serve it.

Seating: a hill climb on a copy of the colony. Each colonist tries every free
unblocked plot (`colonies_plot_blocked_mask` 0, and not native land unless the
colony already works it) with every field job that yields there, and every
indoor job 9..18 the jobs menu offers (`colonies_job_row_offered`, three per
building, the faculty cap for teachers); then every pair tries trading seats.
A change is kept when the score rises.

Score: three turns (`GOV_HORIZON`) of `colony_preview_compute_w` on a scratch
copy, each turn's net goods stocked into its warehouse before the next, so a
stored input runs out and a good stops counting once it reaches capacity
(none for Food, none for a good this colony's Custom House exports). Each
turn adds the goods that went in times per-governor weights, plus bells and
crosses. Food: the net counts at its weight (2.5, a colonist per 200),
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
alone), Politics bells and crosses (bells 1, Farming 0.75, Politics 6), Military food 3.5 (growth, so horses 5.25), muskets, the tools and ore for them, hammers 3. Bells are weighted by SoL (x1.5 under
50%, x0.25 at 100%). There is no specialist bonus: a specialist out-produces
anyone on his own job, so the swaps seat him there unless his output is
unwanted or has no room, and then he works elsewhere.

Stickiness: a free colonist, servant or criminal already on a sugar, tobacco,
cotton or fur plot whose expert the nation lacks (`turn_produce_discover`'s
on-the-job roll, census by `turn_tally_professions_w`) scores a bonus for
staying. Teaching: a teacher scores per turn for each teacher up to the
number of free colonists, servants and criminals in the colony, more for each
non-expert working his expert job; a seated teacher is not moved while there
is a student, and is free to work once none is left. Unseated workers
(`idle_job1`) are not moved; no one enters or leaves the colony. The result is
committed through `colonies_assign_field_w` / `colonies_assign_workplace`, the
player's own calls, for the colonists that move.

Construction: only when no project is queued and the current one is "(No
Production)", a built building, or no longer startable. When a good has no
room left, or fills within three turns at its current rate, Warehouse and
Warehouse Expansion go first; then the governor's list (Cash first tries the
chain of its most valuable raw crop; Industry ends with Artillery and ships; Military puts Stockade, Armory, Fort, Stable, Fortress first and ends with Artillery).
The first startable row is picked; with none, "(No Production)", and the
hammers bank for a later turn. A capture (`colonies_capture_show_ex`) resets
the governor to none. Covered by `unit_colony_governor`.
