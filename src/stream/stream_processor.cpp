#include <mutex>

// Implementation for stream_processor.h
#include "stream_processor.h"
#include <fstream>

namespace latticedb {

void Window::add_event(const StreamEvent& event) {
  events_.push_back(event);
}
bool Window::is_in_window(const StreamEvent& event) const {
  return event.timestamp >= start_time_ && event.timestamp < end_time_;
}

AggregateOperator::AggregateOperator(AggregateType type, const std::string& column,
                                     std::shared_ptr<Schema> schema,
                                     std::function<void(const Tuple&)> callback)
    : agg_type_(type), column_name_(column), output_schema_(std::move(schema)),
      result_callback_(std::move(callback)) {}
void AggregateOperator::process_event(const StreamEvent&) {}
void AggregateOperator::process_window(const Window& window) {
  Value v = compute_aggregate(window.get_events());
  Tuple t({v});
  emit_result(t);
}
std::shared_ptr<Schema> AggregateOperator::get_output_schema() const {
  return output_schema_;
}
void AggregateOperator::emit_result(const Tuple& result) {
  if (result_callback_)
    result_callback_(result);
}
Value AggregateOperator::compute_aggregate(const std::vector<StreamEvent>& events) const {
  if (agg_type_ == AggregateType::COUNT)
    return Value(static_cast<int64_t>(events.size()));
  // Minimal: only COUNT implemented here
  return Value(static_cast<int64_t>(events.size()));
}

FilterOperator::FilterOperator(std::unique_ptr<Expression> pred, std::shared_ptr<Schema> schema,
                               std::function<void(const Tuple&)> callback)
    : predicate_(std::move(pred)), input_schema_(std::move(schema)),
      result_callback_(std::move(callback)) {}
void FilterOperator::process_event(const StreamEvent& event) {
  if (evaluate_predicate(event.data))
    emit_result(event.data);
}
void FilterOperator::process_window(const Window& window) {
  for (auto& ev : window.get_events())
    process_event(ev);
}
std::shared_ptr<Schema> FilterOperator::get_output_schema() const {
  return input_schema_;
}
void FilterOperator::emit_result(const Tuple& result) {
  if (result_callback_)
    result_callback_(result);
}
bool FilterOperator::evaluate_predicate(const Tuple&) const {
  return true;
}

JoinOperator::JoinOperator(const std::string& join_col, std::shared_ptr<Schema> left,
                           std::shared_ptr<Schema> right, std::function<void(const Tuple&)> cb,
                           std::chrono::milliseconds timeout)
    : join_column_(join_col), left_schema_(std::move(left)), right_schema_(std::move(right)),
      output_schema_(left_schema_), result_callback_(std::move(cb)), join_timeout_(timeout) {}
void JoinOperator::process_event(const StreamEvent& event) {
  join_state_[Value(event.data.get_values().empty() ? 0 : 0)].push_back(event.data);
}
void JoinOperator::process_window(const Window& window) {
  for (auto& ev : window.get_events())
    process_event(ev);
}
std::shared_ptr<Schema> JoinOperator::get_output_schema() const {
  return output_schema_;
}
void JoinOperator::emit_result(const Tuple& result) {
  if (result_callback_)
    result_callback_(result);
}
void JoinOperator::cleanup_old_state() {
  join_state_.clear();
}

WindowManager::WindowManager(const WindowConfig& config, std::shared_ptr<StreamOperator> op)
    : config_(config), operator_(std::move(op)) {}
void WindowManager::process_event(const StreamEvent& event) {
  auto windows = create_windows_for_event(event, event.partition_key);
  for (auto& w : windows) {
    if (w->is_in_window(event))
      w->add_event(event);
  }
}
void WindowManager::trigger_windows() {
  std::lock_guard<std::mutex> l(windows_mutex_);
  for (auto& kv : partitioned_windows_)
    for (auto& w : kv.second)
      if (should_trigger_window(*w))
        operator_->process_window(*w);
}
void WindowManager::cleanup_completed_windows() {
  std::lock_guard<std::mutex> l(windows_mutex_);
  for (auto& kv : partitioned_windows_)
    kv.second.erase(
        std::remove_if(kv.second.begin(), kv.second.end(),
                       [](const std::unique_ptr<Window>& w) { return w->is_complete(); }),
        kv.second.end());
}
std::vector<std::unique_ptr<Window>>
WindowManager::create_windows_for_event(const StreamEvent& event, const std::string& partition) {
  std::lock_guard<std::mutex> l(windows_mutex_);
  auto& list = partitioned_windows_[partition];
  auto now = event.timestamp;
  if (list.empty()) {
    auto start = now;
    auto end = now + config_.window_size;
    list.emplace_back(std::make_unique<Window>(start, end, partition));
  }
  return {};
}
bool WindowManager::should_trigger_window(const Window& window) const {
  return window.get_event_count() > 0;
}

StreamProcessor::StreamProcessor()
    : running_(false), events_processed_(0), windows_triggered_(0), events_dropped_(0) {}
StreamProcessor::~StreamProcessor() {
  stop();
}
void StreamProcessor::start(size_t num_threads) {
  running_.store(true);
  for (size_t i = 0; i < num_threads; ++i)
    processing_threads_.emplace_back(&StreamProcessor::processing_loop, this);
}
void StreamProcessor::stop() {
  running_.store(false);
  queue_cv_.notify_all();
  for (auto& t : processing_threads_)
    if (t.joinable())
      t.join();
  processing_threads_.clear();
}
void StreamProcessor::add_event(const StreamEvent& event) {
  {
    std::lock_guard<std::mutex> l(queue_mutex_);
    event_queue_.push(event);
  }
  queue_cv_.notify_one();
}
void StreamProcessor::add_window_manager(std::unique_ptr<WindowManager> manager) {
  window_managers_.push_back(std::move(manager));
}
void StreamProcessor::update_watermark(std::chrono::system_clock::time_point watermark) {
  std::lock_guard<std::mutex> l(watermark_mutex_);
  watermark_ = watermark;
}
std::chrono::system_clock::time_point StreamProcessor::get_watermark() const {
  return watermark_;
}
void StreamProcessor::start_kafka_consumer(const std::string&, const std::string&) {}
void StreamProcessor::start_tcp_listener(uint16_t) {}
void StreamProcessor::start_file_watcher(const std::string&) {}
void StreamProcessor::processing_loop() {
  while (running_.load()) {
    std::unique_lock<std::mutex> lk(queue_mutex_);
    queue_cv_.wait(lk, [&] { return !event_queue_.empty() || !running_.load(); });
    if (!running_.load())
      break;
    auto ev = event_queue_.front();
    event_queue_.pop();
    lk.unlock();
    for (auto& wm : window_managers_)
      wm->process_event(ev);
    events_processed_++;
  }
}
void StreamProcessor::watermark_update_loop() {}
void StreamProcessor::cleanup_old_events() {}

bool StreamSQL::create_stream_table(const std::string&, const std::string&, std::shared_ptr<Schema>,
                                    StreamProcessor*) {
  return true;
}
std::unique_ptr<WindowManager> StreamSQL::parse_window_clause(const std::string&,
                                                              std::shared_ptr<StreamOperator> op) {
  return std::make_unique<WindowManager>(WindowConfig::tumbling(std::chrono::seconds(60)),
                                         std::move(op));
}
bool StreamSQL::create_streaming_materialized_view(const std::string&, const std::string&,
                                                   const WindowConfig&, StreamProcessor*) {
  return true;
}

} // namespace latticedb
