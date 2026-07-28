#ifndef COLONIZE_ASSETS_H
#define COLONIZE_ASSETS_H

#include <stdbool.h>
#include <stddef.h>

bool assets_validate_required_files(const char* data_dir, char* err_buf, size_t err_buf_size);

#endif
