#include <SDL2/SDL_keycode.h>

#include "platform/platform.h"

/* SDL keycode -> game key. Split out of sdl_runtime.c so the input-replay
 * regression harness (tests/regression) folds logged key_down records through
 * the same table the live poll uses. */
ColonizeKey platform_key_from_sdl(int32_t key) {
  switch (key) {
    case SDLK_ESCAPE: return COLONIZE_KEY_ESCAPE;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
      return COLONIZE_KEY_ENTER;
    case SDLK_SPACE: return COLONIZE_KEY_SPACE;
    case SDLK_UP: return COLONIZE_KEY_UP;
    case SDLK_DOWN: return COLONIZE_KEY_DOWN;
    case SDLK_LEFT: return COLONIZE_KEY_LEFT;
    case SDLK_RIGHT: return COLONIZE_KEY_RIGHT;
    case SDLK_KP_1: return COLONIZE_KEY_KP1;
    case SDLK_KP_2: return COLONIZE_KEY_KP2;
    case SDLK_KP_3: return COLONIZE_KEY_KP3;
    case SDLK_KP_4: return COLONIZE_KEY_KP4;
    case SDLK_KP_6: return COLONIZE_KEY_KP6;
    case SDLK_KP_7: return COLONIZE_KEY_KP7;
    case SDLK_KP_8: return COLONIZE_KEY_KP8;
    case SDLK_KP_9: return COLONIZE_KEY_KP9;
    case SDLK_s: return COLONIZE_KEY_S;
    case SDLK_f: return COLONIZE_KEY_F;
    case SDLK_l: return COLONIZE_KEY_L;
    case SDLK_q: return COLONIZE_KEY_Q;
    case SDLK_p: return COLONIZE_KEY_P;
    case SDLK_e: return COLONIZE_KEY_E;
    case SDLK_r: return COLONIZE_KEY_R;
    case SDLK_t: return COLONIZE_KEY_T;
    case SDLK_d: return COLONIZE_KEY_D;
    case SDLK_b: return COLONIZE_KEY_B;
    case SDLK_c: return COLONIZE_KEY_C;
    case SDLK_h: return COLONIZE_KEY_H;
    case SDLK_o: return COLONIZE_KEY_O;
    case SDLK_u: return COLONIZE_KEY_U;
    case SDLK_w: return COLONIZE_KEY_W;
    case SDLK_i: return COLONIZE_KEY_I;
    case SDLK_n: return COLONIZE_KEY_N;
    case SDLK_a: return COLONIZE_KEY_A;
    case SDLK_g: return COLONIZE_KEY_G;
    case SDLK_v: return COLONIZE_KEY_V;
    case SDLK_m: return COLONIZE_KEY_M;
    case SDLK_x: return COLONIZE_KEY_X;
    case SDLK_z: return COLONIZE_KEY_Z;
    case SDLK_LEFTBRACKET: return COLONIZE_KEY_LEFTBRACKET;
    case SDLK_RIGHTBRACKET: return COLONIZE_KEY_RIGHTBRACKET;
    case SDLK_BACKQUOTE: return COLONIZE_KEY_TILDE;
    case SDLK_F1: return COLONIZE_KEY_F1;
    case SDLK_F2: return COLONIZE_KEY_F2;
    case SDLK_F3: return COLONIZE_KEY_F3;
    case SDLK_F4: return COLONIZE_KEY_F4;
    case SDLK_F5: return COLONIZE_KEY_F5;
    case SDLK_F6: return COLONIZE_KEY_F6;
    case SDLK_F7: return COLONIZE_KEY_F7;
    case SDLK_F8: return COLONIZE_KEY_F8;
    case SDLK_F9: return COLONIZE_KEY_F9;
    case SDLK_F10: return COLONIZE_KEY_F10;
    case SDLK_BACKSPACE: return COLONIZE_KEY_BACKSPACE;
    case SDLK_DELETE: return COLONIZE_KEY_DELETE;
    case SDLK_HOME: return COLONIZE_KEY_HOME;
    case SDLK_END: return COLONIZE_KEY_END;
    default: return COLONIZE_KEY_NONE;
  }
}
