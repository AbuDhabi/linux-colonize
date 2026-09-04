#ifndef COLONIZE_DIAGNOSTICS_H
#define COLONIZE_DIAGNOSTICS_H

#include <stdbool.h>
#include <stddef.h>

bool diag_init(int argc, char** argv);
void diag_shutdown(void);
const char* diag_log_path(void);
const char* diag_exe_dir(void);

/* INFO lines are off until debug.logs is on; WARN/ERROR always write. */
void diag_set_info_enabled(bool enabled);
bool diag_info_enabled(void);

/*
 * Screen context stamped into every log line ("map", "colony:Jamestown",
 * "europe", "report:Colony", ...) so a popup / order / production line says
 * where it happened. NULL or "" clears it. Copy is kept; caller keeps
 * ownership.
 */
void diag_set_context(const char* context);
const char* diag_context(void);

void diag_info(const char* fmt, ...);
void diag_warn(const char* fmt, ...);
void diag_error(const char* fmt, ...);

#endif
