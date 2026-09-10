/* _POSIX_C_SOURCE too: _DEFAULT_SOURCE only exists since glibc 2.19, and the
 * release builds against glibc 2.17, where localtime_r/readlink need the
 * POSIX guard. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include "platform/diagnostics.h"

#include "core/strutil.h"

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

static FILE* g_log = NULL;
static char g_log_path[1024];
static char g_exe_dir[1024];
static bool g_info_enabled = false;
static char g_context[64];

/* The startup banner (log path, exe dir, argv[0], cwd, HOME, XDG_DATA_HOME) and
 * settings.c's two "Settings file" lines are emitted before main() can know
 * whether debug_logs is on: diag_init must run first so settings_init can call
 * diag_exe_dir(). So INFO lines emitted before the first diag_set_info_enabled()
 * are held here and either flushed (debug_logs on) or discarded (off) at that
 * call. Timestamps are captured when the line is buffered, so a flushed line
 * carries its real time even though it lands after any WARN/ERROR written in
 * between. */
#define DIAG_PENDING_MAX 16
#define DIAG_LINE_MAX 2176
static char g_pending[DIAG_PENDING_MAX][DIAG_LINE_MAX];
static int g_pending_count = 0;
static int g_pending_dropped = 0;
static bool g_info_settled = false;

static void format_line(char* out, size_t out_size, const char* level, const char* message) {
  time_t now = time(NULL);
  struct tm tm_now;
#ifdef _WIN32
  localtime_s(&tm_now, &now);
#else
  localtime_r(&now, &tm_now);
#endif
  char stamp[32];
  strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tm_now);
  if (g_context[0]) {
    snprintf(out, out_size, "[%s] [%s] [%s] %s\n", stamp, level, g_context, message);
  } else {
    snprintf(out, out_size, "[%s] [%s] %s\n", stamp, level, message);
  }
}

static void write_line(const char* level, const char* message) {
  if (!g_log) {
    return;
  }

  char line[DIAG_LINE_MAX];
  format_line(line, sizeof(line), level, message);
  fputs(line, g_log);
  fflush(g_log);
}

static void buffer_info_line(const char* message) {
  if (g_pending_count >= DIAG_PENDING_MAX) {
    ++g_pending_dropped;
    return;
  }
  format_line(g_pending[g_pending_count], DIAG_LINE_MAX, "INFO", message);
  ++g_pending_count;
}

static void flush_pending_info(void) {
  int i;
  if (g_log) {
    for (i = 0; i < g_pending_count; ++i) {
      fputs(g_pending[i], g_log);
    }
    if (g_pending_dropped > 0) {
      fprintf(g_log, "[INFO] %d earlier startup line(s) dropped (buffer full).\n", g_pending_dropped);
    }
    fflush(g_log);
  }
  g_pending_count = 0;
  g_pending_dropped = 0;
}

static void diag_vlog(const char* level, const char* fmt, va_list args) {
  char message[2048];
  vsnprintf(message, sizeof(message), fmt, args);
  write_line(level, message);
}

static bool resolve_exe_dir(char* out_dir, size_t out_dir_size) {
  if (!out_dir || out_dir_size == 0) {
    return false;
  }
  out_dir[0] = '\0';

#if defined(_WIN32)
  char exe_path[4096];
  DWORD len = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
  if (len > 0 && len < sizeof(exe_path)) {
    char* slash = strrchr(exe_path, '\\');
    if (!slash) {
      slash = strrchr(exe_path, '/');
    }
    if (slash) {
      size_t dir_len = (size_t)(slash - exe_path);
      if (dir_len + 1 < out_dir_size) {
        memcpy(out_dir, exe_path, dir_len);
        out_dir[dir_len] = '\0';
        return true;
      }
    }
  }
#elif defined(__linux__)
  char exe_path[4096];
  ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
  if (len > 0) {
    exe_path[len] = '\0';
    const char* slash = strrchr(exe_path, '/');
    if (slash) {
      size_t dir_len = (size_t)(slash - exe_path);
      if (dir_len + 1 < out_dir_size) {
        memcpy(out_dir, exe_path, dir_len);
        out_dir[dir_len] = '\0';
        return true;
      }
    }
  }
#endif

  if (getcwd(out_dir, out_dir_size) != NULL) {
    return true;
  }
  snprintf(out_dir, out_dir_size, ".");
  return false;
}

bool diag_init(int argc, char** argv) {
  (void)argc;
  resolve_exe_dir(g_exe_dir, sizeof(g_exe_dir));
  str_path_join(g_log_path, sizeof(g_log_path), g_exe_dir, "colonize-linux.log");

  g_log = fopen(g_log_path, "w");
  if (!g_log) {
    str_copy_trunc(g_log_path, sizeof(g_log_path), "./colonize-linux.log");
    g_log = fopen(g_log_path, "w");
  }
  if (!g_log) {
    return false;
  }

  diag_info("=== Colonization Linux diagnostics ===");
  diag_info("Log file: %s", g_log_path);
  diag_info("Executable directory: %s", g_exe_dir);

  if (argv && argv[0]) {
    diag_info("argv[0]: %s", argv[0]);
  }

  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    diag_info("Working directory: %s", cwd);
  } else {
    diag_warn("Working directory unavailable: %s", strerror(errno));
  }

  const char* home = getenv("HOME");
  const char* xdg_data = getenv("XDG_DATA_HOME");
  diag_info("HOME=%s", home ? home : "(unset)");
  diag_info("XDG_DATA_HOME=%s", xdg_data ? xdg_data : "(unset)");
  return true;
}

void diag_shutdown(void) {
  if (g_log) {
    diag_info("Diagnostics shutdown.");
    fclose(g_log);
    g_log = NULL;
  }
}

const char* diag_log_path(void) {
  return g_log_path;
}

const char* diag_exe_dir(void) {
  return g_exe_dir;
}

void diag_set_info_enabled(bool enabled) {
  g_info_enabled = enabled;
  if (!g_info_settled) {
    /* First call decides the fate of everything buffered during startup. */
    g_info_settled = true;
    if (enabled) {
      flush_pending_info();
    } else {
      g_pending_count = 0;
      g_pending_dropped = 0;
    }
  }
}

bool diag_info_enabled(void) {
  return g_info_enabled;
}

void diag_set_context(const char* context) {
  if (!context || !context[0]) {
    g_context[0] = '\0';
    return;
  }
  str_copy_trunc(g_context, sizeof(g_context), context);
}

const char* diag_context(void) {
  return g_context;
}

void diag_info(const char* fmt, ...) {
  va_list args;
  if (!g_info_enabled) {
    char message[2048];
    if (g_info_settled) {
      return;
    }
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    buffer_info_line(message);
    return;
  }
  va_start(args, fmt);
  diag_vlog("INFO", fmt, args);
  va_end(args);
}

void diag_warn(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  diag_vlog("WARN", fmt, args);
  va_end(args);
}

void diag_error(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  diag_vlog("ERROR", fmt, args);
  va_end(args);
}
