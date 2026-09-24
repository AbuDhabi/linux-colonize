# Sound and Music Assets

GSOUND driver, MIDI backend, DOS background music scheduler, and SFX/voice ID ranges.

Reference: [assets.md](assets.md) for graphics and map formats.

## Contents

- [Music / sound](#music--sound)
- [Song ids](#song-ids-verified-2026-08-27)
- [DOS BGM scheduler](#dos-bgm-scheduler-fun_129f_00f6--0318--02cc)
- [GSOUND driver facts](#gsound-driver-facts-from-gsoundasmasm--raw-ndisasm-of-the-mz-image)
- [Sound-ID ranges beyond the 12 BGM tracks](#sound-id-ranges-beyond-the-12-bgm-tracks-re-notes)
- [Trigger parity audit](#trigger-parity-audit-2026-09-24)
- [Discovery Order](#discovery-order)

---

## Music / sound

There are **no** standalone `.MID` / `.XMI` song files. Music lives inside the MZ sound
drivers. The Linux port loads **`GSOUND.COL`** (General MIDI) and **emulates the driver
literally** in [`src/core/gsound_vm.c`](../src/core/gsound_vm.c); [`src/core/sound.c`](../src/core/sound.c)
ticks that VM in real time from the audio callback and mirrors the DOS BGM scheduler
(segment `129f`).

| Driver | Card letter | Role |
|--------|-------------|------|
| `ASOUND.COL` | A | AdLib / OPL |
| `GSOUND.COL` | G | General MIDI (**used**) |
| `PSOUND.COL` | P | PAS / SB-family |
| `RSOUND.COL` | R | Roland / MT-32-style |
| `CONFIG.COL` | — | 20-byte INSTALL card config |
| `COLDIG.BIN` | — | Digital SFX — 35 samples, **decoded and played** (2026-08-27) |

DOS play path: numeric sound IDs through the driver jump table (`FUN_2059_000a`), gated by
Background / Event / SFX (`FUN_12d8_000e`). IDs `0x20..0x3f` are background music;
`0x40..0x5c` event music; IDs `< 0x10` are system (0 = hard stop, 1 = fade out).

### Song ids (verified 2026-08-27)

The DOS Pick Music handler (`2b5a:264c` jump table + sublist offsets) and the tune table in
`FUN_129f_0008` give the **real** id ↔ title mapping. Earlier docs assumed entry *n* → `0x20+n`;
that was wrong for 5 of the 12 main tunes and every sublist, which is why the port sounded
like "a different song". Verified by chroma-DTW against a DOSBox-X capture (Jine the Cavalry =
`0x25`, DTW cost 0.04) and the OST rips in `reference_music/` (which match DOSBox exactly).

| Pick Music entry | id | | entry | id |
|---|---|---|---|---|
| 1 Bird Song | `0x20` | | 7 Joe Clark | `0x26` |
| 2 Smoky Tune | `0x21` | | 8 Little Fiddle | `0x27` |
| 3 Cornwall | `0x22` | | 9 Hornpipe | `0x39` |
| 4 Shady Grove | `0x23` | | 10 Bonny Morn | `0x38` |
| 5 Fiddler's Dance | `0x24` | | 11 Hole In The Wall | `0x3a` |
| 6 Jine the Cavalry | `0x25` | | 12 Nightingale | `0x3b` |

Independence `0x29..0x2d` (Love Forever … Independence Way), Military `0x2e..0x31`
(Reveille, Successful Campaign, Morelli's Lesson, To Arms), Indian `0x32,0x33,0x35,0x36`
(Indian Victory, Natives, Tenochtitlan, Pizarro at Cuzco). `0x28`, `0x34`, `0x37`, `0x3c..0x3f`
are not in Pick Music (`0x28` is drawn by the Europe pool; `0x3c` picks random patches in its
handler). The combat cue `0x32` is therefore **Indian Victory**. The title-screen id `0x33`
(= Natives) is inherited from older notes and unverified.

### DOS BGM scheduler (`FUN_129f_00f6` / `0318` / `02cc`)

* `DS:0x9a` is a **tune pool**, not a track: 1 = map (tunes 1–7: Bird Song … Bonny Morn,
  Hole, Nightingale), 2 = colony (8–12: Fiddler's, Jine, Joe Clark, Little Fiddle,
  Hornpipe), 3 = Europe (`0x28` + Independence), 4 = Military, 5/6/7 = one-shot Natives /
  Tenochtitlan / Pizarro then the general pool (all 12, 1-in-8 chance of the 13–23 set).
  `sound_set_bgm(pool)` mirrors `FUN_129f_0318`: a pool change fades the current song.
* All songs **end** (no `FD` loop except `0x34`); when the driver reports no voice active
  the pump draws the next random tune from the pool, never repeating the last id.
* `sound_play(id)` = `FUN_129f_02cc`: queue + fade, pump starts it when idle.
  Pick Music selection plays immediately and becomes the current BGM (`FUN_281f_04c0`).

### GSOUND driver facts (from `gsound.asm` / raw ndisasm of the MZ image)

* PIT divisor `DS:0081 = 0x4DBF` → **59.95 Hz** ticks; a note lasts exactly `dur` ticks
  (`SUB [bx],1 / JBE parse`); gate = `F6` abs or `dur − F7`; `F7 ≥ 0x80` ties repeated notes.
* Nine voice blocks (`DS:0x8096 + 0x28·i`) map to MIDI channels **1..9** in that order;
  the block at `0x80BE` is channel **9 (GM percussion)** — songs `0x27..0x31`, `0x36`, `0x3d`
  put their drum track there (`call 0x15e0`). Melodic tracks take the first free of
  channels 1–6 (`0x14cd`); event music uses 7–8 (`0x15c1`).
* Song handlers are x86 stubs (`call 0x18cf` fade-old-voices, `mov cx,stream / call alloc`);
  some use the PRNG (`0x3c`), warm-restart variants via `DS:E6/E8/EA` (`0x25`, `0x34`) or
  `C4` code call-outs (`0x20` pokes note bytes). `gsound_vm.c` runs them with a mini x86.
* Opcodes: `≤BA note,dur`; `ED n notes dur` chord; `F4` vel; `F8` prog; `F0/F1/F2` pan/vol/bend;
  `F3/F5/EF` vol/pitch/pan envelopes; `BB n` RPN bend range; `C0/C1/C2` bank/chorus/reverb;
  `C5..CC` conditional **call** (single return slot `+0x22`), `CD..D4` conditional **jump**;
  `D5..E9` byte ALU on `DS:0x5C+r`; `E7/EA/EB/EC` self-modify the stream; `FA/F9` call/ret
  (one slot, not a stack); `FB/FC/FD` jump / set loop / loop-to-start; `FE/FF` counted loops.
  `BE/BF/BC/BD` write tempo words nobody reads — timing is the PIT only.
* Song switch: old voices get `+0x26 = 0xFF` and fade 2 CC7 units/tick (`0x1819`) while the
  new song starts on free voices — a real cross-fade, reproduced.

MicroProse GM drivers of this era were written for **Roland Sound Canvas / SC-55**.
Closest practical playback: FluidSynth + an SC-55-character SoundFont.

**SoundFont search order:** `settings.json` `sound_options.soundfont` → bundled
[`data/soundfonts/Roland_SC-55.sf2`](../data/soundfonts/Roland_SC-55.sf2) (ScummVM’s
GPL-3+ bank by deemster; see [`COPYRIGHT.Roland_SC-55`](../data/soundfonts/COPYRIGHT.Roland_SC-55))
→ system SC-55 / GeneralUser GS → FluidR3 / distro defaults. For an alternate SC-55
character, point `sound_options.soundfont` at [Trevor0402’s SC-55 SoundFont](https://github.com/trevor0402/SC55Soundfont).
Sound tooling retired 2026-09-16 (music complete): `tools/dump_gsound_wav.c`, `tools/compare_music_ab.py`, `original_music_dumps/jine_the_cavalry.wav` are in git history only. Historical A/B verification (2026-08-27): gold reference was `original_music_dumps/jine_the_cavalry.wav` (DOSBox-X capture) and `reference_music/wav/*` (OST rips, verified identical to DOSBox timing); tool output showed dtw 0.04 / 0.09 / 0.15 / 0.17, drift ≤ 1.1 s.
AdLib / MT-32 drivers remain out of scope. `COLDIG.BIN` SFX are in (see below).

Song names for the Pick Music UI are only in `GAME.TXT` `@PICKMUSIC` (plus Independence /
Military / Indian sublists). Options are `@SOUNDOPTIONS` and Col1 `tut2` bits.

**Pick Music (GAME menu):** implemented in [`src/core/pick_music.c`](../src/core/pick_music.c) as a
shared wood **popup** (`popup_draw` + `WOODTILE.SS`) over the map, using the id table above.
Selecting a song plays it at once as the current BGM (closing the dialog does not stop it).

### Sound-ID ranges beyond the 12 BGM tracks (RE notes)

`FUN_1000_19bc` (the driver's numeric-id dispatcher, `GSOUND.COL` and `PSOUND.COL` alike)
has four ranges, all traced by disassembling the raw `.COL` MZ images (not just the
overlay-affected `VICEROY.EXE` decompile):

| Range | Table (image offset) | Confirmed behavior |
|-------|----------------------|---------------------|
| `< 0x10` (only 9 entries, ids 0–8) | `0x2A5C` | **Channel reset/silence**, not player-audible content — e.g. id 4's handler resets MIDI channels 6–7 (`CC121`/`CC123` all-notes-off + reset-controllers), id 1's handler mutes two specific voice slots. `sound_play` already treats id 0/1 as "stop" (`src/core/sound.c`). |
| `0x20..0x3f` songs | `0x2A6E` | Pick Music tracks, situational gameplay tunes, and the `0x34`/`0x3d` opening/closing cues. The mapped combat, LCR, save/load, Europe, colony, and retire cues are wired below; `0x32` is Indian Victory despite the old "Military" nickname. |
| `0x40..0x5c` "event music" | `0x2AC4` | **Triggers found 2026-08-27** — pushed with the id in **AX** (`mov ax,N; callf FUN_281f_04c0`), which Ghidra drops from the decompile, so the earlier "no confirmed trigger" verdict was a decompiler artifact. Each handler queues a `COLDIG.BIN` sample and starts a short MIDI sting on channels 7/8. Decode + playback + mixing are done; per-id push sites and their port wiring status are in the "COLDIG.BIN" table below. |
| `0x8020..0x8026` (7 entries) | `0x2AB6` | Short multi-voice MIDI chord stings. Confirmed callers: `0x8020` war declaration and `0x8024` assign colonist, both wired. The apparent literal `0x8025` elsewhere is an unrelated dialog parameter. |

The three option flags are Background Music (`DS:0xa2`, scheduler), Event Music (`DS:0xa0`,
`0x20..0x3f` song dispatch), and Sound Effects (`DS:0xa4`, `0x40..0x5c` event dispatch and
PCM). The signed `< 0x10` test in `FUN_12d8_000e` also forwards `0x8020`/`0x8024`
regardless of those options. `sound_id_gate_allows` and `unit_sound_gate` cover this gate.

**`COLDIG.BIN` digital SFX — wired 2026-08-27.** Earlier notes ("no reachable trigger,
settled negative") were wrong: the game pushes event ids `0x40..0x5c` with the id in **AX**
(`mov ax,N; callf FUN_281f_04c0` → `FUN_12d8_000e`), which Ghidra's decompile drops. Each
event handler in `GSOUND.COL` does `mov ax,N; call 0x30c4` → `FUN_1000_27b4(N)` (queue COLDIG
sample N, 16-slot ring, played back to back) and then starts a short MIDI sting on channels
7/8. Sample table is static in the driver image at `0x1C7B` (`offset32,len32`, 35 entries,
exactly covering the 993 755-byte file); samples 0–4 play at 11025 Hz, 5–34 at 19050 Hz,
unsigned 8-bit. Stream opcode `C3 n` is the same trigger. `sound.c` loads the file, the VM
callback queues samples, and `sound_render_s16` mixes them after the synth.

Sample contents (user listen test 2026-08-27): 0–4 single shots/fireworks; 5–7 screaming +
shots; 8, 19 burning; 9 cheering; 10, 15 cheering + fireworks; 11 screaming + shooting;
12 wagon wheels; 13 hammering then cheering; 14 shooting + galloping; 16 sinking; 17 shot
glancing; 18 shot; 20 animal shot; 21 pump-action; 22 gunfight; 23–34 shots (25–26 cannon).

| Event id | COLDIG | Sound | DOS push site | Port |
|---|---|---|---|---|
| `0x40`/`0x41` | 31 / 32 | shot | `5fef_1b0e` 5fef:232e-23a7 selects generic attack `0x40` or `0x41` by both units' attack stat; `0x40` also occurs in its loss cue. All are gated by `param_4` (visible). | Generic attack and loss selection ported |
| `0x42`/`0x48` | 30 / 29 | shots | `5fef_1b0e`: `0x42` is generic attack for a ship or Artillery; `0x48` is a native village's partial-loss cue | Both ported |
| `0x43`/`0x49` | 27 / 34 | shots | `5fef_1b0e` 5fef:2577-259f attacker-loss cue: `0x43` for ship/attacking Artillery, `0x49` when the defender tile has a native settlement, else `0x40` | Loss selector ported in `units_combat_resolve.c` |
| `0x44`/`0x45` | 18 / 17 | shot / glancing shot | `5fef_1b0e` 5fef:28b0 tail, gated on `local_6` (set at 5fef:2546: attacker nation ≥ 4, a colony at the defender tile — `281f_07be(x,y)` ≥ 0 — and colony pop > 1 or `local_70 == 0`) + attacker won + visible → `0x44` if the *attacker* is a ship type (`local_86` = `Stack[4]` type in 0xd..0x12, unreachable for Indians) else `0x45` | `units_try_move`: native attacker beating a populated colony defender emits `0x45` after the win cue; the ship-attacker `0x44` arm is dead |
| `0x46`/`0x47` | 16 / 16 | sinking | `5fef_1b0e` 5fef:2292 computes `0x3b + attacker @UNIT row`; rows 11/12 yield these IDs | Same computed dispatch in `units_combat_resolve.c` |
| `0x4a`/`0x4b` | 28 / 33 | shots | `5fef_1b0e` win; `0x4b` in the native attacker/European colony arm | `units_move.c` win selection (same visibility gate) |
| `0x4c` | 14 | shooting + galloping | `5fef_1b0e` 5fef:234e-236a selects it as the generic attack cue for attacker @UNIT rows 4, 5, 7, or 8 | Ported in `units_combat_resolve.c` |
| `0x4d` | 10 | cheering + fireworks | `5fef_0352` 5fef:07db-0803: junction reached from 061c (winner is a ship), 0631, 0722 and the `@ARTILLERY2` popup; when *both* combatants are ship types (0xd..0x12) and the fight is visible → `0x4d`, **before** the damage-flag (5fef:0d0x `0x3148\|0x80`) / sink (`0x57`) / seizure split — a naval-win beat, not a capture; also raid loot | raid loot gold (`ai_contact.c` @RAIDGOLD); **2026-08-29**: `units_apply_naval_loss_outcome` entry (visible) → `0x4d`, ahead of damaged/sunk |
| `0x4e` | 6 | screaming | `5fef_0f14` raid: colonists killed | @RAIDSCALP (2026-08-29) |
| `0x4f` | 11+32 | screaming + shooting | `5fef_0f14` raid loot goods | @RAIDSTORES (2026-08-29) |
| `0x50`/`0x51` | 7+8 / 5+14 | screaming, burning / screaming, galloping | `5fef_1b0e` 5fef:2271: native attacker @UNIT row 21/22 attacks a human European defender; typed cue precedes the generic attack cue | Ported in `units_combat_resolve.c` |
| `0x52` | 12 | wagon wheels | `465b_0000` wagon-train move (human) | `game_loop.c` human move success, type "Wagon Train" (2026-08-29) |
| `0x53` | 19 | burning | `5fef_0f14`/`1b0e` tail: colony burned; `5fef:3063` gates it on the human victim | Colony burned notify, gated on human victim |
| `0x54` | 13 | hammering + cheering | found colony `479b_076e`; colony screen `2f2b_6cd4` **only when `DS:0x34a >= 0`** (the building that just finished, revealed by clear-bit/redraw/set-bit/redraw); nation EOT `3844` | found colony; colony open **gated** on `ColonizeColony.pending_build_reveal` (2026-08-28 — was every open) |
| `0x55` | 20 | animal shot | The typed combat rule cannot reach it, but `OVL13:003dc9` pushes it on the human @CHIEFKILL branch (scout killed by a chief without Coronado) | `ai_contact_actions.c` before @CHIEFKILL |
| `0x56` | 9 | cheering | `38fd_3dc8` tax raise / tea party | `ai_king.c` @TEAPARTY + raise-taxes popup (2026-08-29) |
| `0x57` | 16 | sinking | `5fef_0352` ship sunk | `units.c` @SHIPSUNK via the combat sound hook (2026-08-29) |
| `0x58` | 21 | pump-action | fortify / sentry (`2b5a_1112`, `2f2b_5746`); Europe dock buy muskets (`38fd`) | fortify, sentry, buy muskets |
| `0x59` | — | fireworks cue | `CLOSING.EXE` cinematic frame cue | `closing.c` frame cue |
| `0x5a` | 15 | cheering + fireworks | `5fef_1908` King's Galleon (via `FUN_281f_04b6`) | galleon credit |
| `0x5b` | 22+31 | gunfight | `5fef_0f14` raid repelled | @RAIDNOTHING (2026-08-29) |
| `0x5c` | 8 | burning | Europe dock buy horses (`38fd`, OVL05 near 3bbe) | `europe_dock.c` buy horses |
| `0x8020` / `0x8024` | — (chord stings) | | war declaration `5bfb_153e`, assign colonist `2f2b_2f3e` | `ai_diplo_declare_war_ctx` and the three colony-screen assign sites; `gsound_vm.c` dispatches them, and the signed DOS gate forwards them with every option off |

**Playback call coverage:** DOS code contains a dispatch path for every one of
the 29 event IDs `0x40..0x5c`; the port likewise has a dispatch expression for
each. `0x44` appears as a literal `MOV AX,0x44` followed by the playback call
at `5fef:28c0-28cd`. `0x46` and `0x47` have no literal push in that combat
body because `5fef:2292-229b` computes `0x3b + attacker @UNIT row` before the
playback call: rows 11 and 12 produce them. The same expression can also
produce `0x44` from row 9. Whether a normal game reaches those particular
unit/branch combinations is a separate question from whether code can launch
the IDs. The resident `MOV AX,0x44/0x46/0x47` table at `0000:af6a` is a
character-code mapper and is not the evidence for playback.

**PCM sample use:** `COLDIG.BIN` has 35 indexed samples. Sweeping every
`GSOUND.COL` system, song, event, and chord handler through the port VM for
10,000 ticks each queued 26 indices: `5..22` and `27..34`. Indices `0..4`
and `23..26` were never queued in that sweep. This is a driver-level result,
not a claim that all 26 are reachable in an ordinary game; for example,
event `0x44` has a handler and sample but no normal gameplay trigger.

**BGM cues pushed by gameplay code (2026-08-29 asm sweep of every `281f_04c0`/`04b6`
call):** `75c2_235c` new-game init → `0x39` Hornpipe once (ported: game_loop new-game start);
`38fd_3dc8` King's audience → `0x3e` (ported: `ai_king.c` audience CHOICE); `43f7_10f0`
intervention → `0x3f` after `@INTERVENE` (ported); `41f2_0b70` Retire exploits →
`0x24`/`0x25`/`0x21` by coin tier (≥23 / 7–22 / 0–6), ported through
`sound_retire_tune_id(sc.exploits_tier)` at the exploits reveal. Raw
`OVL06:3e11-3e2d` compares the tier with 23 and 6, then calls `FUN_1000_86b0`;
`364b_0000` is **not** a colony-screen open — it is the colony-screen popup helper
(tags NOMOREWAREHOUSE/NOMOREWAGONS/BUILT/DEPLETION/REBEL*/TORY*/SONSDOWN via
`thunk_FUN_291f_09dc`, 9 sites all inside `364b`) whose 7th arg (`Stack[0x10]`) is an
optional event id — **every caller passes 0** (the NOMOREWAREHOUSE site pushes `AX`,
which is 0 on that path), so no colony popup carries a sound (resolved 2026-08-29);
`2b5a_2464` Pick Music. Pool switches
`281f_04b6(n)` → `129f_034c` (`DS:0x9a = n`, restart if changed): `1` map at colony EOT
(`364b_0688`, when `0xa897`) and for a human *loser* of a naval fight (`5fef_0352`), `4`
Military for a human naval *winner*, `2` colony pool on building complete (`364b_0114`) and
after the King's Galleon popup (`5fef_1908`). Also (`281f_048e` = queue-next, `281f_0498` = pool switch, 2026-08-29 sweep): `48d3_06ba`
treasure cash-in → `0x24` (ported: `europe_cash_treasure` via `europe_set_sound_hook`);
`75c2_20e2` load → `0x3e` (ported); `75c2_2778` Europe screen → pool 3 (ported at both
Europe-open sites); `38fd_5e52` human immigrant → pool 2 (ported, `europe_notify_immigrant_sound`);
`65dd_0004` LCR: Fountain of Youth → `0x37`, Cibola → `0x3c`, Burial Mounds → `0x33` (ported in the
`units.c` LCR switch), plus (mapped 2026-08-29, `units_resolve_lcr_rumour`): case 3/7 small treasure / chief's gift
with gold → pool 2 (`65dd:04ca`); case 5 vanish → pool 1 (`65dd:0778`); burial-mounds
BURIAL3 treasure with no tribe claim → `0x24` (`65dd:0654`), a claim → `0x32` ahead of @SCREWED
(`65dd:06e6`) — all human-only (`local_a`); `4d56_2820` village visit (human, 1-in-3 roll) → pool 5, Inca 7, Aztec 6
(ported in `ai_contact_speak_with_chief`); `5bfb_022e` first meet → same pools from turn 20 (`04ac` = `129f_0318` restart-if-changed, `0498` = the option-gated wrapper; ported in `ai_contact_enqueue_welcome`);
`43f7_1d42` has a pool-3 call only in its unreachable wartime arm: the function
returns at entry when wartime, while the peacetime `@KINGBUY` arm returns before
that call; `43f7_10f0` → pool 3 then `0x3f`
(ported); `3844_00f2` nation EOT → `0x3e` ahead of **`@KINGFRIGATE`** (`LEA BX,[0xef5]`; Crown offers a Frigate to a
harassed, frigate-less nation every 8th peacetime turn — ported 2026-08-29 as `ai_king_frigate_offer`, tune
included);
`5fef_0f14` raid → pool 2 when the raid is wiped out (`local_6 == 0`, 5fef:1299) / `0x32` for any
other outcome (5fef:13b2) — both already in `ai_contact.c`'s raid tail. The port's `sound_set_bgm(1/2)` covers the map/colony switches; the naval `1`/`4`
beat is wired too (`units_set_bgm_hook`: human loser → pool 1, human winner → pool 4); a wiped-out raid on a human colony → pool 2 (`ai_contact.c` @RAIDNOTHING).

Event ids bypass the BGM scheduler (`sound_play` dispatches them directly); the
Sound Effects option gates both the event dispatch and its PCM part.

### Trigger parity audit (2026-09-24)

Count **player contexts**, rather than C call statements: one DOS attack site computes
several IDs from the attacker type, while one port hook services several DOS callers.
The table above is the per-ID event ledger; the BGM paragraph is the situational cue
ledger. The remaining source-level comparison is summarized here.

| Player context | DOS trigger | Port trigger | Result |
|---|---|---|---|
| Combat and raids | `5fef_1b0e`, `5fef_0352`, `5fef_0f14` | `units_combat_resolve.c`, `units_move.c`, `units_combat.c`, `ai_contact_raid.c` | Attack cue order/selection, loss cue selection, village partial-loss, burn victim gate, and Indian Raid woodcut trigger/suppression corrected |
| Movement, settlement, trade | `465b_0000`, `479b_076e`, `2f2b_6cd4`, `48d3_06ba` | `game_loop_orders.c`, `game_dialogs.c`, `game_loop_colony.c`, `europe_harbor.c` | Mapped |
| King, diplomacy, discovery | `38fd_3dc8`, `43f7_10f0`, `5bfb_153e`, `65dd_0004` | `ai_king_*.c`, `ai_diplo.c`, `units_combat.c` | Mapped; `43f7_1d42` pool-3 arm is unreachable |
| Woodcut milestones | `12fd_006c` tune switch | `woodcut.c:woodcut_play_tune` | Mapped for reachable IDs 1–13; ID 0 belongs to demo autoplay |
| Opening and closing executables | `OPENING.EXE` `0x34`; `CLOSING.EXE` `0x3d`, `0x59`, `0x5a` | `opening.c`, `closing.c` | Mapped, including repeating frame cues |
| Retirement exploits | `41f2_0b70`, `OVL06:3e11-3e2d` | `game_retire_after_score`, Hall of Fame exit | Corrected: tier 0–6 → `0x21`, 7–22 → `0x25`, 23 → `0x24`; title tune starts on return to menu (or immediately if there is no exploits screen) |

The answer to per-effect trigger parity is **no**. A visible native attack on a human
defender calls the typed cue at `5fef:2271` and a generic cue at `5fef:23a7`
(unless the Indian Raid woodcut state suppresses the second). The port previously sent
one cue with the wrong attacker/defender condition; it now sends the usual pair.
The DOS loss selector at `5fef:259f` (`0x43`/`0x49`/`0x40`) and partial
village-loss `0x48` are now matched to the corresponding outcomes. The port also
arms woodcut 13 on a native attack against a human defender. DOS tests the
once-only Indian Raid woodcut bit before firing it; on later attacks with
Combat Analysis off, it suppresses the generic cue. The port now matches this
condition. Exact counts for every effect have not been established by a live DOS
trace; the static ledger is a context comparison rather than an execution count.
`SOUND_TITLE_ID=0x33` remains an inherited, unverified title-screen mapping.

## Discovery Order

Intended install layout: put original game files in `<executable-dir>/COLONIZE/`.

1. Explicit `--data-dir` (if that path exists)
2. `<executable-dir>/COLONIZE`
3. `./COLONIZE` (working directory)
