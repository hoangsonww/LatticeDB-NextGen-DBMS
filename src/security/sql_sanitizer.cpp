#include "sql_sanitizer.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <sstream>
#include <unordered_map>

namespace latticedb {

// Static member definitions
const std::vector<std::regex> SQLSanitizer::dangerous_patterns_ = {
    std::regex(R"((\b(DROP|ALTER|CREATE|TRUNCATE)\s+TABLE\b))", std::regex::icase),
    std::regex(R"((\b(EXEC|EXECUTE)\s*\())", std::regex::icase),
    std::regex(R"((;.*--;?))", std::regex::icase),
    std::regex(R"((\bUNION\s+(ALL\s+)?SELECT\b))", std::regex::icase),
    std::regex(R"((\bINTO\s+(OUTFILE|DUMPFILE)\b))", std::regex::icase),
    std::regex(R"((xp_cmdshell))", std::regex::icase),
    std::regex(R"((\bWAITFOR\s+DELAY\b))", std::regex::icase),
    std::regex(R"((BENCHMARK\s*\())", std::regex::icase)};

const std::unordered_set<std::string> SQLSanitizer::blocked_keywords_ = {
    "SHUTDOWN", "KILL", "GRANT", "REVOKE", "FILE", "PROCESS"};

const std::regex SQLSanitizer::comment_pattern_(R"((--|#|/\*|\*/))");
const std::regex SQLSanitizer::string_escape_pattern_(R"((['";\\]))");

bool SQLSanitizer::sanitize_query(std::string& query) {
  // Remove comments
  query = remove_comments(query);

  // Check for injection attempts
  if (detect_injection(query)) {
    return false;
  }

  // Check for multiple statements
  if (has_multiple_statements(query)) {
    return false;
  }

  // Validate structure
  if (!validate_query_structure(query)) {
    return false;
  }

  // Check for dangerous functions
  if (contains_dangerous_functions(query)) {
    return false;
  }

  return true;
}

bool SQLSanitizer::detect_injection(const std::string& input) {
  // Check against dangerous patterns
  for (const auto& pattern : dangerous_patterns_) {
    if (std::regex_search(input, pattern)) {
      return true;
    }
  }

  // Check for blocked keywords
  std::string upper_input = input;
  std::transform(upper_input.begin(), upper_input.end(), upper_input.begin(), ::toupper);

  for (const auto& keyword : blocked_keywords_) {
    if (upper_input.find(keyword) != std::string::npos) {
      return true;
    }
  }

  // Check for tautologies
  if (has_tautology(input)) {
    return true;
  }

  // Check nesting depth
  if (!check_nesting_depth(input)) {
    return false;
  }

  return false;
}

std::string SQLSanitizer::escape_string(const std::string& input) {
  std::string result;
  result.reserve(input.size() * 2);

  for (char c : input) {
    switch (c) {
    case '\'':
      result += "''";
      break;
    case '"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    case '\0':
      result += "\\0";
      break;
    default:
      result += c;
    }
  }

  return result;
}

bool SQLSanitizer::validate_identifier(const std::string& identifier) {
  if (identifier.empty() || identifier.length() > 128) {
    return false;
  }

  // Must start with letter or underscore
  if (!std::isalpha(identifier[0]) && identifier[0] != '_') {
    return false;
  }

  // Rest must be alphanumeric or underscore
  for (size_t i = 1; i < identifier.length(); ++i) {
    if (!std::isalnum(identifier[i]) && identifier[i] != '_') {
      return false;
    }
  }

  return true;
}

std::string SQLSanitizer::remove_comments(const std::string& sql) {
  std::string result;
  bool in_string = false;
  bool in_line_comment = false;
  bool in_block_comment = false;
  char string_delimiter = '\0';

  for (size_t i = 0; i < sql.length(); ++i) {
    if (!in_line_comment && !in_block_comment) {
      // Check for string literals
      if ((sql[i] == '\'' || sql[i] == '"') && (i == 0 || sql[i - 1] != '\\')) {
        if (!in_string) {
          in_string = true;
          string_delimiter = sql[i];
        } else if (sql[i] == string_delimiter) {
          in_string = false;
        }
        result += sql[i];
      }
      // Check for comments
      else if (!in_string) {
        if (i + 1 < sql.length()) {
          if (sql[i] == '-' && sql[i + 1] == '-') {
            in_line_comment = true;
            ++i;
          } else if (sql[i] == '/' && sql[i + 1] == '*') {
            in_block_comment = true;
            ++i;
          } else {
            result += sql[i];
          }
        } else {
          result += sql[i];
        }
      } else {
        result += sql[i];
      }
    }
    // Handle end of comments
    else if (in_line_comment && sql[i] == '\n') {
      in_line_comment = false;
      result += '\n';
    } else if (in_block_comment && i + 1 < sql.length() && sql[i] == '*' && sql[i + 1] == '/') {
      in_block_comment = false;
      ++i;
    }
  }

  return result;
}

bool SQLSanitizer::has_multiple_statements(const std::string& sql) {
  bool in_string = false;
  char string_delimiter = '\0';
  int semicolon_count = 0;

  for (size_t i = 0; i < sql.length(); ++i) {
    if ((sql[i] == '\'' || sql[i] == '"') && (i == 0 || sql[i - 1] != '\\')) {
      if (!in_string) {
        in_string = true;
        string_delimiter = sql[i];
      } else if (sql[i] == string_delimiter) {
        in_string = false;
      }
    } else if (!in_string && sql[i] == ';') {
      semicolon_count++;
      if (semicolon_count > 1) {
        return true;
      }
    }
  }

  return false;
}

std::pair<std::string, std::vector<std::string>>
SQLSanitizer::parameterize_query(const std::string& query) {
  std::string parameterized;
  std::vector<std::string> params;
  bool in_string = false;
  char string_delimiter = '\0';

  for (size_t i = 0; i < query.length(); ++i) {
    if ((query[i] == '\'' || query[i] == '"') && (i == 0 || query[i - 1] != '\\')) {
      if (!in_string) {
        in_string = true;
        string_delimiter = query[i];
        // Start capturing parameter
        std::string param;
        size_t j = i + 1;
        while (j < query.length() && !(query[j] == string_delimiter && query[j - 1] != '\\')) {
          param += query[j++];
        }
        params.push_back(param);
        parameterized += "?";
        i = j;
        in_string = false;
      }
    } else {
      parameterized += query[i];
    }
  }

  return {parameterized, params};
}

bool SQLSanitizer::validate_query_structure(const std::string& query) {
  // Basic structure validation
  if (query.empty() || query.length() > 10000) {
    return false;
  }

  // Check for balanced parentheses
  int paren_count = 0;
  for (char c : query) {
    if (c == '(')
      paren_count++;
    else if (c == ')')
      paren_count--;
    if (paren_count < 0)
      return false;
  }
  if (paren_count != 0)
    return false;

  return true;
}

bool SQLSanitizer::contains_dangerous_functions(const std::string& query) {
  std::vector<std::string> dangerous_funcs = {"LOAD_FILE",    "SLEEP",    "BENCHMARK", "UPDATEXML",
                                              "EXTRACTVALUE", "MAKE_SET", "ELT",       "CHAR",
                                              "ASCII",        "ORD",      "HEX",       "UNHEX"};

  std::string upper_query = query;
  std::transform(upper_query.begin(), upper_query.end(), upper_query.begin(), ::toupper);

  for (const auto& func : dangerous_funcs) {
    if (upper_query.find(func) != std::string::npos) {
      return true;
    }
  }

  return false;
}

bool SQLSanitizer::check_query_rate_limit(const std::string& client_id) {
  static std::unordered_map<std::string, std::vector<std::chrono::steady_clock::time_point>>
      query_times;
  static const int MAX_QUERIES_PER_SECOND = 100;

  auto now = std::chrono::steady_clock::now();
  auto& times = query_times[client_id];

  // Remove old entries
  times.erase(
      std::remove_if(times.begin(), times.end(),
                     [now](const auto& t) {
                       return std::chrono::duration_cast<std::chrono::seconds>(now - t).count() > 1;
                     }),
      times.end());

  if (times.size() >= MAX_QUERIES_PER_SECOND) {
    return false;
  }

  times.push_back(now);
  return true;
}

bool SQLSanitizer::check_nesting_depth(const std::string& query, int max_depth) {
  int depth = 0;
  int max_seen = 0;

  for (char c : query) {
    if (c == '(') {
      depth++;
      max_seen = std::max(max_seen, depth);
    } else if (c == ')') {
      depth--;
    }
  }

  return max_seen <= max_depth;
}

bool SQLSanitizer::has_tautology(const std::string& condition) {
  std::vector<std::regex> tautology_patterns = {
      std::regex(R"(\b1\s*=\s*1\b)", std::regex::icase),
      std::regex(R"(\b'a'\s*=\s*'a'\b)", std::regex::icase),
      std::regex(R"(\b\d+\s*=\s*\d+\b)", std::regex::icase),
      std::regex(R"(\bOR\s+1\s*=\s*1\b)", std::regex::icase),
      std::regex(R"(\bOR\s+'[^']*'\s*=\s*'[^']*'\b)", std::regex::icase)};

  for (const auto& pattern : tautology_patterns) {
    if (std::regex_search(condition, pattern)) {
      return true;
    }
  }

  return false;
}

// PreparedStatement implementation
PreparedStatement::PreparedStatement(const std::string& query)
    : query_template_(query), compiled_(false) {}

void PreparedStatement::set_parameter(int index, const Value& value) {
  if (index >= 0 && index < static_cast<int>(parameters_.size())) {
    parameters_[index] = value.to_string();
    // Store type for validation
    if (index < static_cast<int>(parameter_types_.size())) {
      parameter_types_[index] = value.type();
    }
  }
}

void PreparedStatement::clear_parameters() {
  parameters_.clear();
  parameter_types_.clear();
}

std::string PreparedStatement::get_final_query() const {
  return substitute_parameters();
}

bool PreparedStatement::validate_parameters() const {
  return check_parameter_types();
}

bool PreparedStatement::compile() {
  // Parse and cache execution plan
  compiled_ = true;
  return true;
}

std::string PreparedStatement::substitute_parameters() const {
  std::string result = query_template_;
  size_t param_index = 0;

  size_t pos = 0;
  while ((pos = result.find('?', pos)) != std::string::npos) {
    if (param_index < parameters_.size()) {
      std::string escaped = SQLSanitizer::escape_string(parameters_[param_index]);
      result.replace(pos, 1, "'" + escaped + "'");
      pos += escaped.length() + 2;
      param_index++;
    } else {
      break;
    }
  }

  return result;
}

bool PreparedStatement::check_parameter_types() const {
  // Validate that all parameters have been set
  size_t placeholders = std::count(query_template_.begin(), query_template_.end(), '?');
  return placeholders == parameters_.size();
}

// QueryValidator implementation
bool QueryValidator::validate_query(const std::string& query) {
  if (query.length() > MAX_QUERY_LENGTH) {
    return false;
  }

  if (!check_complexity(query)) {
    return false;
  }

  if (!check_resource_limits(query)) {
    return false;
  }

  return true;
}

bool QueryValidator::check_complexity(const std::string& query) {
  int tables = count_tables(query);
  if (tables > static_cast<int>(MAX_JOIN_TABLES)) {
    return false;
  }

  int joins = count_joins(query);
  if (joins > static_cast<int>(MAX_JOIN_TABLES)) {
    return false;
  }

  if (!check_in_clause_size(query)) {
    return false;
  }

  return true;
}

int QueryValidator::count_tables(const std::string& query) {
  std::regex table_pattern(R"(\bFROM\s+(\w+)|\bJOIN\s+(\w+))", std::regex::icase);
  std::sregex_iterator it(query.begin(), query.end(), table_pattern);
  std::sregex_iterator end;

  return std::distance(it, end);
}

int QueryValidator::count_joins(const std::string& query) {
  std::regex join_pattern(R"(\b(INNER|LEFT|RIGHT|FULL|CROSS)\s+JOIN\b)", std::regex::icase);
  std::sregex_iterator it(query.begin(), query.end(), join_pattern);
  std::sregex_iterator end;

  return std::distance(it, end);
}

bool QueryValidator::check_in_clause_size(const std::string& query) {
  std::regex in_pattern(R"(\bIN\s*\(([^)]+)\))", std::regex::icase);
  std::smatch match;

  if (std::regex_search(query, match, in_pattern)) {
    std::string in_values = match[1];
    int comma_count = std::count(in_values.begin(), in_values.end(), ',');
    if (comma_count + 1 > static_cast<int>(MAX_IN_CLAUSE_SIZE)) {
      return false;
    }
  }

  return true;
}

// InjectionDetector implementation
std::vector<InjectionDetector::Pattern> InjectionDetector::injection_patterns_;

InjectionDetector::DetectionResult InjectionDetector::analyze(const std::string& query) {
  initialize_patterns();
  DetectionResult result{false, 0, "", {}};

  for (const auto& pattern : injection_patterns_) {
    if (std::regex_search(query, pattern.regex)) {
      result.is_malicious = true;
      result.severity = std::max(result.severity, pattern.severity);
      result.matched_patterns.push_back(pattern.description);
    }
  }

  // Check encoding attacks
  if (check_encoding_attacks(query)) {
    result.is_malicious = true;
    result.severity = std::max(result.severity, 8);
    result.matched_patterns.push_back("Encoding attack detected");
  }

  // Check time attacks
  if (check_time_attacks(query)) {
    result.is_malicious = true;
    result.severity = std::max(result.severity, 7);
    result.matched_patterns.push_back("Time-based attack detected");
  }

  // Calculate anomaly score
  double anomaly = calculate_anomaly_score(query);
  if (anomaly > 0.7) {
    result.is_malicious = true;
    result.severity = std::max(result.severity, static_cast<int>(anomaly * 10));
    result.matched_patterns.push_back("Anomalous query structure");
  }

  if (result.is_malicious) {
    result.reason = "SQL injection attempt detected";
  }

  return result;
}

void InjectionDetector::initialize_patterns() {
  if (!injection_patterns_.empty())
    return;

    injection_patterns_ = {
        {std::regex(R"(\bUNION\s+(ALL\s+)?SELECT\b)", std::regex::icase),
         "Union-based injection", 9},
        {std::regex(R"(\bSLEEP\s*\()", std::regex::icase),
         "Time-based injection", 8},
        {std::regex(R"(\bBENCHMARK\s*\()", std::regex::icase),
         "Benchmark injection", 8},
        {std::regex(R"(\bWAITFOR\s+DELAY\b)", std::regex::icase),
         "Delay injection", 8},
        {std::regex(R"(0x[0-9a-fA-F]+)",
         "Hex encoding", 6},
        {std::regex(R"(\bCHAR\s*\([0-9,\s]+\))", std::regex::icase),
         "CHAR encoding", 6},
        {std::regex(R"(\bEXEC\s*\()", std::regex::icase),
         "Command execution", 10},
        {std::regex(R"(xp_cmdshell)", std::regex::icase),
         "XP command shell", 10}
    };
}

bool InjectionDetector::check_encoding_attacks(const std::string& input) {
  // Check for hex encoding
  if (input.find("0x") != std::string::npos) {
    return true;
  }

  // Check for URL encoding
  if (input.find("%") != std::string::npos) {
    std::regex url_pattern(R"(%[0-9a-fA-F]{2})");
    if (std::regex_search(input, url_pattern)) {
      return true;
    }
  }

  // Check for unicode encoding
  if (input.find("\\u") != std::string::npos || input.find("\\x") != std::string::npos) {
    return true;
  }

  return false;
}

bool InjectionDetector::check_time_attacks(const std::string& input) {
  std::vector<std::string> time_functions = {"SLEEP", "BENCHMARK", "WAITFOR", "DELAY", "PG_SLEEP"};

  std::string upper_input = input;
  std::transform(upper_input.begin(), upper_input.end(), upper_input.begin(), ::toupper);

  for (const auto& func : time_functions) {
    if (upper_input.find(func) != std::string::npos) {
      return true;
    }
  }

  return false;
}

double InjectionDetector::calculate_anomaly_score(const std::string& query) {
  double score = 0.0;

  // Check entropy
  double entropy = calculate_entropy(query);
  if (entropy > 4.5) { // High entropy might indicate obfuscation
    score += 0.3;
  }

  // Check character frequency
  if (check_char_frequency(query)) {
    score += 0.3;
  }

  // Check query length vs complexity ratio
  int complexity =
      std::count(query.begin(), query.end(), '(') + std::count(query.begin(), query.end(), ')');
  double ratio = static_cast<double>(complexity) / query.length();
  if (ratio > 0.1) { // Too many parentheses
    score += 0.2;
  }

  // Check for unusual character combinations
  if (query.find("||") != std::string::npos || query.find("--") != std::string::npos ||
      query.find("/*") != std::string::npos) {
    score += 0.2;
  }

  return std::min(score, 1.0);
}

double InjectionDetector::calculate_entropy(const std::string& input) {
  std::unordered_map<char, int> freq;
  for (char c : input) {
    freq[c]++;
  }

  double entropy = 0.0;
  double len = input.length();

  for (const auto& [c, count] : freq) {
    double p = count / len;
    if (p > 0) {
      entropy -= p * std::log2(p);
    }
  }

  return entropy;
}

bool InjectionDetector::check_char_frequency(const std::string& input) {
  int special_chars = 0;
  int alpha_chars = 0;

  for (char c : input) {
    if (std::isalpha(c)) {
      alpha_chars++;
    } else if (!std::isalnum(c) && !std::isspace(c)) {
      special_chars++;
    }
  }

  // Too many special characters relative to alphanumeric
  double ratio = static_cast<double>(special_chars) / (alpha_chars + 1);
  return ratio > 0.5;
}

} // namespace latticedb