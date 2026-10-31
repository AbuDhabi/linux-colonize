# Typos in the DOS text catalogs

STATUS: scan 2026-10-31. Spelling errors in the player-shown `COLONIZE/*.TXT` files that ship with
the DOS game. The port shows this text verbatim (no MicroProse text in the binary, see
[data_vs_hardcoded.md](data_vs_hardcoded.md) Part D), so every typo here appears in opencol
exactly as it did in DOS. Nothing is patched; `COLONIZE/` is the user's original data and is not
tracked in git. If a fix is ever wanted it needs an override layer, never an edit to the files.

Scanned: CLOSING, COLONY, GAME, HOF, LABELS, MAPEDIT, MAPMENU, MENU, NAMES, OPENING, PEDIA, TRIBE,
WOODCUT. Method: `aspell list` over non-comment lines (once as-is, once lowercased to catch
all-caps headings), plus a regex pass for doubled words and common misspellings. Real-word errors
(their/there and similar) are not caught by this method.

## Typos

| File:line | Text | Correct |
|---|---|---|
| GAME.TXT:1577 | umprovoked | unprovoked |
| GAME.TXT:3333 | importatnt | important |
| GAME.TXT:1095 | curiousity | curiosity |
| PEDIA.TXT:1309 | asuage | assuage |
| PEDIA.TXT:1186 | colonistS | colonist |
| PEDIA.TXT:1577 | When the the printing press | the |
| PEDIA.TXT:1588 | When the the newspaper | the |
| PEDIA.TXT:819, 892 | BORREAL FOREST (TERRAIN8, TERRAIN16) | BOREAL (NAMES.TXT has it right) |
| PEDIA.TXT:60 | Maxmillian | Maximilian |
| PEDIA.TXT:242 | John Rolffe | Rolfe |
| PEDIA.TXT:240-241 | Powhattan | Powhatan (NAMES.TXT spelling) |
| MAPEDIT.TXT:98 | shaopes | shapes (map editor help) |

## Not typos

- Period or house style: "cargos", "peaceloving", lowercase "european", "indians",
  "christians", "jewelled", "Cartajena".
- "Jine the Cavalry" (GAME.TXT:2640): real song title.
- "%STRING0catraz" (GAME.TXT:2818): builds "Alcatraz" from a substituted string.
- "C~ycling", "Memor~y", "Find C~ontinents": `~` marks the hotkey letter.
- Repeated words across lines in MENU, MAPMENU, LABELS, PEDIA ("FOOD / Food", "King / King"):
  key/value pairs, not doubled words.

## Not checked

Historical place names in COLONY.TXT ("Fort Miannis" / "Fort Miyamis", "Pontchartain",
"Sandouski", "Roquelai", "Prudhome", ...): period spellings vary and a dictionary cannot judge
them.
