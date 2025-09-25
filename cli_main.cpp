#include "src/common/logger.h"
#include "src/engine/database_engine.h"
#include <algorithm>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

using namespace latticedb;

// ANSI color codes for terminal output
namespace Color {
const std::string RESET = "\033[0m";
const std::string BOLD = "\033[1m";
const std::string RED = "\033[31m";
const std::string GREEN = "\033[32m";
const std::string YELLOW = "\033[33m";
const std::string BLUE = "\033[34m";
const std::string MAGENTA = "\033[35m";
const std::string CYAN = "\033[36m";
const std::string WHITE = "\033[37m";
} // namespace Color

class LatticeDBCLI {
private:
  std::unique_ptr<DatabaseEngine> db_engine_;
  std::vector<std::string> command_history_;
  size_t history_index_;
  std::string current_database_;
  bool running_;
  bool color_output_;
  bool timing_enabled_;
  Transaction* current_transaction_;
  std::string prompt_;

public:
  LatticeDBCLI(const std::string& db_file = "latticedb.db")
      : history_index_(0), current_database_(db_file), running_(true), color_output_(true),
        timing_enabled_(false), current_transaction_(nullptr) {
    prompt_ = "latticedb> ";
  }

  ~LatticeDBCLI() {
    if (current_transaction_) {
      db_engine_->abort_transaction(current_transaction_);
    }
  }

  bool initialize() {
    // Initialize the database engine
    db_engine_ = std::make_unique<DatabaseEngine>(current_database_, false);
    if (!db_engine_->initialize()) {
      std::cerr << color(Color::RED) << "Error: Failed to initialize database engine"
                << Color::RESET << std::endl;
      return false;
    }

    // Disable file logging for CLI
    Logger::get_instance()->set_file_output(false);
    Logger::get_instance()->set_console_output(false);

    return true;
  }

  void run() {
    print_welcome();

    while (running_) {
      std::string input = get_input();
      if (input.empty())
        continue;

      // Add to history
      if (!input.empty() && (command_history_.empty() || command_history_.back() != input)) {
        command_history_.push_back(input);
        history_index_ = command_history_.size();
      }

      process_command(input);
    }

    print_goodbye();
  }

private:
  void print_welcome() {
    std::cout << color(Color::CYAN) << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                                               ║\n";
    std::cout << "║              " << color(Color::BOLD) << "LatticeDB Interactive Shell v1.0.0"
              << color(Color::CYAN) << "              ║\n";
    std::cout << "║                                                               ║\n";
    std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Type 'help' for available commands                          ║\n";
    std::cout << "║  Type 'quit' or 'exit' to leave                              ║\n";
    std::cout << "║  Use UP/DOWN arrows to navigate command history              ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝" << Color::RESET
              << "\n\n";
    std::cout << "Connected to database: " << color(Color::GREEN) << current_database_
              << Color::RESET << "\n\n";
  }

  void print_goodbye() {
    std::cout << "\n"
              << color(Color::CYAN) << "Thank you for using LatticeDB!" << Color::RESET << "\n";
  }

  void print_help() {
    std::cout << color(Color::YELLOW) << "\nAvailable Commands:\n" << Color::RESET;
    std::cout << "═══════════════════════════════════════════════════════════════\n\n";

    // Meta commands
    std::cout << color(Color::CYAN) << "Meta Commands:\n" << Color::RESET;
    std::cout << "  .help, .h                Show this help message\n";
    std::cout << "  .quit, .exit, .q         Exit the CLI\n";
    std::cout << "  .tables                  List all tables\n";
    std::cout << "  .schema [table]          Show table schema\n";
    std::cout << "  .indexes                 List all indexes\n";
    std::cout << "  .import <file>           Execute SQL from file\n";
    std::cout << "  .export <file>           Export database to SQL file\n";
    std::cout << "  .timing on|off           Enable/disable query timing\n";
    std::cout << "  .color on|off            Enable/disable colored output\n";
    std::cout << "  .clear                   Clear the screen\n";
    std::cout << "  .history                 Show command history\n";
    std::cout << "\n";

    // Transaction commands
    std::cout << color(Color::CYAN) << "Transaction Commands:\n" << Color::RESET;
    std::cout << "  BEGIN [TRANSACTION]      Start a new transaction\n";
    std::cout << "  COMMIT                   Commit current transaction\n";
    std::cout << "  ROLLBACK                 Rollback current transaction\n";
    std::cout << "\n";

    // SQL commands
    std::cout << color(Color::CYAN) << "SQL Commands:\n" << Color::RESET;
    std::cout << "  CREATE TABLE             Create a new table\n";
    std::cout << "  CREATE INDEX             Create an index\n";
    std::cout << "  DROP TABLE               Drop a table\n";
    std::cout << "  DROP INDEX               Drop an index\n";
    std::cout << "  INSERT INTO              Insert data\n";
    std::cout << "  SELECT                   Query data\n";
    std::cout << "  UPDATE                   Update data\n";
    std::cout << "  DELETE FROM              Delete data\n";
    std::cout << "  ALTER TABLE              Modify table structure\n";
    std::cout << "\n";

    // Examples
    std::cout << color(Color::CYAN) << "Examples:\n" << Color::RESET;
    std::cout << "  CREATE TABLE users (id INT PRIMARY KEY, name VARCHAR(100));\n";
    std::cout << "  INSERT INTO users VALUES (1, 'Alice');\n";
    std::cout << "  SELECT * FROM users WHERE id = 1;\n";
    std::cout << "  CREATE INDEX idx_name ON users(name);\n";
    std::cout << "\n";
  }

  std::string get_input() {
    std::cout << get_prompt();
    std::cout.flush();

    std::string input;
    std::getline(std::cin, input);

    // Handle multi-line SQL statements
    if (!input.empty() && input.back() != ';' && !is_meta_command(input)) {
      std::string continuation;
      while (true) {
        std::cout << "   ...> ";
        std::cout.flush();
        std::getline(std::cin, continuation);
        input += " " + continuation;
        if (continuation.find(';') != std::string::npos || is_meta_command(continuation)) {
          break;
        }
      }
    }

    return trim(input);
  }

  std::string get_prompt() {
    if (current_transaction_) {
      return color(Color::YELLOW) + "latticedb*> " + Color::RESET;
    }
    return color(Color::GREEN) + prompt_ + Color::RESET;
  }

  void process_command(const std::string& input) {
    if (input.empty())
      return;

    // Check for meta commands
    if (is_meta_command(input)) {
      process_meta_command(input);
      return;
    }

    // Process SQL command
    process_sql_command(input);
  }

  bool is_meta_command(const std::string& input) {
    std::string trimmed = trim(input);
    if (trimmed.empty())
      return false;

    if (trimmed[0] == '.' || trimmed[0] == '\\')
      return true;

    std::istringstream iss(trimmed);
    std::string cmd;
    iss >> cmd;
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    return cmd == "help" || cmd == "h" || cmd == "exit" || cmd == "quit" || cmd == "q" ||
           cmd == "tables" || cmd == "dt" || cmd == "schema" || cmd == "d" ||
           cmd == "indexes" || cmd == "di" || cmd == "import" || cmd == "export" ||
           cmd == "timing" || cmd == "color" || cmd == "clear" || cmd == "c" ||
           cmd == "history";
  }

  void process_meta_command(const std::string& input) {
    std::istringstream iss(input);
    std::string cmd;
    iss >> cmd;

    // Convert to lowercase for comparison
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    if (!cmd.empty() && (cmd[0] == '.' || cmd[0] == '\\')) {
      cmd = cmd.substr(1);
    }

    if (cmd == "help" || cmd == "h") {
      print_help();
    } else if (cmd == "quit" || cmd == "exit" || cmd == "q") {
      running_ = false;
    } else if (cmd == "tables" || cmd == "dt") {
      show_tables();
    } else if (cmd == "schema" || cmd == "d") {
      std::string table_name;
      iss >> table_name;
      show_schema(table_name);
    } else if (cmd == "indexes" || cmd == "di") {
      show_indexes();
    } else if (cmd == "import") {
      std::string filename;
      iss >> filename;
      import_sql(filename);
    } else if (cmd == "export") {
      std::string filename;
      iss >> filename;
      export_database(filename);
    } else if (cmd == "timing") {
      std::string option;
      iss >> option;
      set_timing(option);
    } else if (cmd == "color") {
      std::string option;
      iss >> option;
      set_color(option);
    } else if (cmd == "clear" || cmd == "c") {
      clear_screen();
    } else if (cmd == "history") {
      show_history();
    } else {
      std::cout << color(Color::RED) << "Unknown meta command: " << cmd << Color::RESET << "\n";
      std::cout << "Type '.help' for available commands.\n";
    }
  }

  void process_sql_command(const std::string& sql) {
    auto start = std::chrono::high_resolution_clock::now();

    // Handle transaction commands specially
    std::string upper_sql = to_upper(trim(sql));
    if (upper_sql.find("BEGIN") == 0) {
      begin_transaction();
      return;
    } else if (upper_sql == "COMMIT" || upper_sql == "COMMIT;") {
      commit_transaction();
      return;
    } else if (upper_sql == "ROLLBACK" || upper_sql == "ROLLBACK;") {
      rollback_transaction();
      return;
    }

    // Execute the SQL command
    QueryResult result = db_engine_->execute_sql(sql, current_transaction_);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Display results
    if (result.success) {
      if (!result.rows.empty()) {
        display_result_table(result);
      } else if (result.rows_affected > 0) {
        std::cout << color(Color::GREEN) << "Query OK, " << result.rows_affected
                  << " row(s) affected";
      } else {
        std::cout << color(Color::GREEN) << "Query OK";
      }

      if (timing_enabled_) {
        std::cout << " (" << duration.count() << " ms)";
      }
      std::cout << Color::RESET << "\n";
    } else {
      std::cout << color(Color::RED) << "Error: " << result.message << Color::RESET << "\n";
    }
  }

  void display_result_table(const QueryResult& result) {
    if (result.rows.empty()) {
      std::cout << "Empty set\n";
      return;
    }

    // Calculate column widths
    std::vector<size_t> col_widths;
    for (const auto& col_name : result.column_names) {
      col_widths.push_back(col_name.length());
    }

    for (const auto& row : result.rows) {
      for (size_t i = 0; i < row.size() && i < col_widths.size(); ++i) {
        std::string value_str = value_to_string(row[i]);
        col_widths[i] = std::max(col_widths[i], value_str.length());
      }
    }

    // Print header separator
    print_table_separator(col_widths);

    // Print column names
    std::cout << "│";
    for (size_t i = 0; i < result.column_names.size(); ++i) {
      std::cout << " " << color(Color::CYAN) << std::setw(col_widths[i]) << std::left
                << result.column_names[i] << Color::RESET << " │";
    }
    std::cout << "\n";

    // Print header separator
    print_table_separator(col_widths, true);

    // Print rows
    for (const auto& row : result.rows) {
      std::cout << "│";
      for (size_t i = 0; i < row.size(); ++i) {
        std::cout << " " << std::setw(col_widths[i]) << std::left << value_to_string(row[i])
                  << " │";
      }
      std::cout << "\n";
    }

    // Print footer separator
    print_table_separator(col_widths);

    std::cout << result.rows.size() << " row(s) in set\n";
  }

  void print_table_separator(const std::vector<size_t>& widths, bool is_header = false) {
    if (is_header) {
      std::cout << "├";
      for (size_t i = 0; i < widths.size(); ++i) {
        for (size_t j = 0; j < widths[i] + 2; ++j) {
          std::cout << "─";
        }
        if (i < widths.size() - 1) {
          std::cout << "┼";
        }
      }
      std::cout << "┤\n";
    } else {
      std::cout << "┌";
      for (size_t i = 0; i < widths.size(); ++i) {
        for (size_t j = 0; j < widths[i] + 2; ++j) {
          std::cout << "─";
        }
        if (i < widths.size() - 1) {
          std::cout << "┬";
        }
      }
      std::cout << "┐\n";
    }
  }

  std::string value_to_string(const Value& v) {
    if (v.is_null()) {
      return "NULL";
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
    case ValueType::DECIMAL:
      return std::to_string(v.get<double>());
    case ValueType::VARCHAR:
    case ValueType::TEXT:
    case ValueType::TIMESTAMP:
    case ValueType::DATE:
    case ValueType::TIME:
      return v.get<std::string>();
    case ValueType::VECTOR: {
      std::ostringstream oss;
      oss << "[";
      const auto& vec = v.get<std::vector<double>>();
      for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0)
          oss << ", ";
        oss << vec[i];
      }
      oss << "]";
      return oss.str();
    }
    case ValueType::BLOB:
      return "<BLOB>";
    default:
      return "";
    }
  }

  void show_tables() {
    std::vector<std::string> tables = db_engine_->get_table_names();
    if (tables.empty()) {
      std::cout << "No tables found in database.\n";
      return;
    }

    std::cout << color(Color::CYAN) << "Tables in database:\n" << Color::RESET;
    std::cout << "═══════════════════\n";
    for (const auto& table : tables) {
      std::cout << "  • " << table << "\n";
    }
    std::cout << "\nTotal: " << tables.size() << " table(s)\n";
  }

  void show_schema(const std::string& table_name) {
    if (table_name.empty()) {
      // Show all tables' schemas
      std::vector<std::string> tables = db_engine_->get_table_names();
      for (const auto& table : tables) {
        show_single_table_schema(table);
        std::cout << "\n";
      }
    } else {
      show_single_table_schema(table_name);
    }
  }

  void show_single_table_schema(const std::string& table_name) {
    auto table_info = db_engine_->get_table_info(table_name);
    if (!table_info) {
      std::cout << color(Color::RED) << "Table '" << table_name << "' not found." << Color::RESET
                << "\n";
      return;
    }

    std::cout << color(Color::CYAN) << "Table: " << table_name << Color::RESET << "\n";
    std::cout << "═══════════════════════════════════════\n";

    // Display column information
    const Schema& schema = table_info->get_schema();
    std::cout << color(Color::YELLOW) << "Columns:\n" << Color::RESET;
    for (size_t i = 0; i < schema.column_count(); ++i) {
      const auto& col = schema.get_column(i);
      std::cout << "  " << std::setw(20) << std::left << col.name() << " "
                << value_type_to_string(col.type());
      if (!col.is_nullable()) {
        std::cout << " NOT NULL";
      }
      std::cout << "\n";
    }

    // Note: Index information would need to be retrieved separately from catalog
    std::cout << "\n" << color(Color::YELLOW) << "Indexes:\n" << Color::RESET;
    std::cout << "  (Index listing not yet implemented)\n";
  }

  void show_indexes() {
    std::cout << color(Color::CYAN) << "Indexes in database:\n" << Color::RESET;
    std::cout << "═══════════════════════════════════════\n";
    std::cout << "(Index listing not yet implemented)\n";
  }

  void import_sql(const std::string& filename) {
    if (filename.empty()) {
      std::cout << color(Color::RED) << "Error: Please specify a file to import." << Color::RESET
                << "\n";
      return;
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
      std::cout << color(Color::RED) << "Error: Cannot open file '" << filename << "'"
                << Color::RESET << "\n";
      return;
    }

    std::cout << "Importing from " << filename << "...\n";

    std::string sql;
    std::string line;
    size_t executed = 0;
    size_t failed = 0;

    while (std::getline(file, line)) {
      // Skip comments and empty lines
      line = trim(line);
      if (line.empty() || line.substr(0, 2) == "--" || line[0] == '#') {
        continue;
      }

      sql += line + " ";

      // Check if we have a complete statement
      if (line.find(';') != std::string::npos) {
        QueryResult result = db_engine_->execute_sql(sql, current_transaction_);
        if (result.success) {
          executed++;
        } else {
          failed++;
          std::cout << color(Color::RED) << "Failed: " << sql << "\nError: " << result.message
                    << Color::RESET << "\n";
        }
        sql.clear();
      }
    }

    std::cout << color(Color::GREEN) << "Import completed. Executed: " << executed
              << ", Failed: " << failed << Color::RESET << "\n";
  }

  void export_database(const std::string& filename) {
    if (filename.empty()) {
      std::cout << color(Color::RED) << "Error: Please specify a file to export to." << Color::RESET
                << "\n";
      return;
    }

    std::ofstream file(filename);
    if (!file.is_open()) {
      std::cout << color(Color::RED) << "Error: Cannot create file '" << filename << "'"
                << Color::RESET << "\n";
      return;
    }

    std::cout << "Exporting database to " << filename << "...\n";

    // Write header
    file << "-- LatticeDB Database Export\n";
    file << "-- Generated: " << get_current_timestamp() << "\n\n";

    // Export table schemas
    std::vector<std::string> tables = db_engine_->get_table_names();
    for (const auto& table : tables) {
      file << "-- Table: " << table << "\n";
      // TODO: Generate CREATE TABLE statement
      file << "-- CREATE TABLE statement would go here\n\n";

      // Export table data
      QueryResult result = db_engine_->execute_sql("SELECT * FROM " + table);
      if (result.success && !result.rows.empty()) {
        for (const auto& row : result.rows) {
          file << "INSERT INTO " << table << " VALUES (";
          for (size_t i = 0; i < row.size(); ++i) {
            if (i > 0)
              file << ", ";
            file << "'" << value_to_string(row[i]) << "'";
          }
          file << ");\n";
        }
        file << "\n";
      }
    }

    std::cout << color(Color::GREEN) << "Export completed successfully." << Color::RESET << "\n";
  }

  void begin_transaction() {
    if (current_transaction_) {
      std::cout << color(Color::YELLOW) << "Already in a transaction." << Color::RESET << "\n";
      return;
    }

    current_transaction_ = db_engine_->begin_transaction();
    if (current_transaction_) {
      std::cout << color(Color::GREEN) << "Transaction started." << Color::RESET << "\n";
    } else {
      std::cout << color(Color::RED) << "Failed to start transaction." << Color::RESET << "\n";
    }
  }

  void commit_transaction() {
    if (!current_transaction_) {
      std::cout << color(Color::YELLOW) << "No active transaction." << Color::RESET << "\n";
      return;
    }

    if (db_engine_->commit_transaction(current_transaction_)) {
      std::cout << color(Color::GREEN) << "Transaction committed." << Color::RESET << "\n";
      current_transaction_ = nullptr;
    } else {
      std::cout << color(Color::RED) << "Failed to commit transaction." << Color::RESET << "\n";
    }
  }

  void rollback_transaction() {
    if (!current_transaction_) {
      std::cout << color(Color::YELLOW) << "No active transaction." << Color::RESET << "\n";
      return;
    }

    if (db_engine_->abort_transaction(current_transaction_)) {
      std::cout << color(Color::GREEN) << "Transaction rolled back." << Color::RESET << "\n";
      current_transaction_ = nullptr;
    } else {
      std::cout << color(Color::RED) << "Failed to rollback transaction." << Color::RESET << "\n";
    }
  }

  void set_timing(const std::string& option) {
    if (option == "on") {
      timing_enabled_ = true;
      std::cout << "Timing enabled.\n";
    } else if (option == "off") {
      timing_enabled_ = false;
      std::cout << "Timing disabled.\n";
    } else {
      std::cout << "Usage: .timing on|off\n";
    }
  }

  void set_color(const std::string& option) {
    if (option == "on") {
      color_output_ = true;
      std::cout << "Color output enabled.\n";
    } else if (option == "off") {
      color_output_ = false;
      std::cout << "Color output disabled.\n";
    } else {
      std::cout << "Usage: .color on|off\n";
    }
  }

  void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
  }

  void show_history() {
    std::cout << color(Color::CYAN) << "Command History:\n" << Color::RESET;
    std::cout << "═══════════════════\n";
    for (size_t i = 0; i < command_history_.size(); ++i) {
      std::cout << std::setw(4) << (i + 1) << ": " << command_history_[i] << "\n";
    }
  }

  // Utility functions
  std::string color(const std::string& code) {
    return color_output_ ? code : "";
  }

  std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos)
      return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
  }

  std::string to_upper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
  }

  std::string value_type_to_string(ValueType type) {
    switch (type) {
    case ValueType::NULL_TYPE:
      return "NULL";
    case ValueType::BOOLEAN:
      return "BOOLEAN";
    case ValueType::TINYINT:
      return "TINYINT";
    case ValueType::SMALLINT:
      return "SMALLINT";
    case ValueType::INTEGER:
      return "INTEGER";
    case ValueType::BIGINT:
      return "BIGINT";
    case ValueType::DECIMAL:
      return "DECIMAL";
    case ValueType::REAL:
      return "REAL";
    case ValueType::DOUBLE:
      return "DOUBLE";
    case ValueType::VARCHAR:
      return "VARCHAR";
    case ValueType::TEXT:
      return "TEXT";
    case ValueType::TIMESTAMP:
      return "TIMESTAMP";
    case ValueType::DATE:
      return "DATE";
    case ValueType::TIME:
      return "TIME";
    case ValueType::BLOB:
      return "BLOB";
    case ValueType::VECTOR:
      return "VECTOR";
    default:
      return "UNKNOWN";
    }
  }

  std::string get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
  }
};

// Signal handler for graceful shutdown
std::unique_ptr<LatticeDBCLI> g_cli;

void signal_handler(int signal) {
  if (signal == SIGINT) {
    std::cout << "\n\nInterrupted. Type '.quit' to exit.\n";
    // Reset the signal handler
    std::signal(SIGINT, signal_handler);
  }
}

int main(int argc, char* argv[]) {
  // Parse command line arguments
  std::string db_file = "latticedb.db";
  bool show_help = false;
  bool show_version = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      show_help = true;
    } else if (arg == "--version" || arg == "-v") {
      show_version = true;
    } else if (arg == "--database" || arg == "-d") {
      if (i + 1 < argc) {
        db_file = argv[++i];
      }
    } else if (!arg.empty() && arg[0] != '-') {
      db_file = arg;
    }
  }

  if (show_help) {
    std::cout << "LatticeDB CLI v1.0.0\n\n";
    std::cout << "Usage: " << argv[0] << " [options] [database]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -d, --database <file>  Database file to connect to\n";
    std::cout << "  -h, --help            Show this help message\n";
    std::cout << "  -v, --version         Show version information\n";
    return 0;
  }

  if (show_version) {
    std::cout << "LatticeDB CLI v1.0.0\n";
    std::cout << "Built with C++17\n";
    return 0;
  }

  // Set up signal handler
  std::signal(SIGINT, signal_handler);

  // Create and run the CLI
  try {
    g_cli = std::make_unique<LatticeDBCLI>(db_file);

    if (!g_cli->initialize()) {
      std::cerr << "Failed to initialize LatticeDB CLI\n";
      return 1;
    }

    g_cli->run();

  } catch (const std::exception& e) {
    std::cerr << "Fatal error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
