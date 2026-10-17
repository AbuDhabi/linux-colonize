/*
 * DOS VR_SEED.EXE (seed 100) idle-campaign autosaves. Each invocation starts
 * from the first save in one uninterrupted yearly run, then advances the
 * headless game one skipped human turn at a time. The whole serialized save
 * must match DOS after every turn, except the fields compare_save ignores.
 * CTest runs it continuously from year_1492 to year_1599
 * (GOLDEN_IDLE_KEEP_GOING); docs/idle_campaign.md has the status.
 *
 * With a leading "independence" argument it runs the Dutch every-turn
 * autosaves in original_saves/independence-seed-100 instead (save numbers
 * 0..20, 1680-1690; 0 = START_SAVE.SAV, the manual save the player loaded).
 * The player declares independence between saves 2 and 3 (menu GAME >
 * DECLARE INDEPENDENCE, second option) and otherwise idles the same way.
 * Its CTest case is disabled (the archive is one wall-clock realisation of
 * the music picks); the "tr9" argument runs the same chain against the
 * recorded DOSBox reference run, CTest golden_idle_tr9
 * (docs/idle_independence.md).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_king.h"
#include "core/dos_rng.h"
#include "tests/common/golden_fixture.h"

#define VR_SEED 100u

/* One uninterrupted DOS autosave series. Saves are named by year (yearly
 * campaign) or by save number (every-turn campaign). */
typedef struct {
  const char* path_fmt;
  const char* start_path; /* save index `min`, when named differently; NULL = path_fmt */
  int min, max;
  int per_turn;      /* save index counts turns, not years */
  int declare_save;  /* the human declares independence on the turn before this save; 0 = never */
  int resume_slot;   /* start save loaded in-game: the old day runs on from this slot; -1 = none */
  const char* picks_trace; /* DOSBox draw trace with `PICK save K draw D` lines; NULL = none */
} IdleCampaign;

static const IdleCampaign k_yearly = {
  "original_saves/1492-1600-seed-100/year_%d.sav", NULL, 1492, 1599, 0, 0, -1, NULL,
};
static const IdleCampaign k_independence = {
  "original_saves/independence-seed-100/%06d.SAV",
  /* START_SAVE was loaded from the in-game menu of a French (slot 1) game:
   * the running day finishes from slot 2 (docs/idle_independence.md). */
  "original_saves/independence-seed-100/START_SAVE.SAV", 0, 20, 1, 3, 2, NULL,
};
/* The same chain against the tr9 DOSBox reference run (continuous from
 * 000003, draw-traced; original_saves/independence-seed-100/tr9/readme.md).
 * Saves 1..3 have no tr9 file and are played through; its music picks come
 * from the trace. */
static const IdleCampaign k_tr9 = {
  "original_saves/independence-seed-100/tr9/tr9_%d.sav",
  "original_saves/independence-seed-100/START_SAVE.SAV", 0, 20, 1, 3, 2,
  "original_saves/independence-seed-100/tr9/tr9.txt",
};

static int read_bytes(const char* path, uint8_t** data, size_t* size) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "cannot open %s\n", path);
    return 0;
  }
  if (fseek(f, 0, SEEK_END) != 0) goto fail;
  long len = ftell(f);
  if (len < 0 || fseek(f, 0, SEEK_SET) != 0) goto fail;
  *data = malloc((size_t)len ? (size_t)len : 1);
  if (!*data) goto fail;
  *size = (size_t)len;
  if (fread(*data, 1, *size, f) != *size) {
    free(*data);
    *data = NULL;
    goto fail;
  }
  fclose(f);
  return 1;
fail:
  fprintf(stderr, "cannot read %s\n", path);
  fclose(f);
  return 0;
}

/*
 * Archived years whose DOS save carries unsaved DOS state the port cannot
 * reach (docs/idle_campaign.md "1505/1506"): simulated through, not compared.
 * The continuous run matches again from 1508.
 */
static int known_residue_year(const IdleCampaign* c, int year) {
  return c == &k_yearly && (year == 1505 || year == 1506);
}

/*
 * Ignored when comparing: the human's first-turn UI state, which the headless
 * harness never plays (View mode, tutorial flags, map focus tile), the
 * port-only COLNXEXT tail past the DOS sections, and the port's WoI latches
 * kept in the human's DOS-dead nation bytes (ai_king_latch_pad,
 * unknown23_pad). They are copied from the DOS save into the serialized copy
 * only; the simulated state keeps its own.
 */
static int compare_save(
  ColonizeCol1Save* got, const ColonizeCol1Save* expect, const char* expected_path, int year
) {
  uint8_t *actual = NULL, *expected = NULL;
  size_t actual_size = 0, expected_size = 0;
  char err[256];
  int ok = 0;
  const uint16_t keep_map_mode = got->head.map_mode;
  const ColonizeCol1Tut2 keep_tut2 = got->head.tut2;
  const uint16_t keep_x = got->stuff.x, keep_y = got->stuff.y;
  const uint16_t keep_viewport_x = got->stuff.viewport_x;
  const uint16_t keep_viewport_y = got->stuff.viewport_y;
  uint8_t keep_pad[4][sizeof(got->nation[0].unknown23_pad)];
  for (int n = 0; n < 4; ++n) {
    memcpy(keep_pad[n], got->nation[n].unknown23_pad, sizeof(keep_pad[n]));
  }
  if (expect) {
    got->head.map_mode = expect->head.map_mode;
    got->head.tut2 = expect->head.tut2;
    got->stuff.x = expect->stuff.x;
    got->stuff.y = expect->stuff.y;
    got->stuff.viewport_x = expect->stuff.viewport_x;
    got->stuff.viewport_y = expect->stuff.viewport_y;
    for (int n = 0; n < 4; ++n) {
      memcpy(got->nation[n].unknown23_pad, expect->nation[n].unknown23_pad, sizeof(keep_pad[n]));
    }
  }
  const int written = col1_save_write_memory(got, &actual, &actual_size, err, sizeof(err));
  got->head.map_mode = keep_map_mode;
  got->head.tut2 = keep_tut2;
  got->stuff.x = keep_x;
  got->stuff.y = keep_y;
  got->stuff.viewport_x = keep_viewport_x;
  got->stuff.viewport_y = keep_viewport_y;
  for (int n = 0; n < 4; ++n) {
    memcpy(got->nation[n].unknown23_pad, keep_pad[n], sizeof(keep_pad[n]));
  }
  if (!written) {
    fprintf(stderr, "serialize %d: %s\n", year, err);
    goto done;
  }
  /* No archived save for this year: only dump. */
  if (!expected_path) goto dump;
  if (!read_bytes(expected_path, &expected, &expected_size)) goto done;
  if (actual_size > expected_size + 8 && memcmp(actual + expected_size, "COLNXEXT", 8) == 0) {
    actual_size = expected_size;
  }
  const size_t common = actual_size < expected_size ? actual_size : expected_size;
  size_t offset = 0;
  while (offset < common && actual[offset] == expected[offset]) ++offset;
  if (offset == common && actual_size == expected_size) {
    ok = 1;
  } else if (offset < common) {
    fprintf(stderr, "%s: first byte mismatch at 0x%zx: got %02x, DOS %02x\n",
            expected_path, offset, actual[offset], expected[offset]);
  } else {
    fprintf(stderr, "%s: size mismatch: got %zu bytes, DOS %zu bytes\n",
            expected_path, actual_size, expected_size);
  }
dump:
  if (!ok || getenv("GOLDEN_IDLE_DUMP_ALL")) {
    /* A "%d" in GOLDEN_IDLE_DUMP takes the year (one file per KEEP_GOING turn). */
    const char* dump_env = getenv("GOLDEN_IDLE_DUMP");
    char dump[512];
    if (dump_env && *dump_env) {
      const char* pct = strstr(dump_env, "%d");
      if (pct) {
        snprintf(dump, sizeof(dump), "%.*s%d%s", (int)(pct - dump_env), dump_env, year, pct + 2);
      } else {
        snprintf(dump, sizeof(dump), "%s", dump_env);
      }
      FILE* f = fopen(dump, "wb");
      int written = f && fwrite(actual, 1, actual_size, f) == actual_size;
      if (f && fclose(f) != 0) written = 0;
      if (written) {
        fprintf(stderr, "wrote simulated save to %s\n", dump);
      } else {
        fprintf(stderr, "could not write simulated save to %s\n", dump);
      }
    }
  }
done:
  free(actual);
  free(expected);
  return ok;
}

static int parse_year(const IdleCampaign* c, const char* text, int* year) {
  char* end;
  long value = strtol(text, &end, 10);
  if (*text == '\0' || *end != '\0' || value < c->min || value > c->max) return 0;
  *year = (int)value;
  return 1;
}

static void save_path(const IdleCampaign* c, char* path, size_t size, int year) {
  if (year == c->min && c->start_path) {
    snprintf(path, size, "%s", c->start_path);
  } else {
    snprintf(path, size, c->path_fmt, year);
  }
}

int main(int argc, char** argv) {
  const IdleCampaign* c = &k_yearly;
  if (argc == 4 && strcmp(argv[1], "independence") == 0) {
    c = &k_independence;
    --argc;
    ++argv;
  } else if (argc == 4 && strcmp(argv[1], "tr9") == 0) {
    c = &k_tr9;
    --argc;
    ++argv;
  }
  int first, last;
  if (argc != 3 || !parse_year(c, argv[1], &first) ||
      !parse_year(c, argv[2], &last) || first >= last) {
    fprintf(stderr,
            "usage: %s [independence|tr9] FIRST LAST (one contiguous saved run;"
            " years, or save numbers for independence)\n", argv[0]);
    return 2;
  }

  char from[512], expected[128];
  save_path(c, from, sizeof(from), first);
  /* GOLDEN_IDLE_FROM: start from this save instead of year_FIRST.sav (e.g. a
   * DOSBox from-load autosave). */
  const char* from_env = getenv("GOLDEN_IDLE_FROM");
  if (from_env && *from_env) snprintf(from, sizeof(from), "%s", from_env);
  /* The fixture wants an expected save; the loop reloads it per year. */
  GoldenFixture fx;
  if (!golden_open(from, from, VR_SEED, &fx)) {
    golden_close(&fx);
    return 1;
  }
  dos_rng_set_tick_word(VR_SEED); /* VR_SEED's music-pick tick */
  int ok = 1;
  if (!c->per_turn && fx.orig.head.year != first) {
    fprintf(stderr, "save filename/calendar mismatch near %d\n", first);
    ok = 0;
  }
  /* GOLDEN_IDLE_KEEP_GOING: keep simulating past a mismatch (multi-turn
   * from-load DOSBox targets, or one continuous run from year_1492 through
   * the archive's missing years; GOLDEN_IDLE_DUMP may take a "%d"). */
  const char* keep = getenv("GOLDEN_IDLE_KEEP_GOING");
  const int keep_going = keep && *keep;
  for (int year = first + 1; (ok || keep_going) && year <= last; ++year) {
    save_path(c, expected, sizeof(expected), year);
    int have = 1;
    {
      char err[256];
      col1_save_free(&fx.expect);
      col1_save_init(&fx.expect);
      if (!col1_save_read_file(expected, &fx.expect, err, sizeof(err))) {
        /* The archive skips some years; a continuous run plays through them. */
        if (keep_going) {
          have = 0;
        } else {
          fprintf(stderr, "read %s: %s\n", expected, err);
          ok = 0;
          break;
        }
      }
    }
    /* Every-turn saves skip a turn now and then (000020 follows turn 287
     * with 289): play through the gap with the same idle human. */
    const int steps = c->per_turn && have ? (int)fx.expect.head.turn - (int)fx.start.head.turn : 1;
    if (have && (c->per_turn ? steps < 1 || steps > 4 : fx.expect.head.year != year)) {
      fprintf(stderr, "%s records year %u turn %u\n", expected, fx.expect.head.year,
              fx.expect.head.turn);
      ok = 0;
      break;
    }
    if (year == c->declare_save) {
      /* Headless (no popup queue): the @DECLARE choice confirms, as the
       * player's second option did. */
      ColonizeTurnContext ctx;
      golden_ctx(&fx, &ctx);
      /* The turn's first unit comes up (FUN_1427_08ea) before the player
       * opens the menu; the declaration then spends every unit. */
      fx.units.selected_id = -1;
      if (turn_select_next_unit(&fx.units, fx.br.human_nation)) {
        turn_idle_human_activate(&fx.units, units_get(&fx.units, fx.units.selected_id));
      }
      ai_king_menu_declare_independence(&ctx);
      if (!ai_king_independence_declared(&fx.start)) {
        fprintf(stderr, "declare independence before %d failed\n", year);
        ok = 0;
        break;
      }
    }
    /* GOLDEN_IDLE_PICKS="5:120,900;6:42": music picks (dos_rng.h) recorded
     * for the run into save 5 / 6, as draw indices from that run's start. */
    unsigned long picks[16];
    int npicks = 0;
    {
      const char* pe = getenv("GOLDEN_IDLE_PICKS");
      while (pe && *pe) {
        char* end;
        const long sv = strtol(pe, &end, 10);
        if (*end != ':') break;
        pe = end + 1;
        while (*pe && *pe != ';') {
          const unsigned long ix = strtoul(pe, &end, 10);
          if (end == pe) break;
          if (sv == year && npicks < 16) picks[npicks++] = ix;
          pe = *end == ',' ? end + 1 : end;
        }
        if (*pe == ';') ++pe;
      }
      /* No env spec: the campaign's own trace (`PICK save K draw D`). */
      FILE* tf = (!pe && c->picks_trace) ? fopen(c->picks_trace, "r") : NULL;
      if (tf) {
        char line[160];
        while (fgets(line, sizeof(line), tf)) {
          int sv;
          unsigned long ix;
          if (sscanf(line, "PICK save %d draw %lu", &sv, &ix) == 2 && sv == year && npicks < 16) {
            picks[npicks++] = ix;
          }
        }
        fclose(tf);
      }
    }
    dos_rng_schedule_tick_reseeds(picks, npicks);
    int stepped = 1;
    if (year == first + 1 && first == c->min && c->resume_slot >= 0 &&
        !(from_env && *from_env)) {
      fx.units.selected_id = -1;
      stepped = golden_turn_from_slot(&fx, c->resume_slot);
    }
    for (int step = 0; stepped && step < steps; ++step) {
      /* The skipped human turn: FUN_2b5a_3b68 opens Move Pieces with
       * DS:0x5392 = 0xffff (raw 46833) and the idle player ends it there. */
      {
        /* The idle player's rotation: lasting orders run as each unit comes
         * up, Space spends the rest (turn_idle_human_rotation_w). */
        ColonizeTurnContext rctx;
        golden_ctx(&fx, &rctx);
        const ColonizeWorld rw = world_from_turn_ctx(&rctx);
        turn_idle_human_rotation_w(&rw, fx.br.human_nation);
      }
      fx.units.selected_id = -1;
      stepped = golden_turn(&fx);
      col1_bridge_stamp_autosave_head(&fx.start);
    }
    fprintf(stderr, "save %d: %lu draws\n", year, dos_rng_draw_count());
    dos_rng_schedule_tick_reseeds(NULL, 0);
    if (!stepped) {
      ok = 0;
      break;
    }
    /* GOLDEN_IDLE_RESYNC="5": after the run into that save, take the AI
     * nations' gold and trade ledgers from the archive. For realisation
     * residue a DOSBox replay does not reproduce (independence 000005: the
     * crown's +36 Tools gross), so the later saves can still be compared. */
    {
      const char* rs = getenv("GOLDEN_IDLE_RESYNC");
      for (const char* q = rs; have && q && *q;) {
        char* end;
        const long sv = strtol(q, &end, 10);
        if (end == q) break;
        if (sv == year) {
          for (int n = 0; n < 4; ++n) {
            if (n == (int)fx.br.human_nation) continue;
            fx.start.nation[n].gold = fx.expect.nation[n].gold;
            memcpy(fx.start.nation[n].trade.gold, fx.expect.nation[n].trade.gold,
                   sizeof(fx.start.nation[n].trade.gold));
          }
        }
        q = *end ? end + 1 : end;
      }
    }
    if (!have || known_residue_year(c, year)) {
      compare_save(&fx.start, NULL, NULL, year);
      continue;
    }
    if (!compare_save(&fx.start, &fx.expect, expected, year)) {
      ok = 0;
      if (keep_going) continue;
      break;
    }
    printf("%d -> %d byte-identical\n", year - 1, year);
  }
  golden_close(&fx);
  return ok ? 0 : 1;
}
