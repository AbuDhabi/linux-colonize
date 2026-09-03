#ifndef COLONIZE_AI_POPUP_H
#define COLONIZE_AI_POPUP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/font.h"
#include "core/popup.h"
#include "core/ss.h"
#include "platform/platform.h"

/*
 * Map-level AI popup queue + wood dialog (body-only dismiss / text + choices).
 *
 * AI turn code enqueues requests onto ColonizeTurnContext.ai_popups; the game
 * loop presents them after the turn processor is idle (one at a time). Chrome
 * is popup_draw + WOODTILE; text matches nation-wizard wood dialogs: FONTINTR
 * unbold + black drop-shadow, @COLORS basic/select, default @width=190.
 *
 * KIND_OK: GAME.TXT info sections have no response lines — dismiss with
 * Enter/Space/Esc or any click (no invented "OK" button). KIND_CHOICE: pick
 * a listed option; Esc / click-outside / right-click cancels (result_cancelled).
 */

/* One TURN_PROC_SETUP slice queues every colony's production chrome at once
 * (starve/spoil/built/…) before the blocking presenter can drain any of it —
 * a large empire overflows 16 and enqueue drops the popup silently. */
#define AI_POPUP_QUEUE_MAX 32
#define AI_POPUP_CHOICE_MAX 6
#define AI_POPUP_BODY_LEN 512
#define AI_POPUP_TITLE_LEN 64
/* Keep ≥ POPUP_MSG_CHOICE_LEN (GAME.TXT @DECLARE Never… is 53 chars). */
#define AI_POPUP_CHOICE_LEN 64

typedef enum AiPopupKind {
  AI_POPUP_KIND_OK = 0,
  AI_POPUP_KIND_CHOICE = 1
} AiPopupKind;

/* Caller-defined; used when applying choice results / chaining. */
typedef enum AiPopupTag {
  AI_POPUP_TAG_INFO = 0,
  AI_POPUP_TAG_KING_AUDIENCE = 1,
  AI_POPUP_TAG_KING_MERC = 2,
  AI_POPUP_TAG_KING_CONGRESS = 3,
  AI_POPUP_TAG_KING_ARRIVAL = 4,
  AI_POPUP_TAG_KING_CAPTURE = 5,
  AI_POPUP_TAG_KING_TAX = 6,
  AI_POPUP_TAG_KING_LETTER = 7, /* thin 160a independence rename; game_loop plays the
                                 * 160a signing cinematic (core/declaration.c) in front
                                 * of this popup the frame it is presented */
  AI_POPUP_TAG_FF_CONGRESS = 8, /* Continental Congress FF elect chrome */
  AI_POPUP_TAG_KING_DUMP_GOODS = 9, /* refuse dump-goods second cargo CHOICE */
  AI_POPUP_TAG_CONTACT_MEET = 10,
  AI_POPUP_TAG_CONTACT_TEACH = 11,
  AI_POPUP_TAG_CONTACT_GIFT = 12,
  AI_POPUP_TAG_CONTACT_DEMAND = 13,
  AI_POPUP_TAG_CONTACT_RAID = 14,
  AI_POPUP_TAG_CONTACT_CONVERT = 15,
  AI_POPUP_TAG_CONTACT_REFUSE = 16,
  AI_POPUP_TAG_CONTACT_WELCOME = 17, /* FUN_5bfb_022e @INDIANWELCOME Yes/No */
  AI_POPUP_TAG_CONTACT_VILLAGE_WARN = 18, /* FUN_4d56_4528 human warn: Attack/Leave */
  AI_POPUP_TAG_DIPLO_WAR = 20,
  AI_POPUP_TAG_DIPLO_PEACE = 21,
  AI_POPUP_TAG_DIPLO_ALLIANCE = 22,
  AI_POPUP_TAG_DIPLO_BREAK = 23,
  AI_POPUP_TAG_DIPLO_BOYCOTT = 24,
  AI_POPUP_TAG_DIPLO_FA = 25, /* thin FA 3f41 report / gift chrome (F2–F9 PARKED) */
  AI_POPUP_TAG_LANDFALL = 26, /* ship→bare land: Stay With Ships / Make Landfall */
  /* Map confirm gates (disband / overboard / quit / retire / trade-delete). */
  AI_POPUP_TAG_MAP_CONFIRM = 27,
  /* Combat outcome structural modals (GAME.TXT @SECTION). */
  AI_POPUP_TAG_COMBAT_EUROPE = 28,
  AI_POPUP_TAG_COMBAT_LOOT = 29,
  AI_POPUP_TAG_COMBAT_CAPTURE = 30,
  AI_POPUP_TAG_COMBAT_SHIP = 31,
  AI_POPUP_TAG_COMBAT_DEMOTE = 32,
  AI_POPUP_TAG_COMBAT_AMBUSH = 33,
  AI_POPUP_TAG_COMBAT_RANSOM = 34, /* treasure Accept/Refuse before gold credit */
  AI_POPUP_TAG_COMBAT_COLONY = 35, /* @CAPTURED* / @BURNED* */
  AI_POPUP_TAG_COMBAT_SEIZURE = 36, /* privateer @SEIZURE* */
  AI_POPUP_TAG_KING_SCORED = 37, /* peacetime 1800 @SCORED That's all / Keep playing */
  AI_POPUP_TAG_CONTACT_INCITE = 38, /* FUN_4d56_417e Incite Indians: pick target nation, pay */
  AI_POPUP_TAG_CONTACT_BEGFOOD = 39, /* FUN_5bfb_022e already-met adjacency: @INDIANBEGFOOD Give/Refuse */
  AI_POPUP_TAG_CONTACT_TRADE_OFFER = 40, /* FUN_4d56_2820 LAB_002e92 human buy-offer: Accept/Decline a locked price */
  AI_POPUP_TAG_KING_GALLEON = 41, /* FUN_5fef_1908 @KINGGALLEON2/3 Crown transports coastal Treasure: Accept/Refuse */
  AI_POPUP_TAG_CONTACT_WHACK = 42, /* FUN_465b_0000 @WHACKINDIANS "Shall we attack the X?" Yes/No before first native attack */
  AI_POPUP_TAG_KING_FRIGATE = 43, /* FUN_3844_00f2 @KINGFRIGATE Crown offers a Frigate (+10% tax): Yes/No */
  AI_POPUP_TAG_CONTACT_BUYWHICH = 51, /* FUN_4d56_2820 LAB_002e92 @BUYWHICH: pick one of 3 tribe goods (was 43, collided with KING_FRIGATE) */
  AI_POPUP_TAG_CONTACT_BUY0 = 44, /* FUN_4d56_2820 LAB_002e92 @BUY0: Accept/Refuse the tribe's price */
  AI_POPUP_TAG_DIPLO_TALK = 45, /* FUN_5bfb_153e phases 2-4: human x AI Euro encounter dialog (payload = stage) */
  AI_POPUP_TAG_INDIAN_LAND = 46, /* @INDIANLAND/@INDIANROAD/@INDIANFOREST encroachment CHOICE (nation_a = unit, nation_b = kind, payload = x|y<<8) */
  AI_POPUP_TAG_CONTACT_LEARNSTAY = 47, /* thunk_FUN_1000_a618 @LEARNSTAY Yes/No (payload = unit | skill<<16) */
  AI_POPUP_TAG_FOUNTAIN_YOUTH = 48, /* FUN_65dd_0004 case 1: 8× free FUN_38fd_4884(1,0) Recruit pick (payload = picks left) */
  AI_POPUP_TAG_BREWSTER_PICK = 49, /* 5e52 Brewster branch: FUN_38fd_4884(0,1) @RECRUITCHOOSE free pick (nation_a = human) */
  AI_POPUP_TAG_CONTACT_TRADE_PICK = 50, /* FUN_4d56_2820 shell: multi-hold unit picks which cargo to offer (payload = unit id, 99 = cancel) */
  AI_POPUP_TAG_CONTACT_EURO_WAR = 52, /* FUN_465b_0000 @HAVETREATY Cancel Action/Break Treaty before attacking a treaty peer (nation_a = unit, nation_b = target, payload = x|y<<8) */
  AI_POPUP_TAG_COLONY_EVENT = 53, /* FUN_364b_0000 colony EOT message (payload = colony id): "Continue turn." / "Zoom to colony." choices until zoom elected for that colony */
  /* Was 52, colliding with CONTACT_EURO_WAR — a resolved @HAVETREATY confirm
   * looked like a war-end dismissal (and vice versa). Runtime-only ids. */
  AI_POPUP_TAG_KING_WAR_END = 54, /* @WINNING / @LOSING1-3 / @RETIRING2 revolution-over OK.
                                   * payload 1 = win announcement (throne audience follows),
                                   * payload 4 = loss announcement (throne audience follows),
                                   * payload 2 = loss, dismissal opens the retire score directly. */
  AI_POPUP_TAG_KING_THRONE = 55, /* full-screen royal audience (DOS FUN_75c2_20e2):
                                  * payload 1 = @KINGLOSE on KINGLOSE.SS (war won),
                                  * payload 2 = @KINGWIN on KINGWIN.SS (war lost);
                                  * dismissal opens the retire score chain. */
  AI_POPUP_TAG_TRADE_TYPE = 57, /* @TRADETYPE create-wizard CHOICE (DOS OVL19): 1 = Sea, 0 = Land */
  AI_POPUP_TAG_WAR_SCORED = 56 /* post-HoF @SCORED CHOICE after a WoI win:
                                * 1 = "That's all." (title menu), 2 = "Keep playing anyway." */
} AiPopupTag;

typedef struct AiPopupRequest {
  AiPopupKind kind;
  AiPopupTag tag;
  int nation_a; /* optional context (often human) */
  int nation_b;
  int payload; /* free int (tribe id, colony id, …) */
  char title[AI_POPUP_TITLE_LEN];
  char body[AI_POPUP_BODY_LEN];
  char choices[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
  int choice_ids[AI_POPUP_CHOICE_MAX];
  int choice_count;
  int width; /* GAME.TXT @width (0 = AI_POPUP_DEFAULT_WIDTH); taken from popup_msg_fill's side-channel */
  /* Chief portrait (FUN_6f74_0042): IND{tribe}A{tier}.SS drawn beside the
   * dialog when portrait_tribe is 0..7; tier = alarm band (FUN_15dc_00a2:
   * <25 → 0, <50 → 1, <75 → 2, else 3). -1 = none. */
  int portrait_tribe;
  int portrait_tier;
  /*
   * MSS/MYR popup decoration (DOS DS:0x1f5e / DS:0x1f60 latches,
   * FUN_6f74_00c2/00ec): -1 = none. graphic_mss 0..5 = MSS{n}.SS (theme
   * figure above the dialog), graphic_myr 0..3 = MYR{n}.SS (Euro ruler,
   * index = nation id). Placement comes from the sheet's own header words
   * (ss.h place_*); MYR wins when both are set (DOS loads it last).
   * graphic_mss is auto-filled from popup_msg_fill's section table
   * (popup_msg_take_pending_graphic); MYR is set explicitly.
   */
  int graphic_mss;
  int graphic_myr;
} AiPopupRequest;

typedef struct AiPopupState {
  AiPopupRequest queue[AI_POPUP_QUEUE_MAX];
  int queue_count;

  bool open;
  AiPopupRequest current;
  int selection;

  bool has_result;
  bool result_cancelled;
  int result_choice_id;
  AiPopupTag result_tag;
  int result_nation_a;
  int result_nation_b;
  int result_payload;

  /* King flair one-shot animation (KING2.SS; bugs.md). */
  int king_anim_frame;
  uint32_t king_anim_next_ms;

  /* DOS DS:0xa898 latch, per colony (bit = colony id): once the player picks
   * "Zoom to colony." the rest of that colony's queued messages present
   * optionless, and the colony screen opens after the last one is answered
   * (FUN_364b_0688 tail FUN_281f_0608). */
  uint64_t colony_zoom_elected;

  int dialog_x;
  int dialog_y;
  int dialog_w;
  int dialog_h;
  int list_y0;
  int line_h;
} AiPopupState;

void ai_popup_init(AiPopupState* st);
void ai_popup_clear(AiPopupState* st);

/* Enqueue (no-op if full or st NULL). Returns false if dropped. */
bool ai_popup_enqueue(AiPopupState* st, const AiPopupRequest* req);
bool ai_popup_enqueue_ok(
  AiPopupState* st,
  AiPopupTag tag,
  const char* title,
  const char* body
);
bool ai_popup_enqueue_ok_ctx(
  AiPopupState* st,
  AiPopupTag tag,
  int nation_a,
  int nation_b,
  int payload,
  const char* title,
  const char* body
);
bool ai_popup_enqueue_choice(
  AiPopupState* st,
  AiPopupTag tag,
  const char* title,
  const char* body,
  const char* const* choice_labels,
  const int* choice_ids,
  int choice_count
);
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
);

/*
 * Colony EOT message (DOS FUN_364b_0000): CHOICE with "Continue turn." (1) /
 * "Zoom to colony." (2) — LABELS.TXT @MISC 34/35 (DS:0x2dfe/0x2e00). Presents
 * optionless once zoom is elected for that colony (colony_zoom_elected bit).
 */
bool ai_popup_enqueue_colony_event(AiPopupState* st, int colony_id, const char* body);
/* Override the built-in choice labels from LABELS.TXT @MISC 34/35. */
void ai_popup_set_colony_event_labels(const char* continue_label, const char* zoom_label);
/* Latch a "Zoom to colony." election (result_choice_id 2). */
void ai_popup_colony_zoom_elect(AiPopupState* st, int colony_id);
/*
 * Colony whose zoom was elected and whose queued messages are all answered
 * (DOS: colony screen opens after that colony's message batch). Clears the
 * bit; -1 = none ready.
 */
int ai_popup_take_colony_zoom(AiPopupState* st);

bool ai_popup_queue_pending(const AiPopupState* st);
bool ai_popup_busy(const AiPopupState* st); /* open or queued */

/* If !open && queue non-empty, pop front into current and open. */
bool ai_popup_try_present_next(AiPopupState* st);

/* Cancel the OPEN dialog as if Esc was pressed (result_cancelled for a CHOICE). */
void ai_popup_cancel_current(AiPopupState* st);
bool ai_popup_handle_input(AiPopupState* st, const ColonizeInputState* input);

void ai_popup_render(
  AiPopupState* st,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t hilite_color,
  uint8_t select_color,
  ColonizeFramebuffer8* framebuffer
);

/* Clear has_result after game_loop applies it. */
void ai_popup_consume_result(AiPopupState* st);

/*
 * Chief portraits (P8.6). data_dir + the framebuffer palette the sheets are
 * remapped onto; sheets load lazily on first render. Call once after the
 * palette is loaded. NULL disables portraits.
 */
void ai_popup_set_portrait_source(const char* data_dir, const struct ColonizePalette* palette);
/* Attach IND{tribe}A{tier} to the most recently enqueued request (no-op when
 * the queue is empty or tribe > 7; tribe < 0 clears the portrait). */
void ai_popup_set_last_portrait(AiPopupState* st, int tribe, int tier);
/* Frame clock for the King flair animation — call once per game_update. */
void ai_popup_set_now_ms(uint32_t now_ms);
/* FUN_15dc_00a2 alarm → portrait tier. */
int ai_popup_portrait_tier_from_alarm(int alarm);
/*
 * Set the MSS/MYR decoration on the newest queued request (mirrors the DOS
 * latch writes around FUN_281f_0652 / FUN_2a1f_0688). mss -1..5, myr = Euro
 * nation 0..3 or -1. Sheets load lazily from the portrait source dir.
 */
void ai_popup_set_last_graphic_mss(AiPopupState* st, int mss);
void ai_popup_set_last_graphic_myr(AiPopupState* st, int nation);

#endif
