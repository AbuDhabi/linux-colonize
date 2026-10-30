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
- Unwanted DOS bug fixed in opencol but still reproduced in test binaries
  (`dos_buggy_behavior()`, so the goldens hold): [tolerated_dos_bugs.md](tolerated_dos_bugs.md).
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
worth modelling. Evidence: [archive](archive/dos_bugs_evidence_2026-10-23.md#db-1).

**Port:** main-menu load and in-game load both run the same clean init
(`game_apply_col1_save`, `src/core/game_loop_saveload.c`): the world is rebuilt
from the file alone, nothing from the previous game survives.
**Status:** done (2026-10-18).

### DB-2 — Save files omit live state, so save+reload diverges from continuous play
**DOS:** the save (`FUN_75c2_0288`) does not write several tables the turn loop reads
(Euro AI stance `DS:0x9870`, the AI goal tables `DS:0x98b0..`, the Indian rank table
`DS:0x917c`, ...), so a game played straight through and the same game saved and
reloaded take different paths. [Evidence](archive/dos_bugs_evidence_2026-10-24.md#db-2).

**Port:** the `RTST` chunk of the port extension block (`COLNXEXT`; records listed in
[savegame.md](savegame.md) "Port extension block", `src/core/runtime_ext.c`) carries
every such table plus the port's LCG; `col1_bridge_apply` restores it. DOS loads such a
save (it ignores the tail) and drops the block on re-save.
**Status:** done (2026-10-19). Guarded by the `golden_idle_*_reload` CTests
([details](archive/dos_bugs_evidence_2026-10-24.md#db-2)).

### DB-3 — Independence rename overflows the country-name field
**DOS:** on @OTHERGRANTED (FUN_3844_0442, raw 58602) DOS `strcpy`s the republic name
into `player.country_name` (24 bytes, DS `nation*0x34 + 0x5426`) with no bound. "United
States of America" is 24 characters, so its NUL zeroes `player+0x30`
(`lcr_case5_bonus_used`, `named_new_world`). [Evidence](archive/dos_bugs_evidence_2026-10-24.md#db-3).

**Port:** `turn_year_end_rival_independence` (`src/core/turn_year_end.c`) fills the
field without a NUL at 24 characters and leaves `player+0x30` alone; readers go through
`col1_player_country_name` (`src/core/col1_save.c`).
**Status:** done (2026-10-21).

### DB-4 — Euro AI border park reads an uninitialised stack word
**DOS:** the 0x46 border-park arm of FUN_521d_20e6 (raw 89011-89029) tests `local_ea`
(SS:e7c4), which 20e6 never writes first. A combat land unit beside a foreign colony
parks ('F') or wanders depending on what earlier frames, overlay-loader residue
included, left there (america seed-100 save 63 -> 64). [Evidence](archive/dos_bugs_evidence_2026-10-24.md#db-4).

**Port:** `ColonizeTurnContext.ai_euro_wander_stack_word` (`src/core/turn.h`) models
the deterministic writers only, reset at each nation's turn, so the park decision
depends on game state alone and survives a save/reload.
**Status:** done (2026-10-21; founding and hull writers 2026-10-23, port_bugs.md
#1135). The America idle archives end at save 63 and save 14 for this reason
([idle_america.md](idle_america.md), [idle_america2.md](idle_america2.md)).

### DB-5 — Euro AI Europe hire tail reads an unset stack word
**DOS:** FUN_521d_5d04 writes `local_24` (BP-0x24, the dock's colony demand
`FUN_281f_08bc(dock, 0xe)`) only inside the colony-demand block (raw 92818), which runs
only when `local_22 != 0` and the planning gates pass. The departing-ship loop reads it
whether or not that block ran (raw 93020): with a unit hired this pass, a ship whose
hold count (`DS:0x5237 + type*0xe`) equals `(char)local_24` takes the capacity-match arm
(hired flag cleared, ship exhausted). When the block was skipped, the byte is whatever
an earlier frame left at that stack address. Not traced; no recorded case where it
changed a result.

**Port:** `ai_euro_5d04_hire_tail_colony_demand` (`src/core/ai_euro_europe.c`) starts
`colony_demand_total` at 0 and passes it to `ai_euro_5d04_hire_tail_departing_ships`
through the tail ctx. No ship has 0 holds, so a skipped block never matches.
**Status:** done (port behaviour predates the entry; recorded 2026-10-21).

### DB-6 — Colony "out of ore" message tests net muskets against net tools
**DOS:** the Phase K colony messages in FUN_364b_0688 (raw 57696-57728) are seven
independent tests of the form `unmet[input] != 0 && 0b50(output) == 0` (0b50 = net stock
change). The @ORE arm (raw 57717-57724) instead tests
`unmet[ore] != 0 && 0b50(muskets) == 0b50(tools)`: a copy-paste slip from the @TOOLS arm
below it. So DOS stays silent when the blacksmiths ran the ore out and tools did not
grow while a gunsmith made muskets, and it nags whenever the two nets happen to be
equal, even when tools did grow.

**Port:** the @ORE arm in `turn_run_colony_production_w` (`src/core/turn_production.c`,
Phase K table) tests `unmet[ore] != 0 && net(tools) == 0`, like the other six arms.
Message chrome only: no stock, RNG or AI path reads it. Came from port_bugs.md #911.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-6).
**Status:** done (2026-10-21).

### DB-7 — Denounce Heresy tests Cotton Planter instead of Jesuit
**DOS:** Denounce Heresy (`thunk_FUN_1000_a594`, viceroy_overlays.asm 126507:
`IMUL BX,[BP+6],0x1c / CMP byte ptr [BX+0x315b],0x3`) gives the denouncer's side its
Jesuit bonus (weight ×2, alarm delta ×2) when the unit's profession byte is 3, the @JOB
row for Cotton Planter, not 0x18 (Jesuit Missionary) as the establish-mission body
(`thunk_FUN_1000_a5dc`) tests. A Missionary is never a Cotton Planter, so the bonus is
dead: a Jesuit denounces like a plain missionary, and a won mission is installed without
the Jesuit bit (0x10).

**Port:** `ai_contact_denounce_heresy` (`src/core/ai_contact_actions.c`) tests
`UNITS_JOB_MISSIONARY` (0x18), and a won mission takes the Jesuit bit on the establish
body's test (Jesuit or Brebeuf), so Brebeuf nations never get a plain mission from a
denounce. Came from port_bugs.md #558.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-7).
**Status:** done (2026-10-21).

### DB-8 — Accepting an AI goods demand always hands over Furs
**DOS:** in the Euro AI talk (FUN_5bfb_153e, OVL16), the @WANTSTUFF demand picks a cargo
and amount from the human colony next to the AI unit (`[bp-0xb2]`, amount capped to that
colony's stock and the receiver's warehouse room). When the player accepts, the transfer
(OVL16 asm 0x2995-0x29B2) indexes the colony stock rows with `[bp-0xa2]`, the loop
counter left at 4 by the euro rival-tally do-while, so it always moves Furs (cargo 4) by
the demanded amount, whatever the dialog named. With fewer Furs than the amount the
source stock underflows.

**Port:** the WANTSTUFF accept arm of `ai_talk_resume` (`src/core/ai_diplo.c`) moves
`want_cargo`, the cargo the dialog named; source stock floors at 0 as a guard.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-8).
**Status:** done (2026-10-21).

### DB-9 — Jesuit "leave as Colonist" row keyed on the colony's record slot
**DOS:** FUN_15eb_3454 (raw 13561-13565) refuses a Jesuit Missionary's "leave as
Colonist" row when `*(int*)0x8dc6 < 4` and the control byte
`0x543f + *(int*)0x8dc6 * 0x34` is human, but `DS:0x8dc6` is the colony's record index,
not a nation: the refusal hits only colonies whose record slot equals a human nation's
number.

**Port:** `colonies_eject_row_offered` (`src/core/colony_workers.c`) drops the arm: any
Jesuit is offered the Colonist row (user decision 2026-10-21). Came from port_bugs.md
#559. [Details and tests](archive/dos_bugs_evidence_2026-10-24.md#db-9).
**Status:** done (2026-10-21).

### DB-10 — Euro AI specialist pass can write profession 0xff
**DOS:** the ARM 1 specialist pass of FUN_5952_035e (raw 95905-95914) elects `local_8a`,
or the picked colonist's current occupation (`FUN_281f_0c0e`) when `local_8a` is
negative, and writes it as the profession through `FUN_281f_0cae` (= `FUN_15eb_0e8c`)
with no check. A pick whose occupation is idle stores profession 0xff, which is no @JOB
row. Rare: ARM 1 runs after all placement passes.

**Port:** the ARM 1 block in `src/core/ai_euro_colony_jobs.c` writes the profession only
when `elected >= 0`; otherwise the colonist keeps its old profession. The port's
colonist record reads -1 as "no profession", so the literal write would also change
other readers. Was known_divergences.md #780 (moved 2026-10-22: DOS is the buggy side).
**Status:** done (port behaviour predates the entry; recorded 2026-10-22).
Revisit only if a DOS save shows 0xff colonist profession bytes.

### DB-11 — @TRAINFAIL names whatever colony the last message named
**DOS:** in the school pass of FUN_364b_0688, a teacher with no eligible student raises
@TRAINFAIL (`0xde7`, raw 57542), whose text names `%STRING0`. Token 0 is set to the
colony name only later, in the graduation arm (`FUN_281f_0416(0, colony+2)`, raw 57552),
and nowhere earlier in 0688. So the message shows whatever token 0 held from an earlier
message: another colony's name or an unrelated string.

**Port:** the @TRAINFAIL arm of the school pass in `src/core/turn_production.c` fills
`%STRING0` with the current colony's name. Human-only message chrome; no stock, RNG or
AI path reads it. Was the last bullet of known_divergences.md #909 (moved 2026-10-22).
**Status:** done (port behaviour predates the entry; recorded 2026-10-22).

### DB-12 — Europe Artillery price counter rises when the purchase is cancelled
**DOS:** FUN_38fd_4b50 (raw 64870-64875) computes the Artillery price as base +
`nation[0x1e]*100`, then increments `nation[0x1e]` before the @REALLYBUY popup is asked.
The payment and the unit spawn happen only on answer 1 (Yes). So a No or Esc still makes
the next Artillery 100 dearer, and the saved byte `nation+0x1e` drifts up on every
cancelled buy.

**Port:** `europe_purchase_commit` (`src/core/europe_dock.c`) bumps `artillery_bought`
(nation+0x1e) after the debit, so only a Yes raises the price.
`europe_purchase_open_confirm` no longer bumps it.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-12).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-1.

### DB-13 — Foreign-colony barter never debits the colony's offered goods
**DOS:** FUN_5f7a_020e (Jan de Witt barter at a foreign colony; raw 99019-99038). The
counter-offer is sized from the colony's own stock (`stock[c]`, capped at 100, then
walked down to the sold cargo's value). On "take the goods" the unit's hold is
overwritten with the offered cargo (FUN_281f_0cea / FUN_281f_0ca4) and the colony gets
`stock[sold] += qty`. The offered cargo is never subtracted from the colony, so every
barter creates goods and the AI colony keeps what it handed over.

**Port:** `colonies_foreign_trade_apply` (`src/core/colony_goods.c`) subtracts the
handed-over quantity from the colony's stock of the offered cargo on the "take goods"
arm, clamped at 0 (a War of Independence offer is forced up to 0x32 or 100 and can
exceed the stock). The gold arm is unchanged.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-13).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-2.

### DB-14 — Tea Party picks the cargo without using its random draw
**DOS:** FUN_38fd_3dc8 (raw 64176-64200) stores `rand(1, total)` in `local_84` and never
reads it; the weight walk starts from the total, so the pick is always the last stocked,
non-boycotted cargo with a nonzero weight instead of a weighted roulette.

**Port:** `ai_king_pick_dump_goods_cargo` (`src/core/ai_king.c`) walks from the draw, so
each eligible cargo is picked in proportion to its weight; with all weights zero the
draw picks uniformly among the eligible goods (user decision 2026-10-22).
[Details and tests](archive/dos_bugs_evidence_2026-10-24.md#db-14).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-3.

### DB-15 — @MORETHANTHREE cap skips the Rum Distiller
**DOS:** the work-assign validator thunk_FUN_1000_9808 (viceroy_overlays.c 60486-60496)
tallies every other colonist by occupation and refuses a fourth with
`if (2 < count[job] && 9 < job) return @MORETHANTHREE`. Indoor jobs start at 9 (Rum
Distiller), so `9 <` exempts it and the colony screen lets the player seat any number of
Distillers. Every other indoor building is capped at 3; `8 <` (or `>= 9`) was meant.

**Port:** `colonies_assign_workplace` (`src/core/colony_workers.c`) tests
`occupation > 8`, so the Distiller is capped at 3 like the other indoor jobs. The AI
placement has its own `< 3` check and is unaffected.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-15).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-4.

### DB-16 — withdrawn: King-gift ship loss tax cut is cut content
FUN_5fef_0352 (raw 99671-99690) reads the tax cut one table row too far, so
the King's gift Frigate never cuts the tax. The user judged it an intended
removal (2026-10-22, [why](archive/dos_bugs_evidence_2026-10-23.md#db-16)); the port keeps DOS
(`units_royal_loss_tax_cut`, `src/core/units_combat.c`). Was dos_bug_candidates_2026-10-22.md C-5.

### DB-17 — withdrawn: colony unit project at the unit cap, now tolerated
### DB-18 — withdrawn: leave-as gear at the unit cap, now tolerated
### DB-19 — withdrawn: colony birth at the unit cap, now tolerated
### DB-20 — withdrawn: crosses immigrant at the unit cap, now tolerated
### DB-21 — withdrawn: Brewster recruit at the unit cap, now tolerated
Each of these five 2026-10-22 fixes passed every golden but was reverted at the
user's call; the port reproduces DOS again: [tolerated_dos_bugs.md](tolerated_dos_bugs.md) TB-47.

### DB-22 — Razing a capital clears alarm in the wrong tribe's dwellings
**DOS:** the capital-razed arm of FUN_5fef_1b0e (raw 101293-101298) zeroes the per-Euro
friction and attack words of every dwelling whose settlement `+2` nation byte equals
`DS:0x8d52`. The settlement byte holds a nation id (4..11); `DS:0x8d52` holds the tribe
index (0..7), the same word the alarm calls above it receive. FUN_4cc6_00f2 (raw 80877)
adds the 4 correctly. So the loop clears the dwellings of tribe `idx - 4` (nothing at
all for tribe indices 0..3), and the razed tribe's own dwellings keep the tier clamp
(0x20).

**Port:** `ai_diplo_indian_capital_surrender` (`src/core/ai_diplo.c`) compares the
dwelling's nation byte with the tribe index plus 4, so the razed tribe's own dwellings
are cleared. [Tests](archive/dos_bugs_evidence_2026-10-23.md#db-22).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-11.

### DB-23 — Euro AI road-connect walk never restarts at the colony
**DOS:** FUN_5952_0000 (raw 93589-93685) copies the colony's x / y into `local_18` /
`local_1e` once (raw 93599-93600). Each candidate's distance test and flood-walk step
them in place, so after a failed walk the next candidate is measured and walked from
where the last walk stopped, not from the colony.

**Port:** `ai_euro_5952_road_connect_0000` (`src/core/ai_euro_land.c`) restarts the walk
origin at the colony for each candidate; nothing else changes (TB-15's AND test stays).
[Evidence and tests](archive/dos_bugs_evidence_2026-10-24.md#db-23).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-14.

### DB-24 — Founding join scores the forest penalty from stack residue
**DOS:** FUN_15eb_28c8 (raw 13119-13122) reads `local_32` for its forest penalty but
assigns it only in the food-emergency arm (raw 13037). In the founding join (after
FUN_364b_1ba8, raw 58088) the stack word is the colony x, not a terrain class, so the
founder never gets the forest -10. The tick's residue is tolerated_dos_bugs_ai.md TB-16.

**Port:** `colonies_found` (`src/core/colony.c`) sets no residue; the founding join
scores the tile's own terrain class, like the human auto-assign path.
[Evidence and tests](archive/dos_bugs_evidence_2026-10-24.md#db-24).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-15.

### DB-25 — Euro AI ship delivery tally is a signed byte
**DOS:** FUN_521d_20e6 counts the cargo a ship carries per goods type in a byte array on
the stack (`ADD byte [BP+SI-0xc6],AL`, viceroy_overlays.asm 136271-136273) and reads the
entries back with CBW (sign-extended). Two 100-lots of one cargo read -56 and 256
reads 0. The colony-pick scoring (overflow term `cap - stock - tally`) and the Horses
warehouse gate then misjudge the load: a full hold of one good looks like a negative or
empty delivery.

**Port:** `ai_euro_20e6_delivery_tallies` (`src/core/ai_euro_ship.c`) adds the amounts
into an `int` without wrapping, so `ai_euro_20e6_delivery_colony_pick` and its Horses
gate read the plain sum.
[Evidence and tests](archive/dos_bugs_evidence_2026-10-23.md#db-25).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-22.

### DB-26 — Euro AI ship unload mask is reset inside the neighbour loop
**DOS:** the unload-mask scan in FUN_521d_20e6 sets `local_9c = 0` before the
8-neighbour loop (raw 89451) and again inside it (raw 89463), after the inset, water and
owner tests of each neighbour and before the continent-stance and latitude gates. The
mask is consumed once, after the loop (raw 89561-89566). So only the last land neighbour
passing the first tests counts: an earlier shore that would unload founders, soldiers or
scouts is wiped by a later land neighbour, even one the stance or latitude gate then
rejects.

**Port:** `ai_euro_20e6_unload_mask_snapshot` (`src/core/ai_euro_ship.c`) resets the
mask once before the loop and ORs in the bits of every eligible neighbour (the goal
continent still sets 0xffff). [Tests](archive/dos_bugs_evidence_2026-10-23.md#db-26).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-24.

### DB-27 — War-worthy check reads nation a's byte in nation b's clause
**DOS:** FUN_5bfb_10ec (is a war between nations a and b worthwhile, raw 97148-97211)
tests each of the two nations against the focus nation `*(0x5398)`: "met but not at
peace, and the focus nation is not stronger" (focus word `DS:-0x6be4` and focus byte
`DS:-0x6bf0` both no larger than the nation's). The a-clause (raw 97169-97170) compares
both with a. The b-clause (raw 97172-97173) compares the word with b but still reads the
byte at `param_1` (a): a copy-paste slip. The b-clause's byte test therefore ignores
nation b.

**Port:** `ai_euro_10ec_war_worthy` (`src/core/ai_euro_land.c`) compares the focus byte
with b's byte in the b-clause (`census_pop_proxy[b]`). Its caller is the AI-vs-AI war /
treaty check in `ai_diplo.c`. [Tests](archive/dos_bugs_evidence_2026-10-23.md#db-27).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-26.

### DB-28 — Euro AI explorer hop countdown stores a signed max
**DOS:** FUN_521d_20e6 (raw 90126-90138, duplicate at raw 85300) latches an explorer's
hop countdown (+0x3155) as `max(dx*4, dy*4)` on the signed ring offsets. The NW slot
stores -4 (byte 252), so an explorer sent north-west scans for sites only after about
252 steps instead of 4. The larger absolute value was meant.

**Port:** `ai_euro_20e6_ring_hop` (`src/core/ai_euro_land.c`) stores
`max(|dx|*4, |dy|*4)`. [Evidence and tests](archive/dos_bugs_evidence_2026-10-24.md#db-28).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-27.

### DB-29 — Raid goods pick burns a dummy rand(0,200) for Muskets
**DOS:** the Brave raid goods picker in FUN_5fef_0f14 (raw 99826-99829, asm
5fef:1119-1135) draws a cargo with `rand(0,15)`. When the cargo is Muskets (0xf) it
calls `rand(0,200)` and computes `result - difficulty * i` into a register that nothing
reads. The draw has no effect on the pick, but it advances the LCG, so every raid that
rolls Muskets shifts all later random numbers by one.

**Port:** `ai_contact_raid_roll_stores_cargo` (`src/core/ai_contact_raid.c`) makes no
extra draw. [Tests](archive/dos_bugs_evidence_2026-10-23.md#db-29).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-31.

### DB-30 — Brave grudge latch not reset on other-tribe tiles
**DOS:** in the Brave step scorer FUN_4d56_021a (OVL13 0x66f-0x6ba) the per-direction
owner block sets the grudge latch `[bp-0x14]` on every arm except one. Own or unowned
tile: cleared. Euro-owned tile: `attitude >= 0x80`. Tile owned by another tribe
(owner >= 4): jumps to 0x6ff and skips the store. The latch lives for the whole
direction loop, so a direction next to another tribe's land inherits the previous
direction's grudge, sets the `0x40` grudge flag, and can gain the Brave a loot-free
attack.

**Port:** the owner block of `ai_021a_dir_tile` (`src/core/ai_native_021a.c`) clears
`grudge` on the owner >= 4 arm, so each direction decides its own grudge.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-30).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-33.

### DB-31 — Tribe threat accumulator is a signed byte
**DOS:** FUN_4d56_152e (raw 81503) subtracts the tribe's threat score from the signed
`char` accumulator `indian+0x36+e` as `(char)local_c`. A threat of 128+ (about 22
Dragoons or 13 Artillery next to a capital) truncates negative, so the accumulator rises
and the tribe cools instead of heating.

**Port:** the threat arm of `ai_indian_152e_village_growth` (`src/core/ai_indian.c`)
saturates at -128; below that it matches DOS. [Details and tests](archive/dos_bugs_evidence_2026-10-24.md#db-31).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-34.

### DB-32 — Convert winning a fight draws the promotion roll for nothing
**DOS:** the combat-promotion body FUN_5fef_172c (raw 100064-100154) gates on
FUN_281f_0c9a, which returns 0 (eligible) for profession 0x1b, the Indian Convert. A
winning Convert therefore reaches the Washington test and, without Washington, draws
`FUN_281f_04d4(1, strength pool)` (raw 100100-100104). Then the ladder FUN_5fef_16ea
maps 0x1b to 0x1b (raw 100040-100059), the `iVar3 != iVar2` guard fails and nothing
changes. The roll can never have an effect: it only advances the shared RNG stream.

**Port:** `units_promote_on_win` (`src/core/units_combat.c`, the `UNITS_JOB_CONVERT`
arm) returns at once for a Convert, with no draw. Earlier tracked as port_bugs.md #598.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-32).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-39.

### DB-33 — Royal purchase adds the free-colonist count to the bells-this-turn word
**DOS:** FUN_43f7_1d42 (the King's purse-funded purchase, raw 74889-74890, asm
43f7:1e58) debits 1800 from `royal_money` and then adds the nation's free-colonist count
(`DS:0x9408 + nation`, a byte) to `nation + 0xe`. That word is "bells produced this
turn": it is zeroed at the start of each nation's turn and increased by the bell spend,
and nothing in the game reads it. The add is a stray write on the wrong field.

**Port:** `ai_king_1d42_royal_purse` (`src/core/ai_king.c`) stops after the
`royal_money` debit and leaves `liberty_bells_last_turn` alone.
[Evidence and tests](archive/dos_bugs_evidence_2026-10-23.md#db-33).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-46.

### DB-34 — Warehouse spoilage under 2 tons is never removed
**DOS:** the warehouse clamp in FUN_364b_0688 (raw 57845-57868) computes
`loss = min(stock, excess)`, then `if (loss < 2) loss = 0`, and only then does
`stock -= loss`. The "under 2" test is meant to hide a trivial @SPOIL message, but
because it zeroes the loss before the subtract, a stock one ton over the cap is never
reduced. It sits at cap+1 forever.

**Port:** `colonies_apply_warehouse_spoilage` (`src/core/colony_goods.c`) subtracts the
real loss and only withholds the spoilage count (and so the @SPOIL message) when the
loss is under 2. [Tests](archive/dos_bugs_evidence_2026-10-23.md#db-34).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-49.

### DB-35 — withdrawn: forest-clear lumber bonus follows DOS
FUN_479b_01a6 (raw ~76800) tests `FUN_281f_0754(colony) & 0x0a` on the
receiving colony's tile, so a Lumber Mill colony always gets the +1 step. The
user chose DOS over a 2026-10-22 trial fix ([details](archive/dos_bugs_evidence_2026-10-23.md#db-35));
the port (`units_pioneer_work_tick_w`, `src/core/units_pioneer.c`) keeps DOS.
Was dos_bug_candidates_2026-10-22.md C-52.

### DB-36 — Scored Go To fallback gives every due east/west step cost 1
**DOS:** the scored 8-neighbour fallback of `FUN_6662_0f74` (raw 104676-104690, asm
6662:13d1-1420) prices a step 1 when both tiles have a road or city, or when both have a
river and `cur_x == cand_x`, or when `cur_y == cand_y`. The last test is not inside the
river clause, so every due east or west step costs 1 whether or not a river is involved.
`FUN_465b_0000` (raw 75450-75465) and the flood tier of `FUN_6662_00f2` use the intended
grouping: river on both tiles and (same x or same y).

**Port:** `units_greedy_next_step` (`src/core/units_move.c`) gives cost 1 on the river
clause only when both tiles have a river and the step is cardinal (same x or same y);
other steps cost `terr_cost * 3`.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-36).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-53.

### DB-37 — Scored Go To fallback tests thirds of movement against tiles
**DOS:** the scored fallback of `FUN_6662_0f74` (raw 104700-104706) prices a step `3`
when `FUN_281f_090c(unit) < 2`, else `terr_cost * 3`. `090c` returns the unit's movement
in thirds (`DS:0x5234`, at least 3 for every type), so the test never holds and the
penalty is always `terr_cost * 3`. The flood tier of `FUN_6662_00f2` makes the same
choice correctly with `DS:0x5234 < 4` (raw 103896), i.e. only one-move units get the
flat 3.

**Port:** `units_greedy_next_step` (`src/core/units_move.c`) tests `units_max_mp() < 4`,
so a one-move unit pays 3 per step like in the flood tier.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-37).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-53.

### DB-38 — Arrival tile ring search lets the +e hit overwrite the -e hit
**DOS:** the ring search in `FUN_48d3_048e` (raw 77830-77875) walks each ring along its
horizontal rows (`y = start_y -/+ e`) and then its vertical columns, testing both sides
with an inner loop `local_12 = -1, +1`. The found flag `bVar1` is tested only by the
outer loops, not by the pair loop. When both the -e and the +e tile qualify, the second
hit overwrites the first, so the ship lands on the +e side although the -e tile was
found first and is equally near.

**Port:** `units_spiral_place_hs_near` (`src/core/units_cargo.c`) takes the first
qualifying tile of the pair (-e before +e) and stops. Callers: AI ship placement
(`ai_euro_ship.c`), end-of-turn arrival (`game_loop_eot.c`).
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-38).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-55.

### DB-39 — Market pressure ratio truncated to 16 bits
**DOS:** the Europe price tick `FUN_38fd_0058` (raw ~58845 and ~58899) computes the
demand ratio `3 * Σledger / ledger[c]` with the 32-bit helper `FUN_1d1d_0ec6`, then
stores it in an `int` and subtracts it from the signed bid byte in 16 bits
(`local_6 - (int)lVar15`, and `local_52` in the cargo 1..4 pass). A ratio above 32767
wraps negative, so the sign of `bid - ratio` flips and the pressure word moves the price
the wrong way. The clamp-into-bid arm of the same loop compares the full 32-bit value
and is not affected.

**Port:** `europe_market_bid_sign` (`src/core/europe_harbor.c`), used by phases 2 and 3
of the market tick, takes the sign of `bid - ratio` without truncating. It only differs
once the ratio passes 32767 (late game, large `tons2` sums).
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-39).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-57.

### DB-40 — Voyage roll burns an RNG draw for a west-edge penalty that does not exist
**DOS:** the voyage-length roll `FUN_48d3_0002` (raw 77563-77588) rolls RNG(1,100) and
settles the result (1 or 2 turns). Then `if (param_2 < 3)` it calls `FUN_281f_04d4(0,1)`
(an RNG(0,1) draw) and `FUN_281f_07b4(nation, 5)` (the Magellan test) and discards both.
The pedia describes a west-edge delay that Magellan removes, but the shipped code never
uses either result. The dead draw still advances the shared RNG stream whenever the
crossing starts or ends at a column below 3.

**Port:** `europe_voyage_turns_roll` (`src/core/europe_pool.c`) draws nothing for
`new_world_x < 3` (the argument is kept so callers stay unchanged). The input-replay
regression `europe_buy_ship_and_sail` was re-recorded from the port.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-40).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-58.

### DB-41 — Tory uprising size comes from the last colony scored
**DOS:** the Tory uprising FUN_43f7_06a6 (raw 73829-73932; the crown's fallback once its
land pools are spent) scores every eligible human colony into `local_18` (population x
(100 - SoL) x 2 / 100 + difficulty + 1, minus the attack strength on the colony tile;
0xffff when a crown unit is adjacent or no adjacent land is free) and keeps the best in
`local_16` (raw 73881-73882). The spawn loop then runs `while (0 < (int)local_18)` (raw
73891): it uses the score of the last colony scored, not the best one. The uprising
lands at the best colony with an unrelated number of Soldiers, and when the last colony
was disqualified (-1) nothing spawns and the uprising is silently cancelled.

**Port:** `ai_king_ref_tory_uprising` (`src/core/ai_king_ref.c`) spawns the best
colony's score. The port never reproduced the DOS read; recorded 2026-10-22 at the
user's call. [Tests](archive/dos_bugs_evidence_2026-10-23.md#db-41).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-61.

### DB-42 — Open-continents counter is never reset
**DOS:** the per-nation census FUN_4962_0018 (raw 78316-78319, asm 4962:05d7) adds to
`DS:0x9650` once for every continent with `continent_tally_a > 7` on which the nation
has no colony. Every sibling counter is zeroed at census entry; this one never is, so it
grows with every census call of every nation and stays large after the first turns. Its
readers, the Euro AI ship logic in FUN_521d_20e6 (raw 84955, 84982, 89498, 89527:
`!= 0`; raw 86353, 89663: `< 2`), therefore always see open land: ships keep founders
and soldiers aboard near home while the AI is small for the date, and the delivery pick
never gives under-defended colonies their bonus.

**Port:** `ai_euro_20e6_open_continents` (`src/core/ai_euro_land.c`), used by
`src/core/ai_euro_ship.c`, recomputes the nation's count on each call. The port never
reproduced the accumulation; recorded 2026-10-22 at the user's call.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-42).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-62.

### DB-43 — Coastal forts fire on ships of nations never met
**DOS:** the colony battery FUN_364b_03f6 (raw 57082-57083) fires on the first ship of
each adjacent water tile when `(FUN_281f_0a38(colony, ship) & 0x40) == 0` or the ship is
a Privateer. An unmet pair has relation byte 0, so the fort fires on it although the two
nations were never at war. First contact between Europeans needs land adjacency
(port_bugs.md #545), so a ship passing a colony never meets its owner; the Foreign
Affairs report does not list the nation at all. User-observed 2026-10-22 (campaign5,
[details](archive/dos_bugs_evidence_2026-10-23.md#db-43)).

**Port:** `units_fort_fire_is_hostile` (`src/core/units_combat_resolve.c`) also requires
the MET bit (0x20) for a European ship. Privateers are still fired on whatever the
relation. Recorded 2026-10-22 at the user's call.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-43).
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
**DOS:** the Pioneer progress counter +0x315a (raised by FUN_479b_01a6 / 0526) is zeroed
only on completion, Fortify and colony entry; the order entries (FUN_2b5a_123e / 1454)
write only the order byte. A Pioneer that chops 3 turns on one tile and starts Clear
Forest elsewhere finishes next turn.
A move (FUN_465b_0000) does not touch it either, so a Pioneer that keeps its
Clear/Plow or Road order while it walks finishes the job on the new tile at once.

**Port:** `units_pioneer_plow_w` / `units_pioneer_road_w` (`src/core/units_pioneer.c`)
and the AI 20e6 road arm zero the counter when the order changes to the job.
`units_move_commit_step` (`src/core/units_move.c`) zeroes a Pioneer's counter on
every step (2026-10-30, fandom wiki "pre-charge"). Two Pioneers on one forest are
not covered: the second keeps its own counter and plows at once once the first
clears the tile, as in DOS.
[Details and tests](archive/dos_bugs_evidence_2026-10-24.md#db-46).
**Status:** done (2026-10-23). Came from dos_bug_candidates_2026-10-22.md C-72 (port_bugs.md #620).

### DB-47 — Lost City Rumour counters are process-wide
**DOS:** FUN_65dd_0004 counts rumours explored in DS:0x1dc6 (raw 103459) and Cibola
finds in DS:0x1dc7 (raw 103546). Neither is in the save nor reset on New Game, so they
run for the life of the process: Fountain of Youth needs 4 rumours explored this process
(raw 103497), Cibola stops after 7 finds this process (raw 103524-103527), and a byte
wrap at 256 rumours briefly shuts both off again.

**Port:** `units_reset_state` (`src/core/units_move.c`) zeroes both on New Game and
Load; a port save's RTST chunk restores its own values after that (`runtime_ext.c`). The
rumour counter saturates at 255 (`units_combat.c`).
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-47).
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
**DOS:** the goods pick of the raid FUN_5fef_0f14 (raw 99822-99825) lets a horseless
tribe take Horses on its first try when `stock[rolled cargo] > 0x34` and a coin flip
comes up 0. The pile tested is the rolled cargo's, not the Horses', so the tribe can
steal Horses the colony barely has and miss a large herd.

**Port:** `ai_contact_raid_roll_stores_cargo` (`src/core/ai_contact_raid.c`) tests the
Horses pile. [Tests](archive/dos_bugs_evidence_2026-10-23.md#db-49).
**Status:** done (2026-10-23). Came from dos_bug_candidates_2026-10-22.md C-75.

### DB-50 — Raid burn destroys the lowest owned tier
**DOS:** after picking an owned building, the raid burn of FUN_5fef_0f14 (raw
99856-99863) walks the predecessor column down to the lowest tier the colony owns and
clears that bit: a Church burns and its Cathedral stays, leaving an upper tier without
its base (original saves show such lone tiers). Warehouse and Capitol are counted
(colony +0x95 / +0x96), so the walk down to their base row already takes one level off.

**Port:** `ai_contact_pick_raid_kind` (`src/core/ai_contact_raid.c`) keeps the walk
down, then for every other chain moves back up to the top owned tier, so the Cathedral
burns and the Church stays. [Tests](archive/dos_bugs_evidence_2026-10-23.md#db-50).
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

### DB-54 — Opening the colony jobs menu stamps plots and resets the job counter (port_bugs.md #1205)
**DOS:** FUN_2f2b_348c's jobs-mode build-up (param_1 == 0, raw 50599-50675) computes
each field job's figure by really seating the colonist: FUN_15eb_28c8's final
FUN_15eb_06d2 seat runs with DS:0x34d cleared (raw 13138-13147), then a re-seat on his
own plot (raw 50666-50668), and every job change goes through FUN_15eb_1068, which
zeroes the colony +0x60 turns-in-job nibble (raw 11256-11258). So merely opening the
menu, Escape included, stamps the colony's owner nibble and mask 0x10 on each elected
plot and zeroes the colonist's job counter (the teaching loop's progress).
DOSBox-confirmed 2026-10-23.

**Port:** `game_colony_open_jobs` (`src/core/game_loop_colony.c`) only opens the popup;
the estimates come from the read-only `ai_euro_jobs_menu_estimate`. A real pick still
seats and resets through the normal assign path.
[Trace and tests](archive/dos_bugs_evidence_2026-10-23.md#db-54).
**Status:** done (2026-10-23, user-approved 2026-10-23).

### DB-55 — Escape in the Incite Indians target menu picks the English (port_bugs.md #1157)
**DOS:** FUN_4d56_417e (raw 83595-83617) runs the @INDIANWARPATH menu through
FUN_291f_016a = FUN_6f74_2580, which returns -1 on Esc (LAB_6f74_2b9a). The caller
clamps it (`cmp ax,1; jnl; mov ax,1; dec`, 4d56:436c-437a; rows are nation + 1), so Esc
reads as nation 0. With no English contact DOS shows @NOCONTACT; otherwise
@INDIANWARPATH2 offers to drive the English away for the price (8800 in the trace), and
Pay sets the tribe's tension against England to 100. There is no self check, so a human
England player's Esc offers to incite the tribe against himself. DOSBox-confirmed
2026-10-23. [Trace](archive/dos_bugs_evidence_2026-10-23.md#db-55).

**Port:** Esc cancels: the `AI_CONTACT_INCITE_STAGE_TARGET` arm in
`src/core/ai_contact_actions.c` enqueues the confirm only when the result is not
cancelled, and `ai_contact_enqueue_incite_confirm` (`src/core/ai_contact_demand.c`)
refuses target == the inciter.
**Status:** done (2026-10-23, user-approved 2026-10-23).

### DB-56 — Europe transit-box releases clear orders and the Expected box sails a harbor ship (port_bugs.md #1159)
**DOS:** FUN_38fd_3228 mode 9 zeroes a Bound / Expected ship's orders byte +0x314c
(38fd:326f) before it compares drop and start zone, so any release over its own box, a
plain click included, cancels its orders and trade route; mode 8 sails a harbor ship
dropped on either transit box. DOSBox-confirmed 2026-10-23. [Details](archive/dos_bugs_evidence_2026-10-24.md#db-56).

**Port:** the UI_DRAG_EUROPE_*_SHIP drop handler (`src/core/game_loop_colony.c`) acts
only when the box changes (`europe_reverse_transit`, `src/core/europe_harbor.c`); a
harbor ship sails only from a drop on Bound for the New World.
**Status:** done (2026-10-23, user-approved 2026-10-23).

### DB-57 — Hall of Fame 6th-place retire never shows the new entry (port_bugs.md #1219)
**DOS:** FUN_41f2_0f56 loads 5 records; a Retire entry that outranks none takes the catch-all
slot 5 (local_156). The presenter numbers the last line local_6 + 1 = "6." and draws it in
DS:0x831 hilite when local_6 == local_156 (asm 41f2:12b2-12ef), but reads every field from the
loop counter's slot (SI at 41f2:12ca / 1336 / 13a4), so line 5 shows the old 5th-place entry
labelled "6." and the new entry is never visible.

**Port:** `reports_render_hall_of_fame` (`src/core/reports_score.c`) draws the retired entry's
own fields on that "6." line (`ColonizeGameState.hof_retired`, set in
`src/core/game_loop_saveload.c`); numbering and hilite stay DOS's. Test `unit_reports
hall_of_fame_layout_is_dos`.
**Status:** done (2026-10-24, user-approved 2026-10-24).

### DB-58 — Unload all cargo warns about a boycott only in slot 0
**DOS:** row 3 of the Europe ship menu, FUN_38fd_2bfe (raw 60866-60888), walks the hold
slots from the last down to 0, skips a boycotted one (FUN_38fd_05e8) and sells the rest.
The notice flag `local_62` is cleared at the top of every pass (raw 60872, asm
38fd:2d8c) instead of once before the loop, so @SOMEBOYCOTT (0x1027) shows only when
slot 0 is boycotted; a boycotted hold in any other slot stays aboard without a word.

**Port:** `game_europe_unload_all` (`src/core/game_dialogs.c`) keeps the flag set once
any hold was boycotted. Test `unit_stage_seams test_europe_unload_all_boycott_any_slot`
(mutation-checked). Full ctest and `make golden` green with the fix.
**Status:** done (2026-10-24, user-approved 2026-10-24). Found in the 2026-10-24
post-sweep scan (from port_bugs.md #1176).

### DB-59 — Build list drops the current project by an unrelated unit's type
**DOS:** FUN_15eb_3650's tail (raw 13741-13746, asm 15eb:3889-38b4) lists the colony's
current project (+0x94) whatever the gates said, unless `byte [code * 0x1c + 0x3146]
== 1` and the colony owns `local_e`. That byte is the type of unit record #code: the
build code is used as an index into the unit array. So whether a just-built current
project shows as its "(COMPLETE)" row (FUN_2f2b_5a68) depends on which unit happens to
sit in that slot.

**Port:** `colonies_build_keeps_current` (`src/core/colony_build.c`) always keeps the
current project; `ColoniesBuildableOpts.units`, added only for that read, is gone. The
Euro AI lists with +0x94 = -1 (raw 95711), so AI behaviour is unchanged. Test
`unit_colonies unit_build_list_keeps_current`. Full ctest and `make golden` green.
**Status:** done (2026-10-24, user-approved 2026-10-24). Found in the 2026-10-24
post-sweep scan (from port_bugs.md #1218).

### DB-60 — Construction list pager never resets its default-row flag
**DOS:** FUN_2f2b_5bd2 clears `local_8` once (raw 52575) and every page-0 pass of a
two-page list sets it, so after the first showing neither fallback arm (raw 52620-52629,
DB-63) runs and a turned page without the current project bars its first row.

**Port:** `colony_screen_construction_next_page` (`src/core/colony_screen_popups.c`)
treats every page alike; the page is its own field (`ColonyScreenView.construction_page`).
[Details and tests](archive/dos_bugs_evidence_2026-10-24.md#db-60).
**Status:** done (2026-10-24, user-approved 2026-10-24). Found in the 2026-10-24
post-sweep scan (from port_bugs.md #1215).

### DB-61 — Jobs / leave-as menu reads the occupation before setting it
**DOS:** FUN_2f2b_348c's occupation row (raw 50746) highlights when `local_136 ==
local_8`, but `local_136` is first written at raw 50799, so the first page-0 draw tests
a stale stack word.

**Port:** the eject rows in `src/core/colony_screen_popups.c` use the occupation every
later DOS draw sees. The port never reproduced the stale read; recorded 2026-10-24 at
the user's call.
**Status:** done (2026-10-24). Found in the 2026-10-24 post-sweep scan.

### DB-62 — Colony-screen `T` cheat refunds a departed colonist's goods
**DOS:** the `T` cheat (FUN_2f2b_6372 tail, 2f2b:6820) raises the head count first,
then calls FUN_15eb_1068(old, 0). 1068's refund at its top (raw 11229-11247, through
FUN_15eb_0e18) reads the occupation byte at the new index, a leftover of whichever
colonist last held that slot, and credits the colony the goods of that stale occupation.

**Port:** `colonies_cheat_add_colonist` (`src/core/colony.c`) pays no refund; the port
keeps no byte past the head count. Never reproduced; recorded 2026-10-24 at the user's
call.
**Status:** done (2026-10-24). Found in the 2026-10-24 post-sweep scan.

### DB-63 — Construction list fallback bars a row id that does not exist
**DOS:** on a page without the current project FUN_2f2b_5bd2 calls `FUN_291f_08ec` /
`033c` with 0x65 (page 0, raw 52620-52624) or 100 (page 1, raw 52627-52629): the
"(More)" ids 0x63 / 0x62 (raw 52640-52645) with the row offset + 2 added again. No row
matches, so DOS shows no bar.

**Port:** `colony_screen_open_construction` (`src/core/colony_screen.c`) and
`colony_screen_construction_next_page` bar that "(More)" row and draw it in the hilite
ink. [Evidence and tests](archive/dos_bugs_evidence_2026-10-24.md#db-63).
**Status:** done (2026-10-24, user-approved 2026-10-24). Found while filing DB-60.

### DB-64 — AI colony tick taken over a human colony after a phantom road (TB-14)
**DOS:** the road a Euro AI colony builds through a phantom Pioneer (FUN_5952_035e,
TB-14) completes in FUN_479b_0526, whose FUN_281f_0614 call (raw 76902) rebinds
`DS:0x8542` to the colony nearest the road of ANY nation (filter -1). When that is a
human colony, the AI charges it 20 tools and runs the rest of its colony tick there:
job passes, idle sweep, build cascade. Playtest campaign3 C3-O3: Quebec and Guadeloupe
had their workers re-seated and their build queue replaced three times (1682, 1702,
1771).

**Port:** `ai_euro_0614_payer` (`src/core/ai_euro_land.c`) keeps the ticking AI colony
when the rebound colony belongs to a human-controlled nation (`player[n].control == 0`).
AI-on-AI rebinds still follow TB-14. Test: `unit_ai_euro_5952_build`
`case_0614_rebind_spares_human_colony`. port_bugs.md #1263.
**Status:** done (2026-10-25, user-approved 2026-10-25).

### DB-65 — Foreign units visible far outside the viewer's sight
**DOS:** the map draw (FUN_2f2b_6372) shows a foreign unit while the viewer's bit
is set in its vis byte (`col1_vis_mask`). A move resets that byte through
FUN_1427_0c9a, which ORs in the destination tile's owner nibble (465b_06dc, raw
75766). On open ground that nibble is the first nation to reveal the tile
(FUN_13f1_000a) or the last one to have a unit there. So any unit stepping onto
ground the human explored first shows up anywhere on the map. The bits also
stick: a unit is never hidden again until it moves, and FUN_465b_0673 ORs extra
bits into the units left behind on the tile a mover leaves.

**Port:** the sim and the save keep the DOS bits (AI and goldens use them). For
display only, `units_shown_to` (`src/core/units_map.c`) shows a foreign unit
when it stood on a tile in the viewer's live sight at some point this turn. That
sight is the viewed nation's units at `units_sight_radius` with the
`map_sight_each` domain gate, plus colonies at radius 1.
`game_render_map_composite` feeds the set every frame (`units_seen_note`). It
empties when the turn number or the viewed nation changes, and on load
(`units_seen_reset`). The main map, the minimap and the sidebar's tile stack all
use it, and so does the Go To destination check (`units_set_goto_w`), so a
refusal cannot reveal a hidden unit. Before the first note (unit tests) it
falls back to the DOS bit.
**Status:** done (2026-10-30, user-requested 2026-10-30).
