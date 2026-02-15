# cmake/run_with_stdin.cmake
cmake_minimum_required(VERSION 3.20)

# Required variables:
#   RC_PARSER  = path to rc_parser executable
#   CMD        = parse | tokens | ast
#   INPUT      = path to .rc file used as stdin
#
# Optional:
#   EXTRA_ARGS = extra args string (space-separated), e.g. "--print-tree"

if(NOT DEFINED RC_PARSER OR RC_PARSER STREQUAL "")
  message(FATAL_ERROR "RC_PARSER not set")
endif()

if(NOT DEFINED CMD OR CMD STREQUAL "")
  message(FATAL_ERROR "CMD not set (parse|tokens|ast)")
endif()

if(NOT DEFINED INPUT OR INPUT STREQUAL "")
  message(FATAL_ERROR "INPUT not set")
endif()

# Normalize paths (important on Windows)
file(TO_CMAKE_PATH "${RC_PARSER}" RC_PARSER_NORM)
file(TO_CMAKE_PATH "${INPUT}" INPUT_NORM)

if(NOT EXISTS "${RC_PARSER_NORM}")
  message(FATAL_ERROR "RC_PARSER does not exist: ${RC_PARSER_NORM}")
endif()

if(NOT EXISTS "${INPUT_NORM}")
  message(FATAL_ERROR "INPUT does not exist: ${INPUT_NORM}")
endif()

# Build command list
set(args "${CMD}" "--") # '--' is stdin alias in your CLI

if(DEFINED EXTRA_ARGS AND NOT EXTRA_ARGS STREQUAL "")
  separate_arguments(extra_list NATIVE_COMMAND "${EXTRA_ARGS}")
  list(APPEND args ${extra_list})
endif()

execute_process(
  COMMAND "${RC_PARSER_NORM}" ${args}
  INPUT_FILE "${INPUT_NORM}"
  RESULT_VARIABLE rv
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)

if(NOT rv EQUAL 0)
  message(STATUS "run_with_stdin.cmake failed")
  message(STATUS "RC_PARSER='${RC_PARSER_NORM}'")
  message(STATUS "CMD='${CMD}'")
  message(STATUS "INPUT='${INPUT_NORM}'")
  message(STATUS "ARGS='${args}'")
  if(NOT out STREQUAL "")
    message(STATUS "stdout:\n${out}")
  endif()
  if(NOT err STREQUAL "")
    message(STATUS "stderr:\n${err}")
  endif()
  message(FATAL_ERROR "Command failed with exit code ${rv}")
endif()
