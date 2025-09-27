#include <mutex>

// Implementation for compression_engine.h
#include "compression_engine.h"
#include <cmath>
#include <cstring>

namespace latticedb {

// RLE: simple byte-wise run length encoding
std::vector<uint8_t> RLECompression::compress(const uint8_t* data, size_t size,
                                              CompressionMetadata& metadata) {
  std::vector<uint8_t> out;
  if (size == 0)
    return out;
  out.reserve(size);
  size_t i = 0;
  while (i < size) {
    uint8_t v = data[i];
    size_t j = i + 1;
    while (j < size && data[j] == v && (j - i) < 255)
      j++;
    out.push_back(v);
    out.push_back(static_cast<uint8_t>(j - i));
    i = j;
  }
  metadata.original_size = static_cast<uint32_t>(size);
  metadata.compressed_size = static_cast<uint32_t>(out.size());
  metadata.compression_ratio = size == 0 ? 1.0 : (double)size / std::max<size_t>(1, out.size());
  return out;
}

std::vector<uint8_t> RLECompression::decompress(const uint8_t* compressed_data, size_t size,
                                                const CompressionMetadata&) {
  std::vector<uint8_t> out;
  for (size_t i = 0; i + 1 < size; i += 2) {
    uint8_t v = compressed_data[i];
    uint8_t cnt = compressed_data[i + 1];
    out.insert(out.end(), cnt, v);
  }
  return out;
}

bool RLECompression::is_suitable_for_data(const uint8_t* data, size_t size) const {
  return calculate_repetition_ratio(data, size) > 0.2;
}
double RLECompression::estimate_compression_ratio(const uint8_t* data, size_t size) const {
  double r = calculate_repetition_ratio(data, size);
  return 1.0 + r;
}
double RLECompression::calculate_repetition_ratio(const uint8_t* data, size_t size) const {
  if (size < 2)
    return 0.0;
  size_t rep = 0;
  for (size_t i = 1; i < size; ++i)
    if (data[i] == data[i - 1])
      rep++;
  return (double)rep / (double)size;
}

// DictionaryCompression: pass-through simplistic implementation
std::vector<uint8_t> DictionaryCompression::compress(const uint8_t* data, size_t size,
                                                     CompressionMetadata& metadata) {
  metadata.original_size = static_cast<uint32_t>(size);
  metadata.compressed_size = static_cast<uint32_t>(size);
  metadata.compression_ratio = 1.0; // passthrough
  std::vector<uint8_t> out(size);
  if (size)
    std::memcpy(out.data(), data, size);
  return out;
}
std::vector<uint8_t> DictionaryCompression::decompress(const uint8_t* compressed_data, size_t size,
                                                       const CompressionMetadata&) {
  std::vector<uint8_t> out(size);
  if (size)
    std::memcpy(out.data(), compressed_data, size);
  return out;
}
bool DictionaryCompression::is_suitable_for_data(const uint8_t*, size_t) const {
  return false;
}
double DictionaryCompression::estimate_compression_ratio(const uint8_t*, size_t) const {
  return 1.0;
}
void DictionaryCompression::clear_dictionary() {
  encode_dictionary_.clear();
  decode_dictionary_.clear();
  next_code_ = 0;
}
std::vector<std::vector<uint8_t>> DictionaryCompression::extract_values(const uint8_t*,
                                                                        size_t) const {
  return {};
}
double
DictionaryCompression::calculate_uniqueness_ratio(const std::vector<std::vector<uint8_t>>&) const {
  return 1.0;
}

// DeltaCompression: pass-through for generic data
std::vector<uint8_t> DeltaCompression::compress(const uint8_t* data, size_t size,
                                                CompressionMetadata& metadata) {
  metadata.original_size = static_cast<uint32_t>(size);
  metadata.compressed_size = static_cast<uint32_t>(size);
  metadata.compression_ratio = 1.0;
  std::vector<uint8_t> out(size);
  if (size)
    std::memcpy(out.data(), data, size);
  return out;
}
std::vector<uint8_t> DeltaCompression::decompress(const uint8_t* compressed_data, size_t size,
                                                  const CompressionMetadata&) {
  std::vector<uint8_t> out(size);
  if (size)
    std::memcpy(out.data(), compressed_data, size);
  return out;
}
bool DeltaCompression::is_suitable_for_data(const uint8_t*, size_t) const {
  return false;
}
double DeltaCompression::estimate_compression_ratio(const uint8_t*, size_t) const {
  return 1.0;
}
bool DeltaCompression::is_numeric_data(const uint8_t*, size_t) const {
  return false;
}
double DeltaCompression::calculate_delta_efficiency(const uint8_t*, size_t) const {
  return 0.0;
}

// BitPackingCompression: pass-through minimal
std::vector<uint8_t> BitPackingCompression::compress(const uint8_t* data, size_t size,
                                                     CompressionMetadata& metadata) {
  metadata.original_size = static_cast<uint32_t>(size);
  metadata.compressed_size = static_cast<uint32_t>(size);
  metadata.compression_ratio = 1.0;
  std::vector<uint8_t> out(size);
  if (size)
    std::memcpy(out.data(), data, size);
  return out;
}
std::vector<uint8_t> BitPackingCompression::decompress(const uint8_t* compressed_data, size_t size,
                                                       const CompressionMetadata&) {
  std::vector<uint8_t> out(size);
  if (size)
    std::memcpy(out.data(), compressed_data, size);
  return out;
}
bool BitPackingCompression::is_suitable_for_data(const uint8_t*, size_t) const {
  return false;
}
double BitPackingCompression::estimate_compression_ratio(const uint8_t*, size_t) const {
  return 1.0;
}
uint8_t BitPackingCompression::calculate_required_bits(const std::vector<uint64_t>&) const {
  return 8;
}
std::vector<uint64_t> BitPackingCompression::extract_integer_values(const uint8_t*, size_t) const {
  return {};
}

// LZ4/ZSTD: pass-through in minimal build
std::vector<uint8_t> LZ4Compression::compress(const uint8_t* data, size_t size,
                                              CompressionMetadata& metadata) {
  metadata.original_size = (uint32_t)size;
  metadata.compressed_size = (uint32_t)size;
  metadata.compression_ratio = 1.0;
  std::vector<uint8_t> out(size);
  if (size)
    std::memcpy(out.data(), data, size);
  return out;
}
std::vector<uint8_t> LZ4Compression::decompress(const uint8_t* data, size_t size,
                                                const CompressionMetadata&) {
  std::vector<uint8_t> out(size);
  if (size)
    std::memcpy(out.data(), data, size);
  return out;
}
bool LZ4Compression::is_suitable_for_data(const uint8_t*, size_t) const {
  return false;
}
double LZ4Compression::estimate_compression_ratio(const uint8_t*, size_t) const {
  return 1.0;
}

std::vector<uint8_t> ZSTDCompression::compress(const uint8_t* data, size_t size,
                                               CompressionMetadata& metadata) {
  metadata.original_size = (uint32_t)size;
  metadata.compressed_size = (uint32_t)size;
  metadata.compression_ratio = 1.0;
  std::vector<uint8_t> out(size);
  if (size)
    std::memcpy(out.data(), data, size);
  return out;
}
std::vector<uint8_t> ZSTDCompression::decompress(const uint8_t* data, size_t size,
                                                 const CompressionMetadata&) {
  std::vector<uint8_t> out(size);
  if (size)
    std::memcpy(out.data(), data, size);
  return out;
}
bool ZSTDCompression::is_suitable_for_data(const uint8_t*, size_t) const {
  return false;
}
double ZSTDCompression::estimate_compression_ratio(const uint8_t*, size_t) const {
  return 1.0;
}

CompressionEngine::CompressionEngine() {
  initialize_default_algorithms();
}

std::vector<uint8_t> CompressionEngine::compress_data(const uint8_t* data, size_t size,
                                                      CompressionMetadata& metadata,
                                                      CompressionType hint) {
  std::lock_guard<std::mutex> l(engine_mutex_);
  CompressionType type =
      hint == CompressionType::ADAPTIVE ? select_best_algorithm(data, size) : hint;
  if (!is_algorithm_available(type))
    type = CompressionType::NONE;
  if (type == CompressionType::NONE) {
    metadata.type = CompressionType::NONE;
    metadata.original_size = (uint32_t)size;
    metadata.compressed_size = (uint32_t)size;
    metadata.compression_ratio = 1.0;
    std::vector<uint8_t> out(size);
    if (size)
      std::memcpy(out.data(), data, size);
    return out;
  }
  metadata.type = type;
  auto& algo = algorithms_[type];
  auto out = algo->compress(data, size, metadata);
  update_statistics(type, size, out.size(), true);
  return out;
}

std::vector<uint8_t> CompressionEngine::decompress_data(const uint8_t* compressed_data, size_t size,
                                                        const CompressionMetadata& metadata) {
  std::lock_guard<std::mutex> l(engine_mutex_);
  if (metadata.type == CompressionType::NONE) {
    std::vector<uint8_t> out(size);
    if (size)
      std::memcpy(out.data(), compressed_data, size);
    return out;
  }
  auto it = algorithms_.find(metadata.type);
  if (it == algorithms_.end()) {
    std::vector<uint8_t> out(size);
    if (size)
      std::memcpy(out.data(), compressed_data, size);
    return out;
  }
  auto out = it->second->decompress(compressed_data, size, metadata);
  update_statistics(metadata.type, out.size(), size, false);
  return out;
}

bool CompressionEngine::compress_page(Page* page, CompressionType hint) {
  CompressionMetadata meta(hint);
  auto out =
      compress_data(reinterpret_cast<const uint8_t*>(page->get_data()), PAGE_SIZE, meta, hint);
  if (out.size() <= PAGE_SIZE) {
    std::memcpy(page->get_data(), out.data(), out.size());
    return true;
  }
  return false;
}
bool CompressionEngine::decompress_page(Page* page, const CompressionMetadata& metadata) {
  auto out =
      decompress_data(reinterpret_cast<const uint8_t*>(page->get_data()), PAGE_SIZE, metadata);
  std::memcpy(page->get_data(), out.data(), std::min<size_t>(out.size(), PAGE_SIZE));
  return true;
}

std::vector<uint8_t> CompressionEngine::compress_column(const std::vector<Value>& values,
                                                        CompressionMetadata& metadata,
                                                        CompressionType hint) {
  std::vector<uint8_t> buf; // naive serialize as strings
  for (auto& v : values) {
    auto s = v.to_string();
    uint32_t len = (uint32_t)s.size();
    buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&len), reinterpret_cast<uint8_t*>(&len) + 4);
    buf.insert(buf.end(), s.begin(), s.end());
  }
  return compress_data(buf.data(), buf.size(), metadata, hint);
}

std::vector<Value> CompressionEngine::decompress_column(const uint8_t* compressed_data, size_t size,
                                                        const CompressionMetadata& metadata,
                                                        ValueType column_type) {
  auto bytes = decompress_data(compressed_data, size, metadata);
  std::vector<Value> vals;
  size_t off = 0;
  while (off + 4 <= bytes.size()) {
    uint32_t len;
    std::memcpy(&len, bytes.data() + off, 4);
    off += 4;
    std::string s(bytes.begin() + off, bytes.begin() + std::min<size_t>(off + len, bytes.size()));
    off += len;
    if (column_type == ValueType::INTEGER)
      vals.emplace_back((int32_t)std::stoi(s));
    else
      vals.emplace_back(s);
  }
  return vals;
}

CompressionType CompressionEngine::select_best_algorithm(const uint8_t* data, size_t size) const {
  // Minimal heuristic: prefer RLE if repetition present, else NONE
  RLECompression rle;
  if (rle.is_suitable_for_data(data, size))
    return CompressionType::RLE;
  return CompressionType::NONE;
}

void CompressionEngine::set_config(const CompressionConfig& config) {
  config_ = config;
}
CompressionEngine::CompressionConfig CompressionEngine::get_config() const {
  return config_;
}
CompressionEngine::CompressionStats CompressionEngine::get_statistics() const {
  return stats_;
}
void CompressionEngine::reset_statistics() {
  stats_ = CompressionStats{};
}
void CompressionEngine::register_algorithm(std::unique_ptr<CompressionAlgorithm> algorithm) {
  algorithms_[algorithm->get_type()] = std::move(algorithm);
}
bool CompressionEngine::is_algorithm_available(CompressionType type) const {
  return algorithms_.find(type) != algorithms_.end();
}

void CompressionEngine::compress_pages_batch(const std::vector<Page*>& pages,
                                             std::vector<CompressionMetadata>& metadata_out,
                                             CompressionType hint) {
  metadata_out.clear();
  metadata_out.reserve(pages.size());
  for (auto* p : pages) {
    CompressionMetadata meta(hint);
    compress_page(p, hint);
    metadata_out.push_back(meta);
  }
}
void CompressionEngine::decompress_pages_batch(const std::vector<Page*>&,
                                               const std::vector<CompressionMetadata>&) {}

void CompressionEngine::initialize_default_algorithms() {
  algorithms_[CompressionType::RLE] = std::make_unique<RLECompression>();
  algorithms_[CompressionType::DICTIONARY] = std::make_unique<DictionaryCompression>();
  algorithms_[CompressionType::DELTA] = std::make_unique<DeltaCompression>();
  algorithms_[CompressionType::BIT_PACKING] = std::make_unique<BitPackingCompression>();
  algorithms_[CompressionType::LZ4] = std::make_unique<LZ4Compression>();
  algorithms_[CompressionType::ZSTD] = std::make_unique<ZSTDCompression>();
}

double CompressionEngine::benchmark_algorithm_speed(CompressionType, const uint8_t*, size_t) const {
  return 1.0;
}
CompressionType CompressionEngine::select_by_data_pattern(const uint8_t*, size_t) const {
  return CompressionType::NONE;
}
CompressionType CompressionEngine::select_by_workload_characteristics() const {
  return CompressionType::NONE;
}
void CompressionEngine::update_statistics(CompressionType type, size_t original_size,
                                          size_t compressed_size, bool is_compression) {
  if (is_compression)
    stats_.total_compressions++;
  else
    stats_.total_decompressions++;
  stats_.bytes_compressed += original_size;
  stats_.bytes_decompressed += compressed_size;
  stats_.average_compression_ratio =
      (double)stats_.bytes_compressed / std::max<uint64_t>(1, stats_.bytes_decompressed);
  stats_.algorithm_usage[type]++;
}

// CompressedStorageManager
CompressedStorageManager::CompressedStorageManager()
    : compression_engine_(std::make_unique<CompressionEngine>()) {}

bool CompressedStorageManager::write_compressed_page(page_id_t page_id, const uint8_t* data,
                                                     size_t size, CompressionType hint) {
  CompressionMetadata meta(hint);
  auto bytes = compression_engine_->compress_data(data, size, meta, hint);
  set_page_compression_metadata(page_id, meta);
  // User responsible for persisting bytes with DiskManager
  return !bytes.empty() || size == 0;
}

std::vector<uint8_t> CompressedStorageManager::read_compressed_page(page_id_t page_id) {
  auto meta = get_page_compression_metadata(page_id);
  if (!meta)
    return {};
  // caller must provide compressed bytes in real system; here, return empty
  return {};
}

void CompressedStorageManager::set_page_compression_metadata(page_id_t page_id,
                                                             const CompressionMetadata& metadata) {
  std::lock_guard<std::mutex> l(metadata_mutex_);
  page_compression_metadata_[page_id] = metadata;
}
std::optional<CompressionMetadata>
CompressedStorageManager::get_page_compression_metadata(page_id_t page_id) const {
  std::lock_guard<std::mutex> l(metadata_mutex_);
  auto it = page_compression_metadata_.find(page_id);
  if (it == page_compression_metadata_.end())
    return std::nullopt;
  return it->second;
}
double CompressedStorageManager::get_overall_compression_ratio() const {
  return compression_engine_->get_statistics().average_compression_ratio;
}
std::unordered_map<CompressionType, uint64_t>
CompressedStorageManager::get_algorithm_usage() const {
  return compression_engine_->get_statistics().algorithm_usage;
}
void CompressedStorageManager::set_default_compression_type(CompressionType type) {
  auto cfg = compression_engine_->get_config();
  cfg.default_algorithm = type;
  compression_engine_->set_config(cfg);
}
void CompressedStorageManager::enable_adaptive_compression(bool enable) {
  auto cfg = compression_engine_->get_config();
  cfg.enable_adaptive_compression = enable;
  compression_engine_->set_config(cfg);
}
void CompressedStorageManager::persist_metadata() {}
void CompressedStorageManager::load_metadata() {}

} // namespace latticedb
