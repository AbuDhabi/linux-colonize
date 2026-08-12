#include "core/ai_popup.h"

#include <stdio.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "smoke_ai_popup: %s\n", msg);
  return 1;
}

int main(void) {
  AiPopupState st;
  ai_popup_init(&st);

  if (ai_popup_busy(&st)) {
    return fail("fresh state should be idle");
  }
  if (!ai_popup_enqueue_ok(&st, AI_POPUP_TAG_INFO, "Title", "Hello body")) {
    return fail("enqueue_ok failed");
  }
  if (!ai_popup_queue_pending(&st) || st.queue_count != 1) {
    return fail("queue should have 1");
  }
  if (!ai_popup_try_present_next(&st) || !st.open) {
    return fail("present_next failed");
  }
  if (st.queue_count != 0) {
    return fail("queue should drain on present");
  }
  if (strcmp(st.current.body, "Hello body") != 0) {
    return fail("body mismatch");
  }
  if (st.current.choice_count != 0) {
    return fail("info dialog must not invent OK choice rows");
  }

  /* Body-only dismiss: Enter (any click / Esc / Space same path). */
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_ENTER;
  if (!ai_popup_handle_input(&st, &in)) {
    return fail("handle_input should consume");
  }
  if (st.open || !st.has_result || st.result_cancelled || st.result_choice_id != 0) {
    return fail("info dismiss result expected");
  }
  if (st.result_tag != AI_POPUP_TAG_INFO) {
    return fail("tag mismatch");
  }
  ai_popup_consume_result(&st);

  const char* labels[] = {"Accept", "Refuse"};
  const int ids[] = {1, 2};
  if (!ai_popup_enqueue_choice_ctx(
        &st,
        AI_POPUP_TAG_KING_AUDIENCE,
        0,
        1,
        42,
        "Audience",
        "The King demands higher taxes.",
        labels,
        ids,
        2
      )) {
    return fail("enqueue_choice failed");
  }
  ai_popup_try_present_next(&st);
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_DOWN;
  ai_popup_handle_input(&st, &in);
  in.last_key = COLONIZE_KEY_ENTER;
  ai_popup_handle_input(&st, &in);
  if (!st.has_result || st.result_choice_id != 2 || st.result_payload != 42) {
    return fail("Refuse choice result expected");
  }
  ai_popup_consume_result(&st);

  /* Queue overflow safety: fill then one more fails. */
  ai_popup_clear(&st);
  for (int i = 0; i < AI_POPUP_QUEUE_MAX; ++i) {
    if (!ai_popup_enqueue_ok(&st, AI_POPUP_TAG_INFO, NULL, "x")) {
      return fail("fill queue");
    }
  }
  if (ai_popup_enqueue_ok(&st, AI_POPUP_TAG_INFO, NULL, "overflow")) {
    return fail("overflow should fail");
  }

  /* Presentation tags: FA / King letter / Congress FF (chrome only). */
  ai_popup_clear(&st);
  if (!ai_popup_enqueue_ok_ctx(
        &st, AI_POPUP_TAG_DIPLO_FA, 0, 1, 0, "Foreign Affairs", "Alliance holds.")) {
    return fail("enqueue DIPLO_FA");
  }
  if (!ai_popup_enqueue_ok_ctx(
        &st, AI_POPUP_TAG_KING_LETTER, 0, 1, 0, "United Colonies", "Renamed.")) {
    return fail("enqueue KING_LETTER");
  }
  if (!ai_popup_enqueue_ok_ctx(
        &st, AI_POPUP_TAG_FF_CONGRESS, 0, -1, 3, "Continental Congress", "FF joins.")) {
    return fail("enqueue FF_CONGRESS");
  }
  if (st.queue_count != 3) {
    return fail("chrome tags queue count");
  }
  if (st.queue[0].tag != AI_POPUP_TAG_DIPLO_FA ||
      st.queue[1].tag != AI_POPUP_TAG_KING_LETTER ||
      st.queue[2].tag != AI_POPUP_TAG_FF_CONGRESS) {
    return fail("chrome tag values");
  }

  fprintf(stderr, "smoke_ai_popup: ok\n");
  return 0;
}
