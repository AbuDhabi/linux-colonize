static void sound_decode_track(
  SoundSong* song,
  const uint8_t* ds_img,
  size_t ds_size,
  uint16_t start_off,
  uint8_t channel
) {
  if (!song || !ds_img || ds_size == 0 || (size_t)start_off >= ds_size) {
    return;
  }

  uint32_t time = 0;
  size_t pos = start_off;
  uint8_t velocity = 64;
  uint8_t artic_sub = 0; /* F7: gate = dur - artic_sub */
  uint8_t artic_abs = 0; /* F6: gate = artic_abs when nonzero */
  uint8_t transpose = 0; /* EE */
  uint8_t volume = 100;  /* CC7; F1/F3 */
  int8_t vol_delta = 0;  /* F3 */
  uint8_t vol_period = 0;
  uint8_t vol_count = 0;
  uint8_t regs[64]; /* FUN_1000_01fd DS:5c+reg — song ALU / cond jumps */
  memset(regs, 0, sizeof(regs));

  size_t loop_start = start_off;
  int loop_count = 0;
  int nest_count = 0;
  size_t nest_start = start_off;
  size_t call_stack[SOUND_MAX_CALL_DEPTH];
  int call_depth = 0;
  int stuck = 0;

  while (time < SOUND_MAX_TRACK_TICKS && song->event_count < SOUND_MAX_EVENTS - 8) {
    if (pos >= ds_size) {
      break;
    }
    const size_t pos_before = pos;
    const uint8_t op = ds_img[pos];

    if (op <= 0xBA) {
      if (pos + 1 >= ds_size) {
        break;
      }
      const uint8_t note_raw = op;
      const uint8_t dur = ds_img[pos + 1];
      pos += 2;

      if (note_raw == 0 && dur == 0) {
        /* Terminal rest used at track ends — stop expanding. */
        break;
      }

      const uint8_t gate = sound_note_gate(dur, artic_abs, artic_sub);
      if (note_raw == 0) {
        sound_advance_time(
          song, &time, dur ? (uint32_t)dur : 1u, channel, &volume, &vol_delta, vol_period, &vol_count
        );
      } else {
        sound_emit_note(
          song, time, channel, (int)note_raw + (int8_t)transpose, velocity, dur, gate
        );
        sound_advance_time(
          song, &time, dur ? (uint32_t)dur : 1u, channel, &volume, &vol_delta, vol_period, &vol_count
        );
      }
      stuck = 0;
      continue;
    }

    /* Opcode 0xBB..0xFF — FUN_1000_01fd */
    switch (op) {
      case 0xF4: /* velocity → voice+6 */
        if (pos + 1 >= ds_size) {
          return;
        }
        velocity = ds_img[pos + 1];
        pos += 2;
        break;
      case 0xF8: /* program change */
        if (pos + 1 >= ds_size) {
          return;
        }
        sound_push_event(song, time, 0xc0, ds_img[pos + 1] & 0x7f, 0, channel);
        pos += 2;
        break;
      case 0xC3: /* FUN_1000_01bf → hardware patch queue; not in song streams */
        if (pos + 1 >= ds_size) {
          return;
        }
        pos += 2;
        break;
      case 0xC4: /* far call via stream word — treat like FA into DS when in range */
        if (pos + 2 >= ds_size) {
          return;
        }
        {
          const uint16_t abs = (uint16_t)(ds_img[pos + 1] | ((uint16_t)ds_img[pos + 2] << 8));
          const size_t ret = pos + 3;
          if ((size_t)abs < ds_size && call_depth < SOUND_MAX_CALL_DEPTH) {
            call_stack[call_depth++] = ret;
            pos = abs;
          } else {
            pos = ret;
          }
        }
        break;
      case 0xC5: /* reg[a] <= reg[b] ? skip : jump */
      case 0xC6:
      case 0xC7:
      case 0xC8:
      case 0xC9:
      case 0xCA:
      case 0xCB:
      case 0xCC:
      case 0xCD:
      case 0xCE:
      case 0xCF:
      case 0xD0:
      case 0xD1:
      case 0xD2:
      case 0xD3:
      case 0xD4: {
        /* 5-byte cond jump: op, a, b|imm, tgt_lo, tgt_hi (FUN_1000_01fd). */
        if (pos + 4 >= ds_size) {
          return;
        }
        const uint8_t a = ds_img[pos + 1] & 63;
        const uint8_t b = ds_img[pos + 2];
        const uint8_t ra = regs[a];
        const uint8_t rb = (op <= 0xc8 || (op >= 0xcd && op <= 0xd0)) ? regs[b & 63] : b;
        bool take = false;
        switch (op) {
          case 0xc5: take = !(ra < rb || ra == rb); break; /* ja */
          case 0xc6: take = ra < rb; break;                /* jb */
          case 0xc7: take = ra != rb; break;               /* jne */
          case 0xc8: take = ra == rb; break;               /* je */
          case 0xc9: take = !(ra < rb || ra == rb); break;
          case 0xca: take = ra < rb; break;
          case 0xcb: take = ra != rb; break;
          case 0xcc: take = ra == rb; break;
          case 0xcd: take = !(ra < rb || ra == rb); break;
          case 0xce: take = ra < rb; break;
          case 0xcf: take = ra != rb; break;
          case 0xd0: take = ra == rb; break;
          case 0xd1: take = !(ra < rb || ra == rb); break;
          case 0xd2: take = ra < rb; break;
          case 0xd3: take = ra != rb; break;
          case 0xd4: take = ra == rb; break;
          default: break;
        }
        if (take) {
          if (call_depth < SOUND_MAX_CALL_DEPTH) {
            call_stack[call_depth++] = pos + 5; /* return after insn (driver +0x22) */
          }
          pos = (size_t)(ds_img[pos + 3] | ((uint16_t)ds_img[pos + 4] << 8));
        } else {
          pos += 5;
        }
        break;
      }
      case 0xD5: /* reg[a] ^= reg[b] */
      case 0xD6: /* reg[a] ^= imm */
      case 0xD7:
      case 0xD8:
      case 0xD9:
      case 0xDA:
      case 0xDB:
      case 0xDC:
      case 0xDD:
      case 0xDE:
      case 0xDF:
      case 0xE0:
      case 0xE1:
      case 0xE2:
      case 0xE3:
      case 0xE4:
      case 0xE7:
      case 0xE8:
      case 0xE9: {
        if (pos + 2 >= ds_size) {
          return;
        }
        const uint8_t a = ds_img[pos + 1] & 63;
        const uint8_t b = ds_img[pos + 2];
        const uint8_t rb = (op == 0xd5 || op == 0xd7 || op == 0xd9 || op == 0xdb || op == 0xdd ||
                            op == 0xdf || op == 0xe1 || op == 0xe3 || op == 0xe8)
                             ? regs[b & 63]
                             : b;
        switch (op) {
          case 0xd5:
          case 0xd6: regs[a] = (uint8_t)(regs[a] ^ rb); break;
          case 0xd7:
          case 0xd8: regs[a] = (uint8_t)(regs[a] | rb); break;
          case 0xd9:
          case 0xda: regs[a] = (uint8_t)(regs[a] & rb); break;
          case 0xdb:
          case 0xdc: regs[a] = rb ? (uint8_t)(regs[a] % rb) : 0; break;
          case 0xdd:
          case 0xde: regs[a] = rb ? (uint8_t)(regs[a] / rb) : 0; break;
          case 0xdf:
          case 0xe0: regs[a] = (uint8_t)(regs[a] * rb); break;
          case 0xe1:
          case 0xe2: regs[a] = (uint8_t)(regs[a] - rb); break;
          case 0xe3:
          case 0xe4: regs[a] = (uint8_t)(regs[a] + rb); break;
          case 0xe8: regs[a] = rb; break;
          case 0xe9: regs[a] = b; break;
          case 0xe7: /* stream poke — size-only */ break;
          default: break;
        }
        pos += 3;
        break;
      }
      case 0xE5: /* reg[a]-- */
      case 0xE6: /* reg[a]++ */
        if (pos + 1 >= ds_size) {
          return;
        }
        {
          const uint8_t a = ds_img[pos + 1] & 63;
          if (op == 0xe5) {
            regs[a]--;
          } else {
            regs[a]++;
          }
        }
        pos += 2;
        break;
      case 0xEA: /* indexed stream poke — 4 bytes */
      case 0xEB: /* random in [lo,hi] written ahead — 4 bytes */
        if (pos + 3 >= ds_size) {
          return;
        }
        pos += 4;
        break;
      case 0xEC: { /* pick random of n bytes into stream; then duration */
        if (pos + 1 >= ds_size) {
          return;
        }
        const uint8_t n = ds_img[pos + 1];
        if (pos + 2u + (size_t)n >= ds_size) {
          return;
        }
        pos += 2u + (size_t)n + 1u;
        break;
      }
      case 0xF1: /* CC 7 volume */
        if (pos + 1 >= ds_size) {
          return;
        }
        volume = ds_img[pos + 1] & 0x7f;
        sound_push_event(song, time, 0xb0, 7, volume, channel);
        pos += 2;
        break;
      case 0xF0: /* CC 10 pan */
        if (pos + 1 >= ds_size) {
          return;
        }
        sound_push_event(song, time, 0xb0, 10, ds_img[pos + 1] & 0x7f, channel);
        pos += 2;
        break;
      case 0xC2: /* CC 91 reverb */
        if (pos + 1 >= ds_size) {
          return;
        }
        sound_push_event(song, time, 0xb0, 91, ds_img[pos + 1] & 0x7f, channel);
        pos += 2;
        break;
      case 0xC1: /* CC 93 chorus */
        if (pos + 1 >= ds_size) {
          return;
        }
        sound_push_event(song, time, 0xb0, 93, ds_img[pos + 1] & 0x7f, channel);
        pos += 2;
        break;
      case 0xC0: /* CC 0 bank select */
        if (pos + 1 >= ds_size) {
          return;
        }
        sound_push_event(song, time, 0xb0, 0, ds_img[pos + 1] & 0x7f, channel);
        pos += 2;
        break;
      case 0xF2: /* pitch bend (high byte; low forced 0 like driver) */
        if (pos + 1 >= ds_size) {
          return;
        }
        sound_push_event(song, time, 0xe0, 0, ds_img[pos + 1] & 0x7f, channel);
        pos += 2;
        break;
      case 0xF6: /* absolute gate */
        if (pos + 1 >= ds_size) {
          return;
        }
        artic_abs = ds_img[pos + 1];
        artic_sub = 0;
        pos += 2;
        break;
      case 0xF7: /* subtractive articulation */
        if (pos + 1 >= ds_size) {
          return;
        }
        artic_sub = ds_img[pos + 1];
        artic_abs = 0;
        pos += 2;
        break;
      case 0xEE: /* per-voice transpose */
        if (pos + 1 >= ds_size) {
          return;
        }
        transpose = ds_img[pos + 1];
        pos += 2;
        break;
      case 0xED: { /* chord: ED n note×n dur — up to 4 slots (FUN_1000_01fd) */
        if (pos + 1 >= ds_size) {
          return;
        }
        const uint8_t n_raw = ds_img[pos + 1];
        const uint8_t n_play = n_raw > SOUND_ED_MAX_NOTES ? SOUND_ED_MAX_NOTES : n_raw;
        if (pos + 2u + (size_t)n_raw >= ds_size) {
          return;
        }
        const uint8_t dur = ds_img[pos + 2u + (size_t)n_raw];
        const uint8_t gate = sound_note_gate(dur, artic_abs, artic_sub);
        for (uint8_t i = 0; i < n_play; ++i) {
          const uint8_t note_raw = ds_img[pos + 2u + (size_t)i];
          sound_emit_note(
            song, time, channel, (int)note_raw + (int8_t)transpose, velocity, dur, gate
          );
        }
        pos += 3u + (size_t)n_raw;
        sound_advance_time(
          song, &time, dur ? (uint32_t)dur : 1u, channel, &volume, &vol_delta, vol_period, &vol_count
        );
        break;
      }
      case 0xBB: /* RPN pitch-bend range: CC101=0, CC100=0, CC6=n */
        if (pos + 1 >= ds_size) {
          return;
        }
        sound_push_event(song, time, 0xb0, 101, 0, channel);
        sound_push_event(song, time, 0xb0, 100, 0, channel);
        sound_push_event(song, time, 0xb0, 6, ds_img[pos + 1] & 0x7f, channel);
        pos += 2;
        break;
      case 0xF3: /* volume envelope: period, delta */
        if (pos + 2 >= ds_size) {
          return;
        }
        vol_period = ds_img[pos + 1];
        vol_delta = (int8_t)ds_img[pos + 2];
        vol_count = 1; /* fire on next tick, matching driver init of +0xa = 1 */
        pos += 3;
        break;
      case 0xBF: /* master scale factor → unread product with BE (no tick effect) */
      case 0xBC: /* sets DS:0x50 countdown seed; stream-skip only */
      case 0xBD: /* sets DS:0x52; stream-skip only */
        pos += 2;
        break;
      case 0xBE: /* tempo pair → unread BSS product; IRQ still 60 Hz */
        if (pos + 2 >= ds_size) {
          return;
        }
        pos += 3;
        break;
      case 0xEF: /* pan envelope (unused in BGM corpus) */
        if (pos + 2 >= ds_size) {
          return;
        }
        pos += 3;
        break;
      case 0xF5: /* pitch envelope (rare) */
        if (pos + 3 >= ds_size) {
          return;
        }
        pos += 4;
        break;
      case 0xFC: { /* set loop/stream anchors to absolute DS offset */
        if (pos + 2 >= ds_size) {
          return;
        }
        const uint16_t abs = (uint16_t)(ds_img[pos + 1] | ((uint16_t)ds_img[pos + 2] << 8));
        loop_start = abs;
        nest_start = abs;
        pos = abs;
        break;
      }
      case 0xFB: { /* jump absolute */
        if (pos + 2 >= ds_size) {
          return;
        }
        const uint16_t abs = (uint16_t)(ds_img[pos + 1] | ((uint16_t)ds_img[pos + 2] << 8));
        pos = abs;
        break;
      }
      case 0xFA: { /* call absolute */
        if (pos + 2 >= ds_size) {
          return;
        }
        const uint16_t abs = (uint16_t)(ds_img[pos + 1] | ((uint16_t)ds_img[pos + 2] << 8));
        const size_t ret = pos + 3;
        if (call_depth < SOUND_MAX_CALL_DEPTH) {
          call_stack[call_depth++] = ret;
          pos = abs;
        } else {
          pos = ret;
        }
        break;
      }
      case 0xF9: /* return from FA */
        if (call_depth > 0) {
          pos = call_stack[--call_depth];
        } else {
          pos += 1;
        }
        break;
      case 0xFD: /* jump to FC loop start */
        pos = loop_start;
        break;
      case 0xFF: { /* counted loop to loop_start */
        if (pos + 1 >= ds_size) {
          return;
        }
        const uint8_t count = ds_img[pos + 1];
        if (count == 0) {
          pos += 2;
          loop_start = pos;
        } else if (loop_count <= 0) {
          loop_count = count;
          if (loop_count > SOUND_MAX_LOOP_ITERS) {
            loop_count = SOUND_MAX_LOOP_ITERS;
          }
          pos = loop_start;
        } else {
          loop_count--;
          if (loop_count > 0) {
            pos = loop_start;
          } else {
            pos += 2;
          }
        }
        break;
      }
      case 0xFE: { /* nested counted loop */
        if (pos + 1 >= ds_size) {
          return;
        }
        const uint8_t count = ds_img[pos + 1];
        if (count == 0) {
          pos += 2;
          nest_start = pos;
        } else if (nest_count <= 0) {
          nest_count = count;
          if (nest_count > SOUND_MAX_LOOP_ITERS) {
            nest_count = SOUND_MAX_LOOP_ITERS;
          }
          pos = nest_start;
        } else {
          nest_count--;
          if (nest_count > 0) {
            pos = nest_start;
          } else {
            pos += 2;
          }
        }
        break;
      }
      default:
        /* Unknown high opcode: skip opcode + 1 data byte (common size). */
        pos += 2;
        break;
    }

    if (pos == pos_before) {
      if (++stuck > 8) {
        break;
      }
    } else {
      stuck = 0;
    }
  }
}
