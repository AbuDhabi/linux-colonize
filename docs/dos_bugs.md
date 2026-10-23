# DOS bugs the port does not reproduce

STATUS: living record, not an audit dump. Authoritative.

VICEROY.EXE behaviour that is a bug in the original and that this port
**deliberately does not reproduce**. Fidelity to DOS is the default
([project_goals.md](project_goals.md)); an entry here is an explicit, user-approved
exception to it. Add a row only when the user has agreed the DOS behaviour is
undesirable — an agent must not decide this alone (every "stand-in" so far turned
out wrong; see [conventions.md](conventions.md)).

Not this file:

- Port diverges from DOS but DOS is *right* (data-model cost, capture still
  needed): [known_divergences.md](known_divergences.md).
- DOS bug that **is** ported literally and that nobody has judged unwanted:
  no entry anywhere; the `DOS-LITERAL` comment at the port site records it.
- Unwanted DOS bug the port still reproduces for now (due to move here):
  [tolerated_dos_bugs.md](tolerated_dos_bugs.md).
- Actionable port bug: [port_bugs.md](port_bugs.md).

Entries keep their `port_bugs.md #` id when they came from a bug row, so
citations still resolve; ids are permanent and never reused. Each entry names the
DOS function (`FUN_ssss_oooo` + raw offset), what DOS does, what the port does
instead, and the port site, so a later reader can tell "intentional" from
"drifted". Grep this file for a feature before "fixing" a port/DOS mismatch.

## Entries

### DB-1 — In-game load resumes the outgoing game's day on top of stale RAM
**DOS:** the in-game Load Game does not reset the running game. `FUN_130d_0290`
(year loop) finishes the *outgoing* game's day from the loaded slot's state
(extra production ticks before the first autosave), and tables the save never
carried keep whatever the previous game left in them (see entry DB-2). The result
depends on what was in memory before the load, so it is not reproducible and not
worth modelling. Evidence: the first two idle-independence recordings
([idle_independence.md](idle_independence.md) "Earlier recordings") started from
such a load and drifted from a clean main-menu load of the same slot.

**Port:** main-menu load and in-game load both run the same clean init
(`game_apply_col1_save`, `src/core/game_loop_saveload.c`): the world is rebuilt
from the file alone, nothing from the previous game survives.
**Status:** done (2026-10-18).

### DB-2 — Save files omit live state, so save+reload diverges from continuous play
**DOS:** the save (`FUN_75c2_0288`) does not write several tables the turn loop
reads, so a game played straight through and the same game saved and reloaded
take different paths. Known holes (idle_independence.md "Quirks of this save
set"): the Euro AI stance table `DS:0x9870` (nation*16 + continent) and the AI
goal tables `DS:0x98b0..` read by the 0a60 work-queue military arm; the Indian
rank table `DS:0x917c` (a load starts with zeros, the day top's 0550 refresh
refills it). A DOSBox load of a mid-campaign autosave does not reproduce the
continuous run (save 5: 355 RNG draws vs 381).

**Port:** the `RTST` chunk of the port extension block (`COLNXEXT`,
[savegame.md](savegame.md) "Port extension block"; `src/core/runtime_ext.c`)
carries every such table plus the port's LCG, written on every save and
restored by `col1_bridge_apply` on top of the module resets. Nine records
(2026-10-19): RNG state + the `FUN_281f_04ca` reseed word, stance `DS:0x9870` +
rival strength `DS:0x9e98`, unit-slot compaction residue (`ColonizeUnitPool.dos_tail`),
the AI goal / work-queue / inventory / plan tables, the rank cache `DS:0x917c` +
profession census, the coarse fog plane `DS:0x9faa`, Euro AI scratch (0a60 masks,
explorers, village-visited bits, ship pressure, sticky anti-spin), per-colony
`plot_was_worked_mask`, and the pathfinder word `DS:0x1dd2` + flood cache. Every
port save therefore carries the block and the `*` slot mark; DOS still loads it
(ignores the tail) and drops it on re-save, after which the port loads it the DOS
way (tables zero, LCG from the turn number).
**Status:** done (2026-10-19). Guarded by CTest `golden_idle_reload` (1492-1599),
`golden_idle_independence_reload` (saves 1-21) and `golden_idle_america_reload`
(saves 1-63): the idle campaigns with a save/reload before every turn (`GOLDEN_IDLE_RELOAD=1`) stay byte-identical to
DOS wherever the continuous gates do. A later hole shows up there first; the
recipe is in [idle_campaign.md](idle_campaign.md) "Running and debugging".

### DB-3 — Independence rename overflows the country-name field
**DOS:** when a rival nation is granted independence (@OTHERGRANTED,
FUN_3844_0442, raw 58602) DOS `strcpy`s the NAMES.TXT republic name
into `player.country_name` (DS `nation*0x34 + 0x5426`, 24 bytes) with no bound.
"United States of America" is exactly 24 characters, so its NUL lands on
`player+0x30` and zeroes that byte: `lcr_case5_bonus_used` (bit 6) and
`named_new_world` (bit 7) are wiped as a side effect. Evidence: america seed-100
216->217, England renamed, player 0 byte +0x30 goes from 0x40 to 0.

**Port:** `turn_year_end_rival_independence` (`src/core/turn_year_end.c`) fills
the 24-byte field (no NUL when the name is exactly 24 characters) and leaves
`player+0x30` alone. Every reader takes the name through
`col1_player_country_name` (`src/core/col1_save.c`), which returns a terminated
copy. A DOS load of such a save reads the name up to the next zero byte, so it
can show a stray character after "America" when bit 6 or 7 is set.
**Status:** done (2026-10-21). The idle America resync pair 217 differs from
DOS only in `players[0].lcr_case5_bonus_used` (plus the hold[5] artifact).

### DB-4 — Euro AI border park reads an uninitialised stack word
**DOS:** the 0x46 border-park arm of FUN_521d_20e6 (raw 89011-89029, the LAB_4d2e
tail) tests `local_ea` (BP-0xE8 = SS:e7c4), which 20e6 never writes before that
read. The word is whatever the last frame at that stack address left. Some writers
are deterministic: FUN_6662_00f2's `PUSH SI` (nation*0x13c, only when 00f2 is
entered at SP e852 through 6d8e -> 0972 -> 0f74), the FUN_5bfb_022e met arm (the
mover's x), and the previous 20e6 scan's own saw-foe flag. The others are RTLink
overlay-loader residue (an overlay size or relocation word, e.g. 0x2fe0, 0x4232).
Which overlays reload, and so the residue value, depends on the overlay area's
history since process start. So a combat land unit beside a foreign colony parks
('F') or wanders depending on memory history, not game state: a DOSBox from-load
of the same save and the continuous archive disagree on the same turn. Evidence:
america seed-100 save 63 -> 64 (England's Dragoon u61 beside Quebec parks in the
archive; a from-load parks all three Dragoons), and dozens of resync pairs past it
([archive/idle_america_fixed_2026-10-19_21.md](archive/idle_america_fixed_2026-10-19_21.md),
"e7c4 class").

**Port:** `ColonizeTurnContext.ai_euro_wander_stack_word` (`src/core/turn.h`)
models the deterministic writers only (`ai_euro_act.c` 00f2 flood, `ai_euro.c`
022e met arm, `ai_euro_20e6_wander_step` in `ai_euro_land.c` for its own scan,
which is also the reader). `ai_euro_dispatcher_turn` resets it to 0 at each
nation's turn. The loader residue is not modelled, so the park decision is a
function of game state only, and a save/reload gives the same result.
**Status:** done (2026-10-21). The America idle archives end at save 63 and save 12
for this reason (CTests `golden_idle_america` and `golden_idle_america2`,
[idle_america.md](idle_america.md), [idle_america2.md](idle_america2.md)).
port_bugs.md #1135 is an open lead for one more deterministic writer (the
case-7 founding call tree).

### DB-5 — Euro AI Europe hire tail reads an unset stack word
**DOS:** FUN_521d_5d04 writes `local_24` (BP-0x24, the dock's colony demand
`FUN_281f_08bc(dock, 0xe)`) only inside the colony-demand block (raw 92818),
which runs only when `local_22 != 0` and the planning gates pass. The
departing-ship loop reads it whether or not that block ran (raw 93020): with a
unit hired this pass, a ship whose hold count (`DS:0x5237 + type*0xe`) equals
`(char)local_24` takes the capacity-match arm (hired flag cleared, ship
exhausted). When the block was skipped, the byte is whatever an earlier frame
left at that stack address. Not traced; no recorded case where it changed a
result.

**Port:** `ai_euro_5d04_hire_tail_colony_demand` (`src/core/ai_euro_europe.c`)
starts `colony_demand_total` at 0 and passes it to
`ai_euro_5d04_hire_tail_departing_ships` through the tail ctx. No ship has 0
holds, so a skipped block never matches.
**Status:** done (port behaviour predates the entry; recorded 2026-10-21).

### DB-6 — Colony "out of ore" message tests net muskets against net tools
**DOS:** the Phase K colony messages in FUN_364b_0688 (raw 57696-57728) are
seven independent tests of the form `unmet[input] != 0 && 0b50(output) == 0`
(0b50 = net stock change). The @ORE arm (raw 57717-57724) instead tests
`unmet[ore] != 0 && 0b50(muskets) == 0b50(tools)`: a copy-paste slip from the
@TOOLS arm below it. So DOS stays silent when the blacksmiths ran the ore out
and tools did not grow while a gunsmith made muskets, and it nags whenever
the two nets happen to be equal, even when tools did grow.

**Port:** the @ORE arm in `turn_run_colony_production_w`
(`src/core/turn_production.c`, Phase K table) tests `unmet[ore] != 0 &&
net(tools) == 0`, like the other six arms. Message chrome only: no stock,
RNG or AI path reads it; every golden and idle campaign stays byte-identical.
Unit test: `unit_turn_core` `phase_k_build_advisory` ("ORE net-tools-zero"
fails on the DOS test). Came from port_bugs.md #911.
**Status:** done (2026-10-21).

### DB-7 — Denounce Heresy tests Cotton Planter instead of Jesuit
**DOS:** Denounce Heresy (`thunk_FUN_1000_a594`, viceroy_overlays.asm 126507:
`IMUL BX,[BP+6],0x1c / CMP byte ptr [BX+0x315b],0x3`) gives the denouncer's side
its Jesuit bonus (weight ×2, alarm delta ×2) when the unit's profession byte is
3, the @JOB row for Cotton Planter, not 0x18 (Jesuit Missionary) as the
establish-mission body (`thunk_FUN_1000_a5dc`) tests. A Missionary is never a
Cotton Planter, so the bonus is dead: a Jesuit denounces like a plain
missionary, and a won mission is installed without the Jesuit bit (0x10).

**Port:** `ai_contact_denounce_heresy` (`src/core/ai_contact_actions.c`) tests
`UNITS_JOB_MISSIONARY` (0x18). Unit test: `unit_ai_contact_core`
`actions_menu_missionary` / `actions_menu_wagon` (heresy block). All goldens,
idle campaigns and `golden_ai_joint` stay byte-identical (2026-10-21). Came
from port_bugs.md #558.
**Status:** done (2026-10-21).

### DB-8 — Accepting an AI goods demand always hands over Furs
**DOS:** in the Euro AI talk (FUN_5bfb_153e, OVL16), the @WANTSTUFF demand picks
a cargo and amount from the human colony next to the AI unit (`[bp-0xb2]`,
amount capped to that colony's stock and the receiver's warehouse room). When
the player accepts, the transfer (OVL16 asm 0x2995-0x29B2) indexes the colony
stock rows with `[bp-0xa2]`, the loop counter left at 4 by the euro rival-tally
do-while, so it always moves Furs (cargo 4) by the demanded amount, whatever the
dialog named. With fewer Furs than the amount the source stock underflows.

**Port:** the WANTSTUFF accept arm of `ai_talk_resume`
(`src/core/ai_diplo.c`) moves `want_cargo`, the cargo the dialog named; source
stock floors at 0 as a guard. Unit test: `unit_ai_diplo`
`case_153e_wantstuff_demand` (Muskets move, Furs do not). All goldens, idle
campaigns and `golden_ai_joint` stay byte-identical (2026-10-21).
**Status:** done (2026-10-21).

### DB-9 — Jesuit "leave as Colonist" row keyed on the colony's record slot
**DOS:** FUN_15eb_3454 (raw 13561-13565) decides whether the "leave as
Colonist" row is offered to a unit leaving a colony. For a Jesuit Missionary
(profession 0x18) it refuses the row when `*(int*)0x8dc6 < 4` and the control
byte `*(char*)(0x543f + *(int*)0x8dc6 * 0x34)` is 0 (human). `DS:0x8dc6` is the
active colony's record index (FUN_15eb_002c raw 9318), but `0x543f + n*0x34` is
the nation table. So a Jesuit can leave as a plain Colonist almost everywhere,
except in colonies whose record slot (0..3) happens to equal a human nation's
number, whoever owns the colony.

**Port:** `colonies_eject_row_offered` (`src/core/colony_workers.c`) drops the
arm: any Jesuit, human or AI, is offered the Colonist row (user decision
2026-10-21, matching what DOS does in nearly every colony). Unchanged from DOS:
a Jesuit is always offered the Missionary row, church or not; at the Europe
dock a born Jesuit cannot unbless (38fd:3f4a). Unit test: `unit_colonies`
#559b (fails on the DOS arm). All goldens, idle campaigns and
`golden_ai_joint` stay byte-identical (2026-10-21). Came from port_bugs.md
#559.
**Status:** done (2026-10-21).

### DB-10 — Euro AI specialist pass can write profession 0xff
**DOS:** the ARM 1 specialist pass of FUN_5952_035e (raw 95905-95914) elects
`local_8a`, or the picked colonist's current occupation (`FUN_281f_0c0e`) when
`local_8a` is negative, and writes it as the profession through
`FUN_281f_0cae` (= `FUN_15eb_0e8c`) with no check. A pick whose occupation is
idle stores profession 0xff, which is no @JOB row. Rare: ARM 1 runs after all
placement passes.

**Port:** the ARM 1 block in `src/core/ai_euro_colony_jobs.c` writes the
profession only when `elected >= 0`; otherwise the colonist keeps its old
profession. The port's colonist record reads -1 as "no profession", so the
literal write would also change other readers. Was known_divergences.md #780
(moved 2026-10-22: DOS is the buggy side).
**Status:** done (port behaviour predates the entry; recorded 2026-10-22).
Revisit only if a DOS save shows 0xff colonist profession bytes.

### DB-11 — @TRAINFAIL names whatever colony the last message named
**DOS:** in the school pass of FUN_364b_0688, a teacher with no eligible
student raises @TRAINFAIL (`0xde7`, raw 57542), whose text names `%STRING0`.
Token 0 is set to the colony name only later, in the graduation arm
(`FUN_281f_0416(0, colony+2)`, raw 57552), and nowhere earlier in 0688. So the
message shows whatever token 0 held from an earlier message: another colony's
name or an unrelated string.

**Port:** the @TRAINFAIL arm of the school pass in
`src/core/turn_production.c` fills `%STRING0` with the current colony's name.
Human-only message chrome; no stock, RNG or AI path reads it. Was the last
bullet of known_divergences.md #909 (moved 2026-10-22).
**Status:** done (port behaviour predates the entry; recorded 2026-10-22).

### DB-12 — Europe Artillery price counter rises when the purchase is cancelled
**DOS:** FUN_38fd_4b50 (raw 64870-64875) computes the Artillery price as base
+ `nation[0x1e]*100`, then increments `nation[0x1e]` before the @REALLYBUY
popup is asked. The payment and the unit spawn happen only on answer 1 (Yes).
So a No or Esc still makes the next Artillery 100 dearer, and the saved byte
`nation+0x1e` drifts up on every cancelled buy.

**Port:** `europe_purchase_commit` (`src/core/europe_dock.c`) bumps
`artillery_bought` (nation+0x1e) after the debit, so only a Yes raises the
price. `europe_purchase_open_confirm` no longer bumps it. Unit test:
`unit_europe` (@REALLYBUY block: open and No leave the counter alone, Yes
adds one). All goldens and `golden_ai_joint` stay byte-identical.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-1.

### DB-13 — Foreign-colony barter never debits the colony's offered goods
**DOS:** FUN_5f7a_020e (Jan de Witt barter at a foreign colony; raw
99019-99038). The counter-offer is sized from the colony's own stock
(`stock[c]`, capped at 100, then walked down to the sold cargo's value). On
"take the goods" the unit's hold is overwritten with the offered cargo
(FUN_281f_0cea / FUN_281f_0ca4) and the colony gets `stock[sold] += qty`. The
offered cargo is never subtracted from the colony, so every barter creates
goods and the AI colony keeps what it handed over.

**Port:** `colonies_foreign_trade_apply` (`src/core/colony_goods.c`) subtracts
the handed-over quantity from the colony's stock of the offered cargo on the
"take goods" arm, clamped at 0 (a War of Independence offer is forced up to
0x32 or 100 and can exceed the stock). The gold arm is unchanged. Unit test:
`unit_colonies` foreign-colony trade ("colony hands over the offered cargo").
All goldens and `golden_ai_joint` stay byte-identical.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-2.

### DB-14 — Tea Party picks the cargo without using its random draw
**DOS:** FUN_38fd_3dc8 (raw 64176-64200, asm 38fd:3f63-3fbc). The cargo
weights of the eligible goods are summed into a total, and
`rand(1, LOWORD(total))` is stored in `local_84` and never read. The walk then
subtracts each weight from the original total, so the pick is always the last
stocked, non-boycotted cargo with a nonzero weight. The intended code is the
usual roulette: `r = rand(); r -= w[c]; pick when r <= 0`. The seized or
boycotted good is predictable instead of weighted by traded tonnage.

**Port:** `ai_king_pick_dump_goods_cargo` (`src/core/ai_king.c`) keeps the one
draw and starts the walk from it, so each eligible cargo is picked in
proportion to its weight. If the weights sum to zero (no good with 100+ tons
traded), the one draw picks uniformly among the eligible goods instead (user
decision 2026-10-22): any stocked, not yet boycotted good in a coastal colony
stays a protest option, and only with none is the hike forced without one.
DOS took the last eligible cargo with a nonzero weight, or the first eligible
one when all weights were zero. Unit test: `unit_ai_king_core`
`dump_goods_pick_api` (weighted 7:9 over 200 seeds, zero-weight pick over 50
seeds, exactly one draw burned in both, campaign4 weights). All goldens and
`golden_ai_joint` stay byte-identical.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-3.

### DB-15 — @MORETHANTHREE cap skips the Rum Distiller
**DOS:** the work-assign validator thunk_FUN_1000_9808 (viceroy_overlays.c
60486-60496) tallies every other colonist by occupation and refuses a fourth
with `if (2 < count[job] && 9 < job) return @MORETHANTHREE`. Indoor jobs start
at 9 (Rum Distiller), so `9 <` exempts it and the colony screen lets the
player seat any number of Distillers. Every other indoor building is capped
at 3; `8 <` (or `>= 9`) was meant.

**Port:** `colonies_assign_workplace` (`src/core/colony_workers.c`) tests
`occupation > 8`, so the Distiller is capped at 3 like the other indoor jobs.
The AI placement has its own `< 3` check and is unaffected. Unit test:
`unit_colonies` school faculty + occupation cap (three distillers fit, the
fourth is refused). All goldens and `golden_ai_joint` stay byte-identical.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-4.

### DB-16 — withdrawn: King-gift ship loss tax cut is cut content
FUN_5fef_0352 (raw 99671-99690) reads the tax cut one table row too far, so
the King's gift Frigate never cuts the tax. The same block fills every token
of @KINGMERCY but never shows it, and VICEROY.EXE has no KINGMERCY tag string
(dead text), so the feature was removed on purpose. The user judged it an
intended removal (2026-10-22); the port keeps DOS (`units_royal_loss_tax_cut`,
`src/core/units_combat.c`). Was dos_bug_candidates_2026-10-22.md C-5.

### DB-17 — withdrawn: colony unit project at the unit cap, now tolerated
The 2026-10-22 fix passed every golden but was reverted at the user's call: the
port reproduces DOS again. See [tolerated_dos_bugs.md](tolerated_dos_bugs.md)
TB-47.

### DB-18 — withdrawn: leave-as gear at the unit cap, now tolerated
The 2026-10-22 fix passed every golden but was reverted at the user's call: the
port reproduces DOS again. See [tolerated_dos_bugs.md](tolerated_dos_bugs.md)
TB-47.

### DB-19 — withdrawn: colony birth at the unit cap, now tolerated
The 2026-10-22 fix passed every golden but was reverted at the user's call: the
port reproduces DOS again. See [tolerated_dos_bugs.md](tolerated_dos_bugs.md)
TB-47.

### DB-20 — withdrawn: crosses immigrant at the unit cap, now tolerated
The 2026-10-22 fix passed every golden but was reverted at the user's call: the
port reproduces DOS again. See [tolerated_dos_bugs.md](tolerated_dos_bugs.md)
TB-47.

### DB-21 — withdrawn: Brewster recruit at the unit cap, now tolerated
The 2026-10-22 fix passed every golden but was reverted at the user's call: the
port reproduces DOS again. See [tolerated_dos_bugs.md](tolerated_dos_bugs.md)
TB-47.

### DB-22 — Razing a capital clears alarm in the wrong tribe's dwellings
**DOS:** the capital-razed arm of FUN_5fef_1b0e (raw 101293-101298) zeroes the
per-Euro friction and attack words of every dwelling whose settlement `+2`
nation byte equals `DS:0x8d52`. The settlement byte holds a nation id (4..11);
`DS:0x8d52` holds the tribe index (0..7), the same word the alarm calls above
it receive. FUN_4cc6_00f2 (raw 80877) adds the 4 correctly. So the loop clears
the dwellings of tribe `idx - 4` (nothing at all for tribe indices 0..3), and
the razed tribe's own dwellings keep the tier clamp (0x20).

**Port:** `ai_diplo_indian_capital_surrender` (`src/core/ai_diplo.c`) compares
the dwelling's nation byte with the tribe index plus 4, so the razed tribe's
own dwellings are cleared. Unit test: `unit_ai_contact_core`
(capital-destroy surrender block, friction expectation changed from 0x20 to
0). All goldens, idle campaigns and `golden_ai_joint` stay byte-identical.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-11.

### DB-23 — Euro AI road-connect walk never restarts at the colony
**DOS:** FUN_5952_0000 (raw 93589-93685), the AI's inter-colony road-connect
helper, copies the colony's x and y into `local_18` / `local_1e` once (raw
93599-93600). Each candidate's distance test (raw 93610-93621) and its
flood-walk toward the other colony (raw 93632-93652) then step those two
locals in place and nothing resets them. After one candidate's walk fails
(no gap found, tile occupied, rival's improvement), the next candidate is
measured, and walked, from wherever the last walk stopped instead of from the
colony. The helper is meant to look for links out of this colony, so every
candidate should start at the colony.

**Port:** `ai_euro_5952_road_connect_0000` in `src/core/ai_euro_land.c` takes
the walk origin `cx` / `cy` from the colony at the top of each candidate.
Nothing else changes: the same AND distance test (see tolerated_dos_bugs_ai.md TB-15), the same
one-in-(count-1) roll and the same walk. All goldens, idle campaigns and
`golden_ai_joint` stay byte-identical (2026-10-22); no unit test pinned the
carry-over. The DOS evidence for the carry-over (america resync 129: Penobscot's
walk to Barbados ended at (44,39), so Providence was skipped, one `rng(0,2)`
fewer than a per-candidate reset draws; america2 resync 37) lies past the
enabled golden saves of both Americas archives.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-14.

### DB-24 — Founding join scores the forest penalty from stack residue
**DOS:** FUN_15eb_28c8 (raw 13119-13122) reads `local_32` for its forest
penalty, but assigns it only in the food-emergency arm (raw 13037). See
tolerated_dos_bugs_ai.md TB-16 for the tick. When a colony is founded, the founder's plot is picked
by the same function (the founding join, after FUN_364b_1ba8 raw 58088), and
the stack word there is the colony x left by FUN_137f_015e's pushed argument
(35 at Curacao (35,59), DOSBox seed-100 265 to 266), not a terrain class. The
forest -10 therefore never fires for the founder, whatever tile it considers.

**Port:** `colonies_found` in `src/core/colony.c` no longer sets the
residue before `colonies_seat_new_colonist`; the founding join scores the
candidate tile's own terrain class, like the human auto-assign path. The
residue setter `ai_euro_28c8_set_l32_residue` (`ai_euro.h`,
`ai_euro_colony_jobs.c`, `ai_contact_link_stubs.c`) is deleted. The tick's
residue (tolerated_dos_bugs_ai.md TB-16) is unchanged. All goldens, idle campaigns and
`golden_ai_joint` stay byte-identical (2026-10-22); no unit test pinned the
founder residue.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-15.

### DB-25 — Euro AI ship delivery tally is a signed byte
**DOS:** FUN_521d_20e6 counts the cargo a ship carries per goods type in a byte
array on the stack (`ADD byte [BP+SI-0xc6],AL`, viceroy_overlays.asm 136271-136273)
and reads the entries back with CBW (sign-extended). Two 100-lots of one cargo
read -56 and 256 reads 0. The colony-pick scoring (overflow term
`cap - stock - tally`) and the Horses warehouse gate then misjudge the load: a
full hold of one good looks like a negative or empty delivery.

**Port:** `ai_euro_20e6_delivery_tallies` (`src/core/ai_euro_ship.c`) adds the
amounts into an `int` without wrapping, so `ai_euro_20e6_delivery_colony_pick`
and its Horses gate read the plain sum. No test pins the wrap; all goldens,
idle campaigns and `golden_ai_joint` stay byte-identical. (The DOS evidence for
the wrap, america resyncs 105-119 and 151+, lies past the enabled golden saves of
both Americas archives and would need the old behaviour.)
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-22.

### DB-26 — Euro AI ship unload mask is reset inside the neighbour loop
**DOS:** the unload-mask scan in FUN_521d_20e6 sets `local_9c = 0` before the
8-neighbour loop (raw 89451) and again inside it (raw 89463), after the inset,
water and owner tests of each neighbour and before the continent-stance and
latitude gates. The mask is consumed once, after the loop (raw 89561-89566). So
only the last land neighbour passing the first tests counts: an earlier shore
that would unload founders, soldiers or scouts is wiped by a later land
neighbour, even one the stance or latitude gate then rejects.

**Port:** `ai_euro_20e6_unload_mask_snapshot` (`src/core/ai_euro_ship.c`) resets the mask
once before the loop and ORs in the bits of every eligible neighbour (the goal
continent still sets 0xffff). Unit test: `unit_regress_ai_tables`
`case_530_unload_mask_scan_and_order` ("a later rejected shore must keep the
earlier mask" fails on the DOS behaviour). All goldens, idle campaigns and
`golden_ai_joint` stay byte-identical.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-24.

### DB-27 — War-worthy check reads nation a's byte in nation b's clause
**DOS:** FUN_5bfb_10ec (is a war between nations a and b worthwhile, raw
97148-97211) tests each of the two nations against the focus nation `*(0x5398)`:
"met but not at peace, and the focus nation is not stronger" (focus word
`DS:-0x6be4` and focus byte `DS:-0x6bf0` both no larger than the nation's). The
a-clause (raw 97169-97170) compares both with a. The b-clause (raw 97172-97173)
compares the word with b but still reads the byte at `param_1` (a): a
copy-paste slip. The b-clause's byte test therefore ignores nation b.

**Port:** `ai_euro_10ec_war_worthy` (`src/core/ai_euro_land.c`) compares the
focus byte with b's byte in the b-clause (`census_pop_proxy[b]`). Its caller is
the AI-vs-AI war / treaty check in `ai_diplo.c`. No test pins the slip; all
goldens, idle campaigns and `golden_ai_joint` stay byte-identical.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-26.

### DB-28 — Euro AI explorer hop countdown stores a signed max
**DOS:** when an AI explorer hops to a plot-ring slot, FUN_521d_20e6 latches
the hop length into the unit's countdown byte (+0x3155) as `max(dx*4, dy*4)` on
the signed ring offsets (DS:0xc8 / 0xde; raw 90126-90138, duplicate body at raw
85300). For a slot whose offsets are both negative the result is negative: the
NW slot (-1,-1) stores -4, which the unsigned byte reads as 252. A slot with one
zero offset and one negative offset stores 0. The countdown is decremented once
per step (raw 89294) and the site scan is skipped while it is nonzero, so an
explorer sent north-west scans for sites only after about 252 steps instead of 4.
The hop length is a distance in tiles: the larger absolute value was meant.

**Port:** `ai_euro_20e6_ring_hop` (`src/core/ai_euro_land.c`) stores
`max(|dx|*4, |dy|*4)` in the hop scratch byte `sc[1]`; the countdown in
`ai_euro_move_scoring_gate` (`sc[1]--`) is unchanged. No test pins the signed
store; all goldens, idle campaigns and `golden_ai_joint` stay byte-identical.
The enabled goldens never complete a ring hop, so they do not exercise this
(the hop evidence in the code comment, america resync 92->93, is past the
enabled golden saves).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-27.

### DB-29 — Raid goods pick burns a dummy rand(0,200) for Muskets
**DOS:** the Brave raid goods picker in FUN_5fef_0f14 (raw 99826-99829, asm
5fef:1119-1135) draws a cargo with `rand(0,15)`. When the cargo is Muskets (0xf)
it calls `rand(0,200)` and computes `result - difficulty * i` into a register
that nothing reads. The draw has no effect on the pick, but it advances the
LCG, so every raid that rolls Muskets shifts all later random numbers by one.

**Port:** `ai_contact_raid_roll_stores_cargo` (`src/core/ai_contact_raid.c`)
makes no extra draw. All goldens, idle campaigns and `golden_ai_joint` stay
byte-identical, so no test in the suite reaches a Muskets roll with a later
draw that depends on it.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-31.

### DB-30 — Brave grudge latch not reset on other-tribe tiles
**DOS:** in the Brave step scorer FUN_4d56_021a (OVL13 0x66f-0x6ba) the
per-direction owner block sets the grudge latch `[bp-0x14]` on every arm except
one. Own or unowned tile: cleared. Euro-owned tile: `attitude >= 0x80`. Tile
owned by another tribe (owner >= 4): jumps to 0x6ff and skips the store. The
latch lives for the whole direction loop, so a direction next to another tribe's
land inherits the previous direction's grudge, sets the `0x40` grudge flag, and
can gain the Brave a loot-free attack.

**Port:** the owner block of `ai_021a_dir_tile` (`src/core/ai_native_021a.c`)
clears `grudge` on the owner >= 4 arm, so each direction decides its own grudge.
All goldens, idle campaigns and `golden_ai_joint` stay byte-identical.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-33.

### DB-31 — Tribe threat accumulator is a signed byte
**DOS:** FUN_4d56_152e (raw 81387-81534) keeps a per-Euro accumulator at
`indian+0x36+e`, a signed `char`. The threat arm subtracts the tribe's threat
score (doubled at a capital) from it with `*pcVar2 = *pcVar2 - (char)local_c`
(raw 81503), and later loops spend it: below -7 it adds 8 and raises the alarm
by 1 (heating), above 7 it subtracts 8 and lowers the alarm (cooling). A threat
of 128 or more truncates to a negative `char`, so the subtraction adds and the
accumulator goes positive: a very large threat cools the tribe instead of
heating it. It needs about 22 Dragoons or 13 Artillery next to a capital.

**Port:** the threat arm of `ai_indian_152e_village_growth` (`src/core/ai_indian.c`) saturates
the accumulator at -128 instead of wrapping it. Below that range the result is
identical to DOS. The alarm word itself already used the unwrapped threat. All
goldens, idle campaigns and `golden_ai_joint` stay byte-identical (no idle
campaign puts that much military next to a village).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-34.

### DB-32 — Convert winning a fight draws the promotion roll for nothing
**DOS:** the combat-promotion body FUN_5fef_172c (raw 100064-100154) gates on
FUN_281f_0c9a, which returns 0 (eligible) for profession 0x1b, the Indian
Convert. A winning Convert therefore reaches the Washington test and, without
Washington, draws `FUN_281f_04d4(1, strength pool)` (raw 100100-100104). Then
the ladder FUN_5fef_16ea maps 0x1b to 0x1b (raw 100040-100059), the
`iVar3 != iVar2` guard fails and nothing changes. The roll can never have an
effect: it only advances the shared RNG stream.

**Port:** `units_promote_on_win` (`src/core/units_combat.c`, the
`UNITS_JOB_CONVERT` arm) returns at once for a Convert, with no draw. Unit test:
`unit_units_types` `unit_promote_convert_rng_598` (now expects the same RNG
state with and without Washington). All goldens, idle campaigns and
`golden_ai_joint` stay byte-identical (2026-10-22). Earlier tracked as
port_bugs.md #598.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-39.

### DB-33 — Royal purchase adds the free-colonist count to the bells-this-turn word
**DOS:** FUN_43f7_1d42 (the King's purse-funded purchase, raw 74889-74890, asm
43f7:1e58) debits 1800 from `royal_money` and then adds the nation's
free-colonist count (`DS:0x9408 + nation`, a byte) to `nation + 0xe`. That
word is "bells produced this turn": it is zeroed at the start of each nation's
turn and increased by the bell spend, and nothing in the game reads it. The
add is a stray write on the wrong field.

**Port:** `ai_king_1d42_royal_purse` (`src/core/ai_king.c`) stops after the
`royal_money` debit and leaves `liberty_bells_last_turn` alone. Unit test
`unit_ai_king_core` (audience cut branch, 1d42 buy beat) checks that the word
keeps its seeded value. All goldens and `golden_ai_joint` pass. Only one golden
buy happens (`golden_idle_america`, save 31, free-colonist count 0), so the
goldens do not exercise a non-zero add.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-46.

### DB-34 — Warehouse spoilage under 2 tons is never removed
**DOS:** the warehouse clamp in FUN_364b_0688 (raw 57845-57868) computes
`loss = min(stock, excess)`, then `if (loss < 2) loss = 0`, and only then does
`stock -= loss`. The "under 2" test is meant to hide a trivial @SPOIL message,
but because it zeroes the loss before the subtract, a stock one ton over the
cap is never reduced. It sits at cap+1 forever.

**Port:** `colonies_apply_warehouse_spoilage` (`src/core/colony_goods.c`)
subtracts the real loss and only withholds the spoilage count (and so the
@SPOIL message) when the loss is under 2. Unit test: `unit_colonies`
("1-ton spoil not reported", "1-ton overflow removed"). All goldens, idle
campaigns and `golden_ai_joint` stay byte-identical.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-49.

### DB-35 — withdrawn: forest-clear lumber bonus follows DOS
FUN_479b_01a6 (raw ~76800) tests `FUN_281f_0754(colony) & 0x0a` on the
receiving colony's tile, so a Lumber Mill colony always gets the +1 step. A
2026-10-22 trial moved the test to the cleared tile (goldens passed), but the
user chose the DOS behaviour; the live DOS capture (+60 at Bahia) matches it.
The port (`units_pioneer_work_tick_w`, `src/core/units_pioneer.c`) keeps DOS.
Was dos_bug_candidates_2026-10-22.md C-52.

### DB-36 — Scored Go To fallback gives every due east/west step cost 1
**DOS:** the scored 8-neighbour fallback of `FUN_6662_0f74` (raw 104676-104690,
asm 6662:13d1-1420) prices a step 1 when both tiles have a road or city, or
when both have a river and `cur_x == cand_x`, or when `cur_y == cand_y`. The
last test is not inside the river clause, so every due east or west step costs
1 whether or not a river is involved. `FUN_465b_0000` (raw 75450-75465) and the
flood tier of `FUN_6662_00f2` use the intended grouping: river on both tiles
and (same x or same y).

**Port:** `units_greedy_next_step` (`src/core/units_move.c`) gives cost 1 on the
river clause only when both tiles have a river and the step is cardinal (same
x or same y); other steps cost `terr_cost * 3`. No unit test calls it; all
goldens, idle campaigns and `golden_ai_joint` stay byte-identical.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-53.

### DB-37 — Scored Go To fallback tests thirds of movement against tiles
**DOS:** the scored fallback of `FUN_6662_0f74` (raw 104700-104706) prices a
step `3` when `FUN_281f_090c(unit) < 2`, else `terr_cost * 3`. `090c` returns
the unit's movement in thirds (`DS:0x5234`, at least 3 for every type), so the
test never holds and the penalty is always `terr_cost * 3`. The flood tier of
`FUN_6662_00f2` makes the same choice correctly with `DS:0x5234 < 4` (raw
103896), i.e. only one-move units get the flat 3.

**Port:** `units_greedy_next_step` (`src/core/units_move.c`) tests
`units_max_mp() < 4`, so a one-move unit pays 3 per step like in the flood
tier. No unit test calls it; all goldens, idle campaigns and `golden_ai_joint`
stay byte-identical. The goldens reach this fallback only about once per
campaign, with a ship, so they barely exercise the change.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-53.

### DB-38 — Arrival tile ring search lets the +e hit overwrite the -e hit
**DOS:** the ring search in `FUN_48d3_048e` (raw 77830-77875) walks each ring
along its horizontal rows (`y = start_y -/+ e`) and then its vertical columns,
testing both sides with an inner loop `local_12 = -1, +1`. The found flag
`bVar1` is tested only by the outer loops, not by the pair loop. When both the
-e and the +e tile qualify, the second hit overwrites the first, so the ship
lands on the +e side although the -e tile was found first and is equally near.

**Port:** `units_spiral_place_hs_near` (`src/core/units_cargo.c`) takes the
first qualifying tile of the pair (-e before +e) and stops. Callers: AI
ship placement (`ai_euro_ship.c`), end-of-turn arrival (`game_loop_eot.c`).
`unit_units_ships` still passes; all goldens, idle campaigns and
`golden_ai_joint` stay byte-identical.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-55.

### DB-39 — Market pressure ratio truncated to 16 bits
**DOS:** the Europe price tick `FUN_38fd_0058` (raw ~58845 and ~58899) computes
the demand ratio `3 * Σledger / ledger[c]` with the 32-bit helper
`FUN_1d1d_0ec6`, then stores it in an `int` and subtracts it from the signed bid
byte in 16 bits (`local_6 - (int)lVar15`, and `local_52` in the cargo 1..4
pass). A ratio above 32767 wraps negative, so the sign of `bid - ratio` flips
and the pressure word moves the price the wrong way. The clamp-into-bid arm of
the same loop compares the full 32-bit value and is not affected.

**Port:** `europe_market_bid_sign` (`src/core/europe_harbor.c`), used by phases 2
and 3 of the market tick, takes the sign of `bid - ratio` without truncating.
It only differs once the ratio passes 32767 (late game, large `tons2` sums). No
test reaches that range; `golden_market_prices01`, all idle campaigns and
`golden_ai_joint` stay byte-identical.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-57.

### DB-40 — Voyage roll burns an RNG draw for a west-edge penalty that does not exist
**DOS:** the voyage-length roll `FUN_48d3_0002` (raw 77563-77588) rolls
RNG(1,100) and settles the result (1 or 2 turns). Then `if (param_2 < 3)` it
calls `FUN_281f_04d4(0,1)` (an RNG(0,1) draw) and `FUN_281f_07b4(nation, 5)` (the
Magellan test) and discards both. The pedia describes a west-edge delay that
Magellan removes, but the shipped code never uses either result. The dead
draw still advances the shared RNG stream whenever the crossing starts or
ends at a column below 3.

**Port:** `europe_voyage_turns_roll` (`src/core/europe_pool.c`) draws nothing for
`new_world_x < 3` (the argument is kept so callers stay unchanged). Every golden,
idle campaign and `golden_ai_joint` stay byte-identical, so no golden crossing
starts or ends west of column 3. The input-replay regression
`europe_buy_ship_and_sail` pinned the extra draw: its `output.sav` was
re-recorded from the port (only the 4 LCG-state bytes of the port extension
block differ).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-58.

### DB-41 — Tory uprising size comes from the last colony scored
**DOS:** the Tory uprising FUN_43f7_06a6 (raw 73829-73932; the crown's
fallback once its land pools are spent) scores every eligible human colony
into `local_18` (population x (100 - SoL) x 2 / 100 + difficulty + 1, minus the
attack strength on the colony tile; 0xffff when a crown unit is adjacent or no
adjacent land is free) and keeps the best in `local_16` (raw 73881-73882). The
spawn loop then runs `while (0 < (int)local_18)` (raw 73891): it uses the score
of the last colony scored, not the best one. The uprising lands at the best
colony with an unrelated number of Soldiers, and when the last colony was
disqualified (-1) nothing spawns and the uprising is silently cancelled.

**Port:** `ai_king_ref_tory_uprising` (`src/core/ai_king_ref.c`) spawns the
best colony's score. The port never reproduced the DOS read; recorded
2026-10-22 at the user's call. Not exercised by any golden.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-61.

### DB-42 — Open-continents counter is never reset
**DOS:** the per-nation census FUN_4962_0018 (raw 78316-78319, asm 4962:05d7)
adds to `DS:0x9650` once for every continent with `continent_tally_a > 7` on
which the nation has no colony. Every sibling counter is zeroed at census
entry; this one never is, so it grows with every census call of every nation
and stays large after the first turns. Its readers, the Euro AI ship logic in
FUN_521d_20e6 (raw 84955, 84982, 89498, 89527: `!= 0`; raw 86353, 89663:
`< 2`), therefore always see open land: ships keep founders and soldiers
aboard near home while the AI is small for the date, and the delivery pick
never gives under-defended colonies their bonus.

**Port:** `ai_euro_20e6_open_continents` (`src/core/ai_euro_land.c`), used by
`src/core/ai_euro_ship.c`, recomputes the nation's count on each call. The
port never reproduced the accumulation; recorded 2026-10-22 at the user's
call. The idle goldens (63 saves at most) end before the counts fall below 2,
so they do not show the difference.
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-62.

### DB-43 — Coastal forts fire on ships of nations never met
**DOS:** the colony battery FUN_364b_03f6 (raw 57082-57083) fires on the first
ship of each adjacent water tile when `(FUN_281f_0a38(colony, ship) & 0x40) == 0`
or the ship is a Privateer. An unmet pair has relation byte 0, so the fort
fires on it although the two nations were never at war. First contact between
Europeans needs land adjacency (port_bugs.md #545), so a ship passing a colony
never meets its owner; the Foreign Affairs report does not list the nation at
all. User-observed 2026-10-22 (campaign5): Jamestown's fortress fired on a
French Caravel; England and France had relation byte 0 through turn 123.

**Port:** `units_fort_fire_is_hostile` (`src/core/units_combat_resolve.c`)
also requires the MET bit (0x20) for a European ship. Privateers are still
fired on whatever the relation. Recorded 2026-10-22 at the user's call; unit
test `coastal_fort_naval_fire` (unit_units_core) covers the unmet case. No
golden changed.
**Status:** done (2026-10-22).

### DB-44 — Disband gather budget is 0 for a ship on trade-route orders
**DOS:** Disband's refusal test runs the pickup gather `FUN_1427_10be` (raw 8606-8680)
first. For a ship with orders 2 (trade route) the hold budget stays 0 unless the ship
is off the Europe lane and off every colony tile (`FUN_137f_03e4` < 0) and
`FUN_1427_101c` returns >= 0, so a trade-route ship lying in port gathers nothing,
passes the `FUN_281f_08bc(ship,2) > 1` test and can be disbanded while sentried units
sit on its tile (they stay behind). Read statically at the #1168 fix; accepted by the user 2026-10-23
(not a user-observed bug).

**Port:** `units_ship_has_sentry_boarder` (`src/core/units_cargo.c`) uses cargo minus
goods holds whatever the ship's orders, so a sentried unit on the tile always blocks
the disband.
**Status:** accepted (2026-10-23, port_bugs.md #1168).

### DB-45 — View-mode Shift-D steps the active-unit index back twice
**DOS:** `FUN_2b5a_199e` (raw 42609-42690) disbands the stack head at the cursor in View mode
(`DS:0x5390 != 0`). The pool compaction `FUN_1427_0824` already does
`if (DS:0x5392 != 0 && idx <= DS:0x5392) DS:0x5392--` (raw 7834-7836), then 199e's tail
runs its own `if (idx <= DS:0x5392) { if (idx < DS:0x5392) DS:0x5392--; ...}`. Disbanding a
unit at a lower pool index than the active one steps the active index back twice, so the
active unit silently becomes the one before it. Unit mode never double-steps (the deleted
unit IS the active one, the second test is false). Read statically at the #1173 part 4 port; accepted by the user 2026-10-23.

**Port:** the active unit is held by id (`units.selected_id`), so disbanding another unit
in View mode leaves the selection alone; `game_do_disband` (`src/core/game_dialogs.c`)
re-selects nothing in View mode.
**Status:** accepted (2026-10-23, port_bugs.md #1173).

### DB-46 — Pioneer work counter carries over to a new job
**DOS:** the Pioneer progress counter +0x315a is raised by the work bodies
FUN_479b_01a6 (raw 76753) and FUN_479b_0526 (raw 76886) and zeroed only on
completion (raw 76773 / 76899), Fortify (raw 42418) and colony entry (465b raw
76733). The order entries FUN_2b5a_123e (raw 42511) and FUN_2b5a_1454 (raw
42602) write only the order byte +0x314c. A Pioneer that chops 3 turns on one
tile, walks away and starts Clear Forest on another finishes next turn; road
progress likewise counts toward a later plow on the same tile.

**Port:** `units_pioneer_plow_w` / `units_pioneer_road_w`
(`src/core/units_pioneer.c`) zero the counter when the order changes to the
job, and the AI 20e6 road arm (`ai_euro_land.c`) does the same. The 5952
phantom Pioneer, which seeds 99 to finish in one call, latches its order first
so the entry leaves the 99 alone (DOS calls the work bodies directly there;
zeroing it broke every idle golden). All goldens and `golden_ai_joint` pass.
Unit case `unit_pioneer_dos_gates_2026_09_22` and the replay `map_unit_orders`
(its fixture preloaded the counters for an instant finish; re-blessed: the two
jobs now start at progress 1) were updated. Re-issuing the same job after an
interruption also restarts it.
**Status:** done (2026-10-23). Came from dos_bug_candidates_2026-10-22.md C-72 (port_bugs.md #620).

### DB-47 — Lost City Rumour counters are process-wide
**DOS:** FUN_65dd_0004 counts rumours explored in DS:0x1dc6 (raw 103459) and
Cibola finds in DS:0x1dc7 (raw 103546). Neither is in the save nor reset on New
Game, so they run for the life of the process: Fountain of Youth needs 4
rumours explored this process (raw 103497), Cibola stops after 7 finds this
process (raw 103524-103527), and a byte wrap at 256 rumours briefly shuts both
off again.

**Port:** `units_reset_state` (`src/core/units_move.c`) zeroes both on New
Game and Load; a port save's RTST chunk restores its own values after that
(`runtime_ext.c`). The rumour counter saturates at 255 (`units_combat.c`). All
goldens, the reload goldens and `golden_ai_joint` pass.
**Status:** done (2026-10-23). Came from dos_bug_candidates_2026-10-22.md C-73.

### DB-48 — Rumour trespass can lower the tribe's alarm
**DOS:** the @LOSTCITY8 arm of FUN_65dd_0004 (raw 103557-103559) computes the
alarm hit `rand(1,6) + (difficulty - skill + 1) * 5`, unclamped. A Seasoned
Scout (skill 2) at Discoverer gets -4..1, so the "the tribe is angered" rumour
lowers the alarm (FUN_281f_0d6c, raw 103650-103651; 0 skips it).

**Port:** `units_lcr_roll_outcome` (`src/core/units_combat.c`) clamps the hit at
0. All goldens and `golden_ai_joint` pass.
**Status:** done (2026-10-23). Came from dos_bug_candidates_2026-10-22.md C-74.

### DB-49 — Raid horse grab tests the rolled cargo's pile
**DOS:** the goods pick of the raid FUN_5fef_0f14 (raw 99822-99825) lets a
horseless tribe take Horses on its first try when
`stock[rolled cargo] > 0x34` and a coin flip comes up 0. The pile tested is the
rolled cargo's, not the Horses', so the tribe can steal Horses the colony
barely has and miss a large herd.

**Port:** `ai_contact_raid_roll_stores_cargo` (`src/core/ai_contact_raid.c`)
tests the Horses pile. All goldens and `golden_ai_joint` pass.
**Status:** done (2026-10-23). Came from dos_bug_candidates_2026-10-22.md C-75.

### DB-50 — Raid burn destroys the lowest owned tier
**DOS:** after picking an owned building, the raid burn of FUN_5fef_0f14 (raw
99856-99863) walks the predecessor column down to the lowest tier the colony
owns and clears that bit: a Church burns and its Cathedral stays, leaving an
upper tier without its base (original saves show such lone tiers).
Warehouse and Capitol are counted (colony +0x95 / +0x96), so the walk down to
their base row already takes one level off.

**Port:** `ai_contact_pick_raid_kind` (`src/core/ai_contact_raid.c`) keeps the walk
down, then for every other chain moves back up to the top owned tier, so the
Cathedral burns and the Church stays. All goldens and `golden_ai_joint` pass;
unit case in `unit_ai_contact_core` (port_bugs.md #924) updated.
**Status:** done (2026-10-23). Came from dos_bug_candidates_2026-10-22.md C-76.

### DB-51 — Refusing an AI's cash offer always reads as meek
**DOS:** FUN_5bfb_153e copies "MEEK" into the tone buffer `local_80` when it
shows @GIVECASH (raw 98017), and the WAR tag built at raw 98040-98041 appends
that buffer. A refusal after a cash offer is therefore always @WARMEEK, whatever
the encounter tone; without an offer the greeting tone is used.

**Port:** the @GIVECASH refusal (`AI_TALK_ST_GIVECASH`, `src/core/ai_diplo.c`)
uses the encounter tone, like the no-offer path. Text only; all goldens and
`golden_ai_joint` pass.
**Status:** done (2026-10-23). Came from dos_bug_candidates_2026-10-22.md C-78.

### DB-52 — AI treaty cancellation is silent (no CANCELTREATY text)
**DOS:** FUN_5bfb_13b0 (raw 97260-97316) shows tag DS:0x1898 "CANCELTREATY"
when an AI cancels a treaty, but no `COLONIZE/*.TXT` has that section, so the
dialog never opens and the human is not told (user-confirmed;
[popups_catalog.md](popups_catalog.md)). A data bug.

**Port:** `ai_diplo_13b0_treaty_tick` (`src/core/ai_diplo.c`) shows a
port-authored notice, "The X cancel their treaty with the Y.", when the section
is missing (MET pairs with the human only, as before); a GAME.TXT that has the
section still wins. All goldens and `golden_ai_joint` pass.
**Status:** done (2026-10-23). Came from dos_bug_candidates_2026-10-22.md C-79.

### DB-53 — Go to Port / route stop pickers lose the last colony at 20, 30 or 40 (port_bugs.md #1194)
**DOS:** FUN_647e_01c6 (raw 102368-102374) pages more than 10 destination colonies
into (n + 9) / 10 pages of 10 slots, but with the Europe row offered (slot 0, colony
k in slot k + 1) the colonies need n + 1 slots. For n = 20, 30 or 40 the last
colony's slot n is past the last page, so it is on no page and cannot be picked.

**Port:** `cheat_list_dest_page_math` (`src/core/cheat_list_dialog.c`) counts the
Europe slot, (n + e + 9) / 10 pages; only those three cases gain a last page holding
Previous and that colony. Every other page is DOS's. Test `smoke_popup_dialogs
dest_pager_is_dos`.
**Status:** done (2026-10-23, user-approved 2026-10-23).
