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

static void write_line(const char* level, const char* message) {
  if (!g_log) {
    return;
  }

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
    fprintf(g_log, "[%s] [%s] [%s] %s\n", stamp, level, g_context, message);
  } else {
    fprintf(g_log, "[%s] [%s] %s\n", stamp, level, message);
  }
  fflush(g_log);
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
  if (!g_info_enabled) {
    return;
  }
  va_list args;
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
