#ifndef COLONIZE_SOUND_OPL_H
#define COLONIZE_SOUND_OPL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Nuked-OPL3 wrapper for ASOUND.COL. Instruments / fnum / velocity curve are
 * loaded from the driver DS (0x5376 / 0x2d2 / 0x250) via sound_opl_load_tables().
 */

#define SOUND_OPL_PROG_MAX 56
#define SOUND_OPL_OP_BYTES 22
#define SOUND_OPL_INST_BYTES 44 /* mod + car */

bool sound_opl_init(int sample_rate);
void sound_opl_shutdown(void);
bool sound_opl_ok(void);
bool sound_opl_bank_loaded(void);

/* Copy tables from ASOUND DS image (offsets relative to DS). */
bool sound_opl_load_tables(const uint8_t* ds, size_t ds_size);

void sound_opl_reset(void);
void sound_opl_all_notes_off(void);

void sound_opl_program(int channel, int program);
void sound_opl_volume(int channel, int midi_vol); /* F1: 0..127 */
void sound_opl_master_volume(int vol);            /* C1: 0..255 */
void sound_opl_note_on(int channel, int midi_note, int velocity);
void sound_opl_note_off(int channel, int midi_note);

/* Test helper: packed OPL regs after applying program (mod then car). */
bool sound_opl_program_regs(
  int program,
  uint8_t* out_m_char,
  uint8_t* out_c_char,
  uint8_t* out_c0,
  uint8_t* out_m_tl,
  uint8_t* out_c_tl
);

void sound_opl_render_s16(int16_t* dst, int frames, int channels);

#endif
