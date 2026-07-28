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

add_library(colonize_core STATIC
  src/core/game_loop.c
  src/core/assets.c
  src/core/font.c
  src/core/madspack.c
  src/core/pik.c
  src/core/ss.c
  src/core/savegame.c
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
else()
  message(WARNING "SDL2 not found. colonize_linux target is disabled; build/install SDL2 dev packages to enable.")
endif()

enable_testing()

add_executable(smoke_savegame
  tests/smoke/test_savegame.c
  src/core/savegame.c
  src/platform/diagnostics.c
)

target_include_directories(smoke_savegame PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(NAME smoke_savegame COMMAND smoke_savegame)

add_executable(smoke_savegame_incompatible
  tests/smoke/test_savegame_incompatible.c
  src/core/savegame.c
  src/platform/diagnostics.c
)

target_include_directories(smoke_savegame_incompatible PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(NAME smoke_savegame_incompatible COMMAND smoke_savegame_incompatible)

add_executable(smoke_game_flow
  tests/smoke/test_game_flow.c
  src/core/game_loop.c
  src/core/assets.c
  src/core/font.c
  src/core/madspack.c
  src/core/pik.c
  src/core/ss.c
  src/core/savegame.c
  src/platform/diagnostics.c
  src/platform/dos_compat/dos_compat.c
)

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

target_include_directories(smoke_game_flow PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(NAME smoke_game_flow COMMAND smoke_game_flow)
