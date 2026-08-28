# Village action menu (`@ACTIONS`) and its DOS handlers — static port 2026-08-28

Everything here was read from `viceroy_overlays.c` / `viceroy_overlays.asm`
(overlay 13, the `thunk_FUN_1000_a5xx` family) plus the resident helpers they
call. No live DOSBox session was needed; the earlier "needs a decompile trace
of the `5bfb`/`4d56` dispatch" blockers on P8.1 / P8.3 / P8.5 / P8.8 were all
resolvable by (a) looking up the GAME.TXT tag string addresses
(`docs/popup_tag_ids.md`, DS string = EXE offset 121248+addr), (b) grepping the
overlay decompile for those literals, and (c) reading the `.asm` where Ghidra
dropped `PUSH imm16` suffix arguments (`FUN_0000_d9b4(auStack_c)` with the
suffix missing = a `LEARN`+`MASTER`/`ALREADY`/`SLOW`/`LATER`/`DONE` strcat).

Linux: `src/core/ai_contact.c` (`ai_contact_enqueue_village_meet`,
`ai_contact_live_among_natives`, `ai_contact_speak_with_chief`,
`ai_contact_demand_tribute`, `ai_contact_denounce_heresy`,
`ai_contact_establish_mission`, `ai_contact_enter_hostile_village`) and
`src/core/game_loop.c` (`game_request_indian_land_choice`, Attack Village
commit). Tests: `tests/unit/test_ai_contact.c` "@ACTIONS village menu" block.

## Where the menu lives

`FUN_4d56_4528`'s overlay copy (overlay 13, `0x4600..0x4bdb`) is the human
village-enter function. Head (`LAB_478a`):

```
FUN_1000_8714(7)                                ; side-art
STRING0 = @LEVELS[tribe.tech].col1 (DS:0x9634+tech*6)   "Camp"/"Village"/"City"
STRING1 = tribe name
body    = "VILLAGE" + (alarm>=0x4b ? "WAR" : alarm>=0x32 ? "BAD"
          : (alarm>=0x19 || village.attitude[e] >= 0x80) ? "MEDIUM"
          : slot==2 ? "SAVAGE" : "HAPPY")          ; @VILLAGEWAR.. GAME.TXT 753-790
music when alarm >= 0x32: slot 0 → BGM 7, slot 1 → BGM 6, else 5
dlg = FUN_1000_9372()                            ; load CHOICE, rows from NAMES @ACTIONS
```

`alarm` = `FUN_1000_84fc(slot, e)` = `indian.alarm_by_player[e]` (0..100).
`village.attitude[e]` is the int16 at settlement `+0xa+e*2` (Linux
`tribe.alarm[e]` friction|attacks<<8).

Row enabling (`FUN_1000_8212(row)` + `LAB_1000_9365` per row; asm
`0x486b..0x4ad0`), unit type byte at `0x3146`, attack column `0x5236`:

| row | @ACTIONS text | enabled when |
|---|---|---|
| 1 | Trade With Village | type ∈ {0xc wagon, 0xd..0x12 ship} and alarm < 0x4b |
| 2 | Enter Hostile Village | same types, alarm ≥ 0x4b |
| 3 | Establish Mission | met (`8c28 != 0`), type 3 (Missionaries), village `+5` < 0 (no mission) |
| 4 | Denounce Heresy of %Fs Mission | met, type 3, mission owner ≠ self (%F = owner adjective) |
| 5 | Live Among The Natives | met, not type 3, `FUN_1000_8d68(unit) ≥ 0`, attack < 2, type ≠ 5 (Scouts), profession ≠ 0x1b (Convert) |
| 6 | Ask to Speak With Chief | type 5 (Scouts) |
| 7 | Incite Indians | met, type 3 — always, after row 3/4 |
| 8 | Demand Tribute | met, not type 3, attack ≠ 0, land unit |
| 9 | Attack Village | land unit, attack > 1; also (met) attack ≠ 0 when not yet listed |
| 10 | Cancel Action | always |

Result switch (`switchD 0x4bdb`, 1-based): 1 → `a63c` (2820 trade), 2 →
`a5e8`, 3 → `a5dc`, 4 → `a594`, 5 → `a618(…, 0)`, 6 → `a60c`, 7 → `a5b8`
(= `FUN_4d56_417e` incite), 8 → `a5f4`, 9 → `FUN_1000_8bf6(4)` (attack mode),
default → nothing.

Before the human menu the same function auto-routes AI / mode entries
(`caseD_6..a/2` at `0x472f`): unskilled Free Colonist (0x1c) or Indentured
Servant (0x19) with `8d68 ≥ 0` and alarm < 0x4b → mode 5 (Live Among). The
Linux port keeps its per-turn `ai_contact_teach_skill` pulse for AI nations
and routes humans through the menu.

**Not in DOS:** a "Gift" village action. Gold gifts do not exist; goods gifts
are the 2820 trade flow's gift arm (already ported). The Linux
`AI_CONTACT_CHOICE_GIFT` handler is kept for its amount-CHOICE path but is no
longer listed.

## `thunk_FUN_1000_a618` — Live Among The Natives (teach)

`a618(seg, unit, e, village, preview)`; `preview=1` returns only the skill
(used by `a60c` for @CHIEFHOWDY).

Skill pick: `a624` (= the already-ported `FUN_4d56_2154` meet economics,
bid table DS:0x9e78) then, with the RNG reseeded from `x*256+y + DS:0x8d80`
(`FUN_1000_86ba`) and restored from `DS:0x83a6` afterwards:

```
bid[8] = 0                                   ; fisherman only via the ocean roll
tech<1: bid[12]=0 (FurTrader) bid[6]=0 (OreMiner) bid[0]>>=1
tech<2: bid[11]=0 (Weaver) bid[10]=0 (Tobacconist) bid[7]=0 (Silver) bid[0]-=bid[0]>>2
tech<3: bid[9]=0 (Distiller)
tech==3: bid[7] += bid[7]>>1
r = rand(1, Σbid); skill = first index with cumulative ≥ r
skill==4 (FurTrapper) && (x+y)%3==0 → 0x16 (Seasoned Scout)
skill==0 (Farmer): n = ocean tiles among the 20-ring (DS:0xc8/0xde offsets);
                   rand(1,20) < n → 8 (Fisherman)
```

Outcome (`0x3827..0x39e3`), `band = FUN_1000_8c50(alarm)` quartile:

| condition | suffix / action |
|---|---|
| band > 1 | `MAD`; `FUN_4cc6_00f2(slot, e, +3)`; if `(rel & 0x60) == 0x20` return silently |
| profession 0x1a (Petty Criminal) | `CRIMINAL` |
| profession 0x1b (Convert) | `@TEACHCONVERT` via `FUN_1000_85ee` (BX = 0x15f4), return |
| profession ∉ {0x19, 0x1c} | STRING1 = profession name; `MASTER` |
| village `+3 & 0x02` set and `& 0x04` clear | `ALREADY` |
| band > 0 and `rand(1,1000) < 200*difficulty+100` | `SLOW` |
| human: `@LEARNSTAY` CHOICE ≠ 1 | `LATER` |
| else | `DONE`: profession = skill, village `+3 \|= 0x02` |

Tag = `"LEARN"` + suffix (DS:0x162a + 0x1601/0x1608/0x1610/0x161f/0x1625/
0x15eb/0x15e7), shown for humans only. STRING0 = tribe, STRING1 = skill.

## `thunk_FUN_1000_a60c` — Ask to Speak With Chief

```
seasoned = profession == 0x16
if alarm < 0x4b:
  thr = rand(0, seasoned ? 140 : 100)
  if alarm < 0x19 || alarm>>2 < thr:
    if slot == 2 (Arawak): rand(0, (8-difficulty) << seasoned) == 0 → KILL
    human: @CHIEFHOWDY  STRING0 = @JOB expert name of a618(preview) skill,
                        STRING1..3 = top three of the ask table (DS:0x9e58,
                        FUN_1000_a0c0 sort; last_bought/last_sold zeroed)
    STRING0 = tribe
    if alarm < thr && !(village+3 & 0x08):
      village+3 |= 0x08; r = rand(1,3)
      r==1 && !seasoned: STRING1 = @LEVELS noun; profession = 0x16;
                         @CHIEFGUIDES + @WELLSEASONED; return
      r==3: gold = (tech+1) * rand(1,6) * (rand(1,10-d)+rand(1,10-d)+rand(1,10-d)) * 4
            STRING1 = Euro adjective; NUMBER0 = gold; @CHIEFGIFT; treasury += gold; return
      else (r==2, or seasoned r==1): @CHIEFAREA; FUN_13f1_02b4(unit, DX=6) reveal; return
    → BORED
KILL: if !FF_owned(e, 6 = Coronado): @CHIEFKILL, FUN_1000_89f8(unit) destroy; return 1
BORED: STRING1 = Euro adjective; @CHIEFBORED
```

## `thunk_FUN_1000_a5f4` — Demand Tribute

```
cont  = continent of the unit tile
euro  = byte[-0x6a4e][cont][e] (exposed land combat on cont) + word[-0x6be4][e] >> 1
        e == 2 (Spanish): ×1.5;  FF 10 (Cortes): ×1.5
indian= (byte[-0x6e34][cont][slot] + byte[-0x6e7c][slot] >> 1) * 2 + alarm >> 1
bump  = human ? difficulty+1 : 1
win   = rand(0, indian) < rand(0, euro) && FUN_1000_8804(x,y,e,cont) ≥ 0 (own colony on cont)
if (win || indian < euro) && alarm < 0x4b:
  if win || alarm < 0x32:
    if !(village+3 & 0x10) && win:
      bump <<= 1; village+3 |= 0x10
      good = top of the bid table (a624 + a0c0 sort, index 15)
      qty  = max(10, min(cap - colony.stock[good], min(100, bid[15]*3 + 10)))
             (bid[15] is the muskets slot a624 always zeroes → qty is 10 in practice)
      @EXTORTSTUFF  STRING0 difficulty title, STRING1 tribe, NUMBER0 qty,
                    STRING2 cargo, STRING3 colony;  colony.stock[good] += qty
    else: @EXTORTPOOR (title, tribe); bump = 0
  else: @EXTORTNO (title, player name, tribe)
else: @EXTORTLAUGH (tribe)
FUN_4cc6_00f2(slot, e, bump)
```

The census bytes are recomputed live on Linux (`ai_contact_land_combat_sum`,
same `combat_unit_base_x8(mode 1)` value the incite price uses).

## `thunk_FUN_1000_a594` — Denounce Heresy

```
foreign = village+5 & 0xf
for each village v of the tribe:
  (n, s) = FUN_4cc6_03f8(v)          ; strongest nearby Euro nation + score — NOT ported (thin 0)
  c = (n == foreign ? theirs += s : n == e ? mine += s : s)
  if v has a mission: c += v.population; ×2 if +5 & 0x10 (Jesuit); ×2 if capital
                      owner == e ? mine += c : pro_me += c
jesuit_me = unit profession == 3 per the decompile literal (Linux: is_jesuit_grade)
cap = village+3 & 0x04 (value 4 when set)
pro_me += alarm[foreign] << cap ;  mine += alarm[e] >> ((1 - cap) & 0x1f)
d_me = quartile(mine)+1 ; d_them = quartile(pro_me)+1
capital: pro_me += rand(1,20); mine += rand(1,20); d_them ×2; d_me ×2
jesuit_me: pro_me <<= 1; d_them <<= 1
rival Jesuit (+5 & 0x10): mine <<= 1; d_me <<= 1
STRING0 my adjective, STRING1 rival adjective, STRING2 tribe
rand(1, mine+pro_me) > pro_me → @HERESY1 (missionary burned)   d_them = -d_them
else → @HERESY0: village+5 = e | (jesuit ? 0x10)               d_me   = -d_me
FUN_1000_89f8(unit) (consumed either way)
FUN_4cc6_00f2(slot, foreign, d_them); FUN_4cc6_00f2(slot, e, d_me)
```

Replaces the port's earlier "50/50" automatic heresy pulse for human units
(the AI convert pulse in `ai_contact_missionary_convert` is untouched).

## `thunk_FUN_1000_a5dc` — Establish Mission

```
count = own missions with this tribe
FF 0x17 Sepulveda: ×2 ; FF 0x18 Las Casas: >>1 ; FF 0x10 Pocahontas: >>1 ; French: >>1
base = count*8 - {25, 15, 10, 5}[quartile(alarm)]
capital: base += sign(base)*8
n = quartile; base > -6 → n ≥ 1; base > 0 → n ≥ 2; base > 9 → n = 3
@MISSION{n}: STRING0 adjective, STRING1 nearest own colony (any continent) or
             the home-country name, STRING2 season, NUMBER0 year, STRING3 tribe
village+5 = e; |= 0x10 if profession 0x18 (Jesuit) or FF 0x16 (Brebeuf)
FUN_1000_89f8(unit); FUN_4cc6_00f2(slot, e, base)
```

## `thunk_FUN_1000_a5e8` — Enter Hostile Village

`r = rand(0,500)`: `r ≤ alarm` → `@KILLWAGONS` + unit destroyed; `r ≤ 2·alarm`
→ `@MADATWAGONS`; else `@GRUDGEWAGONS` and the 2820 trade runs.

## Encroachment CHOICEs (P8.5) — `@INDIANLAND` / `@INDIANFOREST` / `@INDIANROAD`

Three sites, one shape. Forest = `thunk_FUN_1000_91fc` (clear/plow order,
overlay 2), road = `thunk_FUN_1000_9304` (road order), land = the found-colony
tile-buy tail in `FUN_OVL03_L0000__003011`'s function (`0x9cd0/0x9cce` cursor,
`DS:0xbb8` founding flag). Common body:

```
owner  = tribe owning the tile (5x5 native cache; nearest village on the same
         continent within FUN_15dc_006a(tribe) = tech tier: 0/1 → 1, 2 → 2, 3 → 3)
gate   = FUN_1000_8c28(owner, e) & 0x40 (PEACE); tile layer2 & 0x10 clear
         forest site only: terrain class 8..0x17
price  = FUN_1000_8f68 = FUN_4cc6_07c2 (colonies_indian_land_purchase_gold)
if price > 0:
  NUMBER1 = price; STRING0 = tribe
  dlg = FUN_1000_9372(); if treasury < price: func_0x000193a6(dlg, 2, 1)   ; grey row 2
  r = FUN_1000_935a(dlg)         ; 1-based
  r == 1: cancel (order cleared / colony not founded)
  r == 2: indian.lands_bought++ ; FUN_1000_8ce6(e, price) ; tile |= 0x10
          (land site also clears the cache entry) ; @INDIANBRIBE
  else  : proceed — no immediate consequence in any of the three sites
```

Conclusions that unblock the old P8.5 note: "Take it" has **no** invented
balance number to guess — nothing happens at the site; encroachment friction is
the already-ported `FUN_4d56_152e` pass. "Offer gold" is greyed, not hidden,
when unaffordable (the Linux port drops the row). Outside the dialog (no
peace, Minuit, bought tile) DOS founds/clears/builds **without charging**; the
Linux "need N gold" hard block is gone.

Radius correction: Linux `colonies_tile_indian_homeland` used the manual's
"capital → 2" rule; DOS keys the radius on tech tier (Inca 3, Aztec 2, others
1) and filters villages by continent. Fixed in `colony.c`.

## Resolved tag identities (P8.3)

`@TRIBUTE`, `@TRIBUTEUSA`, `@GIFTS`, `@WANTSTUFF*` are **Euro** diplomacy
(`FUN_1d1d_07e4(local, 0x1916)` builds `TRIBUTE`+`USA` in the resident
rival-demand code; `FUN_2a1f_0688(0x19e4)` = `@GIFTS`). They are not Indian
gift/demand text. The Indian "demand" is Demand Tribute above
(`@EXTORTSTUFF/POOR/NO/LAUGH`); `@CHIEFGIFT`/`@CHIEFBORED` belong to Ask to
Speak With Chief.

## Thin spots / open (updated same day)

- `FUN_4cc6_03f8` (nearby-Euro presence per village) **ported** into the
  heresy roll as `ai_contact_4cc6_03f8` — ring threat sums, colony score by
  buildings/pop/tech/difficulty/distance, continent halving, French and
  Pocahontas halving, mission-owner scaling of the best score. The
  building count reads the 48 building bits (`colony+0x84..0x89`), Linux
  `has_building[]`.
- The a618 RNG reseed base `DS:0x8d80` is `boot_timer` (save row 608,
  written at boot from the clock, not the seed) — DOS's "stable per
  village" skill only holds within a session; Linux seeds from the village
  position, so it is stable across sessions. Nothing left to name.
- `FUN_1000_8d68` = `FUN_15eb_0902` = `DS:0x30e[unit type]` default
  profession `{19,21,20,24,23,22,-1,23,-1,21,-1…}`; ≥ 0 only for
  Colonists/Soldiers/Pioneers/Missionaries/Dragoons/Scouts/Cont. Cavalry/
  Cont. Army — equivalent to the port's colonist-class test once the other
  gates apply. Documented in `ai_contact_classify_unit`.
- Establish Mission's `%STRING1` fallback table (`-0x7c74` = `DS:0x838c`)
  holds non-string words in the unpacked image (`0x14bc` → `MENUCOLR.SS`);
  probably overlay-relative. Linux keeps `player.country_name`. Cosmetic.
- P8.4 raid defence (the "stockade/soldier defense odds" half) — the
  `FUN_5fef_0f14` head is now ported into `ai_contact_pick_raid_kind`:
  walls = `FUN_281f_0ab0(0)` = Stockade→Fort→Fortress chain count,
  `r = rand(0,12) - 1 (+ difficulty-2 for a human victim)`, `r < walls*3+1`
  → `@RAIDNOTHING`; plus the early-game demotion (Discoverer/Explorer,
  turn < (2-difficulty)*40: building/unit kinds → nothing). The
  soldier-vs-brave fight itself was already the real combat engine
  (`units_resolve_land_combat` with the colony multipliers; undefended
  colony = the P5.4 token militia). Still PARKED: the rest of `0f14`'s
  kind ladder / `09fc` building probes.
