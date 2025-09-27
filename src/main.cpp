#include "common/logger.h"
#include "engine/database_engine.h"
#ifdef HAVE_TELEMETRY
#include "telemetry/metrics_collector.h"
#include "telemetry/telemetry_manager.h"
#endif
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>
#if defined(_WIN32)
#include <io.h>
#endif

using namespace latticedb;

namespace {

constexpr const char* COLOR_RESET = "\033[0m";
constexpr const char* COLOR_CYAN = "\033[36m";
constexpr const char* COLOR_YELLOW = "\033[33m";
constexpr const char* COLOR_MAGENTA = "\033[35m";
constexpr const char* COLOR_GREEN = "\033[32m";

// ---- Version ----
constexpr const char* LATTICEDB_VERSION = "1.1.0";

bool supports_color() {
#if defined(_WIN32)
  return _isatty(_fileno(stdout)) != 0;
#else
  return isatty(fileno(stdout)) != 0;
#endif
}

std::string colorize(const char* color, const std::string& text) {
  if (!supports_color())
    return text;
  return std::string(color) + text + COLOR_RESET;
}

void print_command_line(const std::string& command, const std::string& description) {
  constexpr int width = 28;
  std::cout << "  " << std::left << std::setw(width) << command << description << "\n";
}

} // namespace

static std::string trim_copy(const std::string& text) {
  const auto first = text.find_first_not_of(" \t\n\r");
  if (first == std::string::npos)
    return "";
  const auto last = text.find_last_not_of(" \t\n\r");
  return text.substr(first, last - first + 1);
}

static void print_version() {
  std::cout << "LatticeDB version " << LATTICEDB_VERSION << "\n";
}

static void print_welcome() {
  static const std::string banner = R"(
  _           _   _   _          _____  ____  
 | |         | | | | (_)        |  __ \|  _ \ 
 | |     __ _| |_| |_ _  ___ ___| |  | | |_) |
 | |    / _` | __| __| |/ __/ _ \ |  | |  _ < 
 | |___| (_| | |_| |_| | (_|  __/ |__| | |_) |
 |______\__,_|\__|\__|_|\___\___|_____/|____/ 
  )";

  std::cout << colorize(COLOR_CYAN, banner) << "\n";
  std::cout << colorize(COLOR_YELLOW, std::string("LatticeDB v") + LATTICEDB_VERSION +
                                          " — Modern Relational Database Engine")
            << "\n";
  std::cout << colorize(COLOR_MAGENTA,
                        "ACID Transactions · MVCC · Write-Ahead Logging · Advanced Security")
            << "\n";
  std::cout << colorize(COLOR_GREEN,
                        "Vector Search · Time Travel · Adaptive Compression · Streaming Analytics")
            << "\n\n";
  std::cout << "Type 'help' for commands, 'exit' to quit.\n\n";
}

static void print_help() {
  // Show version at the top of help
  std::cout << colorize(COLOR_YELLOW, std::string("LatticeDB v") + LATTICEDB_VERSION) << "\n\n";
  std::cout << colorize(COLOR_CYAN, "Available Commands:") << "\n\n";
  print_command_line("help", "Show this help message");
  print_command_line("exit, quit", "Exit the database");
  print_command_line("\\d", "List all tables");
  print_command_line("\\d <table>", "Describe table schema");
  print_command_line("\\stats", "Show database statistics");
  print_command_line("BEGIN", "Start a new transaction");
  print_command_line("COMMIT", "Commit current transaction");
  print_command_line("ROLLBACK", "Rollback current transaction");

  std::cout << "\n" << colorize(COLOR_CYAN, "SQL Commands:") << "\n\n";
  print_command_line("CREATE TABLE <name> (...)", "Create a new table");
  print_command_line("DROP TABLE <name>", "Drop a table");
  print_command_line("INSERT INTO <table> VALUES (...)", "Insert data");
  print_command_line("SELECT ... FROM <table>", "Query data");
  print_command_line("UPDATE <table> SET ...", "Update data");
  print_command_line("DELETE FROM <table> ...", "Delete data");
  print_command_line("CREATE INDEX <name> ON <table>(...)", "Create index");
  print_command_line("DROP INDEX <name>", "Drop index");
  std::cout << "\n";
}

static void print_table_list(DatabaseEngine* engine) {
  auto table_names = engine->get_table_names();
  if (table_names.empty()) {
    std::cout << "No tables found.\n";
    return;
  }

  std::cout << "Tables:\n";
  for (const auto& name : table_names) {
    auto* table_info = engine->get_table_info(name);
    if (table_info) {
      std::cout << "  " << name << " (" << table_info->get_schema().column_count() << " columns)\n";
    } else {
      std::cout << "  " << name << "\n";
    }
  }
}

static void print_table_schema(DatabaseEngine* engine, const std::string& table_name) {
  auto* table_info = engine->get_table_info(table_name);
  if (!table_info) {
    std::cout << "Table '" << table_name << "' not found.\n";
    return;
  }

  const auto& schema = table_info->get_schema();
  std::cout << "Table: " << table_name << "\n";
  std::cout << std::left << std::setw(20) << "Column" << std::setw(15) << "Type" << std::setw(10)
            << "Nullable" << "Default\n";
  std::cout << std::string(55, '-') << "\n";

  for (uint32_t i = 0; i < schema.column_count(); ++i) {
    const auto& col = schema.get_column(i);
    std::string type_str;
    switch (col.type()) {
    case ValueType::INTEGER:
      type_str = "INTEGER";
      break;
    case ValueType::BIGINT:
      type_str = "BIGINT";
      break;
    case ValueType::DOUBLE:
      type_str = "DOUBLE";
      break;
    case ValueType::VARCHAR:
      type_str = "VARCHAR(" + std::to_string(col.length()) + ")";
      break;
    case ValueType::TEXT:
      type_str = "TEXT";
      break;
    case ValueType::BOOLEAN:
      type_str = "BOOLEAN";
      break;
    case ValueType::TIMESTAMP:
      type_str = "TIMESTAMP";
      break;
    case ValueType::DATE:
      type_str = "DATE";
      break;
    case ValueType::TIME:
      type_str = "TIME";
      break;
    case ValueType::BLOB:
      type_str = "BLOB";
      break;
    case ValueType::VECTOR:
      type_str = "VECTOR";
      break;
    default:
      type_str = "UNKNOWN";
      break;
    }

    std::cout << std::left << std::setw(20) << col.name() << std::setw(15) << type_str
              << std::setw(10) << (col.is_nullable() ? "YES" : "NO");

    if (col.has_default()) {
      std::cout << col.default_value().to_string();
    } else {
      std::cout << "NULL";
    }
    std::cout << "\n";
  }
}

static void print_stats(DatabaseEngine* engine) {
  auto* bpm = engine->get_buffer_pool_manager();
  std::cout << "Database Statistics:\n";
  std::cout << "  Buffer Pool Size: " << bpm->get_pool_size() << " pages\n";
  std::cout << "  Free Pages: " << bpm->get_free_list_size() << "\n";
  std::cout << "  Tables: " << engine->get_table_names().size() << "\n";
}

static void print_result_interactive(const QueryResult& result) {
  if (!result.success) {
    std::cout << "ERROR: " << result.message << "\n";
    return;
  }

  if (result.rows.empty() && result.rows_affected == 0) {
    std::cout << result.message << "\n";
    return;
  }

  if (result.rows_affected > 0) {
    std::cout << "Query OK, " << result.rows_affected << " rows affected.\n";
    return;
  }

  if (!result.column_names.empty()) {
    // Print headers
    for (size_t i = 0; i < result.column_names.size(); ++i) {
      if (i > 0)
        std::cout << " | ";
      std::cout << std::left << std::setw(15) << result.column_names[i];
    }
    std::cout << "\n" << std::string(result.column_names.size() * 18 - 3, '-') << "\n";

    // Print rows
    for (const auto& row : result.rows) {
      for (size_t i = 0; i < row.size(); ++i) {
        if (i > 0)
          std::cout << " | ";
        std::cout << std::left << std::setw(15) << row[i].to_string();
      }
      std::cout << "\n";
    }

    std::cout << "\n(" << result.rows.size() << " rows)\n";
  }
}

static void print_result_test(const std::string& stmt_upper, const QueryResult& result) {
  if (stmt_upper.rfind("CREATE TABLE", 0) == 0) {
    std::cout << "CREATE TABLE\n";
    return;
  }
  if (stmt_upper.rfind("INSERT", 0) == 0) {
    std::cout << "INSERT " << result.rows_affected << " row(s)\n";
    return;
  }
  if (stmt_upper.rfind("UPDATE", 0) == 0) {
    std::cout << "UPDATE " << result.rows_affected << " row(s)\n";
    return;
  }
  if (stmt_upper.rfind("SELECT", 0) == 0) {
    for (size_t i = 0; i < result.column_names.size(); ++i) {
      if (i)
        std::cout << " | ";
      std::cout << result.column_names[i];
    }
    std::cout << "\n";
    for (const auto& row : result.rows) {
      for (size_t i = 0; i < row.size(); ++i) {
        if (i)
          std::cout << " | ";
        const auto& v = row[i];
        if (v.type() == ValueType::VARCHAR || v.type() == ValueType::TEXT)
          std::cout << "'" << v.to_string() << "'";
        else
          std::cout << v.to_string();
      }
      std::cout << "\n";
    }
    return;
  }
  if (!result.success && !result.message.empty())
    std::cout << "ERROR: " << result.message << "\n";
}

int main(int argc, char* argv[]) {
  std::string db_file = "latticedb.db";
  bool enable_logging = true;

  // Parse command line arguments
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--file" && i + 1 < argc) {
      db_file = argv[++i];
    } else if (arg == "--no-logging") {
      enable_logging = false;
    } else if (arg == "--version") {
      print_version();
      return 0;
    } else if (arg == "--help") {
      std::cout << "Usage: " << argv[0] << " [database_file] [options]\n";
      std::cout << "LatticeDB version " << LATTICEDB_VERSION << "\n";
      std::cout << "Arguments:\n";
      std::cout << "  database_file    Database file path (default: latticedb.db)\n";
      std::cout << "Options:\n";
      std::cout << "  --file <path>    Database file path (overrides positional argument)\n";
      std::cout << "  --no-logging     Disable write-ahead logging\n";
      std::cout << "  --version        Show version and exit\n";
      std::cout << "  --help           Show this help\n";
      return 0;
    } else if (arg[0] != '-') {
      // First non-option argument is the database file
      db_file = arg;
    }
  }

  // Logger disabled in minimal build to avoid exit-time issues

  try {
#ifdef HAVE_TELEMETRY
    // Initialize telemetry if enabled
    auto& telemetry_manager = latticedb::telemetry::TelemetryManager::getInstance();
    auto& metrics_collector = latticedb::telemetry::MetricsCollector::getInstance();

    latticedb::telemetry::TelemetryConfig telemetry_config;

    // Read telemetry configuration from environment variables
    const char* telemetry_enabled = std::getenv("TELEMETRY_ENABLED");
    const char* jaeger_endpoint = std::getenv("JAEGER_ENDPOINT");
    const char* prometheus_endpoint = std::getenv("PROMETHEUS_ENDPOINT");
    const char* otlp_endpoint = std::getenv("OTLP_ENDPOINT");

    if (telemetry_enabled && std::string(telemetry_enabled) == "true") {
      telemetry_config.enabled = true;
      if (jaeger_endpoint)
        telemetry_config.jaeger_endpoint = jaeger_endpoint;
      if (prometheus_endpoint)
        telemetry_config.prometheus_endpoint = prometheus_endpoint;
      if (otlp_endpoint)
        telemetry_config.otlp_endpoint = otlp_endpoint;

      telemetry_manager.initialize(telemetry_config);
      metrics_collector.initialize();
    }
#endif

    bool test_mode = !isatty(fileno(stdin));
    // Initialize database engine
    auto engine = std::make_unique<DatabaseEngine>(db_file, enable_logging);

    if (!engine->initialize()) {
      std::cerr << "Failed to initialize database engine.\n";
      return 1;
    }

    if (!test_mode)
      print_welcome();

    std::string input;
    std::string stmt_accum;
    Transaction* current_txn = nullptr;

    auto handle_transaction_keyword = [&](const std::string& keyword_upper) -> bool {
      if (test_mode)
        return false;

      if (keyword_upper == "BEGIN") {
        if (current_txn)
          std::cout << "Transaction already in progress.\n";
        else {
          current_txn = engine->begin_transaction();
          std::cout << "BEGIN\n";
        }
        return true;
      }

      if (keyword_upper == "COMMIT") {
        if (!current_txn)
          std::cout << "No transaction in progress.\n";
        else {
          if (engine->commit_transaction(current_txn))
            std::cout << "COMMIT\n";
          else
            std::cout << "COMMIT failed.\n";
          current_txn = nullptr;
        }
        return true;
      }

      if (keyword_upper == "ROLLBACK") {
        if (!current_txn)
          std::cout << "No transaction in progress.\n";
        else {
          if (engine->abort_transaction(current_txn))
            std::cout << "ROLLBACK\n";
          else
            std::cout << "ROLLBACK failed.\n";
          current_txn = nullptr;
        }
        return true;
      }

      return false;
    };

    while (true) {
      if (!test_mode) {
        if (current_txn)
          std::cout << "latticedb*> ";
        else
          std::cout << "latticedb> ";
      }
      if (!std::getline(std::cin, input))
        break;
      std::string trimmed_line = trim_copy(input);
      if (trimmed_line.empty())
        continue;

      std::string meta_candidate = trimmed_line;
      if (!meta_candidate.empty() && meta_candidate.back() == ';') {
        meta_candidate.pop_back();
        meta_candidate = trim_copy(meta_candidate);
      }

      if (stmt_accum.empty()) {
        auto upper_meta = meta_candidate;
        for (auto& c : upper_meta)
          c = std::toupper(static_cast<unsigned char>(c));

        if (upper_meta == "EXIT" || upper_meta == "QUIT") {
          break;
        }

        if (!test_mode) {
          if (upper_meta == "HELP") {
            print_help();
            continue;
          }

          if (meta_candidate == "\\d") {
            print_table_list(engine.get());
            continue;
          }

          if (meta_candidate.size() > 3 && meta_candidate.substr(0, 3) == "\\d ") {
            auto table_name = trim_copy(meta_candidate.substr(3));
            if (!table_name.empty())
              print_table_schema(engine.get(), table_name);
            else
              print_table_list(engine.get());
            continue;
          }

          if (meta_candidate == "\\stats") {
            print_stats(engine.get());
            continue;
          }

          if (handle_transaction_keyword(upper_meta))
            continue;
        }
      }

      stmt_accum += input;
      if (stmt_accum.find(';') == std::string::npos)
        continue;
      auto pos = stmt_accum.find(';');
      std::string statement = stmt_accum.substr(0, pos);
      stmt_accum.erase(0, pos + 1);
      statement = trim_copy(statement);
      if (statement.empty())
        continue;

      auto up = statement;
      for (auto& c : up)
        c = std::toupper(static_cast<unsigned char>(c));
      if (up == "EXIT" || up == "QUIT") {
        break;
      }
      if (!test_mode && up == "HELP") {
        print_help();
        continue;
      }
      if (!test_mode && statement == "\\d") {
        print_table_list(engine.get());
        continue;
      }
      if (!test_mode && statement.substr(0, 2) == "\\d" && statement.length() > 3) {
        print_table_schema(engine.get(), trim_copy(statement.substr(3)));
        continue;
      }
      if (!test_mode && statement == "\\stats") {
        print_stats(engine.get());
        continue;
      }

      if (handle_transaction_keyword(up))
        continue;

      try {
        auto result = engine->execute_sql(statement, current_txn);
        if (test_mode)
          print_result_test(up, result);
        else
          print_result_interactive(result);
      } catch (const Exception& e) {
        std::cout << "ERROR: " << e.what() << "\n";
        if (current_txn && current_txn->get_state() == TransactionState::ABORTED) {
          std::cout << "Transaction aborted.\n";
          current_txn = nullptr;
        }
      } catch (const std::exception& e) {
        std::cout << "ERROR: " << e.what() << "\n";
      }
    }

    if (current_txn) {
      try {
        engine->abort_transaction(current_txn);
      } catch (...) {
        // Ignore errors during shutdown
      }
    }

    try {
      engine->shutdown();
    } catch (...) {
      // Ignore errors during shutdown
    }

#ifdef HAVE_TELEMETRY
    // Shutdown telemetry
    telemetry_manager.shutdown();
#endif

    if (!test_mode) {
      std::cout << "\nGoodbye!\n";
    }

  } catch (const std::exception& e) {
    std::cerr << "Fatal error: " << e.what() << "\n";
    _exit(1);
  }

  // Use _exit to bypass destructors and avoid mutex issues
  _exit(0);
}
