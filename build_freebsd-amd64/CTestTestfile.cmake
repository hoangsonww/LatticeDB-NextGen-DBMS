# CMake generated Testfile for 
# Source directory: /workspaces/app
# Build directory: /workspaces/app/build_freebsd-amd64
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(BufferPoolTest "/workspaces/app/build_freebsd-amd64/latticedb_test" "--test=buffer_pool")
set_tests_properties(BufferPoolTest PROPERTIES  _BACKTRACE_TRIPLES "/workspaces/app/CMakeLists.txt;248;add_test;/workspaces/app/CMakeLists.txt;0;")
add_test(BPlusTreeTest "/workspaces/app/build_freebsd-amd64/latticedb_test" "--test=b_plus_tree")
set_tests_properties(BPlusTreeTest PROPERTIES  _BACKTRACE_TRIPLES "/workspaces/app/CMakeLists.txt;249;add_test;/workspaces/app/CMakeLists.txt;0;")
add_test(TransactionTest "/workspaces/app/build_freebsd-amd64/latticedb_test" "--test=transaction")
set_tests_properties(TransactionTest PROPERTIES  _BACKTRACE_TRIPLES "/workspaces/app/CMakeLists.txt;250;add_test;/workspaces/app/CMakeLists.txt;0;")
add_test(StorageTest "/workspaces/app/build_freebsd-amd64/latticedb_test" "--test=storage")
set_tests_properties(StorageTest PROPERTIES  _BACKTRACE_TRIPLES "/workspaces/app/CMakeLists.txt;251;add_test;/workspaces/app/CMakeLists.txt;0;")
add_test(VectorSearchTest "/workspaces/app/build_freebsd-amd64/latticedb_test" "--test=vector_search")
set_tests_properties(VectorSearchTest PROPERTIES  _BACKTRACE_TRIPLES "/workspaces/app/CMakeLists.txt;252;add_test;/workspaces/app/CMakeLists.txt;0;")
add_test(CompressionTest "/workspaces/app/build_freebsd-amd64/latticedb_test" "--test=compression")
set_tests_properties(CompressionTest PROPERTIES  _BACKTRACE_TRIPLES "/workspaces/app/CMakeLists.txt;253;add_test;/workspaces/app/CMakeLists.txt;0;")
add_test(SecurityTest "/workspaces/app/build_freebsd-amd64/latticedb_test" "--test=security")
set_tests_properties(SecurityTest PROPERTIES  _BACKTRACE_TRIPLES "/workspaces/app/CMakeLists.txt;254;add_test;/workspaces/app/CMakeLists.txt;0;")
add_test(StreamProcessingTest "/workspaces/app/build_freebsd-amd64/latticedb_test" "--test=stream_processing")
set_tests_properties(StreamProcessingTest PROPERTIES  _BACKTRACE_TRIPLES "/workspaces/app/CMakeLists.txt;255;add_test;/workspaces/app/CMakeLists.txt;0;")
