#ifndef COLONIZE_GSOUND_VM_H
#define COLONIZE_GSOUND_VM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Literal emulation of the GSOUND.COL MicroProse General MIDI driver.
 *
 * The driver is a small bytecode sequencer driven by a ~60 Hz PIT IRQ
 * (divisor 0x4DBF). Nine voice blocks (0x28 bytes each at DS:0x8096) map to
 * MIDI channels 1..9 (block at DS:0x80BE is channel 9 = GM percussion).
 * Song handlers (jump tables at image 0x2A6E / 0x2AC4) are tiny x86 stubs
 * that allocate voices; a mini x86 interpreter runs them so per-song quirks
 * (random patches, warm restarts, segment callbacks via DS:E6/E8/EA) behave
 * exactly like DOS.
 *
 * Everything below works on a private copy of the driver's data segment, so
 * self-modifying stream opcodes (E7/EA/EB/EC, C4 code call-outs) are safe.
 */

typedef void (*GsoundMidiFn)(void* user, uint8_t status, uint8_t d1, uint8_t d2);

typedef struct GsoundVm GsoundVm;

/* img = MZ load image (header stripped). Returns NULL on bad image. */
GsoundVm* gsound_vm_create(const uint8_t* img, size_t img_size);
void gsound_vm_destroy(GsoundVm* vm);

void gsound_vm_set_midi(GsoundVm* vm, GsoundMidiFn fn, void* user);

/* Driver install: reset MIDI channels 0..9 like the resident init does. */
void gsound_vm_reset_channels(GsoundVm* vm);

/* Dispatch a sound id through FUN_1000_19bc (0 = hard stop, 1 = fade out). */
bool gsound_vm_play(GsoundVm* vm, int id);
bool gsound_vm_has_song(const GsoundVm* vm, int id);

/* One PIT tick (~59.95 Hz). */
void gsound_vm_tick(GsoundVm* vm);

/* True while any voice block has a nonzero duration (FUN 0x145b). */
bool gsound_vm_active(const GsoundVm* vm);

/* Ticks elapsed since create/reset; monotonic. */
uint32_t gsound_vm_tick_count(const GsoundVm* vm);

/*
 * Tick at which a voice first executed the whole-song loop opcode (FD) with a
 * song start set (i.e. the song restarted from the top); 0 if never.
 */
uint32_t gsound_vm_loop_tick(const GsoundVm* vm);

/* Diagnostics: count of unimplemented handler opcodes / native stubs hit. */
int gsound_vm_unsupported_count(const GsoundVm* vm);

#define GSOUND_TICK_HZ (1193182.0 / 19903.0)

#endif
