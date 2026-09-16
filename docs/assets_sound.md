# Sound and Music Assets

GSOUND driver, MIDI backend, DOS background music scheduler, and SFX/voice ID ranges.

Reference: [assets.md](assets.md) for graphics and map formats.

## Contents

- [Music / sound](#music--sound)
- [Song ids](#song-ids-verified-2026-08-27)
- [DOS BGM scheduler](#dos-bgm-scheduler-fun_129f_00f6--0318--02cc)
- [GSOUND driver facts](#gsound-driver-facts-from-gsoundasmasm--raw-ndisasm-of-the-mz-image)
- [Sound-ID ranges beyond the 12 BGM tracks](#sound-id-ranges-beyond-the-12-bgm-tracks-re-notes)
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
| `0x20..0x3f` BGM | `0x2A6E` | The 12 Pick-Music tracks + named submenus (Independence/Military/Indian) **and** situational ids outside those submenus (`0x24`, `0x25`, `0x3e`) pushed directly by DOS gameplay code via `FUN_281f_048e`→`FUN_129f_02cc`. Confirmed real trigger: combat (`FUN_5fef`, land+naval) pushes `0x32` ("Military" sublist track 1) when an engagement begins — ported as `units_combat_music_sting()` (`units.c`), gated through `units_set_combat_music_hooks` (kept as a function-pointer hook so `units.c` stays linkable without `sound.c` in standalone `unit_*` test binaries). Other confirmed-real-but-unmapped-to-a-precise-trigger call sites: segments `65dd` (LCR), `75c2` (save/load), `48d3` (Europe exit), `364b` (colony), `38fd`/`3844` (trade) — left unwired pending closer per-site tracing. |
| `0x40..0x5c` "event music" | `0x2AC4` | **Triggers found 2026-08-27** — pushed with the id in **AX** (`mov ax,N; callf FUN_281f_04c0`), which Ghidra drops from the decompile, so the earlier "no confirmed trigger" verdict was a decompiler artifact. Each handler queues a `COLDIG.BIN` sample and starts a short MIDI sting on channels 7/8. Decode + playback + mixing are done; per-id push sites and their port wiring status are in the "COLDIG.BIN" table below. |
| `≥0x8020` (7 entries, ids `0x8020..0x8026`) | `0x2AB6` | Short pre-scripted multi-voice MIDI chord stings (writes directly into the same voice-struct engine used for BGM playback — not digital audio). No confirmed DOS caller found (the one literal `0x8025` reference elsewhere in `VICEROY.EXE` turned out to be an unrelated dialog-box parameter, not a sound id). Left unwired. |

The `sound_effects` option flag (`ColonizeSoundOptions.sound_effects`, DS offset `0xa2` in
the driver) is real and consulted by DOS at several BGM-change call sites
(`FUN_129f_0300`/`0318`/`034c`) — but it gates **whether a BGM track change applies
immediately or gets deferred to the next idle-pump poll**, not a separate audio category.
`sound_play`'s existing BGM gating (`background_music`/`event_music` bits) already covers
the player-visible effect; the immediate-vs-deferred nuance is DOS-internal scheduling with
no equivalent complexity in the port's single-threaded playback and was not replicated.

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
| `0x40`/`0x41` | 31 / 32 | shot | `5fef_1b0e` attack fire (0x41 artillery class), **only when `param_4` (visible) is set** — `465b_0000` passes 1 for the viewport nation or a human side, the AI scorer `521d:52aa` passes 0 | `units.c` engagement (0x40), gated by `units_combat_is_visible` (2026-08-28 — AI-vs-AI combat was audible, cannon fire landed over unrelated popups) |
| `0x42`/`0x48` | 30 / 29 | shots | `5fef_1b0e` 5fef:2271: human attacker vs Indian (nation ≥ 4) pushes `0x3b + attacker unit type` — Cont. Cav. (7) / Treasure (0xd) | `units.c` engagement (2026-08-29): typed id when defender is Indian |
| `0x43`/`0x49` | 27 / 34 | shots | same rule: Cavalry (8) / Wagon Train (0xc)… — the "unit-class variants" are the attacker's type index | same |
| `0x44`/`0x45` | 18 / 17 | shot / glancing shot | `5fef_1b0e` 5fef:28b0 tail, gated on `local_6` (set at 5fef:2546: attacker nation ≥ 4, a colony at the defender tile — `281f_07be(x,y)` ≥ 0 — and colony pop > 1 or `local_70 == 0`) + attacker won + visible → `0x44` if the *attacker* is a ship type (`local_86` = `Stack[4]` type in 0xd..0x12, unreachable for Indians) else `0x45`; `0x44` is also Cont. Army (9) via the typed rule | **2026-08-29**: `units_try_move` combat branch — Indian attacker beats a colony defender (colony at dest, pop > 1) → `0x45` after the 0x4a win beat; the ship-attacker `0x44` arm is dead |
| `0x4a`/`0x4b` | 28 / 33 | shots | `5fef_1b0e` win; 0x4b when natives involved | `units.c` win (same visibility gate) |
| `0x4c` | 14 | shooting + galloping | `0x3b + type` would need attacker type 0x11 (Frigate) — ships cannot attack land units, so this id is unreachable through the typed rule; no other push site located | — |
| `0x4d` | 10 | cheering + fireworks | `5fef_0352` 5fef:07db-0803: junction reached from 061c (winner is a ship), 0631, 0722 and the `@ARTILLERY2` popup; when *both* combatants are ship types (0xd..0x12) and the fight is visible → `0x4d`, **before** the damage-flag (5fef:0d0x `0x3148\|0x80`) / sink (`0x57`) / seizure split — a naval-win beat, not a capture; also raid loot | raid loot gold (`ai_contact.c` @RAIDGOLD); **2026-08-29**: `units_apply_naval_loss_outcome` entry (visible) → `0x4d`, ahead of damaged/sunk |
| `0x4e` | 6 | screaming | `5fef_0f14` raid: colonists killed | @RAIDSCALP (2026-08-29) |
| `0x4f` | 11+32 | screaming + shooting | `5fef_0f14` raid loot goods | @RAIDSTORES (2026-08-29) |
| `0x50`/`0x51` | 7+8 / 5+14 | screaming, burning / screaming, galloping | typed rule: Mounted Braves (0x15) / Mounted Warriors (0x16) — but the rule is gated on a *human* attacker, so these fire only for captured/converted native types | typed rule |
| `0x52` | 12 | wagon wheels | `465b_0000` wagon-train move (human) | `game_loop.c` human move success, type "Wagon Train" (2026-08-29) |
| `0x53` | 19 | burning | `5fef_0f14`/`1b0e` tail: colony burned | colony burned notify |
| `0x54` | 13 | hammering + cheering | found colony `479b_076e`; colony screen `2f2b_6cd4` **only when `DS:0x34a >= 0`** (the building that just finished, revealed by clear-bit/redraw/set-bit/redraw); nation EOT `3844` | found colony; colony open **gated** on `ColonizeColony.pending_build_reveal` (2026-08-28 — was every open) |
| `0x55` | 20 | animal shot | `0x3b + type` would need type 0x1a (past the land-unit range) — unreachable through the typed rule; no other push site located | — |
| `0x56` | 9 | cheering | `38fd_3dc8` tax raise / tea party | `ai_king.c` @TEAPARTY + raise-taxes popup (2026-08-29) |
| `0x57` | 16 | sinking | `5fef_0352` ship sunk | `units.c` @SHIPSUNK via the combat sound hook (2026-08-29) |
| `0x58` | 21 | pump-action | fortify / sentry (`2b5a_1112`, `2f2b_5746`) | fortify, sentry |
| `0x5a` | 15 | cheering + fireworks | `5fef_1908` King's Galleon (via `FUN_281f_04b6`) | galleon credit |
| `0x5b` | 22+31 | gunfight | `5fef_0f14` raid repelled | @RAIDNOTHING (2026-08-29) |
| `0x5c` | 8 | burning | typed rule: type 0x21 (past the unit table — unreachable) | — |
| `0x8020` / `0x8024` | — (chord stings) | | war declaration `5bfb_153e`, assign colonist `2f2b_2f3e` | **wired 2026-08-29**: `FUN_1000_19bc` has a fourth handler table at `0x2AB6` indexed `id − 0x8020` (bound `DS:0xFE`); `gsound_vm.c` now dispatches it, gated by Event Music. `ai_diplo_declare_war_ctx` (human involved, via `ai_diplo_set_sound_hook`) and the three colony-screen assign sites |

**BGM cues pushed by gameplay code (2026-08-29 asm sweep of every `281f_04c0`/`04b6`
call):** `75c2_235c` new-game init → `0x39` Hornpipe once (ported: game_loop new-game start);
`38fd_3dc8` King's audience → `0x3e` (ported: `ai_king.c` audience CHOICE); `43f7_10f0`
intervention → `0x3f` after `@INTERVENE` (ported); `41f2_0b70` Retire → `0x24`/`0x25`/`0x21`
by the coin-animation tier (≥23 / >6 / else) — not ported (tier derivation still PARK, see
difficulty.md); `364b_0000` is **not** a colony-screen open — it is the colony-screen popup helper
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
`43f7_1d42` after `@KINGBUY` → pool 3 (KINGBUY itself unported); `43f7_10f0` → pool 3 then `0x3f`
(ported); `3844_00f2` nation EOT → `0x3e` ahead of **`@KINGFRIGATE`** (`LEA BX,[0xef5]`; Crown offers a Frigate to a
harassed, frigate-less nation every 8th peacetime turn — ported 2026-08-29 as `ai_king_frigate_offer`, tune
included);
`5fef_0f14` raid → pool 2 when the raid is wiped out (`local_6 == 0`, 5fef:1299) / `0x32` for any
other outcome (5fef:13b2) — both already in `ai_contact.c`'s raid tail; `41f2_0b70` Retire → `0x24/0x25/0x21` by coin
tier (PARK). The port's `sound_set_bgm(1/2)` covers the map/colony switches; the naval `1`/`4`
beat is wired too (`units_set_bgm_hook`: human loser → pool 1, human winner → pool 4); a wiped-out raid on a human colony → pool 2 (`ai_contact.c` @RAIDNOTHING).

Event ids bypass the BGM scheduler (`sound_play` dispatches them directly), gated by the
Event Music option in the driver and by Sound Effects for the PCM part.

## Discovery Order

Intended install layout: put original game files in `<executable-dir>/COLONIZE/`.

1. Explicit `--data-dir` (if that path exists)
2. `<executable-dir>/COLONIZE`
3. `./COLONIZE` (working directory)
