# Popup authenticity audit

Cross-check of **currently ported** player-facing modals against
`COLONIZE/GAME.TXT` / `DEBUG.TXT`. Goal: no invented wood dialogs; bodies and
choices from data where a real `@SECTION` exists.

**Verdicts**

| Verdict | Meaning |
|---------|---------|
| **Authentic** | Real `@SECTION`; port loads body (and choices when applicable) via `popup_msg_*` / `ai_contact_msg_body` |
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
| Overboard | same | `@OVERBOARD` | Authentic | — |
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
| Save/load title | `save_load_dialog` | — | Invented title strings | Keep UI; titles cosmetic |
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
| Meet / gift / demand CHOICE | (deep HELLO/trade PARKED) | PARKED / Invented body | Keep structural CHOICE; no new invent |
| Teach refuse OK | `@LEARNMAD` | Authentic | Both mid (40-54) and hostile (≥55) alarm bands |
| Teach already-expert OK | `@LEARNMASTER` | Authentic | Refuses without consuming the village's one-shot teach |
| Teach / convert / raid OK (remainder) | `@LEARNALREADY` / … | MissingWire / PARKED | Prefer msg_body where clear; else status. `@LEARNALREADY` (already-taught village, non-capital) deliberately stays silent — see `indian_contact.md` "preserve gift/trade chrome" note; not touched here, a real design tension not an oversight. **`@RAID*` fixed 2026-08-26** (was cited here as MissingWire): 6 of 7 kinds now render the real `GAME.TXT` body via `popup_msg_fill` — see `port_plan.md` P8.4 / `indian_raid_outcomes.md` |
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
| Independence letter | `@INDEPENDENCE` | Authentic | — |
| Declare independence CHOICE | `@DECLARE` | Authentic | Never / Yes; STRING0 = motherland |
| Merc offer CHOICE | `@MERCENARIES` | Authentic | No thank you / Pay |
| Merc arrive OK | `@MERCS` | Authentic | — |
| REF arrival | `@INVASION` | Authentic | REF `1528` wave; STRING0 = colony name |
| Merc decline / cannot-afford OK | — | Invented → status | Demoted |
| Colonial Era Ends OK | — | Invented → status | Demoted |
| Peacetime 1800 score CHOICE | `@SCORED` | Authentic | That's all / Keep playing; retire on That's all |
| Peacetime retire prose | `@RETIRING` | Authentic | That's all apply → estate near richest colony |
| Anniversary soon-retire (1790) | `@SOONRETIRING0` | Authentic | Spring peacetime; `unknown46[8]` |
| Anniversary soon-retire (1840) | `@SOONRETIRING1` | Authentic | wartime WoI; `unknown46[9]` |
| WoI begins / restless | various | Invented → status | Restless demoted to status-only |
| REF capture | `@CAPTURED3` | Authentic | REF take without plunder |
| Foreign intervene | `@INTERVENTION` / `@INTERVENE` | Authentic | declare + landing ARRIVAL |
| Declare war briefing | `@HOWTOWIN` | Authentic | after `@INDEPENDENCE` letter; invent WoI-begins demoted |
| Revolution win | `@WINNING` | Authentic | year≥1850 + no crown |
| Revolution stalemate (1850) | `@RETIRING2` | Authentic | year≥1850 + crown still alive |
| Revolution lose (ports) | `@LOSING1` | Authentic | all coastal ports lost (inland may remain) |
| Revolution lose (colonies) | `@LOSING2` | Authentic | all colonies lost |
| Mid-war port warn | `@WARN1` | Authentic | one coastal port left; `unknown46[6]` episode |
| Mid-war colony warn | `@WARN2` | Authentic | one colony left; `unknown46[7]` episode |
| Mid-war pop warn | `@WARN3` | Authentic | crown pop share 50–89%; `unknown46[10]` |
| Revolution lose (pop) | `@LOSING3` | Authentic | crown pop share ≥90% |

## Remediation completed in this pass

- Choice extraction: recognize LANDFALL / ABANDON / MERCENARIES / TAXOPTIONS labels; `%%` → `%`
- Wired: `@LANDFALL`, `@ABANDON`/`@ABANDON2`, `@KEEPSTOCKADE`, `@MORETHANTHREE`, `@DONTKNOWSHIPS`, `@MADATSHIPS`, `@INDIANCOMMENT`, `@WHICHFREEDOM`, `@FREEDOM`, `@KINGTAX`+`@TAXOPTIONS`, `@MERCENARIES`, `@MERCS`, `@LOSTCITY1`-`9`/`@BURIAL1`-`3`/`@SCREWED`, `@DECLAREWAR`, `@SIGNTREATY`, `@CANCELPEACE`, `@LEARNMAD`, `@LEARNMASTER`, `@BURNED3`, `@LOOTCASH`
- Demoted invented INFO OKs: Privateer prize, war upkeep, FA gift/holds, diplo refuse follow-ups, colonial-era end, merc decline/cannot-afford

Still Partial/PARKED: diplo Accept/Refuse CHOICE bodies (FA `3f41`), many contact teach/raid OKs, some king WoI chrome.
