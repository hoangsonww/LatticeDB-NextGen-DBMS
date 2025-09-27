#include <mutex>

// Implementation for vector_search.h
#include "vector_search.h"
#include <algorithm>
#include <cmath>

namespace latticedb {

// FlatVectorIndex
FlatVectorIndex::FlatVectorIndex(uint32_t dimension, const VectorSearchConfig& config)
    : config_(config), dimension_(dimension) {}

void FlatVectorIndex::add_vector(uint64_t id, const std::vector<double>& vector) {
  if (vector.size() == dimension_)
    vectors_[id] = vector;
}
void FlatVectorIndex::remove_vector(uint64_t id) {
  vectors_.erase(id);
}
std::vector<std::pair<uint64_t, double>> FlatVectorIndex::search(const std::vector<double>& query,
                                                                 uint32_t k, double threshold) {
  std::vector<std::pair<uint64_t, double>> results;
  if (query.size() != dimension_)
    return results;
  for (auto& kv : vectors_) {
    double d = compute_distance(query, kv.second);
    if (d <= threshold)
      results.emplace_back(kv.first, d);
  }
  std::sort(results.begin(), results.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
  if (results.size() > k)
    results.resize(k);
  return results;
}
void FlatVectorIndex::build_index() {}
size_t FlatVectorIndex::memory_usage() const {
  size_t s = 0;
  for (auto& kv : vectors_)
    s += kv.second.size() * sizeof(double);
  return s;
}
double FlatVectorIndex::compute_distance(const std::vector<double>& a,
                                         const std::vector<double>& b) const {
  switch (config_.metric) {
  case VectorDistanceMetric::L2:
    return VectorFunctions::l2_distance(a, b);
  case VectorDistanceMetric::COSINE:
    return 1.0 - VectorFunctions::cosine_similarity(a, b);
  case VectorDistanceMetric::DOT_PRODUCT:
    return -VectorFunctions::dot_product(a, b);
  case VectorDistanceMetric::MANHATTAN:
    return VectorFunctions::manhattan_distance(a, b);
  }
  return VectorFunctions::l2_distance(a, b);
}

// HNSWVectorIndex (minimal: delegate to flat behavior)
HNSWVectorIndex::HNSWVectorIndex(uint32_t dimension, const VectorSearchConfig& config)
    : config_(config), dimension_(dimension) {}
void HNSWVectorIndex::add_vector(uint64_t id, const std::vector<double>& vector) {
  nodes_[id] = std::make_unique<HNSWNode>(id, vector);
}
void HNSWVectorIndex::remove_vector(uint64_t id) {
  nodes_.erase(id);
}
std::vector<std::pair<uint64_t, double>> HNSWVectorIndex::search(const std::vector<double>& query,
                                                                 uint32_t k, double threshold) {
  // Linear scan of nodes
  std::vector<std::pair<uint64_t, double>> results;
  for (auto& kv : nodes_) {
    double d = compute_distance(query, kv.second->vector);
    if (d <= threshold)
      results.emplace_back(kv.first, d);
  }
  std::sort(results.begin(), results.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
  if (results.size() > k)
    results.resize(k);
  return results;
}
void HNSWVectorIndex::build_index() {}
size_t HNSWVectorIndex::memory_usage() const {
  size_t s = 0;
  for (auto& kv : nodes_)
    s += kv.second->vector.size() * sizeof(double);
  return s;
}
uint32_t HNSWVectorIndex::get_random_level() const {
  return 0;
}
double HNSWVectorIndex::compute_distance(const std::vector<double>& a,
                                         const std::vector<double>& b) const {
  return VectorFunctions::l2_distance(a, b);
}
std::vector<std::pair<uint64_t, double>>
HNSWVectorIndex::search_layer(const std::vector<double>& q, const std::vector<uint64_t>& e,
                              uint32_t, uint32_t) const {
  (void)q;
  (void)e;
  return {};
}

// VectorSearchEngine
bool VectorSearchEngine::create_index(const std::string& index_name, uint32_t dimension,
                                      VectorIndexType type, const VectorSearchConfig& config) {
  std::lock_guard<std::mutex> l(indexes_mutex_);
  if (indexes_.count(index_name))
    return false;
  if (type == VectorIndexType::HNSW)
    indexes_[index_name] = std::make_unique<HNSWVectorIndex>(dimension, config);
  else
    indexes_[index_name] = std::make_unique<FlatVectorIndex>(dimension, config);
  return true;
}
bool VectorSearchEngine::drop_index(const std::string& index_name) {
  std::lock_guard<std::mutex> l(indexes_mutex_);
  return indexes_.erase(index_name) > 0;
}
bool VectorSearchEngine::add_vector(const std::string& index_name, uint64_t id,
                                    const std::vector<double>& vector) {
  std::lock_guard<std::mutex> l(indexes_mutex_);
  auto it = indexes_.find(index_name);
  if (it == indexes_.end())
    return false;
  it->second->add_vector(id, vector);
  return true;
}
bool VectorSearchEngine::remove_vector(const std::string& index_name, uint64_t id) {
  std::lock_guard<std::mutex> l(indexes_mutex_);
  auto it = indexes_.find(index_name);
  if (it == indexes_.end())
    return false;
  it->second->remove_vector(id);
  return true;
}
std::vector<std::pair<uint64_t, double>>
VectorSearchEngine::search(const std::string& index_name, const std::vector<double>& query,
                           uint32_t k, double threshold) {
  std::lock_guard<std::mutex> l(indexes_mutex_);
  auto it = indexes_.find(index_name);
  if (it == indexes_.end())
    return {};
  return it->second->search(query, k, threshold);
}
std::vector<std::string> VectorSearchEngine::list_indexes() const {
  std::lock_guard<std::mutex> l(const_cast<std::mutex&>(indexes_mutex_));
  std::vector<std::string> names;
  for (auto& kv : indexes_)
    names.push_back(kv.first);
  return names;
}
size_t VectorSearchEngine::get_index_memory_usage(const std::string& index_name) const {
  std::lock_guard<std::mutex> l(const_cast<std::mutex&>(indexes_mutex_));
  auto it = indexes_.find(index_name);
  if (it == indexes_.end())
    return 0;
  return it->second->memory_usage();
}
bool VectorSearchEngine::batch_add_vectors(
    const std::string& index_name,
    const std::vector<std::pair<uint64_t, std::vector<double>>>& vectors) {
  for (auto& p : vectors)
    add_vector(index_name, p.first, p.second);
  return true;
}
void VectorSearchEngine::build_all_indexes() {
  std::lock_guard<std::mutex> l(indexes_mutex_);
  for (auto& kv : indexes_)
    kv.second->build_index();
}

// VectorFunctions
double VectorFunctions::l2_distance(const std::vector<double>& a, const std::vector<double>& b) {
  if (a.size() != b.size())
    return std::numeric_limits<double>::infinity();
  double s = 0;
  for (size_t i = 0; i < a.size(); ++i) {
    double d = a[i] - b[i];
    s += d * d;
  }
  return std::sqrt(s);
}
double VectorFunctions::cosine_similarity(const std::vector<double>& a,
                                          const std::vector<double>& b) {
  if (a.size() != b.size() || a.empty())
    return 0.0;
  double dot = 0, na = 0, nb = 0;
  for (size_t i = 0; i < a.size(); ++i) {
    dot += a[i] * b[i];
    na += a[i] * a[i];
    nb += b[i] * b[i];
  }
  if (na == 0 || nb == 0)
    return 0.0;
  return dot / (std::sqrt(na) * std::sqrt(nb));
}
double VectorFunctions::dot_product(const std::vector<double>& a, const std::vector<double>& b) {
  if (a.size() != b.size())
    return 0.0;
  double s = 0;
  for (size_t i = 0; i < a.size(); ++i)
    s += a[i] * b[i];
  return s;
}
double VectorFunctions::manhattan_distance(const std::vector<double>& a,
                                           const std::vector<double>& b) {
  if (a.size() != b.size())
    return std::numeric_limits<double>::infinity();
  double s = 0;
  for (size_t i = 0; i < a.size(); ++i)
    s += std::abs(a[i] - b[i]);
  return s;
}
std::vector<double> VectorFunctions::normalize_vector(const std::vector<double>& v) {
  double norm = 0;
  for (auto x : v)
    norm += x * x;
  norm = std::sqrt(norm);
  std::vector<double> out(v.size());
  if (norm == 0)
    return out;
  for (size_t i = 0; i < v.size(); ++i)
    out[i] = v[i] / norm;
  return out;
}
std::vector<double> VectorFunctions::add_vectors(const std::vector<double>& a,
                                                 const std::vector<double>& b) {
  std::vector<double> out;
  if (a.size() != b.size())
    return out;
  out.resize(a.size());
  for (size_t i = 0; i < a.size(); ++i)
    out[i] = a[i] + b[i];
  return out;
}
std::vector<double> VectorFunctions::subtract_vectors(const std::vector<double>& a,
                                                      const std::vector<double>& b) {
  std::vector<double> out;
  if (a.size() != b.size())
    return out;
  out.resize(a.size());
  for (size_t i = 0; i < a.size(); ++i)
    out[i] = a[i] - b[i];
  return out;
}
std::vector<double> VectorFunctions::multiply_scalar(const std::vector<double>& v, double s) {
  std::vector<double> out(v.size());
  for (size_t i = 0; i < v.size(); ++i)
    out[i] = v[i] * s;
  return out;
}

} // namespace latticedb
