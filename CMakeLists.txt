cmake_minimum_required(VERSION 3.15)
project(latticedb CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(latticedb_cli
  src/main.cpp
  src/util.cpp
  src/dp.cpp
  src/crdt.cpp
  src/catalog.cpp
  src/storage.cpp
  src/sql_parser.cpp
  src/executor.cpp
  src/vector.cpp
  src/engine.cpp
)
set_target_properties(latticedb_cli PROPERTIES OUTPUT_NAME latticedb)

if (MSVC)
  target_compile_options(latticedb_cli PRIVATE /W4)
else()
  target_compile_options(latticedb_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()
