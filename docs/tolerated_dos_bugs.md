# DOS bugs tolerated for now

STATUS: living record, not an audit dump. Authoritative.

VICEROY.EXE behaviour that is a bug in the original, that the user has judged
undesirable, but that the port **still reproduces for now** so development can
continue (typically because the golden fixtures and idle campaigns that guard
DOS fidelity depend on it). Each entry is due to move to
[dos_bugs.md](dos_bugs.md) (bugs the port does not reproduce) once the port
stops reproducing it. Add an entry only on the user's say-so; an agent must not
decide this alone.

Not this file:

- DOS bug the port already does not reproduce: [dos_bugs.md](dos_bugs.md).
- Port diverges from DOS but DOS is *right*: [known_divergences.md](known_divergences.md).
- Actionable port bug: [port_bugs.md](port_bugs.md).

Ids are `TB-N`, permanent and never reused. An entry moving to `dos_bugs.md`
gets the next `DB-N` there and notes "was TB-N"; leave a one-line stub here
pointing at it so citations still resolve. Each entry names the DOS function
(`FUN_ssss_oooo` + raw offset), what DOS does, the port sites that reproduce it,
and what blocks removing it.

## Entries

### TB-1 — Coarse connectivity planes drop some NE/SW links
**DOS:** `FUN_67f4_0088` builds the land (`DS:0x85e8`) and sea (`DS:0x86f6`)
coarse connectivity planes: 15×18 cells of 4×4 tiles, one direction bit per
neighbouring cell that is reachable within the hop cap. For each cell it tests
directions 0..3 through `FUN_6662_0906` -> `FUN_6662_00f2` and accepts a link
only when `0 < cost < 8` (raw 105074-105075). `00f2` reuses its last
destination flood (`DS:0x2d1a/0x2d1c`, grid at `DS:0xa270`) when the
destination is unchanged and `grid[from] != 0`; it then leaves `DS:0xa370` at 0,
and `0906` returns 0. So a NE test whose destination is the one the previous
E/SE test flooded is rejected even when the cells are connected, and its
mirrored SW bit on the neighbour is never set. Bits are only ever missing, never
spurious. smcol (`supplemental-info.md`) records the same NE/SW mis-fill.

**Effect:** only the far tier of the Go To pathfinder (`FUN_6662_0f74` ->
`FUN_6662_09ae` snap / coarse BFS) reads the planes: long-range Go To for human
units, Euro AI and braves. Usually the coarse BFS detours around the missing
diagonal (N then E), giving a different waypoint and a slightly slower route.
Where two areas are joined only through that diagonal (a diagonal strait or
isthmus), the coarse BFS sees them as disconnected or far apart and the unit
falls back to the scored 8-neighbour step: odd detours, back-and-forth
wandering, or a Go To that never arrives. Frequency on real maps not measured.

**Port (reproduces it):**
- Saves: `col1_post_map_rebuild_connectivity` (`src/core/col1_post_map.c`,
  `PostCheapCache`) writes the planes byte-exact, quirk included (COLONY00
  planes match), so DOS loading a port save reads what it would have built.
- Live pathing: `units_coarse_build` (`src/core/units_move.c`) builds the
  port's own coarse grid through `units_flood_0906` on the shared `00f2` grid,
  which has the same reuse.

**Blocks removal:** dropping the reuse changes far-tier Go To routes, so the
idle campaigns (CTest `golden_idle_*`) and goldens stop matching DOS. Removing
it from live pathing only (keep the save planes DOS-exact) needs a way to keep
those gates meaningful, e.g. a switch the golden harnesses turn off.
**Status:** tolerated (2026-10-21).
