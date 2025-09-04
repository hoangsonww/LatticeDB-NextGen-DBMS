#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include "crdt.h"
#include "util.h"

struct ColumnDef {
    std::string name;
    ColumnType type;
    MergeSpec merge;
    size_t vector_dim = 0; // for VECTOR
    bool primary_key = false;
};

struct TableDef {
    std::string name;
    std::vector<ColumnDef> cols;
    bool mergeable = true; // MRT toggle
    int pk_index = -1;

    int col_index(const std::string& n) const {
        for (size_t i=0;i<cols.size();++i) if (util::iequals(cols[i].name, n)) return (int)i;
        return -1;
    }
};

struct Catalog {
    std::unordered_map<std::string, TableDef> tables;

    bool has_table(const std::string& t) const {
        return tables.find(util::upper(t)) != tables.end();
    }
    void add_table(const TableDef& d) {
        TableDef x = d;
        x.name = util::upper(d.name);
        x.pk_index = -1;
        for (size_t i=0;i<x.cols.size();++i) if (x.cols[i].primary_key) x.pk_index=(int)i;
        tables[x.name] = x;
    }
    const TableDef* get(const std::string& t) const {
        auto it = tables.find(util::upper(t));
        if (it==tables.end()) return nullptr;
        return &it->second;
    }
    TableDef* get_mut(const std::string& t) {
        auto it = tables.find(util::upper(t));
        if (it==tables.end()) return nullptr;
        return &it->second;
    }
};
