#include <iostream>
#include <iomanip>
#include <string>
#include <memory>
#include "engine/database_engine.h"
#include "common/logger.h"

using namespace latticedb;

static void print_welcome() {
    std::cout << "=== LatticeDB - Modern Relational Database Management System ===\n";
    std::cout << "Version 2.0 - Rebuilt with Professional DBMS Architecture\n";
    std::cout << "Features: Buffer Pool, B+Trees, ACID Transactions, Query Optimizer\n";
    std::cout << "Type 'help' for commands, 'exit' to quit.\n\n";
}

static void print_help() {
    std::cout << "Available Commands:\n";
    std::cout << "  help                     - Show this help message\n";
    std::cout << "  exit, quit               - Exit the database\n";
    std::cout << "  \\d                      - List all tables\n";
    std::cout << "  \\d <table>             - Describe table schema\n";
    std::cout << "  \\stats                 - Show database statistics\n";
    std::cout << "  BEGIN                    - Start transaction\n";
    std::cout << "  COMMIT                   - Commit transaction\n";
    std::cout << "  ROLLBACK                 - Rollback transaction\n";
    std::cout << "\nSQL Commands:\n";
    std::cout << "  CREATE TABLE <name> (...)  - Create a new table\n";
    std::cout << "  DROP TABLE <name>           - Drop a table\n";
    std::cout << "  INSERT INTO <table> VALUES (...) - Insert data\n";
    std::cout << "  SELECT ... FROM <table>     - Query data\n";
    std::cout << "  UPDATE <table> SET ...      - Update data\n";
    std::cout << "  DELETE FROM <table> ...     - Delete data\n";
    std::cout << "  CREATE INDEX <name> ON <table>(...) - Create index\n";
    std::cout << "  DROP INDEX <name>           - Drop index\n\n";
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
    std::cout << std::left << std::setw(20) << "Column" << std::setw(15) << "Type"
              << std::setw(10) << "Nullable" << "Default\n";
    std::cout << std::string(55, '-') << "\n";

    for (uint32_t i = 0; i < schema.column_count(); ++i) {
        const auto& col = schema.get_column(i);
        std::string type_str;
        switch (col.type()) {
            case ValueType::INTEGER: type_str = "INTEGER"; break;
            case ValueType::BIGINT: type_str = "BIGINT"; break;
            case ValueType::DOUBLE: type_str = "DOUBLE"; break;
            case ValueType::VARCHAR: type_str = "VARCHAR(" + std::to_string(col.length()) + ")"; break;
            case ValueType::TEXT: type_str = "TEXT"; break;
            case ValueType::BOOLEAN: type_str = "BOOLEAN"; break;
            case ValueType::TIMESTAMP: type_str = "TIMESTAMP"; break;
            case ValueType::DATE: type_str = "DATE"; break;
            case ValueType::TIME: type_str = "TIME"; break;
            case ValueType::BLOB: type_str = "BLOB"; break;
            case ValueType::VECTOR: type_str = "VECTOR"; break;
            default: type_str = "UNKNOWN"; break;
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

static void print_result(const QueryResult& result) {
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
            if (i > 0) std::cout << " | ";
            std::cout << std::left << std::setw(15) << result.column_names[i];
        }
        std::cout << "\n" << std::string(result.column_names.size() * 18 - 3, '-') << "\n";

        // Print rows
        for (const auto& row : result.rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                if (i > 0) std::cout << " | ";
                std::cout << std::left << std::setw(15) << row[i].to_string();
            }
            std::cout << "\n";
        }

        std::cout << "\n(" << result.rows.size() << " rows)\n";
    }
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
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --file <path>    Database file path (default: latticedb.db)\n";
            std::cout << "  --no-logging     Disable write-ahead logging\n";
            std::cout << "  --help           Show this help\n";
            return 0;
        }
    }

    // Initialize logger
    Logger::get_instance()->set_level(LogLevel::INFO);
    Logger::get_instance()->set_console_output(false);
    Logger::get_instance()->set_file_output(true);
    Logger::get_instance()->set_log_file("latticedb.log");

    try {
        // Initialize database engine
        auto engine = std::make_unique<DatabaseEngine>(db_file, enable_logging);

        if (!engine->initialize()) {
            std::cerr << "Failed to initialize database engine.\n";
            return 1;
        }

        print_welcome();

        std::string input;
        Transaction* current_txn = nullptr;

        while (true) {
            if (current_txn) {
                std::cout << "latticedb*> ";
            } else {
                std::cout << "latticedb> ";
            }

            if (!std::getline(std::cin, input)) {
                break;
            }

            // Trim whitespace
            size_t start = input.find_first_not_of(" \t");
            size_t end = input.find_last_not_of(" \t");
            if (start == std::string::npos) {
                continue;
            }
            input = input.substr(start, end - start + 1);

            if (input.empty()) {
                continue;
            }

            // Handle meta commands
            if (input == "exit" || input == "quit") {
                break;
            } else if (input == "help") {
                print_help();
                continue;
            } else if (input == "\\d") {
                print_table_list(engine.get());
                continue;
            } else if (input.substr(0, 2) == "\\d" && input.length() > 3) {
                std::string table_name = input.substr(3);
                print_table_schema(engine.get(), table_name);
                continue;
            } else if (input == "\\stats") {
                print_stats(engine.get());
                continue;
            }

            // Handle transaction commands
            if (input == "BEGIN" || input == "begin") {
                if (current_txn) {
                    std::cout << "Transaction already in progress.\n";
                } else {
                    current_txn = engine->begin_transaction();
                    std::cout << "BEGIN\n";
                }
                continue;
            } else if (input == "COMMIT" || input == "commit") {
                if (!current_txn) {
                    std::cout << "No transaction in progress.\n";
                } else {
                    if (engine->commit_transaction(current_txn)) {
                        std::cout << "COMMIT\n";
                    } else {
                        std::cout << "COMMIT failed.\n";
                    }
                    current_txn = nullptr;
                }
                continue;
            } else if (input == "ROLLBACK" || input == "rollback") {
                if (!current_txn) {
                    std::cout << "No transaction in progress.\n";
                } else {
                    if (engine->abort_transaction(current_txn)) {
                        std::cout << "ROLLBACK\n";
                    } else {
                        std::cout << "ROLLBACK failed.\n";
                    }
                    current_txn = nullptr;
                }
                continue;
            }

            // Execute SQL
            try {
                auto result = engine->execute_sql(input, current_txn);
                print_result(result);
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
            engine->abort_transaction(current_txn);
        }

        engine->shutdown();
        std::cout << "\nGoodbye!\n";

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}