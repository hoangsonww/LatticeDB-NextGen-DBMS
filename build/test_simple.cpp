#include <iostream>
#include "../src/engine/database_engine.h"

int main() {
    std::cout << "Starting test..." << std::endl;
    try {
        latticedb::DatabaseEngine engine("test.db", false);
        std::cout << "Engine created" << std::endl;

        if (!engine.initialize()) {
            std::cout << "Failed to initialize" << std::endl;
            return 1;
        }
        std::cout << "Engine initialized" << std::endl;

        auto result = engine.execute_sql("CREATE TABLE test (id INTEGER)", nullptr);
        std::cout << "Result: " << result.message << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }

    std::cout << "Test complete" << std::endl;
    return 0;
}