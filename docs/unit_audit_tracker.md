# Unit-type audit tracker

STATUS: live tracker. Update when an audit starts or closes.

One audit = one unit type (or expert profession) checked end to end against the
decomp: human paths, AI arms, Europe/dock handling, combat, colony work, sidebar and
popup text, save round-trip. Findings go into bugs.md as rows prefixed
`<Type> audit:`; this file only records coverage.

Unit types come from `COLONIZE/NAMES.TXT` `@UNIT` (type byte = row); expert
professions from `@JOB` (profession byte = row). Identify by row, never by name.

## Done

| Date | Audit | bugs.md rows | Result |
|------|-------|--------------|--------|
| 2026-09-17 | Ships (Caravel, Merchantman, Galleon, Privateer, Frigate) | #481-490 | all fixed |
| 2026-09-18 | Scout / Seasoned Scout | #491-502 | 11 fixed, 1 refuted (#500) |
| 2026-09-18 | Indentured Servant / Petty Criminal | (in memory notes, no prefixed rows) | fixed: 5952 equip picker, carpenter arm, Convert score |
| 2026-09-18 | Soldier / Veteran Soldier | #503-529 | fixed; #514, #529 refuted |
| 2026-09-22 | Missionary / Jesuit Missionary | #555-561 | all fixed |
| 2026-09-22 | Farmer / Expert Farmer | #562-578 | all fixed |
| 2026-09-22 | Fisherman / Expert Fisherman | #579-609, #554 | all fixed |
| 2026-09-22 | Pioneer / Hardy Pioneer (all fitted forms, AI arms) | #610-644 | 33 fixed, #625 refuted (user: one Clear = open land), #644 lead open |
| 2026-09-22 | Dragoons / Veteran Dragoon / Continental Cavalry (props, combat, promote/demote, equip, AI) | #645-659 | 12 fixed, #652 refuted, #658 record-only, #659 lead (non-dragoon) |
| 2026-09-22 | REF Regulars / Cavalry (props, peels, outcomes, 0982 spawn/landing, crown-turn arms, display, save) | #660-665 | 5 fixed, #665 record-only; landing-MP claim refuted (+0x3149 = spent byte) |
| 2026-09-23 | Continental Army (props, combat, promote/demote/muster, equip, colony, UI, save, AI/crown arms, cheat) | #666-670 | 4 fixed, #669 record-only; combat/outcome/muster paths verified literal, no defect |
| 2026-09-23 | Colonists (@UNIT 0: Free Colonist / Servant / Criminal / Convert / specialists off-colony: movement, orders, Build/Join, goto, Europe immigration + docks, combat defender/capture, Live Among Natives, AI arms, sidebar) | #671-735 | 52 fixed, 6 refuted (#679 #723 #727 #731 #733 + Arctic premise), 7 OPEN leads (#680, #711/#712 tie #530, #715 cross-file, #732, #734, #735); #679 #733 refuted by user |
| 2026-09-23 | Braves / Armed Braves / Mtd. Braves / Mtd. Warriors (@UNIT 19-22: props, movement, 021a/1816 AI, 465b step, field attack, raids 0f14, combat outcomes, spawn/replacement 152e/0824, equipment stocks, display, save) | #822-850 | 25 fixed, #845/#846 refuted, #848/#849 lead bundles open (021a score plotter, transport_chain stack use, 0f14 building-arm counters, 0498/048e raid sounds, 0902/08d0 polarity) |
| 2026-09-23 | Artillery / Damaged Artillery (props, purchase + @REALLYBUY, construction + AI tools gift, REF pool/wave/merc/intervention, combat peels + 0352 outcomes, fort fire, movement/transport/orders, AI 20e6/06ae/5c3c arms, reports/pedia/save) | #752-769 | see rows; #769 = static-unresolved leads |
| 2026-09-23 | Master Sugar / Tobacco / Cotton Planter (@JOB 1-3: 18ec/17fa yield, 1f72 commons pick, production/craft order, OTJ learning + 0606 census, school tiers, Europe unreachability, village teach bid table, LCR/FoY, skill loss, 28c8/5952 AI arms, reports/score/sidebar/pedia, save, capture) | #770-781 | 10 fixed (#770 H: census dropped map units), #780 #781 record-only; yield pipeline verified literal |
| 2026-09-23 | Treasure Train (props, creation LCR/conquest/cheat, boarding/movement/combat, King's Galleon offer + Europe cash-in, AI arms, colony admit, save byte, UI) | #736-751 | 15 fixed (#736-750), #751 = 5 static-unresolved leads; audit corrected: 465b 0x10 bit = Privateer sighting, not treasure |
| 2026-09-23 | Wagon Train (@UNIT 12: props/catalog columns, construction + @NOMOREWAGONS cap, movement/colony enter, combat/capture, cargo model + colony strip load/unload, village trade 2820 + @INDIANWAGONS, foreign-colony trade, AI 20e6 wagon errand/load/457e, 021a/brave scoring, save round-trip) | #782-821 | 31 fixed, 2 refuted (#789 #812), #803 re-scoped OPEN, #818-821 follow-up leads |
| 2026-09-23 | Expert Teacher (@JOB 18, cut pre-release: Europe pool remap 38fd_46d4, school gate 364b_0688 level 4, 5952 indoor pass skip, village teach table 0..15, labor report/pedia hidden, sprite 99 + label 15eb_0002 kept as save tolerance) | none | no defect; type is unreachable in port and DOS, only occupation slot 18 (teaching) is live; rule in conventions.md `expert-teacher-cut-type` |
| 2026-09-23 | Man-O-War (@UNIT 18: props/catalog/holds/sight/build+buy exclusion, REF 0982 spawn+pool+landing tile+sail-home 20e6+5d04 seize, intervention 10f0 free arm + JPJ fallback, merc delivery 2022/2244/10f0 paid arm, combat 1b0e Bombard bit / 0352 outcomes + repair / 312e ship-slow + evade, Europe gate, reports/save) | #865-878 | 12 fixed (#865 H: DOS 0x5382 bit 0x02 = intervention announced, port set it at declare; #866 H: 0982 MoW self-goto freeze; #867 H: crown last MoW unsinkable, raw 99527-99570 gate block ported), #877 lead (4393 haul gate, ties #818), #878 L bundle partly done (e)(j)+leads open; Bombard +50% verified attacker-only global flag, no nearby-MoW effect on colony attacks |
| 2026-09-23 | Fur Trapper / Expert Fur Trapper (@JOB 4: 17fa/18ec fur pipeline incl. Game/Beaver/river/road pre-add + Hudson position, 1f72 commons, OTJ/school/village teaching, Europe unreachability 46d4 remap + price -1, fitted forms equip/combat/promotion/join/labels/save, AI 28c8 seating/0x864/5952/20e6, village gift/bid, craft consumption of furs) | #851-862, #819 | 12 fixed (#851 factory-tier input = colony-total floor 2:3, #855 pedia Beaver +3, #819 live gift price byte, #861 invented Silver-first export list deleted), leads #863 (beg-conceded falls through to gift, bVar7) #864 (export ship-load arm has no DOS body) |

Open carry-over: #530 (AI first-colony opening scaffolding) came out of the Vet
Soldier audit and is still OPEN.

## Remaining: unit types (`@UNIT`)

None left: every `@UNIT` row has had its own pass (Man-O-War closed the set 2026-09-23).

## Remaining: expert professions (`@JOB`)

| Row | Profession |
|-----|-----------|
| 5 | Expert Lumberjack |
| 6 | Expert Ore Miner |
| 7 | Expert Silver Miner |
| 9 | Master Distiller |
| 10 | Master Tobacconist |
| 11 | Master Weaver |
| 12 | Master Fur Trader |
| 13 | Master Carpenter |
| 14 | Master Blacksmith |
| 15 | Master Gunsmith |
| 16 | Firebrand Preacher |
| 17 | Elder Statesman |
| 27 | Indian Convert (touched by Servant/Criminal audit) |

Suggested grouping for future passes: field producers (1-7) together, craft
workers (9-15) together, then Preacher + Statesman, Teacher, Convert.
