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
- DOS bug that **is** ported literally (e.g. `colony_workers.c` ~989, the
  `ai_contact_demand.c` ~2517 census total, the `ai_diplo.c` ~2087 OVL16 transfer):
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
**Status:** done (2026-10-21). The America idle archive ends at save 63 for this
reason (CTest `golden_idle_america`, [idle_america.md](idle_america.md)).
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
