#pragma once
#include <thread>
#include <shared_mutex>
#include <mutex>

#include "../transaction/transaction.h"
#include "../types/value.h"
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace latticedb {

enum class PermissionType {
  SELECT,
  INSERT,
  UPDATE,
  DELETE,
  CREATE_TABLE,
  DROP_TABLE,
  CREATE_INDEX,
  DROP_INDEX,
  ALTER_TABLE,
  CREATE_USER,
  DROP_USER,
  GRANT,
  REVOKE,
  ADMIN
};

enum class AuthenticationMethod { PASSWORD, CERTIFICATE, LDAP, KERBEROS, JWT_TOKEN, API_KEY };

struct User {
  std::string username;
  std::string password_hash; // bcrypt hash
  std::string salt;
  AuthenticationMethod auth_method;
  std::unordered_set<std::string> roles;
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point last_login;
  bool is_active = true;
  uint32_t failed_login_attempts = 0;
  std::chrono::system_clock::time_point locked_until;

  // Additional metadata
  std::string email;
  std::unordered_map<std::string, std::string> attributes;

  User(const std::string& name)
      : username(name), auth_method(AuthenticationMethod::PASSWORD),
        created_at(std::chrono::system_clock::now()) {}
};

struct Role {
  std::string name;
  std::string description;
  std::unordered_set<PermissionType> permissions;
  std::unordered_map<std::string, std::unordered_set<PermissionType>> table_permissions;
  std::chrono::system_clock::time_point created_at;

  Role(const std::string& role_name, const std::string& desc = "")
      : name(role_name), description(desc), created_at(std::chrono::system_clock::now()) {}
};

struct SessionInfo {
  std::string session_id;
  std::string username;
  std::string client_ip;
  std::chrono::system_clock::time_point login_time;
  std::chrono::system_clock::time_point last_activity;
  std::unordered_map<std::string, std::string> session_attributes;
  Transaction* current_transaction = nullptr;

  SessionInfo(const std::string& id, const std::string& user, const std::string& ip)
      : session_id(id), username(user), client_ip(ip), login_time(std::chrono::system_clock::now()),
        last_activity(std::chrono::system_clock::now()) {}
};

// Row-Level Security (RLS) policy
struct RLSPolicy {
  std::string policy_name;
  std::string table_name;
  PermissionType permission_type;
  std::string condition_expression; // SQL WHERE condition
  std::vector<std::string> target_roles;
  bool is_permissive = true; // true for PERMISSIVE, false for RESTRICTIVE
  bool is_enabled = true;

  RLSPolicy(const std::string& name, const std::string& table, PermissionType perm,
            const std::string& condition)
      : policy_name(name), table_name(table), permission_type(perm),
        condition_expression(condition) {}
};

// Column-Level Security (masking/encryption)
struct ColumnSecurityPolicy {
  std::string table_name;
  std::string column_name;
  std::string masking_function; // SQL function for masking
  std::string encryption_key_id;
  std::vector<std::string> authorized_roles;
  bool require_audit_log = false;

  ColumnSecurityPolicy(const std::string& table, const std::string& column)
      : table_name(table), column_name(column) {}
};

class PasswordManager {
private:
  uint32_t salt_length_ = 16;
  uint32_t bcrypt_rounds_ = 12;

public:
  std::string generate_salt();
  std::string hash_password(const std::string& password, const std::string& salt);
  bool verify_password(const std::string& password, const std::string& hash,
                       const std::string& salt);
  bool is_password_strong(const std::string& password);
  std::string generate_secure_token(uint32_t length = 32);

private:
  bool has_mixed_case(const std::string& password);
  bool has_numbers(const std::string& password);
  bool has_special_chars(const std::string& password);
};

class AuditLogger {
public:
  enum class AuditEventType {
    LOGIN_SUCCESS,
    LOGIN_FAILURE,
    LOGOUT,
    QUERY_EXECUTION,
    SCHEMA_CHANGE,
    PERMISSION_CHANGE,
    DATA_ACCESS,
    SECURITY_VIOLATION,
    SYSTEM_EVENT
  };

  struct AuditEvent {
    std::string event_id;
    AuditEventType event_type;
    std::string username;
    std::string session_id;
    std::string client_ip;
    std::string database_name;
    std::string object_name;
    std::string sql_statement;
    std::string result_status;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> additional_info;

    AuditEvent(AuditEventType type, const std::string& user = "")
        : event_type(type), username(user), timestamp(std::chrono::system_clock::now()) {
      event_id = generate_event_id();
    }

  private:
    std::string generate_event_id();
  };

private:
  std::string audit_log_path_;
  mutable std::mutex audit_mutex_;
  bool async_logging_ = true;
  std::queue<AuditEvent> event_queue_;
  std::thread logging_thread_;
  std::atomic<bool> stop_logging_;

public:
  explicit AuditLogger(const std::string& log_path = "latticedb_audit.log");
  ~AuditLogger();

  void log_event(const AuditEvent& event);
  void log_login_attempt(const std::string& username, const std::string& client_ip, bool success);
  void log_query_execution(const std::string& username, const std::string& sql,
                           const std::string& status, double execution_time_ms);
  void log_schema_change(const std::string& username, const std::string& operation,
                         const std::string& object_name);
  void log_security_violation(const std::string& username, const std::string& violation_type,
                              const std::string& details);

  std::vector<AuditEvent>
  query_audit_log(const std::string& username = "",
                  AuditEventType event_type = AuditEventType::QUERY_EXECUTION,
                  std::chrono::hours time_range = std::chrono::hours(24));

  void enable_async_logging(bool enable);
  void flush_logs();

private:
  void logging_thread_function();
  void write_event_to_file(const AuditEvent& event);
};

class EncryptionManager {
public:
  enum class EncryptionAlgorithm { AES_256_GCM, AES_256_CBC, CHACHA20_POLY1305 };

private:
  std::unordered_map<std::string, std::vector<uint8_t>> encryption_keys_;
  EncryptionAlgorithm default_algorithm_ = EncryptionAlgorithm::AES_256_GCM;
  mutable std::mutex keys_mutex_;

public:
  EncryptionManager();

  // Key management
  std::string generate_key(const std::string& key_id,
                           EncryptionAlgorithm algorithm = EncryptionAlgorithm::AES_256_GCM);
  bool load_key(const std::string& key_id, const std::vector<uint8_t>& key_data);
  bool remove_key(const std::string& key_id);
  std::vector<std::string> list_keys() const;

  // Encryption/Decryption
  std::vector<uint8_t> encrypt_data(const std::vector<uint8_t>& plaintext,
                                    const std::string& key_id);
  std::vector<uint8_t> decrypt_data(const std::vector<uint8_t>& ciphertext,
                                    const std::string& key_id);

  // Column-level encryption
  Value encrypt_value(const Value& value, const std::string& key_id);
  Value decrypt_value(const Value& encrypted_value, const std::string& key_id);

  // Key rotation
  void rotate_key(const std::string& key_id);
  void schedule_key_rotation(const std::string& key_id, std::chrono::hours interval);

private:
  std::vector<uint8_t> generate_random_key(size_t key_size);
  std::vector<uint8_t> derive_key_from_password(const std::string& password,
                                                const std::vector<uint8_t>& salt);
};

class SecurityManager {
private:
  std::unordered_map<std::string, std::unique_ptr<User>> users_;
  std::unordered_map<std::string, std::unique_ptr<Role>> roles_;
  std::unordered_map<std::string, std::unique_ptr<SessionInfo>> active_sessions_;
  std::vector<std::unique_ptr<RLSPolicy>> rls_policies_;
  std::vector<std::unique_ptr<ColumnSecurityPolicy>> column_policies_;

  std::unique_ptr<PasswordManager> password_manager_;
  std::unique_ptr<AuditLogger> audit_logger_;
  std::unique_ptr<EncryptionManager> encryption_manager_;

  mutable std::shared_mutex security_mutex_;

  // Security configuration
  struct SecurityConfig {
    bool enforce_password_policy = true;
    uint32_t session_timeout_minutes = 60;
    uint32_t max_failed_login_attempts = 5;
    uint32_t account_lockout_minutes = 30;
    bool require_ssl = false;
    bool enable_audit_logging = true;
    bool enable_row_level_security = true;
  } config_;

public:
  SecurityManager();
  ~SecurityManager() = default;

  // User management
  bool create_user(const std::string& username, const std::string& password,
                   AuthenticationMethod auth_method = AuthenticationMethod::PASSWORD);
  bool delete_user(const std::string& username);
  bool change_user_password(const std::string& username, const std::string& old_password,
                            const std::string& new_password);
  bool is_user_active(const std::string& username);
  bool lock_user_account(const std::string& username, std::chrono::minutes duration);

  // Role management
  bool create_role(const std::string& role_name, const std::string& description = "");
  bool delete_role(const std::string& role_name);
  bool grant_role_to_user(const std::string& username, const std::string& role_name);
  bool revoke_role_from_user(const std::string& username, const std::string& role_name);

  // Permission management
  bool grant_permission(const std::string& role_name, PermissionType permission,
                        const std::string& table_name = "");
  bool revoke_permission(const std::string& role_name, PermissionType permission,
                         const std::string& table_name = "");
  bool check_permission(const std::string& username, PermissionType permission,
                        const std::string& table_name = "");

  // Authentication and session management
  std::string authenticate_user(const std::string& username, const std::string& password,
                                const std::string& client_ip);
  bool validate_session(const std::string& session_id);
  void logout_user(const std::string& session_id);
  void cleanup_expired_sessions();

  // Row-Level Security (RLS)
  bool create_rls_policy(const std::string& policy_name, const std::string& table_name,
                         PermissionType permission, const std::string& condition);
  bool drop_rls_policy(const std::string& policy_name);
  bool enable_rls_for_table(const std::string& table_name);
  bool disable_rls_for_table(const std::string& table_name);
  std::string build_rls_condition(const std::string& username, const std::string& table_name,
                                  PermissionType permission);

  // Column-Level Security
  bool create_column_policy(const std::string& table_name, const std::string& column_name,
                            const std::string& masking_function = "",
                            const std::string& encryption_key = "");
  bool authorize_column_access(const std::string& username, const std::string& table_name,
                               const std::string& column_name);
  Value apply_column_security(const Value& value, const std::string& username,
                              const std::string& table_name, const std::string& column_name);

  // Security configuration
  void set_security_config(const SecurityConfig& config);
  SecurityConfig get_security_config() const;

  // Information functions
  std::vector<std::string> list_users() const;
  std::vector<std::string> list_roles() const;
  std::vector<std::string> get_user_roles(const std::string& username) const;
  std::vector<PermissionType> get_user_permissions(const std::string& username,
                                                   const std::string& table_name = "") const;

  // Utility functions
  SessionInfo* get_session_info(const std::string& session_id);
  bool is_admin_user(const std::string& username);
  void change_session_attribute(const std::string& session_id, const std::string& key,
                                const std::string& value);

  // Audit and monitoring
  AuditLogger* get_audit_logger() {
    return audit_logger_.get();
  }
  EncryptionManager* get_encryption_manager() {
    return encryption_manager_.get();
  }

private:
  void initialize_default_roles();
  bool validate_password_policy(const std::string& password);
  void handle_failed_login(const std::string& username);
  std::string generate_session_id();
  bool is_session_expired(const SessionInfo& session);
  void apply_account_lockout(User& user);
};

} // namespace latticedb