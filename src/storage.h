#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <limits>
#include <functional>
#include "catalog.h"
#include "util.h"

struct RowVersion {
    std::string row_id;      // derived from PK
    int64_t tx_from = 0;
    int64_t tx_to   = std::numeric_limits<int64_t>::max();
    std::string valid_from = util::now_iso();
    std::string valid_to   = "9999-12-31T23:59:59Z";
    std::vector<Value> data; // aligned to columns
};

struct TableData {
    TableDef def;
    std::vector<RowVersion> versions; // append-only
};

struct Database {
    Catalog catalog;
    std::unordered_map<std::string, TableData> data;
    int64_t next_tx = 1;

    void create_table(const TableDef& t) {
        catalog.add_table(t);
        TableData td; td.def = *catalog.get(t.name);
        data[util::upper(t.name)] = std::move(td);
    }
    void drop_table(const std::string& name) {
        auto u = util::upper(name);
        data.erase(u);
        catalog.tables.erase(u);
    }

    TableData* table(const std::string& t) {
        auto it = data.find(util::upper(t));
        if (it==data.end()) return nullptr;
        return &it->second;
    }

    int64_t begin_tx() { return next_tx++; }

    void save(const std::string& path) const;
    bool load(const std::string& path);
};
