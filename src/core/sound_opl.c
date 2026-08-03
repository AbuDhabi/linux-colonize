#include "core/sound_opl.h"

#include <string.h>

#include "opl3.h"

#define SOUND_OPL_CHANNELS 9
#define SOUND_OPL_BANK_OFF 0x5376
#define SOUND_OPL_FNUM_OFF 0x2d2
#define SOUND_OPL_VEL_OFF 0x250
#define SOUND_OPL_VEL_LEN 48

/* Melodic op register offsets for channels 0..8 (same as DS:0x300 via 0x2ee map). */
static const uint8_t k_op_offset[9] = {0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12};

/*
 * ASOUND 22-byte operator record (RE from apply @ img 0x21A8):
 *  [0] AR  [1] DR  [2] SL  [3] RR
 *  [4] EG-type flag  [5] KSR flag
 *  [6] TL level (0..63, higher = louder in driver math)
 *  [7] KSL (0..3)
 *  [8] wave  [9] MULT
 *  [a] feedback  [b] AM  [c] VIB  [d] connection (1=additive)
 */
typedef struct AsoundOp {
  uint8_t raw[SOUND_OPL_OP_BYTES];
} AsoundOp;

typedef struct AsoundInstrument {
  AsoundOp mod;
  AsoundOp car;
} AsoundInstrument;

typedef struct SoundOplChannel {
  int program;
  int volume; /* F1 0..127 */
  int note;
  bool active;
  uint8_t b0;
  uint8_t mod_ksl;
  uint8_t car_ksl;
  uint8_t conn; /* 0=FM, 1=AM */
  uint8_t mod_lvl; /* instrument [6] */
  uint8_t car_lvl;
} SoundOplChannel;

static struct {
  bool ok;
  bool bank_ok;
  opl3_chip chip;
  SoundOplChannel ch[SOUND_OPL_CHANNELS];
  AsoundInstrument bank[SOUND_OPL_PROG_MAX];
  uint16_t fnum[12];
  uint8_t vel_curve[SOUND_OPL_VEL_LEN];
  int master_vol; /* 0..255 from C1; default 0x0a-ish full-ish */
} g_opl;

static void sound_opl_write(uint16_t reg, uint8_t val) {
  /* Immediate writes — buffered path delayed register updates and could miss short notes. */
  OPL3_WriteReg(&g_opl.chip, reg, val);
}

static uint8_t sound_opl_pack_20(const AsoundOp* op) {
  uint8_t v = (uint8_t)(op->raw[9] & 0x0f);
  if (op->raw[5]) {
    v = (uint8_t)(v | 0x10);
  }
  if (op->raw[4]) {
    v = (uint8_t)(v | 0x20);
  }
  if (op->raw[12]) {
    v = (uint8_t)(v | 0x40);
  }
  if (op->raw[11]) {
    v = (uint8_t)(v | 0x80);
  }
  return v;
}

static uint8_t sound_opl_pack_60(const AsoundOp* op) {
  return (uint8_t)(((op->raw[0] & 0x0f) << 4) | (op->raw[1] & 0x0f));
}

static uint8_t sound_opl_pack_80(const AsoundOp* op) {
  return (uint8_t)(((op->raw[2] & 0x0f) << 4) | (op->raw[3] & 0x0f));
}

static uint8_t sound_opl_pack_c0(const AsoundOp* op) {
  /* feedback in bits 1..3, connection in bit 0 (apply @ 0x21F5). */
  uint8_t v = (uint8_t)((op->raw[10] & 0x07) << 1);
  if (op->raw[13]) {
    v = (uint8_t)(v | 0x01);
  }
  return v;
}

static void sound_opl_write_op_regs(uint8_t op_off, const AsoundOp* op) {
  sound_opl_write(0x20 + op_off, sound_opl_pack_20(op));
  sound_opl_write(0x60 + op_off, sound_opl_pack_60(op));
  sound_opl_write(0x80 + op_off, sound_opl_pack_80(op));
  sound_opl_write(0xe0 + op_off, (uint8_t)(op->raw[8] & 0x03));
}

static int sound_opl_clamp(int v, int lo, int hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

/*
 * Driver TL math (AdLib path @ 0x1F01): 
 *   si' = clamp(si - (0x3f - inst[6]), 0, 0x3f);  tl = 0x3f - si'
 * Incoming si is a 0..63 loudness. Instrument[6] is also a loudness-ish level.
 */
static uint8_t sound_opl_calc_tl(uint8_t inst_lvl, int loudness_0_63, uint8_t ksl) {
  int si = sound_opl_clamp(loudness_0_63, 0, 63);
  const int inst_atten = 0x3f - (int)(inst_lvl & 0x3f);
  si = sound_opl_clamp(si - inst_atten, 0, 63);
  const int tl = 0x3f - si;
  return (uint8_t)(((ksl & 3) << 6) | (tl & 0x3f));
}

static int sound_opl_note_loudness(int channel, int velocity) {
  /*
   * Map F4 velocity + F1 volume to 0..63 loudness for the driver TL formula.
   * ASOUND C1 (e.g. 0x0a at track start) is NOT a 0..255 PCM master — treating
   * it as such crushed output to silence. Ignore master_vol here.
   */
  int vol = g_opl.ch[channel].volume;
  if (vol <= 0) {
    vol = 100;
  }
  if (vol > 127) {
    vol = 127;
  }
  if (velocity < 0) {
    velocity = 0;
  }
  if (velocity > 127) {
    velocity = 127;
  }
  const int loud = (velocity * vol * 63) / (127 * 127);
  return sound_opl_clamp(loud, 0, 63);
}

static const AsoundInstrument* sound_opl_instr(int program) {
  if (!g_opl.bank_ok) {
    return NULL;
  }
  if (program < 0) {
    program = 0;
  }
  if (program >= SOUND_OPL_PROG_MAX) {
    program %= SOUND_OPL_PROG_MAX;
  }
  return &g_opl.bank[program];
}

static void sound_opl_apply_program(int channel, int program) {
  const AsoundInstrument* inst = sound_opl_instr(program);
  if (!inst || channel < 0 || channel >= SOUND_OPL_CHANNELS) {
    return;
  }
  const uint8_t op_m = k_op_offset[channel];
  const uint8_t op_c = (uint8_t)(op_m + 3);

  sound_opl_write(0xc0 + (uint16_t)channel, sound_opl_pack_c0(&inst->mod));
  sound_opl_write_op_regs(op_m, &inst->mod);
  sound_opl_write_op_regs(op_c, &inst->car);

  g_opl.ch[channel].program = program;
  g_opl.ch[channel].mod_ksl = (uint8_t)(inst->mod.raw[7] & 3);
  g_opl.ch[channel].car_ksl = (uint8_t)(inst->car.raw[7] & 3);
  g_opl.ch[channel].conn = inst->mod.raw[13] ? 1 : 0;
  g_opl.ch[channel].mod_lvl = (uint8_t)(inst->mod.raw[6] & 0x3f);
  g_opl.ch[channel].car_lvl = (uint8_t)(inst->car.raw[6] & 0x3f);

  /* Program-change path writes KSL|0x3F (silent) until note-on sets TL. */
  sound_opl_write(0x40 + op_m, (uint8_t)((g_opl.ch[channel].mod_ksl << 6) | 0x3f));
  sound_opl_write(0x40 + op_c, (uint8_t)((g_opl.ch[channel].car_ksl << 6) | 0x3f));
}

static void sound_opl_apply_tl(int channel, int velocity) {
  if (channel < 0 || channel >= SOUND_OPL_CHANNELS) {
    return;
  }
  const uint8_t op_m = k_op_offset[channel];
  const uint8_t op_c = (uint8_t)(op_m + 3);
  const int loud = sound_opl_note_loudness(channel, velocity);
  const uint8_t car_tl = sound_opl_calc_tl(g_opl.ch[channel].car_lvl, loud, g_opl.ch[channel].car_ksl);
  const uint8_t mod_tl = sound_opl_calc_tl(g_opl.ch[channel].mod_lvl, loud, g_opl.ch[channel].mod_ksl);
  sound_opl_write(0x40 + op_c, car_tl);
  sound_opl_write(0x40 + op_m, mod_tl);
}

bool sound_opl_load_tables(const uint8_t* ds, size_t ds_size) {
  g_opl.bank_ok = false;
  if (!ds) {
    return false;
  }
  const size_t bank_bytes = (size_t)SOUND_OPL_PROG_MAX * SOUND_OPL_INST_BYTES;
  if (ds_size < SOUND_OPL_BANK_OFF + bank_bytes ||
      ds_size < SOUND_OPL_FNUM_OFF + 24 ||
      ds_size < SOUND_OPL_VEL_OFF + SOUND_OPL_VEL_LEN) {
    return false;
  }
  for (int p = 0; p < SOUND_OPL_PROG_MAX; ++p) {
    const uint8_t* base = ds + SOUND_OPL_BANK_OFF + (size_t)p * SOUND_OPL_INST_BYTES;
    memcpy(g_opl.bank[p].mod.raw, base, SOUND_OPL_OP_BYTES);
    memcpy(g_opl.bank[p].car.raw, base + SOUND_OPL_OP_BYTES, SOUND_OPL_OP_BYTES);
  }
  for (int i = 0; i < 12; ++i) {
    g_opl.fnum[i] = (uint16_t)(ds[SOUND_OPL_FNUM_OFF + i * 2] |
                               ((uint16_t)ds[SOUND_OPL_FNUM_OFF + i * 2 + 1] << 8));
  }
  memcpy(g_opl.vel_curve, ds + SOUND_OPL_VEL_OFF, SOUND_OPL_VEL_LEN);
  g_opl.bank_ok = true;
  return true;
}

bool sound_opl_bank_loaded(void) {
  return g_opl.ok && g_opl.bank_ok;
}

bool sound_opl_init(int sample_rate) {
  const bool keep_bank = g_opl.bank_ok;
  AsoundInstrument bank_copy[SOUND_OPL_PROG_MAX];
  uint16_t fnum_copy[12];
  uint8_t vel_copy[SOUND_OPL_VEL_LEN];
  if (keep_bank) {
    memcpy(bank_copy, g_opl.bank, sizeof(bank_copy));
    memcpy(fnum_copy, g_opl.fnum, sizeof(fnum_copy));
    memcpy(vel_copy, g_opl.vel_curve, sizeof(vel_copy));
  }
  memset(&g_opl, 0, sizeof(g_opl));
  if (keep_bank) {
    memcpy(g_opl.bank, bank_copy, sizeof(bank_copy));
    memcpy(g_opl.fnum, fnum_copy, sizeof(fnum_copy));
    memcpy(g_opl.vel_curve, vel_copy, sizeof(vel_copy));
    g_opl.bank_ok = true;
  }
  if (sample_rate <= 0) {
    sample_rate = 44100;
  }
  OPL3_Reset(&g_opl.chip, (uint32_t)sample_rate);
  sound_opl_write(0x01, 0x20); /* waveform select */
  sound_opl_write(0xBD, 0x00);
  g_opl.master_vol = 0x69; /* typical F1-ish default until C1 seen */
  for (int i = 0; i < SOUND_OPL_CHANNELS; ++i) {
    g_opl.ch[i].program = 0;
    g_opl.ch[i].volume = 100;
    g_opl.ch[i].note = -1;
    if (g_opl.bank_ok) {
      sound_opl_apply_program(i, 0);
    }
  }
  g_opl.ok = true;
  return true;
}

void sound_opl_shutdown(void) {
  if (!g_opl.ok) {
    return;
  }
  sound_opl_all_notes_off();
  memset(&g_opl, 0, sizeof(g_opl));
}

bool sound_opl_ok(void) {
  return g_opl.ok;
}

void sound_opl_reset(void) {
  if (!g_opl.ok) {
    return;
  }
  OPL3_Reset(&g_opl.chip, 44100);
  sound_opl_write(0x01, 0x20);
  sound_opl_write(0xBD, 0x00);
  for (int i = 0; i < SOUND_OPL_CHANNELS; ++i) {
    g_opl.ch[i].active = false;
    g_opl.ch[i].note = -1;
    if (g_opl.bank_ok) {
      sound_opl_apply_program(i, g_opl.ch[i].program);
    }
  }
}

void sound_opl_all_notes_off(void) {
  if (!g_opl.ok) {
    return;
  }
  for (int ch = 0; ch < SOUND_OPL_CHANNELS; ++ch) {
    if (g_opl.ch[ch].active) {
      sound_opl_write(0xb0 + (uint16_t)ch, (uint8_t)(g_opl.ch[ch].b0 & 0x1f));
      g_opl.ch[ch].active = false;
      g_opl.ch[ch].note = -1;
    }
  }
}

void sound_opl_program(int channel, int program) {
  if (!g_opl.ok || channel < 0 || channel >= SOUND_OPL_CHANNELS) {
    return;
  }
  sound_opl_apply_program(channel, program);
}

void sound_opl_volume(int channel, int midi_vol) {
  if (!g_opl.ok || channel < 0 || channel >= SOUND_OPL_CHANNELS) {
    return;
  }
  g_opl.ch[channel].volume = midi_vol;
}

void sound_opl_master_volume(int vol) {
  /* Retained for C1 stream events; does not gate note loudness (see note_loudness). */
  if (!g_opl.ok) {
    return;
  }
  g_opl.master_vol = vol;
}

void sound_opl_note_on(int channel, int midi_note, int velocity) {
  if (!g_opl.ok || channel < 0 || channel >= SOUND_OPL_CHANNELS) {
    return;
  }
  if (midi_note < 0) {
    midi_note = 0;
  }
  if (midi_note > 95) {
    midi_note = 12 + (midi_note % 84);
  }
  if (velocity <= 0) {
    sound_opl_note_off(channel, midi_note);
    return;
  }

  sound_opl_apply_tl(channel, velocity);

  /* Driver (@ 0x1FFE): block = note/12, fnum = table[note%12] (no -1). */
  int block = midi_note / 12;
  if (block > 7) {
    block = 7;
  }
  uint16_t fnum = g_opl.bank_ok ? g_opl.fnum[midi_note % 12] : (uint16_t)(0x200 + (midi_note % 12) * 0x20);
  /* Key-on: A0 then B0 with keyon|block<<2|fnum_hi (@ 0x202D). */
  const uint8_t a0 = (uint8_t)(fnum & 0xff);
  const uint8_t b0 = (uint8_t)(0x20 | ((block & 7) << 2) | ((fnum >> 8) & 3));
  sound_opl_write(0xa0 + (uint16_t)channel, a0);
  sound_opl_write(0xb0 + (uint16_t)channel, b0);
  g_opl.ch[channel].b0 = b0;
  g_opl.ch[channel].note = midi_note;
  g_opl.ch[channel].active = true;
}

void sound_opl_note_off(int channel, int midi_note) {
  if (!g_opl.ok || channel < 0 || channel >= SOUND_OPL_CHANNELS) {
    return;
  }
  if (!g_opl.ch[channel].active) {
    return;
  }
  if (midi_note >= 0 && g_opl.ch[channel].note >= 0 && g_opl.ch[channel].note != midi_note) {
    return;
  }
  sound_opl_write(0xb0 + (uint16_t)channel, (uint8_t)(g_opl.ch[channel].b0 & 0x1f));
  g_opl.ch[channel].active = false;
  g_opl.ch[channel].note = -1;
}

bool sound_opl_program_regs(
  int program,
  uint8_t* out_m_char,
  uint8_t* out_c_char,
  uint8_t* out_c0,
  uint8_t* out_m_tl,
  uint8_t* out_c_tl
) {
  const AsoundInstrument* inst = sound_opl_instr(program);
  if (!inst) {
    return false;
  }
  if (out_m_char) {
    *out_m_char = sound_opl_pack_20(&inst->mod);
  }
  if (out_c_char) {
    *out_c_char = sound_opl_pack_20(&inst->car);
  }
  if (out_c0) {
    *out_c0 = sound_opl_pack_c0(&inst->mod);
  }
  if (out_m_tl) {
    *out_m_tl = (uint8_t)(((inst->mod.raw[7] & 3) << 6) | (inst->mod.raw[6] & 0x3f));
  }
  if (out_c_tl) {
    *out_c_tl = (uint8_t)(((inst->car.raw[7] & 3) << 6) | (inst->car.raw[6] & 0x3f));
  }
  return true;
}

static int16_t sound_opl_amplify(int32_t s) {
  /* Nuked melodic output is quieter than FluidSynth; boost for audible A/B. */
  s *= 5;
  if (s > 32767) {
    return 32767;
  }
  if (s < -32768) {
    return -32768;
  }
  return (int16_t)s;
}

void sound_opl_render_s16(int16_t* dst, int frames, int channels) {
  if (!dst || frames <= 0) {
    return;
  }
  if (!g_opl.ok) {
    const int stride = channels > 0 ? channels : 1;
    memset(dst, 0, (size_t)frames * (size_t)stride * sizeof(int16_t));
    return;
  }
  if (channels <= 1) {
    for (int i = 0; i < frames; ++i) {
      int16_t stereo[2];
      OPL3_GenerateResampled(&g_opl.chip, stereo);
      dst[i] = sound_opl_amplify(((int32_t)stereo[0] + (int32_t)stereo[1]) / 2);
    }
    return;
  }
  for (int i = 0; i < frames; ++i) {
    int16_t stereo[2];
    OPL3_GenerateResampled(&g_opl.chip, stereo);
    dst[i * channels + 0] = sound_opl_amplify(stereo[0]);
    dst[i * channels + 1] = sound_opl_amplify(stereo[1]);
    for (int c = 2; c < channels; ++c) {
      dst[i * channels + c] = 0;
    }
  }
}
