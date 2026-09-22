cmake_minimum_required(VERSION 3.20)
project(colonize_linux C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(COLONIZE_ENABLE_AUDIO "Enable SDL2 audio backend" ON)
option(COLONIZE_STATIC_DEPS "Link SDL2/FluidSynth (and their deps) statically for release" OFF)
option(COLONIZE_DEBUG_MENU "Include DEBUG map menu (sprite viewer, mouse coords, debug logs)" ON)
option(COLONIZE_ENABLE_TSF "Enable bundled TinySoundFont MIDI backend (fallback when FluidSynth is absent)" ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(SDL2_PC QUIET sdl2)
    if(SDL2_PC_FOUND)
      add_library(SDL2::SDL2 INTERFACE IMPORTED)
      target_include_directories(SDL2::SDL2 INTERFACE ${SDL2_PC_INCLUDE_DIRS})
      if(COLONIZE_STATIC_DEPS)
        # Static archive + its full dependency closure. Link flags like
        # -mwindows are deliberately not taken: the exe stays a console-
        # subsystem binary with a plain main() (no SDL2main/WinMain).
        target_link_directories(SDL2::SDL2 INTERFACE ${SDL2_PC_STATIC_LIBRARY_DIRS})
        target_link_libraries(SDL2::SDL2 INTERFACE ${SDL2_PC_STATIC_LINK_LIBRARIES})
      else()
        target_link_libraries(SDL2::SDL2 INTERFACE ${SDL2_PC_LINK_LIBRARIES})
      endif()
      set(SDL2_FOUND ON)
    endif()
  endif()
endif()

# Static release builds link SDL2's static archive (SDL still dlopens
# X11/Wayland/audio from the host at runtime, so portability is unaffected).
set(COLONIZE_SDL2_TARGET SDL2::SDL2)
if(COLONIZE_STATIC_DEPS AND TARGET SDL2::SDL2-static)
  set(COLONIZE_SDL2_TARGET SDL2::SDL2-static)
endif()

find_package(PkgConfig QUIET)
set(COLONIZE_HAS_FLUIDSYNTH OFF)
if(PkgConfig_FOUND)
  pkg_check_modules(FLUIDSYNTH QUIET fluidsynth)
  if(FLUIDSYNTH_FOUND)
    set(COLONIZE_HAS_FLUIDSYNTH ON)
  endif()
endif()

# ---------------------------------------------------------------------------
# Layering: colonize_sim (simulation + shared) and colonize_ui (presentation).
#
# The port keeps UI paint in core (docs/architecture.md), but the *direction*
# of dependency is now enforced by the linker: colonize_ui links colonize_sim,
# never the reverse. A DOS-logic port that grows a fb_/font_/ss_/pik_/popup_
# side effect fails the colonize_sim_linkcheck target (below) rather than
# silently entangling the two halves.
#
# Sim -> UI traffic goes through the existing channels only: ai_popup (the
# popup queue), the units_set_* / combat_analysis_set_presenter / woodcut_set_
# sound_hooks callback pointers the UI registers at game_create, and diag_*.
#
# SHARED (no UI, no game rules) lives in colonize_sim: assets, strutil,
# settings, json_min, ff/madspack decode, sound, diagnostics, dos_compat,
# popup_msg (GAME.TXT text templating - measurement/markup only, never paint)
# and ai_popup (the sim->UI message channel).
# ---------------------------------------------------------------------------

add_library(colonize_sim STATIC
  # --- Simulation -----------------------------------------------------------
  src/core/map.c
  src/core/map_gen.c
  src/core/dos_rng.c
  src/core/units.c
  src/core/unit_chrome.c
  src/core/village_trade_intel.c
  src/core/colony.c
  src/core/colony_yield.c
  src/core/colony_production.c
  src/core/colony_craft.c
  src/core/combat_strength.c
  src/core/combat_analysis.c
  src/core/turn.c
  src/core/founding_fathers.c
  src/core/europe.c
  src/core/new_game_scenario.c
  src/core/reports_names.c
  src/core/woodcut.c
  src/core/ai.c
  src/core/ai_goals.c
  src/core/ai_euro.c
  src/core/ai_diplo.c
  src/core/ai_contact.c
  src/core/ai_king.c
  src/core/savegame.c
  src/core/col1_save.c
  src/core/col1_bridge.c
  src/core/col1_post_map.c
  src/core/col1_stuff_census.c
  # --- Shared ---------------------------------------------------------------
  src/core/ai_popup.c
  src/core/popup_msg.c
  src/core/assets.c
  src/core/strutil.c
  src/core/ff.c
  src/core/madspack.c
  src/core/sound.c
  src/core/gsound_vm.c
  src/core/json_min.c
  src/core/settings.c
  src/data/viceroy_tables.c
  src/platform/diagnostics.c
  src/platform/dos_compat/dos_compat.c
)

add_library(colonize_ui STATIC
  src/core/game_loop.c
  src/core/game_dialogs.c
  src/core/fb.c
  src/core/font.c
  src/core/pik.c
  src/core/ss.c
  src/core/popup.c
  src/core/ui_button.c
  src/core/ui_drag.c
  src/core/text_edit.c
  src/core/map_menu.c
  src/core/map_panel.c
  src/core/pick_music.c
  src/core/cheat_list_dialog.c
  src/core/howmuch_dialog.c
  src/core/name_entry_dialog.c
  src/core/options_dialog.c
  src/core/save_load_dialog.c
  src/core/unit_stack.c
  src/core/unit_chrome_draw.c
  src/core/units_render.c
  src/core/new_game.c
  src/core/pedia.c
  src/core/reports.c
  src/core/europe_art.c
  src/core/colony_screen.c
  src/core/colony_chrome.c
  src/core/colony_preview.c
  src/core/combat_analysis_render.c
  src/core/ai_popup_render.c
  src/core/trade_screen.c
  src/core/debug_atlas.c
  src/core/declaration.c
  src/core/closing.c
  src/core/opening.c
  src/core/woodcut_present.c
)

target_link_libraries(colonize_ui PUBLIC colonize_sim)

# Kept so every existing consumer (colonize_linux, all tests/tools) can go on
# linking one name. INTERFACE, so it contributes no objects of its own.
add_library(colonize_core INTERFACE)
target_link_libraries(colonize_core INTERFACE colonize_ui)

# Usage requirements ride on colonize_sim and propagate through colonize_ui's
# PUBLIC link to colonize_core and every consumer, so there is still exactly
# one place to edit.
target_include_directories(colonize_sim PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(colonize_sim PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)
target_compile_options(colonize_ui PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

target_link_libraries(colonize_sim PUBLIC m pthread)

# colonize_sim objects are reused by the colonize_sim_linkcheck shared probe
# at the bottom of this section, which needs PIC.
set_target_properties(colonize_sim PROPERTIES POSITION_INDEPENDENT_CODE ON)

# COLONIZE_TESTING flips the COLONIZE_INTERNAL macro (core/internal.h) from
# `static` to nothing on the split-stage functions (ai_euro_act_*, game_update_*,
# game_render_*, game_move_*, ai_contact_raid_*, ai_king_0982_*, ai_021a_*,
# turn_step_*, turn_year_end_*), so tests/unit/test_stage_seams.c can call one
# stage directly. colonize_sim is built exactly once and linked by every test
# binary (see colonize_add_test below) — there is no separate "test" build of
# the core lib to scope this to — so the define is applied to the one
# colonize_sim target unconditionally. It only widens symbol linkage; no
# stage function's body or call sites change, so this has no effect on
# program behaviour. See docs/conventions.md "Code conventions".
target_compile_definitions(colonize_sim PUBLIC COLONIZE_TESTING=1)

if(COLONIZE_DEBUG_MENU)
  target_compile_definitions(colonize_sim PUBLIC COLONIZE_DEBUG_MENU=1)
else()
  target_compile_definitions(colonize_sim PUBLIC COLONIZE_DEBUG_MENU=0)
endif()

if(COLONIZE_HAS_FLUIDSYNTH)
  target_include_directories(colonize_sim PUBLIC ${FLUIDSYNTH_INCLUDE_DIRS})
  if(COLONIZE_STATIC_DEPS)
    # pkg-config --static gives -l names; -l would still pick the .so when both
    # exist, so resolve each dep to its .a explicitly. System runtime libs
    # (m/dl/rt/pthread) stay dynamic.
    set(_fs_libs "")
    foreach(_lib IN LISTS FLUIDSYNTH_STATIC_LIBRARIES)
      if(_lib MATCHES "^(m|dl|rt|pthread|c)$")
        list(APPEND _fs_libs ${_lib})
      else()
        find_library(COLONIZE_STATIC_${_lib} NAMES lib${_lib}.a
          PATHS ${FLUIDSYNTH_STATIC_LIBRARY_DIRS})
        if(COLONIZE_STATIC_${_lib})
          list(APPEND _fs_libs ${COLONIZE_STATIC_${_lib}})
        else()
          list(APPEND _fs_libs ${_lib})
        endif()
      endif()
    endforeach()
    target_link_libraries(colonize_sim PUBLIC ${_fs_libs})
  else()
    target_link_libraries(colonize_sim PUBLIC ${FLUIDSYNTH_LIBRARIES})
  endif()
  target_compile_definitions(colonize_sim PUBLIC COLONIZE_HAS_FLUIDSYNTH=1)
  message(STATUS "FluidSynth found — music backend enabled")
else()
  message(STATUS "FluidSynth not found — music uses square-wave fallback")
endif()

if(NOT COLONIZE_ENABLE_TSF)
  # sound.c defaults COLONIZE_HAS_TSF to 1; only the opt-out needs a define.
  target_compile_definitions(colonize_sim PUBLIC COLONIZE_HAS_TSF=0)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(colonize_sim PRIVATE -Wno-unused-parameter)
  target_compile_options(colonize_ui PRIVATE -Wno-unused-parameter)
endif()

# ---------------------------------------------------------------------------
# Layering enforcement. Static archives alone cannot make a sim -> UI call
# fail (the UI archive is on the link line anyway), so the direction is
# checked by re-linking the simulation objects ALONE into a shared object with
# -Wl,--no-undefined: every symbol must resolve inside colonize_sim or libc /
# libm / pthread / FluidSynth. One fb_/font_/ss_/pik_/popup_/game_ reference
# from a sim file and this fails with an undefined reference, by name.
#
# It is a normal ALL target, so `make build` / `make test` enforce it.
# ---------------------------------------------------------------------------
add_library(colonize_sim_linkcheck SHARED cmake/sim_linkcheck.c)
target_link_libraries(colonize_sim_linkcheck PRIVATE colonize_sim)
target_link_options(colonize_sim_linkcheck PRIVATE
  -Wl,--no-undefined
  -Wl,--whole-archive $<TARGET_FILE:colonize_sim> -Wl,--no-whole-archive
)
add_dependencies(colonize_sim_linkcheck colonize_sim)

if(SDL2_FOUND)
  add_executable(colonize_linux
    src/main.c
    src/platform/linux_sdl2/sdl_runtime.c
  )

  target_include_directories(colonize_linux PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(colonize_linux PRIVATE
    colonize_core
    ${COLONIZE_SDL2_TARGET}
  )

  if(COLONIZE_ENABLE_AUDIO)
    target_compile_definitions(colonize_linux PRIVATE COLONIZE_ENABLE_AUDIO=1)
  endif()

  # LICENSE always. Bundled SC-55 SoundFont is optional — copy only if present
  # so deleting it (or dropping it from the repo) does not fail the build.
  add_custom_command(
    TARGET colonize_linux POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/LICENSE
            $<TARGET_FILE_DIR:colonize_linux>/LICENSE
    COMMAND ${CMAKE_COMMAND}
            -DSRC=${CMAKE_SOURCE_DIR}/data/soundfonts/Roland_SC-55.sf2
            -DDST=$<TARGET_FILE_DIR:colonize_linux>/data/soundfonts/Roland_SC-55.sf2
            -P ${CMAKE_SOURCE_DIR}/cmake/copy_if_exists.cmake
    COMMAND ${CMAKE_COMMAND}
            -DSRC=${CMAKE_SOURCE_DIR}/data/soundfonts/COPYRIGHT.Roland_SC-55
            -DDST=$<TARGET_FILE_DIR:colonize_linux>/data/soundfonts/COPYRIGHT.Roland_SC-55
            -P ${CMAKE_SOURCE_DIR}/cmake/copy_if_exists.cmake
    COMMENT "Copy LICENSE and optional bundled soundfont next to colonize_linux"
  )
else()
  message(WARNING "SDL2 not found. colonize_linux target is disabled; build/install SDL2 dev packages to enable.")
endif()

enable_testing()

# ---------------------------------------------------------------------------
# colonize_add_test(NAME <name> SOURCES <src...>
#                    [LIBS <lib...>] [INCLUDE_DIRS <dir...>]
#                    [COMPILE_DEFS <def...>] [NO_CTEST] [SLIM])
#
# Uniform boilerplate for the test/tool executables below: add_executable +
# target_include_directories (always ${CMAKE_SOURCE_DIR}/src, plus any extra
# INCLUDE_DIRS) + target_link_libraries (if LIBS given) + target_compile_
# definitions (if COMPILE_DEFS given) + add_test with WORKING_DIRECTORY
# ${CMAKE_SOURCE_DIR} always set (unless NO_CTEST, for build-only tool
# executables that are not registered as tests).
#
# SLIM appends src/core/ai_contact_link_stubs.c to SOURCES and defines
# COLONIZE_SLIM_TEST=1 — see the comment on that file: it may ONLY be listed
# by targets that pass SLIM here.
# ---------------------------------------------------------------------------
function(colonize_add_test)
  set(options NO_CTEST SLIM)
  set(oneValueArgs NAME)
  set(multiValueArgs SOURCES LIBS INCLUDE_DIRS COMPILE_DEFS)
  cmake_parse_arguments(CAT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT CAT_NAME)
    message(FATAL_ERROR "colonize_add_test: NAME is required")
  endif()

  set(_sources ${CAT_SOURCES})
  # Fixture name -> id resolvers. The shipped binary has no such tables; tests
  # that hand-build pools out of names get them from here.
  list(APPEND _sources tests/common/test_name_kinds.c)
  if(CAT_SLIM)
    list(APPEND _sources src/core/ai_contact_link_stubs.c)
  endif()

  add_executable(${CAT_NAME} ${_sources})

  target_include_directories(${CAT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CAT_INCLUDE_DIRS}
  )

  if(CAT_LIBS)
    target_link_libraries(${CAT_NAME} PRIVATE ${CAT_LIBS})
  endif()

  set(_defs ${CAT_COMPILE_DEFS})
  if(CAT_SLIM)
    list(APPEND _defs COLONIZE_SLIM_TEST=1)
  endif()
  if(_defs)
    target_compile_definitions(${CAT_NAME} PRIVATE ${_defs})
  endif()

  if(NOT CAT_NO_CTEST)
    add_test(NAME ${CAT_NAME} COMMAND ${CAT_NAME} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  endif()
endfunction()

colonize_add_test(NAME unit_col1_save
  SOURCES tests/unit/test_col1_save.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_settings
  SOURCES tests/unit/test_settings.c
  LIBS colonize_core
)

colonize_add_test(NAME smoke_game_flow
  SOURCES
    tests/smoke/test_game_flow.c
    tests/common/platform_cursor_stubs.c
    tests/common/test_assets.c
  LIBS colonize_core
  INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)

colonize_add_test(NAME unit_new_game
  SOURCES
    tests/unit/test_new_game.c
    tests/common/platform_cursor_stubs.c
    tests/common/test_assets.c
  LIBS colonize_core
  INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)

colonize_add_test(NAME unit_hall_of_fame
  SOURCES
    tests/unit/test_hall_of_fame.c
    tests/common/platform_cursor_stubs.c
    tests/common/test_assets.c
  LIBS colonize_core
  INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)

colonize_add_test(NAME smoke_popup
  SOURCES
    tests/smoke/test_popup.c
    src/core/popup.c
    src/core/fb.c
    src/core/font.c
    src/core/ss.c
    src/core/madspack.c
    src/core/assets.c
    src/platform/diagnostics.c
    src/core/strutil.c
    src/platform/dos_compat/dos_compat.c
)

colonize_add_test(NAME smoke_ai_popup
  SOURCES tests/smoke/test_ai_popup.c
  LIBS colonize_core
)

colonize_add_test(NAME smoke_popup_dialogs
  SOURCES tests/smoke/test_popup_dialogs.c
  LIBS colonize_core
)

set(_fs_defs "")
if(COLONIZE_HAS_FLUIDSYNTH)
  set(_fs_defs COLONIZE_HAS_FLUIDSYNTH=1)
endif()

colonize_add_test(NAME smoke_pick_music
  SOURCES
    tests/smoke/test_pick_music.c
    src/core/pick_music.c
    src/core/popup.c
    src/core/sound.c
    src/core/gsound_vm.c
    src/core/fb.c
    src/core/font.c
    src/core/assets.c
    src/core/ss.c
    src/core/madspack.c
    src/platform/diagnostics.c
    src/core/strutil.c
    src/platform/dos_compat/dos_compat.c
  LIBS m pthread ${FLUIDSYNTH_LIBRARIES}
  INCLUDE_DIRS ${FLUIDSYNTH_INCLUDE_DIRS}
  COMPILE_DEFS ${_fs_defs}
)

colonize_add_test(NAME smoke_save_load_dialog
  SOURCES tests/smoke/test_save_load_dialog.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_map
  SOURCES
    tests/unit/test_map.c
    src/core/map.c
    src/data/viceroy_tables.c
    src/platform/diagnostics.c
    src/core/strutil.c
)

colonize_add_test(NAME unit_map_gen
  SOURCES tests/unit/test_map_gen.c
  LIBS colonize_core
)

colonize_add_test(NAME golden_mapgen_seed100
  SOURCES tests/golden/test_mapgen_seed100.c
  LIBS colonize_core
)

colonize_add_test(NAME sav_json
  SOURCES tools/sav_json_main.c tools/col1_json.c
  LIBS colonize_core
  INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tools
  NO_CTEST
)

colonize_add_test(NAME render_report
  SOURCES tools/render_report_main.c tools/render_common.c
  LIBS colonize_core
  INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  NO_CTEST
)

colonize_add_test(NAME render_map_panel
  SOURCES tools/render_map_panel_main.c tools/render_common.c
  LIBS colonize_core
  INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  NO_CTEST
)

colonize_add_test(NAME render_colony
  SOURCES tools/render_colony_main.c tools/render_common.c
  LIBS colonize_core
  INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  NO_CTEST
)

colonize_add_test(NAME smoke_viceroy_tables
  SOURCES tests/smoke/test_viceroy_tables.c src/data/viceroy_tables.c
)

colonize_add_test(NAME unit_pedia
  SOURCES tests/unit/test_pedia.c tests/common/platform_cursor_stubs.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_sound_gate
  SOURCES tests/unit/test_sound_gate.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_europe
  SOURCES tests/unit/test_europe.c tests/common/platform_cursor_stubs.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_colony_screen
  SOURCES tests/unit/test_colony_screen.c tests/common/platform_cursor_stubs.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_map_panel
  SOURCES tests/unit/test_map_panel.c
  LIBS colonize_core
)

# Shared source list for the four "slim" unit tests (duplication audit
# TT-16). They deliberately do NOT link colonize_core: they compile
# src/core/ai_contact_link_stubs.c (added automatically by the SLIM flag on
# colonize_add_test) instead of the real ai_contact/ai_euro/ai_king, and an
# archive would shadow those weak stubs. Each target adds its own test file
# plus the one extra source it needs.
set(COLONIZE_SLIM_SOURCES
  src/core/units.c
  src/core/units_render.c
  src/core/village_trade_intel.c
  src/core/reports.c
  src/core/reports_names.c
  src/core/colony_production.c
  src/core/colony_craft.c
  src/core/woodcut.c
  src/core/woodcut_present.c
  src/core/combat_strength.c
  src/core/combat_analysis.c
  src/core/combat_analysis_render.c
  src/core/unit_chrome.c
  src/core/unit_chrome_draw.c
  src/core/colony.c
  src/core/colony_chrome.c
  src/core/colony_yield.c
  src/core/founding_fathers.c
  src/core/ai_diplo.c
  src/core/ai_popup.c
  src/core/ai_popup_render.c
  src/core/popup.c
  src/core/popup_msg.c
  src/core/map_menu.c
  src/core/europe.c
  src/core/europe_art.c
  src/core/pik.c
  src/core/map.c
  src/core/dos_rng.c
  src/core/strutil.c
  src/core/assets.c
  src/core/fb.c
  src/core/font.c
  src/core/ff.c
  src/core/ss.c
  src/core/madspack.c
  src/data/viceroy_tables.c
  src/platform/diagnostics.c
  src/platform/dos_compat/dos_compat.c
)

colonize_add_test(NAME unit_units
  SOURCES tests/unit/test_units.c src/core/unit_stack.c ${COLONIZE_SLIM_SOURCES}
  SLIM
)

colonize_add_test(NAME unit_combat_strength
  SOURCES tests/unit/test_combat_strength.c src/core/unit_stack.c ${COLONIZE_SLIM_SOURCES}
  SLIM
)

colonize_add_test(NAME unit_colony_yield
  SOURCES
    tests/unit/test_colony_yield.c
    src/core/colony_yield.c
    src/core/reports_names.c
    src/core/assets.c
    src/core/map.c
    src/data/viceroy_tables.c
    src/platform/diagnostics.c
    src/platform/dos_compat/dos_compat.c
    src/core/strutil.c
)

colonize_add_test(NAME unit_colonies
  SOURCES tests/unit/test_colonies.c ${COLONIZE_SLIM_SOURCES}
  SLIM
)

colonize_add_test(NAME unit_reports
  SOURCES tests/unit/test_reports.c src/core/col1_save.c ${COLONIZE_SLIM_SOURCES}
  SLIM
)

colonize_add_test(NAME unit_text_edit
  SOURCES tests/unit/test_text_edit.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_map_menu
  SOURCES tests/unit/test_map_menu.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_turn
  SOURCES tests/unit/test_turn.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_ff
  SOURCES
    tests/unit/test_ff.c
    src/core/ff.c
    src/core/fb.c
    src/core/font.c
    src/core/madspack.c
    src/platform/diagnostics.c
    src/core/strutil.c
)

colonize_add_test(NAME unit_woodcut
  SOURCES
    tests/unit/test_woodcut.c
    src/core/woodcut.c
  src/core/woodcut_present.c
    src/core/ss.c
    src/core/fb.c
    src/core/font.c
    src/core/ff.c
    src/core/madspack.c
    src/core/assets.c
    src/platform/diagnostics.c
    src/core/strutil.c
    src/platform/dos_compat/dos_compat.c
)

colonize_add_test(NAME unit_declaration
  SOURCES
    tests/unit/test_declaration.c
    src/core/declaration.c
    src/core/ss.c
    src/core/pik.c
    src/core/madspack.c
    src/core/assets.c
    src/platform/diagnostics.c
    src/core/strutil.c
    src/platform/dos_compat/dos_compat.c
)

colonize_add_test(NAME unit_closing
  SOURCES
    tests/unit/test_closing.c
    src/core/closing.c
    src/core/ss.c
    src/core/pik.c
    src/core/madspack.c
    src/core/assets.c
    src/platform/diagnostics.c
    src/core/strutil.c
    src/platform/dos_compat/dos_compat.c
)

colonize_add_test(NAME unit_opening
  SOURCES
    tests/unit/test_opening.c
    src/core/opening.c
    src/core/ss.c
    src/core/pik.c
    src/core/madspack.c
    src/core/assets.c
    src/platform/diagnostics.c
    src/core/strutil.c
    src/platform/dos_compat/dos_compat.c
)

colonize_add_test(NAME smoke_ss
  SOURCES
    tests/smoke/test_ss.c
    src/core/ss.c
    src/core/madspack.c
    src/core/assets.c
    src/platform/diagnostics.c
    src/core/strutil.c
    src/platform/dos_compat/dos_compat.c
)

colonize_add_test(NAME smoke_assets
  SOURCES
    tests/smoke/test_assets.c
    src/core/assets.c
    src/platform/diagnostics.c
    src/core/strutil.c
    src/platform/dos_compat/dos_compat.c
)

colonize_add_test(NAME unit_popup_msg
  SOURCES
    tests/unit/test_popup_msg.c
    src/core/popup_msg.c
    src/core/assets.c
    src/core/strutil.c
    src/platform/diagnostics.c
    src/platform/dos_compat/dos_compat.c
)

colonize_add_test(NAME smoke_pik
  SOURCES
    tests/smoke/test_pik.c
    src/core/madspack.c
    src/core/pik.c
    src/core/assets.c
    src/platform/diagnostics.c
    src/core/strutil.c
    src/platform/dos_compat/dos_compat.c
)

colonize_add_test(NAME unit_ai
  SOURCES tests/unit/test_ai.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_ai_goals
  SOURCES tests/unit/test_ai_goals.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_ai_contact
  SOURCES tests/unit/test_ai_contact.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_ai_indian_census
  SOURCES tests/unit/test_ai_indian_census.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_ai_diplo
  SOURCES tests/unit/test_ai_diplo.c
  LIBS colonize_core
)

# RE-ENABLED 2026-08-20 (old AI queue T1.12, now docs/port_plan.md): the tax/audience blocks
# were rewritten against the real DOS RNG-driven favor-score formula
# (FUN_38fd_5be8/3dc8) — seeded RNG streams per scenario, turn-gated (not
# year-gated), apply-then-optionally-revert CHOICE shape. Was DISABLED
# 2026-08-19 (old AI-doc R6, git history) because the fixture predated
# this formula port; that gap is closed.
colonize_add_test(NAME unit_ai_king
  SOURCES tests/unit/test_ai_king.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_stage_seams
  SOURCES tests/unit/test_stage_seams.c tests/common/platform_cursor_stubs.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_ai_euro_5952_indoor
  SOURCES tests/unit/test_ai_euro_5952_indoor.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_ai_euro_5952_build
  SOURCES tests/unit/test_ai_euro_5952_build.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_ai_euro_5952_equip
  SOURCES tests/unit/test_ai_euro_5952_equip.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_ai_euro_5952_train
  SOURCES tests/unit/test_ai_euro_5952_train.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_ai_euro_5952_carpenter
  SOURCES tests/unit/test_ai_euro_5952_carpenter.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_ai_euro_5952_lumber
  SOURCES tests/unit/test_ai_euro_5952_lumber.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_ai_euro_expand
  SOURCES tests/unit/test_ai_euro_expand.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_ai_euro_war
  SOURCES tests/unit/test_ai_euro_war.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_ai_euro_20e6
  SOURCES tests/unit/test_ai_euro_20e6.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_ai_euro_20e6_ports
  SOURCES tests/unit/test_ai_euro_20e6_ports.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_ai_euro_5d04_hire
  SOURCES tests/unit/test_ai_euro_5d04_hire.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_ai_euro_28c8_job_score
  SOURCES tests/unit/test_ai_euro_28c8_job_score.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_founding_fathers
  SOURCES tests/unit/test_founding_fathers.c
  LIBS colonize_core
)

colonize_add_test(NAME unit_kill_indians
  SOURCES tests/unit/test_kill_indians.c
  LIBS colonize_core
)

colonize_add_test(NAME golden_ai_turns
  SOURCES tests/golden/test_ai_turns.c tests/common/golden_fixture.c
  LIBS colonize_core
  INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)
# T2 early gate: full Euro dispatcher (TURN1→7). The AI_EURO_EARLY_FIXTURE bisect fixture was deleted 2026-09-14.

# Renamed from golden_ai_* 2026-09-14 (duplication audit TT-13): both generate
# their own input fixture each run and assert only self-consistency, so the
# golden_ prefix wrongly implied a DOS-derived expectation.
colonize_add_test(NAME smoke_ai_mid01
  SOURCES tests/smoke/test_ai_mid01.c tests/common/joint_fixture.c
  LIBS colonize_core
  INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)

colonize_add_test(NAME smoke_ai_late01
  SOURCES tests/smoke/test_ai_late01.c tests/common/joint_fixture.c
  LIBS colonize_core
  INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)

# History: this AI cluster (golden_ai_turns / the two joint smoke tests /
# golden_ai_joint) was parked 2026-08-19 while the AI transcription was
# still structural. 2026-08-27 brought the joint pair back as regression
# gates, 2026-09-05 (T1.23, all six TURN steps green) golden_ai_turns and
# golden_ai_joint. Nothing here is DISABLED any more; there is no
# set_tests_properties call.

# Colony production golden vs real-DOS-run saves (COLONY00/01_no-transports.SAV):
colonize_add_test(NAME golden_colony_prod01
  SOURCES
    tests/golden/test_colony_prod01.c
    tests/common/col1_compare.c
    tests/common/golden_fixture.c
  LIBS colonize_core
  INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)

# Colony production preview == EOT warehouse delta (port_plan P4.10), same fixture:
colonize_add_test(NAME golden_colony_preview01
  SOURCES tests/golden/test_colony_preview01.c
  LIBS colonize_core
)

# FUN_38fd_0058 EOT market tick vs two real-DOS turn pairs (port_plan P6.1):
colonize_add_test(NAME golden_market_prices01
  SOURCES tests/golden/test_market_prices01.c
  LIBS colonize_core
)

# War of Independence: REF prosecutes the war on the real Dutch fixture (port_plan P5.1):
colonize_add_test(NAME golden_woi_ref01
  SOURCES tests/golden/test_woi_ref01.c
  LIBS colonize_core
)

# Colony production golden #2 vs real-DOS-run saves (COLONY00/01-dutch2-t0/t1.SAV):
colonize_add_test(NAME golden_colony_prod02
  SOURCES
    tests/golden/test_colony_prod02.c
    tests/common/col1_compare.c
    tests/common/golden_fixture.c
  LIBS colonize_core
  INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)

# Colony production golden #3: farmer skill/river arithmetic (farming/case1+case2 DOS pairs):
colonize_add_test(NAME golden_colony_prod03
  SOURCES
    tests/golden/test_colony_prod03.c
    tests/common/col1_compare.c
    tests/common/golden_fixture.c
  LIBS colonize_core
  INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)

# Joint Euro↔Indian harness (T3 roadmap Phase 0 + Series D/H/K mid+late):
add_custom_target(golden_ai_joint
  COMMAND ${CMAKE_COMMAND} -E echo "golden_ai_joint: mapgen + turns + contact + diplo + mid01 + late01"
  COMMAND $<TARGET_FILE:golden_mapgen_seed100>
  COMMAND $<TARGET_FILE:golden_ai_turns>
  COMMAND $<TARGET_FILE:unit_ai_contact>
  COMMAND $<TARGET_FILE:unit_ai_diplo>
  COMMAND $<TARGET_FILE:smoke_ai_mid01>
  COMMAND $<TARGET_FILE:smoke_ai_late01>
  DEPENDS golden_mapgen_seed100 golden_ai_turns unit_ai_contact unit_ai_diplo smoke_ai_mid01 smoke_ai_late01
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Joint Euro↔Indian AI golden gate (shared-surface)"
)
# No add_test() here (duplication audit TT-14): all six COMMANDs above are
# already registered as their own ctest tests, so registering the target too
# ran every one of them twice per `ctest`. Build it by hand when you want the
# joint gate in one shot: cmake --build <dir> --target golden_ai_joint

colonize_add_test(NAME smoke_sound
  SOURCES
    tests/smoke/test_sound.c
    src/core/sound.c
    src/core/gsound_vm.c
    src/platform/diagnostics.c
    src/core/strutil.c
    src/platform/dos_compat/dos_compat.c
  LIBS m pthread ${FLUIDSYNTH_LIBRARIES}
  INCLUDE_DIRS ${FLUIDSYNTH_INCLUDE_DIRS}
  COMPILE_DEFS ${_fs_defs}
)

colonize_add_test(NAME smoke_play
  SOURCES tests/smoke/test_play_smoke.c tests/common/platform_cursor_stubs.c
  LIBS colonize_core
)
