cmake_minimum_required(VERSION 3.20)
project(colonize_linux C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(COLONIZE_ENABLE_AUDIO "Enable SDL2 audio backend" ON)

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
      target_link_libraries(SDL2::SDL2 INTERFACE ${SDL2_PC_LINK_LIBRARIES})
      set(SDL2_FOUND ON)
    endif()
  endif()
endif()

find_package(PkgConfig QUIET)
set(COLONIZE_HAS_FLUIDSYNTH OFF)
if(PkgConfig_FOUND)
  pkg_check_modules(FLUIDSYNTH QUIET fluidsynth)
  if(FLUIDSYNTH_FOUND)
    set(COLONIZE_HAS_FLUIDSYNTH ON)
  endif()
endif()

add_library(colonize_core STATIC
  src/core/game_loop.c
  src/core/assets.c
  src/core/font.c
  src/core/ff.c
  src/core/map.c
  src/core/map_gen.c
  src/core/dos_rng.c
  src/core/map_menu.c
  src/core/map_panel.c
  src/core/popup.c
  src/core/ui_button.c
  src/core/pick_music.c
  src/core/unit_stack.c
  src/core/new_game.c
  src/core/pedia.c
  src/core/reports.c
  src/core/europe.c
  src/core/colony_screen.c
  src/core/debug_atlas.c
  src/core/units.c
  src/core/colony.c
  src/core/colony_yield.c
  src/core/colony_production.c
  src/core/colony_craft.c
  src/core/colony_preview.c
  src/core/turn.c
  src/core/ai.c
  src/core/sound.c
  src/core/madspack.c
  src/core/pik.c
  src/core/ss.c
  src/core/savegame.c
  src/core/col1_save.c
  src/core/col1_bridge.c
  src/data/viceroy_tables.c
  src/platform/diagnostics.c
  src/platform/dos_compat/dos_compat.c
)

target_include_directories(colonize_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(colonize_core PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

target_link_libraries(colonize_core PUBLIC m pthread)

if(COLONIZE_HAS_FLUIDSYNTH)
  target_include_directories(colonize_core PUBLIC ${FLUIDSYNTH_INCLUDE_DIRS})
  target_link_libraries(colonize_core PUBLIC ${FLUIDSYNTH_LIBRARIES})
  target_compile_definitions(colonize_core PUBLIC COLONIZE_HAS_FLUIDSYNTH=1)
  message(STATUS "FluidSynth found — music backend enabled")
else()
  message(STATUS "FluidSynth not found — music uses square-wave fallback")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(colonize_core PRIVATE -Wno-unused-parameter)
endif()

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
    SDL2::SDL2
  )

  if(COLONIZE_ENABLE_AUDIO)
    target_compile_definitions(colonize_linux PRIVATE COLONIZE_ENABLE_AUDIO=1)
  endif()

  # Ship bundled SoundFont next to the binary (no ScummVM install required).
  add_custom_command(
    TARGET colonize_linux POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:colonize_linux>/data/soundfonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/data/soundfonts/Roland_SC-55.sf2
            $<TARGET_FILE_DIR:colonize_linux>/data/soundfonts/Roland_SC-55.sf2
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/data/soundfonts/COPYRIGHT.Roland_SC-55
            $<TARGET_FILE_DIR:colonize_linux>/data/soundfonts/COPYRIGHT.Roland_SC-55
    COMMENT "Copy bundled Roland_SC-55.sf2 next to colonize_linux"
  )
else()
  message(WARNING "SDL2 not found. colonize_linux target is disabled; build/install SDL2 dev packages to enable.")
endif()

enable_testing()

add_executable(smoke_savegame
  tests/smoke/test_savegame.c
  src/core/savegame.c
  src/core/col1_save.c
  src/platform/diagnostics.c
)

target_include_directories(smoke_savegame PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(NAME smoke_savegame COMMAND smoke_savegame)

add_executable(smoke_savegame_incompatible
  tests/smoke/test_savegame_incompatible.c
  src/core/savegame.c
  src/core/col1_save.c
  src/platform/diagnostics.c
)

target_include_directories(smoke_savegame_incompatible PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(NAME smoke_savegame_incompatible COMMAND smoke_savegame_incompatible)

add_executable(smoke_col1_save
  tests/smoke/test_col1_save.c
)

target_include_directories(smoke_col1_save PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(smoke_col1_save PRIVATE colonize_core)

add_test(NAME smoke_col1_save COMMAND smoke_col1_save WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(smoke_game_flow
  tests/smoke/test_game_flow.c
  tests/smoke/platform_cursor_stubs.c
  src/core/game_loop.c
  src/core/assets.c
  src/core/font.c
  src/core/ff.c
  src/core/map.c
  src/core/map_gen.c
  src/core/dos_rng.c
  src/core/map_menu.c
  src/core/map_panel.c
  src/core/popup.c
  src/core/ui_button.c
  src/core/pick_music.c
  src/core/unit_stack.c
  src/core/new_game.c
  src/core/pedia.c
  src/core/reports.c
  src/core/europe.c
  src/core/colony_screen.c
  src/core/debug_atlas.c
  src/core/units.c
  src/core/colony.c
  src/core/colony_yield.c
  src/core/colony_production.c
  src/core/colony_craft.c
  src/core/colony_preview.c
  src/core/turn.c
  src/core/ai.c
  src/core/sound.c
  src/core/madspack.c
  src/core/pik.c
  src/core/ss.c
  src/core/savegame.c
  src/core/col1_save.c
  src/core/col1_bridge.c
  src/data/viceroy_tables.c
  src/platform/diagnostics.c
  src/platform/dos_compat/dos_compat.c
)

add_executable(smoke_new_game
  tests/smoke/test_new_game.c
  tests/smoke/platform_cursor_stubs.c
  src/core/game_loop.c
  src/core/assets.c
  src/core/font.c
  src/core/ff.c
  src/core/map.c
  src/core/map_gen.c
  src/core/dos_rng.c
  src/core/map_menu.c
  src/core/map_panel.c
  src/core/popup.c
  src/core/ui_button.c
  src/core/pick_music.c
  src/core/unit_stack.c
  src/core/new_game.c
  src/core/pedia.c
  src/core/reports.c
  src/core/europe.c
  src/core/colony_screen.c
  src/core/debug_atlas.c
  src/core/units.c
  src/core/colony.c
  src/core/colony_yield.c
  src/core/colony_production.c
  src/core/colony_craft.c
  src/core/colony_preview.c
  src/core/turn.c
  src/core/ai.c
  src/core/sound.c
  src/core/madspack.c
  src/core/pik.c
  src/core/ss.c
  src/core/savegame.c
  src/core/col1_save.c
  src/core/col1_bridge.c
  src/data/viceroy_tables.c
  src/platform/diagnostics.c
  src/platform/dos_compat/dos_compat.c
)

target_include_directories(smoke_new_game PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(smoke_new_game PRIVATE m pthread)
if(COLONIZE_HAS_FLUIDSYNTH)
  target_include_directories(smoke_new_game PRIVATE ${FLUIDSYNTH_INCLUDE_DIRS})
  target_link_libraries(smoke_new_game PRIVATE ${FLUIDSYNTH_LIBRARIES})
  target_compile_definitions(smoke_new_game PRIVATE COLONIZE_HAS_FLUIDSYNTH=1)
endif()
add_test(NAME smoke_new_game COMMAND smoke_new_game)

add_executable(smoke_popup
  tests/smoke/test_popup.c
  src/core/popup.c
  src/core/ss.c
  src/core/madspack.c
  src/core/assets.c
  src/platform/diagnostics.c
  src/platform/dos_compat/dos_compat.c
)

target_include_directories(smoke_popup PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(NAME smoke_popup COMMAND smoke_popup)

add_executable(smoke_pick_music
  tests/smoke/test_pick_music.c
  src/core/pick_music.c
  src/core/popup.c
  src/core/sound.c
  src/core/font.c
  src/core/assets.c
  src/core/ss.c
  src/core/madspack.c
  src/platform/diagnostics.c
  src/platform/dos_compat/dos_compat.c
)

target_include_directories(smoke_pick_music PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(smoke_pick_music PRIVATE m pthread)
if(COLONIZE_HAS_FLUIDSYNTH)
  target_include_directories(smoke_pick_music PRIVATE ${FLUIDSYNTH_INCLUDE_DIRS})
  target_link_libraries(smoke_pick_music PRIVATE ${FLUIDSYNTH_LIBRARIES})
  target_compile_definitions(smoke_pick_music PRIVATE COLONIZE_HAS_FLUIDSYNTH=1)
endif()

add_test(NAME smoke_pick_music COMMAND smoke_pick_music WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(smoke_map
  tests/smoke/test_map.c
  src/core/map.c
  src/data/viceroy_tables.c
  src/platform/diagnostics.c
)

target_include_directories(smoke_map PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(NAME smoke_map COMMAND smoke_map WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(smoke_map_gen
  tests/smoke/test_map_gen.c
)

target_include_directories(smoke_map_gen PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(smoke_map_gen PRIVATE colonize_core)

add_test(NAME smoke_map_gen COMMAND smoke_map_gen WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(smoke_mapgen_seed100
  tests/smoke/test_mapgen_seed100.c
)

target_include_directories(smoke_mapgen_seed100 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(smoke_mapgen_seed100 PRIVATE colonize_core)

add_test(NAME smoke_mapgen_seed100 COMMAND smoke_mapgen_seed100 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(smoke_viceroy_tables
  tests/smoke/test_viceroy_tables.c
  src/data/viceroy_tables.c
)

target_include_directories(smoke_viceroy_tables PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(NAME smoke_viceroy_tables COMMAND smoke_viceroy_tables)

add_executable(smoke_pedia
  tests/smoke/test_pedia.c
  src/core/pedia.c
  src/core/assets.c
  src/core/pik.c
  src/core/madspack.c
  src/core/font.c
  src/core/ff.c
  src/data/viceroy_tables.c
  src/platform/diagnostics.c
  src/platform/dos_compat/dos_compat.c
)

target_include_directories(smoke_pedia PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(NAME smoke_pedia COMMAND smoke_pedia WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(smoke_europe
  tests/smoke/test_europe.c
)

target_include_directories(smoke_europe PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(smoke_europe PRIVATE colonize_core)

add_test(NAME smoke_europe COMMAND smoke_europe WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(smoke_colony_screen
  tests/smoke/test_colony_screen.c
)

target_include_directories(smoke_colony_screen PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(smoke_colony_screen PRIVATE colonize_core)

add_test(NAME smoke_colony_screen COMMAND smoke_colony_screen WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(smoke_map_panel
  tests/smoke/test_map_panel.c
)

target_include_directories(smoke_map_panel PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(smoke_map_panel PRIVATE colonize_core)

add_test(NAME smoke_map_panel COMMAND smoke_map_panel WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(smoke_units
  tests/smoke/test_units.c
  src/core/units.c
  src/core/colony.c
  src/core/colony_yield.c
  src/core/map.c
  src/core/assets.c
  src/core/font.c
  src/core/ff.c
  src/core/ss.c
  src/core/madspack.c
  src/data/viceroy_tables.c
  src/platform/diagnostics.c
  src/platform/dos_compat/dos_compat.c
)

target_include_directories(smoke_units PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(NAME smoke_units COMMAND smoke_units WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(smoke_colonies
  tests/smoke/test_colonies.c
  src/core/colony.c
  src/core/units.c
  src/core/map.c
  src/core/assets.c
  src/core/font.c
  src/core/ff.c
  src/core/ss.c
  src/core/madspack.c
  src/data/viceroy_tables.c
  src/platform/diagnostics.c
  src/platform/dos_compat/dos_compat.c
)

target_include_directories(smoke_colonies PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(NAME smoke_colonies COMMAND smoke_colonies WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(smoke_reports
  tests/smoke/test_reports.c
  src/core/reports.c
  src/core/col1_save.c
  src/core/colony.c
  src/core/units.c
  src/core/map.c
  src/core/europe.c
  src/core/pik.c
  src/core/ss.c
  src/core/madspack.c
  src/core/assets.c
  src/core/font.c
  src/core/ff.c
  src/data/viceroy_tables.c
  src/platform/diagnostics.c
  src/platform/dos_compat/dos_compat.c
)

target_include_directories(smoke_reports PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(NAME smoke_reports COMMAND smoke_reports WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(smoke_map_menu
  tests/smoke/test_map_menu.c
  src/core/map_menu.c
  src/core/assets.c
  src/core/font.c
  src/core/ff.c
  src/core/madspack.c
  src/platform/diagnostics.c
  src/platform/dos_compat/dos_compat.c
)

target_include_directories(smoke_map_menu PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(NAME smoke_map_menu COMMAND smoke_map_menu WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(smoke_turn
  tests/smoke/test_turn.c
)

target_include_directories(smoke_turn PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(smoke_turn PRIVATE colonize_core)

add_test(NAME smoke_turn COMMAND smoke_turn WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(smoke_ff
  tests/smoke/test_ff.c
  src/core/ff.c
  src/core/font.c
  src/core/madspack.c
  src/platform/diagnostics.c
)

target_include_directories(smoke_ff PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(NAME smoke_ff COMMAND smoke_ff WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(smoke_ss
  tests/smoke/test_ss.c
  src/core/ss.c
  src/core/madspack.c
  src/core/assets.c
  src/platform/diagnostics.c
  src/platform/dos_compat/dos_compat.c
)

target_include_directories(smoke_ss PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(NAME smoke_ss COMMAND smoke_ss WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(smoke_assets
  tests/smoke/test_assets.c
  src/core/assets.c
  src/platform/diagnostics.c
  src/platform/dos_compat/dos_compat.c
)

target_include_directories(smoke_assets PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(NAME smoke_assets COMMAND smoke_assets)

add_executable(smoke_pik
  tests/smoke/test_pik.c
  src/core/madspack.c
  src/core/pik.c
  src/core/assets.c
  src/platform/diagnostics.c
  src/platform/dos_compat/dos_compat.c
)

target_include_directories(smoke_pik PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(NAME smoke_pik COMMAND smoke_pik WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(smoke_ai
  tests/smoke/test_ai.c
)

target_include_directories(smoke_ai PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(smoke_ai PRIVATE colonize_core)

add_test(NAME smoke_ai COMMAND smoke_ai WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(smoke_sound
  tests/smoke/test_sound.c
  src/core/sound.c
  src/platform/diagnostics.c
  src/platform/dos_compat/dos_compat.c
)

target_include_directories(smoke_sound PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(smoke_sound PRIVATE m pthread)
if(COLONIZE_HAS_FLUIDSYNTH)
  target_include_directories(smoke_sound PRIVATE ${FLUIDSYNTH_INCLUDE_DIRS})
  target_link_libraries(smoke_sound PRIVATE ${FLUIDSYNTH_LIBRARIES})
  target_compile_definitions(smoke_sound PRIVATE COLONIZE_HAS_FLUIDSYNTH=1)
endif()

add_test(NAME smoke_sound COMMAND smoke_sound WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

target_include_directories(smoke_game_flow PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(smoke_game_flow PRIVATE m pthread)
if(COLONIZE_HAS_FLUIDSYNTH)
  target_include_directories(smoke_game_flow PRIVATE ${FLUIDSYNTH_INCLUDE_DIRS})
  target_link_libraries(smoke_game_flow PRIVATE ${FLUIDSYNTH_LIBRARIES})
  target_compile_definitions(smoke_game_flow PRIVATE COLONIZE_HAS_FLUIDSYNTH=1)
endif()

add_test(NAME smoke_game_flow COMMAND smoke_game_flow)
