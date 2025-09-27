#pragma once
#include <mutex>

#include "../query/query_executor.h"
#include "../types/schema.h"
#include "../types/tuple.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <queue>
#include <thread>

namespace latticedb {

struct StreamEvent {
  std::chrono::system_clock::time_point timestamp;
  Tuple data;
  uint64_t sequence_number;
  std::string partition_key;

  StreamEvent(const Tuple& tuple, const std::string& partition = "")
      : timestamp(std::chrono::system_clock::now()), data(tuple), sequence_number(0),
        partition_key(partition) {}
};

enum class WindowType {
  TUMBLING, // Non-overlapping fixed-size windows
  SLIDING,  // Overlapping fixed-size windows
  SESSION,  // Variable-size windows based on gaps
  GLOBAL    // Single window containing all data
};

struct WindowConfig {
  WindowType type;
  std::chrono::milliseconds window_size;
  std::chrono::milliseconds slide_interval;  // For sliding windows
  std::chrono::milliseconds session_timeout; // For session windows
  std::string partition_by_column;

  static WindowConfig tumbling(std::chrono::milliseconds size, const std::string& partition = "") {
    return {WindowType::TUMBLING, size, size, std::chrono::milliseconds(0), partition};
  }

  static WindowConfig sliding(std::chrono::milliseconds size, std::chrono::milliseconds slide,
                              const std::string& partition = "") {
    return {WindowType::SLIDING, size, slide, std::chrono::milliseconds(0), partition};
  }

  static WindowConfig session(std::chrono::milliseconds timeout,
                              const std::string& partition = "") {
    return {WindowType::SESSION, std::chrono::milliseconds(0), std::chrono::milliseconds(0),
            timeout, partition};
  }
};

class Window {
private:
  std::chrono::system_clock::time_point start_time_;
  std::chrono::system_clock::time_point end_time_;
  std::vector<StreamEvent> events_;
  std::string partition_key_;
  bool is_complete_;

public:
  Window(std::chrono::system_clock::time_point start, std::chrono::system_clock::time_point end,
         const std::string& partition = "")
      : start_time_(start), end_time_(end), partition_key_(partition), is_complete_(false) {}

  void add_event(const StreamEvent& event);
  bool is_in_window(const StreamEvent& event) const;
  bool is_complete() const {
    return is_complete_;
  }
  void mark_complete() {
    is_complete_ = true;
  }

  const std::vector<StreamEvent>& get_events() const {
    return events_;
  }
  std::chrono::system_clock::time_point get_start_time() const {
    return start_time_;
  }
  std::chrono::system_clock::time_point get_end_time() const {
    return end_time_;
  }
  const std::string& get_partition_key() const {
    return partition_key_;
  }
  size_t get_event_count() const {
    return events_.size();
  }
};

class StreamOperator {
public:
  virtual ~StreamOperator() = default;
  virtual void process_event(const StreamEvent& event) = 0;
  virtual void process_window(const Window& window) = 0;
  virtual std::shared_ptr<Schema> get_output_schema() const = 0;
  virtual void emit_result(const Tuple& result) = 0;
};

class AggregateOperator : public StreamOperator {
public:
  enum class AggregateType { COUNT, SUM, AVG, MIN, MAX, STDDEV };

private:
  AggregateType agg_type_;
  std::string column_name_;
  std::shared_ptr<Schema> output_schema_;
  std::function<void(const Tuple&)> result_callback_;

public:
  AggregateOperator(AggregateType type, const std::string& column, std::shared_ptr<Schema> schema,
                    std::function<void(const Tuple&)> callback);

  void process_event(const StreamEvent& event) override;
  void process_window(const Window& window) override;
  std::shared_ptr<Schema> get_output_schema() const override;
  void emit_result(const Tuple& result) override;

private:
  Value compute_aggregate(const std::vector<StreamEvent>& events) const;
};

class FilterOperator : public StreamOperator {
private:
  std::unique_ptr<Expression> predicate_;
  std::shared_ptr<Schema> input_schema_;
  std::function<void(const Tuple&)> result_callback_;

public:
  FilterOperator(std::unique_ptr<Expression> pred, std::shared_ptr<Schema> schema,
                 std::function<void(const Tuple&)> callback);

  void process_event(const StreamEvent& event) override;
  void process_window(const Window& window) override;
  std::shared_ptr<Schema> get_output_schema() const override;
  void emit_result(const Tuple& result) override;

private:
  bool evaluate_predicate(const Tuple& tuple) const;
};

class JoinOperator : public StreamOperator {
private:
  std::string join_column_;
  std::shared_ptr<Schema> left_schema_;
  std::shared_ptr<Schema> right_schema_;
  std::shared_ptr<Schema> output_schema_;
  std::unordered_map<Value, std::vector<Tuple>> join_state_;
  std::function<void(const Tuple&)> result_callback_;
  std::chrono::milliseconds join_timeout_;

public:
  JoinOperator(const std::string& join_col, std::shared_ptr<Schema> left_schema,
               std::shared_ptr<Schema> right_schema, std::function<void(const Tuple&)> callback,
               std::chrono::milliseconds timeout = std::chrono::minutes(5));

  void process_event(const StreamEvent& event) override;
  void process_window(const Window& window) override;
  std::shared_ptr<Schema> get_output_schema() const override;
  void emit_result(const Tuple& result) override;

private:
  void cleanup_old_state();
};

class WindowManager {
private:
  WindowConfig config_;
  std::map<std::string, std::vector<std::unique_ptr<Window>>> partitioned_windows_;
  std::mutex windows_mutex_;
  std::shared_ptr<StreamOperator> operator_;

public:
  explicit WindowManager(const WindowConfig& config, std::shared_ptr<StreamOperator> op);

  void process_event(const StreamEvent& event);
  void trigger_windows();
  void cleanup_completed_windows();

private:
  std::vector<std::unique_ptr<Window>> create_windows_for_event(const StreamEvent& event,
                                                                const std::string& partition);
  bool should_trigger_window(const Window& window) const;
};

class StreamProcessor {
private:
  std::queue<StreamEvent> event_queue_;
  std::vector<std::unique_ptr<WindowManager>> window_managers_;
  std::vector<std::thread> processing_threads_;
  std::atomic<bool> running_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;

  // Watermark management
  std::chrono::system_clock::time_point watermark_;
  std::mutex watermark_mutex_;

  // Metrics
  std::atomic<uint64_t> events_processed_;
  std::atomic<uint64_t> windows_triggered_;
  std::atomic<uint64_t> events_dropped_;

public:
  StreamProcessor();
  ~StreamProcessor();

  void start(size_t num_threads = 4);
  void stop();

  void add_event(const StreamEvent& event);
  void add_window_manager(std::unique_ptr<WindowManager> manager);

  // Watermark management
  void update_watermark(std::chrono::system_clock::time_point watermark);
  std::chrono::system_clock::time_point get_watermark() const;

  // Stream sources
  void start_kafka_consumer(const std::string& topic, const std::string& bootstrap_servers);
  void start_tcp_listener(uint16_t port);
  void start_file_watcher(const std::string& file_path);

  // Metrics
  uint64_t get_events_processed() const {
    return events_processed_.load();
  }
  uint64_t get_windows_triggered() const {
    return windows_triggered_.load();
  }
  uint64_t get_events_dropped() const {
    return events_dropped_.load();
  }

private:
  void processing_loop();
  void watermark_update_loop();
  void cleanup_old_events();
};

// Stream SQL Extensions
class StreamSQL {
public:
  // CREATE STREAM table_name AS SELECT ... FROM stream_source
  static bool create_stream_table(const std::string& table_name, const std::string& source_config,
                                  std::shared_ptr<Schema> schema, StreamProcessor* processor);

  // SELECT ... FROM stream_table WINDOW TUMBLING(INTERVAL '5 MINUTES')
  static std::unique_ptr<WindowManager>
  parse_window_clause(const std::string& window_spec, std::shared_ptr<StreamOperator> operator_);

  // Materialized view with streaming updates
  static bool create_streaming_materialized_view(const std::string& view_name,
                                                 const std::string& query,
                                                 const WindowConfig& window_config,
                                                 StreamProcessor* processor);
};

} // namespace latticedb