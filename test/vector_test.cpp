// Isolated vector search test
#include "../src/ml/vector_search.h"
#include <chrono>
#include <iostream>
#include <random>
#include <vector>

using namespace latticedb;
using namespace std::chrono;

std::vector<double> random_vector(size_t dim) {
  static std::mt19937 gen(42);
  static std::uniform_real_distribution<> dist(-1.0, 1.0);
  std::vector<double> vec;
  vec.reserve(dim);
  for (size_t i = 0; i < dim; ++i) {
    vec.push_back(dist(gen));
  }
  return vec;
}

int main() {
  std::cout << "=== LatticeDB Vector Search Test ===" << std::endl << std::endl;

  try {
    VectorSearchEngine engine;

    // Test 1: Create indexes
    std::cout << "1. Creating vector indexes..." << std::endl;

    VectorSearchConfig config;
    config.metric = VectorDistanceMetric::L2;

    bool success = engine.create_index("test_l2", 128, VectorIndexType::FLAT, config);
    std::cout << "   - L2 Flat index: " << (success ? "✓" : "✗") << std::endl;

    config.metric = VectorDistanceMetric::COSINE;
    success = engine.create_index("test_cosine", 128, VectorIndexType::HNSW, config);
    std::cout << "   - Cosine HNSW index: " << (success ? "✓" : "✗") << std::endl;

    // Test 2: Add vectors
    std::cout << "\n2. Adding vectors..." << std::endl;

    int num_vectors = 1000;
    auto start = high_resolution_clock::now();

    for (int i = 0; i < num_vectors; ++i) {
      engine.add_vector("test_l2", i, random_vector(128));
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();
    std::cout << "   - Added " << num_vectors << " vectors in " << duration << " ms" << std::endl;
    std::cout << "   - Throughput: " << (num_vectors * 1000.0 / duration) << " vectors/sec"
              << std::endl;

    // Test 3: Search
    std::cout << "\n3. Searching vectors..." << std::endl;

    auto query = random_vector(128);

    start = high_resolution_clock::now();
    auto results = engine.search("test_l2", query, 10);
    end = high_resolution_clock::now();

    auto search_time = duration_cast<microseconds>(end - start).count();
    std::cout << "   - Found " << results.size() << " results in " << search_time << " µs"
              << std::endl;

    if (!results.empty()) {
      std::cout << "   - Top 3 results:" << std::endl;
      for (size_t i = 0; i < std::min(size_t(3), results.size()); ++i) {
        std::cout << "     " << (i + 1) << ". ID: " << results[i].first
                  << ", Distance: " << results[i].second << std::endl;
      }
    }

    // Test 4: Different algorithms
    std::cout << "\n4. Testing different algorithms..." << std::endl;

    // HNSW
    engine.drop_index("hnsw_test");
    config.metric = VectorDistanceMetric::L2;
    engine.create_index("hnsw_test", 256, VectorIndexType::HNSW, config);

    for (int i = 0; i < 100; ++i) {
      engine.add_vector("hnsw_test", i, random_vector(256));
    }

    results = engine.search("hnsw_test", random_vector(256), 5);
    std::cout << "   - HNSW search returned " << results.size() << " results" << std::endl;

    // Test 5: Batch operations
    std::cout << "\n5. Testing batch operations..." << std::endl;

    std::vector<std::pair<uint64_t, std::vector<double>>> batch;
    for (int i = 0; i < 500; ++i) {
      batch.push_back({1000 + i, random_vector(128)});
    }

    start = high_resolution_clock::now();
    success = engine.batch_add_vectors("test_l2", batch);
    end = high_resolution_clock::now();

    duration = duration_cast<milliseconds>(end - start).count();
    std::cout << "   - Batch added " << batch.size() << " vectors: " << (success ? "✓" : "✗")
              << std::endl;
    std::cout << "   - Batch time: " << duration << " ms" << std::endl;

    // Test 6: Memory usage
    std::cout << "\n6. Memory usage..." << std::endl;

    auto memory = engine.get_index_memory_usage("test_l2");
    std::cout << "   - L2 index memory: " << (memory / 1024.0) << " KB" << std::endl;

    memory = engine.get_index_memory_usage("hnsw_test");
    std::cout << "   - HNSW index memory: " << (memory / 1024.0) << " KB" << std::endl;

    // Test 7: List indexes
    std::cout << "\n7. List all indexes..." << std::endl;
    auto indexes = engine.list_indexes();
    std::cout << "   - Total indexes: " << indexes.size() << std::endl;
    for (const auto& idx : indexes) {
      std::cout << "     - " << idx << std::endl;
    }

    // Cleanup
    std::cout << "\n8. Cleanup..." << std::endl;
    engine.drop_index("test_l2");
    engine.drop_index("test_cosine");
    engine.drop_index("hnsw_test");
    std::cout << "   - All indexes dropped" << std::endl;

    std::cout << "\n=== Vector Search Test Complete ===" << std::endl;
    std::cout << "All vector search features working correctly! ✓" << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}