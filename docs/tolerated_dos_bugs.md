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

### TB-2 — Passengers switch ships when another own ship leaves the tile first
**DOS:** a passenger is not "aboard" anything; it is a sentried land unit in
the tile's unit chain. When a ship leaves a tile, `FUN_1427_10be` (raw
8606-8680) sorts the tile with `FUN_1427_04d6` (hulls on top, then Treasure,
then @UNIT size 6..1, each bucket in chain order) and boards every same-nation
land unit that fits the hull's room, charging each its @UNIT size column
(`FUN_4720_00e0`, raw 74655-74661). Riders of other ships on the tile are in
that chain too, so whichever ship moves off a shared tile first takes them,
first come, first served, and may leave some of its own passengers behind for
the next hull. Evidence: idle independence seed-100 1->2, at (1,53) Galleon 109
keeps four Dragoons, takes unit 58's two, and leaves its own Artillery and
Colonist (DOSBox BPM trace 2026-10-16).

**Effect:** with two or more own ships on one tile (colony or open sea), units
loaded on one ship can sail off on another, and a ship can depart without some
of the units the player loaded on it. Affects human players and the AI alike.

**Port (reproduces it):** `units_ship_departure_pickup`
(`src/core/units_cargo.c`, declared in `units_cargo.h`) walks the hull's own
riders, sentried land units on the tile and riders of other own hulls there in
the 04d6 order; called from the ship move in `src/core/units_move.c` before the
group moves. The port keeps an explicit `aboard_ship_id`, so this is a
deliberate re-creation, not a side effect. Unit test: `unit_units_core`
`ship_switch_quirk`.

**Blocks removal:** the idle independence campaign (CTest
`golden_idle_independence`, from save 1->2) depends on the DOS pickup order;
boarding only the hull's own riders changes which units sail and breaks the
DOS match.
**Status:** tolerated (2026-10-21).
