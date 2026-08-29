#include "core/gsound_vm.h"

#include <stdlib.h>
#include <string.h>

/*
 * Symbols below are image offsets in GSOUND.COL (MZ header stripped). The
 * data segment starts at paragraph 0x322 (image 0x3220); DS-relative
 * addresses in the sequencer are resolved through vm->ds.
 */
#define GS_DS_PARA 0x0322
#define GS_DS_BASE ((uint32_t)GS_DS_PARA * 16u)
#define GS_DS_SIZE 0x8300u /* image tail (0x8082) + BSS voice blocks */

#define GS_TABLE_SYS 0x2A5C
#define GS_TABLE_BGM 0x2A6E
#define GS_TABLE_EVT 0x2AC4
#define GS_BOUND_SYS 0x00F8
#define GS_BOUND_BGM 0x00FA
#define GS_BOUND_EVT 0x00FC
/* FUN_1000_19bc: ids 0x8020.. index table 0x2AB6 by (id + 0x7fe0) = id − 0x8020;
 * these are the chord stings (war declaration 0x8020, assign colonist 0x8024). */
#define GS_TABLE_STING 0x2AB6
#define GS_BOUND_STING 0x00FE

#define GS_VOICE_STRIDE 0x28
#define GS_VOICE0 0x8096
#define GS_SLOTS 0x8200 /* four note slots per channel */
#define GS_TERMINATOR 0x8080 /* "00 00" rest that ends a stream */
#define GS_REGS 0x005C /* song ALU register file */
#define GS_PRNG 0x0087
#define GS_E6 0x00E6
#define GS_E8 0x00E8
#define GS_EA 0x00EA

/* Voice block fields (DS:voice + off). */
enum {
  V_DUR = 0x00,
  V_PENV_DELTA = 0x01,
  V_VENV_DELTA = 0x02,
  V_PAN_DELTA = 0x03,
  V_LAST_NOTE = 0x04,
  V_PROG = 0x05,
  V_VEL = 0x06,
  V_ARTIC_SUB = 0x07,
  V_ARTIC_ABS = 0x08,
  V_GATE = 0x09,
  V_VENV_CNT = 0x0a,
  V_VENV_PERIOD = 0x0b,
  V_PENV_CNT = 0x0c,
  V_PAN_CNT = 0x0d,
  V_PAN_PERIOD = 0x0e,
  V_PAN = 0x0f,
  V_VOL = 0x10,
  V_BEND = 0x11,
  V_PENV_PERIOD = 0x12,
  V_PENV_LEN = 0x13,
  W_LOOP0 = 0x14,
  W_POS = 0x16,
  W_FF_MARK = 0x18,
  W_FE_MARK = 0x1a,
  W_FF_CNT = 0x1c,
  W_FE_CNT = 0x1e,
  W_SONG_START = 0x20,
  W_RET = 0x22,
  V_TRANSPOSE = 0x25,
  V_FADE = 0x26,
};

/* Voice block address for MIDI channel 1..9 (channel 9 lives at 0x80BE). */
static const uint16_t k_voice_by_channel[10] = {
  0, 0x8096, 0x80E6, 0x810E, 0x8136, 0x815E, 0x8186, 0x81AE, 0x81D6, 0x80BE,
};

struct GsoundVm {
  const uint8_t* img;
  size_t img_size;
  uint8_t ds[GS_DS_SIZE];
  GsoundMidiFn midi;
  void* midi_user;
  GsoundSfxFn sfx;
  void* sfx_user;

  uint8_t chord_flag; /* DS:5A */
  uint8_t cur_channel; /* DS:81FE */
  uint8_t fade_enable; /* CS:1106 */
  int8_t fade_count;   /* CS:1105 */

  uint32_t ticks;
  uint32_t loop_tick;
  int unsupported;
  int depth;
};

static uint16_t rd16(const uint8_t* p) {
  return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static void wr16(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}

static uint16_t ds16(const GsoundVm* vm, uint16_t a) {
  return rd16(vm->ds + a);
}

static void ds16w(GsoundVm* vm, uint16_t a, uint16_t v) {
  wr16(vm->ds + a, v);
}

static void emit(GsoundVm* vm, uint8_t status, uint8_t d1, uint8_t d2) {
  if (vm->midi) {
    vm->midi(vm->midi_user, status, d1 & 0x7f, d2 & 0x7f);
  }
}

static void emit_cc(GsoundVm* vm, uint8_t ch, uint8_t cc, uint8_t val) {
  emit(vm, (uint8_t)(0xB0 | (ch & 0x0f)), cc, val);
}

/* FUN_1000_118a: ax = ror3(0x9249 + seed); seed = ax. */
static uint16_t prng(GsoundVm* vm) {
  uint16_t ax = (uint16_t)(0x9249 + ds16(vm, GS_PRNG));
  for (int i = 0; i < 3; ++i) {
    ax = (uint16_t)((ax >> 1) | ((ax & 1) << 15));
  }
  ds16w(vm, GS_PRNG, ax);
  return ax;
}

/* FUN_1000_0048: release every slot of the current channel. */
static void notes_off(GsoundVm* vm) {
  const uint8_t ch = vm->cur_channel;
  uint8_t* slots = vm->ds + GS_SLOTS + (uint16_t)ch * 4u;
  for (int i = 0; i < 4; ++i) {
    if (slots[i] != 0xff) {
      emit(vm, (uint8_t)(0x90 | (ch & 0x0f)), slots[i], 0);
    }
    slots[i] = 0xff;
  }
}

static void note_on(GsoundVm* vm, uint8_t* v, uint8_t note) {
  emit(vm, (uint8_t)(0x90 | (vm->cur_channel & 0x0f)), note, v[V_VEL]);
}

static uint8_t* reg_ptr(GsoundVm* vm, uint8_t idx) {
  return vm->ds + (uint16_t)(GS_REGS + (int8_t)idx);
}

static void run_x86(GsoundVm* vm, uint16_t ip);

/* FUN_1000_01fd: advance one voice by one tick. vm->cur_channel selects slots. */
static void voice_tick(GsoundVm* vm, uint16_t vaddr) {
  uint8_t* ds = vm->ds;
  uint8_t* v = ds + vaddr;
  const uint8_t ch = vm->cur_channel;
  uint8_t* slots = ds + GS_SLOTS + (uint16_t)ch * 4u;

  if (v[V_DUR] == 0) {
    goto envelopes;
  }
  if (v[V_GATE] != 0) {
    v[V_GATE]--;
    if (v[V_GATE] == 0) {
      notes_off(vm);
    }
  }
  {
    const uint8_t old = v[V_DUR];
    v[V_DUR]--;
    if (old > 1) { /* SUB [bx],1 ; JBE parse */
      goto envelopes;
    }
  }

  for (;;) {
    uint16_t p = rd16(v + W_POS);
    const uint8_t op = ds[p];
    if (!((op & 0x80) && (int8_t)op > (int8_t)0xBA)) {
      /* Note / rest: note, dur. */
      if (vm->chord_flag) {
        vm->chord_flag = 0;
        break;
      }
      const uint8_t note = ds[p];
      const uint8_t dur = ds[(uint16_t)(p + 1)];
      v[V_DUR] = dur;
      wr16(v + W_POS, (uint16_t)(p + 2));
      if (v[V_LAST_NOTE] != note) {
        notes_off(vm);
      }
      if (note == 0 || v[V_DUR] == 0) {
        notes_off(vm);
        break;
      }
      v[V_GATE] = v[V_ARTIC_ABS] ? v[V_ARTIC_ABS] : (uint8_t)(dur - v[V_ARTIC_SUB]);
      const uint8_t n = (uint8_t)(note + v[V_TRANSPOSE]);
      if ((int8_t)v[V_ARTIC_SUB] >= 0 || slots[0] != n) {
        v[V_LAST_NOTE] = (uint8_t)(n - v[V_TRANSPOSE]);
        slots[0] = n;
        note_on(vm, v, n);
      }
      break;
    }

    const uint8_t b1 = ds[(uint16_t)(p + 1)];
    const uint8_t b2 = ds[(uint16_t)(p + 2)];
    uint16_t next = (uint16_t)(p + 2);
    switch (op) {
      case 0xBB:
        emit_cc(vm, ch, 101, 0);
        emit_cc(vm, ch, 100, 0);
        emit_cc(vm, ch, 6, b1);
        emit_cc(vm, ch, 38, 0);
        break;
      case 0xBC: /* seed DS:50 / clear 8092,52 — unread */
      case 0xBD:
      case 0xBF: /* tempo product — written, never read */
        break;
      case 0xBE:
        next = (uint16_t)(p + 3);
        break;
      case 0xC0:
        emit_cc(vm, ch, 0, b1);
        break;
      case 0xC1:
        emit_cc(vm, ch, 93, b1);
        break;
      case 0xC2:
        emit_cc(vm, ch, 91, b1);
        break;
      case 0xC3: /* FUN_1000_01bf → digital sample b1 when host enabled DS:7C */
        if (vm->sfx) {
          vm->sfx(vm->sfx_user, (int8_t)b1);
        }
        break;
      case 0xC4: /* call code at CS:imm16 */
        run_x86(vm, rd16(ds + (uint16_t)(p + 1)));
        next = (uint16_t)(p + 3);
        break;
      case 0xC5: case 0xC6: case 0xC7: case 0xC8:
      case 0xC9: case 0xCA: case 0xCB: case 0xCC:
      case 0xCD: case 0xCE: case 0xCF: case 0xD0:
      case 0xD1: case 0xD2: case 0xD3: case 0xD4: {
        const bool imm = (op >= 0xC9 && op <= 0xCC) || op >= 0xD1;
        const bool is_call = op <= 0xCC;
        const uint16_t ra = *reg_ptr(vm, b1);
        const uint16_t rb = imm ? (uint16_t)(int16_t)(int8_t)b2 : (uint16_t)*reg_ptr(vm, b2);
        bool take = false;
        switch ((op - 0xC5) & 3) {
          case 0: take = ra > rb; break;
          case 1: take = ra < rb; break;
          case 2: take = ra != rb; break;
          default: take = ra == rb; break;
        }
        if (take) {
          if (is_call) {
            wr16(v + W_RET, (uint16_t)(p + 5));
          }
          next = rd16(ds + (uint16_t)(p + 3));
        } else {
          next = (uint16_t)(p + 5);
        }
        break;
      }
      case 0xD5: case 0xD6: case 0xD7: case 0xD8: case 0xD9: case 0xDA:
      case 0xDB: case 0xDC: case 0xDD: case 0xDE: case 0xDF: case 0xE0:
      case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE8: case 0xE9: {
        uint8_t* ra = reg_ptr(vm, b1);
        const bool imm = (op & 1) == 0 || op == 0xE9;
        const uint8_t rb = imm ? b2 : *reg_ptr(vm, b2);
        switch (op) {
          case 0xD5: case 0xD6: *ra ^= rb; break;
          case 0xD7: case 0xD8: *ra |= rb; break;
          case 0xD9: case 0xDA: *ra &= rb; break;
          case 0xDB: *ra = rb ? (uint8_t)(*ra % rb) : *ra; break;
          case 0xDC: {
            const uint16_t d = (uint16_t)(int16_t)(int8_t)b2;
            *ra = d ? (uint8_t)(*ra % d) : *ra;
            break;
          }
          case 0xDD: *ra = rb ? (uint8_t)(*ra / rb) : *ra; break;
          case 0xDE: {
            const uint16_t d = (uint16_t)(int16_t)(int8_t)b2;
            *ra = d ? (uint8_t)(*ra / d) : *ra;
            break;
          }
          case 0xDF: case 0xE0: *ra = (uint8_t)((int8_t)*ra * (int8_t)rb); break;
          case 0xE1: case 0xE2: *ra = (uint8_t)(*ra - rb); break;
          case 0xE3: case 0xE4: *ra = (uint8_t)(*ra + rb); break;
          case 0xE8: case 0xE9: *ra = rb; break;
          default: break;
        }
        next = (uint16_t)(p + 3);
        break;
      }
      case 0xE5:
        (*reg_ptr(vm, b1))--;
        break;
      case 0xE6:
        (*reg_ptr(vm, b1))++;
        break;
      case 0xE7: /* poke reg into stream at +disp */
        ds[(uint16_t)(p + 2 + (int8_t)b2 + 1)] = *reg_ptr(vm, b1);
        next = (uint16_t)(p + 3);
        break;
      case 0xEA: { /* indexed table pick into stream */
        const int n = (int8_t)b2;
        const uint16_t P = (uint16_t)(p + 3);
        const uint8_t src = ds[(uint16_t)(P + *reg_ptr(vm, b1))];
        ds[(uint16_t)(P + (int8_t)ds[(uint16_t)(P + n)] + n + 1)] = src;
        next = (uint16_t)(p + n + 4);
        break;
      }
      case 0xEB: { /* random in [lo,hi] into stream */
        const int lo = (int8_t)b1;
        const int hi = (int8_t)b2;
        const uint16_t P = (uint16_t)(p + 3);
        const uint16_t r = prng(vm);
        const int span = hi - lo + 1;
        const int val = span ? (int)((r & 0x7fff) % (uint16_t)span) : 0;
        ds[(uint16_t)(P + (int8_t)ds[P] + 1)] = (uint8_t)(val + lo);
        next = (uint16_t)(p + 4);
        break;
      }
      case 0xEC: { /* random pick of n bytes into stream */
        const uint16_t n = (uint16_t)(int16_t)(int8_t)b1;
        const uint16_t P = (uint16_t)(p + 2);
        const uint16_t r = prng(vm);
        const uint8_t src = ds[(uint16_t)(P + (n ? (r & 0x7fff) % n : 0))];
        ds[(uint16_t)(P + (int8_t)ds[(uint16_t)(P + n)] + n + 1)] = src;
        next = (uint16_t)(p + n + 3);
        break;
      }
      case 0xED: { /* chord: n, notes[n], dur */
        const uint16_t n = (uint16_t)(int16_t)(int8_t)b1;
        const int first = (int)(int8_t)b2 + (int)(int8_t)v[V_TRANSPOSE];
        if (first != (int)slots[0]) {
          notes_off(vm);
        }
        uint16_t q = (uint16_t)(p + 1);
        for (uint16_t i = 0; i < n; ++i) {
          q++;
          const uint8_t nn = (uint8_t)(v[V_TRANSPOSE] + ds[q]);
          if ((int8_t)v[V_ARTIC_SUB] >= 0 || ds[(uint16_t)(GS_SLOTS + ch * 4u + i)] != nn) {
            ds[(uint16_t)(GS_SLOTS + ch * 4u + i)] = nn;
            note_on(vm, v, nn);
          }
        }
        for (uint16_t i = n; i < 4; ++i) {
          ds[(uint16_t)(GS_SLOTS + ch * 4u + i)] = 0xff;
        }
        q++;
        const uint8_t dur = ds[q];
        v[V_DUR] = dur;
        v[V_GATE] = v[V_ARTIC_ABS] ? v[V_ARTIC_ABS] : (uint8_t)(dur - v[V_ARTIC_SUB]);
        next = (uint16_t)(p + n + 3);
        vm->chord_flag = 1;
        break;
      }
      case 0xEE:
        v[V_TRANSPOSE] = b1;
        break;
      case 0xEF:
        v[V_PAN_PERIOD] = b1;
        v[V_PAN_DELTA] = b2;
        v[V_PAN_CNT] = 1;
        next = (uint16_t)(p + 3);
        break;
      case 0xF0:
        v[V_PAN] = b1;
        emit_cc(vm, ch, 10, b1);
        break;
      case 0xF1:
        if (v[V_FADE] == 0 || (uint16_t)(int16_t)(int8_t)b1 < v[V_VOL]) {
          v[V_VOL] = b1;
        }
        emit_cc(vm, ch, 7, v[V_VOL]);
        break;
      case 0xF2:
        v[V_BEND] = b1;
        emit(vm, (uint8_t)(0xE0 | ch), 0, v[V_BEND]);
        break;
      case 0xF3:
        if (v[V_FADE] == 0) {
          v[V_VENV_PERIOD] = b1;
          v[V_VENV_DELTA] = b2;
          v[V_VENV_CNT] = 1;
        }
        next = (uint16_t)(p + 3);
        break;
      case 0xF4:
        v[V_VEL] = b1;
        break;
      case 0xF5:
        v[V_PENV_PERIOD] = b1;
        v[V_PENV_DELTA] = b2;
        v[V_PENV_LEN] = ds[(uint16_t)(p + 3)];
        v[V_PENV_CNT] = 1;
        next = (uint16_t)(p + 4);
        break;
      case 0xF6:
        v[V_ARTIC_ABS] = b1;
        v[V_ARTIC_SUB] = 0;
        break;
      case 0xF7:
        v[V_ARTIC_SUB] = b1;
        v[V_ARTIC_ABS] = 0;
        break;
      case 0xF8:
        v[V_PROG] = b1;
        emit(vm, (uint8_t)(0xC0 | ch), v[V_PROG], 0);
        break;
      case 0xF9:
        if (rd16(v + W_RET) == 0) {
          next = (uint16_t)(p + 1);
        } else {
          next = rd16(v + W_RET);
          wr16(v + W_RET, 0);
        }
        break;
      case 0xFA:
        wr16(v + W_RET, (uint16_t)(p + 3));
        next = rd16(ds + (uint16_t)(p + 1));
        break;
      case 0xFB:
        next = rd16(ds + (uint16_t)(p + 1));
        break;
      case 0xFC: {
        const uint16_t t = rd16(ds + (uint16_t)(p + 1));
        wr16(v + W_LOOP0, t);
        wr16(v + W_FF_MARK, t);
        wr16(v + W_FE_MARK, t);
        wr16(v + W_SONG_START, t);
        next = t;
        break;
      }
      case 0xFD:
        if (rd16(v + W_SONG_START) == 0) {
          next = rd16(v + W_LOOP0);
        } else {
          const uint16_t t = rd16(v + W_SONG_START);
          wr16(v + W_LOOP0, t);
          wr16(v + W_FF_MARK, t);
          wr16(v + W_FE_MARK, t);
          next = t;
          if (t != GS_TERMINATOR && vm->loop_tick == 0) {
            vm->loop_tick = vm->ticks;
          }
        }
        break;
      case 0xFE:
        if (rd16(v + W_FE_CNT) == 0) {
          if (b1 == 0) {
            next = (uint16_t)(p + 2);
            wr16(v + W_FE_MARK, next);
            wr16(v + W_FF_CNT, 0);
            wr16(v + W_FE_CNT, 0);
            wr16(v + W_POS, next);
            wr16(v + W_FF_MARK, next);
            continue;
          }
          wr16(v + W_FE_CNT, (uint16_t)(int16_t)(int8_t)b1);
          next = rd16(v + W_FE_MARK);
        } else {
          uint16_t c = (uint16_t)(rd16(v + W_FE_CNT) - 1);
          wr16(v + W_FE_CNT, c);
          if (c == 0) {
            next = (uint16_t)(p + 2);
            wr16(v + W_FE_MARK, next);
          } else {
            next = rd16(v + W_FE_MARK);
          }
        }
        wr16(v + W_FF_MARK, next);
        break;
      case 0xFF:
        if (rd16(v + W_FF_CNT) == 0) {
          if (b1 == 0) {
            next = (uint16_t)(p + 2);
            wr16(v + W_FF_MARK, next);
            wr16(v + W_FF_CNT, 0);
          } else {
            wr16(v + W_FF_CNT, (uint16_t)(int16_t)(int8_t)b1);
            next = rd16(v + W_FF_MARK);
          }
        } else {
          uint16_t c = (uint16_t)(rd16(v + W_FF_CNT) - 1);
          wr16(v + W_FF_CNT, c);
          if (c == 0) {
            next = (uint16_t)(p + 2);
            wr16(v + W_FF_MARK, next);
          } else {
            next = rd16(v + W_FF_MARK);
          }
        }
        break;
      default:
        break;
    }
    wr16(v + W_POS, next);
    if (op == 0xED) {
      vm->chord_flag = 0;
      break;
    }
  }

envelopes:
  if (v[V_VENV_DELTA] != 0) {
    v[V_VENV_CNT]--;
    if (v[V_VENV_CNT] == 0) {
      v[V_VENV_CNT] = v[V_VENV_PERIOD];
      v[V_VOL] = (uint8_t)(v[V_VOL] + v[V_VENV_DELTA]);
      if (v[V_VOL] > 0x7f) {
        v[V_VENV_DELTA] = 0;
        v[V_VOL] = (v[V_VOL] <= 0xaf) ? 0x7f : 0;
      }
      emit_cc(vm, ch, 7, v[V_VOL]);
    }
  }
  if (v[V_PENV_DELTA] != 0) {
    v[V_PENV_CNT]--;
    if (v[V_PENV_CNT] == 0) {
      v[V_PENV_CNT] = v[V_PENV_PERIOD];
      v[V_BEND] = (uint8_t)(v[V_BEND] + v[V_PENV_DELTA]);
      emit(vm, (uint8_t)(0xE0 | ch), 0, v[V_BEND]);
    }
    v[V_PENV_LEN]--;
    if (v[V_PENV_LEN] == 0) {
      v[V_PENV_DELTA] = 0;
    }
  }
  if (v[V_PAN_DELTA] != 0) {
    v[V_PAN_CNT]--;
    if (v[V_PAN_CNT] == 0) {
      v[V_PAN_CNT] = v[V_PAN_PERIOD];
      v[V_PAN] = (uint8_t)(v[V_PAN] + v[V_PAN_DELTA]);
      emit_cc(vm, ch, 10, v[V_PAN]);
    }
  }
  vm->cur_channel++;
}

/* ---- resident driver routines (native) ---------------------------------- */

/* 0x1697: reset one MIDI channel's controllers. */
static void channel_reset(GsoundVm* vm, uint8_t ch) {
  vm->fade_enable = 0;
  emit_cc(vm, ch, 0x7b, 0);
  emit_cc(vm, ch, 0x79, 0);
  emit_cc(vm, ch, 7, 0x64);
  emit_cc(vm, ch, 10, 0x40);
  emit_cc(vm, ch, 91, 0);
  emit_cc(vm, ch, 93, 0);
  emit_cc(vm, ch, 101, 0);
  emit_cc(vm, ch, 100, 0);
  emit_cc(vm, ch, 6, 2);
  emit_cc(vm, ch, 38, 0);
}

/* 0x171f / 0x1786 / 0x17dd: silence voice blocks and clear note slots. */
static void clear_voices(GsoundVm* vm, bool ch78, bool others) {
  for (int ch = 1; ch <= 9; ++ch) {
    const bool is78 = (ch == 7 || ch == 8);
    if ((is78 && !ch78) || (!is78 && !others)) {
      continue;
    }
    uint8_t* v = vm->ds + k_voice_by_channel[ch];
    v[V_DUR] = 0;
    v[V_VENV_DELTA] = 0;
  }
  if (others) {
    memset(vm->ds + GS_SLOTS, 0xff, 0x40);
  }
}

/* 0x1710: hard stop everything. */
static void hard_stop(GsoundVm* vm) {
  ds16w(vm, GS_EA, 0);
  clear_voices(vm, true, true);
  for (int ch = 9; ch >= 0; --ch) {
    channel_reset(vm, (uint8_t)ch);
  }
}

/* 0x1901 helper: mark a live voice for fade-out and end it at its loop point. */
static void fade_voice(GsoundVm* vm, uint16_t vaddr) {
  uint8_t* v = vm->ds + vaddr;
  if (v[V_DUR] != 0) {
    v[V_FADE] = 0xff;
    wr16(v + W_SONG_START, GS_TERMINATOR);
  }
}

/* 0x180c */
static void begin_transition(GsoundVm* vm) {
  ds16w(vm, GS_EA, 0);
  vm->fade_enable = 1;
}

/* 0x18cf: new BGM — fade channels 1..6 and 9. */
static void soft_new_song(GsoundVm* vm) {
  ds16w(vm, GS_EA, 0);
  begin_transition(vm);
  static const int chans[] = {1, 2, 3, 4, 5, 6, 9};
  for (size_t i = 0; i < sizeof(chans) / sizeof(chans[0]); ++i) {
    fade_voice(vm, k_voice_by_channel[chans[i]]);
  }
}

/* 0x1912: fade channels 7 and 8 (event music). */
static void soft_stop_78(GsoundVm* vm) {
  begin_transition(vm);
  fade_voice(vm, k_voice_by_channel[7]);
  fade_voice(vm, k_voice_by_channel[8]);
}

/* 0x190f: fade everything. */
static void soft_stop_all(GsoundVm* vm) {
  soft_new_song(vm);
  soft_stop_78(vm);
}

/* 0x1640 + 0x1626 tail: start a stream on a voice block. */
static void start_voice(GsoundVm* vm, int ch, uint16_t stream) {
  uint8_t* v = vm->ds + k_voice_by_channel[ch];
  memset(v, 0, GS_VOICE_STRIDE);
  wr16(v + W_LOOP0, stream);
  wr16(v + W_POS, stream);
  wr16(v + W_FF_MARK, stream);
  wr16(v + W_FE_MARK, stream);
  wr16(v + W_SONG_START, stream);
  v[V_PAN] = 0x40;
  v[V_BEND] = 0x40;
  v[V_VOL] = 0x64;
  v[V_DUR] = 1;
  emit(vm, (uint8_t)(0xE0 | ch), 0, 0x40); /* 0x1481 */
}

static bool voice_free(const GsoundVm* vm, int ch) {
  return vm->ds[k_voice_by_channel[ch] + V_DUR] == 0;
}

static bool voice_fading(const GsoundVm* vm, int ch) {
  return vm->ds[k_voice_by_channel[ch] + V_FADE] == 0xff;
}

/* 0x14cd (channels 1..6) / 0x1540 (1..8) / 0x15c1 (7..8): first free, else first fading. */
static void alloc_voice(GsoundVm* vm, int lo, int hi, uint16_t stream) {
  for (int ch = lo; ch <= hi; ++ch) {
    if (voice_free(vm, ch)) {
      start_voice(vm, ch, stream);
      return;
    }
  }
  for (int ch = hi; ch >= lo; --ch) {
    if (voice_fading(vm, ch)) {
      start_voice(vm, ch, stream);
      return;
    }
  }
}

/* 0x11e0: find a live voice whose song start is `stream` (channels 1..8). */
static int find_voice_by_start(const GsoundVm* vm, uint16_t stream) {
  for (int ch = 1; ch <= 8; ++ch) {
    const uint8_t* v = vm->ds + k_voice_by_channel[ch];
    if (v[V_DUR] != 0 && rd16(v + W_SONG_START) == stream) {
      return ch;
    }
  }
  return 0;
}

/* 0x1819: per-tick fade of voices flagged by 0x1901. */
static void fade_tick(GsoundVm* vm) {
  if (!vm->fade_enable) {
    return;
  }
  vm->fade_count--;
  if (vm->fade_count > 0) {
    return;
  }
  vm->fade_count = (int8_t)vm->fade_enable;
  for (int ch = 1; ch <= 9; ++ch) {
    uint8_t* v = vm->ds + k_voice_by_channel[ch];
    if (v[V_DUR] == 0 || v[V_FADE] == 0) {
      v[V_FADE] = 0;
      continue;
    }
    if (v[V_VOL] == 0) {
      v[V_VENV_DELTA] = 0;
      wr16(v + W_POS, GS_TERMINATOR);
      v[V_FADE] = 0;
      continue;
    }
    v[V_VOL]--;
    if (v[V_VOL] != 0) {
      v[V_VOL]--;
    }
    emit_cc(vm, (uint8_t)ch, 7, v[V_VOL]);
  }
}

/* 0x2a2e: segment countdown → host callback in DS:EA. */
static void countdown_tick(GsoundVm* vm) {
  if (ds16(vm, GS_E8) == 0) {
    return;
  }
  const uint16_t e6 = (uint16_t)(ds16(vm, GS_E6) - 1);
  ds16w(vm, GS_E6, e6);
  if (e6 != 0) {
    return;
  }
  ds16w(vm, GS_E6, ds16(vm, GS_E8));
  const uint16_t cb = ds16(vm, GS_EA);
  if (cb != 0) {
    ds16w(vm, GS_EA, 0);
    run_x86(vm, cb);
  }
}

/* ---- mini x86 for song handlers ---------------------------------------- */

typedef struct X86 {
  uint16_t ax, bx, cx, dx;
  bool zf, cf;
} X86;

static void set_cmp16(X86* r, uint16_t a, uint16_t b) {
  r->zf = (a == b);
  r->cf = (a < b);
}

/* Returns true when the call was serviced natively. */
static bool native_call(GsoundVm* vm, X86* r, uint16_t target) {
  switch (target) {
    case 0x118a: r->ax = prng(vm); return true;
    case 0x11e0: {
      const int ch = find_voice_by_start(vm, r->cx);
      r->bx = ch ? k_voice_by_channel[ch] : 0;
      r->zf = (r->bx == 0);
      r->cf = false;
      return true;
    }
    case 0x145b: {
      uint8_t al = 0;
      for (int ch = 1; ch <= 9; ++ch) {
        al |= vm->ds[k_voice_by_channel[ch] + V_DUR];
      }
      r->ax = al;
      r->zf = (al == 0);
      r->cf = false;
      return true;
    }
    case 0x14cd: alloc_voice(vm, 1, 6, r->cx); return true;
    case 0x1540: alloc_voice(vm, 1, 8, r->cx); return true;
    case 0x15c1: alloc_voice(vm, 7, 8, r->cx); return true;
    case 0x15e0: start_voice(vm, 9, r->cx); return true;
    case 0x15e8: start_voice(vm, 8, r->cx); return true;
    case 0x15f0: start_voice(vm, 7, r->cx); return true;
    case 0x15f8: start_voice(vm, 6, r->cx); return true;
    case 0x1600: start_voice(vm, 5, r->cx); return true;
    case 0x1608: start_voice(vm, 4, r->cx); return true;
    case 0x1610: start_voice(vm, 3, r->cx); return true;
    case 0x1618: start_voice(vm, 2, r->cx); return true;
    case 0x1620: start_voice(vm, 1, r->cx); return true;
    case 0x166a: /* reset channels 6,7 after clearing 7/8 blocks */
      clear_voices(vm, true, false);
      channel_reset(vm, 6);
      channel_reset(vm, 7);
      return true;
    case 0x167a:
      clear_voices(vm, false, true);
      for (int ch = 5; ch >= 0; --ch) channel_reset(vm, (uint8_t)ch);
      channel_reset(vm, 9);
      return true;
    case 0x168d:
      for (int ch = 9; ch >= 0; --ch) channel_reset(vm, (uint8_t)ch);
      return true;
    case 0x1710: hard_stop(vm); return true;
    case 0x171f: clear_voices(vm, true, true); return true;
    case 0x180c: begin_transition(vm); return true;
    case 0x18cf: soft_new_song(vm); return true;
    case 0x190f: soft_stop_all(vm); return true;
    case 0x1912: soft_stop_78(vm); return true;
    case 0x2a55: ds16w(vm, GS_EA, 0); return true;
    case 0x30c4: /* digital sample AX via far call 0000:27b4 (FUN_1000_27b4) */
      if (vm->sfx) {
        vm->sfx(vm->sfx_user, (int)r->ax);
      }
      return true;
    case 0x13a7:
    case 0x1403:
      vm->unsupported++;
      return true;
    default:
      return false;
  }
}

static void run_x86_regs(GsoundVm* vm, X86* r, uint16_t ip) {
  const uint8_t* c = vm->img;
  uint8_t* ds = vm->ds;
  if (vm->depth > 16) {
    vm->unsupported++;
    return;
  }
  vm->depth++;
  for (int guard = 0; guard < 4096; ++guard) {
    if ((size_t)ip + 6 > vm->img_size) {
      vm->unsupported++;
      break;
    }
    if (native_call(vm, r, ip)) { /* jumped into a resident routine */
      break;
    }
    const uint8_t op = c[ip];
    const uint8_t m = c[ip + 1];
    const uint16_t w1 = rd16(c + ip + 1);
    const uint16_t w2 = rd16(c + ip + 2);
    if (op == 0xC3) {
      break;
    }
    switch (op) {
      case 0xB8: r->ax = w1; ip += 3; continue;
      case 0xB9: r->cx = w1; ip += 3; continue;
      case 0xBA: r->dx = w1; ip += 3; continue;
      case 0xBB: r->bx = w1; ip += 3; continue;
      case 0xE8: {
        const uint16_t target = (uint16_t)(ip + 3 + w1);
        ip += 3;
        if (!native_call(vm, r, target)) {
          run_x86_regs(vm, r, target);
        }
        continue;
      }
      case 0xE9: ip = (uint16_t)(ip + 3 + w1); continue;
      case 0xEB: ip = (uint16_t)(ip + 2 + (int8_t)m); continue;
      case 0x74: ip = r->zf ? (uint16_t)(ip + 2 + (int8_t)m) : (uint16_t)(ip + 2); continue;
      case 0x75: ip = !r->zf ? (uint16_t)(ip + 2 + (int8_t)m) : (uint16_t)(ip + 2); continue;
      case 0x77: ip = (!r->cf && !r->zf) ? (uint16_t)(ip + 2 + (int8_t)m) : (uint16_t)(ip + 2); continue;
      case 0x50: case 0x58: ip += 1; continue; /* push/pop ax around 30c4 */
      case 0x9A: ip += 5; continue; /* far call into the host (0000:27b4/2855) */
      case 0x33: if (m == 0xC0) { r->ax = 0; ip += 2; continue; } break;
      case 0xA3: ds16w(vm, w1, r->ax); ip += 3; continue;
      case 0x86: if (m == 0xDF) { r->bx = (uint16_t)((r->bx >> 8) | (r->bx << 8)); ip += 2; continue; } break;
      case 0x88:
        if (m == 0x1E) { ds[w2] = (uint8_t)r->bx; ip += 4; continue; }
        if (m == 0x3E) { ds[w2] = (uint8_t)(r->bx >> 8); ip += 4; continue; }
        if (m == 0x47) { ds[(uint16_t)(r->bx + (int8_t)c[ip + 2])] = (uint8_t)r->ax; ip += 3; continue; }
        break;
      case 0x89: if (m == 0x1E) { ds16w(vm, w2, r->bx); ip += 4; continue; } break;
      case 0x8B: if (m == 0xCB) { r->cx = r->bx; ip += 2; continue; } break;
      case 0xC6: if (m == 0x06) { ds[w2] = c[ip + 4]; ip += 5; continue; } break;
      case 0xC7: if (m == 0x06) { ds16w(vm, w2, rd16(c + ip + 4)); ip += 6; continue; } break;
      case 0x83:
        if (m == 0x3E) { set_cmp16(r, ds16(vm, w2), (uint16_t)(int16_t)(int8_t)c[ip + 4]); ip += 5; continue; }
        if (m == 0xE0) { r->ax &= (uint16_t)(int16_t)(int8_t)c[ip + 2]; r->zf = (r->ax == 0); r->cf = false; ip += 3; continue; }
        if (m == 0xC0) { r->ax = (uint16_t)(r->ax + (int8_t)c[ip + 2]); ip += 3; continue; }
        break;
      case 0x39:
        if (m == 0x4F) { set_cmp16(r, ds16(vm, (uint16_t)(r->bx + (int8_t)c[ip + 2])), r->cx); ip += 3; continue; }
        break;
      default: break;
    }
    vm->unsupported++;
    break;
  }
  vm->depth--;
}

static void run_x86(GsoundVm* vm, uint16_t ip) {
  X86 r;
  memset(&r, 0, sizeof(r));
  run_x86_regs(vm, &r, ip);
}

/* ---- public API --------------------------------------------------------- */

GsoundVm* gsound_vm_create(const uint8_t* img, size_t img_size) {
  if (!img || img_size <= GS_DS_BASE + 0x8000u || img_size - GS_DS_BASE > GS_DS_SIZE) {
    return NULL;
  }
  GsoundVm* vm = (GsoundVm*)calloc(1, sizeof(*vm));
  if (!vm) {
    return NULL;
  }
  vm->img = img;
  vm->img_size = img_size;
  memcpy(vm->ds, img + GS_DS_BASE, img_size - GS_DS_BASE);
  memset(vm->ds + GS_SLOTS, 0xff, 0x40);
  vm->cur_channel = 1;
  return vm;
}

void gsound_vm_destroy(GsoundVm* vm) {
  free(vm);
}

void gsound_vm_set_midi(GsoundVm* vm, GsoundMidiFn fn, void* user) {
  if (vm) {
    vm->midi = fn;
    vm->midi_user = user;
  }
}

void gsound_vm_set_sfx(GsoundVm* vm, GsoundSfxFn fn, void* user) {
  if (vm) {
    vm->sfx = fn;
    vm->sfx_user = user;
  }
}

#define GS_SFX_TABLE 0x1C7B

int gsound_vm_sfx_table(const GsoundVm* vm, size_t coldig_size, uint32_t* offs, uint32_t* lens, int max) {
  if (!vm) {
    return 0;
  }
  int n = 0;
  for (;;) {
    const size_t e = GS_SFX_TABLE + (size_t)n * 8u;
    if (e + 8 > vm->img_size) {
      break;
    }
    const uint32_t off = (uint32_t)rd16(vm->img + e) | ((uint32_t)rd16(vm->img + e + 2) << 16);
    const uint32_t len = (uint32_t)rd16(vm->img + e + 4) | ((uint32_t)rd16(vm->img + e + 6) << 16);
    if (len == 0 || (uint64_t)off + len > coldig_size) {
      break;
    }
    if (n < max) {
      offs[n] = off;
      lens[n] = len;
    }
    n++;
  }
  return n;
}

int gsound_vm_sfx_rate(int sfx_index) {
  return sfx_index < 5 ? 11025 : 19050;
}

void gsound_vm_reset_channels(GsoundVm* vm) {
  if (vm) {
    for (int ch = 9; ch >= 0; --ch) {
      channel_reset(vm, (uint8_t)ch);
    }
  }
}

static uint16_t handler_for(const GsoundVm* vm, int id) {
  uint32_t table;
  int idx;
  uint16_t bound;
  if (id < 0) {
    return 0;
  }
  if (id < 0x20) {
    table = GS_TABLE_SYS;
    idx = id;
    bound = ds16(vm, GS_BOUND_SYS);
  } else if (id < 0x40) {
    table = GS_TABLE_BGM;
    idx = id - 0x20;
    bound = ds16(vm, GS_BOUND_BGM);
  } else if (id < 0x8020) {
    table = GS_TABLE_EVT;
    idx = id - 0x40;
    bound = ds16(vm, GS_BOUND_EVT);
  } else {
    table = GS_TABLE_STING;
    idx = id - 0x8020;
    bound = ds16(vm, GS_BOUND_STING);
  }
  if (id > bound) {
    return 0;
  }
  const uint32_t e = table + (uint32_t)idx * 2u;
  if (e + 2 > vm->img_size) {
    return 0;
  }
  return rd16(vm->img + e);
}

bool gsound_vm_has_song(const GsoundVm* vm, int id) {
  return vm && handler_for(vm, id) != 0;
}

bool gsound_vm_play(GsoundVm* vm, int id) {
  if (!vm) {
    return false;
  }
  const uint16_t h = handler_for(vm, id);
  if (h == 0) {
    return false;
  }
  X86 r;
  memset(&r, 0, sizeof(r));
  if (!native_call(vm, &r, h)) {
    run_x86_regs(vm, &r, h);
  }
  return true;
}

void gsound_vm_tick(GsoundVm* vm) {
  if (!vm) {
    return;
  }
  vm->ticks++;
  prng(vm); /* 0x119b steps the generator every IRQ */
  /* 0x1098: channel order 1,2,3,4,5,6,7,8,9 (block 0x80BE last). */
  vm->cur_channel = 1;
  for (int ch = 1; ch <= 9; ++ch) {
    voice_tick(vm, k_voice_by_channel[ch]);
  }
  countdown_tick(vm);
  fade_tick(vm);
}

bool gsound_vm_active(const GsoundVm* vm) {
  if (!vm) {
    return false;
  }
  for (int ch = 1; ch <= 9; ++ch) {
    if (vm->ds[k_voice_by_channel[ch] + V_DUR] != 0) {
      return true;
    }
  }
  return false;
}

uint32_t gsound_vm_tick_count(const GsoundVm* vm) {
  return vm ? vm->ticks : 0;
}

uint32_t gsound_vm_loop_tick(const GsoundVm* vm) {
  return vm ? vm->loop_tick : 0;
}

int gsound_vm_unsupported_count(const GsoundVm* vm) {
  return vm ? vm->unsupported : 0;
}
