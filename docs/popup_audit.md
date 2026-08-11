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
| Disband land/ship | same | `@SUREDISBAND` / `@DISBANDSHIP` | Authentic | — |
| Overboard | same | `@OVERBOARD` | Authentic | — |
| Trade delete | same | `@SUREDELETE` | Authentic | — |
| Find colony / trade select | `cheat_list` + `popup_msg_fill` | `@FINDCITY` / `@TRADE` | Authentic | — |
| Howmuch | `howmuch_dialog` | `@HOWMUCH*` | Authentic | — |
| Options | `options_dialog` | `@GAMEOPTIONS` / `@COLONYOPTIONS` / `@SOUNDOPTIONS` | Authentic | — |
| Found / rename / Land Ho name | `name_entry` | `@COLONY` / `@RENAMECOLONY` / `@LANDHO` | Authentic | — |
| Landfall CHOICE | `game_loop` LANDFALL | `@LANDFALL` / `@LANDFALL2` | Authentic | — |
| Stockade min-pop OK | `game_loop` colony | `@KEEPSTOCKADE` | Authentic | — |
| Abandon confirm | `colony_screen` | `@ABANDON` / `@ABANDON2` | Authentic | — |
| Building slot max | (if shown) | `@MORETHANTHREE` | MissingWire | Wire when shown |
| EOT starve/spoil/ship/warn | `turn.c` | `@STARVE1` / `@SPOIL1` / `@CARGOREADY0` / `@WARN*` | Authentic | — |
| Pick music | `pick_music` | `@PICKMUSIC` | Authentic | — |
| Save/load title | `save_load_dialog` | — | Invented title strings | Keep UI; titles cosmetic |
| Unit stack | `unit_stack` | — | n/a (unit names) | — |
| FF debate CHOICE | `founding_fathers.c` | `@WHICHFREEDOM` | Authentic | Choices remain FF names |
| FF elect OK | same | `@FREEDOM` | Authentic | — |

## Contact (`ai_contact.c`)

| Site | Section | Verdict | Action |
|------|---------|---------|--------|
| Welcome Yes/No | `@INDIANWELCOME` | Authentic | — |
| Peace / come / shun OK | `@INDIANPEACE` / `@INDIANCOME` / `@INDIANSHUN` | Authentic | — |
| Ship unmet | `@DONTKNOWSHIPS` | Authentic | — |
| Ship mad | `@MADATSHIPS` | Authentic | — |
| Colony encroachment OK | `@INDIANCOMMENT` | Authentic | Tribe/colony tokens |
| Meet / gift / demand CHOICE | (deep HELLO/trade PARKED) | PARKED / Invented body | Keep structural CHOICE; no new invent |
| Teach / convert / raid OK | `@LEARN*` / `@RAID*` / … | MissingWire / PARKED | Prefer msg_body where clear; else status |

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
| `@DECLAREWAR` OK | `@DECLAREWAR` | MissingWire | Optional wire later |

## King (`ai_king.c`)

| Site | Section | Verdict | Action |
|------|---------|---------|--------|
| Tax audience CHOICE | `@KINGTAX` + `@TAXOPTIONS` | Authentic | Kiss pinky / Hold Tea Party |
| Tea party / refuse OK | `@TEAPARTY` | MissingWire | Wire when dump/refuse fires |
| Independence letter | `@INDEPENDENCE` | Authentic | — |
| Declare independence CHOICE | `@DECLARE` | MissingWire | Wire body+choices |
| Merc offer CHOICE | `@MERCENARIES` | Authentic | No thank you / Pay |
| Merc arrive OK | `@MERCS` | Authentic | — |
| REF arrival | `@INVASION` | MissingWire | Wire |
| Merc decline / cannot-afford OK | — | Invented → status | Demoted |
| Colonial Era Ends OK | — | Invented → status | Demoted |
| WoI begins / capture / restless | various | Invented / Mismatch | Demote invents; wire clear sections only |

## Remediation completed in this pass

- Choice extraction: recognize LANDFALL / ABANDON / MERCENARIES / TAXOPTIONS labels; `%%` → `%`
- Wired: `@LANDFALL`, `@ABANDON`/`@ABANDON2`, `@KEEPSTOCKADE`, `@DONTKNOWSHIPS`, `@MADATSHIPS`, `@INDIANCOMMENT`, `@WHICHFREEDOM`, `@FREEDOM`, `@KINGTAX`+`@TAXOPTIONS`, `@MERCENARIES`, `@MERCS`
- Demoted invented INFO OKs: Privateer prize, war upkeep, FA gift/holds, diplo refuse follow-ups, colonial-era end, merc decline/cannot-afford

Still Partial/PARKED: diplo Accept/Refuse CHOICE bodies (FA `3f41`), many contact teach/raid OKs, some king WoI chrome.
