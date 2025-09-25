#include "sql_parser.h"
#include <cctype>

namespace latticedb {

static std::string upper(const std::string& s) {
  std::string r = s;
  for (auto& c : r)
    c = static_cast<char>(std::toupper(c));
  return r;
}

ParsedQuery SQLParser::parse(const std::string& sql) {
  sql_ = sql;
  pos_ = 0;
  ParsedQuery result;
  auto up = upper(sql);
  if (up.rfind("CREATE TABLE", 0) == 0) {
    result.type = QueryType::CREATE_TABLE;
    result.create_table = parse_create_table();
    if (!result.create_table) {
      result.type = QueryType::INVALID;
      result.error_message = "Invalid CREATE TABLE";
    } else { /* ok */
    }
    return result;
  }
  if (up.rfind("INSERT INTO", 0) == 0) {
    result.type = QueryType::INSERT;
    result.insert = parse_insert();
    if (!result.insert) {
      result.type = QueryType::INVALID;
      result.error_message = "Invalid INSERT";
    }
    return result;
  }
  if (up.rfind("SELECT", 0) == 0) {
    result.type = QueryType::SELECT;
    result.select = parse_select();
    if (!result.select) {
      result.type = QueryType::INVALID;
      result.error_message = "Invalid SELECT";
    }
    return result;
  }
  if (up.rfind("UPDATE", 0) == 0) {
    result.type = QueryType::UPDATE;
    result.update = parse_update();
    if (!result.update) {
      result.type = QueryType::INVALID;
      result.error_message = "Invalid UPDATE";
    }
    return result;
  }
  if (up.rfind("DELETE", 0) == 0) {
    result.type = QueryType::DELETE;
    result.delete_query = parse_delete();
    if (!result.delete_query) {
      result.type = QueryType::INVALID;
      result.error_message = "Invalid DELETE";
    }
    return result;
  }
  if (up.rfind("DROP TABLE", 0) == 0) {
    result.type = QueryType::DROP_TABLE;
    result.drop_table = parse_drop_table();
    if (!result.drop_table) {
      result.type = QueryType::INVALID;
      result.error_message = "Invalid DROP TABLE";
    }
    return result;
  }
  if (up == "BEGIN" || up == "BEGIN TRANSACTION") {
    result.type = QueryType::BEGIN;
    return result;
  }
  if (up == "COMMIT" || up == "COMMIT TRANSACTION") {
    result.type = QueryType::COMMIT;
    return result;
  }
  if (up == "ROLLBACK" || up == "ROLLBACK TRANSACTION") {
    result.type = QueryType::ROLLBACK;
    return result;
  }
  result.type = QueryType::INVALID;
  result.error_message = "Unsupported SQL";
  return result;
}

void SQLParser::skip_whitespace() {
  while (pos_ < sql_.size() && std::isspace(sql_[pos_]))
    ++pos_;
}

std::string SQLParser::next_token() {
  skip_whitespace();
  if (pos_ >= sql_.size())
    return "";
  if (std::isalpha(sql_[pos_]) || sql_[pos_] == '_') {
    size_t start = pos_++;
    while (pos_ < sql_.size() && (std::isalnum(sql_[pos_]) || sql_[pos_] == '_'))
      ++pos_;
    return sql_.substr(start, pos_ - start);
  }
  if (std::isdigit(sql_[pos_]) || sql_[pos_] == '-') {
    size_t start = pos_++;
    while (pos_ < sql_.size() && (std::isdigit(sql_[pos_]) || sql_[pos_] == '.'))
      ++pos_;
    return sql_.substr(start, pos_ - start);
  }
  if (sql_[pos_] == '\'' || sql_[pos_] == '"') {
    char q = sql_[pos_++];
    size_t start = pos_;
    while (pos_ < sql_.size() && sql_[pos_] != q)
      ++pos_;
    std::string s = sql_.substr(start, pos_ - start);
    if (pos_ < sql_.size())
      ++pos_;
    return s;
  }
  // single char tokens
  return std::string(1, sql_[pos_++]);
}

std::string SQLParser::peek_token() {
  size_t save = pos_;
  auto t = next_token();
  pos_ = save;
  return t;
}

bool SQLParser::expect_token(const std::string& expected) {
  auto t = next_token();
  return upper(t) == upper(expected);
}

bool SQLParser::is_keyword(const std::string& token) const {
  (void)token;
  return false;
}
bool SQLParser::is_operator(const std::string& token) const {
  (void)token;
  return false;
}

ValueType SQLParser::parse_column_type(const std::string& type_str) {
  auto t = upper(type_str);
  if (t == "INT" || t == "INTEGER")
    return ValueType::INTEGER;
  if (t == "BIGINT")
    return ValueType::BIGINT;
  if (t == "DOUBLE" || t == "REAL" || t == "DECIMAL")
    return ValueType::DOUBLE;
  if (t == "TEXT")
    return ValueType::TEXT;
  if (t.rfind("VARCHAR", 0) == 0)
    return ValueType::VARCHAR;
  if (t == "BOOLEAN" || t == "BOOL")
    return ValueType::BOOLEAN;
  return ValueType::NULL_TYPE;
}

std::unique_ptr<SelectQuery> SQLParser::parse_select() {
  auto q = std::make_unique<SelectQuery>();
  expect_token("SELECT");
  auto tok = peek_token();
  if (tok == "*") {
    next_token();
  } else {
    q->select_list = parse_select_list();
  }
  if (!expect_token("FROM"))
    return nullptr;
  q->table_name = next_token();

  // Optional JOINs
  while (upper(peek_token()) == "JOIN" || upper(peek_token()) == "INNER" ||
         upper(peek_token()) == "LEFT" || upper(peek_token()) == "RIGHT") {
    JoinClause join;
    auto join_type = upper(next_token());
    if (join_type == "LEFT" || join_type == "RIGHT" || join_type == "INNER") {
      if (join_type == "LEFT")
        join.type = JoinType::LEFT;
      else if (join_type == "RIGHT")
        join.type = JoinType::RIGHT;
      else
        join.type = JoinType::INNER;
      expect_token("JOIN");
    } else {
      join.type = JoinType::INNER;
    }
    join.table_name = next_token();
    if (upper(peek_token()) == "ON") {
      next_token();
      join.condition = parse_expression();
    }
    q->joins.push_back(std::move(join));
  }

  // Optional WHERE
  if (upper(peek_token()) == "WHERE") {
    next_token();
    q->where_clause = parse_expression();
  }

  // Optional GROUP BY
  if (upper(peek_token()) == "GROUP") {
    next_token();
    if (upper(next_token()) == "BY") {
      while (true) {
        q->group_by.push_back(next_token());
        if (peek_token() != ",")
          break;
        next_token();
      }
    }
  }

  // Optional HAVING
  if (upper(peek_token()) == "HAVING") {
    next_token();
    q->having_clause = parse_expression();
  }
  // Optional temporal clause: FOR SYSTEM_TIME AS OF TX <n>
  if (upper(peek_token()) == "FOR") {
    next_token();
    if (upper(next_token()) == "SYSTEM_TIME" && upper(next_token()) == "AS" &&
        upper(next_token()) == "OF" && upper(next_token()) == "TX") {
      std::string num = next_token();
      try {
        q->system_time_tx = std::stoi(num);
      } catch (...) {
      }
    }
  }
  // Optional LIMIT n
  auto nxt = upper(peek_token());
  if (nxt == "LIMIT") {
    next_token();
    q->limit = std::stoi(next_token());
  }
  return q;
}

std::unique_ptr<InsertQuery> SQLParser::parse_insert() {
  auto q = std::make_unique<InsertQuery>();
  expect_token("INSERT");
  expect_token("INTO");
  q->table_name = next_token();
  // Optional column list
  if (peek_token() == "(") {
    next_token();
    while (true) {
      q->columns.push_back(next_token());
      auto separator = next_token();
      if (separator == ")") {
        break;
      }
      if (separator != ",") {
        return nullptr;
      }
    }
  }
  auto tok = next_token();
  if (upper(tok) != "VALUES")
    return nullptr;
  if (next_token() != "(")
    return nullptr;

  // Parse multiple value sets
  while (true) {
    std::vector<Value> row;
    while (true) {
      Value literal = parse_literal();
      row.emplace_back(std::move(literal));
      auto separator = next_token();
      if (separator == ")") {
        break;
      }
      if (separator != ",") {
        return nullptr;
      }
    }
    q->values.push_back(std::move(row));

    // Check if there are more value sets
    if (peek_token() == ",") {
      next_token(); // consume ","
      if (next_token() != "(") {
        return nullptr;
      }
    } else {
      break; // No more value sets
    }
  }
  return q;
}

std::unique_ptr<UpdateQuery> SQLParser::parse_update() {
  auto q = std::make_unique<UpdateQuery>();
  expect_token("UPDATE");
  q->table_name = next_token();
  if (!expect_token("SET"))
    return nullptr;
  // parse assignments col=literal [, col=literal]*
  while (true) {
    std::string col = next_token();
    if (next_token() != "=")
      return nullptr;
    Value v = parse_literal();
    q->assignments.emplace_back(col, v);
    auto pk = peek_token();
    if (pk == ",") {
      next_token();
      continue;
    }
    break;
  }
  if (upper(peek_token()) == "WHERE") {
    next_token();
    q->where_clause = parse_expression();
  }
  return q;
}
std::unique_ptr<DeleteQuery> SQLParser::parse_delete() {
  auto q = std::make_unique<DeleteQuery>();
  if (!expect_token("DELETE"))
    return nullptr;
  if (!expect_token("FROM"))
    return nullptr;
  q->table_name = next_token();
  if (q->table_name.empty())
    return nullptr;
  if (upper(peek_token()) == "WHERE") {
    next_token();
    q->where_clause = parse_expression();
    // Allow DELETE even if WHERE clause parsing fails
  }
  return q;
}

std::unique_ptr<DropTableQuery> SQLParser::parse_drop_table() {
  auto q = std::make_unique<DropTableQuery>();
  if (!expect_token("DROP"))
    return nullptr;
  if (!expect_token("TABLE"))
    return nullptr;

  // Optional IF EXISTS
  if (upper(peek_token()) == "IF") {
    next_token(); // consume IF
    if (upper(next_token()) == "EXISTS") {
      q->if_exists = true;
    } else {
      return nullptr; // Invalid syntax
    }
  }

  q->table_name = next_token();
  if (q->table_name.empty())
    return nullptr;

  return q;
}

std::unique_ptr<CreateTableQuery> SQLParser::parse_create_table() {
  auto q = std::make_unique<CreateTableQuery>();
  expect_token("CREATE");
  expect_token("TABLE");
  q->table_name = next_token();
  if (next_token() != "(")
    return nullptr;
  while (true) {
    auto colname = next_token();
    auto typestr = next_token();
    ColumnDefinition col;
    col.name = colname;
    col.type = parse_column_type(typestr);
    // parse optional constraints until ',' or ')'
    while (true) {
      auto pk = peek_token();
      auto up = upper(pk);
      if (pk == "," || pk == ")")
        break;
      if (up == "PRIMARY") {
        next_token();
        expect_token("KEY");
        col.primary_key = true;
        col.nullable = false;
        continue;
      }
      if (up == "NOT") {
        next_token();
        if (upper(next_token()) == "NULL")
          col.nullable = false;
        continue;
      }
      if (up == "UNIQUE") {
        next_token();
        col.unique = true;
        continue;
      }
      // unrecognized token, consume to avoid infinite loop
      next_token();
    }
    q->columns.push_back(col);
    auto sep = next_token();
    if (sep == ")")
      break;
    if (sep != ",")
      return nullptr;
  }
  return q;
}

std::unique_ptr<CreateIndexQuery> SQLParser::parse_create_index() {
  return nullptr;
}
std::unique_ptr<DropIndexQuery> SQLParser::parse_drop_index() {
  return nullptr;
}

std::unique_ptr<Expression> SQLParser::parse_expression() {
  return parse_comparison();
}
std::unique_ptr<Expression> SQLParser::parse_and_or() {
  return parse_comparison();
}
std::unique_ptr<Expression> SQLParser::parse_comparison() {
  // support: <col> = <literal>
  auto lhs = next_token();
  if (lhs.empty())
    return nullptr;
  std::string op = peek_token();
  if (op != "=")
    return nullptr;
  next_token(); // consume '='
  Value lit = parse_literal();
  auto col_expr = Expression::make_column_ref(lhs);
  std::vector<std::unique_ptr<Expression>> children;
  children.push_back(std::move(col_expr));
  children.push_back(Expression::make_constant(lit));
  return Expression::make_operator(OperatorType::EQUAL, std::move(children));
}
std::unique_ptr<Expression> SQLParser::parse_term() {
  return parse_comparison();
}
std::unique_ptr<Expression> SQLParser::parse_factor() {
  return parse_comparison();
}
std::unique_ptr<Expression> SQLParser::parse_primary() {
  return parse_comparison();
}

std::vector<std::unique_ptr<Expression>> SQLParser::parse_select_list() {
  std::vector<std::unique_ptr<Expression>> list;
  while (true) {
    auto tok = peek_token();
    auto tok_upper = upper(tok);

    // Check for aggregate functions
    if (tok_upper == "COUNT" || tok_upper == "SUM" || tok_upper == "AVG" || tok_upper == "MIN" ||
        tok_upper == "MAX") {
      next_token(); // consume aggregate name
      AggregateType agg_type;
      if (tok_upper == "COUNT")
        agg_type = AggregateType::COUNT;
      else if (tok_upper == "SUM")
        agg_type = AggregateType::SUM;
      else if (tok_upper == "AVG")
        agg_type = AggregateType::AVG;
      else if (tok_upper == "MIN")
        agg_type = AggregateType::MIN;
      else
        agg_type = AggregateType::MAX;

      if (next_token() == "(") {
        auto arg_tok = peek_token();
        if (arg_tok == "*") {
          next_token();
          list.push_back(Expression::make_aggregate(agg_type));
        } else {
          auto col = next_token();
          list.push_back(Expression::make_aggregate(agg_type, Expression::make_column_ref(col)));
        }
        expect_token(")");
      }
    } else if (tok == "*") {
      next_token();
      auto expr = std::make_unique<Expression>(ExpressionType::STAR);
      list.push_back(std::move(expr));
    } else {
      auto col = next_token();
      // Allow quoted identifiers
      if (!col.empty() && (col.front() == '"' || col.front() == '`'))
        col = col.substr(1, col.size() - 2);

      // Check for table.column notation
      if (peek_token() == ".") {
        next_token();
        auto actual_col = next_token();
        auto expr = Expression::make_column_ref(actual_col);
        expr->table_alias = col;
        list.push_back(std::move(expr));
      } else {
        list.push_back(Expression::make_column_ref(col));
      }
    }

    auto sep = peek_token();
    if (sep == ",") {
      next_token();
      continue;
    }
    break;
  }
  return list;
}
std::unique_ptr<Expression> SQLParser::parse_where_clause() {
  return parse_expression();
}
std::vector<OrderByClause> SQLParser::parse_order_by() {
  return {};
}
Value SQLParser::parse_literal() {
  auto tok = next_token();
  if (tok.empty())
    return Value();
  if ((tok.front() == '\'' && tok.back() == '\'') || (tok.front() == '"' && tok.back() == '"')) {
    return Value(tok.substr(1, tok.size() - 2));
  }
  // numeric or bare identifier treated as string
  bool is_num = (std::isdigit(tok[0]) || tok[0] == '-');
  if (is_num) {
    if (tok.find('.') != std::string::npos) {
      return Value(std::stod(tok));
    }
    return Value(static_cast<int64_t>(std::stoll(tok)));
  }
  return Value(tok);
}
ColumnDefinition SQLParser::parse_column_definition() {
  return ColumnDefinition();
}
void SQLParser::set_error(const std::string& /*message*/) {}
bool SQLParser::match_keyword(const std::string& kw) {
  return upper(peek_token()) == upper(kw);
}

} // namespace latticedb
