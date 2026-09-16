/*
 * Anchor translation unit for the colonize_sim_linkcheck target (see
 * CMakeLists.txt). The probe links every colonize_sim object with
 * -Wl,--no-undefined, so a simulation file that calls into the UI layer
 * (fb_/font_/ss_/pik_/popup_/game_/reports rendering, ...) fails the build
 * with a named undefined reference instead of quietly entangling the layers.
 *
 * It intentionally contains no code of its own.
 */
int colonize_sim_linkcheck_anchor(void);
int colonize_sim_linkcheck_anchor(void) {
  return 0;
}
