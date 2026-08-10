# Census + profession tally (`FUN_4962_*`)

Hosted by `FUN_3844_00f2` (`291f_0a74` / `291f_0a9e`). Also year-end chrome
census of human/crown. Bridge: [`between_turns.md`](between_turns.md).

---

## `FUN_4962_0018` — census

| Item | Value |
|------|-------|
| Lines | **78111–78327** (~217) |
| Thunk | `FUN_291f_0a74` |
| Arg | `param_1` = nation 0..3 |

### Phases

| # | Role |
|---|------|
| 1 | Zero nation counters + `unit_type_counts[19]` + cargo/continent scratch |
| 2 | **Unit loop**: type tallies, ships vs land, Europe-dock proxies, continent OR bits `0x95f2`, combat/`0x941c`, free colonist, field combat; foreign Euro near continent → OR `0x02` |
| 3 | **Colony loop**: `0x9298++`, pop sum; clear `colony+0x1b` bits 0/1; 11×11 ship probe → MoW/armed bits; globals `0xa89a`/`0xa89b`; foreign colony continent OR `0x04` |
| 4 | **Tribe loop**: continent OR `0x01` |
| 5 | Empty-colony continent bump `0x9650`; **mean pop** `0x944e = pop_sum / colonies` |

### Key DS (stuff window)

| Addr | Field |
|------|-------|
| `0x8cfc` | `all_unit_counts[4]` |
| `0x9298` | `colony_counts[4]` |
| `0x9408`…`0x942c` | free / pop / proxy / land / ship / armed / field |
| `0x941c` | `land_combat_strength[4]` |
| `0x924c` | `unit_type_counts[4][19]` |
| `0x944e` | mean colony pop |
| `0x95f2` | continent AI flag bytes |
| colony `+0x1b` | ship-pressure bits (Linux `ai_flags`) |

Helper: `FUN_4962_0006` — saturating +1 to 255.

### Linux

| DOS | Linux | Fidelity |
|-----|-------|----------|
| Full EOT census | no live mid-campaign freshen | Intentional lag |
| Blank-template fill | `col1_stuff_census_fill_blank` | **Partial** |
| Colony `+0x1b` ship bits | `ai_euro_refresh_colony_ai_flags` | Thin |

---

## `FUN_4962_0606` — profession tally

| Item | Value |
|------|-------|
| Lines | **78332–78373** (~42) |
| Thunk | `FUN_291f_0a9e` |
| Arg | nation nibble |

### Phases

1. Zero `DS:0x9430[0x1d]` profession histogram  
2. Units of nation with specialty (`281f_0b78`): `hist[unit+0x315b]++`  
3. Colonies: each colonist job (`281f_0c54`) → `hist[job]++`

### Linux

No dedicated port. Profession data on live units/jobs. **PARKED** as EOT
histogram writer.

**Order in `3844_00f2`:** `0606` → Europe EOT `5e52` → … → production →
census `0018` → king `2424`.
