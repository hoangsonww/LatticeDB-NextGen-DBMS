#include "sql_parser.h"
#include "util.h"
#include <cctype>
#include <iostream>

static Value parse_value(const std::string& tok) {
    auto t = util::trim(tok);
    if (util::upper(t)=="NULL") return Value::Null();
    if (t.size() && (std::isdigit((unsigned char)t[0]) || t[0]=='-' || t[0]=='+')) {
        if (auto i = util::to_i64(t)) return Value::I(*i);
        if (auto f = util::to_f64(t)) return Value::D(*f);
    }
    if (t.size() && (t.front()=='\'' || t.front()=='\"')) return Value::S(util::unquote(t));
    return Value::S(t);
}

static std::set<std::string> parse_set(const std::string& s) {
    auto t = util::trim(s);
    if (t.size()<2) return {};
    size_t l = t.find('{'), r = t.rfind('}');
    if (l==std::string::npos || r==std::string::npos || r<=l) return {};
    auto inner = t.substr(l+1, r-l-1);
    std::set<std::string> out;
    for (auto& a : util::split(inner, ',')) {
        out.insert(util::unquote(util::trim(a)));
    }
    return out;
}

static std::vector<double> parse_vector(const std::string& s) {
    auto t = util::trim(s);
    if (t.size()<2) return {};
    size_t l = t.find('['), r = t.rfind(']');
    if (l==std::string::npos || r==std::string::npos || r<=l) return {};
    auto inner = t.substr(l+1, r-l-1);
    std::vector<double> out;
    for (auto& a : util::split(inner, ',')) {
        if (auto f = util::to_f64(util::trim(a))) out.push_back(*f);
    }
    return out;
}

static std::vector<std::string> split_tuple_vals(const std::string& tuple) {
    std::vector<std::string> vals; std::string cur; int depth=0;
    for (char ch : tuple) {
        if (ch==',' && depth==0) { vals.push_back(util::trim(cur)); cur.clear(); continue; }
        if (ch=='{' || ch=='[') depth++;
        else if (ch=='}' || ch==']') depth--;
        cur.push_back(ch);
    }
    if (!cur.empty()) vals.push_back(util::trim(cur));
    return vals;
}

static MergeSpec parse_merge(const std::string& s) {
    MergeSpec m;
    auto u = util::upper(util::trim(s));
    if (u=="MERGE LWW") { m.kind = MergeSpec::Kind::LWW; return m; }
    if (u.find("MERGE SUM_BOUNDED")!=std::string::npos) {
        m.kind = MergeSpec::Kind::SUM_BOUNDED;
        size_t l=u.find('('), r=u.find(')');
        if (l!=std::string::npos && r!=std::string::npos && r>l) {
            auto inner = u.substr(l+1, r-l-1);
            auto parts = util::split(inner, ',');
            if (parts.size()==2) { m.minv = std::stoll(parts[0]); m.maxv = std::stoll(parts[1]); }
        }
        return m;
    }
    if (u.find("MERGE GSET")!=std::string::npos) { m.kind = MergeSpec::Kind::GSET; return m; }
    m.kind = MergeSpec::Kind::NONE; return m;
}

static bool read_quoted_path(const std::string& src, std::string* out_path) {
    size_t q = src.find_first_of("'\"");
    if (q==std::string::npos) return false;
    *out_path = util::unquote(src.substr(q));
    return true;
}

static SelectItem parse_select_item(const std::string& t) {
    auto trimmed = util::trim(t);
    auto U = util::upper(trimmed);
    if (U=="*") return {SelectItem::Kind::STAR, "*"};
    if (U=="DP_COUNT(*)") return {SelectItem::Kind::DP_COUNT, "*"};
    if (U=="COUNT(*)") return {SelectItem::Kind::AGG_COUNT, "*"};
    if (util::starts_with(U, "SUM(") && U.back()==')') return {SelectItem::Kind::AGG_SUM, util::trim(trimmed.substr(4, trimmed.size()-5))};
    if (util::starts_with(U, "AVG(") && U.back()==')') return {SelectItem::Kind::AGG_AVG, util::trim(trimmed.substr(4, trimmed.size()-5))};
    if (util::starts_with(U, "MIN(") && U.back()==')') return {SelectItem::Kind::AGG_MIN, util::trim(trimmed.substr(4, trimmed.size()-5))};
    if (util::starts_with(U, "MAX(") && U.back()==')') return {SelectItem::Kind::AGG_MAX, util::trim(trimmed.substr(4, trimmed.size()-5))};
    return {SelectItem::Kind::COLUMN, trimmed};
}

ParsedSQL parse_sql(const std::string& ssql) {
    ParsedSQL out;
    auto sql = util::trim(ssql);
    if (sql.empty()) return out;
    auto U = util::upper(sql);

    if (U=="EXIT" || U=="QUIT") { out.kind=ParsedSQL::Kind::EXIT; return out; }
    if (U=="BEGIN" || U=="BEGIN TRANSACTION") { out.kind=ParsedSQL::Kind::BEGIN_; return out; }
    if (U=="COMMIT" || U=="END") { out.kind=ParsedSQL::Kind::COMMIT_; return out; }
    if (U=="ROLLBACK") { out.kind=ParsedSQL::Kind::ROLLBACK_; return out; }

    if (util::starts_with(U, "SAVE DATABASE")) {
        size_t p = sql.find("DATABASE");
        auto rest = util::trim(sql.substr(p+8));
        if (!read_quoted_path(rest, &out.saveq.path)) { out.error="SAVE DATABASE expects quoted path"; return out; }
        out.kind = ParsedSQL::Kind::SAVE; return out;
    }
    if (util::starts_with(U, "LOAD DATABASE")) {
        size_t p = sql.find("DATABASE");
        auto rest = util::trim(sql.substr(p+8));
        if (!read_quoted_path(rest, &out.loadq.path)) { out.error="LOAD DATABASE expects quoted path"; return out; }
        out.kind = ParsedSQL::Kind::LOAD; return out;
    }

    if (util::starts_with(U, "SET ")) {
        auto rest = util::trim(sql.substr(3));
        auto eq = rest.find('=');
        if (eq==std::string::npos) { out.error="SET expects k = v"; return out; }
        out.setq.key = util::upper(util::trim(rest.substr(0,eq)));
        auto v = util::trim(rest.substr(eq+1));
        if (auto f = util::to_f64(v)) out.setq.value=*f; else out.setq.value=1.0;
        out.kind = ParsedSQL::Kind::SET; return out;
    }

    if (util::starts_with(U, "DROP TABLE")) {
        out.kind = ParsedSQL::Kind::DROP;
        out.drop.table = util::trim(sql.substr(10));
        return out;
    }

    if (util::starts_with(U, "CREATE TABLE")) {
        out.kind = ParsedSQL::Kind::CREATE;
        size_t l = U.find("CREATE TABLE")+12;
        auto rest = util::trim(sql.substr(l));
        size_t p = rest.find('(');
        if (p==std::string::npos) { out.error="Missing column list"; return out; }
        out.create.table = util::trim(rest.substr(0,p));
        size_t r = rest.rfind(')');
        if (r==std::string::npos || r<=p) { out.error="Unclosed column list"; return out; }
        auto inner = rest.substr(p+1, r-p-1);
        auto coldefs = util::split(inner, ',');
        for (auto cd : coldefs) {
            cd = util::trim(cd);
            if (cd.empty()) continue;
            CreateTableQuery::Col col;
            auto toks = util::split(cd, ' ');
            if (toks.size()<2) { out.error="Invalid column def: "+cd; return out; }
            col.name = util::trim(toks[0]);
            auto T = util::upper(toks[1]);
            if (util::starts_with(T, "VECTOR<")) {
                size_t a = T.find('<'), b = T.find('>');
                size_t dim = std::stoul(T.substr(a+1, b-a-1));
                col.type = ColumnType::VECTOR; col.vec_dim = dim;
            } else if (T=="TEXT") col.type = ColumnType::TEXT;
            else if (T=="INT"||T=="INTEGER") col.type = ColumnType::INT;
            else if (T=="DOUBLE"||T=="FLOAT") col.type = ColumnType::DOUBLE;
            else if (T=="SET<TEXT>") col.type = ColumnType::SET_TEXT;
            else { out.error="Unknown type: "+toks[1]; return out; }
            for (size_t i=2;i<toks.size();++i) {
                auto tk = util::upper(toks[i]);
                if (tk=="PRIMARY") { if (i+1<toks.size() && util::upper(toks[i+1])=="KEY") { col.pk=true; ++i; } }
                else if (tk=="MERGE") {
                    auto pos = cd.find("MERGE");
                    col.merge = parse_merge(cd.substr(pos));
                    break;
                }
            }
            out.create.cols.push_back(col);
        }
        out.create.mergeable = true;
        return out;
    }

    if (util::starts_with(U, "INSERT INTO")) {
        out.kind = ParsedSQL::Kind::INSERT;
        size_t l = U.find("INSERT INTO")+11;
        auto rest = util::trim(sql.substr(l));
        size_t p = rest.find('(');
        out.insert.table = util::trim(rest.substr(0, p));
        size_t r = rest.find(')', p);
        auto cols_s = rest.substr(p+1, r-p-1);
        for (auto c : util::split(cols_s, ',')) out.insert.cols.push_back(util::trim(c));
        auto vals_pos = util::upper(rest).find("VALUES", r);
        auto after = util::trim(rest.substr(vals_pos+6));
        size_t i=0;
        while (i<after.size()) {
            if (after[i]=='(') {
                size_t j=i+1, depth=1;
                for (; j<after.size(); ++j) {
                    if (after[j]=='(') depth++;
                    else if (after[j]==')') { depth--; if (depth==0) break; }
                }
                auto tuple = after.substr(i+1, j-i-1);
                std::vector<Value> row;
                for (auto val : split_tuple_vals(tuple)) {
                    auto s = util::trim(val);
                    if (!s.empty() && s.front()=='{') row.push_back(Value::SET(parse_set(s)));
                    else if (!s.empty() && s.front()=='[') row.push_back(Value::VEC(parse_vector(s)));
                    else row.push_back(parse_value(s));
                }
                out.insert.rows.push_back(std::move(row));
                i = j+1;
                while (i<after.size() && (after[i]==',' || std::isspace((unsigned char)after[i]))) ++i;
            } else break;
        }
        auto tail = util::upper(util::trim(after.substr(i)));
        if (tail.find("ON CONFLICT MERGE")!=std::string::npos) out.insert.on_conflict_merge = true;
        return out;
    }

    if (util::starts_with(U, "UPDATE ")) {
        out.kind = ParsedSQL::Kind::UPDATE;
        auto rest = util::trim(sql.substr(6));
        size_t p = util::upper(rest).find(" SET ");
        out.update.table = util::trim(rest.substr(0,p));
        auto after = rest.substr(p+5);
        size_t wherep = util::upper(after).find(" WHERE ");
        std::string setpart = wherep==std::string::npos ? after : after.substr(0, wherep);
        for (auto asgn : util::split(setpart, ',')) {
            auto eq = asgn.find('=');
            if (eq==std::string::npos) continue;
            auto key = util::trim(asgn.substr(0,eq));
            auto val = util::trim(asgn.substr(eq+1));
            if (!val.empty() && val.front()=='{') out.update.sets[key] = Value::SET(parse_set(val));
            else if (!val.empty() && val.front()=='[') out.update.sets[key] = Value::VEC(parse_vector(val));
            else out.update.sets[key] = parse_value(val);
        }
        if (wherep!=std::string::npos) {
            auto conds = util::trim(after.substr(wherep+7));
            for (auto part : util::split(conds, 'A')) {
                auto c = util::trim(part);
                if (c.empty()) continue;
                Condition cond;
                auto Uc = util::upper(c);
                if (Uc.find("DISTANCE(")==0) {
                    auto inner = c.substr(9);
                    auto rp = inner.find(')');
                    auto inside = inner.substr(0, rp);
                    auto vecparts = util::split(inside, ',');
                    cond.is_distance = true;
                    cond.vec_col = util::trim(vecparts[0]);
                    cond.vec_value = parse_vector(vecparts[1]);
                    auto afterd = util::trim(inner.substr(rp+1));
                    auto ltpos = afterd.find('<');
                    cond.dist_threshold = std::stod(util::trim(afterd.substr(ltpos+1)));
                } else if (Uc.find(" IS NULL")!=std::string::npos) {
                    cond.op = Condition::Op::IS_NULL;
                    cond.column = util::trim(c.substr(0, Uc.find(" IS NULL")));
                } else if (Uc.find(" IS NOT NULL")!=std::string::npos) {
                    cond.op = Condition::Op::IS_NOT_NULL;
                    cond.column = util::trim(c.substr(0, Uc.find(" IS NOT NULL")));
                } else {
                    const char* ops[]={"<=",">=","!=","=","<",">"};
                    Condition::Op maps[]={Condition::Op::LE,Condition::Op::GE,Condition::Op::NE,Condition::Op::EQ,Condition::Op::LT,Condition::Op::GT};
                    for (int i=0;i<6;++i){
                        auto pos = c.find(ops[i]);
                        if (pos!=std::string::npos) {
                            cond.column = util::trim(c.substr(0,pos));
                            cond.op = maps[i];
                            cond.literal = parse_value(util::trim(c.substr(pos+std::string(ops[i]).size())));
                            break;
                        }
                    }
                }
                out.update.where.push_back(cond);
            }
        }
        auto Up = util::upper(sql);
        auto vp = Up.find("VALID PERIOD");
        if (vp!=std::string::npos) {
            auto b = sql.find('[', vp), e=sql.find(')', vp);
            auto mid = sql.substr(b+1, e-b-1);
            auto parts = util::split(mid, ',');
            if (parts.size()==2) out.update.valid_period = { util::unquote(parts[0]), util::unquote(parts[1]) };
        }
        return out;
    }

    if (util::starts_with(U, "DELETE FROM")) {
        out.kind = ParsedSQL::Kind::DELETE;
        auto rest = util::trim(sql.substr(11));
        size_t wherep = util::upper(rest).find(" WHERE ");
        out.del.table = util::trim(wherep==std::string::npos? rest : rest.substr(0, wherep));
        if (wherep!=std::string::npos) {
            auto conds = util::trim(rest.substr(wherep+7));
            Condition cond;
            const char* ops[]={"<=",">=","!=","=","<",">"};
            Condition::Op maps[]={Condition::Op::LE,Condition::Op::GE,Condition::Op::NE,Condition::Op::EQ,Condition::Op::LT,Condition::Op::GT};
            for (int i=0;i<6;++i){
                auto pos = conds.find(ops[i]);
                if (pos!=std::string::npos) {
                    cond.column = util::trim(conds.substr(0,pos));
                    cond.op = maps[i];
                    cond.literal = parse_value(util::trim(conds.substr(pos+std::string(ops[i]).size())));
                    break;
                }
            }
            out.del.where.push_back(cond);
        }
        return out;
    }

    if (util::starts_with(U, "SELECT ")) {
        out.kind = ParsedSQL::Kind::SELECT;
        auto body = util::trim(sql.substr(7));
        size_t fromp = util::upper(body).find(" FROM ");
        auto selpart = util::trim(body.substr(0, fromp));
        auto rest = body.substr(fromp+6);
        if (util::trim(selpart)=="*" ) out.select.items.push_back({SelectItem::Kind::STAR,"*"});
        else for (auto it : util::split(selpart, ',')) out.select.items.push_back(parse_select_item(it));

        // FROM t [JOIN t2 ON a=b] [FOR SYSTEM_TIME AS OF TX n] [WHERE ...] [GROUP BY ...] [ORDER BY ... [DESC]] [LIMIT n]
        // parse table and JOIN
        std::string after_from = rest;
        // find potential clauses earliest positions
        auto Urest = util::upper(after_from);
        size_t jpos = Urest.find(" JOIN ");
        size_t sysidx = Urest.find(" FOR SYSTEM_TIME AS OF ");
        size_t wherep = Urest.find(" WHERE ");
        size_t gbp = Urest.find(" GROUP BY ");
        size_t obp = Urest.find(" ORDER BY ");
        size_t lim = Urest.find(" LIMIT ");

        size_t first_clause = std::string::npos;
        for (auto p : {jpos, sysidx, wherep, gbp, obp, lim}) if (p!=std::string::npos) first_clause = std::min(first_clause, p);
        if (first_clause==std::string::npos) out.select.table = util::trim(after_from);
        else out.select.table = util::trim(after_from.substr(0, first_clause));

        if (jpos!=std::string::npos) {
            size_t onp = Urest.find(" ON ", jpos+6);
            size_t endj = std::string::npos;
            for (auto p : {sysidx, wherep, gbp, obp, lim}) if (p!=std::string::npos && p>onp) endj = (endj==std::string::npos? p : std::min(endj, p));
            auto right_tbl = util::trim(after_from.substr(jpos+6, onp-(jpos+6)));
            auto onexpr = util::trim(after_from.substr(onp+4, (endj==std::string::npos? after_from.size():endj) - (onp+4)));
            // expect a = b
            auto eqp = onexpr.find('=');
            JoinClause jc; jc.right_table = right_tbl; jc.left_col = util::trim(onexpr.substr(0,eqp)); jc.right_col = util::trim(onexpr.substr(eqp+1));
            out.select.join = jc;
        }

        if (sysidx!=std::string::npos) {
            static const std::string kSysTime = " FOR SYSTEM_TIME AS OF ";
            auto after = util::trim(after_from.substr(sysidx + kSysTime.size()));
            auto U2 = util::upper(after);
            size_t txp = U2.find("TX ");
            if (txp!=std::string::npos) {
                auto nstr = util::trim(after.substr(txp+3));
                if (auto i=util::to_i64(nstr)) out.select.asof_tx = *i;
            }
        }
        if (wherep!=std::string::npos) {
            auto conds = util::trim(after_from.substr(wherep+7));
            // stop at GROUP/ORDER/LIMIT
            size_t stop = std::string::npos;
            for (auto p : {gbp, obp, lim}) if (p!=std::string::npos) stop = (stop==std::string::npos? p : std::min(stop,p));
            if (stop!=std::string::npos) conds = util::trim(after_from.substr(wherep+7, stop-(wherep+7)));
            for (auto part : util::split(conds, 'A')) {
                auto c = util::trim(part); if (c.empty()) continue;
                Condition cond;
                auto Uc = util::upper(c);
                if (Uc.find(" IS NULL")!=std::string::npos) { cond.op=Condition::Op::IS_NULL; cond.column=util::trim(c.substr(0,Uc.find(" IS NULL"))); }
                else if (Uc.find(" IS NOT NULL")!=std::string::npos){ cond.op=Condition::Op::IS_NOT_NULL; cond.column=util::trim(c.substr(0,Uc.find(" IS NOT NULL"))); }
                else {
                    const char* ops[]={"<=",">=","!=","=","<",">"};
                    Condition::Op maps[]={Condition::Op::LE,Condition::Op::GE,Condition::Op::NE,Condition::Op::EQ,Condition::Op::LT,Condition::Op::GT};
                    for (int i=0;i<6;++i){
                        auto pos = c.find(ops[i]);
                        if (pos!=std::string::npos) {
                            cond.column = util::trim(c.substr(0,pos));
                            cond.op = maps[i];
                            cond.literal = parse_value(util::trim(c.substr(pos+std::string(ops[i]).size())));
                            break;
                        }
                    }
                }
                out.select.where.push_back(cond);
            }
        }
        if (gbp!=std::string::npos) {
            auto gb = util::trim(after_from.substr(gbp+10));
            size_t stop = std::string::npos;
            for (auto p : {obp, lim}) if (p!=std::string::npos) stop = (stop==std::string::npos? p : std::min(stop,p));
            if (stop!=std::string::npos) gb = util::trim(after_from.substr(gbp+10, stop-(gbp+10)));
            for (auto g : util::split(gb, ',')) out.select.group_by.push_back(util::trim(g));
        }
        if (obp!=std::string::npos) {
            auto ob = util::trim(after_from.substr(obp+10));
            size_t stop = (lim==std::string::npos? after_from.size() : lim);
            ob = util::trim(after_from.substr(obp+10, stop-(obp+10)));
            bool desc = util::upper(ob).find(" DESC")!=std::string::npos;
            if (desc) ob = util::trim(ob.substr(0, util::upper(ob).find(" DESC")));
            out.select.order_by = { ob, desc };
        }
        if (lim!=std::string::npos) {
            auto ltxt = util::trim(after_from.substr(lim+7));
            if (auto n=util::to_i64(ltxt)) out.select.limit_n = *n;
        }
        return out;
    }

    out.kind = ParsedSQL::Kind::INVALID;
    out.error = "Unrecognized SQL";
    return out;
}
