#pragma once
#include <string>
#include <set>
#include <optional>
#include <vector>
#include <variant>
#include <unordered_map>
#include <limits>
#include <algorithm>

struct Value {
    using V = std::variant<std::monostate,int64_t,double,std::string,std::set<std::string>,std::vector<double>>;
    V v;

    static Value Null() { return Value{}; }
    static Value I(int64_t x){Value r;r.v=x;return r;}
    static Value D(double x){Value r;r.v=x;return r;}
    static Value S(const std::string& x){Value r;r.v=x;return r;}
    static Value SET(const std::set<std::string>& x){Value r;r.v=x;return r;}
    static Value VEC(const std::vector<double>& x){Value r;r.v=x;return r;}

    bool is_null() const { return std::holds_alternative<std::monostate>(v); }
};

enum class ColumnType { INT, DOUBLE, TEXT, SET_TEXT, VECTOR };
struct MergeSpec {
    enum class Kind { NONE, LWW, SUM_BOUNDED, GSET } kind = Kind::NONE;
    int64_t minv = std::numeric_limits<int64_t>::min();
    int64_t maxv = std::numeric_limits<int64_t>::max();
};

inline Value crdt_merge(const MergeSpec& m, const Value& oldv, const Value& newv) {
    using K = MergeSpec::Kind;
    if (m.kind==K::NONE) return newv.is_null() ? oldv : newv;
    if (m.kind==K::LWW) return newv.is_null()? oldv : newv;
    if (m.kind==K::SUM_BOUNDED) {
        int64_t a = 0;
        if (std::holds_alternative<int64_t>(oldv.v)) a = std::get<int64_t>(oldv.v);
        int64_t inc = 0;
        if (std::holds_alternative<int64_t>(newv.v)) inc = std::get<int64_t>(newv.v);
        __int128 s = (__int128)a + (__int128)inc;
        if (s < m.minv) s = m.minv;
        if (s > m.maxv) s = m.maxv;
        return Value::I((int64_t)s);
    }
    if (m.kind==K::GSET) {
        std::set<std::string> s;
        if (std::holds_alternative<std::set<std::string>>(oldv.v)) s = std::get<std::set<std::string>>(oldv.v);
        if (std::holds_alternative<std::set<std::string>>(newv.v)) {
            const auto& add = std::get<std::set<std::string>>(newv.v);
            s.insert(add.begin(), add.end());
        }
        return Value::SET(s);
    }
    return newv;
}
