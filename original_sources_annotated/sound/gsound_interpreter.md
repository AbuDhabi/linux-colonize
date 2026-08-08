# GSOUND.COL voice interpreter (`FUN_1000_01fd`)

Layer D notes for [`original_sources_decompiled/gsound.c`](../../original_sources_decompiled/gsound.c).
Linux port: [`src/core/sound.c`](../../src/core/sound.c).

Ghidra entry/`MPU`/EMS tails are noisy; the fidelity-relevant core is small.

## Dispatch (`FUN_1000_19bc`)

| ID range | Table (image) | Bound (DS) |
|----------|---------------|------------|
| `< 0x20` | `0x2A5C` | `DS:00F8` |
| `0x20..` BGM | `0x2A6E` | `DS:00FA` (= image `0x331A`, value `0x3F`) |
| `0x40..` event | `0x2AC4` | `DS:00FC` (value `0x5C`) |
| `≥ 0x8020` | `0x2AB6` | `DS:00FE` |

## MIDI emit helpers

| Symbol | Status | Payload |
|--------|--------|---------|
| `FUN_1000_00cb` | `0x90\|ch` note-on | note, velocity at voice+6 |
| `FUN_1000_010f` | `0xC0\|ch` program | voice+5 |
| `FUN_1000_0143` | `0xB0\|ch` CC | controller, value |
| `FUN_1000_0183` | `0xE0\|ch` pitch bend | `0`, voice+0x11 (high byte) |
| `FUN_1000_1395` | UART/MPU byte out | via `12c6` |

Channel index lives at `DS:81FE` while the active voice struct is at `[DS:8240]`.

## Voice struct (`[DS:8240]`, selected fields)

| Off | Role |
|-----|------|
| +0 | Remaining duration (ticks) |
| +1 / +0x12 / +0x13 / +0xc | Pitch envelope (F5) |
| +2 / +0xb / +0xa | Volume envelope delta / period / countdown (F3) |
| +3 / +0xe / +0xd | Pan envelope (EF) |
| +4 | Last note (pre-transpose) |
| +5 | Program |
| +6 | Velocity |
| +7 / +8 | Articulation F7 sub / F6 abs gate |
| +9 | Gate countdown |
| +0xf / +0x10 | Pan / volume CC mirrors |
| +0x11 | Pitch-bend high |
| +0x14..+0x22 | Loop / call / nest stream pointers |
| +0x25 | Transpose (EE) |
| +0x16 | Stream cursor |

Active notes for the channel: four slots at `DS:8200 + ch*4` (`0xFF` = empty).

## Opcode map (`FUN_1000_01fd`) — fidelity deltas

| Op | Bytes | Driver | Linux (`sound.c`) |
|----|------:|--------|-------------------|
| `≤BA` note,dur | 2 | note-on + gate | done |
| `C4..EB` | ALU / cond-jump VM | **done** (sizes + regs; was default `+2` desync) |
| `ED n notes dur` | 3+n | chord into ≤4 slots | **done** (was `+2` desync) |
| `BB n` | 2 | RPN CC101=0,100=0,CC6=n | **done** |
| `F3 period delta` | 3 | per-tick CC7 ramp | **done** |
| `F5` / `EF` | 4 / 3 | pitch / pan envelopes | skip (rare / unused in BGM) |
| `BE a b` | 3 | write `DS:54`,`8091`; product `8091*8090` → `53`/`8092` | skip timing (see below) |
| `BF n` | 2 | set `8090`, recompute product | skip timing |
| `BC` / `BD` | 2 | seed `DS:50` / `DS:52` | skip |
| `C3` | 2 | `01bf` → hardware patch queue | skip (absent from songs) |
| `F4`/`F8`/`F0`/`F1`/`F2`/`F6`/`F7`/`EE` | 2 | vel/prog/pan/vol/bend/gate/xpose | done |
| `C0`/`C1`/`C2` | 2 | bank / chorus / reverb CC | done |
| `FA`/`F9`/`FB`/`FC`/`FD`/`FE`/`FF` | var | call/ret/jump/loop | done |

## Tick rate / BE·BF

PIT divisor `DS:0081 = 0x4DBF` (~60 Hz). The IRQ path (misparsed as data near `0x1098`) **always** calls the unrolled per-voice `01fd` each IRQ.

`BE`/`BF` write a product into `DS:8092` / `DS:53`, but **no instruction in `GSOUND.COL` reads those locations**. A separate countdown at `DS:E6`←`DS:E8` (song handlers set `0x30`/`0xC0`) gates a slower host callback (`call bx` via `DS:EA`), not voice advancing.

So bytecode duration ticks already match wall-clock at ~60 Hz; do **not** scale time by the BE·BF product.

## Multi-voice tick (`0x1098`, Ghidra-as-data)

Unrolled: set `DS:81FE = ch`, point `DS:8240` at each voice block (`0x8096`, `0x80E6`, …), `CALL 01fd`. Recover as code if deepening further.

## Song handler cold-start

Table entries normally begin `CALL … / MOV CX,track / CALL start_voice / … / RET`.

Some (e.g. id `0x25` Fiddler's Dance) point at a **warm-restart** stub
(`CMP word [DS:E6],0 / … / RET`). The real cold-start (seed `E6`/`E8`/`EA` then
the `B9` track list) is the next function after that `RET`. Linux
`sound_parse_handler_tracks` detects the `83 3E E6 00` stub and parses from there.
