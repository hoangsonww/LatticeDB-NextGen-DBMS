#pragma once

#include "../engine/database_engine.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace latticedb {

enum class MessageType : uint8_t {
  CONNECT = 0x01,
  DISCONNECT = 0x02,
  QUERY = 0x03,
  RESULT = 0x04,
  ERROR = 0x05,
  BEGIN_TRANSACTION = 0x06,
  COMMIT = 0x07,
  ROLLBACK = 0x08,
  PREPARE = 0x09,
  EXECUTE = 0x0A,
  PING = 0x0B,
  PONG = 0x0C
};

struct ClientMessage {
  MessageType type;
  uint32_t length;
  std::string payload;
};

struct ServerMessage {
  MessageType type;
  uint32_t length;
  std::string payload;
  QueryResult result;
};

class ClientSession {
private:
  int client_fd_;
  DatabaseEngine* db_engine_;
  Transaction* current_txn_;
  std::string session_id_;
  std::chrono::time_point<std::chrono::steady_clock> last_activity_;
  std::mutex session_mutex_;

public:
  ClientSession(int fd, DatabaseEngine* engine, const std::string& id);
  ~ClientSession();

  void handle_request();
  void send_response(const ServerMessage& msg);
  ClientMessage receive_message();

  bool is_active() const;
  void close();

private:
  void handle_query(const std::string& sql);
  void handle_begin_transaction();
  void handle_commit();
  void handle_rollback();
  void handle_ping();

  void send_error(const std::string& error_msg);
  void send_result(const QueryResult& result);
};

class NetworkServer {
private:
  DatabaseEngine* db_engine_;
  int server_socket_;
  uint16_t port_;
  std::atomic<bool> running_;
  std::thread accept_thread_;
  std::vector<std::thread> worker_threads_;
  std::queue<int> client_queue_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::unordered_map<std::string, std::unique_ptr<ClientSession>> sessions_;
  std::mutex sessions_mutex_;

  // Configuration
  size_t max_connections_;
  size_t worker_pool_size_;
  std::chrono::seconds session_timeout_;

public:
  NetworkServer(DatabaseEngine* engine, uint16_t port = 5432);
  ~NetworkServer();

  bool start();
  void stop();
  bool is_running() const {
    return running_;
  }

  void set_max_connections(size_t max) {
    max_connections_ = max;
  }
  void set_worker_pool_size(size_t size) {
    worker_pool_size_ = size;
  }
  void set_session_timeout(std::chrono::seconds timeout) {
    session_timeout_ = timeout;
  }

private:
  void accept_connections();
  void worker_loop();
  void handle_client(int client_fd);
  void cleanup_sessions();

  bool initialize_socket();
  void close_socket();
  std::string generate_session_id();
};

// Wire protocol helpers
class WireProtocol {
public:
  static std::vector<uint8_t> serialize_message(const ServerMessage& msg);
  static ClientMessage deserialize_message(const std::vector<uint8_t>& data);

  static std::vector<uint8_t> serialize_result(const QueryResult& result);
  static std::string deserialize_query(const std::vector<uint8_t>& data);

private:
  static void write_uint32(std::vector<uint8_t>& buffer, uint32_t value);
  static uint32_t read_uint32(const uint8_t* data);

  static void write_string(std::vector<uint8_t>& buffer, const std::string& str);
  static std::string read_string(const uint8_t* data, size_t& offset, size_t length);
};

} // namespace latticedb