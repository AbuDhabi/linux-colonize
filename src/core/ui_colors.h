#ifndef COLONIZE_UI_COLORS_H
#define COLONIZE_UI_COLORS_H

/*
 * Text / dialog colors from NAMES.TXT @COLORS (also baked into VICEROY.EXE):
 *   basic, hilite, grey, enhance, shadow, select, border0, border1, border2
 *   68,    149,    8,    128,     47,     138,    134,     128,     138
 *
 * basic  — normal dialog / menu / Colonizopedia link text (index under WOODPANL /
 *          in-game palettes; OPENMENU remaps by RGB — see game_loop menu_col_*)
 * hilite — {braced} emphasis (and hover highlight)
 * select — selection fill / chrome
 */
#define COLONIZE_COL_BASIC 68
#define COLONIZE_COL_HILITE 149
#define COLONIZE_COL_GREY 8
#define COLONIZE_COL_ENHANCE 128
#define COLONIZE_COL_SHADOW 47
#define COLONIZE_COL_SELECT 138
#define COLONIZE_COL_BORDER0 134
#define COLONIZE_COL_BORDER1 128
#define COLONIZE_COL_BORDER2 138

#endif
