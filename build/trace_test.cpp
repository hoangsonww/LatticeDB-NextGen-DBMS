#include <iostream>
#include <fstream>
int main() {
    std::fstream file;
    file.open("test_write.db", std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::ofstream create("test_write.db", std::ios::out | std::ios::binary);
        create.close();
        file.open("test_write.db", std::ios::in | std::ios::out | std::ios::binary);
    }
    file.seekp(0, std::ios::beg);
    file.write("TEST", 4);
    file.flush();
    file.close();
    return 0;
}
