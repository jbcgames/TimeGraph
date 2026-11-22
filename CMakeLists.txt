cmake_minimum_required(VERSION 3.15)
project(TimeGraph LANGUAGES C)

include(GenerateG1A)
find_package(Gint 2.9 REQUIRED)

set(SOURCES
  src/main.c
  src/expr.c
)

add_executable(TimeGraph ${SOURCES})
target_compile_options(TimeGraph PRIVATE -Wall -Wextra -Os)
target_link_libraries(TimeGraph Gint::Gint m)

generate_g1a(TARGET TimeGraph OUTPUT "TimeGraph.g1a"
  NAME "TimeGraph"
  ICON "assets/icon.png"
)
