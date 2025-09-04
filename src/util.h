#pragma once
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace util {

inline std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e-b+1);
}

inline std::vector<std::string> split(const std::string& s, char delim, bool keep_empty=false) {
    std::vector<std::string> out;
    std::string cur; std::istringstream ss(s);
    while (std::getline(ss, cur, delim)) {
        if (keep_empty || !cur.empty()) out.push_back(cur);
    }
    return out;
}

inline std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){return std::toupper(c);});
    return s;
}

inline std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){return std::tolower(c);});
    return s;
}

inline std::string now_iso() {
    using namespace std::chrono;
    auto tp = system_clock::now();
    auto t = system_clock::to_time_t(tp);
    auto tm = *std::gmtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

inline bool starts_with(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && std::equal(p.begin(), p.end(), s.begin());
}

inline bool iequals(const std::string& a, const std::string& b) {
    if (a.size()!=b.size()) return false;
    for (size_t i=0;i<a.size();++i) if (std::tolower(a[i])!=std::tolower(b[i])) return false;
    return true;
}

inline std::optional<int64_t> to_i64(const std::string& s) {
    try { size_t idx=0; long long v= std::stoll(trim(s), &idx, 10); if (idx==trim(s).size()) return (int64_t)v; } catch(...) {}
    return std::nullopt;
}

inline std::optional<double> to_f64(const std::string& s) {
    try { size_t idx=0; double v= std::stod(trim(s), &idx); if (idx==trim(s).size()) return v; } catch(...) {}
    return std::nullopt;
}

inline std::string unquote(const std::string& s) {
    auto t = trim(s);
    if (t.size()>=2 && ((t.front()=='\'' && t.back()=='\'') || (t.front()=='\"' && t.back()=='\"')))
        return t.substr(1, t.size()-2);
    return t;
}

inline std::string join(const std::vector<std::string>& v, const std::string& sep) {
    std::ostringstream os;
    for (size_t i=0;i<v.size();++i){ if(i) os<<sep; os<<v[i];}
    return os.str();
}

} // namespace util
