#pragma once

#include "../types/value.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace latticedb {

enum class QueryType {
  INVALID,
  SELECT,
  INSERT,
  UPDATE,
  DELETE,
  CREATE_TABLE,
  DROP_TABLE,
  CREATE_INDEX,
  DROP_INDEX,
  BEGIN,
  COMMIT,
  ROLLBACK
};

enum class ExpressionType { CONSTANT, COLUMN_REF, OPERATOR, FUNCTION, STAR, AGGREGATE };

enum class AggregateType { COUNT, SUM, AVG, MIN, MAX };

enum class OperatorType {
  PLUS,
  MINUS,
  MULTIPLY,
  DIVIDE,
  EQUAL,
  NOT_EQUAL,
  LESS_THAN,
  LESS_THAN_EQUAL,
  GREATER_THAN,
  GREATER_THAN_EQUAL,
  AND,
  OR,
  NOT
};

class Expression {
public:
  ExpressionType type;
  std::string column_name;
  std::string table_alias;
  Value value;
  OperatorType op_type;
  AggregateType agg_type;
  std::vector<std::unique_ptr<Expression>> children;

  explicit Expression(ExpressionType t) : type(t) {}

  static std::unique_ptr<Expression> make_constant(const Value& val) {
    auto expr = std::make_unique<Expression>(ExpressionType::CONSTANT);
    expr->value = val;
    return expr;
  }

  static std::unique_ptr<Expression> make_column_ref(const std::string& col_name) {
    auto expr = std::make_unique<Expression>(ExpressionType::COLUMN_REF);
    expr->column_name = col_name;
    return expr;
  }

  static std::unique_ptr<Expression>
  make_operator(OperatorType op, std::vector<std::unique_ptr<Expression>> children) {
    auto expr = std::make_unique<Expression>(ExpressionType::OPERATOR);
    expr->op_type = op;
    expr->children = std::move(children);
    return expr;
  }

  static std::unique_ptr<Expression> make_aggregate(AggregateType agg_type,
                                                    std::unique_ptr<Expression> arg = nullptr) {
    auto expr = std::make_unique<Expression>(ExpressionType::AGGREGATE);
    expr->agg_type = agg_type;
    if (arg) {
      expr->children.push_back(std::move(arg));
    }
    return expr;
  }
};

struct OrderByClause {
  std::string column_name;
  bool is_ascending = true;
};

enum class JoinType { INNER, LEFT, RIGHT, FULL };

struct JoinClause {
  JoinType type = JoinType::INNER;
  std::string table_name;
  std::unique_ptr<Expression> condition;
};

struct SelectQuery {
  std::vector<std::unique_ptr<Expression>> select_list;
  std::string table_name;
  std::vector<JoinClause> joins;
  std::unique_ptr<Expression> where_clause;
  std::vector<std::string> group_by;
  std::unique_ptr<Expression> having_clause;
  std::vector<OrderByClause> order_by;
  std::optional<int> limit;
  bool distinct = false;
  std::optional<int> system_time_tx; // FOR SYSTEM_TIME AS OF TX n
};

struct InsertQuery {
  std::string table_name;
  std::vector<std::string> columns;
  std::vector<std::vector<Value>> values;
};

struct UpdateQuery {
  std::string table_name;
  std::vector<std::pair<std::string, Value>> assignments;
  std::unique_ptr<Expression> where_clause;
};

struct DeleteQuery {
  std::string table_name;
  std::unique_ptr<Expression> where_clause;
};

struct ColumnDefinition {
  std::string name;
  ValueType type;
  bool nullable = true;
  bool primary_key = false;
  bool unique = false;
  std::optional<Value> default_value;
  int length = 0; // for VARCHAR
};

struct CreateTableQuery {
  std::string table_name;
  std::vector<ColumnDefinition> columns;
  std::vector<std::string> primary_key_columns;
};

struct DropTableQuery {
  std::string table_name;
  bool if_exists = false;
};

struct CreateIndexQuery {
  std::string index_name;
  std::string table_name;
  std::vector<std::string> columns;
  bool unique = false;
};

struct DropIndexQuery {
  std::string index_name;
  bool if_exists = false;
};

struct ParsedQuery {
  QueryType type;
  std::string error_message;

  // Query-specific data (only one will be set)
  std::unique_ptr<SelectQuery> select;
  std::unique_ptr<InsertQuery> insert;
  std::unique_ptr<UpdateQuery> update;
  std::unique_ptr<DeleteQuery> delete_query;
  std::unique_ptr<CreateTableQuery> create_table;
  std::unique_ptr<DropTableQuery> drop_table;
  std::unique_ptr<CreateIndexQuery> create_index;
  std::unique_ptr<DropIndexQuery> drop_index;

  ParsedQuery() : type(QueryType::INVALID) {}
};

class SQLParser {
private:
  std::string sql_;
  size_t pos_;
  std::string current_token_;

public:
  ParsedQuery parse(const std::string& sql);

private:
  void skip_whitespace();
  std::string next_token();
  std::string peek_token();
  bool expect_token(const std::string& expected);
  bool is_keyword(const std::string& token) const;
  bool is_operator(const std::string& token) const;
  ValueType parse_column_type(const std::string& type_str);

  std::unique_ptr<SelectQuery> parse_select();
  std::unique_ptr<InsertQuery> parse_insert();
  std::unique_ptr<UpdateQuery> parse_update();
  std::unique_ptr<DeleteQuery> parse_delete();
  std::unique_ptr<CreateTableQuery> parse_create_table();
  std::unique_ptr<DropTableQuery> parse_drop_table();
  std::unique_ptr<CreateIndexQuery> parse_create_index();
  std::unique_ptr<DropIndexQuery> parse_drop_index();

  std::unique_ptr<Expression> parse_expression();
  std::unique_ptr<Expression> parse_and_or();
  std::unique_ptr<Expression> parse_comparison();
  std::unique_ptr<Expression> parse_term();
  std::unique_ptr<Expression> parse_factor();
  std::unique_ptr<Expression> parse_primary();

  std::vector<std::unique_ptr<Expression>> parse_select_list();
  std::unique_ptr<Expression> parse_where_clause();
  std::vector<OrderByClause> parse_order_by();

  Value parse_literal();
  ColumnDefinition parse_column_definition();

  void set_error(const std::string& message);
  bool match_keyword(const std::string& kw);
};

} // namespace latticedb
