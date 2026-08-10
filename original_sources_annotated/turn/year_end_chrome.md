# Year-end chrome UI (`FUN_3844_0442`)

Full string / subst / threshold map for year-end Euro chrome. Annotated body:
[`year_end_chrome.c`](year_end_chrome.c). Orchestration:
[`between_turns.md`](between_turns.md) ·
[`docs/turn_between_players.md`](../../docs/turn_between_players.md).

**Port status:** mapped; Linux has no dedicated year-end module (king/war thin
in `ai_king`; dialogs **PARKED**).

Decomp: `viceroy_unpacked.c` **58430–58680**. Thunk `281f_061e`.

## Sections (A–F)

| Sec | Gate | Behavior |
|-----|------|----------|
| A | always | Census human (`291f_0a74`→`4962_0018`); if war also census crown `0x53d2`; recount human colonies → scratch `nation−0x6d68` |
| B | year≥`0x640` (1600) && human colonies==0 && !war | Defeat dialog `0xf09` → **LAB_3844_04ec** (HoF if bit4 clear; clear `0x53c2`) |
| C1 | war && !(flags&8) && (crown colonies==0 \|\| flags&0x20) | Count crown warships types 6/8/0xb; if fleets thin **and** REF pools thin → victory `0xf20`; OR flags bit3; set `0x104`; → **LAB_3844_0b4a** |
| C2 | war && !(flags&8) else | Count rebel colonies (`0x1c&0x40`); SoL ratio; peace-offer or pressure |
| D | !war | Rival loop EN..DU: SoL pressure `0xf5e`/`0xf69` or auto-declare |
| E | !(flags&0x10) | Anniversary / game-over years |
| F | **LAB_3844_0b4a** | If `0x53c2==0`: optional continue dialog; OR flags bit4 |

## String ID table

| ID | Section | Role |
|----|---------|------|
| `0xf09` | B | Defeat / no colonies (side-art style 8 via `291f_0ad4`) |
| `0xf20` | C1 | Victory announce (`291f_0aba` args 1,2) |
| `0xf29` | C2 peace | Template copied to `local_58`; `local_52 += cVar8` severity |
| `0xf31` | C2 peace | Follow-up announce after `0xf29` (`0aba` 2,1) |
| `0xf39` | C2 pressure | Template; `local_54 += cVar1`; numeric subst then `0652` |
| `0xf5e` | D | Rival SoL rising pressure (flush mode 2) |
| `0xf69` | D | Rival SoL falling / relief (flush mode 2) |
| `0xf4b` / `0xf3f` | D auto-declare | Dual-id load (`281f_0422`) into dialog |
| `0xf73` | E anniversary | Spring 1790 (`0x6fe`) or 1840 (`0x730`) |
| (none named) | E game-over | 1800 (`0x708`) / 1850 (`0x73a`): richest-colony subst + `03fe` + HoF `0574`; clear `0x53c2` |

## Subst-slot matrix

| Call | Slot | Value |
|------|------|-------|
| C1 victory | `0416` 0 | human `country_name` @ `0x540e+n*0x34` |
| C1 victory | `0416` 1 | human alt name @ `0x5426+n*0x34` |
| C2 peace | `0416` 0 | human alt `0x5426` |
| C2 peace | `0416` 1 | human `0x540e` |
| C2 peace | `0ac8` 2 | crown name via `0x53d4` |
| C2 peace | `0438` 0 | human ptr from `−0x72be` table |
| C2 pressure | `0416` 0 | human alt `0x5426` |
| C2 pressure | `09ae` 0 | rebel colony count `local_68` |
| C2 pressure | `09ae` 1 | human colony scratch `−0x6d68` |
| C2 pressure | `09ae` 2 | SoL ratio `local_8` |
| D pressure | `0ac8` 0 | rival nation |
| D pressure | `09ae` 0/1/2 | SoL `iVar5`, table `−0x6bf0`, threshold `local_6` |
| D pressure | `0438` 1 | rival nation name |
| D declare | `0ac8` 0 | rival |
| D declare | `0416` 1/2 | rival alt / country name |
| D declare | `0422` | dual strings `0xf4b`/`0xf3f` |
| D declare | `0416` 3 | buffer `0x833c` |
| E anniversary | `0438` 0 | difficulty name `0x53a6`→`−0x7c6c` |
| E anniversary | `0416` 1 | human `0x540e` |
| E game-over | `0438` 0 | difficulty name |
| E game-over | `0416` 1 | human `0x540e` |
| E game-over | `0416` 2 | richest colony name `local_6c*0xca+0x5d48` |
| B defeat | `0438` 0 | difficulty name |
| B defeat | `0416` 1 | human `0x540e` |

## Thresholds

### Victory fleet / REF (C1)

- Count crown units type ∈ {`0x06`,`0x08`,`0x0b`} → `local_5a`
- Cap: if flags bit6 clear → need `local_5a < 8`; if bit6 set → `< 1` (expr: `(−(bit6==0)&0xfff9)+8`)
- REF pool thin: `(2 − (0x53dc==0) − (0x53e0==0) + 0x53da) < 4` **or** flags bit5
- On fire: OR `0x5382` bit3; set `DS:0x104=1`

### SoL peace / pressure (C2)

```
sol = (crown_sol_byte_adj + 1) * 100 / (human_sol_byte_adj + crown_sol_byte_adj + 1)
  where byte at nation + (−0x6bf4); zero byte → treat as 1 via ~x+1
```

| Condition | Effect |
|-----------|--------|
| `sol > 0x4f` (79) | force pressure severity `cVar1 = 3` |
| `sol > 0x59` (89) | force peace severity `cVar8 = 3` |
| human colonies `< 3` | `cVar1 = 2` |
| human colonies `== 0` | `cVar8 = 2` |
| `cVar8 != 0` | peace-offer path → `04ec` |
| else `cVar1 != 0` | pressure dialog `0xf39` |

Rebel colony count `local_68`: colonies with owner==human and `flags+0x1c & 0x40`.

### Rival SoL (D)

```
iVar5 = europe_market[0x19] * table[rival − 0x6bf0] / 100  (clamp 100)
local_6 = (difficulty − 8) * −10
```

- If `iVar5 < local_6`: rising/falling dialogs vs last-shown `0x84fc+0x1a`
- Else: auto-declare — OR europe flags bit2; set diplo bit `0x40` vs others; clear bit `0xbb`

### Calendar years (E)

| Year const | Decimal | Event |
|------------|---------|-------|
| `0x640` | 1600 | Defeat gate (with no colonies) |
| `0x6fe` | 1790 | Anniversary (Spring only, peacetime) |
| `0x708` | 1800 | Game-over richest colony (peacetime) |
| `0x730` | 1840 | Anniversary (any season) |
| `0x73a` | 1850 | Game-over |

Anniversary requires `autumn==0` for 1790 path; both require flags bit4 clear.

## LAB exits

| LAB | Effect |
|-----|--------|
| `LAB_3844_04ec` | Optional HoF (`0574` if bit4 clear); `0x53c2=0`; `bVar2=false` |
| `LAB_3844_0b4a` | If stopped: if victory bit3 → `0x53a2=1`; teardown; `03fe` continue may set `0x53c2=1`; always OR flags bit4 |

## `0x5382` bits used here

| Bit | Meaning in `0442` |
|-----|-------------------|
| 0 | War / WoI |
| 3 | Victory handled (`\|8` on C1) |
| 4 | Splash / year-end done (`\|0x10` in epilogue) |
| 5 | Force victory path (`0x20`) |
| 6 | Stricter fleet cap (`0x40`) |

## Linux

No `year_end_chrome` module. Pieces: `ai_king` war/declare thin; HoF stub;
calendar in SETUP. Full dialog string port **PARKED**.
