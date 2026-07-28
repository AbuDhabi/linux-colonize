#ifndef COLONIZE_DIAGNOSTICS_H
#define COLONIZE_DIAGNOSTICS_H

#include <stdbool.h>
#include <stddef.h>

bool diag_init(int argc, char** argv);
void diag_shutdown(void);
const char* diag_log_path(void);
const char* diag_exe_dir(void);

void diag_info(const char* fmt, ...);
void diag_warn(const char* fmt, ...);
void diag_error(const char* fmt, ...);

#endif
