cmake_minimum_required(VERSION 3.20)
project(racing_choreographies_parser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg / antlr4-runtime 
find_package(antlr4-runtime CONFIG REQUIRED)

# paths 
set(GRAMMAR_DIR   "${CMAKE_SOURCE_DIR}/grammar")
set(GENERATED_DIR "${CMAKE_SOURCE_DIR}/generated")

set(ANTLR_JAR "" CACHE FILEPATH "Path to antlr-4.x-complete.jar")
if(NOT EXISTS "${ANTLR_JAR}")
  message(FATAL_ERROR "ANTLR_JAR not set or not found. Set -DANTLR_JAR=.../antlr-4.13.x-complete.jar")
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

add_custom_target(antlr_generate DEPENDS ${GENERATED_SOURCES} ${GENERATED_HEADERS})

add_executable(rc_parser
  src/main.cpp
  src/ErrorListener.cpp
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

include(CTest)
enable_testing()

add_test(
  NAME parse_ok_01
  COMMAND rc_parser parse "${CMAKE_SOURCE_DIR}/tests/ok_01.rc"
)

add_test(
  NAME parse_err_01
  COMMAND rc_parser parse "${CMAKE_SOURCE_DIR}/tests/err_01.rc"
)

set_tests_properties(parse_err_01 PROPERTIES WILL_FAIL TRUE)
