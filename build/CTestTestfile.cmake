# CMake generated Testfile for 
# Source directory: /Users/davidnguyen/WebstormProjects/LatticeDB-DBMS
# Build directory: /Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(BufferPoolTest "/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/build/latticedb_test" "--test=buffer_pool")
set_tests_properties(BufferPoolTest PROPERTIES  _BACKTRACE_TRIPLES "/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/CMakeLists.txt;193;add_test;/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/CMakeLists.txt;0;")
add_test(BPlusTreeTest "/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/build/latticedb_test" "--test=b_plus_tree")
set_tests_properties(BPlusTreeTest PROPERTIES  _BACKTRACE_TRIPLES "/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/CMakeLists.txt;194;add_test;/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/CMakeLists.txt;0;")
add_test(TransactionTest "/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/build/latticedb_test" "--test=transaction")
set_tests_properties(TransactionTest PROPERTIES  _BACKTRACE_TRIPLES "/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/CMakeLists.txt;195;add_test;/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/CMakeLists.txt;0;")
add_test(StorageTest "/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/build/latticedb_test" "--test=storage")
set_tests_properties(StorageTest PROPERTIES  _BACKTRACE_TRIPLES "/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/CMakeLists.txt;196;add_test;/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/CMakeLists.txt;0;")
add_test(VectorSearchTest "/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/build/latticedb_test" "--test=vector_search")
set_tests_properties(VectorSearchTest PROPERTIES  _BACKTRACE_TRIPLES "/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/CMakeLists.txt;197;add_test;/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/CMakeLists.txt;0;")
add_test(CompressionTest "/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/build/latticedb_test" "--test=compression")
set_tests_properties(CompressionTest PROPERTIES  _BACKTRACE_TRIPLES "/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/CMakeLists.txt;198;add_test;/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/CMakeLists.txt;0;")
add_test(SecurityTest "/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/build/latticedb_test" "--test=security")
set_tests_properties(SecurityTest PROPERTIES  _BACKTRACE_TRIPLES "/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/CMakeLists.txt;199;add_test;/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/CMakeLists.txt;0;")
add_test(StreamProcessingTest "/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/build/latticedb_test" "--test=stream_processing")
set_tests_properties(StreamProcessingTest PROPERTIES  _BACKTRACE_TRIPLES "/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/CMakeLists.txt;200;add_test;/Users/davidnguyen/WebstormProjects/LatticeDB-DBMS/CMakeLists.txt;0;")
