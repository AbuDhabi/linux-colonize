#include "core/popup_msg.h"

#include <stdio.h>
#include <string.h>

#include "core/strutil.h"

bool popup_msg_is_directive(const char* line) {
  return line && line[0] == '@';
}

static bool popup_msg_is_choice_word(const char* line) {
  if (!line || line[0] == '\0') {
    return false;
  }
  return strcmp(line, "Yes") == 0 || strcmp(line, "No") == 0 ||
         strcmp(line, "OK") == 0 || strcmp(line, "Never mind.") == 0 ||
         strncmp(line, "Unload the", 10) == 0;
}

size_t popup_msg_section_body(
  const ColonizeMsgSection* section,
  char* out,
  size_t out_size,
  bool stop_before_choices
) {
  if (!out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  if (!section) {
    return 0;
  }
  size_t used = 0;
  int saw_prose = 0;
  for (int i = 0; i < section->line_count; ++i) {
    const char* line = section->lines[i];
    if (!line || line[0] == '\0' || popup_msg_is_directive(line)) {
      continue;
    }
    if (stop_before_choices && saw_prose && popup_msg_is_choice_word(line)) {
      break;
    }
    /* Skip lone "Name:" / "Amount:" / "Colony:" prompt labels for body. */
    if (strcmp(line, "Name:") == 0 || strcmp(line, "Amount:") == 0 ||
        strcmp(line, "Colony:") == 0) {
      continue;
    }
    if (used > 0 && used + 1 < out_size) {
      out[used++] = ' ';
    }
    const size_t n = strlen(line);
    if (used + n >= out_size) {
      size_t copy = out_size - 1 - used;
      memcpy(out + used, line, copy);
      used += copy;
      out[used] = '\0';
      return used;
    }
    memcpy(out + used, line, n);
    used += n;
    out[used] = '\0';
    saw_prose = 1;
  }
  return used;
}

static void popup_msg_append(char* dst, size_t dst_size, size_t* used, const char* add) {
  if (!dst || !used || dst_size == 0 || !add) {
    return;
  }
  while (*add && *used + 1 < dst_size) {
    dst[(*used)++] = *add++;
  }
  dst[*used] = '\0';
}

void popup_msg_apply_tokens(
  char* dst,
  size_t dst_size,
  const char* src,
  const PopupMsgTokens* tok
) {
  if (!dst || dst_size == 0) {
    return;
  }
  dst[0] = '\0';
  if (!src) {
    return;
  }
  size_t used = 0;
  for (size_t i = 0; src[i] != '\0' && used + 1 < dst_size;) {
    if (src[i] == '{' || src[i] == '}') {
      ++i;
      continue;
    }
    if (src[i] == '%' && tok) {
      if (strncmp(src + i, "%STRING0", 8) == 0) {
        popup_msg_append(dst, dst_size, &used, tok->string0 ? tok->string0 : "");
        i += 8;
        continue;
      }
      if (strncmp(src + i, "%STRING1", 8) == 0) {
        popup_msg_append(dst, dst_size, &used, tok->string1 ? tok->string1 : "");
        i += 8;
        continue;
      }
      if (strncmp(src + i, "%STRING2", 8) == 0) {
        popup_msg_append(dst, dst_size, &used, tok->string2 ? tok->string2 : "");
        i += 8;
        continue;
      }
      if (strncmp(src + i, "%STRING3", 8) == 0) {
        popup_msg_append(dst, dst_size, &used, tok->string3 ? tok->string3 : "");
        i += 8;
        continue;
      }
      if (strncmp(src + i, "%STRING4", 8) == 0) {
        popup_msg_append(dst, dst_size, &used, tok->string4 ? tok->string4 : "");
        i += 8;
        continue;
      }
      if (strncmp(src + i, "%COUNTRY", 8) == 0) {
        popup_msg_append(dst, dst_size, &used, tok->country ? tok->country : "");
        i += 8;
        continue;
      }
      if (strncmp(src + i, "%NUMBER0", 8) == 0) {
        char num[16];
        snprintf(num, sizeof(num), "%d", tok->has_number0 ? tok->number0 : 0);
        popup_msg_append(dst, dst_size, &used, num);
        i += 8;
        continue;
      }
      if (strncmp(src + i, "%NUMBER1", 8) == 0) {
        char num[16];
        snprintf(num, sizeof(num), "%d", tok->has_number1 ? tok->number1 : 0);
        popup_msg_append(dst, dst_size, &used, num);
        i += 8;
        continue;
      }
      if (strncmp(src + i, "%NUMBER2", 8) == 0) {
        char num[16];
        snprintf(num, sizeof(num), "%d", tok->has_number2 ? tok->number2 : 0);
        popup_msg_append(dst, dst_size, &used, num);
        i += 8;
        continue;
      }
    }
    dst[used++] = src[i++];
    dst[used] = '\0';
  }
}

void popup_msg_fill(
  const ColonizeMsgCatalog* catalog,
  const char* section_name,
  const PopupMsgTokens* tok,
  const char* fallback,
  char* out,
  size_t out_size
) {
  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  char raw[512];
  raw[0] = '\0';
  const ColonizeMsgSection* sec =
    (catalog && section_name) ? assets_msg_find(catalog, section_name) : NULL;
  if (sec) {
    popup_msg_section_body(sec, raw, sizeof(raw), true);
  }
  if (raw[0] == '\0' && fallback) {
    str_copy_trunc(raw, sizeof(raw), fallback);
  }
  popup_msg_apply_tokens(out, out_size, raw, tok);
  if (out[0] == '\0' && fallback) {
    str_copy_trunc(out, out_size, fallback);
  }
}

int popup_msg_choices(const ColonizeMsgSection* section, char out[][48], int max_choices) {
  if (!section || !out || max_choices <= 0) {
    return 0;
  }
  int count = 0;
  int saw_prose = 0;
  for (int i = 0; i < section->line_count; ++i) {
    const char* line = section->lines[i];
    if (!line || line[0] == '\0' || popup_msg_is_directive(line)) {
      continue;
    }
    if (strcmp(line, "Name:") == 0 || strcmp(line, "Amount:") == 0 ||
        strcmp(line, "Colony:") == 0) {
      continue;
    }
    if (popup_msg_is_choice_word(line) ||
        (saw_prose && (strcmp(line, "Yes") == 0 || strcmp(line, "No") == 0))) {
      if (count < max_choices) {
        str_copy_trunc(out[count], 48, line);
        count++;
      }
      continue;
    }
    saw_prose = 1;
  }
  /* If section ends with Yes/No only collected after prose: rescan for Yes/No. */
  if (count == 0) {
    for (int i = 0; i < section->line_count && count < max_choices; ++i) {
      const char* line = section->lines[i];
      if (line && (strcmp(line, "Yes") == 0 || strcmp(line, "No") == 0)) {
        str_copy_trunc(out[count], 48, line);
        count++;
      }
    }
  }
  return count;
}
