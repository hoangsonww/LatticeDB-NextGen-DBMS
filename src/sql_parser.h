#pragma once
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include "crdt.h"

struct Condition {
    enum class Op { EQ, NE, LT, LE, GT, GE, IS_NULL, IS_NOT_NULL } op = Op::EQ;
    std::string column;      // optionally qualified t.col
    Value literal;
    bool is_distance = false;
    std::string vec_col;
    std::vector<double> vec_value;
    double dist_threshold = 0.0;
};

struct JoinClause {
    std::string right_table;
    std::string left_col;   // t1.col
    std::string right_col;  // t2.col
};

struct SelectItem {
    enum class Kind { COLUMN, STAR, DP_COUNT, AGG_COUNT, AGG_SUM, AGG_AVG, AGG_MIN, AGG_MAX } kind = Kind::COLUMN;
    std::string column; // for COLUMN/SUM/AVG/MIN/MAX
};

struct SelectQuery {
    std::string table;
    std::optional<JoinClause> join;
    std::vector<SelectItem> items;
    std::optional<int64_t> asof_tx; // FOR SYSTEM_TIME AS OF TX n
    std::vector<Condition> where;
    std::vector<std::string> group_by; // optional
    std::optional<std::pair<std::string,bool>> order_by; // col, desc?
    std::optional<int64_t> limit_n;
};

struct InsertQuery {
    std::string table;
    std::vector<std::string> cols;
    std::vector<std::vector<Value>> rows;
    bool on_conflict_merge = false;
};

struct UpdateQuery {
    std::string table;
    std::unordered_map<std::string, Value> sets;
    std::vector<Condition> where;
    std::optional<std::pair<std::string,std::string>> valid_period; // [from,to)
};

struct DeleteQuery {
    std::string table;
    std::vector<Condition> where;
};

struct CreateTableQuery {
    std::string table;
    struct Col {
        std::string name;
        ColumnType type;
        bool pk=false;
        MergeSpec merge;
        size_t vec_dim=0;
    };
    std::vector<Col> cols;
    bool mergeable = true;
};

struct DropTableQuery { std::string table; };

struct SetQuery {
    std::string key; // DP_EPSILON
    double value = 1.0;
};
struct SaveQuery { std::string path; };
struct LoadQuery { std::string path; };
struct ExitQuery {};
struct BeginQuery {};
struct CommitQuery {};
struct RollbackQuery {};

struct ParsedSQL {
    enum class Kind {
        CREATE, DROP, INSERT, UPDATE, DELETE, SELECT, SET, SAVE, LOAD, EXIT, BEGIN_, COMMIT_, ROLLBACK_, INVALID
    } kind = Kind::INVALID;
    std::string error;

    CreateTableQuery create;
    DropTableQuery drop;
    InsertQuery insert;
    UpdateQuery update;
    DeleteQuery del;
    SelectQuery select;
    SetQuery setq;
    SaveQuery saveq;
    LoadQuery loadq;
    ExitQuery exitq;
    BeginQuery beginq;
    CommitQuery commitq;
    RollbackQuery rollbackq;
};

ParsedSQL parse_sql(const std::string& sql);
