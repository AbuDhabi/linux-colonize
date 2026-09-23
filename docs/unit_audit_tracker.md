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
| 2026-09-23 | Artillery / Damaged Artillery (props, purchase + @REALLYBUY, construction + AI tools gift, REF pool/wave/merc/intervention, combat peels + 0352 outcomes, fort fire, movement/transport/orders, AI 20e6/06ae/5c3c arms, reports/pedia/save) | #752-769 | see rows; #769 = static-unresolved leads |
| 2026-09-23 | Treasure Train (props, creation LCR/conquest/cheat, boarding/movement/combat, King's Galleon offer + Europe cash-in, AI arms, colony admit, save byte, UI) | #736-751 | 15 fixed (#736-750), #751 = 5 static-unresolved leads; audit corrected: 465b 0x10 bit = Privateer sighting, not treasure |

Open carry-over: #530 (AI first-colony opening scaffolding) came out of the Vet
Soldier audit and is still OPEN.

## Remaining: unit types (`@UNIT`)

Partly covered by earlier audits is noted; still needs its own pass.

| Row | Type | Notes |
|-----|------|-------|
| 12 | Wagon Train | wagon errand + 359c touched |
| 18 | Man-O-War | not in ship audit scope |
| 19 | Braves | D3 brave work touched AI side |
| 20 | Armed Braves | |
| 21 | Mounted Braves | |
| 22 | Mounted Warriors | |

## Remaining: expert professions (`@JOB`)

| Row | Profession |
|-----|-----------|
| 1-3 | Master Sugar / Tobacco / Cotton Planter |
| 4 | Expert Fur Trapper |
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
| 18 | Expert Teacher (schoolhouse partly covered by #506) |
| 27 | Indian Convert (touched by Servant/Criminal audit) |

Suggested grouping for future passes: field producers (1-7) together, craft
workers (9-15) together, then Preacher + Statesman, Teacher, Convert.
