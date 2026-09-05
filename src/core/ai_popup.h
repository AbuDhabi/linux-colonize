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
  AI_POPUP_TAG_SAILHOME = 58, /* @SAILHOME (FUN_4720_049e reason 5): eastward step deeper into the
                               * sea lane. 1 = sail for Europe; 0 = remain (step still commits).
                               * nation_a = ship id, nation_b = dest x, payload = dest y. */
  AI_POPUP_TAG_COLONY_ABANDON = 59, /* @ABANDON / @ABANDON2 colony-screen confirm (DOS 2f2b
                                     * caseD_a: FUN_281f_0652(name, 5), @default=2 → "Never!").
                                     * 1 = abandon, 2 / Esc = keep.
                                     * nation_a = colonist slot, nation_b = eject role. */
  AI_POPUP_TAG_WAR_SCORED = 56, /* post-HoF @SCORED CHOICE after a WoI win:
                                 * 1 = "That's all." (title menu), 2 = "Keep playing anyway." */
  AI_POPUP_TAG_COMBAT_HALF = 60 /* @HALF (FUN_5fef_1b0e, viceroy_unpacked.c ~100365): the
                                 * attacker has less than one whole movement point left, so it
                                 * would fight at remaining/3 strength. 1 = "Charge!",
                                 * 2 / Esc = "Then let them rest."
                                 * nation_a = attacker unit id, nation_b = remaining thirds,
                                 * payload = dest x | dest y << 8. */
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
  /*
   * GAME.TXT `@default=N`: 1-based choice row DOS pre-highlights when the
   * dialog opens (FUN_6f74_32a4 default slot). 0 = first row.
   * Auto-filled from popup_msg_fill's side-channel, same as `width`.
   */
  int default_choice;
} AiPopupRequest;

/*
 * Status-line ring. DOS's FUN_1009_0036 waits min(armed deadline, now + 30
 * ticks) on the 60.877 Hz clock and breaks early on any key or mouse button,
 * so in practice each line holds the strip for about half a second.
 */
#define AI_POPUP_BAR_MSG_MAX 12
#define AI_POPUP_BAR_MSG_LEN 96
/* Held while more lines are queued behind this one (DOS's now + 30 ticks). */
#define AI_POPUP_BAR_MSG_MS 500u
/*
 * Held by the last line of a run: nothing composes behind it, so it lives out
 * the full armed deadline. Both producers arm 0x78 ticks on the 60.877 Hz
 * clock (FUN_291f_07b0 / FUN_38fd_19d8) = 1971 ms.
 */
#define AI_POPUP_BAR_MSG_LAST_MS 1971u

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

  /*
   * DOS status-line queue (DS:0x2d54 + the DS:0x4a "message armed" flag).
   * FUN_1009_00b4 flushes the previous line — waiting out its dwell or a key
   * press — before the next one is composed, so these present one at a time
   * on the map's top strip and never as dialogs. Custom House sales are the
   * player-visible case (FUN_364b_0688's per-cargo 0056/006a/.../0092 run).
   */
  char bar_msg[AI_POPUP_BAR_MSG_MAX][AI_POPUP_BAR_MSG_LEN];
  /*
   * DOS FUN_1009_0244's first argument, kept per line because it picks the
   * ink FUN_1009_0004 hands the strip painter: 1/2 = 0x95 (gold, every
   * success line), 3 = 0x0c (red, the Europe refusals), anything else 0x44
   * (@COLORS basic green). See ai_popup_bar_message_color.
   */
  uint8_t bar_msg_kind[AI_POPUP_BAR_MSG_MAX];
  int bar_msg_count;       /* [0] is the one on screen */
  uint32_t bar_msg_until_ms; /* dwell deadline; 0 = not started yet */

  int dialog_x;
  int dialog_y;
  int dialog_w;
  int dialog_h;
  int list_y0;
  int line_h;
} AiPopupState;

/*
 * Queue one status line for the map's top strip. Silently drops when full,
 * like ai_popup_enqueue.
 */
bool ai_popup_enqueue_bar_message(AiPopupState* st, const char* text);

/*
 * As above with an explicit DOS arm kind (FUN_1009_0244's first argument).
 * The plain enqueue is kind 1 — what both DOS producers use for a sale.
 */
bool ai_popup_enqueue_bar_message_kind(AiPopupState* st, const char* text, int kind);

/* Line currently owning the strip, or NULL. */
const char* ai_popup_bar_message(const AiPopupState* st);

/* Ink for that line (DOS FUN_1009_0004); COLONIZE_COL_BASIC when none is up. */
uint8_t ai_popup_bar_message_color(const AiPopupState* st);

/*
 * Advance the strip: start the dwell on a freshly-shown line, retire it when
 * the dwell expires or `dismiss` (any key / mouse button, DOS FUN_1009_0036).
 * Returns true while a line still owns the strip — the caller holds the turn
 * pipeline for exactly that long, the way DOS's blocking wait does.
 */
bool ai_popup_bar_service(AiPopupState* st, uint32_t now_ms, bool dismiss);

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

/*
 * Stable-reorder the queue: entries tagged `promote` that sit behind the first
 * entry tagged `before` move to just in front of it (relative order kept).
 * DOS FUN_364b_0688 runs bells+FF (4345_0a22, Phase A prologue) BEFORE the
 * colony message phases, so a Congress nomination/election presents ahead of
 * the colony production chrome; the port computes production first (RNG order
 * pinned by goldens) and fixes the presentation order here instead.
 */
void ai_popup_promote_tag_before(AiPopupState* st, AiPopupTag promote, AiPopupTag before);

/* Debug-log name for a tag ("KING_TAX", "COMBAT_LOOT", ...). Never NULL. */
const char* ai_popup_tag_name(AiPopupTag tag);

bool ai_popup_queue_pending(const AiPopupState* st);
bool ai_popup_busy(const AiPopupState* st); /* open or queued */

/* If !open && queue non-empty, pop front into current and open. */
bool ai_popup_try_present_next(AiPopupState* st);
/* Present the first queued NON-colony-event popup, bypassing the colony-zoom
 * hold — for nested blocking pumps only (bugs.md 404). */
bool ai_popup_try_present_next_urgent(AiPopupState* st);

/*
 * Open the newest queued request with `tag` right now, ahead of everything
 * else. For player-initiated modals only (a colony-screen confirm): DOS runs
 * those as a nested blocking dialog from inside the screen's own event loop,
 * so they never wait behind queued AI chrome or a colony-zoom hold.
 */
bool ai_popup_present_now(AiPopupState* st, AiPopupTag tag);

/*
 * Reorder only: move the newest queued request with `tag` to the head of the
 * queue without opening it. For a modal that steps aside for a full-screen
 * detour (F1 on the Congress debate) and must be the first thing back when
 * that detour closes, rather than losing its place to whatever else queued up.
 */
bool ai_popup_move_tag_to_front(AiPopupState* st, AiPopupTag tag);

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
 * Chief portraits (P8.6): the directory the popup art sheets load from, lazily
 * on first render. Call once at startup; NULL disables portraits.
 */
void ai_popup_set_portrait_source(const char* data_dir);
/*
 * Lend `dst` the open popup's portrait / MSS / MYR sheet its own palette
 * entries, for every slot `dst` leaves black. DOS shows these sheets over
 * screen palettes that reserve a block of DAC slots (TERRAIN.SS 152..251,
 * EUROPE.PIK 120..251) and loads the sheet's own entries there; the port
 * therefore blits the art raw and merges here instead of nearest-colour
 * remapping it (bugs.md: the King's tax-audience flair). No-op when no
 * decorated popup is open.
 */
void ai_popup_art_palette_merge(AiPopupState* st, struct ColonizePalette* dst);
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
