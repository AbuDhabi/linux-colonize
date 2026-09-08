# Overlay `3f41` (OVL06) reconnaissance — 2026-09-08

Commissioned as "recover the FA / diplomacy negotiation screen from the
never-touched `3f41` segment". **Both halves of that premise are stale.**
This file records what `3f41` actually is, where the negotiation tags
really live, and the two genuine gaps the pass turned up.

## Headline

1. **`3f41` is the F2–F9 adviser-report overlay, not diplomacy.** All 18
   functions are report-screen drawing code. The only GAME.TXT popup tag
   pushed anywhere in the segment is `@FOREIGNNOTAVAIL`.
2. **`3f41` was not "never recovered".** It is one of the better-documented
   overlays in the project: `docs/reports.md` cites 14 of its 18 functions
   by name, `docs/report_screens.md` is a whole method doc for porting them,
   and `src/core/reports.c` is the live port.
3. **The negotiation screen is `FUN_5bfb_153e`** (`viceroy_unpacked.c`
   :97321–98432), which was fully recovered and ported on 2026-09-06 —
   see [`euro_diplo_153e_full.md`](euro_diplo_153e_full.md). Every one of
   the ~17 entry tags is pushed from inside that one function.
4. Stale text to fix on a later pass (this file does not edit them):
   - [`euro_diplo.md`](euro_diplo.md) §"FA negotiation screen — … function
     not yet found" (lines ~374–520) — the whole section is superseded.
   - `docs/popups.md` — ~30 rows saying "full `3f41` PARKED" for diplomacy
     tags; those tags are in `5bfb`, and most are **Done**.
   - `docs/popup_audit.md`, `docs/manual_gap.md` carry the same phrasing.

---

## Part 1 — where the negotiation tags actually are

Method: the tags' DS ids come from `docs/popup_tag_ids.md`; grepping each id
literal across `viceroy_unpacked.c` puts **every** hit inside
`FUN_5bfb_153e` (97321–98432). Grepping the whole `3f41` body (69397–70945)
for `0x1xxx` literals yields exactly four: `0x11a2`, `0x11a9`, `0x11b4`,
`0x11b6` — three of which are not popup tags at all (below).

Several tags never appear as DS strings because DOS builds them by
`strcat` at runtime; that is why the earlier searches came up empty:

| runtime tag | built from |
|---|---|
| `@HELLOFIRST` / `@HELLOAHOY` | `0x18c7` `HELLO` + `0x18d2` `FIRST` / `0x18cd` `AHOY` |
| `@HELLOMEEK` / `@HELLOMANLY` | `0x18c7` `HELLO` + `0x18b0` `MEEK` / `0x18b5` `MANLY` |
| `@PEACEMEEK` / `@PEACEMANLY` | `0x1964` `PEACE` + `MEEK`/`MANLY` |
| `@OLDPEACEMEEK` / `@OLDPEACEMANLY` | `0x1984` `OLDPEACE` + `MEEK`/`MANLY` |
| `@WARMEEK` / `@WARMANLY` | `0x1978` `WAR` + `MEEK`/`MANLY` (and the literal `0x1940` `WARMANLY`) |
| `…USA` variants | any of the above + `0x17e8` `USA`, gated on `local_9c` |

### Tag → owning site (all in `FUN_5bfb_153e`)

`raw` = line in `viceroy_unpacked.c`. "Linux" = state in `src/core/ai_diplo.c`
(`ai_diplo_153e_encounter` state machine) as of 2026-09-08.

| tag | DS id | raw | trigger (as read) | Linux |
|---|---|---|---|---|
| `@HELLO`+`FIRST`/`AHOY` | 0x18c7 + 0x18d2/0x18cd | 97705–97715 | first contact: `FUN_281f_0a38(self,target) & 0x20 == 0`; `AHOY` when the encountering unit's type (`+0x3146`) is in `0x0d..0x12` (a ship), else `FIRST` | live (`ai_diplo.c:2941-2944`) |
| `@HELLO`+`MEEK`/`MANLY` | + 0x18b0/0x18b5 | 97691–97705 | already met; tone = `iVar6` (see "tone selector") | live |
| `@HELLOUSA` | 0x18d8 | 97720 | `local_9c` (target has declared independence) | live |
| `@APOSTATES` | 0x18e1 | 97732 | a third **Euro** power `iVar17` that self has met, `iVar17 < 4`, crown-armed gate `local_9e == 0` | live (`ST_THIRD`) |
| `@HEATHEN` | 0x18eb | 97740 | same slot but `iVar17 >= 4` → an Indian tribe | live |
| `@PIRACY` | 0x18fa | 97785 | `FUN_281f_0a38(target,self) & 0x80` (privateer grievance bit) && crown gate && self has ≥1 unit of the counted class (`[-0x6da4 + self*0x13] != 0`) | live |
| `@SIEGES` | 0x1908 | 97842 | military adjacency to target's colonies; see the `own_border`-is-dead note in `euro_diplo_153e_full.md` §1 | live |
| `@TRIBUTE` | 0x1916 | 97905 | `local_68 != 0 && iVar6 == 1` (manly) && human can afford `local_68` | live |
| `@WANTSTUFF` | 0x1926 | 97937 | `iVar6 != 0 && local_68 != 999 && local_b4 >= 0` (a cargo was picked) | live (incl. the Furs stale-index DOS bug) |
| `@WARMANLY` (post-tribute) | 0x1940 | 97960 | `worthy && local_68 == 999` → OK popup, then `FUN_281f_0a10(self,target,0x40)` = war | **MISSING** (see gap 1) |
| `@RID` | 0x1951 | 97966 | `worthy` and neither the PROVOKE nor the 999 arm → OK popup only, **no** war | **MISSING** (see gap 1) |
| `@PROVOKE` | 0x1930 | 97978 | `worthy && at_peace(0x40) && local_68 >= 0x65` → war | live (`ai_diplo.c:2292`, `:2626`) |
| `@WORTHY` | 0x195d | 97997 | `!worthy && !at_peace`; skipped (treated as "yes") when `local_9c` | live |
| `@PEACE`+tone | 0x1964 | 98006 | `@WORTHY` answered 1 → sets peace, `[0x53c8 + target*2] = turn + 0x10` | live |
| `@GIVECASH` | 0x196f | 98021 | `@WORTHY` answered 2, not already at war, offer `= clamp((dominance−2)*2, 0, gold/100) * 100` | live |
| `@WAR`+tone (`0x1978`) | 0x1978 + 0x18b0/0x18b5 | 98042 | still not at peace after the `@GIVECASH` arm | live |
| `@OLDPEACE`+tone | 0x1984 | 98056 | already at peace and `!worthy` | live |
| `@PEACEUSA` | 0x198d | 98082 | as above with `local_9c` | tag-variant delta, documented |
| `@NOTHINGWITHDRAW` | 0x1996 | 98093 | player picked "withdraw" from the shared menu, no adjacent enemy units found | live |
| `@MAYBEWITHDRAW` | 0x19bb | 98132 | counter-offer with demobilisation cost | live |
| `@NOTWITHDRAW` | 0x19af | 98127 | refuse | live |
| `@WITHDRAW` | 0x19a6 / 0x19c9 | 98172 / 98155 | comply (two sites: free and paid) | live (teleport-vs-walk delta documented) |
| `@THREATS`, `@NOCONTACT`, `@ALREADYSMITE`, `@SMITEINDIANS`, `@SMITEEUROPE`, `@UNFORTUNATE`, `@MERCENARY` | 0x19f2, 0x1a03, 0x1a0d, 0x1a1a, 0x1a27, 0x1a33, 0x1a3f | 98200+ | shared response menu / ally-hire tail | live |

`@RIDUSA` exists in GAME.TXT but no separate DS id — it is `RID`+`USA`
like the rest.

### The three formulas worth quoting

All from `FUN_5bfb_153e`; all three are already live in `ai_diplo.c` except
where noted. Kept here because the recon brief asked for them explicitly.

**1. Tone selector (`MEEK` vs `MANLY`) — `iVar6`.** Not a roll. Raw :97587
sets `iVar6 = 1` iff `local_a8` ("worthy" / threat armed) survived the
suppression gates; the greeting then picks `0x18b0` MEEK when `iVar6 == 0`,
`0x18b5` MANLY otherwise (raw :97688-97694). So MANLY = "this AI rates
itself strong enough to threaten you". Ghidra reuses the name `iVar6` as a
scratch between those two points, so treat the decompiled C alone as
suggestive — but the port's independent asm-audited derivation agrees
(`ai_diplo.c:2805`, `k->manly = w.worthy ? 1 : 0`), which is what makes this
solid. The earlier user-testimony guess ("war tone mirrors the peace-offer
tone") is consistent but is not what the code does: every site re-reads the
same one flag, so it is identity, not mirroring.

**2. Tribute / bribe amount — `local_68`** (raw :97514-97580, ported at
`ai_diplo.c:1778-1843`):

```
base   = combat_delta_sum accumulated over the colony/unit sweep
if (armed) base = clamp(base, difficulty*200 + 100, 0x26ac)   /* :97513 */
scaled = ((difficulty + 8) * base * 10) / 100                 /* :97539 */
if (!at_war) {                                                /* local_ae == 0 */
    base = scaled >> 2
    if (dominance >= 0) {
        if (turn <  0x32) scaled >>= 1
        else if (turn < 100) scaled -= base
        base = scaled
        if (colonies[self] < 3 && tax[self] < 8) base >>= 1
    }
} else base = scaled << 1
base = RNG(((difficulty + 1) * base) >> 3, 0) * 0x32          /* :97560 */
if (crown_armed) base += (difficulty + 1) * 500
/* affordability walk-down to 50-gold granularity, then */
if (has_FF(self, 0x13)) base >>= 1                            /* FF 19 = Benjamin Franklin */
```

Cheap sanity anchor: `difficulty` is `DS:0x53a6`, `turn` is `DS:0x538e`.

**3. Difficulty/turn suppression of the whole threat track** (raw
:97530-97536, `ai_diplo.c:1805`): when not already at war,
`thr = (difficulty − 10) * −10`; if `turn <= thr && turn != thr` the
threat arm is disarmed entirely. On Discoverer (`difficulty == 0`) that is
`thr = 100` — no tribute/threat events at all for the first 100 turns.

### Gap 1 — the `@RID` / post-tribute `@WARMANLY` arm is unported

DOS, raw :97954-97982 (structure confirmed against the OVL16 listing in
`euro_diplo_153e_full.md:1313-1340`, `LAB_OVL16_L0040__0029fc`):

```
if (worthy) {
    if (at_peace && score >= 0x65) { tag = @PROVOKE; goto declare; }
    /* falls through */
}
if (worthy && score == 999) {          /* i.e. a TRIBUTE dialog was shown */
    prep @LEADER2 (0x1938); sound 4; tag = @WARMANLY;
declare:
    popup(tag); FUN_281f_0a10(self, target, 0x40);   /* clear PEACE -> war */
} else if (worthy) {
    prep @LEADER2 (0x1949); popup(@RID);              /* ultimatum, NO war */
}
```

Linux `ai_diplo.c:2290-2296` implements only the `@PROVOKE` leg and then
falls straight to `AI_TALK_ST_PEACEMENU`. Consequences:

- `@RID` ("…we order you to leave {%STRING1} immediately. If you do not, we
  shall drive you into the sea.") never fires — it is the *only* one of the
  17 entry tags with no Linux site at all.
- The post-tribute war declaration is silently skipped: a "worthy" AI that
  already ran a `@TRIBUTE` dialog (which unconditionally stamps
  `score = 999`) should declare war; in Linux it just opens the peace menu.

This is the answer to the brief's "war-declare dispatch is completely
unknown": it is these three lines, and it is a real behavioural defect, not
a chrome gap. `@RID`'s tokens are `%STRING0` = `@LEADER2`-prepped name for
`target` and `%STRING1` = the nation string at `target*0x34 + 0x5426`;
**which** field `+0x5426` is (adjective vs. colony name) is *not verified* —
check before wiring, the GAME.TXT wording ("leave {X}") reads like a place.

---

## Part 2 — what `3f41` is: per-function table

Segment `3f41` = `OVL06_L0040` (`tools/address_mapping.csv:2051-2075`; file
offset = segment offset + 0x400). 18 functions, 0x0000–0x2af0, then the
far-thunk table at 0x2af0.

Every entry point is reached through segment `291f`'s far-thunk table; the
report menu dispatcher is `FUN_2f2b_…` at `viceroy_unpacked.c:55654-55686`,
keyed on `param_1` 0x13c–0x144.

| sym | bytes | raw lines | thunk | what it draws | key DS operands | ported? |
|---|---|---|---|---|---|---|
| `0000` | 138 | 69397-69420 | `291f_0f4a` | **Shared plate bring-up.** Builds `"REPORT"` + plate number (`0x11a2` + appended int) → `REPORT<N>.PIK`, loads into the `0x2da8`/`0x2daa`/`0x2dac`/`0x2dae` UI box, then applies the 256×3 palette (`local_354[768]`) via `FUN_1c2e_0022` with `DS:0x372` temporarily zeroed | `0x11a2` = `"REPORT"` (art basename, **not** a GAME.TXT tag) | yes — `reports_load()` |
| `008a` | 128 | 69424-69448 | `291f_0ee8` | **Shared OK button.** Callers pass `(0xfffe, 0xffff)`; defaults resolve to x=`0x11e` (286), y=`0xb8` (184); label = @MISC 46 `"OK"` | `0x2e16` = @MISC 46 | yes — `reports_render_ok_button` |
| `010a` | 1294 | 69451-69608 | `291f_041a` (key 0x143) | **F9 Indian Adviser.** 8 tribe rows, plate 1 | met gate `0a38&0x20`; extinct `[0x8d4e+3]&0x80`; tension `(alarm + n*−0x19)/5`, flipped `4−x` when `n>0`, clamped 0..4; settlement-name triple at `−0x69ce/−0x69cc/−0x69ca` (stride 6 by `[0x8d4e+2]`); villages from `0x539a`/`0x54ee` stride 0x12; missions = `[0x8d4e+7]` + units of type 0x14/0x16 → `× 0x32`; horses `[0x8d4e+8]` @MISC 45; @MISC 130 `"Extinct"` | yes |
| `0618` | 184 | 69611-69647 | `291f_040c` (key 0x13c) | **F2 Religious Adviser.** Plate 2, crosses progress bar `FUN_281f_0236(1,0,0,300,0x19)` | @MISC 30 title; cheat-only `"(%d of %d)"` (`0x11a9`) from nation `+0x2e`/`+0x30`, gated `DS:0x5383 & 0x20` | yes (cheat line no) |
| `06d0` | 1646 | 69650-69821 | `291f_03fe` (key 0x13d) | **F3 Continental Congress**, plate 3. Bells header, Rebel/Tory bar, Expeditionary/Intervention force, FF list | `0x5382&1` = WoI → @MISC 113 else @MISC 112 + FF name (`−0x69ae`, stride 6); SoL `0x53d0` and `100−0x53d0`; @MISC 69/70/71; force sums `0x53da[4]` / `0x53e2[4]`; @MISC 85 / 111 / 89; 25 FFs via `FUN_281f_07b4` | yes |
| `0ae6` | (split) | 69824-69911 | — | **Ghidra split tail of `06d0`** (`unaff_BP` frame). Not a real function; its 4 spurious "callers" (`:47815`, `:55216`, `:62435`, `:67562`) are decompiler noise | FF grid: col step `0x4e` = 78px, 4 cols | yes |
| `0d3e` | 922 | 69914-70055 | `291f_0f3c` | **F4 Labor zoom** (one profession). Profession `0x13` remapped to `0x1c` | colonies `0x539e` (rec ptr `0x8542`, `+0x1a` owner, `+0x1f` pop, slot prof via `0c54`); map units `0x539c` stride 0x1c (`+0x3147&0xf` owner, `+0x315b` prof); prof names `−0x715c` stride 8; @MISC 53/54/55 columns | yes |
| `10d8` | 864 | 70058-70178 | `291f_03f0` (key 0x13e) | **F4 Labor grid.** 29 slots, skips ids 0x12/0x13/0x17, renders 0x1c in slot 0x13; hit cells 0x69×0x12; click → `0d3e` | @MISC 49 + @MISC 56 `"(Click on item to zoom)"` | yes |
| `1438` | 280 | 70181-70209 | `291f_0ef6` | **F5 header, page 2**: @MISC 50 + @MISC 207 `"Cargo in Port"` | — | yes |
| `1550` | 448 | 70212-70278 | `291f_0f2e` | **F5 page 2 body.** 16 goods × up to 16 colonies; ≥1000 → `n/1000` + @MISC 57 `"K"` | colony stock at `[0x8542 + 0x9a + g*2]` | yes |
| `1710` | 1156 | 70281-70423 | `291f_03e2` (key 0x13f) | **F5 Economic Adviser**, plate 5, page 1 "European Trade". Tail chains to `1550` | 32-bit accumulators `−0x773c` (tons) and `−0x777c` (gold), index `(nation*0x4f + good)*4`; `"$"` = `0x11b4`; bid `291f_09ea`, ask `291f_0c3e`; @MISC 58/59/203/204/206 | yes |
| `1b94` | 88 | 70426-70440 | `291f_0f04` | **F6 header, page 2**: @MISC 51 + @MISC 209 `"Sons of Liberty"` | — | yes |
| `1bec` | 660 | 70443-70535 | `291f_0f20` | **F6 page 2 body.** Per-colony SoL % + current build (`09fc(0x14)` then `09fc(0x13)`), colonist icons where `0c0e(slot) == 0x11`; 9 rows/page | `0x9072` fallback string | yes |
| `1e80` | 88 | 70538-70552 | `291f_0f58` | **F6 header, page 1**: @MISC 51 + @MISC 208 `"Military Garrisons"` | — | yes |
| `1ed8` | 476 | 70555-70627 | `291f_03d4` (key 0x140) | **F6 Colony Adviser**, plate 6, page 1. Garrison icons per colony, skips unit types 0x0d..0x12 (ships), row pitch `0xd2 / max(1,count)` clamped 1..0x12, x cut-off 0x12d; 9 rows/page; tail chains to `1bec` | unit-flag table `0x5236` stride 0xe | yes |
| `20b4` | 344 | 70630-70672 | `291f_0f12` | **F7 header**: @MISC 52 + column labels @MISC 61/62/63/64 (`Ship`/`Cargo`/`Location`/`Destination`) | — | yes |
| `220c` | 828 | 70675-70784 | `291f_03c6` (key 0x141) | **F7 Naval Adviser**, plate 7. Ships only (type outside 0x0d..0x12 must pass `FUN_281f_0768`); cargo icons per `+0x3150`; unit names `0x5230` stride 0xe; location `291f_0f82`; destination when `+0x314c ∈ {2,3,0xb}` else home/Europe name; 7 rows/page | nation names `param*0x34+0x5426`, `−0x7c74` | yes |
| `2548` | 1448 | 70787-70945 | `291f_03b8` (key 0x142) | **F8 Foreign Affairs Adviser**, plate 8. 4 fixed nation blocks | `0x5382&1` → `@FOREIGNNOTAVAIL` early-out (below); crown slot `0x53d2` → @MISC 190; `nation_flags & 4` → @MISC 191 `"Free"`; relation byte `0a38(a,b)` bit 0x20 met / 0x40 peace → @MISC 102 `"Peace"` else 101 `"War"`; de Witt grid gated on `07b4(viewer,4) \|\| 0x53a2`: @MISC 95-100 (`Colonies`/`Population`/`Average Colony`/`Military Power`/`Naval Power`/`Merchant Marine`); Rebels/Tories @MISC 86/87 | yes (except the WoI gate) |

**Caution on `2548`'s operands.** `docs/reports.md` F8 records that Ghidra
drops and reorders this function's pushed values, so its decompiled C
mis-pairs labels with numbers; the port was re-derived from raw
`.asm 3f41:2548..2aca` on 2026-08-31. The `2548` row above therefore lists
only the gates and label ids, not the arithmetic — take formulas from
`docs/reports.md` F8, not from the decompiled C.

Key `0x144` is F10 Score → `FUN_291f_03aa` → `FUN_41f2_0092`, which is **not**
in this overlay; with cheats on (`0x5383 & 0x20`) it dispatches
`FUN_281f_0574` instead.

### Cheat-mode extras (`DS:0x5383 & 0x20`)

`DS:0x5383` bit 5 is `game_options.cheats_enabled`
(`docs/save_format_map.md:108`). Three report sites read it, and none are
ported. Treated here as **inference from that one mapping row** — the bit's
identity was not independently re-derived this pass:

- `0618`: F2 prints the exact crosses counter `"(%d of %d)"` from nation
  `+0x2e` / `+0x30`.
- `06d0`: F3 prints an exact bells figure using @MISC 26 `"in"`
  (`local_56 − min(...)` then `local_56`).
- dispatcher: F10 goes to `FUN_281f_0574` instead of the score screen.

Low value; recorded so a future pass doesn't rediscover them as "missing".

### Gap 2 — `@FOREIGNNOTAVAIL` is unported

`FUN_3f41_2548`'s first statement (raw :70792):

```c
if ((*(byte *)0x5382 & 1) != 0) { FUN_281f_0652(0x3f41, 0x11b6, 1); return; }
```

`0x5382` bit 0 = `head.game_options.woi`. GAME.TXT `@FOREIGNNOTAVAIL`: *"The
Foreign Affairs Adviser's report is no longer available once the {War of
Independence} has begun."* `src/core/reports.c` has no WoI gate on
`COLONIZE_REPORT_FOREIGN` (only F3 Congress reads `game_options.woi`, at
`reports.c:1203`), so the port still renders the full F8 after declaring.
`"FOREIGNNOTAVAIL"` is registered in `popup_msg.c:257` as a 1-button popup
already, so wiring is a one-line gate plus the popup call.

---

## Proposed port shape

There is no new module to build. Two targeted edits, in modules that
already own the behaviour:

1. **`src/core/ai_diplo.c`, `AI_TALK_ST_WORTHY`** (~2290): add the two
   missing legs of the DOS `worthy` cascade — `score == 999` →
   `@WARMANLY` + `ai_diplo_clear_both(..., AI_DIPLO_PEACE)`; else →
   `@RID` OK popup with no state change. Reuse `ai_talk_ok`,
   `ai_diplo_clear_both`, and the existing `tok` token block. Verify the
   `@RID` `%STRING1` field before wiring (see Gap 1).
2. **`src/core/reports.c`, F8 render entry**: early-out to the
   `@FOREIGNNOTAVAIL` popup when `col1->head.game_options.woi` — the popup
   already exists in `popup_msg.c`; the gate belongs wherever
   `game_loop.c` opens `COLONIZE_REPORT_FOREIGN`, so the report screen is
   never entered rather than entered-and-blanked (DOS returns before any
   plate load).

Reuse points already in place: `ai_diplo_153e_encounter` is the full talk
machine (all stages, all tags), `reports_render()` dispatches all F2–F9.
Nothing in `3f41` needs re-recovering.

## Blockers / unverified

- ~~`@RID`'s `%STRING1` operand (`target*0x34 + 0x5426`) — field identity not
  confirmed.~~ **RESOLVED 2026-09-08: it is `player[target].country_name`**
  ("New France"), not a colony name. DS players base is `0x540e`
  (`player.name` — the greeting at raw :97700 appends
  `param_2*0x34 + 0x540e` for its `%STRING0`), `country_name` sits at
  `+0x18` = `0x5426`. Anchored twice more: `col1_save.h` puts `unknown06` at
  `nation*0x34 + 0x543e` (= name+0x30), and `COLONY00.SAV` has a 158-byte
  head, players at file `0x9e`, `country_name` at `0xb6` → DS base `0x5370`
  (which also lands `difficulty` on `0x53a6` = 0 and `year` on `0x538a` =
  1492). `ai_diplo_rival_name` already returns `country_name`, so the port
  needed no new accessor. *(Note: `declaration.h`'s "DS:0x53f6 + slot*0x34 +
  0x18" for the same field has a base typo — it should read `0x540e`.)*
  Gap 1 shipped 2026-09-08 in `ai_diplo.c` `AI_TALK_ST_WORTHY`; Gap 2
  shipped in `reports.c` / `game_loop.c`.
- Also resolved on the way: the name-prep thunk `FUN_2a1f_0618(slot, base,
  nation)` prepends **`GREAT`** to its DS base word, so `"LEADER2"` →
  GAME.TXT `@GREATLEADER2` ("the Queen / the King / the Pope / the
  Stadtholder"), `"KINGS"` → `@GREATKINGS`, `"DEEDS"` → `@GREATDEEDS`.
  That is `@RID`/`@WARMANLY`'s `%STRING0`; `ai_talk_great_line` reads it.
- New DOS detail found in the same tail (raw :98017, OVL16 listing :1377):
  showing the `@GIVECASH` offer strcpy's `"MEEK"` over the shared tone
  buffer, so the refusal that follows is always `@WARMEEK` — the encounter
  tone only survives on the no-offer path.
- The cheat-bit reading of `DS:0x5383 & 0x20` rests on a single existing
  doc row, not a fresh derivation.
- No build/test was run this pass (disk constraint); all findings are
  static reads.
