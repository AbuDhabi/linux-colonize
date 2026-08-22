#include "json_min.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char* p;
  char* err;
  size_t err_size;
  bool failed;
} Parser;

static void perr(Parser* ps, const char* msg) {
  if (ps->failed) {
    return;
  }
  ps->failed = true;
  if (ps->err && ps->err_size > 0) {
    snprintf(ps->err, ps->err_size, "json parse error near \"%.24s\": %s", ps->p, msg);
  }
}

static void skip_ws(Parser* ps) {
  while (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\n' || *ps->p == '\r') {
    ps->p++;
  }
}

static JsonValue* jv_new(JsonType t) {
  JsonValue* v = calloc(1, sizeof(JsonValue));
  v->type = t;
  return v;
}

void json_free(JsonValue* v) {
  if (!v) {
    return;
  }
  switch (v->type) {
    case JV_STR:
      free(v->str);
      break;
    case JV_ARR:
      for (size_t i = 0; i < v->arr.count; ++i) {
        json_free(v->arr.items[i]);
      }
      free(v->arr.items);
      break;
    case JV_OBJ:
      for (size_t i = 0; i < v->obj.count; ++i) {
        free(v->obj.keys[i]);
        json_free(v->obj.vals[i]);
      }
      free(v->obj.keys);
      free(v->obj.vals);
      break;
    default:
      break;
  }
  free(v);
}

static JsonValue* parse_value(Parser* ps);

static char* parse_raw_string(Parser* ps) {
  if (*ps->p != '"') {
    perr(ps, "expected string");
    return NULL;
  }
  ps->p++;
  size_t cap = 32, len = 0;
  char* buf = malloc(cap);
  while (*ps->p && *ps->p != '"') {
    unsigned char c = (unsigned char)*ps->p;
    unsigned int cp = 0;
    if (c == '\\') {
      ps->p++;
      switch (*ps->p) {
        case '"': cp = '"'; ps->p++; break;
        case '\\': cp = '\\'; ps->p++; break;
        case '/': cp = '/'; ps->p++; break;
        case 'n': cp = '\n'; ps->p++; break;
        case 't': cp = '\t'; ps->p++; break;
        case 'r': cp = '\r'; ps->p++; break;
        case 'b': cp = '\b'; ps->p++; break;
        case 'f': cp = '\f'; ps->p++; break;
        case 'u': {
          ps->p++;
          unsigned int v = 0;
          for (int i = 0; i < 4; ++i) {
            char h = *ps->p;
            if (!isxdigit((unsigned char)h)) {
              perr(ps, "bad \\u escape");
              free(buf);
              return NULL;
            }
            v = v * 16 + (unsigned int)(isdigit((unsigned char)h) ? h - '0' : (tolower(h) - 'a' + 10));
            ps->p++;
          }
          cp = v; /* BMP only; good enough for our controlled output */
          break;
        }
        default:
          perr(ps, "bad escape");
          free(buf);
          return NULL;
      }
    } else {
      cp = c;
      ps->p++;
    }
    /* Re-encode as UTF-8 (cp may be >127 from \u, or a raw byte already). */
    unsigned char enc[4];
    size_t enc_len;
    if (cp < 0x80) {
      enc[0] = (unsigned char)cp;
      enc_len = 1;
    } else if (cp < 0x800) {
      enc[0] = (unsigned char)(0xC0 | (cp >> 6));
      enc[1] = (unsigned char)(0x80 | (cp & 0x3F));
      enc_len = 2;
    } else {
      enc[0] = (unsigned char)(0xE0 | (cp >> 12));
      enc[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
      enc[2] = (unsigned char)(0x80 | (cp & 0x3F));
      enc_len = 3;
    }
    if (len + enc_len + 1 > cap) {
      cap *= 2;
      buf = realloc(buf, cap);
    }
    memcpy(buf + len, enc, enc_len);
    len += enc_len;
  }
  if (*ps->p != '"') {
    perr(ps, "unterminated string");
    free(buf);
    return NULL;
  }
  ps->p++;
  buf[len] = '\0';
  return buf;
}

static JsonValue* parse_string(Parser* ps) {
  char* s = parse_raw_string(ps);
  if (!s) {
    return NULL;
  }
  JsonValue* v = jv_new(JV_STR);
  v->str = s;
  return v;
}

static JsonValue* parse_number(Parser* ps) {
  const char* start = ps->p;
  if (*ps->p == '-') {
    ps->p++;
  }
  while (isdigit((unsigned char)*ps->p)) {
    ps->p++;
  }
  if (*ps->p == '.') {
    ps->p++;
    while (isdigit((unsigned char)*ps->p)) {
      ps->p++;
    }
  }
  if (*ps->p == 'e' || *ps->p == 'E') {
    ps->p++;
    if (*ps->p == '+' || *ps->p == '-') {
      ps->p++;
    }
    while (isdigit((unsigned char)*ps->p)) {
      ps->p++;
    }
  }
  if (ps->p == start) {
    perr(ps, "expected number");
    return NULL;
  }
  JsonValue* v = jv_new(JV_NUM);
  v->num = strtod(start, NULL);
  return v;
}

static JsonValue* parse_array(Parser* ps) {
  ps->p++; /* [ */
  JsonValue* v = jv_new(JV_ARR);
  size_t cap = 8;
  v->arr.items = malloc(cap * sizeof(JsonValue*));
  skip_ws(ps);
  if (*ps->p == ']') {
    ps->p++;
    return v;
  }
  for (;;) {
    skip_ws(ps);
    JsonValue* item = parse_value(ps);
    if (ps->failed) {
      json_free(item);
      json_free(v);
      return NULL;
    }
    if (v->arr.count == cap) {
      cap *= 2;
      v->arr.items = realloc(v->arr.items, cap * sizeof(JsonValue*));
    }
    v->arr.items[v->arr.count++] = item;
    skip_ws(ps);
    if (*ps->p == ',') {
      ps->p++;
      continue;
    }
    if (*ps->p == ']') {
      ps->p++;
      break;
    }
    perr(ps, "expected , or ] in array");
    json_free(v);
    return NULL;
  }
  return v;
}

static JsonValue* parse_object(Parser* ps) {
  ps->p++; /* { */
  JsonValue* v = jv_new(JV_OBJ);
  size_t cap = 8;
  v->obj.keys = malloc(cap * sizeof(char*));
  v->obj.vals = malloc(cap * sizeof(JsonValue*));
  skip_ws(ps);
  if (*ps->p == '}') {
    ps->p++;
    return v;
  }
  for (;;) {
    skip_ws(ps);
    char* key = parse_raw_string(ps);
    if (ps->failed) {
      free(key);
      json_free(v);
      return NULL;
    }
    skip_ws(ps);
    if (*ps->p != ':') {
      perr(ps, "expected :");
      free(key);
      json_free(v);
      return NULL;
    }
    ps->p++;
    skip_ws(ps);
    JsonValue* val = parse_value(ps);
    if (ps->failed) {
      free(key);
      json_free(val);
      json_free(v);
      return NULL;
    }
    if (v->obj.count == cap) {
      cap *= 2;
      v->obj.keys = realloc(v->obj.keys, cap * sizeof(char*));
      v->obj.vals = realloc(v->obj.vals, cap * sizeof(JsonValue*));
    }
    v->obj.keys[v->obj.count] = key;
    v->obj.vals[v->obj.count] = val;
    v->obj.count++;
    skip_ws(ps);
    if (*ps->p == ',') {
      ps->p++;
      continue;
    }
    if (*ps->p == '}') {
      ps->p++;
      break;
    }
    perr(ps, "expected , or } in object");
    json_free(v);
    return NULL;
  }
  return v;
}

static JsonValue* parse_value(Parser* ps) {
  skip_ws(ps);
  switch (*ps->p) {
    case '{': return parse_object(ps);
    case '[': return parse_array(ps);
    case '"': return parse_string(ps);
    case 't':
      if (strncmp(ps->p, "true", 4) == 0) {
        ps->p += 4;
        JsonValue* v = jv_new(JV_BOOL);
        v->b = true;
        return v;
      }
      perr(ps, "bad literal");
      return NULL;
    case 'f':
      if (strncmp(ps->p, "false", 5) == 0) {
        ps->p += 5;
        JsonValue* v = jv_new(JV_BOOL);
        v->b = false;
        return v;
      }
      perr(ps, "bad literal");
      return NULL;
    case 'n':
      if (strncmp(ps->p, "null", 4) == 0) {
        ps->p += 4;
        return jv_new(JV_NULL);
      }
      perr(ps, "bad literal");
      return NULL;
    default:
      if (*ps->p == '-' || isdigit((unsigned char)*ps->p)) {
        return parse_number(ps);
      }
      perr(ps, "unexpected character");
      return NULL;
  }
}

JsonValue* json_parse(const char* text, char* err, size_t err_size) {
  Parser ps = {.p = text, .err = err, .err_size = err_size, .failed = false};
  if (err && err_size > 0) {
    err[0] = '\0';
  }
  skip_ws(&ps);
  JsonValue* v = parse_value(&ps);
  if (ps.failed) {
    json_free(v);
    return NULL;
  }
  skip_ws(&ps);
  if (*ps.p != '\0') {
    perr(&ps, "trailing data after top-level value");
    json_free(v);
    return NULL;
  }
  return v;
}

JsonValue* json_obj_get(const JsonValue* obj, const char* key) {
  if (!obj || obj->type != JV_OBJ) {
    return NULL;
  }
  for (size_t i = 0; i < obj->obj.count; ++i) {
    if (strcmp(obj->obj.keys[i], key) == 0) {
      return obj->obj.vals[i];
    }
  }
  return NULL;
}

size_t json_arr_len(const JsonValue* arr) {
  if (!arr || arr->type != JV_ARR) {
    return 0;
  }
  return arr->arr.count;
}

JsonValue* json_arr_at(const JsonValue* arr, size_t idx) {
  if (!arr || arr->type != JV_ARR || idx >= arr->arr.count) {
    return NULL;
  }
  return arr->arr.items[idx];
}

bool json_get_i64(const JsonValue* obj, const char* key, int64_t* out) {
  JsonValue* v = json_obj_get(obj, key);
  if (!v || v->type != JV_NUM) {
    return false;
  }
  *out = (int64_t)v->num;
  return true;
}

bool json_get_u64(const JsonValue* obj, const char* key, uint64_t* out) {
  JsonValue* v = json_obj_get(obj, key);
  if (!v || v->type != JV_NUM) {
    return false;
  }
  *out = (uint64_t)v->num;
  return true;
}

bool json_get_bool(const JsonValue* obj, const char* key, bool* out) {
  JsonValue* v = json_obj_get(obj, key);
  if (!v || v->type != JV_BOOL) {
    return false;
  }
  *out = v->b;
  return true;
}

const char* json_get_str(const JsonValue* obj, const char* key) {
  JsonValue* v = json_obj_get(obj, key);
  if (!v || v->type != JV_STR) {
    return NULL;
  }
  return v->str;
}

static int hex_nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool json_get_hex(const JsonValue* obj, const char* key, uint8_t* out, size_t out_len) {
  const char* s = json_get_str(obj, key);
  if (!s) {
    return false;
  }
  if (strlen(s) != out_len * 2) {
    return false;
  }
  for (size_t i = 0; i < out_len; ++i) {
    int hi = hex_nibble(s[i * 2]);
    int lo = hex_nibble(s[i * 2 + 1]);
    if (hi < 0 || lo < 0) {
      return false;
    }
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}

bool json_get_cstr_fixed(const JsonValue* obj, const char* key, char* buf, size_t buf_len) {
  const char* s = json_get_str(obj, key);
  if (!s) {
    return false;
  }
  memset(buf, 0, buf_len);
  strncpy(buf, s, buf_len - 1);
  return true;
}

void json_write_escaped_string(FILE* f, const char* s, size_t max_len) {
  fputc('"', f);
  size_t n = 0;
  for (const unsigned char* p = (const unsigned char*)s; *p && n < max_len; ++p, ++n) {
    switch (*p) {
      case '"': fputs("\\\"", f); break;
      case '\\': fputs("\\\\", f); break;
      case '\n': fputs("\\n", f); break;
      case '\t': fputs("\\t", f); break;
      case '\r': fputs("\\r", f); break;
      default:
        if (*p < 0x20) {
          fprintf(f, "\\u%04x", *p);
        } else {
          fputc((int)*p, f);
        }
    }
  }
  fputc('"', f);
}

void json_write_hex(FILE* f, const uint8_t* data, size_t len) {
  fputc('"', f);
  for (size_t i = 0; i < len; ++i) {
    fprintf(f, "%02x", data[i]);
  }
  fputc('"', f);
}
