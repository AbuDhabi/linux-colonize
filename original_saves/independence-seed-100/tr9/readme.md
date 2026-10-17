# tr9: DOSBox reference run (continuous from 000003)

Recorded 2026-10-17 with `tr9_trace.py` (run through `tools/dosbox_trace.py`, see
docs/idle_independence.md "DOSBox reference run"): load `../000003.SAV`, idle with Space, log
every RNG draw (BP 1541:0E04) with the acting unit's record (DS:0x2d12), bracket music picks
(BP 0AC3:0138 / 0AC3:0250, `PICK save K draw D`), copy COLONY09.SAV after each autosave.

- `tr9.txt`: the draw trace. `save K draws N` closes interval K (save K-1 -> K).
- `tr9_K.sav`: the DOS autosave after interval K (K = 4..20). Interval 20 covers two DOS
  turns (288 and 289) because the archive has no turn-288 save.

Compare the port against it from the repo root:
`/usr/bin/python3 tools/trcmp.py original_saves/independence-seed-100/tr9/tr9.txt K [show]`
(writes port dumps to `tools/tc/`; jdiff output columns are (DOS, port)).
