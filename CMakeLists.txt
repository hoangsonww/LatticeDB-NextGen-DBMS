cmake_minimum_required(VERSION 3.15)
project(ForgeDB CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(forgedb
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

if (MSVC)
  target_compile_options(forgedb PRIVATE /W4)
else()
  target_compile_options(forgedb PRIVATE -Wall -Wextra -Wpedantic)
endif()
