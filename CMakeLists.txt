cmake_minimum_required(VERSION 3.20)
project(racing_choreographies_parser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------- Dependencies --------------------
find_package(antlr4-runtime CONFIG REQUIRED)

# -------------------- Paths --------------------
set(GRAMMAR_DIR   "${CMAKE_SOURCE_DIR}/grammar")
set(GENERATED_DIR "${CMAKE_SOURCE_DIR}/generated")

# -------------------- ANTLR --------------------
set(ANTLR_JAR "" CACHE FILEPATH "Path to antlr-4.x-complete.jar")
if(NOT EXISTS "${ANTLR_JAR}")
  message(FATAL_ERROR
    "ANTLR_JAR not set or not found. Set -DANTLR_JAR=.../antlr-4.13.x-complete.jar"
  )
endif()

set(GRAMMAR_NAME "RacingChoreo")
set(GRAMMAR_FILE "${GRAMMAR_DIR}/${GRAMMAR_NAME}.g4")

file(MAKE_DIRECTORY "${GENERATED_DIR}")

set(GENERATED_SOURCES
  "${GENERATED_DIR}/${GRAMMAR_NAME}Lexer.cpp"
  "${GENERATED_DIR}/${GRAMMAR_NAME}Parser.cpp"
  "${GENERATED_DIR}/${GRAMMAR_NAME}BaseVisitor.cpp"
  "${GENERATED_DIR}/${GRAMMAR_NAME}Visitor.cpp"
)

set(GENERATED_HEADERS
  "${GENERATED_DIR}/${GRAMMAR_NAME}Lexer.h"
  "${GENERATED_DIR}/${GRAMMAR_NAME}Parser.h"
  "${GENERATED_DIR}/${GRAMMAR_NAME}BaseVisitor.h"
  "${GENERATED_DIR}/${GRAMMAR_NAME}Visitor.h"
)

add_custom_command(
  OUTPUT ${GENERATED_SOURCES} ${GENERATED_HEADERS}
  COMMAND java -jar "${ANTLR_JAR}"
          -Dlanguage=Cpp
          -visitor
          -no-listener
          -o "${GENERATED_DIR}"
          "${GRAMMAR_FILE}"
  DEPENDS "${GRAMMAR_FILE}"
  COMMENT "Generating ANTLR4 C++ parser/lexer from ${GRAMMAR_NAME}.g4"
  VERBATIM
)

add_custom_target(antlr_generate
  DEPENDS ${GENERATED_SOURCES} ${GENERATED_HEADERS}
)

# -------------------- Executable --------------------
add_executable(rc_parser
  src/main.cpp
  src/ErrorListener.cpp

  # AST
  src/AstBuilderVisitor.cpp
  src/AstPrinter.cpp
  src/AstJson.cpp

  # Validation
  src/Validation.cpp

  ${GENERATED_SOURCES}
)

add_dependencies(rc_parser antlr_generate)

target_include_directories(rc_parser PRIVATE
  "${GENERATED_DIR}"
  "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(rc_parser PRIVATE antlr4_shared)

if(MSVC)
  target_compile_options(rc_parser PRIVATE /W4)
endif()

# -------------------- Tests dir autodetect --------------------
if(EXISTS "${CMAKE_SOURCE_DIR}/tests")
  set(TESTS_DIR "${CMAKE_SOURCE_DIR}/tests")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/test")
  set(TESTS_DIR "${CMAKE_SOURCE_DIR}/test")
else()
  message(FATAL_ERROR "Cannot find tests directory: expected 'tests/' or 'test/' in project root")
endif()

# -------------------- Tests --------------------
include(CTest)
enable_testing()

add_test(NAME parse_ok_01       COMMAND rc_parser parse  "${TESTS_DIR}/ok_01.rc")
add_test(NAME parse_err_01      COMMAND rc_parser parse  "${TESTS_DIR}/err_01.rc")
add_test(NAME parse_err_lex_01  COMMAND rc_parser parse  "${TESTS_DIR}/err_lex_01.rc")
add_test(NAME ast_ok_01         COMMAND rc_parser ast    "${TESTS_DIR}/ok_01.rc")

# semantic (validation) tests
add_test(NAME parse_err_sem_01  COMMAND rc_parser parse "${TESTS_DIR}/err_sem_01.rc")
add_test(NAME parse_err_sem_02  COMMAND rc_parser parse "${TESTS_DIR}/err_sem_02.rc")
add_test(NAME ast_err_sem_01    COMMAND rc_parser ast   "${TESTS_DIR}/err_sem_01.rc")
add_test(NAME ast_err_sem_02    COMMAND rc_parser ast   "${TESTS_DIR}/err_sem_02.rc")

# stdin tests via cmake script
add_test(
  NAME parse_ok_stdin
  COMMAND "${CMAKE_COMMAND}"
    "-DRC_PARSER:FILEPATH=$<TARGET_FILE:rc_parser>"
    "-DCMD=parse"
    "-DINPUT:FILEPATH=${TESTS_DIR}/ok_01.rc"
    -P "${CMAKE_SOURCE_DIR}/cmake/run_with_stdin.cmake"
)

add_test(
  NAME ast_ok_stdin
  COMMAND "${CMAKE_COMMAND}"
    "-DRC_PARSER:FILEPATH=$<TARGET_FILE:rc_parser>"
    "-DCMD=ast"
    "-DINPUT:FILEPATH=${TESTS_DIR}/ok_01.rc"
    -P "${CMAKE_SOURCE_DIR}/cmake/run_with_stdin.cmake"
)

add_test(
  NAME tokens_ok_stdin
  COMMAND "${CMAKE_COMMAND}"
    "-DRC_PARSER:FILEPATH=$<TARGET_FILE:rc_parser>"
    "-DCMD=tokens"
    "-DINPUT:FILEPATH=${TESTS_DIR}/ok_01.rc"
    -P "${CMAKE_SOURCE_DIR}/cmake/run_with_stdin.cmake"
)

add_test(NAME parse_ok_quiet      COMMAND rc_parser parse "${TESTS_DIR}/ok_01.rc" --quiet)
add_test(NAME parse_ok_print_tree COMMAND rc_parser parse "${TESTS_DIR}/ok_01.rc" --print-tree)
add_test(NAME ast_ok_with_loc     COMMAND rc_parser ast   "${TESTS_DIR}/ok_01.rc" --with-loc)

# JSON tests
add_test(NAME parse_ok_json   COMMAND rc_parser parse  "${TESTS_DIR}/ok_01.rc" --json)
add_test(NAME parse_err_json  COMMAND rc_parser parse  "${TESTS_DIR}/err_01.rc" --json)
add_test(NAME tokens_ok_json  COMMAND rc_parser tokens "${TESTS_DIR}/ok_01.rc" --json)
add_test(NAME ast_ok_json     COMMAND rc_parser ast    "${TESTS_DIR}/ok_01.rc" --json)

# semantic JSON tests
add_test(NAME parse_err_sem_01_json COMMAND rc_parser parse "${TESTS_DIR}/err_sem_01.rc" --json)
add_test(NAME parse_err_sem_02_json COMMAND rc_parser parse "${TESTS_DIR}/err_sem_02.rc" --json)
add_test(NAME ast_err_sem_01_json   COMMAND rc_parser ast   "${TESTS_DIR}/err_sem_01.rc" --json)
add_test(NAME ast_err_sem_02_json   COMMAND rc_parser ast   "${TESTS_DIR}/err_sem_02.rc" --json)

# Expected failures
set_tests_properties(parse_err_01     PROPERTIES WILL_FAIL TRUE)
set_tests_properties(parse_err_lex_01 PROPERTIES WILL_FAIL TRUE)
set_tests_properties(parse_err_json   PROPERTIES WILL_FAIL TRUE)

set_tests_properties(parse_err_sem_01 PROPERTIES WILL_FAIL TRUE)
set_tests_properties(parse_err_sem_02 PROPERTIES WILL_FAIL TRUE)
set_tests_properties(ast_err_sem_01   PROPERTIES WILL_FAIL TRUE)
set_tests_properties(ast_err_sem_02   PROPERTIES WILL_FAIL TRUE)

set_tests_properties(parse_err_sem_01_json PROPERTIES WILL_FAIL TRUE)
set_tests_properties(parse_err_sem_02_json PROPERTIES WILL_FAIL TRUE)
set_tests_properties(ast_err_sem_01_json   PROPERTIES WILL_FAIL TRUE)
set_tests_properties(ast_err_sem_02_json   PROPERTIES WILL_FAIL TRUE)
