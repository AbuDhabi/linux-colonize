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

**Port:** carry the DOS-unsaved state in the port extension block
(`COLNXEXT`, [savegame.md](savegame.md) "Port extension block"; chunk tags in
`col1_save.h`, `VTIN` is the only one so far), so a port save+reload continues
exactly where continuous play would. DOS still loads such a file (it ignores the
tail) and drops the block on re-save; the slot list marks it with `*`.
**Status:** not done. Needs one chunk per table above (and any further holes the
idle campaigns expose), written on save and restored by `col1_bridge_apply`
before the first turn step; a continuous-vs-reload golden should guard it.
