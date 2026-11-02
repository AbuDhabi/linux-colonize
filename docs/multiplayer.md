STATUS: PLAN (2026-11-02, rev 2: the server is authoritative). Nothing is implemented yet.

# Networked multiplayer (port-only)

DOS Colonization has no network play. It does have hot-seat (docs/hotseat.md):
several human powers take sequential slots in one year loop, with `@MULTINEXT`
between them. Network play keeps the hot-seat rules and the slot order. What
changes is who runs the sim.

## Core model: authoritative server, input replay

**The server owns the only game.** The host's process holds the one
`ColonizeGameState` that counts. It runs every AI Euro, Indian, King and
end-of-turn step, writes the autosaves, and decides what is true. Clients never
simulate anything the server has not seen.

**A client sends inputs, not results.** When a remote human's turn comes up:

1. **SYNC.** The server stops at that human's slot (the point where
   `turn_enter_slot` sets `handoff_pending` today). It captures the state and
   sends `SYNC{blob, turn_proc, nation, frame}` to that client. The blob is a
   Col1 save from `col1_save_write_memory`, including the COLNXEXT tail and
   the RTST chunk, so the RNG and the DOS-unsaved tables travel with it.
2. **Resume.** Both sides resume from that exact state, the same way
   `game_hotseat_begin_turn` resumes after its in-place reload: `@MULTINEXT`,
   then the nation's own FINISH/KING, then Move Pieces. The two copies are
   now identical, UI state included (map screen, popup queue, `turn_proc`).
3. **Play and stream.** The client plays at full local speed and streams its
   per-frame input records (`ColonizeInputState` plus the frame's `t_ms`,
   i.e. the `debug.input_logger` record) to the server in batches.
4. **Replay.** The server feeds those records to its own `game_update`, one
   per frame, exactly as the `regression_replay` harness does. The server
   therefore computes the result itself. A client cannot submit an illegal
   move or an edited state; at worst it submits inputs a human could have
   typed.
5. **End of turn.** The client's End of Turn does not start the turn
   processor locally. The client sends its remaining frames, sends `EOT` with
   a state hash, and enters the waiting state. The server replays up to the
   same End of Turn, runs the processor (AI, Indians, calendar, King), and at
   the next human slot either hands control to its local seat or goes back to
   step 1 for a remote one.

This works because the codebase already proves the determinism it needs:
`regression_replay` reloads a save, replays an input log, and requires the
resulting save to match **byte for byte**, and `golden_idle_reload` proves that
the save/reload round trip is exact. The client and the server must run the
**exact same build**, which is checked at `HELLO`.

### Popups during server-run slots

During AI and Indian slots, DOS hot-seat routes interactive popups (AI
negotiation, chief demands, King events) to the human at the controls,
DS:0x5394: the last human who moved. The server keeps that rule. When the sim
blocks on a popup and that human is remote, the server forwards the entry from
the `ai_popup` queue (`POPUP{tag, tokens, body, choices}`). The client shows
it standalone and answers with `ANSWER{index}`, and the server injects that
answer as the popup result. The `ai_popup` queue plus the hooks are the only
channel from the sim to the UI, so this is the whole seam. Phase 0 audits every
popup tag and every hook that can fire outside the human's own slot:
pictures, the village talk and King audience screens, and colony zooms.

### Desync handling

Every ~5 s spent idle on the main map, and at `EOT`, the client sends a
hash of its captured save. The server compares that hash with its own at the
same frame. On a mismatch the server's state wins: at the next quiet point it
sends a fresh `SYNC`, the client reloads onto the main map, and the server logs
a desync (that is a port bug). Nothing the client computed survives a
mismatch.

### Determinism traps (from the regression harness)

- **Music picks reseed the game RNG from the wall clock.** In a network game
  this has to be off on both sides, or derived from synced state. Phase 0
  confirms where it happens.
- **Window geometry.** Mouse coordinates are logical screen pixels, and the
  logical size is resizable (320x200 up to `SCREEN_MAX`). `SYNC` carries the
  client's logical size, and the server replays that client's frames at that
  size. Clients cannot resize during their turn (or a resize becomes an input
  record carrying the new size).
- **Nested polls.** `game_update` polls input internally (combat analysis,
  the combat popup pump). On the server those polls read from the remote
  queue. When the queue runs dry mid-nest, the server waits on the socket and
  keeps pumping SDL events so the host's window stays alive.
- **Gameplay-relevant settings.** The host's `divergences.*` flags travel in
  `START` and override the client's flags for the session. Options stored in
  the save head are already shared.
- **Module statics.** The harness forks per test for a reason. The server
  process hosts one game for its whole life, which matches the single-game
  case already in use.

### Spectating

Clients that are not in control get a snapshot blob after every human turn and
after the AI pass, and show it as a read-only map. The host, while a remote
player is in control, sees a waiting panel ("<Nation> is moving") rather than
the server game's screen: that screen is the remote player's private UI, and
it is drawn at the remote player's geometry.

## Transport: TCP only

Every message must arrive, in order, once. Inputs are small and ordered. Lost
or reordered input frames would desync the replay, so UDP would mean rebuilding
TCP on top of it. Latency only delays the server's copy, never the client's
play. The host listens on one port and clients connect out, so only the host
needs a NAT port forward (accepted limitation). LAN and same-machine play use
the same path (`127.0.0.1`).

- **Sockets.** Plain BSD sockets / Winsock (`#ifdef _WIN32`, plus `ws2_32` in
  the mingw build), non-blocking, polled from the game loop. No new
  dependency.
- **Name resolution.** `getaddrinfo` blocks, so JOIN connects on an
  `SDL_Thread` behind a "Connecting…" box. The listener is dual-stack (`::`
  with `IPV6_V6ONLY` off) and falls back to `0.0.0.0`.
- **Framing.** `u8 type, u32 length (LE), payload`. Lobby messages carry
  small JSON (`json_min`), `SYNC`/snapshots carry a binary save blob, and
  `INPUT` carries packed frame records. Frames are capped at a few MB. The
  network is a trust boundary: validate every field, run
  `col1_save_validate_head` on every blob, and on the server ignore `INPUT`
  from any peer that is not in control.
- **Keepalive.** `PING` every 15 s; drop the peer after 60 s of silence.

### Messages

| Type | Direction | Payload |
|------|-----|---------|
| `HELLO` | client → server | protocol version, build id, leader name |
| `WELCOME` / `REJECT` | server → client | player id / reason (version mismatch, full, started) |
| `LOBBY` | server → all | full lobby state, sent on every change |
| `CLAIM` / `SET_NAME` / `READY` | client → server | nation / leader name / bool |
| `START` | server → all | seat assignment, the host's divergences |
| `SYNC` | server → controller | blob, `turn_proc`, nation, frame number, logical screen size |
| `INPUT` | controller → server | batch of frame records (`t_ms`, keys, mouse) |
| `HASH` / `EOT` | controller → server | frame number + state hash |
| `POPUP` / `ANSWER` | server ↔ controller | forwarded `ai_popup` entry / chosen index |
| `SNAPSHOT` | server → spectators | blob for the read-only map |
| `GAME_END` | server → all | reason |
| `PING` / `BYE` | any | — |

Build id = the exact version string (git describe). A mismatch is refused at
`HELLO`.

## settings.json

```json
"multiplayer": {
  "enable": false,
  "server_port": 14920,
  "leader_name": "",
  "last_server": ""
}
```

- `enable`: shows MULTIPLAYER on the title menu. False means no menu row and
  no socket code runs.
- `server_port`: the port HOST listens on, and the default port JOIN uses
  when the typed address has none.
- `leader_name`: pre-fills the leader name in the lobby. Empty means the
  chosen nation's default leader from NAMES.TXT.
- `last_server`: pre-fills the JOIN prompt (`host:port`); rewritten on a
  successful connect.

Everything else is per game and lives in the lobby. Only the server autosaves,
so client copies on the same machine never write save slots.

## Screens

These are port-only screens built from DOS chrome (wood panel, FONTTINY, popup
boxes); a working-draft layout is fine (project_goals.md). Every modal and
every waiting state joins `game_modal_open`.

1. **Title menu.** When `multiplayer.enable` is on, a MULTIPLAYER row is
   appended to `@BEGINMENU`.
2. **MULTIPLAYER.** HOST / JOIN / Cancel.
3. **HOST, source.** New game, or Load (the save_load dialog). A loaded save
   keeps its map, difficulty and leader names. Its control-0 nations start
   Open and the rest start AI; the host can change either.
4. **HOST lobby.** Map (America / New World / Customize, using the
   new-game wizard phases), difficulty, and four nation rows, each AI / Open /
   Host. Also the host's leader name, the player list (name, nation, ready)
   and Kick. START is enabled once every connected client has claimed a
   nation and is ready; the host is always ready. An Open nation that nobody
   claimed starts as AI.
5. **JOIN prompt.** A `host[:port]` box pre-filled from `last_server`, then
   "Connecting…" with Cancel.
6. **JOIN lobby.** The same screen, read-only except for claiming an open
   nation, the leader name, and Ready. Changing the claim or the name clears
   Ready.
7. **In game.** The controller plays normally. A spectator gets the read-only
   map plus "Waiting for <Nation>": scrolling and View only, GAME → Quit.
   A forwarded `POPUP` shows over either.

## Game start (server)

- **New game.** The existing new-game commit, with
  `AiNewGameParams.human_mask` set to the human seats, leader names written
  into the nation records, DS:0x5381 bit 0x80 set when two or more humans
  play, and DS:0x5398 set to the highest human index (DOS "last ticked").
  The lore, King and sail intro phases are skipped (open question).
- **Load.** The human seats get control 0 and the rest get 1. Bit 0x80 is set
  when two or more humans play. DS:0x5398 is kept if it is still human,
  otherwise it becomes the highest human.
- Then `START` goes out and the year loop runs on the server until the first
  human slot: a local seat plays there, a remote seat gets `SYNC`.

A network game is a valid DOS hot-seat save at every point.

## Disconnects

- **Spectator drops.** Their seat shows "disconnected". Play goes on until
  that seat's slot comes up, and then the host chooses Wait or Hand to AI.
- **Controller drops mid-turn.** The server has replayed every frame it
  received, but that state may be mid-screen. The host chooses between
  **Wait**, which rolls the server back to that turn's `SYNC` blob and resends
  it on rejoin, and **Hand to AI**, which rolls back the same way, sets control
  1, and lets the AI play the slot.
- **Rejoin.** `HELLO` with a matching leader name reclaims the seat.
- **Host drops.** The game ends. The host relaunches, picks HOST → Load (the
  autosave), and the others rejoin.

## Touch points

- **Server, at a remote human slot.** Instead of rebinding in place
  (`game_hotseat_begin_turn`), the server captures, sends `SYNC`, and
  switches its input source to the remote queue at the client's geometry.
- **Client, resume.** A new `game_load_col1_memory` next to
  `game_load_col1_file` that resumes at the blob's `nation_turn` with the
  given `turn_proc`, not at DS:0x5398 the way a DOS hot-seat load does.
- **Client, End of Turn.** `game_do_end_turn` sends `EOT` and waits; it never
  starts the processor.
- **Input source.** One switch at `platform_poll_input`'s caller, choosing
  between live input, the replay queue (server) and the recorder tap
  (client). The recorder already exists (`debug.input_logger`), and so does
  the replayer (`regression_replay`). Both get lifted out of debug/test-only
  code into a shared piece.
- **Popups.** A forwarding hook at `ai_popup_try_present_next` for a remote
  controller.
- **Off in network games:** autosave on clients, cheat and debug menus, and
  the music-pick RNG reseed.

Network code stays out of `colonize_sim`: `src/platform/net_socket.c` (the
socket layer), `src/core/game_loop_net.c` (the protocol, SYNC/replay and popup
forwarding), and `src/core/net_lobby_screen.c` (the screens).
`colonize_sim_linkcheck` stays green.

## Rules inherited from DOS hot-seat (decision pending)

Phase 1 ships DOS hot-seat rules unchanged:

- Only DS:0x5398 has a King (taxes, rebel messages, score, year-end
  win/loss).
- The first human to declare independence withdraws every other power,
  humans included.
- There is no War of Succession.

Per-human Kings would be a `divergences.*` flag that needs its own design
pass.

## Phases

0. **Spike, no sockets.** In one process, two game states with an in-memory
   pipe. A runs the `hotseat_handoff` regression as the "client" with a
   recorder tap, and B is the "server": it SYNCs, replays the taps and runs
   the AI pass. B's final save must equal the plain hot-seat run's save. This
   also settles the music reseed, the nested polls and geometry, and
   produces the popup/hook audit for forwarding. The test stays as the
   sim-side golden.
1. **Transport + protocol.** `net_socket`, framing, messages, the settings
   block and docs/settings.md. Unit test: frame encode/decode with split and
   partial reads over a localhost pair.
2. **Lobby.** The title-menu row, the HOST/JOIN screens, lobby sync, the
   version check and START gating.
3. **Play.** START (new game and load), SYNC/INPUT/EOT, server replay, popup
   forwarding, snapshots for spectators, hash checks, and server autosave.
   Integration test: server + one client in one process over 127.0.0.1,
   scripted through a short game, with the server's end state equal to the
   phase 0 golden.
4. **Robustness.** Disconnect/rejoin and rollback, Wait / Hand to AI,
   keepalive, Kick, the Windows build, and manual two-copy and two-machine
   runs.

## Open questions

- **Port-only wording** (menu row, lobby labels, "Waiting for…"): hard-coded
  in the lobby file, or a port-owned catalog in GAME.TXT format?
- **Intro cutscenes in network games:** skip them, or play each one locally
  on the client's first `SYNC`?
- **A headless dedicated server** (`--server`, no seat, no window): it
  becomes cheap under this model. Is it wanted?
- **Chat:** not planned yet.
