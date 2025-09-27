#pragma once
#include <mutex>

#include "../storage/page.h"
#include "../types/value.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace latticedb {

enum class CompressionType {
  NONE,
  RLE,         // Run-Length Encoding
  DICTIONARY,  // Dictionary Encoding
  BIT_PACKING, // Bit Packing
  DELTA,       // Delta Encoding
  LZ4,         // LZ4 Compression
  ZSTD,        // Zstandard Compression
  SNAPPY,      // Google Snappy
  ADAPTIVE     // Adaptive compression based on data characteristics
};

struct CompressionMetadata {
  CompressionType type;
  uint32_t original_size;
  uint32_t compressed_size;
  double compression_ratio;
  uint32_t dictionary_size = 0;
  std::chrono::system_clock::time_point compressed_at;

  CompressionMetadata(CompressionType t = CompressionType::NONE)
      : type(t), original_size(0), compressed_size(0), compression_ratio(1.0),
        compressed_at(std::chrono::system_clock::now()) {}
};

class CompressionAlgorithm {
public:
  virtual ~CompressionAlgorithm() = default;

  virtual std::vector<uint8_t> compress(const uint8_t* data, size_t size,
                                        CompressionMetadata& metadata) = 0;

  virtual std::vector<uint8_t> decompress(const uint8_t* compressed_data, size_t size,
                                          const CompressionMetadata& metadata) = 0;

  virtual CompressionType get_type() const = 0;
  virtual bool is_suitable_for_data(const uint8_t* data, size_t size) const = 0;
  virtual double estimate_compression_ratio(const uint8_t* data, size_t size) const = 0;
};

// Run-Length Encoding for repetitive data
class RLECompression : public CompressionAlgorithm {
public:
  std::vector<uint8_t> compress(const uint8_t* data, size_t size,
                                CompressionMetadata& metadata) override;

  std::vector<uint8_t> decompress(const uint8_t* compressed_data, size_t size,
                                  const CompressionMetadata& metadata) override;

  CompressionType get_type() const override {
    return CompressionType::RLE;
  }
  bool is_suitable_for_data(const uint8_t* data, size_t size) const override;
  double estimate_compression_ratio(const uint8_t* data, size_t size) const override;

private:
  double calculate_repetition_ratio(const uint8_t* data, size_t size) const;
};

// Dictionary Encoding for categorical data
class DictionaryCompression : public CompressionAlgorithm {
private:
  std::unordered_map<std::vector<uint8_t>, uint32_t> encode_dictionary_;
  std::vector<std::vector<uint8_t>> decode_dictionary_;
  uint32_t next_code_ = 0;

public:
  std::vector<uint8_t> compress(const uint8_t* data, size_t size,
                                CompressionMetadata& metadata) override;

  std::vector<uint8_t> decompress(const uint8_t* compressed_data, size_t size,
                                  const CompressionMetadata& metadata) override;

  CompressionType get_type() const override {
    return CompressionType::DICTIONARY;
  }
  bool is_suitable_for_data(const uint8_t* data, size_t size) const override;
  double estimate_compression_ratio(const uint8_t* data, size_t size) const override;

  void clear_dictionary();
  size_t get_dictionary_size() const {
    return decode_dictionary_.size();
  }

private:
  std::vector<std::vector<uint8_t>> extract_values(const uint8_t* data, size_t size) const;
  double calculate_uniqueness_ratio(const std::vector<std::vector<uint8_t>>& values) const;
};

// Delta Encoding for sequential/monotonic data
class DeltaCompression : public CompressionAlgorithm {
public:
  std::vector<uint8_t> compress(const uint8_t* data, size_t size,
                                CompressionMetadata& metadata) override;

  std::vector<uint8_t> decompress(const uint8_t* compressed_data, size_t size,
                                  const CompressionMetadata& metadata) override;

  CompressionType get_type() const override {
    return CompressionType::DELTA;
  }
  bool is_suitable_for_data(const uint8_t* data, size_t size) const override;
  double estimate_compression_ratio(const uint8_t* data, size_t size) const override;

private:
  bool is_numeric_data(const uint8_t* data, size_t size) const;
  double calculate_delta_efficiency(const uint8_t* data, size_t size) const;
};

// Bit Packing for small-range integers
class BitPackingCompression : public CompressionAlgorithm {
public:
  std::vector<uint8_t> compress(const uint8_t* data, size_t size,
                                CompressionMetadata& metadata) override;

  std::vector<uint8_t> decompress(const uint8_t* compressed_data, size_t size,
                                  const CompressionMetadata& metadata) override;

  CompressionType get_type() const override {
    return CompressionType::BIT_PACKING;
  }
  bool is_suitable_for_data(const uint8_t* data, size_t size) const override;
  double estimate_compression_ratio(const uint8_t* data, size_t size) const override;

private:
  uint8_t calculate_required_bits(const std::vector<uint64_t>& values) const;
  std::vector<uint64_t> extract_integers(const uint8_t* data, size_t size) const;
};

// LZ4 Compression for general-purpose compression
class LZ4Compression : public CompressionAlgorithm {
public:
  std::vector<uint8_t> compress(const uint8_t* data, size_t size,
                                CompressionMetadata& metadata) override;

  std::vector<uint8_t> decompress(const uint8_t* compressed_data, size_t size,
                                  const CompressionMetadata& metadata) override;

  CompressionType get_type() const override {
    return CompressionType::LZ4;
  }
  bool is_suitable_for_data(const uint8_t* data, size_t size) const override;
  double estimate_compression_ratio(const uint8_t* data, size_t size) const override;
};

// Zstandard Compression for high compression ratios
class ZSTDCompression : public CompressionAlgorithm {
private:
  int compression_level_ = 3; // Default compression level

public:
  explicit ZSTDCompression(int level = 3) : compression_level_(level) {}

  std::vector<uint8_t> compress(const uint8_t* data, size_t size,
                                CompressionMetadata& metadata) override;

  std::vector<uint8_t> decompress(const uint8_t* compressed_data, size_t size,
                                  const CompressionMetadata& metadata) override;

  CompressionType get_type() const override {
    return CompressionType::ZSTD;
  }
  bool is_suitable_for_data(const uint8_t* data, size_t size) const override;
  double estimate_compression_ratio(const uint8_t* data, size_t size) const override;

  void set_compression_level(int level) {
    compression_level_ = level;
  }
};

class CompressionEngine {
private:
  std::unordered_map<CompressionType, std::unique_ptr<CompressionAlgorithm>> algorithms_;

  // Compression strategy configuration
  struct CompressionConfig {
    bool enable_adaptive_compression = true;
    double min_compression_ratio = 1.2; // Minimum ratio to justify compression
    size_t min_data_size = 1024;        // Minimum size to attempt compression
    CompressionType default_algorithm = CompressionType::LZ4;
    bool prefer_speed_over_ratio = false;
  } config_;

  // Statistics tracking
  struct CompressionStats {
    uint64_t total_compressions = 0;
    uint64_t total_decompressions = 0;
    uint64_t bytes_compressed = 0;
    uint64_t bytes_decompressed = 0;
    double average_compression_ratio = 1.0;
    std::unordered_map<CompressionType, uint64_t> algorithm_usage;
  } stats_;

  mutable std::mutex engine_mutex_;

public:
  CompressionEngine();
  ~CompressionEngine() = default;

  // Main compression interface
  std::vector<uint8_t> compress_data(const uint8_t* data, size_t size,
                                     CompressionMetadata& metadata,
                                     CompressionType hint = CompressionType::ADAPTIVE);

  std::vector<uint8_t> decompress_data(const uint8_t* compressed_data, size_t size,
                                       const CompressionMetadata& metadata);

  // Page-level compression
  bool compress_page(Page* page, CompressionType hint = CompressionType::ADAPTIVE);
  bool decompress_page(Page* page, const CompressionMetadata& metadata);

  // Column-level compression for columnar storage
  std::vector<uint8_t> compress_column(const std::vector<Value>& values,
                                       CompressionMetadata& metadata,
                                       CompressionType hint = CompressionType::ADAPTIVE);

  std::vector<Value> decompress_column(const uint8_t* compressed_data, size_t size,
                                       const CompressionMetadata& metadata, ValueType column_type);

  // Adaptive compression selection
  CompressionType select_best_algorithm(const uint8_t* data, size_t size) const;

  // Configuration and statistics
  void set_config(const CompressionConfig& config);
  CompressionConfig get_config() const;
  CompressionStats get_statistics() const;
  void reset_statistics();

  // Algorithm management
  void register_algorithm(std::unique_ptr<CompressionAlgorithm> algorithm);
  bool is_algorithm_available(CompressionType type) const;

  // Batch compression for multiple pages/columns
  void compress_pages_batch(const std::vector<Page*>& pages,
                            std::vector<CompressionMetadata>& metadata_out,
                            CompressionType hint = CompressionType::ADAPTIVE);

  void decompress_pages_batch(const std::vector<Page*>& pages,
                              const std::vector<CompressionMetadata>& metadata);

private:
  void initialize_default_algorithms();
  double benchmark_algorithm_speed(CompressionType type, const uint8_t* sample_data,
                                   size_t size) const;

  // Adaptive selection heuristics
  CompressionType select_by_data_pattern(const uint8_t* data, size_t size) const;
  CompressionType select_by_workload_characteristics() const;

  void update_statistics(CompressionType type, size_t original_size, size_t compressed_size,
                         bool is_compression);
};

// Compression-aware storage interface
class CompressedStorageManager {
private:
  std::unique_ptr<CompressionEngine> compression_engine_;
  std::unordered_map<page_id_t, CompressionMetadata> page_compression_metadata_;
  std::mutex metadata_mutex_;

public:
  CompressedStorageManager();

  // Compressed page operations
  bool write_compressed_page(page_id_t page_id, const uint8_t* data, size_t size,
                             CompressionType hint = CompressionType::ADAPTIVE);

  std::vector<uint8_t> read_compressed_page(page_id_t page_id);

  // Metadata management
  void set_page_compression_metadata(page_id_t page_id, const CompressionMetadata& metadata);
  std::optional<CompressionMetadata> get_page_compression_metadata(page_id_t page_id) const;

  // Compression statistics and monitoring
  double get_overall_compression_ratio() const;
  std::unordered_map<CompressionType, uint64_t> get_algorithm_usage() const;

  // Configuration
  void set_default_compression_type(CompressionType type);
  void enable_adaptive_compression(bool enable);

private:
  void persist_metadata();
  void load_metadata();
};

} // namespace latticedb