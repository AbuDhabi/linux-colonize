# assets_sound.md trigger ledger (archived 2026-10-24)

STATUS: archived 2026-10-24 from docs/assets_sound.md "Trigger parity audit (2026-09-24)" — history, not a spec. Every row reads DOS == port; the per-overlay wrapper counts, the COLDIG push-site table and the conclusion stay live in [assets_sound.md](../assets_sound.md). Lookup key: event id.

## Per-ID non-music launch counts (2026-09-24 audit)

The following is the exact non-music launch count after normalizing those shared selectors:

| ID | DOS | Port | Contexts |
|---|---:|---:|---|
| `0x40` | 2 | 2 | generic attack selector; ordinary attacker loss |
| `0x41` | 1 | 1 | generic attack selector |
| `0x42` | 1 | 1 | artillery/ship generic attack selector |
| `0x43` | 1 | 1 | artillery/ship attacker loss |
| `0x44` | 2 | 2 | computed native typed cue; normally unreachable native-ship colony tail |
| `0x45` | 1 | 1 | native attacker wins at qualifying colony |
| `0x46` | 1 | 1 | computed typed cue, @UNIT row 11 |
| `0x47` | 1 | 1 | computed typed cue, @UNIT row 12 |
| `0x48` | 1 | 1 | native village partial population loss |
| `0x49` | 1 | 1 | attacker loses at a native village |
| `0x4a` | 3 | 3 | ordinary win; alternate win branch; final village loss |
| `0x4b` | 2 | 2 | temporary defender/European colony win; raided ship first cue |
| `0x4c` | 1 | 1 | mounted-unit generic attack selector |
| `0x4d` | 2 | 2 | visible naval win; raided ship second cue |
| `0x4e` | 1 | 1 | raid steals gold |
| `0x4f` | 1 | 1 | raid steals stores |
| `0x50` | 1 | 1 | computed native typed cue, @UNIT row 21 |
| `0x51` | 1 | 1 | computed native typed cue, @UNIT row 22 |
| `0x52` | 1 | 1 | human Wagon Train move |
| `0x53` | 3 | 3 | failed heresy; raid burns building; colony burned |
| `0x54` | 3 | 3 | building complete; ship repair complete; colony founded |
| `0x55` | 1 | 1 | chief kills scout |
| `0x56` | 1 | 1 | completed tea party |
| `0x57` | 1 | 1 | visible ship sunk outcome |
| `0x58` | 3 | 3 | fortify; sentry; buy muskets |
| `0x59` | 1 | 1 | closing fireworks frame selector |
| `0x5a` | 1 | 1 | closing hat frame selector |
| `0x5b` | 1 | 1 | raid repelled/nothing stolen |
| `0x5c` | 1 | 1 | buy horses |
| `0x8020` | 1 | 1 | war declaration involving the human |
| `0x8024` | 4 | 4 | colony assignment; blessing; mission founded; heresy won |

Thus every DOS event ID `0x40..0x5c` has launch code in both programs, including
the normally unreachable `0x44` branch, and the static context count for every effect
matches. This is source-level proof of attempted playback, independent of whether a
normal campaign can reach a branch or whether the user's sound options allow output.
