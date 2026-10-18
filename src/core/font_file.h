#ifndef COLONIZE_FONT_FILE_H
#define COLONIZE_FONT_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ColonizeFont {
  uint8_t max_height;
  uint8_t max_width;
  uint8_t char_widths[128];
  uint16_t char_offsets[128];
  uint8_t* section_data;
  size_t section_size;
} ColonizeFont;

bool font_file_load(const char* path, ColonizeFont* out_font, char* err, size_t err_size);
void font_file_free(ColonizeFont* font);

#endif
