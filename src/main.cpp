#include <iostream>
#include <iomanip>
#include <unistd.h>
#include "engine.h"

static void print_value(const Value& v) {
    if (std::holds_alternative<std::monostate>(v.v)) std::cout << "NULL";
    else if (std::holds_alternative<int64_t>(v.v)) std::cout << std::get<int64_t>(v.v);
    else if (std::holds_alternative<double>(v.v)) std::cout << std::fixed << std::setprecision(6) << std::get<double>(v.v);
    else if (std::holds_alternative<std::string>(v.v)) std::cout << "'" << std::get<std::string>(v.v) << "'";
    else if (std::holds_alternative<std::set<std::string>>(v.v)) {
        std::cout << "{";
        bool first=true; for (auto& s : std::get<std::set<std::string>>(v.v)) { if(!first) std::cout<<","; std::cout<<"'"<<s<<"'"; first=false; }
        std::cout << "}";
    } else if (std::holds_alternative<std::vector<double>>(v.v)) {
        std::cout << "[";
        const auto& ar = std::get<std::vector<double>>(v.v);
        for (size_t i=0;i<ar.size();++i){ if(i) std::cout<<","; std::cout<<ar[i]; }
        std::cout << "]";
    }
}

static void print_result(const QueryResult& r) {
    if (!r.ok) { std::cout << "ERROR: " << r.message << "\n"; return; }
    if (!r.headers.empty()) {
        for (size_t i=0;i<r.headers.size();++i){ if(i) std::cout<<" | "; std::cout<<r.headers[i]; }
        std::cout << "\n";
    }
    for (auto& row : r.rows) {
        for (size_t i=0;i<row.cols.size();++i){ if(i) std::cout<<" | "; print_value(row.cols[i]); }
        std::cout << "\n";
    }
    if (!r.message.empty() && r.rows.empty()) std::cout << r.message << "\n";
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Engine eng;
    bool interactive = isatty(fileno(stdin)) && isatty(fileno(stdout));
    if (interactive) std::cout << "ForgeDB (C++17) - type EXIT to quit\n";
    std::string line, sql;
    while (true) {
        if (interactive) std::cout << "fdb> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        sql += line;
        if (sql.back()==';') {
            while (!sql.empty() && (sql.back()==';' || std::isspace((unsigned char)sql.back()))) sql.pop_back();
            auto r = eng.execute(sql);
            if (r.message=="EXIT" && r.ok) { std::cout << "Bye.\n"; break; }
            print_result(r);
            sql.clear();
        } else {
            sql.push_back(' ');
        }
    }
    return 0;
}
