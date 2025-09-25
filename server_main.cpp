#include "src/common/logger.h"
#include "src/engine/database_engine.h"
#include "src/network/server.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

using namespace latticedb;

std::atomic<bool> server_running(true);
NetworkServer* global_server = nullptr;

void signal_handler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    std::cout << "\n[INFO] Shutting down server..." << std::endl;
    server_running = false;
    if (global_server) {
      global_server->stop();
    }
  }
}

static std::string json_escape(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
    case '\"':
      o += "\\\"";
      break;
    case '\\':
      o += "\\\\";
      break;
    case '\b':
      o += "\\b";
      break;
    case '\f':
      o += "\\f";
      break;
    case '\n':
      o += "\\n";
      break;
    case '\r':
      o += "\\r";
      break;
    case '\t':
      o += "\\t";
      break;
    default:
      if ((unsigned char)c < 0x20) {
        std::ostringstream os;
        os << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)(unsigned char)c;
        o += os.str();
      } else {
        o += c;
      }
    }
  }
  return o;
}

static std::string value_to_json(const Value& v) {
  if (v.is_null()) {
    return "null";
  }

  switch (v.type()) {
  case ValueType::BOOLEAN:
    return v.get<bool>() ? "true" : "false";

  case ValueType::TINYINT:
    return std::to_string(v.get<int8_t>());

  case ValueType::SMALLINT:
    return std::to_string(v.get<int16_t>());

  case ValueType::INTEGER:
    return std::to_string(v.get<int32_t>());

  case ValueType::BIGINT:
    return std::to_string(v.get<int64_t>());

  case ValueType::DOUBLE:
  case ValueType::REAL:
  case ValueType::DECIMAL: {
    std::ostringstream os;
    os << std::setprecision(15) << v.get<double>();
    return os.str();
  }

  case ValueType::VARCHAR:
  case ValueType::TEXT:
  case ValueType::TIMESTAMP:
  case ValueType::DATE:
  case ValueType::TIME:
    return "\"" + json_escape(v.get<std::string>()) + "\"";

  case ValueType::VECTOR: {
    std::ostringstream os;
    os << "[";
    const auto& vec = v.get<std::vector<double>>();
    for (size_t i = 0; i < vec.size(); ++i) {
      if (i > 0)
        os << ",";
      os << std::setprecision(15) << vec[i];
    }
    os << "]";
    return os.str();
  }

  case ValueType::BLOB: {
    // Convert blob to base64 or hex string
    return "\"<blob>\"";
  }

  default:
    return "null";
  }
}

int main(int argc, char* argv[]) {
  // Parse command line arguments
  uint16_t port = 5432;
  std::string db_file = "latticedb.db";
  bool enable_logging = true;
  size_t max_connections = 100;
  size_t worker_pool_size = 4;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--port" && i + 1 < argc) {
      port = static_cast<uint16_t>(std::stoi(argv[++i]));
    } else if (arg == "--db" && i + 1 < argc) {
      db_file = argv[++i];
    } else if (arg == "--no-log") {
      enable_logging = false;
    } else if (arg == "--max-connections" && i + 1 < argc) {
      max_connections = std::stoul(argv[++i]);
    } else if (arg == "--workers" && i + 1 < argc) {
      worker_pool_size = std::stoul(argv[++i]);
    } else if (arg == "--help") {
      std::cout << "Usage: " << argv[0] << " [options]\n"
                << "Options:\n"
                << "  --port <port>           Server port (default: 5432)\n"
                << "  --db <file>            Database file (default: latticedb.db)\n"
                << "  --no-log               Disable logging\n"
                << "  --max-connections <n>  Maximum concurrent connections (default: 100)\n"
                << "  --workers <n>          Worker thread pool size (default: 4)\n"
                << "  --help                 Show this help message\n";
      return 0;
    }
  }

  // Set up signal handlers
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  try {
    // Initialize logger
    if (enable_logging) {
      Logger::get_instance()->set_level(LogLevel::INFO);
      Logger::get_instance()->set_file_output(true);
      Logger::get_instance()->set_log_file("latticedb_server.log");
    }

    LOG_INFO("Starting LatticeDB Server...");
    LOG_INFO("Database file: {}", db_file);
    LOG_INFO("Port: {}", port);

    // Create and initialize database engine
    auto db_engine = std::make_unique<DatabaseEngine>(db_file, enable_logging);
    if (!db_engine->initialize()) {
      LOG_ERROR("Failed to initialize database engine");
      return 1;
    }

    LOG_INFO("Database engine initialized successfully");

    // Create and configure network server
    auto server = std::make_unique<NetworkServer>(db_engine.get(), port);
    server->set_max_connections(max_connections);
    server->set_worker_pool_size(worker_pool_size);
    server->set_session_timeout(std::chrono::seconds(300)); // 5 minute timeout

    global_server = server.get();

    // Start the server
    if (!server->start()) {
      LOG_ERROR("Failed to start network server");
      return 1;
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "LatticeDB Server v1.0.0" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Server listening on port " << port << std::endl;
    std::cout << "Database: " << db_file << std::endl;
    std::cout << "Max connections: " << max_connections << std::endl;
    std::cout << "Worker threads: " << worker_pool_size << std::endl;
    std::cout << "Press Ctrl+C to shutdown" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // Main loop - wait for shutdown signal
    while (server_running && server->is_running()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Graceful shutdown
    LOG_INFO("Shutting down server...");
    server->stop();
    db_engine->shutdown();

    LOG_INFO("Server shutdown complete");
    std::cout << "Server shutdown complete." << std::endl;

  } catch (const std::exception& e) {
    LOG_ERROR("Server error: {}", e.what());
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}