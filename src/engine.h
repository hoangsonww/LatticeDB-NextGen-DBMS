#pragma once
#include "storage.h"
#include "sql_parser.h"
#include "executor.h"

class Engine {
    Database db_;
    ExecutionContext ctx_;
public:
    Engine() { ctx_.db = &db_; }

    QueryResult execute(const std::string& sql) {
        auto p = parse_sql(sql);
        if (p.kind==ParsedSQL::Kind::INVALID) return {{},{}, p.error, false};

        switch (p.kind) {
            case ParsedSQL::Kind::CREATE:   return exec_create(ctx_, p.create);
            case ParsedSQL::Kind::DROP:     return exec_drop(ctx_, p.drop);
            case ParsedSQL::Kind::INSERT:   return exec_insert(ctx_, p.insert);
            case ParsedSQL::Kind::UPDATE:   return exec_update(ctx_, p.update);
            case ParsedSQL::Kind::DELETE:   return exec_delete(ctx_, p.del);
            case ParsedSQL::Kind::SELECT:   return exec_select(ctx_, p.select);
            case ParsedSQL::Kind::SET:      return exec_set(ctx_, p.setq);
            case ParsedSQL::Kind::SAVE:     return exec_save(ctx_, p.saveq);
            case ParsedSQL::Kind::LOAD:     return exec_load(ctx_, p.loadq);
            case ParsedSQL::Kind::BEGIN_:   return exec_begin(ctx_);
            case ParsedSQL::Kind::COMMIT_:  return exec_commit(ctx_);
            case ParsedSQL::Kind::ROLLBACK_:return exec_rollback(ctx_);
            case ParsedSQL::Kind::EXIT:     return QueryResult{{},{}, "EXIT", true};
            default: break;
        }
        return {{},{}, "Unsupported", false};
    }
};
