#include <shared_mutex>

#include <mutex>

// Implementation for security_manager.h
#include "security_manager.h"
#include <random>
#include <sstream>

namespace latticedb {

// PasswordManager (insecure minimal hashing using std::hash for demo only)
std::string PasswordManager::generate_salt() {
  static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  std::string s;
  s.resize(salt_length_);
  std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<int> dist(0, (int)sizeof(alphanum) - 2);
  for (auto& c : s)
    c = alphanum[dist(rng)];
  return s;
}
std::string PasswordManager::hash_password(const std::string& password, const std::string& salt) {
  return std::to_string(std::hash<std::string>{}(salt + password));
}
bool PasswordManager::verify_password(const std::string& password, const std::string& hash,
                                      const std::string& salt) {
  return hash_password(password, salt) == hash;
}
bool PasswordManager::is_password_strong(const std::string& password) {
  return password.size() >= 8 && has_uppercase(password) && has_lowercase(password) &&
         has_digit(password) && has_special_chars(password);
}
std::string PasswordManager::generate_secure_token(uint32_t length) {
  std::string s;
  s.resize(length);
  std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<int> dist(33, 126);
  for (auto& c : s)
    c = (char)dist(rng);
  return s;
}
bool PasswordManager::has_uppercase(const std::string& p) {
  for (char c : p)
    if (c >= 'A' && c <= 'Z')
      return true;
  return false;
}
bool PasswordManager::has_lowercase(const std::string& p) {
  for (char c : p)
    if (c >= 'a' && c <= 'z')
      return true;
  return false;
}
bool PasswordManager::has_digit(const std::string& p) {
  for (char c : p)
    if (c >= '0' && c <= '9')
      return true;
  return false;
}
bool PasswordManager::has_special_chars(const std::string& p) {
  for (char c : p)
    if (!(c == ' ' || std::isalnum(c)))
      return true;
  return false;
}

AuditLogger::AuditLogger(const std::string& log_path)
    : audit_log_path_(log_path), stop_logging_(false) {}
AuditLogger::~AuditLogger() {
  stop_logging_.store(true);
  if (logging_thread_.joinable())
    logging_thread_.join();
}
void AuditLogger::log_event(const AuditEvent& event) {
  std::lock_guard<std::mutex> l(audit_mutex_);
  event_queue_.push(event);
}
void AuditLogger::log_login_attempt(const std::string& username, const std::string& client_ip,
                                    bool success) {
  AuditEvent e(success ? AuditEventType::LOGIN_SUCCESS : AuditEventType::LOGIN_FAILURE, username);
  e.client_ip = client_ip;
  log_event(e);
}
void AuditLogger::log_query_execution(const std::string& username, const std::string& sql,
                                      const std::string& status, double) {
  AuditEvent e(AuditEventType::QUERY_EXECUTION, username);
  e.sql_statement = sql;
  e.result_status = status;
  log_event(e);
}
void AuditLogger::log_schema_change(const std::string& username, const std::string& operation,
                                    const std::string& object_name) {
  AuditEvent e(AuditEventType::SCHEMA_CHANGE, username);
  e.object_name = object_name;
  e.sql_statement = operation;
  log_event(e);
}
void AuditLogger::log_security_violation(const std::string& username,
                                         const std::string& violation_type,
                                         const std::string& details) {
  AuditEvent e(AuditEventType::SECURITY_VIOLATION, username);
  e.sql_statement = violation_type + ":" + details;
  log_event(e);
}
std::vector<AuditLogger::AuditEvent>
AuditLogger::query_audit_log(const std::string&, AuditEventType, std::chrono::hours) {
  return {};
}
void AuditLogger::enable_async_logging(bool enable) {
  async_logging_ = enable;
}
void AuditLogger::flush_logs() {
  while (!event_queue_.empty()) {
    write_event_to_file(event_queue_.front());
    event_queue_.pop();
  }
}
void AuditLogger::logging_thread_function() {
  while (!stop_logging_.load()) {
    if (!event_queue_.empty()) {
      write_event_to_file(event_queue_.front());
      event_queue_.pop();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}
void AuditLogger::write_event_to_file(const AuditEvent&) {}
std::string AuditLogger::AuditEvent::generate_event_id() {
  return std::to_string(std::hash<std::string>{}(
      std::to_string(std::chrono::system_clock::now().time_since_epoch().count())));
}

EncryptionManager::EncryptionManager() {}
std::string EncryptionManager::generate_key(const std::string& key_id, EncryptionAlgorithm) {
  std::vector<uint8_t> key(32, 0);
  encryption_keys_[key_id] = key;
  return key_id;
}
bool EncryptionManager::load_key(const std::string& key_id, const std::vector<uint8_t>& key_data) {
  encryption_keys_[key_id] = key_data;
  return true;
}
bool EncryptionManager::remove_key(const std::string& key_id) {
  return encryption_keys_.erase(key_id) > 0;
}
std::vector<std::string> EncryptionManager::list_keys() const {
  std::vector<std::string> ids;
  for (auto& kv : encryption_keys_)
    ids.push_back(kv.first);
  return ids;
}
std::vector<uint8_t> EncryptionManager::encrypt_data(const std::vector<uint8_t>& plaintext,
                                                     const std::string& key_id) {
  (void)key_id;
  return plaintext;
}
std::vector<uint8_t> EncryptionManager::decrypt_data(const std::vector<uint8_t>& ciphertext,
                                                     const std::string& key_id) {
  (void)key_id;
  return ciphertext;
}
Value EncryptionManager::encrypt_value(const Value& value, const std::string& key_id) {
  (void)key_id;
  return value;
}
Value EncryptionManager::decrypt_value(const Value& value, const std::string& key_id) {
  (void)key_id;
  return value;
}
void EncryptionManager::rotate_key(const std::string&) {}
void EncryptionManager::schedule_key_rotation(const std::string&, std::chrono::hours) {}
std::vector<uint8_t> EncryptionManager::generate_random_key(size_t key_size) {
  return std::vector<uint8_t>(key_size, 0);
}
std::vector<uint8_t> EncryptionManager::derive_key_from_password(const std::string&,
                                                                 const std::vector<uint8_t>&) {
  return std::vector<uint8_t>(32, 0);
}

SecurityManager::SecurityManager() {
  password_manager_ = std::make_unique<PasswordManager>();
  audit_logger_ = std::make_unique<AuditLogger>();
  encryption_manager_ = std::make_unique<EncryptionManager>();
  initialize_default_roles();
}
bool SecurityManager::create_user(const std::string& username, const std::string& password,
                                  AuthenticationMethod auth_method) {
  std::unique_lock<std::shared_mutex> lk(security_mutex_);
  if (users_.count(username))
    return false;
  auto u = std::make_unique<User>(username);
  u->salt = password_manager_->generate_salt();
  u->password_hash = password_manager_->hash_password(password, u->salt);
  u->auth_method = auth_method;
  users_[username] = std::move(u);
  return true;
}
bool SecurityManager::delete_user(const std::string& username) {
  std::unique_lock<std::shared_mutex> lk(security_mutex_);
  return users_.erase(username) > 0;
}
bool SecurityManager::change_user_password(const std::string& username,
                                           const std::string& old_password,
                                           const std::string& new_password) {
  std::unique_lock<std::shared_mutex> lk(security_mutex_);
  auto it = users_.find(username);
  if (it == users_.end())
    return false;
  if (!password_manager_->verify_password(old_password, it->second->password_hash,
                                          it->second->salt))
    return false;
  it->second->salt = password_manager_->generate_salt();
  it->second->password_hash = password_manager_->hash_password(new_password, it->second->salt);
  return true;
}
bool SecurityManager::is_user_active(const std::string& username) {
  std::shared_lock<std::shared_mutex> lk(security_mutex_);
  auto it = users_.find(username);
  return it != users_.end() && it->second->is_active;
}
bool SecurityManager::lock_user_account(const std::string& username,
                                        std::chrono::minutes duration) {
  std::unique_lock<std::shared_mutex> lk(security_mutex_);
  auto it = users_.find(username);
  if (it == users_.end())
    return false;
  it->second->locked_until = std::chrono::system_clock::now() + duration;
  return true;
}
bool SecurityManager::create_role(const std::string& role_name, const std::string& description) {
  std::unique_lock<std::shared_mutex> lk(security_mutex_);
  if (roles_.count(role_name))
    return false;
  roles_[role_name] = std::make_unique<Role>(role_name, description);
  return true;
}
bool SecurityManager::delete_role(const std::string& role_name) {
  std::unique_lock<std::shared_mutex> lk(security_mutex_);
  return roles_.erase(role_name) > 0;
}
bool SecurityManager::grant_role_to_user(const std::string& username,
                                         const std::string& role_name) {
  std::unique_lock<std::shared_mutex> lk(security_mutex_);
  auto it = users_.find(username);
  auto rt = roles_.find(role_name);
  if (it == users_.end() || rt == roles_.end())
    return false;
  it->second->roles.insert(role_name);
  return true;
}
bool SecurityManager::revoke_role_from_user(const std::string& username,
                                            const std::string& role_name) {
  std::unique_lock<std::shared_mutex> lk(security_mutex_);
  auto it = users_.find(username);
  if (it == users_.end())
    return false;
  it->second->roles.erase(role_name);
  return true;
}
bool SecurityManager::grant_permission(const std::string& role_name, PermissionType permission,
                                       const std::string& table_name) {
  std::unique_lock<std::shared_mutex> lk(security_mutex_);
  auto it = roles_.find(role_name);
  if (it == roles_.end())
    return false;
  if (table_name.empty())
    it->second->permissions.insert(permission);
  else
    it->second->table_permissions[table_name].insert(permission);
  return true;
}
bool SecurityManager::revoke_permission(const std::string& role_name, PermissionType permission,
                                        const std::string& table_name) {
  std::unique_lock<std::shared_mutex> lk(security_mutex_);
  auto it = roles_.find(role_name);
  if (it == roles_.end())
    return false;
  if (table_name.empty())
    it->second->permissions.erase(permission);
  else
    it->second->table_permissions[table_name].erase(permission);
  return true;
}
bool SecurityManager::check_permission(const std::string& username, PermissionType permission,
                                       const std::string& table_name) {
  std::shared_lock<std::shared_mutex> lk(security_mutex_);
  auto it = users_.find(username);
  if (it == users_.end())
    return false;
  for (auto& r : it->second->roles) {
    auto rt = roles_.find(r);
    if (rt != roles_.end()) {
      if (rt->second->permissions.count(permission))
        return true;
      if (!table_name.empty()) {
        auto t = rt->second->table_permissions.find(table_name);
        if (t != rt->second->table_permissions.end() && t->second.count(permission))
          return true;
      }
    }
  }
  return false;
}
std::string SecurityManager::authenticate_user(const std::string& username,
                                               const std::string& password,
                                               const std::string& client_ip) {
  std::unique_lock<std::shared_mutex> lk(security_mutex_);
  auto it = users_.find(username);
  if (it == users_.end())
    return {};
  auto& u = *it->second;
  if (!password_manager_->verify_password(password, u.password_hash, u.salt)) {
    handle_failed_login(username);
    return {};
  }
  std::string sid = generate_session_id();
  active_sessions_[sid] = std::make_unique<SessionInfo>(sid, username, client_ip);
  return sid;
}
bool SecurityManager::validate_session(const std::string& session_id) {
  std::shared_lock<std::shared_mutex> lk(security_mutex_);
  auto it = active_sessions_.find(session_id);
  if (it == active_sessions_.end())
    return false;
  return !is_session_expired(*it->second);
}
void SecurityManager::logout_user(const std::string& session_id) {
  std::unique_lock<std::shared_mutex> lk(security_mutex_);
  active_sessions_.erase(session_id);
}
void SecurityManager::cleanup_expired_sessions() {
  std::unique_lock<std::shared_mutex> lk(security_mutex_);
  for (auto it = active_sessions_.begin(); it != active_sessions_.end();) {
    if (is_session_expired(*it->second))
      it = active_sessions_.erase(it);
    else
      ++it;
  }
}
bool SecurityManager::create_rls_policy(const std::string& policy_name,
                                        const std::string& table_name, PermissionType permission,
                                        const std::string& condition) {
  std::unique_lock<std::shared_mutex> lk(security_mutex_);
  rls_policies_.push_back(
      std::make_unique<RLSPolicy>(policy_name, table_name, permission, condition));
  return true;
}
bool SecurityManager::drop_rls_policy(const std::string& policy_name) {
  std::unique_lock<std::shared_mutex> lk(security_mutex_);
  rls_policies_.erase(std::remove_if(rls_policies_.begin(), rls_policies_.end(),
                                     [&](auto& p) { return p->policy_name == policy_name; }),
                      rls_policies_.end());
  return true;
}
bool SecurityManager::enable_rls_for_table(const std::string&) {
  return true;
}
bool SecurityManager::disable_rls_for_table(const std::string&) {
  return true;
}
std::string SecurityManager::build_rls_condition(const std::string&, const std::string&,
                                                 PermissionType) {
  return "1=1";
}
bool SecurityManager::create_column_policy(const std::string& table_name,
                                           const std::string& column_name, const std::string&,
                                           const std::string&) {
  std::unique_lock<std::shared_mutex> lk(security_mutex_);
  column_policies_.push_back(std::make_unique<ColumnSecurityPolicy>(table_name, column_name));
  return true;
}
bool SecurityManager::authorize_column_access(const std::string&, const std::string&,
                                              const std::string&) {
  return true;
}
Value SecurityManager::apply_column_security(const Value& value, const std::string&,
                                             const std::string&, const std::string&) {
  return value;
}
void SecurityManager::set_security_config(const SecurityConfig& config) {
  config_ = config;
}
SecurityManager::SecurityConfig SecurityManager::get_security_config() const {
  return config_;
}
std::vector<std::string> SecurityManager::list_users() const {
  std::shared_lock<std::shared_mutex> lk(security_mutex_);
  std::vector<std::string> v;
  for (auto& kv : users_)
    v.push_back(kv.first);
  return v;
}
std::vector<std::string> SecurityManager::list_roles() const {
  std::shared_lock<std::shared_mutex> lk(security_mutex_);
  std::vector<std::string> v;
  for (auto& kv : roles_)
    v.push_back(kv.first);
  return v;
}
std::vector<std::string> SecurityManager::get_user_roles(const std::string& username) const {
  std::shared_lock<std::shared_mutex> lk(security_mutex_);
  std::vector<std::string> v;
  auto it = users_.find(username);
  if (it == users_.end())
    return v;
  for (auto& r : it->second->roles)
    v.push_back(r);
  return v;
}
std::vector<PermissionType>
SecurityManager::get_user_permissions(const std::string& username,
                                      const std::string& table_name) const {
  std::shared_lock<std::shared_mutex> lk(security_mutex_);
  std::vector<PermissionType> perms;
  auto it = users_.find(username);
  if (it == users_.end())
    return perms;
  for (auto& r : it->second->roles) {
    auto rt = roles_.find(r);
    if (rt != roles_.end()) {
      perms.insert(perms.end(), rt->second->permissions.begin(), rt->second->permissions.end());
      if (!table_name.empty()) {
        auto tp = rt->second->table_permissions.find(table_name);
        if (tp != rt->second->table_permissions.end())
          perms.insert(perms.end(), tp->second.begin(), tp->second.end());
      }
    }
  }
  return perms;
}
SessionInfo* SecurityManager::get_session_info(const std::string& session_id) {
  std::shared_lock<std::shared_mutex> lk(security_mutex_);
  auto it = active_sessions_.find(session_id);
  if (it == active_sessions_.end())
    return nullptr;
  return it->second.get();
}
bool SecurityManager::is_admin_user(const std::string& username) {
  return check_permission(username, PermissionType::ADMIN, "");
}
void SecurityManager::change_session_attribute(const std::string& session_id,
                                               const std::string& key, const std::string& value) {
  std::unique_lock<std::shared_mutex> lk(security_mutex_);
  auto it = active_sessions_.find(session_id);
  if (it == active_sessions_.end())
    return;
  it->second->session_attributes[key] = value;
}
void SecurityManager::initialize_default_roles() {
  roles_["public"] = std::make_unique<Role>("public", "Default role");
  roles_["admin"] = std::make_unique<Role>("admin", "Administrator");
  roles_["admin"]->permissions.insert(PermissionType::ADMIN);
}
bool SecurityManager::validate_password_policy(const std::string& password) {
  return password_manager_->is_password_strong(password);
}
void SecurityManager::handle_failed_login(const std::string& username) {
  auto it = users_.find(username);
  if (it == users_.end())
    return;
  it->second->failed_login_attempts++;
  if (it->second->failed_login_attempts > config_.max_failed_login_attempts)
    apply_account_lockout(*it->second);
}
std::string SecurityManager::generate_session_id() {
  return password_manager_->generate_secure_token(32);
}
bool SecurityManager::is_session_expired(const SessionInfo& session) {
  auto now = std::chrono::system_clock::now();
  return (now - session.last_activity) > std::chrono::minutes(config_.session_timeout_minutes);
}
void SecurityManager::apply_account_lockout(User& user) {
  user.locked_until =
      std::chrono::system_clock::now() + std::chrono::minutes(config_.account_lockout_minutes);
}

} // namespace latticedb
