# Nation EOT — ship-build ready + immigrant ship spawn

Deepens arms that stay thin stubs in [`nation_eot.c`](nation_eot.c).
Host: `FUN_3844_00f2` @58305–58425. Bridge: [`between_turns.md`](between_turns.md).

---

## A. Ship-build ready chrome (per-unit loop)

**Lines:** 58334–58370 (inside unit walk after treasure tick).

### Gate (all must hold)

| Check | Bytes |
|-------|-------|
| Unit nation nibble = active `0x5394` | `unit+0x3147 & 0xf` |
| Treasure tick kept unit | `291f_0a58` ≠ 0 |
| Type ∈ `(0x0c, 0x13)` i.e. **0xd..0x12** | `unit+0x3146` |
| Under-construction flag | `unit+0x3148` bit **0x80** |
| Type ≠ Frigate `0x0b` | redundant vs range; explicit `!= '\v'` |

### Progress

1. `turns_worked` (`+0x315a`) **+1**
2. If tile in colony (`281f_0302` / colony-at-xy shape): **+1** again (double progress on colony tile)
3. Threshold = type table byte at **`type*0xe + 0x5235`**
4. When `turns_worked ≥ threshold`:
   - Clear bit **0x80** on `+0x3148` (ship completed)
   - If Euro nation **and** human (`0x543f==0`):
     - Subst ship-type name from `type*0xe+0x5230`
     - Subst colony name **or** nation name if not on colony tile
     - BGM + side art `0xeef`; flush dialog
     - If **not** on colony tile → set **`DS:0x14c = 1`** (open Europe later)

**Not set here:** `DS:0x14e` focus unit — that comes from `48d3_06ba` treasure/ship focus path.

### Linux

Ship construction progress / ready chrome largely **PARKED**. Europe open on arrivals:
`game_europe_deliver_bound_ships`. Colony ship-build UI separate.

---

## B. Optional Europe screen

**Lines:** 58378–58380

If `DS:0x14c != 0` after landfall tax (`48d3_06ba` may also set it):
`281f_05fa` → `38fd_55b6(nation, DS:0x14e)`.

---

## C. Rare immigrant / ship spawn (tail of `00f2`)

**Lines:** 58393–58423

### Outer gate

| Condition | Meaning |
|-----------|---------|
| `DS:0xa89b != 0` **or** `DS:0xa89a > 3` | Census / continent pressure crumbs (filled by `4962_0018`) |
| `*(nation*0x13 + -0x6da3) == 0` | Nation dock / spawn-armed flag clear |
| `!(0x5382 & 1)` | Peacetime (no war chrome bit) |
| `(0x538e & 7) == 0` | Every **8th** turn |

### Human confirm

If Euro + human: dialog (difficulty string + player name + nation name), sound `0x3e`, flush → `local_4`. AI: `local_4 = 1`.

### Spawn

If `local_4 == 1`:

1. `281f_095c` → spawn type **`0x11`** (Merchantman-class) at Europe proxy coords `nation-0x18`
2. Clear `+0x314c`; copy Europe landfall target from nation Europe block `+0x32/+0x33` → unit `+0x314d/+0x314e`
3. `291f_0aee` → `48d3_0002` landfall goto duration → `+0x315a`
4. OR flag bit **`0x40`** on `+0x3148` (in-transit / bound)
5. Human: `291f_0ae0` → `38fd_3dc8` tax-delta chrome `0xf01` mode 10

### Linux reshape

Dock immigrants / crosses thresholds live in `turn_run_nation_ticks` (SETUP).
Bound-ship delivery in `game_finish_end_turn`. Atomic every-8-turns Merc spawn
from census pressure **PARKED** as DOS `00f2` tail.
