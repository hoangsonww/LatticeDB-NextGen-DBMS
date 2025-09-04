#pragma once
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include "storage.h"
#include "sql_parser.h"
#include "dp.h"
#include "vector.h"

struct QueryRow { std::vector<Value> cols; };
struct QueryResult {
    std::vector<std::string> headers;
    std::vector<QueryRow> rows;
    std::string message;
    bool ok = true;
};

struct PendingWrite {
    std::function<void()> apply;
    std::function<void()> undo;
};

struct ExecutionContext {
    Database* db = nullptr;
    double dp_epsilon = 1.0;
    dp::Laplace lap;
    bool in_tx = false;
    std::vector<PendingWrite> staged;
};

QueryResult exec_create(ExecutionContext&, const CreateTableQuery&);
QueryResult exec_drop(ExecutionContext&, const DropTableQuery&);
QueryResult exec_insert(ExecutionContext&, const InsertQuery&);
QueryResult exec_update(ExecutionContext&, const UpdateQuery&);
QueryResult exec_delete(ExecutionContext&, const DeleteQuery&);
QueryResult exec_select(ExecutionContext&, const SelectQuery&);
QueryResult exec_set(ExecutionContext&, const SetQuery&);
QueryResult exec_save(ExecutionContext&, const SaveQuery&);
QueryResult exec_load(ExecutionContext&, const LoadQuery&);
QueryResult exec_begin(ExecutionContext&);
QueryResult exec_commit(ExecutionContext&);
QueryResult exec_rollback(ExecutionContext&);
