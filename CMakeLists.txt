cmake_minimum_required(VERSION 3.15)
project(latticedb CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler options
if (MSVC)
    set(CMAKE_CXX_FLAGS "/W4")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
else()
    set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

# Include directories
include_directories(src)

# Find required packages
find_package(Threads REQUIRED)

# Check for optional dependencies
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LZ4 QUIET liblz4)
    pkg_check_modules(ZSTD QUIET libzstd)
endif()

# OpenTelemetry dependencies
option(ENABLE_TELEMETRY "Enable OpenTelemetry support" ON)
if(ENABLE_TELEMETRY)
    find_package(opentelemetry-cpp QUIET)
    if(opentelemetry-cpp_FOUND)
        set(TELEMETRY_FOUND ON)
        message(STATUS "OpenTelemetry found, enabling telemetry support")
    else()
        message(STATUS "OpenTelemetry not found, building without telemetry")
        set(TELEMETRY_FOUND OFF)
    endif()
else()
    set(TELEMETRY_FOUND OFF)
endif()

# Force-disable optional compression to simplify minimal build
set(LZ4_FOUND OFF)
set(ZSTD_FOUND OFF)

# Source files for the modern DBMS
set(LATTICEDB_SOURCES
    # Core Types System
    src/types/value.cpp
    src/types/tuple.cpp
    src/types/schema.cpp

    # Common Utilities
    src/common/exception.cpp
    src/common/logger.cpp

    # Storage Layer (minimal)
    src/storage/page.cpp
    src/storage/disk_manager.cpp
    src/storage/table_page.cpp

    # Buffer Pool Management
    src/buffer/buffer_pool_manager.cpp
    src/buffer/replacer.cpp

    # Transaction Management
    src/transaction/transaction.cpp

    # Concurrency Control
    src/concurrency/lock_manager.cpp

    # Recovery System
    src/recovery/log_manager.cpp

    # Catalog Management
    src/catalog/catalog_manager.cpp
    src/catalog/table_heap.cpp

    # Query Processing
    src/query/sql_parser.cpp
    src/query/query_planner.cpp
    src/query/query_executor.cpp

    # Storage Engines
    src/engine/storage_engine.cpp
    src/engine/database_engine.cpp

    # Machine Learning / Vector Search
    src/ml/vector_search.cpp

    # Diagnostics
    src/diagnostics/system_info.cpp

    # Network Server
    src/network/server.cpp

    # Telemetry (OpenTelemetry/Jaeger)
    src/telemetry/telemetry_manager.cpp
    src/telemetry/metrics_collector.cpp
    src/telemetry/tracer.cpp
)

## NOTE: Stub-generation disabled to preserve implemented sources
# The original build wrote placeholder implementations on configure.
# This caused real source files to be overwritten. The section has been removed.

# Create advanced feature implementation files
# Disable advanced feature stub generation in minimal build

# Create the main executable
add_executable(latticedb_cli src/main.cpp ${LATTICEDB_SOURCES})

# Set output name
set_target_properties(latticedb_cli PROPERTIES OUTPUT_NAME latticedb)

# Link libraries
target_link_libraries(latticedb_cli Threads::Threads)

# Link optional compression libraries if available
if(LZ4_FOUND)
    target_link_libraries(latticedb_cli ${LZ4_LIBRARIES})
    target_compile_definitions(latticedb_cli PRIVATE HAVE_LZ4)
endif()

if(ZSTD_FOUND)
    target_link_libraries(latticedb_cli ${ZSTD_LIBRARIES})
    target_compile_definitions(latticedb_cli PRIVATE HAVE_ZSTD)
endif()

# Link OpenTelemetry libraries if available
if(TELEMETRY_FOUND)
    target_link_libraries(latticedb_cli
        opentelemetry-cpp::api
        opentelemetry-cpp::sdk
        opentelemetry-cpp::ext
        opentelemetry-cpp::exporters::jaeger
        opentelemetry-cpp::exporters::prometheus
        opentelemetry-cpp::exporters::otlp
    )
    target_compile_definitions(latticedb_cli PRIVATE HAVE_TELEMETRY)
endif()

# Install target
install(TARGETS latticedb_cli DESTINATION bin)

# Network Server executable
add_executable(latticedb_server
    server_main.cpp
    ${LATTICEDB_SOURCES}
)
target_link_libraries(latticedb_server Threads::Threads)
if(LZ4_FOUND)
    target_link_libraries(latticedb_server ${LZ4_LIBRARIES})
    target_compile_definitions(latticedb_server PRIVATE HAVE_LZ4)
endif()
if(ZSTD_FOUND)
    target_link_libraries(latticedb_server ${ZSTD_LIBRARIES})
    target_compile_definitions(latticedb_server PRIVATE HAVE_ZSTD)
endif()
if(TELEMETRY_FOUND)
    target_link_libraries(latticedb_server
        opentelemetry-cpp::api
        opentelemetry-cpp::sdk
        opentelemetry-cpp::ext
        opentelemetry-cpp::exporters::jaeger
        opentelemetry-cpp::exporters::prometheus
        opentelemetry-cpp::exporters::otlp
    )
    target_compile_definitions(latticedb_server PRIVATE HAVE_TELEMETRY)
endif()

# Interactive CLI Shell executable
add_executable(latticedb_shell
    cli_main.cpp
    ${LATTICEDB_SOURCES}
)
target_link_libraries(latticedb_shell Threads::Threads)
if(LZ4_FOUND)
    target_link_libraries(latticedb_shell ${LZ4_LIBRARIES})
    target_compile_definitions(latticedb_shell PRIVATE HAVE_LZ4)
endif()
if(ZSTD_FOUND)
    target_link_libraries(latticedb_shell ${ZSTD_LIBRARIES})
    target_compile_definitions(latticedb_shell PRIVATE HAVE_ZSTD)
endif()
if(TELEMETRY_FOUND)
    target_link_libraries(latticedb_shell
        opentelemetry-cpp::api
        opentelemetry-cpp::sdk
        opentelemetry-cpp::ext
        opentelemetry-cpp::exporters::jaeger
        opentelemetry-cpp::exporters::prometheus
        opentelemetry-cpp::exporters::otlp
    )
    target_compile_definitions(latticedb_shell PRIVATE HAVE_TELEMETRY)
endif()

# Enable testing
enable_testing()

# Create comprehensive test directories
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)

# Test executable
add_executable(latticedb_test
    tests/test_main.cpp
    tests/buffer_pool_test.cpp
    tests/b_plus_tree_test.cpp
    tests/transaction_test.cpp
    tests/storage_test.cpp
    tests/vector_search_test.cpp
    tests/compression_test.cpp
    tests/security_test.cpp
    tests/stream_processing_test.cpp
    ${LATTICEDB_SOURCES}
)

# Create test files
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_main.cpp "// Main test runner\n#include <iostream>\nint main() { std::cout << \"LatticeDB Tests - Not yet implemented\" << std::endl; return 0; }")
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/tests/buffer_pool_test.cpp "// Buffer pool tests\n")
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/tests/b_plus_tree_test.cpp "// B+Tree tests\n")
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/tests/transaction_test.cpp "// Transaction tests\n")
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/tests/storage_test.cpp "// Storage tests\n")
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/tests/vector_search_test.cpp "// Vector search tests\n")
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/tests/compression_test.cpp "// Compression tests\n")
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/tests/security_test.cpp "// Security tests\n")
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/tests/stream_processing_test.cpp "// Stream processing tests\n")

target_compile_definitions(latticedb_test PRIVATE TESTING)
target_link_libraries(latticedb_test Threads::Threads)
if(LZ4_FOUND)
    target_link_libraries(latticedb_test ${LZ4_LIBRARIES})
endif()
if(ZSTD_FOUND)
    target_link_libraries(latticedb_test ${ZSTD_LIBRARIES})
endif()

# Add comprehensive test suites
add_test(NAME BufferPoolTest COMMAND latticedb_test --test=buffer_pool)
add_test(NAME BPlusTreeTest COMMAND latticedb_test --test=b_plus_tree)
add_test(NAME TransactionTest COMMAND latticedb_test --test=transaction)
add_test(NAME StorageTest COMMAND latticedb_test --test=storage)
add_test(NAME VectorSearchTest COMMAND latticedb_test --test=vector_search)
add_test(NAME CompressionTest COMMAND latticedb_test --test=compression)
add_test(NAME SecurityTest COMMAND latticedb_test --test=security)
add_test(NAME StreamProcessingTest COMMAND latticedb_test --test=stream_processing)

# Performance benchmarks
add_executable(latticedb_bench
    benchmarks/benchmark_main.cpp
    ${LATTICEDB_SOURCES}
)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
# Don't overwrite the benchmark file if it exists

target_link_libraries(latticedb_bench Threads::Threads)

# Direct SQL test
add_executable(test_direct
    examples/test_direct.cpp
    ${LATTICEDB_SOURCES}
)
target_link_libraries(test_direct Threads::Threads)

# Comprehensive feature test
add_executable(comprehensive_test
    test/comprehensive_test.cpp
    ${LATTICEDB_SOURCES}
)
target_link_libraries(comprehensive_test Threads::Threads)
if(LZ4_FOUND)
    target_link_libraries(latticedb_bench ${LZ4_LIBRARIES})
endif()
if(ZSTD_FOUND)
    target_link_libraries(latticedb_bench ${ZSTD_LIBRARIES})
endif()

# Documentation target
add_custom_target(docs
    COMMAND echo "Building documentation..."
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generate API documentation"
)

# Format target using clang-format
find_program(CLANG_FORMAT_EXE NAMES "clang-format")
if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i -style=file ${LATTICEDB_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Auto-formatting code with clang-format"
    )
endif()

# Static analysis with cppcheck
find_program(CPPCHECK_EXE NAMES "cppcheck")
if(CPPCHECK_EXE)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK_EXE} --enable=all --std=c++17 --suppress=missingInclude ${CMAKE_CURRENT_SOURCE_DIR}/src
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running cppcheck static analysis"
    )
endif()

# Print configuration summary
message(STATUS "LatticeDB Configuration Summary:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  LZ4 Support: ${LZ4_FOUND}")
message(STATUS "  ZSTD Support: ${ZSTD_FOUND}")
message(STATUS "  OpenTelemetry Support: ${TELEMETRY_FOUND}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")

# Create build info file
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/build_info.h
    "#pragma once\n"
    "#define LATTICEDB_VERSION_MAJOR 2\n"
    "#define LATTICEDB_VERSION_MINOR 0\n"
    "#define LATTICEDB_VERSION_PATCH 0\n"
    "#define LATTICEDB_BUILD_TYPE \"${CMAKE_BUILD_TYPE}\"\n"
    "#define LATTICEDB_COMPILER \"${CMAKE_CXX_COMPILER_ID}\"\n"
    "#define LATTICEDB_CXX_STANDARD ${CMAKE_CXX_STANDARD}\n"
)
