# Popup authenticity audit

STATUS: audit dump, not a spec. Most flagged rows are resolved in place
(2026-09-28 sweep: bugs.md #984-990, all Fixed) or explicitly Demoted/PARKED
by design. One open item remains: "Village-approach warning CHOICE" (below)
has no matching `GAME.TXT` section and is still unresolved as of 2026-09-28.

Cross-check of **currently ported** player-facing modals against
`COLONIZE/GAME.TXT` / `DEBUG.TXT`. Goal: no invented wood dialogs; bodies and
choices from data where a real `@SECTION` exists.

**Verdicts**

| Verdict | Meaning |
|---------|---------|
| **Authentic** | Real `@SECTION`; port loads body (and choices when applicable) via `popup_msg_*` (`popup_msg_section_body` / `popup_msg_choices` / `popup_msg_fill`) |
| **MissingWire** | Section exists; port still uses hardcoded English (fixable) |
| **Mismatch** | Wrong section or wrong choice set vs GAME.TXT |
| **Invented** | No DOS wood dialog for this chrome; demote to status or remove modal |
| **PARKED** | Deep DOS UI deferred (FA `3f41`, deep trade); thin CHOICE may remain for gameplay apply only |

## Dedicated / map / EOT

| Site | Port | Section | Verdict | Action |
|------|------|---------|---------|--------|
| Quit / title exit | `game_enqueue_yes_no` | `@DOS` | Authentic | — |
| Retire | same | `@RETIRE` | Authentic | — |
| Disband land/ship | same | `@SUREDISBAND` / `@DISBANDSHIP` | Authentic | `@DISBANDSHIP` = cargo-blocked error OK; confirm always `@SUREDISBAND` |
| Overboard | `game_request_overboard_confirm` | `@OVERBOARD` | Authentic | Hold picker, not a Yes/No (bugs.md #984) |
| Trade delete | same | `@SUREDELETE` | Authentic | — |
| Find colony / trade select | `cheat_list` + `popup_msg_fill` | `@FINDCITY` / `@TRADE` | Authentic | — |
| Howmuch | `howmuch_dialog` | `@HOWMUCH*` | Authentic | — |
| Options | `options_dialog` | `@GAMEOPTIONS` / `@COLONYOPTIONS` / `@SOUNDOPTIONS` | Authentic | — |
| Found / rename / Land Ho name | `name_entry` | `@COLONY` / `@RENAMECOLONY` / `@LANDHO` | Authentic | — |
| Landfall CHOICE | `game_loop` LANDFALL | `@LANDFALL` / `@LANDFALL2` | Authentic | — |
| Stockade min-pop OK | `game_loop` colony | `@KEEPSTOCKADE` | Authentic | — |
| Abandon confirm | `colony_screen` | `@ABANDON` / `@ABANDON2` | Authentic | — |
| Building slot max | `colony.c` / `game_loop` | `@MORETHANTHREE` | Authentic | Assign-to-building now caps at 3 workers and shows this |
| EOT starve/spoil/ship/warn | `turn.c` | `@STARVE1` / `@SPOIL1` / `@CARGOREADY0` / `@WARN*` | Authentic | — |
| Pick music | `pick_music` | `@PICKMUSIC` | Authentic | — |
| Save/load title | `save_load_dialog` | `@SAVEGAME` / `@LOADGAME` | Authentic | Title + @width=190 from GAME.TXT; rows per FUN_7562_0052 ("Diff Leader of the Nation, Season Year" / "(EMPTY)") |
| Unit stack | `unit_stack` | — | n/a (unit names) | — |
| FF debate CHOICE | `founding_fathers.c` | `@WHICHFREEDOM` | Authentic | Choices remain FF names |
| FF elect OK | same | `@FREEDOM` | Authentic | — |

## Contact (`ai_contact.c`)

| Site | Section | Verdict | Action |
|------|---------|---------|--------|
| Welcome Yes/No | `@INDIANWELCOME` | Authentic | `popup_msg_fill` tribe/pop/braves tokens |
| Peace / come / shun OK | `@INDIANPEACE` / `@INDIANCOME` / `@INDIANSHUN` | Authentic | — |
| Ship unmet | `@DONTKNOWSHIPS` | Authentic | — |
| Ship mad | `@MADATSHIPS` | Authentic | — |
| Colony encroachment OK | `@INDIANCOMMENT` | Authentic | Tribe/colony tokens |
| Village action menu CHOICE | `@VILLAGEHAPPY/SAVAGE/MEDIUM/BAD/WAR` + NAMES `@ACTIONS` rows | Authentic (2026-08-28) | `ai_contact_enqueue_village_meet`: DOS `FUN_4d56_4528` human arm, per-unit row gating; invented Gift row + hand-typed "welcomes the most worthy" body gone — [indian_actions_menu.md](../original_sources_annotated/ai/indian_actions_menu.md) |
| Live Among The Natives | `@LEARNSTAY` / `@LEARNLATER` / `@LEARNDONE` / `@LEARNSLOW` / `@TEACHCONVERT` | Authentic (2026-08-28) | `ai_contact_live_among_natives` (`thunk_FUN_1000_a618`); LEARNSTAY is a real Yes/No CHOICE (`AI_POPUP_TAG_CONTACT_LEARNSTAY`) |
| Ask to Speak With Chief | `@CHIEFHOWDY` / `@CHIEFGUIDES` + `@WELLSEASONED` / `@CHIEFAREA` / `@CHIEFGIFT` / `@CHIEFBORED` / `@CHIEFKILL` | Authentic (2026-08-28) | `ai_contact_speak_with_chief` (`thunk_FUN_1000_a60c`) |
| Demand Tribute | `@EXTORTSTUFF` / `@EXTORTPOOR` / `@EXTORTNO` / `@EXTORTLAUGH` | Authentic (2026-08-28) | `ai_contact_demand_tribute` (`thunk_FUN_1000_a5f4`) |
| Denounce Heresy / Establish Mission (menu) | `@HERESY0` / `@HERESY1` / `@MISSION0..3` | Authentic (2026-08-28) | `ai_contact_denounce_heresy` (`a594`), `ai_contact_establish_mission` (`a5dc`); AI convert pulse unchanged |
| Enter Hostile Village | `@KILLWAGONS` / `@MADATWAGONS` / `@GRUDGEWAGONS` | Authentic (2026-08-28) | `ai_contact_enter_hostile_village` (`a5e8`) then the 2820 trade arm |
| Encroachment CHOICE | `@INDIANLAND` / `@INDIANFOREST` / `@INDIANROAD` → `@INDIANBRIBE` | Authentic (2026-08-28) | `game_loop.c` `game_request_indian_land_choice` (`AI_POPUP_TAG_INDIAN_LAND`); "offer gold" row dropped when unaffordable (DOS greys it) |
| Village-approach warning CHOICE ("Leave" / "Attack") | *none* | **Invented** (found 2026-09-20) | The four alarm-tier bodies and both choice rows have no GAME.TXT section — grepping every distinctive phrase ("shout warnings", "bar the path", "blood is shed", "fall back") returns nothing, and the site never calls `popup_msg_fill`. Either find the real DOS chrome or demote it; do not wire an unrelated section to it |
| Teach refuse OK | `@LEARNMAD` | Authentic | Both mid (40-54) and hostile (≥55) alarm bands |
| Teach already-expert OK | `@LEARNMASTER` | Authentic | Refuses without consuming the village's one-shot teach |
| Teach / convert / raid OK (remainder) | `@LEARNALREADY` / … | Authentic for the menu path (2026-08-28) | `@LEARNALREADY` now shows from the Live Among menu action (DOS: taught && !capital); the AI-only auto pulse still skips silently — see `indian_contact.md` "preserve gift/trade chrome" note; not touched here, a real design tension not an oversight. **`@RAID*` fixed 2026-08-26** (was cited here as MissingWire): 6 of 7 kinds now render the real `GAME.TXT` body via `popup_msg_fill` — see `port_plan.md` P8.4 / `indian_raid_outcomes.md` |
| Teach: Petty Criminal refuse | `@LEARNCRIMINAL` | Authentic (2026-08-26) | `ai_contact_teach_skill` refuses outright, one-shot not consumed (`ai_contact.c` `ai_contact_is_petty_criminal`) |

## Diplo (`ai_diplo.c`)

| Site | Section | Verdict | Action |
|------|---------|---------|--------|
| Peace/war/alliance/break CHOICE | FA `3f41` PARKED | PARKED structural | Keep Accept/Refuse for apply; no fake GAME.TXT body |
| Refuse follow-up INFO OK | — | Invented → status | Demoted |
| War upkeep INFO OK | — | Invented → status | Demoted |
| Boycott / Tools-lift INFO OK | — | Invented → status | Demoted |
| Privateer commission | — | Invented (status only) | Status-only |
| Privateer prize INFO OK | — | Invented → status | Demoted |
| FA gift/longevity INFO OK | — | Invented → status | Demoted |
| `@DECLAREWAR` OK | `@DECLAREWAR` | Authentic | Base war-declared line now `popup_msg_fill("DECLAREWAR", …)`; boycott/hostility chrome may still override with a more specific status |
| `@SIGNTREATY` OK | `@SIGNTREATY` | Authentic | Base peace-concluded line now `popup_msg_fill("SIGNTREATY", …)`; Tools-embargo-lift chrome may still override |
| `@CANCELPEACE` CHOICE prompt | `@CANCELPEACE` | Authentic | 10ec AI→human war-declare CHOICE body (was invented "%s declares war!"); `popup_msg_fill` |

## King (`ai_king.c`)

| Site | Section | Verdict | Action |
|------|---------|---------|--------|
| Tax audience CHOICE | `@KINGTAX` + `@TAXOPTIONS` | Authentic | Kiss pinky / Hold Tea Party |
| Tea party / refuse OK | `@TEAPARTY` | Authentic | Wired on refuse (no dump CHOICE) + dump-goods apply; thin `3dc8` stock dump |
| Crown frigate offer CHOICE | `@KINGFRIGATE` | Authentic | 2026-08-29 `ai_king_frigate_offer` (FUN_3844_00f2 tail): Yes → Frigate sails from Europe + `@KINGTAX` +10%; No → nothing |
| Independence letter | `@INDEPENDENCE` | Authentic | — |
| Declare independence CHOICE | `@DECLARE` | Authentic | Never / Yes; STRING0 = motherland |
| Merc offer CHOICE | `@MERCENARIES` | Authentic | No thank you / Pay |
| Merc arrive OK | `@MERCS` | Authentic | — |
| REF arrival | `@INVASION` | Authentic | REF `1528` wave; STRING0 = colony name |
| Merc decline / cannot-afford OK | — | Invented → status | Demoted |
| Colonial Era Ends OK | — | Invented → status | Demoted |
| Peacetime 1800 score CHOICE | `@SCORED` | Authentic | That's all / Keep playing; retire on That's all |
| Peacetime retire prose | `@RETIRING` | Authentic | That's all apply → estate near richest colony |
| Anniversary soon-retire (1790) | `@SOONRETIRING0` | Authentic | Spring peacetime; `market_demand_pool_raw[8]` |
| Anniversary soon-retire (1840) | `@SOONRETIRING1` | Authentic | wartime WoI; `market_demand_pool_raw[9]` |
| WoI begins / restless | various | Invented → status | Restless demoted to status-only |
| REF capture | `@CAPTURED3` | Authentic | REF take without plunder |
| Foreign intervene | `@INTERVENTION` / `@INTERVENE` | Authentic | declare + landing ARRIVAL |
| Declare war briefing | `@HOWTOWIN` | Authentic | after `@INDEPENDENCE` letter; invent WoI-begins demoted |
| Revolution win | `@WINNING` | Authentic | year≥1850 + no crown |
| Revolution stalemate (1850) | `@RETIRING2` | Authentic | year≥1850 + crown still alive |
| Revolution lose (ports) | `@LOSING1` | Authentic | all coastal ports lost (inland may remain) |
| Revolution lose (colonies) | `@LOSING2` | Authentic | all colonies lost |
| Mid-war port warn | `@WARN1` | Authentic | ports<3; lowest arm of the one-per-turn selector; `market_demand_pool_raw[6]` episode |
| Mid-war colony warn | `@WARN2` | Authentic | colonies<3; wins the selector (last write, raw 58530); `market_demand_pool_raw[7]` episode |
| Mid-war pop warn | `@WARN3` | Authentic | crown pop share ≥80% (<90%); `market_demand_pool_raw[10]` |
| Revolution lose (pop) | `@LOSING3` | Authentic | crown pop share ≥90% |

## Remediation completed in this pass

- Choice extraction: recognize LANDFALL / ABANDON / MERCENARIES / TAXOPTIONS labels; `%%` → `%`
- Wired: `@LANDFALL`, `@ABANDON`/`@ABANDON2`, `@KEEPSTOCKADE`, `@MORETHANTHREE`, `@DONTKNOWSHIPS`, `@MADATSHIPS`, `@INDIANCOMMENT`, `@WHICHFREEDOM`, `@FREEDOM`, `@KINGTAX`+`@TAXOPTIONS`, `@MERCENARIES`, `@MERCS`, `@LOSTCITY1`-`9`/`@BURIAL1`-`3`/`@SCREWED`, `@DECLAREWAR`, `@SIGNTREATY`, `@CANCELPEACE`, `@LEARNMAD`, `@LEARNMASTER`, `@BURNED3`, `@LOOTCASH`
- Demoted invented INFO OKs: Privateer prize, war upkeep, FA gift/holds, diplo refuse follow-ups, colonial-era end, merc decline/cannot-afford

2026-09-16 wave: the war/peace OK popups now always carry the `@DECLAREWAR` /
`@SIGNTREATY` body — the invented "<cargo> boycott imposed." / "Tools embargo
lifted." overrides were dead or port chrome and now touch only the status
line (`DIPLO_BOYCOTT` has no producer). Every remaining Partial row in
[popups_catalog.md](popups_catalog.md) Appendix A names its DOS site and blocker.

## 2026-09-28 sweep (mechanical, whole tree)

Method: parse every `ai_popup_enqueue_ok/_ok_ctx/_choice/_choice_ctx/_colony_event`
and `game_enqueue_yes_no` call, resolve the body argument back to its writer in
the enclosing function, and flag any body/label that is a literal or an
`snprintf` of typed English instead of `popup_msg_*` over a `COLONIZE/*.TXT`
section. 152 popup sites; all but the rows below resolve to a real `@SECTION`.

| Site | Verdict | Row | Fixed |
|------|---------|-----|-------|
| Throw Cargo Overboard (`game_dialogs.c`) | **Invented** labels + wrong shape (`@OVERBOARD` is a cargo picker) | bugs.md #984 | Rebuilt as the @TRADEWICH-shaped hold picker (`AI_POPUP_TAG_OVERBOARD_WHICH`); typed labels gone |
| Foreign-colony attack confirm (`game_loop_orders.c`) | **Invented** body + labels | bugs.md #985 | Deleted — DOS's only gate is FUN_465b_0000 (@HAVETREATY / open hostilities), which already runs on that move; tag 63 retired |
| Privateer prize notice (`units_combat_resolve.c`) | **Invented** (self-declared port-authored) | bugs.md #986 | Popup dropped; the Royal-Navy arm keeps `@SEIZURESEA` |
| Raid ship-sunk, no-port arm (`units_combat.c`) | **Invented** wording vs `@SHIPSUNK` | bugs.md #987 | Renders `@SHIPSUNK` with the loser tokens and empty sinker pair; the coastal-fort arm's typed "shore"/"batteries" became the firing nation + empty hull |
| Diplo ally-pick body (`ai_diplo.c`) | **Invented** body; Accept/Refuse labels PARKED | bugs.md #988 | Body blank until the section/numeric id is found; labels unchanged (parked apply stand-in) |
| 8 × typed `popup_msg_fill` fallback bodies | Rule violation, dead text | bugs.md #989 | All `""` now, including the `europe->status`-as-fallback chain in `turn_production.c` (`turn_emit_built_chrome` lost the parameter) |
| Chrome headings/titles | **Invented** although LABELS.TXT rows exist | bugs.md #990 | `@CMISC[1]` "Units Present" for the stack title; `@CTITLE[4]/[5]/[6]` for the build picker heading, clear row and More row; "Leave as" heading and both cheat prompts now draw nothing |

Residual, deliberately left: `unit_stack.c` annotates a unit aboard a ship
with "aboard"/"ready" — typed English, but no catalog row carries either word
and dropping it would lose the state cue. Needs the DOS stack-list read.

Clean by construction: the popup-choice label arrays (only `ai_diplo.c:3411`
holds literals), `popup_msg_fill` section names, and every `turn_production.c`
colony-event body. Status-line text (`europe->status`, `set_status`) is port
chrome and out of scope here.
