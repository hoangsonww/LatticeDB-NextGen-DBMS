#pragma once
#include <mutex>

#include "../index/b_plus_tree.h"
#include "../types/tuple.h"
#include "../types/value.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace latticedb {

enum class VectorDistanceMetric { L2, COSINE, DOT_PRODUCT, MANHATTAN };

enum class VectorIndexType { FLAT, HNSW, IVF_FLAT, IVF_PQ };

struct VectorSearchConfig {
  VectorDistanceMetric metric = VectorDistanceMetric::L2;
  uint32_t max_connections = 16;  // M parameter for HNSW
  uint32_t ef_construction = 200; // efConstruction for HNSW
  uint32_t ef_search = 64;        // ef for HNSW search
  uint32_t nlist = 100;           // number of clusters for IVF
  uint32_t nprobe = 10;           // number of clusters to search for IVF
};

class VectorIndex {
public:
  virtual ~VectorIndex() = default;

  virtual void add_vector(uint64_t id, const std::vector<double>& vector) = 0;
  virtual void remove_vector(uint64_t id) = 0;
  virtual std::vector<std::pair<uint64_t, double>>
  search(const std::vector<double>& query, uint32_t k,
         double threshold = std::numeric_limits<double>::max()) = 0;

  virtual void build_index() = 0;
  virtual size_t memory_usage() const = 0;
  virtual VectorIndexType get_type() const = 0;
};

class FlatVectorIndex : public VectorIndex {
private:
  std::unordered_map<uint64_t, std::vector<double>> vectors_;
  VectorSearchConfig config_;
  uint32_t dimension_;

public:
  explicit FlatVectorIndex(uint32_t dimension, const VectorSearchConfig& config = {});

  void add_vector(uint64_t id, const std::vector<double>& vector) override;
  void remove_vector(uint64_t id) override;
  std::vector<std::pair<uint64_t, double>>
  search(const std::vector<double>& query, uint32_t k,
         double threshold = std::numeric_limits<double>::max()) override;

  void build_index() override;
  size_t memory_usage() const override;
  VectorIndexType get_type() const override {
    return VectorIndexType::FLAT;
  }

private:
  double compute_distance(const std::vector<double>& a, const std::vector<double>& b) const;
};

class HNSWVectorIndex : public VectorIndex {
private:
  struct HNSWNode {
    uint64_t id;
    std::vector<double> vector;
    std::vector<std::vector<uint64_t>> connections; // connections at each level

    HNSWNode(uint64_t id, const std::vector<double>& vec) : id(id), vector(vec) {}
  };

  std::unordered_map<uint64_t, std::unique_ptr<HNSWNode>> nodes_;
  std::vector<uint64_t> entry_points_;
  VectorSearchConfig config_;
  uint32_t dimension_;
  mutable std::mutex index_mutex_;

public:
  explicit HNSWVectorIndex(uint32_t dimension, const VectorSearchConfig& config = {});

  void add_vector(uint64_t id, const std::vector<double>& vector) override;
  void remove_vector(uint64_t id) override;
  std::vector<std::pair<uint64_t, double>>
  search(const std::vector<double>& query, uint32_t k,
         double threshold = std::numeric_limits<double>::max()) override;

  void build_index() override;
  size_t memory_usage() const override;
  VectorIndexType get_type() const override {
    return VectorIndexType::HNSW;
  }

private:
  uint32_t get_random_level() const;
  double compute_distance(const std::vector<double>& a, const std::vector<double>& b) const;
  std::vector<std::pair<uint64_t, double>> search_layer(const std::vector<double>& query,
                                                        const std::vector<uint64_t>& entry_points,
                                                        uint32_t num_closest, uint32_t level) const;
};

class VectorSearchEngine {
private:
  std::unordered_map<std::string, std::unique_ptr<VectorIndex>> indexes_;
  std::mutex indexes_mutex_;

public:
  VectorSearchEngine() = default;
  ~VectorSearchEngine() = default;

  bool create_index(const std::string& index_name, uint32_t dimension,
                    VectorIndexType type = VectorIndexType::HNSW,
                    const VectorSearchConfig& config = {});

  bool drop_index(const std::string& index_name);

  bool add_vector(const std::string& index_name, uint64_t id, const std::vector<double>& vector);

  bool remove_vector(const std::string& index_name, uint64_t id);

  std::vector<std::pair<uint64_t, double>>
  search(const std::string& index_name, const std::vector<double>& query, uint32_t k,
         double threshold = std::numeric_limits<double>::max());

  std::vector<std::string> list_indexes() const;
  size_t get_index_memory_usage(const std::string& index_name) const;

  // Batch operations for better performance
  bool batch_add_vectors(const std::string& index_name,
                         const std::vector<std::pair<uint64_t, std::vector<double>>>& vectors);

  void build_all_indexes();
};

// Vector similarity functions for SQL expressions
class VectorFunctions {
public:
  static double l2_distance(const std::vector<double>& a, const std::vector<double>& b);
  static double cosine_similarity(const std::vector<double>& a, const std::vector<double>& b);
  static double dot_product(const std::vector<double>& a, const std::vector<double>& b);
  static double manhattan_distance(const std::vector<double>& a, const std::vector<double>& b);

  static std::vector<double> normalize_vector(const std::vector<double>& vector);
  static std::vector<double> add_vectors(const std::vector<double>& a,
                                         const std::vector<double>& b);
  static std::vector<double> subtract_vectors(const std::vector<double>& a,
                                              const std::vector<double>& b);
  static std::vector<double> multiply_scalar(const std::vector<double>& vector, double scalar);
};

} // namespace latticedb