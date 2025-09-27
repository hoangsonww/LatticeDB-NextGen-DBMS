#include <thread>

#include <mutex>

#include "server.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <random>
#include <sys/socket.h>
#include <unistd.h>

namespace latticedb {

// ClientSession implementation
ClientSession::ClientSession(int fd, DatabaseEngine* engine, const std::string& id)
    : client_fd_(fd), db_engine_(engine), current_txn_(nullptr), session_id_(id),
      last_activity_(std::chrono::steady_clock::now()) {}

ClientSession::~ClientSession() {
  if (current_txn_) {
    db_engine_->abort_transaction(current_txn_);
  }
  close();
}

void ClientSession::handle_request() {
  try {
    auto msg = receive_message();
    last_activity_ = std::chrono::steady_clock::now();

    switch (msg.type) {
    case MessageType::QUERY:
      handle_query(msg.payload);
      break;
    case MessageType::BEGIN_TRANSACTION:
      handle_begin_transaction();
      break;
    case MessageType::COMMIT:
      handle_commit();
      break;
    case MessageType::ROLLBACK:
      handle_rollback();
      break;
    case MessageType::PING:
      handle_ping();
      break;
    case MessageType::DISCONNECT:
      close();
      break;
    default:
      send_error("Unknown message type");
    }
  } catch (const std::exception& e) {
    send_error(e.what());
  }
}

void ClientSession::handle_query(const std::string& sql) {
  std::lock_guard<std::mutex> lock(session_mutex_);

  auto result = db_engine_->execute_sql(sql, current_txn_);
  send_result(result);
}

void ClientSession::handle_begin_transaction() {
  std::lock_guard<std::mutex> lock(session_mutex_);

  if (current_txn_) {
    send_error("Transaction already in progress");
    return;
  }

  current_txn_ = db_engine_->begin_transaction(IsolationLevel::READ_COMMITTED);
  ServerMessage response;
  response.type = MessageType::RESULT;
  response.payload = "BEGIN";
  send_response(response);
}

void ClientSession::handle_commit() {
  std::lock_guard<std::mutex> lock(session_mutex_);

  if (!current_txn_) {
    send_error("No transaction in progress");
    return;
  }

  bool success = db_engine_->commit_transaction(current_txn_);
  current_txn_ = nullptr;

  ServerMessage response;
  response.type = MessageType::RESULT;
  response.payload = success ? "COMMIT" : "COMMIT FAILED";
  send_response(response);
}

void ClientSession::handle_rollback() {
  std::lock_guard<std::mutex> lock(session_mutex_);

  if (!current_txn_) {
    send_error("No transaction in progress");
    return;
  }

  db_engine_->abort_transaction(current_txn_);
  current_txn_ = nullptr;

  ServerMessage response;
  response.type = MessageType::RESULT;
  response.payload = "ROLLBACK";
  send_response(response);
}

void ClientSession::handle_ping() {
  ServerMessage response;
  response.type = MessageType::PONG;
  send_response(response);
}

void ClientSession::send_response(const ServerMessage& msg) {
  auto data = WireProtocol::serialize_message(msg);
  send(client_fd_, data.data(), data.size(), 0);
}

void ClientSession::send_error(const std::string& error_msg) {
  ServerMessage response;
  response.type = MessageType::ERROR;
  response.payload = error_msg;
  send_response(response);
}

void ClientSession::send_result(const QueryResult& result) {
  ServerMessage response;
  response.type = MessageType::RESULT;
  response.result = result;
  send_response(response);
}

ClientMessage ClientSession::receive_message() {
  uint8_t header[5];
  if (recv(client_fd_, header, 5, MSG_WAITALL) != 5) {
    throw std::runtime_error("Failed to receive message header");
  }

  ClientMessage msg;
  msg.type = static_cast<MessageType>(header[0]);
  msg.length = (header[1] << 24) | (header[2] << 16) | (header[3] << 8) | header[4];

  if (msg.length > 0) {
    std::vector<uint8_t> payload(msg.length);
    if (recv(client_fd_, payload.data(), msg.length, MSG_WAITALL) !=
        static_cast<ssize_t>(msg.length)) {
      throw std::runtime_error("Failed to receive message payload");
    }
    msg.payload = std::string(payload.begin(), payload.end());
  }

  return msg;
}

bool ClientSession::is_active() const {
  auto now = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_activity_);
  return duration.count() < 300; // 5 minute timeout
}

void ClientSession::close() {
  if (client_fd_ >= 0) {
    ::close(client_fd_);
    client_fd_ = -1;
  }
}

// NetworkServer implementation
NetworkServer::NetworkServer(DatabaseEngine* engine, uint16_t port)
    : db_engine_(engine), server_socket_(-1), port_(port), running_(false), max_connections_(100),
      worker_pool_size_(10), session_timeout_(std::chrono::seconds(300)) {}

NetworkServer::~NetworkServer() {
  stop();
}

bool NetworkServer::start() {
  if (running_)
    return false;

  if (!initialize_socket()) {
    return false;
  }

  running_ = true;

  // Start worker threads
  for (size_t i = 0; i < worker_pool_size_; ++i) {
    worker_threads_.emplace_back(&NetworkServer::worker_loop, this);
  }

  // Start accept thread
  accept_thread_ = std::thread(&NetworkServer::accept_connections, this);

  std::cout << "LatticeDB server listening on port " << port_ << std::endl;
  return true;
}

void NetworkServer::stop() {
  if (!running_)
    return;

  running_ = false;

  // Close server socket to interrupt accept
  close_socket();

  // Wake up workers
  queue_cv_.notify_all();

  // Join threads
  if (accept_thread_.joinable()) {
    accept_thread_.join();
  }

  for (auto& thread : worker_threads_) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  // Close all sessions
  {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.clear();
  }

  std::cout << "LatticeDB server stopped" << std::endl;
}

void NetworkServer::accept_connections() {
  while (running_) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = accept(server_socket_, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) {
      if (running_) {
        std::cerr << "Accept failed: " << strerror(errno) << std::endl;
      }
      continue;
    }

    // Check connection limit
    {
      std::lock_guard<std::mutex> lock(sessions_mutex_);
      if (sessions_.size() >= max_connections_) {
        ::close(client_fd);
        continue;
      }
    }

    // Add to queue for worker to handle
    {
      std::lock_guard<std::mutex> lock(queue_mutex_);
      client_queue_.push(client_fd);
    }
    queue_cv_.notify_one();
  }
}

void NetworkServer::worker_loop() {
  while (running_) {
    int client_fd = -1;

    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      queue_cv_.wait(lock, [this] { return !client_queue_.empty() || !running_; });

      if (!running_)
        break;

      if (!client_queue_.empty()) {
        client_fd = client_queue_.front();
        client_queue_.pop();
      }
    }

    if (client_fd >= 0) {
      handle_client(client_fd);
    }
  }
}

void NetworkServer::handle_client(int client_fd) {
  std::string session_id = generate_session_id();

  auto session = std::make_unique<ClientSession>(client_fd, db_engine_, session_id);

  {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_[session_id] = std::move(session);
  }

  // Send initial connect response
  ServerMessage welcome;
  welcome.type = MessageType::CONNECT;
  welcome.payload = "LatticeDB " + session_id;
  sessions_[session_id]->send_response(welcome);

  // Handle client requests
  while (sessions_[session_id]->is_active() && running_) {
    sessions_[session_id]->handle_request();
  }

  // Remove session
  {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(session_id);
  }
}

void NetworkServer::cleanup_sessions() {
  std::lock_guard<std::mutex> lock(sessions_mutex_);

  auto it = sessions_.begin();
  while (it != sessions_.end()) {
    if (!it->second->is_active()) {
      it = sessions_.erase(it);
    } else {
      ++it;
    }
  }
}

bool NetworkServer::initialize_socket() {
  server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket_ < 0) {
    std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
    return false;
  }

  // Allow socket reuse
  int opt = 1;
  if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    std::cerr << "Failed to set socket options: " << strerror(errno) << std::endl;
    close_socket();
    return false;
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port_);

  if (bind(server_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    std::cerr << "Failed to bind socket: " << strerror(errno) << std::endl;
    close_socket();
    return false;
  }

  if (listen(server_socket_, 128) < 0) {
    std::cerr << "Failed to listen on socket: " << strerror(errno) << std::endl;
    close_socket();
    return false;
  }

  return true;
}

void NetworkServer::close_socket() {
  if (server_socket_ >= 0) {
    ::close(server_socket_);
    server_socket_ = -1;
  }
}

std::string NetworkServer::generate_session_id() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> dis(0, 15);

  std::string id;
  for (int i = 0; i < 32; ++i) {
    int val = dis(gen);
    id += (val < 10) ? ('0' + val) : ('a' + val - 10);
  }
  return id;
}

// WireProtocol implementation
std::vector<uint8_t> WireProtocol::serialize_message(const ServerMessage& msg) {
  std::vector<uint8_t> buffer;

  // Type (1 byte)
  buffer.push_back(static_cast<uint8_t>(msg.type));

  // Serialize based on type
  if (msg.type == MessageType::RESULT && !msg.result.rows.empty()) {
    auto result_data = serialize_result(msg.result);
    write_uint32(buffer, result_data.size());
    buffer.insert(buffer.end(), result_data.begin(), result_data.end());
  } else {
    // Length (4 bytes) + Payload
    write_uint32(buffer, msg.payload.size());
    buffer.insert(buffer.end(), msg.payload.begin(), msg.payload.end());
  }

  return buffer;
}

ClientMessage WireProtocol::deserialize_message(const std::vector<uint8_t>& data) {
  ClientMessage msg;
  if (data.size() < 5) {
    throw std::runtime_error("Invalid message size");
  }

  msg.type = static_cast<MessageType>(data[0]);
  msg.length = read_uint32(&data[1]);

  if (data.size() >= 5 + msg.length) {
    msg.payload = std::string(data.begin() + 5, data.begin() + 5 + msg.length);
  }

  return msg;
}

std::vector<uint8_t> WireProtocol::serialize_result(const QueryResult& result) {
  std::vector<uint8_t> buffer;

  // Success flag
  buffer.push_back(result.success ? 1 : 0);

  // Rows affected
  write_uint32(buffer, result.rows_affected);

  // Column count
  write_uint32(buffer, result.column_names.size());

  // Column names
  for (const auto& name : result.column_names) {
    write_string(buffer, name);
  }

  // Row count
  write_uint32(buffer, result.rows.size());

  // Row data
  for (const auto& row : result.rows) {
    write_uint32(buffer, row.size());
    for (const auto& value : row) {
      write_string(buffer, value.to_string());
    }
  }

  // Message
  write_string(buffer, result.message);

  return buffer;
}

void WireProtocol::write_uint32(std::vector<uint8_t>& buffer, uint32_t value) {
  buffer.push_back((value >> 24) & 0xFF);
  buffer.push_back((value >> 16) & 0xFF);
  buffer.push_back((value >> 8) & 0xFF);
  buffer.push_back(value & 0xFF);
}

uint32_t WireProtocol::read_uint32(const uint8_t* data) {
  return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

void WireProtocol::write_string(std::vector<uint8_t>& buffer, const std::string& str) {
  write_uint32(buffer, str.size());
  buffer.insert(buffer.end(), str.begin(), str.end());
}

std::string WireProtocol::read_string(const uint8_t* data, size_t& offset, size_t length) {
  if (offset + 4 > length) {
    throw std::runtime_error("Invalid string length");
  }

  uint32_t str_len = read_uint32(data + offset);
  offset += 4;

  if (offset + str_len > length) {
    throw std::runtime_error("Invalid string data");
  }

  std::string str(reinterpret_cast<const char*>(data + offset), str_len);
  offset += str_len;

  return str;
}

} // namespace latticedb