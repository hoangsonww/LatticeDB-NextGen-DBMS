#pragma once

#include <regex>
#include <string>
#include <unordered_set>
#include <vector>

namespace latticedb {

class SQLSanitizer {
private:
  // Dangerous SQL patterns
  static const std::vector<std::regex> dangerous_patterns_;
  static const std::unordered_set<std::string> blocked_keywords_;
  static const std::regex comment_pattern_;
  static const std::regex string_escape_pattern_;

public:
  // Main sanitization function
  static bool sanitize_query(std::string& query);

  // Check for SQL injection attempts
  static bool detect_injection(const std::string& input);

  // Escape special characters in string literals
  static std::string escape_string(const std::string& input);

  // Validate and sanitize identifiers (table/column names)
  static bool validate_identifier(const std::string& identifier);

  // Remove comments from SQL
  static std::string remove_comments(const std::string& sql);

  // Check for multiple statements
  static bool has_multiple_statements(const std::string& sql);

  // Parameterize query - convert to prepared statement format
  static std::pair<std::string, std::vector<std::string>>
  parameterize_query(const std::string& query);

  // Validate query structure
  static bool validate_query_structure(const std::string& query);

  // Check for dangerous functions
  static bool contains_dangerous_functions(const std::string& query);

  // Rate limiting check
  static bool check_query_rate_limit(const std::string& client_id);

private:
  // Helper to check for nested queries beyond allowed depth
  static bool check_nesting_depth(const std::string& query, int max_depth = 3);

  // Helper to validate WHERE clause
  static bool validate_where_clause(const std::string& where_clause);

  // Helper to check for always-true conditions (1=1, 'a'='a', etc)
  static bool has_tautology(const std::string& condition);
};

class PreparedStatement {
private:
  std::string query_template_;
  std::vector<std::string> parameters_;
  std::vector<ValueType> parameter_types_;
  bool compiled_;

public:
  PreparedStatement(const std::string& query);

  // Set parameter value
  void set_parameter(int index, const Value& value);

  // Clear all parameters
  void clear_parameters();

  // Execute with current parameters
  std::string get_final_query() const;

  // Validate parameters
  bool validate_parameters() const;

  // Compile and cache execution plan
  bool compile();

private:
  // Replace placeholders with actual values
  std::string substitute_parameters() const;

  // Validate parameter types
  bool check_parameter_types() const;
};

class QueryValidator {
private:
  static const size_t MAX_QUERY_LENGTH = 10000;
  static const size_t MAX_IDENTIFIER_LENGTH = 128;
  static const size_t MAX_IN_CLAUSE_SIZE = 1000;
  static const size_t MAX_JOIN_TABLES = 10;

public:
  // Comprehensive query validation
  static bool validate_query(const std::string& query);

  // Check query complexity
  static bool check_complexity(const std::string& query);

  // Validate SELECT query
  static bool validate_select(const std::string& query);

  // Validate INSERT query
  static bool validate_insert(const std::string& query);

  // Validate UPDATE query
  static bool validate_update(const std::string& query);

  // Validate DELETE query
  static bool validate_delete(const std::string& query);

  // Check for resource limits
  static bool check_resource_limits(const std::string& query);

  // Validate privilege requirements
  static bool check_privileges(const std::string& query, const std::string& user);

private:
  // Count number of tables in query
  static int count_tables(const std::string& query);

  // Count number of joins
  static int count_joins(const std::string& query);

  // Check IN clause size
  static bool check_in_clause_size(const std::string& query);

  // Validate column list
  static bool validate_columns(const std::string& columns);
};

// SQL injection detection patterns
class InjectionDetector {
private:
  struct Pattern {
    std::regex regex;
    std::string description;
    int severity; // 1-10, 10 being most severe
  };

  static std::vector<Pattern> injection_patterns_;

public:
  struct DetectionResult {
    bool is_malicious;
    int severity;
    std::string reason;
    std::vector<std::string> matched_patterns;
  };

  // Analyze query for injection attempts
  static DetectionResult analyze(const std::string& query);

  // Check for encoded attacks (hex, unicode, etc)
  static bool check_encoding_attacks(const std::string& input);

  // Check for time-based attacks
  static bool check_time_attacks(const std::string& input);

  // Check for union-based attacks
  static bool check_union_attacks(const std::string& input);

  // Check for boolean-based blind injection
  static bool check_boolean_attacks(const std::string& input);

  // Machine learning-based detection (simplified)
  static double calculate_anomaly_score(const std::string& query);

private:
  // Initialize patterns
  static void initialize_patterns();

  // Calculate entropy of input
  static double calculate_entropy(const std::string& input);

  // Check for suspicious character frequency
  static bool check_char_frequency(const std::string& input);
};

} // namespace latticedb