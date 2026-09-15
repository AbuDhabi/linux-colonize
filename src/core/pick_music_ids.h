#ifndef COLONIZE_PICK_MUSIC_IDS_H
#define COLONIZE_PICK_MUSIC_IDS_H

/*
 * Song id tables in GAME.TXT list order for each Pick Music section, from
 * the DOS Pick Music handler (2b5a:264c jump table + sublist offsets 0x28 /
 * 0x2d / 0x31, with the Indian list skipping 0x34). Verified against DOSBox
 * captures of Jine the Cavalry (0x25) and Hole In The Wall (0x3a). Shared by
 * pick_music.c and tools/dump_gsound_wav.c so the two can never disagree
 * again (the tool used to carry a shifted "0x20+n" table).
 */
static const int k_pick_music_main_song_ids[] = {
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x39, 0x38, 0x3a, 0x3b
};
static const int k_pick_music_independence_song_ids[] = {0x29, 0x2a, 0x2b, 0x2c, 0x2d};
static const int k_pick_music_military_song_ids[] = {0x2e, 0x2f, 0x30, 0x31};
static const int k_pick_music_indian_song_ids[] = {0x32, 0x33, 0x35, 0x36};

#define PICK_MUSIC_IDS_COUNT(tbl) ((int)(sizeof(tbl) / sizeof((tbl)[0])))

#endif
