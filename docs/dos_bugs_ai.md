# DOS bugs the port does not reproduce: Euro and native AI

STATUS: living record, not an audit dump. Authoritative.

Second file of [dos_bugs.md](dos_bugs.md), split off 2026-11-03 to keep each file
under the doc-length limit. It holds the entries about the Euro AI (colony tick,
ships, land units, AI diplomacy) and the native AI (villages, Braves, raids). The
rules, the id scheme and the entry format are the ones in dos_bugs.md; `DB-N` ids
are shared across both files and never reused.

## Entries

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

### DB-49 — Raid horse grab tests the rolled cargo's pile
**DOS:** the goods pick of the raid FUN_5fef_0f14 (raw 99822-99825) lets a horseless
tribe take Horses on its first try when `stock[rolled cargo] > 0x34` and a coin flip
comes up 0. The pile tested is the rolled cargo's, not the Horses', so the tribe can
steal Horses the colony barely has and miss a large herd.

**Port:** `ai_contact_raid_roll_stores_cargo` (`src/core/ai_contact_raid.c`) tests the
Horses pile. [Tests](archive/dos_bugs_evidence_2026-10-23.md#db-49).
**Status:** done (2026-10-23). Came from dos_bug_candidates_2026-10-22.md C-75.

### DB-50 — withdrawn: raid burn destroys the lowest owned tier
The premise was a misread. FUN_5fef_0f14 kind 2 (raw 99857-99864) walks byte
`row*0xc-0x707a`, which is the SUCCESSOR column (FUN_15eb_039e raw 9561-9577 counts
tiers along it; the predecessor is `-0x707b`, which FUN_15eb_14aa raw 11444-11455
follows down to the chain root). So DOS already burns the top owned tier: the
Cathedral burns and the Church stays. Warehouse Expansion and Capitol Expansion have no
building bit in DOS (FUN_364b_0114 raw 56911-56923), so a roll on them re-rolls and the
walk stops at the Warehouse / Capitol, whose arm takes one level off +0x95 / +0x96.
`ai_contact_pick_raid_kind` (`src/core/ai_contact_raid.c`) ports the walk literally
(2026-11-04). Was dos_bug_candidates_2026-10-22.md C-76.

### DB-51 — Refusing an AI's cash offer always reads as meek
**DOS:** FUN_5bfb_153e copies "MEEK" into the tone buffer `local_80` when it
shows @GIVECASH (raw 98017), and the WAR tag built at raw 98040-98041 appends
that buffer. A refusal after a cash offer is therefore always @WARMEEK, whatever
the encounter tone; without an offer the greeting tone is used.

**Port:** the @GIVECASH refusal (`AI_TALK_ST_GIVECASH`, `src/core/ai_diplo.c`)
uses the encounter tone, like the no-offer path; the peace-menu withdraw
refusal (raw 98159-98168, `ai_talk_say_war`) shares the helper and keeps the
encounter tone the same way. Text only; all goldens and `golden_ai_joint` pass.
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

### DB-67 — Military Assistance passes the nation id to the tribe selector
Was dos_bug_candidates_2026-10-22.md C-63.
**DOS:** `FUN_5bfb_153e` raw 98271 / 98335 (OVL16 0x2da4, 0x2ec6) hands
`FUN_281f_0a42` the player id 4..11; all 17 other callers subtract 4. The
extinct test of the ally list and the hire price read the wrong tribe.

**Port:** `ai_talk_stage_ally_pick` / `ai_talk_stage_ally_pay`
(`src/core/ai_diplo.c`) index `col1->indian[p - 4]` and `tribe_data_9184[p - 4]`.
**Status:** done (user 2026-11-03; port already differed, all goldens pass).

### DB-68 — Euro diplomacy score and withdraw cost overflow int16
Was C-64.
**DOS:** `FUN_5bfb_153e` raw 97530-97577, 98150-98156 (OVL16 0x1990-0x1a02):
`(diff+8)*delta*10` is 16-bit and wraps past 32767 (delta > 273 at Viceroy), so
the score comes out 0 or garbage; the withdraw cost wraps and is then raised to 100.

**Port:** the 153e score and withdraw arithmetic in `src/core/ai_diplo.c` is
32-bit `int` (the raw 545-566 scaling block and `withdraw_cost`).
**Status:** done (user 2026-11-03; port already differed, all goldens pass).

### DB-70 — Missionary revisit gate reads a tribe index as a nation id
Was C-67.
**DOS:** `FUN_521d_20e6` raw 90057-90063 (overlays asm 138096-138101):
`FUN_1000_8c28([0x8d52], [0x5398])` passes the tribe index without `+ 4`, so the
"tribe has met the human" test reads a Euro nation's (or the wrong tribe's)
relation row.

**Port:** `ai_euro_20e6_missionary_arm` (`src/core/ai_euro_land.c`) reads
`col1->indian[tn - 4].euro_diplo[human]`, the tribe's own row. The gate's other
reads stay DOS-literal: alarm and contact are toward DS:0x5398 (the human), and
power_rank compares the human with the AI.
**Status:** done (user 2026-11-03; port already differed, all goldens pass).

### DB-71 — Wander scorer's Euro-foe arm on unclaimed land
Was C-68.
**DOS:** `FUN_521d_20e6` raw 88877-88880 (asm 138877 signed `CMP [BP-0xe],4`):
an owner nibble of −1 (unclaimed land) enters the Euro arm and reads
`FUN_15b3_0004(nation, −1)`, the byte before the nation's relation row
(`return_from_europe_y`), as the relation. The PEACE test is effectively random
there.

**Port:** `src/core/ai_euro_land.c` (Euro-foe arm of the 20e6 direction scorer)
takes the relation with the occupant's nation when the land is unclaimed
(port_bugs.md #521).
**Status:** done (user 2026-11-03; port already differed, all goldens pass).

### DB-72 — Homeless Brave despawn steps the freed slot
Was C-69.
**DOS:** `FUN_4d56_021a` asm 0x337-0x365 destroys a Brave whose home village
index is out of range and returns −1; `FUN_4d56_14fe` 0x1502-0x1521 then steps
the freed record with direction −1 (use-after-free; corrupt or edited saves only).

**Port:** `src/core/ai_brave.c` despawns and returns STOP (port_bugs.md #826).
**Status:** done (user 2026-11-03; port already differed, all goldens pass).

### DB-73 — 5d04 Artillery price unset before turn 40
Was C-70.
**DOS:** `FUN_521d_5d04` raw 92834: `local_38` (the Artillery price) is
assigned only when turn >= 40, so before that the purchase reads a stale or
uninitialised stack word. Same class as DB-5.

**Port:** `src/core/ai_euro_europe.c` (colony demand vs. purchase block): the
AI pays the Europe purchase price at any turn, 500 + 100 per Artillery the
nation already bought (`FUN_38fd_4b50`'s nation+0x1e escalation), and the buy
bumps that counter. This also replaces DOS's `(10 - difficulty) * 100` from
turn 40 on (user decision). The King-grace arm still makes the buy free.
Test: `unit_ai_euro_5d04_hire` `artillery_price_escalates`.
**Status:** done (user 2026-11-03; ungated fix passes all goldens).

### DB-80 — Brave contact latch on unclaimed land hits the musket count
**DOS:** the `FUN_4d56_021a` tail (021a:1277) indexes the tribe's contact_state row with the
target tile's owner nibble after a signed `< 4` test, so an owner of −1 (unclaimed land) reads
and writes `contact_state[-1]`, which is the indian record's +0x2c word (tons[15], muskets).
A Brave contact or attack pick there is cancelled when that word is 2 and otherwise sets it
to 1, so the tribe's musket stock can be overwritten (port_bugs.md #1563).

**Port:** `ai_native_021a_tail` (`src/core/ai_native_021a.c`) runs the latch only for owners
0..3 and leaves the musket word alone. Goldens and idle campaigns pass unchanged.
**Status:** done (user 2026-11-07; port already differed).
