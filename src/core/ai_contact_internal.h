#ifndef COLONIZE_CORE_AI_CONTACT_INTERNAL_H
#define COLONIZE_CORE_AI_CONTACT_INTERNAL_H

/*
 * Stage seams for ai_contact.c's ai_contact_indian_raids pass
 * (ai_contact_raid_*). See core/internal.h for the COLONIZE_INTERNAL /
 * COLONIZE_TESTING pattern this follows.
 */

#include "core/ai_contact.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/internal.h"
#include "core/turn.h"

typedef enum {
  AI_RAID_CONTINUE = 0,  /* stage fell through — run the next one */
  AI_RAID_NEXT_BRAVE = 1 /* stage ended this Brave (was a bare `continue;`) */
} AiRaidStatus;

/* Per-Brave state shared between the stages of one ai_contact_indian_raids pass. */
struct ai_contact_raid_ctx {
  ColonizeTurnContext* ctx;
  ColonizeCol1Indian* ind;
  ColonizeDosRng* rng;
  int nation_id;
  ColonizeUnit* brave; /* re-read after any call that can free/replace it */
  int target_euro;
  int max_alarm;
  int attacked;
};

#ifdef COLONIZE_TESTING
int ai_contact_raid_port_ship(ColonizeTurnContext* ctx, const ColonizeColony* c);
AiRaidKind ai_contact_raid_kind_demote(
  ColonizeTurnContext* ctx, ColonizeColony* c, AiRaidKind kind
);
int ai_contact_raid_gate_target(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id,
  int* out_euro, int* out_alarm
);
int ai_contact_raid_alarm_delta(AiRaidKind kind);
void ai_contact_raid_stage_combat(struct ai_contact_raid_ctx* a);
int ai_contact_raid_pick_colony(struct ai_contact_raid_ctx* a);
AiRaidStatus ai_contact_raid_stage_colony(struct ai_contact_raid_ctx* a);
#endif /* COLONIZE_TESTING */

/* ===== Cross-file seams of the ai_contact.c split (2026-09-23) =====
 * ai_contact.c was 10.3k lines; it is now split along its banners into
 * ai_contact{,_demand,_trade,_raid,_actions}.c. The declarations below are
 * the symbols used across those files (plus the module's file-scope types,
 * which used to sit in the single .c); everything else stayed `static` in
 * its own file. Code moved verbatim — these are the only de-static'd names.
 * ===================================================================== */

#include "core/ai_diplo.h"
#include "core/colony.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/units.h"
#include "core/world.h"

#include <stdint.h>

/* Module-wide types and popup/choice id enums (were file-local). */
enum {
  AI_CONTACT_CHOICE_TRADE = 1,
  AI_CONTACT_CHOICE_GIFT = 2,
  AI_CONTACT_CHOICE_DEMAND = 3,
  AI_CONTACT_CHOICE_TEACH = 4,
  AI_CONTACT_CHOICE_LEAVE = 5,
  AI_CONTACT_CHOICE_INCITE = 6,
  AI_CONTACT_CHOICE_MISSION = 7,
  AI_CONTACT_CHOICE_HERESY = 8,
  AI_CONTACT_CHOICE_CHIEF = 9,
  AI_CONTACT_CHOICE_ENTER_HOSTILE = 10,
  AI_CONTACT_CHOICE_ATTACK_VILLAGE = AI_CONTACT_CHOICE_ATTACK
};

enum {
  AI_CONTACT_LEARNSTAY_YES = 1,
  AI_CONTACT_LEARNSTAY_NO = 2
};

enum {
  AI_CONTACT_GIFT_SMALL = 1,    /* −5 gold, friction −1 */
  AI_CONTACT_GIFT_LARGE = 2,    /* −10 gold, friction −2 */
  AI_CONTACT_GIFT_GENEROUS = 3  /* −20 gold, friction −3 (deep amount arm thin) */
};

enum {
  AI_CONTACT_DEMAND_TOOLS = 1, /* −10 tools (stock/unit ≥20), friction −3 */
  AI_CONTACT_DEMAND_GOLD = 2   /* −15 gold (treasury ≥50), friction −3 */
};

enum {
  AI_CONTACT_REPARATIONS_ROW1 = 1,
  AI_CONTACT_REPARATIONS_ROW2 = 2
};

enum {
  AI_CONTACT_REPARATIONS_CITY = 0,  /* 0x1866 @INDIANCITY — colony stores */
  AI_CONTACT_REPARATIONS_WAGONS = 1 /* 0x1871 @INDIANWAGONS — a wagon's hold */
};

enum {
  AI_CONTACT_TRADE_OFFER_ACCEPT = 1,
  AI_CONTACT_TRADE_OFFER_DECLINE = 2,

  AI_CONTACT_TRADE_OFFER_HAGGLE = 3, /* @TRADE0 fairer-price arm (LAB_002bbc iStack_5e == 2) */

  AI_CONTACT_TRADE_OFFER_GIFT = 4 /* @TRADE0 gift arm (iStack_5e == 3, round 0) */
};

enum {
  AI_CONTACT_WELCOME_YES = 1,
  AI_CONTACT_WELCOME_NO = 2
};

typedef struct AiContactUnitClass {
  int is_ship;
  int is_wagon;
  int is_scout;
  int is_missionary;
  int attack;
  int can_live_among; /* FUN_1000_8d68 >= 0 && attack < 2 && !scout (bugs.md #726) */
} AiContactUnitClass;

typedef struct AiContactMeetEcon2154 {
  int16_t ask[16];
  int16_t bid[16];
} AiContactMeetEcon2154;

typedef struct AiContactVisitMood {
  int valid;
  int bvar6;
  int turn;
  int brave_id;
} AiContactVisitMood;

typedef struct AiContact2820 {
  int active;
  int nation_id;
  int unit_id;
  int cargo; /* iStack_c8; -1 = empty-handed */
  int slot; /* iStack_7e */
  int qty; /* iStack_6a = hold amount; DS:0x8dc4 after the slot removal */
  int16_t ask[16]; /* DS:0x9e58 (-25000) after the shell's zeroing */
  int16_t bid[16]; /* DS:0x9e78 (-0x6188) */
  int cand[16]; /* acStack_98: cargo ids sorted ascending by bid */
  int price; /* uStack_62 */
  int fair; /* iStack_ce */
  int c4; /* iStack_c4 */
  int tier2; /* iStack_80 */
  int value_idx; /* uStack_ca → NAMES @VALUES */
  int round; /* iStack_88 */
  int sold_ok; /* iStack_c6 */
  int buy_cargo; /* LAB_002e92 pick */
  int buy_qty;
  ColonizeDosRng rng;
} AiContact2820;

typedef struct AiContactReparations {
  int active;
  int nation_id; /* Indian nation 4..11 */
  int flavor;    /* AI_CONTACT_REPARATIONS_CITY / _WAGONS */
  int tribe_index;
  int brave_id;
  int colony_id; /* CITY: the colony asked; WAGONS: -1 */
  int unit_id;   /* WAGONS: the wagon; CITY: -1 */
  int hold;      /* WAGONS: hold slot (DOS always 0) */
  int cargo;
  int qty;
  int score; /* CITY: DOS local_46, the winning price*stock product */
} AiContactReparations;

typedef enum AiRaidTokKind {
  AI_RAID_TOK_NONE = 0,
  AI_RAID_TOK_SHIP,
  AI_RAID_TOK_GOLD,
  AI_RAID_TOK_STORES,
  AI_RAID_TOK_BURN
} AiRaidTokKind;

typedef struct AiRaidChrome {
  AiRaidKind kind;
  const char* section;       /* GAME.TXT tag, NULL = thin line only */
  const char* popup_fallback;
  const char* thin_colony;   /* NULL = always use thin_bare */
  const char* thin_bare;
  int sound;                 /* -1 = silent */
  int bgm;                   /* -1 = leave the tune pool alone */
  AiRaidTokKind tok;
  int popup_without_colony;
} AiRaidChrome;

typedef enum {
  AI_CONTACT_POPUP_CONTINUE = 0, /* not this arm — run the next stage */
  AI_CONTACT_POPUP_DONE = 1      /* arm consumed the result (was a bare `return;`) */
} AiContactPopupStatus;

/* FUN_4d56_417e Mode 1 incite payload bits (see ai_contact_demand.c). */
#define AI_CONTACT_INCITE_STAGE_CONFIRM 4
#define AI_CONTACT_INCITE_PAY 1

/* Shared file-scope state (was `static s_*` in the single file). */
extern int ai_contact_s_last_raid_kind;
extern char ai_contact_s_last_burn_building[48];
extern char ai_contact_s_last_stores_cargo[48];
extern char ai_contact_s_last_ship_type[48];
extern int ai_contact_s_last_gold_drained;
extern int ai_contact_s_visit_brave_id[8];
extern int ai_contact_s_visit_brave_turn[8];
extern const ColonizeMsgCatalog* ai_contact_s_contact_names;
extern AiContactVisitMood ai_contact_s_visit_mood[8][4];
extern AiContact2820 ai_contact_s_2820[4];
extern AiContactReparations ai_contact_s_reparations[4];

/* Shared helpers (were `static` in the single file). */
ColonizeCol1Tribe* ai_contact_menu_village(ColonizeTurnContext* ctx, int nation_id, const ColonizeUnit* u);
ColonizeUnit* ai_contact_find_adjacent_euro( ColonizeTurnContext* ctx, int nation_id, int e, int* near_x, int* near_y );
const char* ai_contact_cargo_name(int cargo_idx);
const char* ai_contact_euro_name(int euro_nation);
const char* ai_contact_learner_skill_name( const ColonizeUnitPool* units, const ColonizeUnit* u );
const char* ai_contact_level_noun(const ColonizeTurnContext* ctx, int tech);
int ai_contact_2820_begin( ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, ColonizeUnit* unit );
int ai_contact_a618_skill(ColonizeTurnContext* ctx, int nation_id, const ColonizeCol1Tribe* t);
int ai_contact_apply_demand_gold( ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e );
int ai_contact_apply_demand_tools( ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, ColonizeUnit* other, int near_x, int near_y );
int ai_contact_auto_trade( ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, ColonizeUnit* unit );
int ai_contact_brave_visited_this_turn( const ColonizeTurnContext* ctx, int nation_id, int brave_id );
int ai_contact_brave_walked_up_to(const ColonizeUnit* brave, int cx, int cy);
int ai_contact_enqueue_demand_amount_choice( ColonizeTurnContext* ctx, int e, int nation_id, ColonizeUnit* other, int near_x, int near_y );
int ai_contact_enqueue_gift_amount_choice( ColonizeTurnContext* ctx, int e, int nation_id );
int ai_contact_enqueue_incite_target_choice( ColonizeTurnContext* ctx, int e, int nation_id, int is_missionary, int is_capital );
int ai_contact_enter_hostile_village( ColonizeTurnContext* ctx, int e, int nation_id, ColonizeUnit* u );
int ai_contact_euro_is_human(const ColonizeTurnContext* ctx, int e);
int ai_contact_is_petty_criminal(const ColonizeUnitPool* units, const ColonizeUnit* u);
int ai_contact_meet_economics_2154( ColonizeTurnContext* ctx, int indian_nation, const ColonizeCol1Tribe* tribe, AiContactMeetEcon2154* out );
int ai_contact_nearest_colony( const ColonizeTurnContext* ctx, int nation, int x, int y, int continent, int min_tools, int max_dist, int* out_dist );
int ai_contact_pair_friction( const ColonizeCol1Indian* ind, const ColonizeCol1Save* col1, int nation_id, int e );
int ai_contact_visit_demand_allowed( ColonizeTurnContext* ctx, const ColonizeCol1Indian* ind, const ColonizeCol1Tribe* t, int nation_id, int e, int brave_id, int alarm );
int ai_contact_welcome_pending(const AiPopupState* st, int e, int nation_id);
void ai_contact_apply_beg_food( ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, int colony_id, int home_tribe, int accept );
void ai_contact_apply_buy0( ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, int payload, int choice );
void ai_contact_apply_buywhich( ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, int unit_id, int cargo );
void ai_contact_apply_gift_gold( ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, unsigned gold_cost, int friction_decay );
void ai_contact_apply_incite( ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, int target, int is_missionary, int is_capital );
void ai_contact_apply_raid_loot( ColonizeTurnContext* ctx, ColonizeColony* c, int target_euro, AiRaidKind kind, int max_alarm );
void ai_contact_apply_reparations( ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, int flavor, int accept );
void ai_contact_apply_trade_offer( ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, int price, int choice );
void ai_contact_apply_trade_pick( ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, int unit_id, int choice );
void ai_contact_apply_welcome_accept( ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e );
void ai_contact_apply_welcome_reject( ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e );
void ai_contact_bind_names(const ColonizeTurnContext* ctx);
void ai_contact_chief_flair(ColonizeTurnContext* ctx, int e, int nation_b);
void ai_contact_clamp_alarms(ColonizeCol1Indian* ind);
void ai_contact_clear_peace(ColonizeCol1Save* col1, int indian_nation, int euro_nation);
void ai_contact_enqueue_incite_confirm( ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, int target, int is_missionary, int is_capital );
void ai_contact_enqueue_village_meet( ColonizeTurnContext* ctx, int e, int nation_id, int is_missionary, int is_capital, int unit_id, int tribe_index );
void ai_contact_gift_or_demand( ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, ColonizeUnit* other, int near_x, int near_y );
void ai_contact_human_chrome( ColonizeTurnContext* ctx, int e, AiPopupTag tag, int nation_b, const char* title, const char* body );
void ai_contact_live_among_natives( ColonizeTurnContext* ctx, int e, int nation_id, ColonizeUnit* u, ColonizeCol1Tribe* t );
void ai_contact_local_rng(ColonizeTurnContext* ctx, int nation_id, ColonizeDosRng* out);
void ai_contact_mark_visit_brave( const ColonizeTurnContext* ctx, int nation_id, int brave_id );
void ai_contact_refuse_chrome( ColonizeTurnContext* ctx, int e, int nation_id, AiPopupTag tag, const char* title, const char* what );
void ai_contact_set_status(ColonizeTurnContext* ctx, const char* msg);
void ai_contact_try_village_reparations(ColonizeTurnContext* ctx, int nation_id);
void ai_contact_visit_mood_clear(int nation_id, int e);
void ai_contact_visit_mood_publish( const ColonizeTurnContext* ctx, int nation_id, int e, int brave_id, int bvar6 );

#endif /* COLONIZE_CORE_AI_CONTACT_INTERNAL_H */
