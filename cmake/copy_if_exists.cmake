# Copy SRC -> DST when SRC exists. No-op (success) if SRC is missing.
# Usage: cmake -DSRC=... -DDST=... -P copy_if_exists.cmake
if(NOT DEFINED SRC OR NOT DEFINED DST)
  message(FATAL_ERROR "copy_if_exists.cmake requires -DSRC= and -DDST=")
endif()
if(EXISTS "${SRC}")
  get_filename_component(_dir "${DST}" DIRECTORY)
  file(MAKE_DIRECTORY "${_dir}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SRC}" "${DST}"
    COMMAND_ERROR_IS_FATAL ANY
  )
endif()
