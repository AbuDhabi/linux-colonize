# AI popup unpark marathon (30 min)

- Started epoch: 1786161249
- Deadline epoch: 1786163049 (+30 min)
- Stopped: after R3 thin final (~13 min before deadline); all smokes green
- Goal: wire AI-spawned text/choice popups into Contact / King / Diplo

## Infra (parent) — Done
- `src/core/ai_popup.h/.c` — queue + OK/choice wood dialog (`popup_draw`)
- `ColonizeTurnContext.ai_popups` — enqueue during turn
- `game_loop` presents when idle; input/render/`ai_*_apply_popup_result`
- `smoke_ai_popup` green

## Rounds
- **R1:** King audience/merc/congress CHOICE + OK; Contact meet CHOICE + OK chrome; Diplo war/peace/ally OK + alliance CHOICE
- **R2:** King restless/refuse/rename/WoI OK; Contact Trade concluded + mission-burn; Diplo peace CHOICE + war no-spam
- **R3:** King merc success + intervene landing OK; Contact teach-refuse/convert smokes; Diplo alliance Accept follow-up + privateer OK

## Verdict
Wireable AI popup + text + choice chains for Contact / King / Diplo are structurally unparked (status kept; queue presents post-turn). VGA-identical look out of scope.

## Still PARKED / not exhausted
- Deep DOS bodies: `2820`/`4528`/`2154`, full trade haggle UI, gift amount picker
- King: `160a` letter cinematic, dump-goods boycott cargos, MoW×6 chrome, VGA modals
- FA `3f41` full advisor UI; Congress / FF report UI
- Exact DOS dialog drawing routines
