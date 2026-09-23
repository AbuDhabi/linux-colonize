#include "core/ai_popup.h"
#include "core/ai_popup_render.h"

#include <stdio.h>
#include <string.h>

#include "core/map_menu.h"
#include "core/popup_msg.h"
#include "core/ui_button.h"
#include "core/ui_colors.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

static void ai_popup_log_present(const AiPopupRequest* req);
static const char* ai_popup_tag_name(AiPopupTag tag);

void ai_popup_init(AiPopupState* st) {
  if (!st) {
    return;
  }
  memset(st, 0, sizeof(*st));
}

void ai_popup_clear(AiPopupState* st) {
  ai_popup_init(st);
}

/*
 * bugs.md #427 — contact chains must not interleave.
 *
 * DOS never queues a contact dialog: FUN_5bfb_3180 walks the eight neighbours
 * and, for the one it picks, calls FUN_5bfb_022e (Indian) or FUN_5bfb_153e
 * (Euro Nation) INLINE (viceroy_unpacked.c:98757-98764, the
 * thunk_FUN_2a1f_05fc / thunk_FUN_2a1f_066c pair). Those calls block until the
 * whole exchange — meet, greeting, the tribute/peace follow-ups — is answered,
 * so a Tupi chain and a Spanish chain are strictly sequential.
 *
 * The port enqueues each dialog of a chain only when the previous one is
 * answered, so a second chain queued in the same pulse would drain in between.
 * Tagging every request of one exchange with the same key and presenting keyed
 * requests consecutively restores DOS's ordering.
 *
 * Every tag below carries nation_a = the Euro nation and nation_b = the other
 * party (0..3 Euro peer, 4..11 Indian nation) — the tags whose nation_a is a
 * unit id instead (WHACK, EURO_WAR, INDIAN_LAND) are deliberately
 * absent: they are single pre-attack confirms, not chains.
 */
static int ai_popup_chain_key(AiPopupTag tag, int nation_a, int nation_b) {
  switch (tag) {
    case AI_POPUP_TAG_CONTACT_MEET:
    case AI_POPUP_TAG_CONTACT_TEACH:
    case AI_POPUP_TAG_CONTACT_GIFT:
    case AI_POPUP_TAG_CONTACT_DEMAND:
    case AI_POPUP_TAG_CONTACT_RAID:
    case AI_POPUP_TAG_CONTACT_CONVERT:
    case AI_POPUP_TAG_CONTACT_REFUSE:
    case AI_POPUP_TAG_CONTACT_WELCOME:
    case AI_POPUP_TAG_CONTACT_INCITE:
    case AI_POPUP_TAG_CONTACT_BEGFOOD:
    case AI_POPUP_TAG_CONTACT_TRADE_OFFER:
    case AI_POPUP_TAG_CONTACT_BUYWHICH:
    case AI_POPUP_TAG_CONTACT_BUY0:
    case AI_POPUP_TAG_CONTACT_LEARNSTAY:
    case AI_POPUP_TAG_CONTACT_TRADE_PICK:
    case AI_POPUP_TAG_CONTACT_REPARATIONS:
    case AI_POPUP_TAG_DIPLO_TALK:
      break;
    default:
      return 0;
  }
  if (nation_a < 0 || nation_a > 3 || nation_b < 0 || nation_b > 11 || nation_a == nation_b) {
    return 0;
  }
  return 1 + nation_a * 12 + nation_b;
}

/* Enqueue (no-op if full or st NULL). Returns false if dropped. */
static bool ai_popup_enqueue(AiPopupState* st, const AiPopupRequest* req) {
  if (!st || !req) {
    return false;
  }
  if (st->queue_count >= AI_POPUP_QUEUE_MAX) {
    diag_warn(
      "POPUP dropped (queue full, %d) tag=%s body=\"%.80s\"",
      AI_POPUP_QUEUE_MAX, ai_popup_tag_name(req->tag), req->body
    );
    return false;
  }
  st->queue[st->queue_count] = *req;
  if (st->queue[st->queue_count].chain == 0) {
    st->queue[st->queue_count].chain =
      ai_popup_chain_key(req->tag, req->nation_a, req->nation_b);
  }
  st->queue_count++;
  if (diag_info_enabled()) {
    diag_info(
      "POPUP queued tag=%s kind=%s choices=%d queue=%d",
      ai_popup_tag_name(req->tag),
      req->kind == AI_POPUP_KIND_CHOICE ? "choice" : "message",
      req->choice_count,
      st->queue_count
    );
  }
  return true;
}

/*
 * Status line with an explicit DOS arm kind (FUN_1009_0244's first argument).
 * The plain enqueue is kind 1, the gold success ink DOS uses for a sale line;
 * kind 3 (red refusal) is DOS-real but has no Linux producer yet, so this
 * entry point is currently reached only through the kind-1 wrapper.
 */
static bool ai_popup_enqueue_bar_message_kind(
  AiPopupState* st, const char* text, int kind
) {
  if (!st || !text || !text[0]) {
    return false;
  }
  if (st->bar_msg_count >= AI_POPUP_BAR_MSG_MAX) {
    diag_warn("BAR MSG dropped (queue full, %d) \"%.80s\"", AI_POPUP_BAR_MSG_MAX, text);
    return false;
  }
  snprintf(st->bar_msg[st->bar_msg_count], AI_POPUP_BAR_MSG_LEN, "%s", text);
  st->bar_msg_kind[st->bar_msg_count] = (uint8_t)(kind < 0 ? 0 : kind);
  st->bar_msg_count++;
  return true;
}

bool ai_popup_enqueue_bar_message(AiPopupState* st, const char* text) {
  return ai_popup_enqueue_bar_message_kind(st, text, 1);
}

const char* ai_popup_bar_message(const AiPopupState* st) {
  if (!st || st->bar_msg_count <= 0) {
    return NULL;
  }
  return st->bar_msg[0];
}

/*
 * DOS FUN_1009_0004 (resident twin FUN_0000_0094, viceroy_unpacked.c:449-463):
 * the arm kind stored in DS:0x4c picks the strip ink. 1/2 -> 0x95 (@COLORS
 * hilite gold), 3 -> 0x0c (bright red), anything else 0x44 (@COLORS basic).
 * bugs.md #375: the port painted every line basic green.
 *
 * The `1 || 2` disjunct is DOS's own — transcribed, not a port invention — but
 * no producer in either game passes 2. Every literal arm kind that reaches
 * FUN_1009_0244 is 1 or 3: 1 for the sale/success lines
 * (FUN_281f_0dc2(1, 0x78, 0), raw 77421 and 77503) and 3 for the red refusal
 * (FUN_281f_0de0(0x479b, 0x16, 3), raw 77369, plus the overlay Europe screen's
 * thunk_FUN_1000_99a0(3, 0x78, 0)). Kind 3 is DOS-real and simply unported —
 * the Linux ring has one producer, ai_popup_enqueue_bar_message's kind 1.
 * Corrected 2026-09-10 (audit #20), which read the 1/2 pair as two producers.
 */
uint8_t ai_popup_bar_message_color(const AiPopupState* st) {
  if (!st || st->bar_msg_count <= 0) {
    return (uint8_t)COLONIZE_COL_BASIC;
  }
  switch (st->bar_msg_kind[0]) {
    case 1:
    case 2:
      return (uint8_t)COLONIZE_COL_HILITE;
    case 3:
      return 0x0cu;
    default:
      return (uint8_t)COLONIZE_COL_BASIC;
  }
}

bool ai_popup_bar_service(AiPopupState* st, uint32_t now_ms, bool dismiss) {
  if (!st || st->bar_msg_count <= 0) {
    if (st) {
      st->bar_msg_until_ms = 0;
    }
    return false;
  }
  if (st->bar_msg_until_ms == 0) {
    /*
     * First frame this line is on screen — arm its dwell (FUN_1009_0244).
     * DOS's wait is min(armed deadline, now + 30 ticks), and only the *next*
     * compose runs it, so a line with more behind it is cut to ~0.5 s while
     * the last of a run lives out the full 0x78-tick arm.
     */
    uint32_t hold = st->bar_msg_count > 1 ? AI_POPUP_BAR_MSG_MS : AI_POPUP_BAR_MSG_LAST_MS;
    /*
     * bugs.md #425: the sale lines run AI_POPUP_BAR_SALE_SPEEDUP times faster
     * than DOS at the user's explicit request. That is arm kind 1 — DOS's
     * gold "success" arm, and the only kind this ring produces (the Custom
     * House autosell run and the European Status sell lines both come in
     * through ai_popup_enqueue_bar_message). Any other arm kind keeps DOS's
     * dwell; the dead `|| kind == 2` disjunct went 2026-09-10 (audit #20 —
     * nothing in either game arms kind 2, see ai_popup_bar_message_color).
     */
    if (st->bar_msg_kind[0] == 1) {
      hold /= AI_POPUP_BAR_SALE_SPEEDUP;
    }
    st->bar_msg_until_ms = now_ms + hold;
    return true;
  }
  if (!dismiss && (int32_t)(now_ms - st->bar_msg_until_ms) < 0) {
    return true;
  }
  for (int i = 1; i < st->bar_msg_count; ++i) {
    memcpy(st->bar_msg[i - 1], st->bar_msg[i], AI_POPUP_BAR_MSG_LEN);
    st->bar_msg_kind[i - 1] = st->bar_msg_kind[i];
  }
  st->bar_msg_count--;
  st->bar_msg_until_ms = 0;
  return st->bar_msg_count > 0;
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
  req->portrait_tribe = -1;
  req->portrait_tier = -1;
  req->graphic_myr = -1;
  req->kind = kind;
  req->tag = tag;
  /* P11.3: the @width of whatever popup_msg_fill last resolved (0 = default). */
  req->width = popup_msg_take_pending_width();
  /* MSS decoration of that same section (DOS DS:0x1f5e latch; -1 = none). */
  req->graphic_mss = popup_msg_take_pending_graphic();
  /* @default=N pre-highlighted row (0 = first). */
  req->default_choice = popup_msg_take_pending_default();
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
  /* DOS info wood: no choice rows in GAME.TXT — click/key dismisses. */
  req.choice_count = 0;
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

/* LABELS.TXT @MISC 34/35 (DS:0x2dfe/0x2e00); literals until the file loads. */
static char g_colony_event_continue[AI_POPUP_CHOICE_LEN] = "";
static char g_colony_event_zoom[AI_POPUP_CHOICE_LEN] = "";

void ai_popup_set_colony_event_labels(const char* continue_label, const char* zoom_label) {
  if (continue_label && continue_label[0]) {
    snprintf(g_colony_event_continue, sizeof(g_colony_event_continue), "%s", continue_label);
  }
  if (zoom_label && zoom_label[0]) {
    snprintf(g_colony_event_zoom, sizeof(g_colony_event_zoom), "%s", zoom_label);
  }
}

bool ai_popup_enqueue_colony_event(AiPopupState* st, int colony_id, const char* body) {
  /* DOS FUN_364b_0000 appends the two rows with ids 1/2 (FUN_281f_0022). */
  const char* labels[2] = {g_colony_event_continue, g_colony_event_zoom};
  const int ids[2] = {1, 2};
  return ai_popup_enqueue_choice_ctx(
    st, AI_POPUP_TAG_COLONY_EVENT, -1, -1, colony_id, NULL, body, labels, ids, 2
  );
}

void ai_popup_colony_zoom_elect(AiPopupState* st, int colony_id) {
  if (!st || colony_id < 0 || colony_id >= 64) {
    return;
  }
  st->colony_zoom_elected |= (uint64_t)1u << colony_id;
}

static bool ai_popup_colony_has_pending(const AiPopupState* st, int colony_id) {
  if (st->open && st->current.tag == AI_POPUP_TAG_COLONY_EVENT &&
      st->current.payload == colony_id) {
    return true;
  }
  if (st->has_result && st->result_tag == AI_POPUP_TAG_COLONY_EVENT &&
      st->result_payload == colony_id) {
    return true;
  }
  for (int i = 0; i < st->queue_count; ++i) {
    if (st->queue[i].tag == AI_POPUP_TAG_COLONY_EVENT && st->queue[i].payload == colony_id) {
      return true;
    }
  }
  return false;
}

int ai_popup_take_colony_zoom(AiPopupState* st) {
  if (!st || st->colony_zoom_elected == 0) {
    return -1;
  }
  for (int c = 0; c < 64; ++c) {
    const uint64_t bit = (uint64_t)1u << c;
    if ((st->colony_zoom_elected & bit) != 0 && !ai_popup_colony_has_pending(st, c)) {
      st->colony_zoom_elected &= ~bit;
      return c;
    }
  }
  return -1;
}

void ai_popup_promote_tag_before(AiPopupState* st, AiPopupTag promote, AiPopupTag before) {
  if (!st || st->queue_count <= 1) {
    return;
  }
  int insert = -1;
  for (int i = 0; i < st->queue_count; ++i) {
    if (st->queue[i].tag == before) {
      insert = i;
      break;
    }
  }
  if (insert < 0) {
    return;
  }
  for (int i = insert + 1; i < st->queue_count; ++i) {
    if (st->queue[i].tag != promote) {
      continue;
    }
    const AiPopupRequest tmp = st->queue[i];
    for (int j = i; j > insert; --j) {
      st->queue[j] = st->queue[j - 1];
    }
    st->queue[insert] = tmp;
    insert++;
  }
}

bool ai_popup_move_tag_to_front(AiPopupState* st, AiPopupTag tag) {
  if (!st || st->queue_count <= 0) {
    return false;
  }
  int at = -1;
  for (int i = st->queue_count - 1; i >= 0; --i) {
    if (st->queue[i].tag == tag) {
      at = i;
      break;
    }
  }
  if (at < 0) {
    return false;
  }
  const AiPopupRequest req = st->queue[at];
  for (int j = at; j > 0; --j) {
    st->queue[j] = st->queue[j - 1];
  }
  st->queue[0] = req;
  return true;
}

bool ai_popup_present_now(AiPopupState* st, AiPopupTag tag) {
  if (!st || st->open || st->has_result || st->queue_count <= 0) {
    return false;
  }
  if (!ai_popup_move_tag_to_front(st, tag)) {
    return false;
  }
  const uint64_t saved_zoom = st->colony_zoom_elected;
  const int saved_chain = st->active_chain;
  st->colony_zoom_elected = 0; /* player-initiated: never held by a zoom batch */
  st->active_chain = 0;        /* …nor by another exchange's chain hold */
  const bool ok = ai_popup_try_present_next(st);
  st->colony_zoom_elected = saved_zoom;
  if (st->active_chain == 0) {
    st->active_chain = saved_chain;
  }
  return ok;
}

/* One line per tag so the debug log names the popup, not a raw id. */
static const char* ai_popup_tag_name(AiPopupTag tag) {
  switch (tag) {
    case AI_POPUP_TAG_INFO:
      return "INFO";
    case AI_POPUP_TAG_KING_AUDIENCE:
      return "KING_AUDIENCE";
    case AI_POPUP_TAG_EUROPE_KISSUP:
      return "EUROPE_KISSUP";
    case AI_POPUP_TAG_FOREIGN_TRADE_WHICH:
      return "FOREIGN_TRADE_WHICH";
    case AI_POPUP_TAG_FOREIGN_TRADE_OFFER:
      return "FOREIGN_TRADE_OFFER";
    case AI_POPUP_TAG_KING_MERC:
      return "KING_MERC";
    case AI_POPUP_TAG_KING_MERC_PEACE:
      return "KING_MERC_PEACE";
    case AI_POPUP_TAG_KING_CONGRESS:
      return "KING_CONGRESS";
    case AI_POPUP_TAG_KING_ARRIVAL:
      return "KING_ARRIVAL";
    case AI_POPUP_TAG_KING_CAPTURE:
      return "KING_CAPTURE";
    case AI_POPUP_TAG_KING_TAX:
      return "KING_TAX";
    case AI_POPUP_TAG_KING_LETTER:
      return "KING_LETTER";
    case AI_POPUP_TAG_FF_CONGRESS:
      return "FF_CONGRESS";
    case AI_POPUP_TAG_KING_DUMP_GOODS:
      return "KING_DUMP_GOODS";
    case AI_POPUP_TAG_CONTACT_MEET:
      return "CONTACT_MEET";
    case AI_POPUP_TAG_CONTACT_TEACH:
      return "CONTACT_TEACH";
    case AI_POPUP_TAG_CONTACT_GIFT:
      return "CONTACT_GIFT";
    case AI_POPUP_TAG_CONTACT_DEMAND:
      return "CONTACT_DEMAND";
    case AI_POPUP_TAG_CONTACT_RAID:
      return "CONTACT_RAID";
    case AI_POPUP_TAG_CONTACT_CONVERT:
      return "CONTACT_CONVERT";
    case AI_POPUP_TAG_CONTACT_REFUSE:
      return "CONTACT_REFUSE";
    case AI_POPUP_TAG_CONTACT_WELCOME:
      return "CONTACT_WELCOME";
    case AI_POPUP_TAG_DIPLO_WAR:
      return "DIPLO_WAR";
    case AI_POPUP_TAG_DIPLO_PEACE:
      return "DIPLO_PEACE";
    case AI_POPUP_TAG_DIPLO_BREAK:
      return "DIPLO_BREAK";
    case AI_POPUP_TAG_DIPLO_BOYCOTT:
      return "DIPLO_BOYCOTT";
    case AI_POPUP_TAG_DIPLO_FA:
      return "DIPLO_FA";
    case AI_POPUP_TAG_LANDFALL:
      return "LANDFALL";
    case AI_POPUP_TAG_MAP_CONFIRM:
      return "MAP_CONFIRM";
    case AI_POPUP_TAG_COMBAT_EUROPE:
      return "COMBAT_EUROPE";
    case AI_POPUP_TAG_COMBAT_LOOT:
      return "COMBAT_LOOT";
    case AI_POPUP_TAG_COMBAT_CAPTURE:
      return "COMBAT_CAPTURE";
    case AI_POPUP_TAG_COMBAT_SHIP:
      return "COMBAT_SHIP";
    case AI_POPUP_TAG_COMBAT_DEMOTE:
      return "COMBAT_DEMOTE";
    case AI_POPUP_TAG_COMBAT_AMBUSH:
      return "COMBAT_AMBUSH";
    case AI_POPUP_TAG_COMBAT_COLONY:
      return "COMBAT_COLONY";
    case AI_POPUP_TAG_COMBAT_SEIZURE:
      return "COMBAT_SEIZURE";
    case AI_POPUP_TAG_KING_SCORED:
      return "KING_SCORED";
    case AI_POPUP_TAG_CONTACT_INCITE:
      return "CONTACT_INCITE";
    case AI_POPUP_TAG_CONTACT_BEGFOOD:
      return "CONTACT_BEGFOOD";
    case AI_POPUP_TAG_CONTACT_TRADE_OFFER:
      return "CONTACT_TRADE_OFFER";
    case AI_POPUP_TAG_KING_GALLEON:
      return "KING_GALLEON";
    case AI_POPUP_TAG_CONTACT_WHACK:
      return "CONTACT_WHACK";
    case AI_POPUP_TAG_KING_FRIGATE:
      return "KING_FRIGATE";
    case AI_POPUP_TAG_CONTACT_BUYWHICH:
      return "CONTACT_BUYWHICH";
    case AI_POPUP_TAG_CONTACT_BUY0:
      return "CONTACT_BUY0";
    case AI_POPUP_TAG_DIPLO_TALK:
      return "DIPLO_TALK";
    case AI_POPUP_TAG_INDIAN_LAND:
      return "INDIAN_LAND";
    case AI_POPUP_TAG_CONTACT_LEARNSTAY:
      return "CONTACT_LEARNSTAY";
    case AI_POPUP_TAG_FOUNTAIN_YOUTH:
      return "FOUNTAIN_YOUTH";
    case AI_POPUP_TAG_BREWSTER_PICK:
      return "BREWSTER_PICK";
    case AI_POPUP_TAG_CONTACT_TRADE_PICK:
      return "CONTACT_TRADE_PICK";
    case AI_POPUP_TAG_CONTACT_EURO_WAR:
      return "CONTACT_EURO_WAR";
    case AI_POPUP_TAG_FORTIFY_TREATY:
      return "FORTIFY_TREATY";
    case AI_POPUP_TAG_COLONY_EVENT:
      return "COLONY_EVENT";
    case AI_POPUP_TAG_KING_WAR_END:
      return "KING_WAR_END";
    case AI_POPUP_TAG_KING_THRONE:
      return "KING_THRONE";
    case AI_POPUP_TAG_TRADE_TYPE:
      return "TRADE_TYPE";
    case AI_POPUP_TAG_SAILHOME:
      return "SAILHOME";
    case AI_POPUP_TAG_COLONY_ABANDON:
      return "COLONY_ABANDON";
    case AI_POPUP_TAG_WAR_SCORED:
      return "WAR_SCORED";
    case AI_POPUP_TAG_COMBAT_HALF:
      return "COMBAT_HALF";
    case AI_POPUP_TAG_CONTACT_REPARATIONS:
      return "CONTACT_REPARATIONS";
    case AI_POPUP_TAG_COLONY_CLEARSPEC:
      return "COLONY_CLEARSPEC";
    case AI_POPUP_TAG_COLONY_ATTACK:
      return "COLONY_ATTACK";
    case AI_POPUP_TAG_SCOUT_COLONY:
      return "SCOUT_COLONY";
  }
  return "UNKNOWN";
}

/* Body text with newlines flattened, so one popup is one log line. */
static void ai_popup_flatten(char* out, size_t out_size, const char* text) {
  if (!out || out_size == 0) {
    return;
  }
  size_t n = 0;
  for (const char* p = text ? text : ""; *p && n + 1 < out_size; ++p) {
    out[n++] = (*p == '\n' || *p == '\r') ? ' ' : *p;
  }
  out[n] = '\0';
}

static void ai_popup_log_present(const AiPopupRequest* req) {
  if (!diag_info_enabled() || !req) {
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  ai_popup_flatten(body, sizeof(body), req->body);
  char choices[AI_POPUP_CHOICE_MAX * (AI_POPUP_CHOICE_LEN + 8)];
  choices[0] = '\0';
  size_t at = 0;
  for (int i = 0; i < req->choice_count && i < AI_POPUP_CHOICE_MAX; ++i) {
    const int n = snprintf(
      choices + at, sizeof(choices) - at, "%s[%d]%s", i ? " " : "",
      req->choice_ids[i], req->choices[i]
    );
    if (n <= 0 || (size_t)n >= sizeof(choices) - at) {
      break;
    }
    at += (size_t)n;
  }
  diag_info(
    "POPUP show tag=%s kind=%s title=\"%s\" ctx=(a=%d b=%d payload=%d) body=\"%s\"%s%s",
    ai_popup_tag_name(req->tag),
    req->kind == AI_POPUP_KIND_CHOICE ? "choice" : "message",
    req->title[0] ? req->title : "-",
    req->nation_a,
    req->nation_b,
    req->payload,
    body,
    req->choice_count > 0 ? " choices=" : "",
    req->choice_count > 0 ? choices : ""
  );
}

bool ai_popup_queue_pending(const AiPopupState* st) {
  return st && st->queue_count > 0;
}

bool ai_popup_busy(const AiPopupState* st) {
  return st && (st->open || st->queue_count > 0 || st->has_result);
}

static bool ai_popup_present_index(AiPopupState* st, int pick);

bool ai_popup_try_present_next(AiPopupState* st) {
  if (!st || st->open || st->has_result || st->queue_count <= 0) {
    return false;
  }
  int pick = 0;
  /*
   * Zoom elected: DOS FUN_364b_0688 is per-colony BLOCKING — the elected
   * colony's remaining messages and its colony screen finish before anything
   * else (other colonies' chrome, king dialogs, …) can appear. Present only
   * the elected colony's own batch; everything else holds until
   * ai_popup_take_colony_zoom hands the colony to game_loop (which keeps the
   * hold up while the zoomed colony screen is open).
   */
  if (st->colony_zoom_elected != 0) {
    pick = -1;
    for (int i = 0; i < st->queue_count; ++i) {
      const AiPopupRequest* q = &st->queue[i];
      if (q->tag == AI_POPUP_TAG_COLONY_EVENT && q->payload >= 0 && q->payload < 64 &&
          (st->colony_zoom_elected & ((uint64_t)1u << q->payload)) != 0) {
        pick = i;
        break;
      }
    }
    if (pick < 0) {
      return false;
    }
    return ai_popup_present_index(st, pick);
  }
  /*
   * Contact chain in flight (bugs.md #427): DOS's contact dialogs are one
   * blocking inline call per neighbour, so the rest of THIS exchange presents
   * before any other chain starts. The follow-ups are enqueued as each dialog
   * is answered, so they sit at the tail behind whatever else was queued in
   * the same pulse — find them by key instead of taking the head. The key is
   * dropped as soon as nothing carries it any more.
   */
  if (st->active_chain != 0) {
    int chain_pick = -1;
    for (int i = 0; i < st->queue_count; ++i) {
      if (st->queue[i].chain == st->active_chain) {
        chain_pick = i;
        break;
      }
    }
    if (chain_pick >= 0) {
      pick = chain_pick;
    } else {
      st->active_chain = 0;
    }
  }
  return ai_popup_present_index(st, pick);
}

/*
 * bugs.md #398: a nested blocking pump (combat / king chrome raised mid-beat)
 * may bypass the colony-zoom hold — but ONLY for popups that are outside the
 * per-colony batch flow. A COLONY_EVENT of a non-elected colony must stay
 * held: the old "zero the elected mask and present anything" fallback showed
 * another colony's zoom CHOICE mid-batch, and the player ended up queued into
 * two colony screens back to back.
 */
bool ai_popup_try_present_next_urgent(AiPopupState* st) {
  if (!st || st->open || st->has_result || st->queue_count <= 0) {
    return false;
  }
  /* A chain in flight still owns the presenter (bugs.md #427) — an urgent pump
   * may skip the colony-zoom hold, but not into the middle of a contact
   * exchange. */
  if (st->active_chain != 0) {
    for (int i = 0; i < st->queue_count; ++i) {
      if (st->queue[i].chain == st->active_chain) {
        return ai_popup_present_index(st, i);
      }
    }
    st->active_chain = 0;
  }
  for (int i = 0; i < st->queue_count; ++i) {
    if (st->queue[i].tag != AI_POPUP_TAG_COLONY_EVENT) {
      return ai_popup_present_index(st, i);
    }
  }
  return false;
}

static bool ai_popup_present_index(AiPopupState* st, int pick) {
  st->current = st->queue[pick];
  for (int i = pick + 1; i < st->queue_count; ++i) {
    st->queue[i - 1] = st->queue[i];
  }
  st->queue_count--;
  /* DOS FUN_364b_0000: choices only while DS:0xa898 is clear — once zoom is
   * elected, the rest of that colony's batch presents as plain messages. */
  if (st->current.tag == AI_POPUP_TAG_COLONY_EVENT && st->current.payload >= 0 &&
      st->current.payload < 64 &&
      (st->colony_zoom_elected & ((uint64_t)1u << st->current.payload)) != 0) {
    st->current.choice_count = 0;
    st->current.kind = AI_POPUP_KIND_OK;
  }
  st->open = true;
  /* GAME.TXT @default=N (1-based) pre-highlights that row — DOS 6f74 stores
   * the matching option in the box's default slot as it parses. */
  st->selection = 0;
  if (st->current.default_choice > 0 && st->current.default_choice <= st->current.choice_count) {
    st->selection = st->current.default_choice - 1;
  }
  st->has_result = false;
  st->result_cancelled = false;
  /* Latch the exchange this dialog belongs to; only cleared once the queue
   * holds nothing with that key (bugs.md #427). A chainless popup does not
   * clear it — a colony-event batch cutting in must not let a rival chain
   * slip in front of the rest of this one. */
  if (st->current.chain != 0) {
    st->active_chain = st->current.chain;
  }
  st->king_anim_frame = 0;
  st->king_anim_next_ms = 0;
  ai_popup_log_present(&st->current);
  return true;
}

void ai_popup_finish(AiPopupState* st, bool cancelled, int choice_id) {
  if (!st) {
    return;
  }
  if (diag_info_enabled()) {
    const char* label = "-";
    for (int i = 0; i < st->current.choice_count && i < AI_POPUP_CHOICE_MAX; ++i) {
      if (st->current.choice_ids[i] == choice_id) {
        label = st->current.choices[i];
        break;
      }
    }
    diag_info(
      "POPUP answered tag=%s %s id=%d choice=\"%s\" ctx=(a=%d b=%d payload=%d)",
      ai_popup_tag_name(st->current.tag),
      cancelled ? "cancelled" : (st->current.choice_count > 0 ? "picked" : "dismissed"),
      choice_id,
      label,
      st->current.nation_a,
      st->current.nation_b,
      st->current.payload
    );
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

void ai_popup_cancel_current(AiPopupState* st) {
  if (!st || !st->open) {
    return;
  }
  const int is_info = (st->current.kind == AI_POPUP_KIND_OK || st->current.choice_count <= 0);
  ai_popup_finish(st, !is_info, is_info ? 0 : -1);
}

int ai_popup_portrait_tier_from_alarm(int alarm) {
  /* FUN_15dc_00a2 */
  if (alarm < 0x19) {
    return 0;
  }
  if (alarm < 0x32) {
    return 1;
  }
  if (alarm < 0x4b) {
    return 2;
  }
  return 3;
}

void ai_popup_set_last_portrait(AiPopupState* st, int tribe, int tier) {
  /* tribe 8 = the King (DS:0x1f5c = 8 → KING.SS/KING2.SS animated flair);
   * the old > 7 guard silently dropped the tax-audience King portrait. */
  if (!st || st->queue_count <= 0 || tribe > 8) {
    return;
  }
  AiPopupRequest* req = &st->queue[st->queue_count - 1];
  req->portrait_tribe = tribe < 0 ? -1 : tribe;
  req->portrait_tier = tier < 0 ? 0 : (tier > 3 ? 3 : tier);
}

/*
 * AK-55: both graphic latches patch the last queued request the same way —
 * clamp a negative to -1, drop the call entirely above the sheet family's top
 * index (MSS0..5, MYR0..3).
 */
static void ai_popup_set_last_graphic(AiPopupState* st, int* field, int value, int max_value) {
  if (!st || st->queue_count <= 0 || value > max_value) {
    return;
  }
  *field = value < 0 ? -1 : value;
}

void ai_popup_set_last_graphic_mss(AiPopupState* st, int mss) {
  if (!st || st->queue_count <= 0) {
    return;
  }
  ai_popup_set_last_graphic(st, &st->queue[st->queue_count - 1].graphic_mss, mss, 5);
}

void ai_popup_set_last_graphic_myr(AiPopupState* st, int nation) {
  if (!st || st->queue_count <= 0) {
    return;
  }
  ai_popup_set_last_graphic(st, &st->queue[st->queue_count - 1].graphic_myr, nation, 3);
}

void ai_popup_consume_result(AiPopupState* st) {
  if (!st) {
    return;
  }
  st->has_result = false;
  st->result_cancelled = false;
  st->result_choice_id = -1;
}

/* ---- Shared queue predicates and chrome tail (audit AC-19 / theme L) ---- */

static bool ai_popup_req_matches(
  const AiPopupRequest* req,
  AiPopupTag tag,
  int kind,
  AiPopupKeyField key_field,
  int key_a,
  int key_b
) {
  if (req->tag != tag) {
    return false;
  }
  if (kind >= 0 && (int)req->kind != kind) {
    return false;
  }
  switch (key_field) {
    case AI_POPUP_KEY_NATION_A:
      return req->nation_a == key_a;
    case AI_POPUP_KEY_NATION_AB:
      return req->nation_a == key_a && req->nation_b == key_b;
    case AI_POPUP_KEY_ANY:
    default:
      return true;
  }
}

bool ai_popup_pending(
  const AiPopupState* st,
  AiPopupTag tag,
  int kind,
  AiPopupKeyField key_field,
  int key_a,
  int key_b
) {
  if (!st) {
    return false;
  }
  for (int i = 0; i < st->queue_count; ++i) {
    if (ai_popup_req_matches(&st->queue[i], tag, kind, key_field, key_a, key_b)) {
      return true;
    }
  }
  return st->open && ai_popup_req_matches(&st->current, tag, kind, key_field, key_a, key_b);
}

bool ai_popup_pending_payload(
  const AiPopupState* st,
  AiPopupTag tag,
  int kind,
  int (*decode)(int payload),
  int key
) {
  if (!st || !decode) {
    return false;
  }
  for (int i = 0; i < st->queue_count; ++i) {
    const AiPopupRequest* req = &st->queue[i];
    if (ai_popup_req_matches(req, tag, kind, AI_POPUP_KEY_ANY, 0, 0) &&
        decode(req->payload) == key) {
      return true;
    }
  }
  return st->open &&
         ai_popup_req_matches(&st->current, tag, kind, AI_POPUP_KEY_ANY, 0, 0) &&
         decode(st->current.payload) == key;
}

void popup_chrome_ok(
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages,
  const char* section,
  const PopupMsgTokens* tok,
  const char* fallback
) {
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(messages, section, tok, fallback, body, sizeof(body));
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}
