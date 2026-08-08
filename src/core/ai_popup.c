#include "core/ai_popup.h"

#include <stdio.h>
#include <string.h>

#include "core/map_menu.h"

void ai_popup_init(AiPopupState* st) {
  if (!st) {
    return;
  }
  memset(st, 0, sizeof(*st));
}

void ai_popup_clear(AiPopupState* st) {
  ai_popup_init(st);
}

bool ai_popup_enqueue(AiPopupState* st, const AiPopupRequest* req) {
  if (!st || !req || st->queue_count >= AI_POPUP_QUEUE_MAX) {
    return false;
  }
  st->queue[st->queue_count++] = *req;
  return true;
}

static void ai_popup_fill_base(
  AiPopupRequest* req,
  AiPopupKind kind,
  AiPopupTag tag,
  int nation_a,
  int nation_b,
  int payload,
  const char* title,
  const char* body
) {
  memset(req, 0, sizeof(*req));
  req->kind = kind;
  req->tag = tag;
  req->nation_a = nation_a;
  req->nation_b = nation_b;
  req->payload = payload;
  if (title) {
    snprintf(req->title, sizeof(req->title), "%s", title);
  }
  if (body) {
    snprintf(req->body, sizeof(req->body), "%s", body);
  }
}

bool ai_popup_enqueue_ok(
  AiPopupState* st,
  AiPopupTag tag,
  const char* title,
  const char* body
) {
  return ai_popup_enqueue_ok_ctx(st, tag, -1, -1, 0, title, body);
}

bool ai_popup_enqueue_ok_ctx(
  AiPopupState* st,
  AiPopupTag tag,
  int nation_a,
  int nation_b,
  int payload,
  const char* title,
  const char* body
) {
  AiPopupRequest req;
  ai_popup_fill_base(&req, AI_POPUP_KIND_OK, tag, nation_a, nation_b, payload, title, body);
  req.choice_count = 1;
  snprintf(req.choices[0], sizeof(req.choices[0]), "OK");
  req.choice_ids[0] = 0;
  return ai_popup_enqueue(st, &req);
}

bool ai_popup_enqueue_choice(
  AiPopupState* st,
  AiPopupTag tag,
  const char* title,
  const char* body,
  const char* const* choice_labels,
  const int* choice_ids,
  int choice_count
) {
  return ai_popup_enqueue_choice_ctx(
    st, tag, -1, -1, 0, title, body, choice_labels, choice_ids, choice_count
  );
}

bool ai_popup_enqueue_choice_ctx(
  AiPopupState* st,
  AiPopupTag tag,
  int nation_a,
  int nation_b,
  int payload,
  const char* title,
  const char* body,
  const char* const* choice_labels,
  const int* choice_ids,
  int choice_count
) {
  AiPopupRequest req;
  if (!choice_labels || choice_count <= 0 || choice_count > AI_POPUP_CHOICE_MAX) {
    return false;
  }
  ai_popup_fill_base(
    &req, AI_POPUP_KIND_CHOICE, tag, nation_a, nation_b, payload, title, body
  );
  req.choice_count = choice_count;
  for (int i = 0; i < choice_count; ++i) {
    snprintf(
      req.choices[i],
      sizeof(req.choices[i]),
      "%s",
      choice_labels[i] ? choice_labels[i] : ""
    );
    req.choice_ids[i] = choice_ids ? choice_ids[i] : i;
  }
  return ai_popup_enqueue(st, &req);
}

bool ai_popup_queue_pending(const AiPopupState* st) {
  return st && st->queue_count > 0;
}

bool ai_popup_busy(const AiPopupState* st) {
  return st && (st->open || st->queue_count > 0 || st->has_result);
}

bool ai_popup_try_present_next(AiPopupState* st) {
  if (!st || st->open || st->has_result || st->queue_count <= 0) {
    return false;
  }
  st->current = st->queue[0];
  for (int i = 1; i < st->queue_count; ++i) {
    st->queue[i - 1] = st->queue[i];
  }
  st->queue_count--;
  st->open = true;
  st->selection = 0;
  st->has_result = false;
  st->result_cancelled = false;
  return true;
}

static void ai_popup_finish(AiPopupState* st, bool cancelled, int choice_id) {
  if (!st) {
    return;
  }
  st->has_result = true;
  st->result_cancelled = cancelled;
  st->result_choice_id = choice_id;
  st->result_tag = st->current.tag;
  st->result_nation_a = st->current.nation_a;
  st->result_nation_b = st->current.nation_b;
  st->result_payload = st->current.payload;
  st->open = false;
}

static int ai_popup_option_at_y(const AiPopupState* st, int mouse_y) {
  if (!st || st->line_h <= 0) {
    return -1;
  }
  const int rel = mouse_y - st->list_y0;
  if (rel < 0) {
    return -1;
  }
  const int idx = rel / st->line_h;
  if (idx < 0 || idx >= st->current.choice_count) {
    return -1;
  }
  return idx;
}

bool ai_popup_handle_input(AiPopupState* st, const ColonizeInputState* input) {
  if (!st || !st->open || !input) {
    return false;
  }

  if (input->last_key == COLONIZE_KEY_ESCAPE) {
    ai_popup_finish(st, true, -1);
    return true;
  }
  if (input->last_key == COLONIZE_KEY_UP && st->selection > 0) {
    st->selection--;
    return true;
  }
  if (input->last_key == COLONIZE_KEY_DOWN &&
      st->selection + 1 < st->current.choice_count) {
    st->selection++;
    return true;
  }
  if (input->last_key == COLONIZE_KEY_ENTER || input->last_key == COLONIZE_KEY_SPACE) {
    if (st->selection >= 0 && st->selection < st->current.choice_count) {
      ai_popup_finish(st, false, st->current.choice_ids[st->selection]);
    }
    return true;
  }

  if (input->mouse_left_clicked) {
    const int mx = input->mouse_x;
    const int my = input->mouse_y;
    if (mx < st->dialog_x || my < st->dialog_y || mx >= st->dialog_x + st->dialog_w ||
        my >= st->dialog_y + st->dialog_h) {
      ai_popup_finish(st, true, -1);
      return true;
    }
    const int idx = ai_popup_option_at_y(st, my);
    if (idx >= 0) {
      st->selection = idx;
      ai_popup_finish(st, false, st->current.choice_ids[idx]);
    }
    return true;
  }

  if (input->mouse_right_clicked) {
    ai_popup_finish(st, true, -1);
    return true;
  }

  return true;
}

void ai_popup_render(
  AiPopupState* st,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t select_color,
  ColonizeFramebuffer8* framebuffer
) {
  if (!st || !st->open || !framebuffer || !framebuffer->pixels) {
    return;
  }

  const AiPopupRequest* req = &st->current;
  const int line_h = font ? (font->max_height + 2) : 8;
  const int pad_x = 6;
  const int pad_y = 4;
  const int title_h = req->title[0] ? line_h + 2 : 0;
  /* Body: up to 4 wrapped visual lines by crude char budget. */
  const int body_chars = (int)strlen(req->body);
  const int body_lines = body_chars <= 0 ? 0 : (body_chars + 39) / 40;
  const int body_h = body_lines > 0 ? body_lines * line_h + 2 : 0;
  const int options_h = req->choice_count * line_h;
  int dialog_h = POPUP_FRAME_INSET * 2 + pad_y + title_h + body_h + options_h + pad_y;
  if (dialog_h < 48) {
    dialog_h = 48;
  }
  if (dialog_h > framebuffer->height - 8) {
    dialog_h = framebuffer->height - 8;
  }

  int dialog_w = 200;
  if (dialog_w > framebuffer->width - 8) {
    dialog_w = framebuffer->width - 8;
  }
  int dialog_x = (framebuffer->width - dialog_w) / 2;
  int dialog_y = (framebuffer->height - dialog_h) / 2;
  if (dialog_y < MAP_MENU_BAR_H + 2) {
    dialog_y = MAP_MENU_BAR_H + 2;
  }

  ColonizePopupColors local_colors;
  if (!colors) {
    popup_colors_from_ui(&local_colors);
    colors = &local_colors;
  }

  int inner_x = 0;
  int inner_y = 0;
  int inner_w = 0;
  int inner_h = 0;
  popup_draw(
    framebuffer,
    dialog_x,
    dialog_y,
    dialog_w,
    dialog_h,
    wood_tile,
    colors,
    &inner_x,
    &inner_y,
    &inner_w,
    &inner_h
  );

  st->dialog_x = dialog_x;
  st->dialog_y = dialog_y;
  st->dialog_w = dialog_w;
  st->dialog_h = dialog_h;
  st->line_h = line_h;

  int text_y = inner_y + pad_y;
  if (req->title[0] && font) {
    font_draw_text(font, framebuffer, inner_x + pad_x, text_y, req->title, text_color);
    text_y += title_h;
  }

  if (req->body[0] && font) {
    /* Crude wrap at ~40 chars. */
    char line[48];
    const char* p = req->body;
    while (*p && text_y + line_h <= inner_y + inner_h - options_h - pad_y) {
      size_t n = 0;
      while (p[n] && n < 40 && p[n] != '\n') {
        n++;
      }
      if (n >= sizeof(line)) {
        n = sizeof(line) - 1;
      }
      memcpy(line, p, n);
      line[n] = '\0';
      font_draw_text(font, framebuffer, inner_x + pad_x, text_y, line, text_color);
      text_y += line_h;
      p += n;
      if (*p == '\n') {
        p++;
      }
    }
    text_y += 2;
  }

  st->list_y0 = text_y;
  for (int i = 0; i < req->choice_count; ++i) {
    const uint8_t col = (i == st->selection) ? select_color : text_color;
    if (font) {
      font_draw_text(
        font, framebuffer, inner_x + pad_x, text_y, req->choices[i], col
      );
    }
    text_y += line_h;
  }
}

void ai_popup_consume_result(AiPopupState* st) {
  if (!st) {
    return;
  }
  st->has_result = false;
  st->result_cancelled = false;
  st->result_choice_id = -1;
}
