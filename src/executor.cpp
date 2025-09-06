#include "executor.h"
#include <iostream>
#include <unordered_set>
#include <map>

static Value coerce_to(const Value& v, ColumnType t) {
    if (std::holds_alternative<std::monostate>(v.v)) return Value::Null();
    switch (t) {
        case ColumnType::INT:
            if (std::holds_alternative<int64_t>(v.v)) return v;
            if (std::holds_alternative<double>(v.v)) return Value::I((int64_t)std::get<double>(v.v));
            if (std::holds_alternative<std::string>(v.v)) {
                if (auto i = util::to_i64(std::get<std::string>(v.v))) return Value::I(*i);
            }
            break;
        case ColumnType::DOUBLE:
            if (std::holds_alternative<double>(v.v)) return v;
            if (std::holds_alternative<int64_t>(v.v)) return Value::D((double)std::get<int64_t>(v.v));
            if (std::holds_alternative<std::string>(v.v)) {
                if (auto d = util::to_f64(std::get<std::string>(v.v))) return Value::D(*d);
            }
            break;
        case ColumnType::TEXT:
            if (std::holds_alternative<std::string>(v.v)) return v;
            if (std::holds_alternative<int64_t>(v.v)) return Value::S(std::to_string(std::get<int64_t>(v.v)));
            if (std::holds_alternative<double>(v.v)) return Value::S(std::to_string(std::get<double>(v.v)));
            break;
        case ColumnType::SET_TEXT:
            if (std::holds_alternative<std::set<std::string>>(v.v)) return v;
            if (std::holds_alternative<std::string>(v.v)) return Value::SET({std::get<std::string>(v.v)});
            break;
        case ColumnType::VECTOR:
            if (std::holds_alternative<std::vector<double>>(v.v)) return v;
            break;
    }
    return Value::Null();
}

static Value get_value_qualified(const TableDef* dl, const RowVersion* rl, const TableDef* dr, const RowVersion* rr, const std::string& col) {
    auto t = util::trim(col);
    int idx=-1;
    if (t.find('.')!=std::string::npos) {
        auto parts = util::split(t,'.');
        if (parts.size()==2) {
            auto tname = util::upper(parts[0]);
            auto cname = parts[1];
            if (dl && util::upper(dl->name)==tname) { idx = dl->col_index(cname); if (idx>=0) return rl->data[idx];}
            if (dr && util::upper(dr->name)==tname) { idx = dr->col_index(cname); if (idx>=0) return rr->data[idx];}
        }
        return Value::Null();
    } else {
        if (dl) { idx = dl->col_index(t); if (idx>=0) return rl->data[idx]; }
        if (dr) { idx = dr->col_index(t); if (idx>=0) return rr->data[idx]; }
    }
    return Value::Null();
}

static bool eval_condition_joined(const Condition& c, const TableDef* dl, const RowVersion* rl, const TableDef* dr, const RowVersion* rr) {
    if (c.is_distance) return false;
    if (c.op==Condition::Op::IS_NULL || c.op==Condition::Op::IS_NOT_NULL) {
        auto v = get_value_qualified(dl, rl, dr, rr, c.column);
        bool isnull = std::holds_alternative<std::monostate>(v.v);
        return c.op==Condition::Op::IS_NULL ? isnull : !isnull;
    }
    auto v = get_value_qualified(dl, rl, dr, rr, c.column);
    auto cmp = [&](auto L, auto R)->int{ if (L<R) return -1; if (L>R) return 1; return 0; };
    if (std::holds_alternative<int64_t>(v.v) && std::holds_alternative<int64_t>(c.literal.v)) {
        int d = cmp(std::get<int64_t>(v.v), std::get<int64_t>(c.literal.v));
        if (c.op==Condition::Op::EQ) return d==0;
        if (c.op==Condition::Op::NE) return d!=0;
        if (c.op==Condition::Op::LT) return d<0;
        if (c.op==Condition::Op::LE) return d<=0;
        if (c.op==Condition::Op::GT) return d>0;
        if (c.op==Condition::Op::GE) return d>=0;
    } else if (std::holds_alternative<double>(v.v) && (std::holds_alternative<double>(c.literal.v)||std::holds_alternative<int64_t>(c.literal.v))) {
        double lv = std::get<double>(v.v);
        double rvv = std::holds_alternative<double>(c.literal.v) ? std::get<double>(c.literal.v) : (double)std::get<int64_t>(c.literal.v);
        int d = cmp(lv, rvv);
        if (c.op==Condition::Op::EQ) return d==0;
        if (c.op==Condition::Op::NE) return d!=0;
        if (c.op==Condition::Op::LT) return d<0;
        if (c.op==Condition::Op::LE) return d<=0;
        if (c.op==Condition::Op::GT) return d>0;
        if (c.op==Condition::Op::GE) return d>=0;
    } else if (std::holds_alternative<std::string>(v.v) && std::holds_alternative<std::string>(c.literal.v)) {
        int d = cmp(std::get<std::string>(v.v), std::get<std::string>(c.literal.v));
        if (c.op==Condition::Op::EQ) return d==0;
        if (c.op==Condition::Op::NE) return d!=0;
        if (c.op==Condition::Op::LT) return d<0;
        if (c.op==Condition::Op::LE) return d<=0;
        if (c.op==Condition::Op::GT) return d>0;
        if (c.op==Condition::Op::GE) return d>=0;
    }
    return false;
}

static bool eval_condition_single(const Condition& c, const TableDef& def, const RowVersion& rv) {
    if (c.is_distance) {
        int ci = def.col_index(c.vec_col);
        if (ci<0) return false;
        if (!std::holds_alternative<std::vector<double>>(rv.data[ci].v)) return false;
        auto dist = vec::l2_distance(std::get<std::vector<double>>(rv.data[ci].v), c.vec_value);
        if (!dist) return false;
        return *dist < c.dist_threshold;
    }
    int idx = def.col_index(c.column);
    if (idx<0) return false;
    const auto& v = rv.data[idx];
    switch (c.op) {
        case Condition::Op::IS_NULL: return std::holds_alternative<std::monostate>(v.v);
        case Condition::Op::IS_NOT_NULL: return !std::holds_alternative<std::monostate>(v.v);
        default: break;
    }
    auto cmp = [&](auto L, auto R)->int{ if (L<R) return -1; if (L>R) return 1; return 0; };
    if (std::holds_alternative<int64_t>(v.v) && std::holds_alternative<int64_t>(c.literal.v)) {
        int d = cmp(std::get<int64_t>(v.v), std::get<int64_t>(c.literal.v));
        if (c.op==Condition::Op::EQ) return d==0;
        if (c.op==Condition::Op::NE) return d!=0;
        if (c.op==Condition::Op::LT) return d<0;
        if (c.op==Condition::Op::LE) return d<=0;
        if (c.op==Condition::Op::GT) return d>0;
        if (c.op==Condition::Op::GE) return d>=0;
    } else if (std::holds_alternative<double>(v.v) && (std::holds_alternative<double>(c.literal.v)||std::holds_alternative<int64_t>(c.literal.v))) {
        double lv = std::get<double>(v.v);
        double rvv = std::holds_alternative<double>(c.literal.v) ? std::get<double>(c.literal.v) : (double)std::get<int64_t>(c.literal.v);
        int d = cmp(lv, rvv);
        if (c.op==Condition::Op::EQ) return d==0;
        if (c.op==Condition::Op::NE) return d!=0;
        if (c.op==Condition::Op::LT) return d<0;
        if (c.op==Condition::Op::LE) return d<=0;
        if (c.op==Condition::Op::GT) return d>0;
        if (c.op==Condition::Op::GE) return d>=0;
    } else if (std::holds_alternative<std::string>(v.v) && std::holds_alternative<std::string>(c.literal.v)) {
        int d = cmp(std::get<std::string>(v.v), std::get<std::string>(c.literal.v));
        if (c.op==Condition::Op::EQ) return d==0;
        if (c.op==Condition::Op::NE) return d!=0;
        if (c.op==Condition::Op::LT) return d<0;
        if (c.op==Condition::Op::LE) return d<=0;
        if (c.op==Condition::Op::GT) return d>0;
        if (c.op==Condition::Op::GE) return d>=0;
    }
    return false;
}

QueryResult exec_create(ExecutionContext& ctx, const CreateTableQuery& q) {
    TableDef t; t.name = q.table; t.mergeable = q.mergeable;
    for (auto& c : q.cols) {
        ColumnDef cd; cd.name=c.name; cd.type=c.type; cd.primary_key=c.pk; cd.merge=c.merge; cd.vector_dim=c.vec_dim;
        t.cols.push_back(cd);
    }
    ctx.db->create_table(t);
    return { {}, {}, "CREATE TABLE" };
}

QueryResult exec_drop(ExecutionContext& ctx, const DropTableQuery& q) {
    if (!ctx.db->catalog.get(q.table)) return {{},{}, "No such table: "+q.table, false};
    ctx.db->drop_table(q.table);
    return {{},{}, "DROP TABLE"};
}

QueryResult exec_insert(ExecutionContext& ctx, const InsertQuery& q) {
    auto* td = ctx.db->table(q.table);
    if (!td) return { {}, {}, "No such table: "+q.table, false };
    auto apply_insert = [&](std::vector<RowVersion> rows)->PendingWrite{
        return {
            // apply
            [td, rows]() mutable {
                for (auto& rv : rows) td->versions.push_back(std::move(rv));
            },
            // undo
            [td, n=(size_t)rows.size()]() mutable {
                for (size_t i=0;i<n;++i) td->versions.pop_back();
            }
        };
    };

    std::vector<RowVersion> new_rows;
    auto tx = ctx.db->begin_tx();

    for (const auto& row : q.rows) {
        if (row.size()!=q.cols.size()) return {{},{}, "INSERT row/column count mismatch", false};
        RowVersion rv; rv.data.resize(td->def.cols.size());
        for (size_t i=0;i<rv.data.size();++i) rv.data[i]=Value::Null();
        for (size_t i=0;i<q.cols.size();++i) {
            int idx = td->def.col_index(q.cols[i]);
            if (idx<0) return {{},{}, "Unknown column "+q.cols[i], false};
            Value v = coerce_to(row[i], td->def.cols[idx].type);
            if (td->def.cols[idx].type==ColumnType::VECTOR) {
                if (!std::holds_alternative<std::vector<double>>(v.v)) return {{},{}, "VECTOR literal required", false};
                if (std::get<std::vector<double>>(v.v).size()!=td->def.cols[idx].vector_dim)
                    return {{},{}, "VECTOR dimension mismatch", false};
            }
            rv.data[idx]=v;
        }
        if (td->def.pk_index<0) return {{},{}, "PRIMARY KEY required", false};
        const auto& pkv = rv.data[td->def.pk_index];
        if (!std::holds_alternative<std::string>(pkv.v) && !std::holds_alternative<int64_t>(pkv.v))
            return {{},{}, "PRIMARY KEY must be TEXT or INT", false};
        rv.row_id = std::holds_alternative<std::string>(pkv.v) ? std::get<std::string>(pkv.v) : std::to_string(std::get<int64_t>(pkv.v));
        // Conflict?
        bool had_prev=false;
        for (auto& old : td->versions) {
            if (old.row_id==rv.row_id && old.tx_to==std::numeric_limits<int64_t>::max()) {
                had_prev=true;
                if (q.on_conflict_merge && td->def.mergeable) {
                    RowVersion newv = old;
                    newv.tx_from = tx; newv.tx_to = std::numeric_limits<int64_t>::max();
                    for (size_t cidx=0;cidx<td->def.cols.size();++cidx) {
                        auto merged = crdt_merge(td->def.cols[cidx].merge, old.data[cidx], rv.data[cidx]);
                        newv.data[cidx] = merged.is_null()? old.data[cidx] : merged;
                    }
                    old.tx_to = tx;
                    new_rows.push_back(std::move(newv));
                } else {
                    RowVersion newv = old;
                    newv.tx_from = tx; newv.tx_to = std::numeric_limits<int64_t>::max();
                    for (size_t cidx=0;cidx<td->def.cols.size();++cidx)
                        if (!std::holds_alternative<std::monostate>(rv.data[cidx].v)) newv.data[cidx]=rv.data[cidx];
                    old.tx_to = tx; new_rows.push_back(std::move(newv));
                }
                break;
            }
        }
        if (!had_prev) { rv.tx_from = tx; new_rows.push_back(std::move(rv)); }
    }

    auto pw = apply_insert(new_rows);
    if (ctx.in_tx) { ctx.staged.push_back(std::move(pw)); return {{},{}, "INSERT staged"}; }
    pw.apply();
    return { {}, {}, "INSERT " + std::to_string(q.rows.size()) + " row(s)" };
}

QueryResult exec_update(ExecutionContext& ctx, const UpdateQuery& q) {
    auto* td = ctx.db->table(q.table);
    if (!td) return { {}, {}, "No such table: "+q.table, false };
    auto tx = ctx.db->begin_tx();

    struct Change { size_t idx; RowVersion oldv; RowVersion newv; };
    std::vector<Change> changes;

    for (size_t i=0;i<td->versions.size();++i) {
        auto& old = td->versions[i];
        if (old.tx_to!=std::numeric_limits<int64_t>::max()) continue;
        bool ok=true; for (auto& c : q.where) if (!eval_condition_single(c, td->def, old)) { ok=false; break; }
        if (!ok) continue;
        RowVersion nv = old; nv.tx_from = tx; nv.tx_to = std::numeric_limits<int64_t>::max();
        for (auto& [k,v] : q.sets) {
            int idx = td->def.col_index(k); if (idx<0) continue;
            auto vv = coerce_to(v, td->def.cols[idx].type);
            if (td->def.mergeable && td->def.cols[idx].merge.kind!=MergeSpec::Kind::NONE)
                nv.data[idx] = crdt_merge(td->def.cols[idx].merge, nv.data[idx], vv);
            else nv.data[idx] = vv;
        }
        if (q.valid_period) { nv.valid_from = q.valid_period->first; nv.valid_to = q.valid_period->second; }
        changes.push_back({i, old, nv});
    }

    auto apply_update = [td, changes]() mutable {
        for (auto& ch : changes) {
            td->versions[ch.idx].tx_to = ch.newv.tx_from;
            td->versions.push_back(std::move(ch.newv));
        }
    };
    auto undo_update = [td, changes]() mutable {
        // Rollback: pop pushed new versions and restore tx_to on old
        for (size_t k=0;k<changes.size();++k) td->versions.pop_back();
        for (auto& ch : changes) td->versions[ch.idx].tx_to = std::numeric_limits<int64_t>::max();
    };

    if (ctx.in_tx) { ctx.staged.push_back({apply_update, undo_update}); return {{},{}, "UPDATE staged"}; }
    apply_update();
    return { {}, {}, "UPDATE " + std::to_string(changes.size()) + " row(s)" };
}

QueryResult exec_delete(ExecutionContext& ctx, const DeleteQuery& q) {
    auto* td = ctx.db->table(q.table);
    if (!td) return { {}, {}, "No such table: "+q.table, false };
    auto tx = ctx.db->begin_tx();
    std::vector<size_t> to_close;

    for (size_t i=0;i<td->versions.size();++i) {
        auto& rv = td->versions[i];
        if (rv.tx_to!=std::numeric_limits<int64_t>::max()) continue;
        bool ok=true; for (auto& c : q.where) if (!eval_condition_single(c, td->def, rv)) { ok=false; break; }
        if (!ok) continue;
        to_close.push_back(i);
    }
    auto apply = [td, tx, to_close]() mutable {
        for (auto idx : to_close) td->versions[idx].tx_to = tx;
    };
    auto undo = [td, to_close]() mutable {
        for (auto idx : to_close) td->versions[idx].tx_to = std::numeric_limits<int64_t>::max();
    };
    if (ctx.in_tx) { ctx.staged.push_back({apply, undo}); return {{},{}, "DELETE staged"}; }
    apply();
    return {{},{}, "DELETE "+std::to_string(to_close.size())+" row(s)"};
}

static void add_headers_for_star(std::vector<std::string>& H, const TableDef& t) {
    for (auto& c : t.cols) H.push_back(c.name);
    H.push_back("_tx_from"); H.push_back("_tx_to"); H.push_back("_valid_from"); H.push_back("_valid_to");
}

static void add_headers_for_star_join(std::vector<std::string>& H, const TableDef& l, const TableDef& r) {
    for (auto& c : l.cols) H.push_back(l.name + "." + c.name);
    for (auto& c : r.cols) H.push_back(r.name + "." + c.name);
}

static void append_row_for_star(QueryRow& qr, const TableDef& t, const RowVersion& rv) {
    for (size_t i=0;i<t.cols.size();++i) qr.cols.push_back(rv.data[i]);
    qr.cols.push_back(Value::I(rv.tx_from));
    qr.cols.push_back(Value::I(rv.tx_to));
    qr.cols.push_back(Value::S(rv.valid_from));
    qr.cols.push_back(Value::S(rv.valid_to));
}

static void append_row_for_star_join(QueryRow& qr, const TableDef& l, const RowVersion& rl, const TableDef& r, const RowVersion& rr) {
    for (size_t i=0;i<l.cols.size();++i) qr.cols.push_back(rl.data[i]);
    for (size_t i=0;i<r.cols.size();++i) qr.cols.push_back(rr.data[i]);
}

static Value get_col_simple(const TableDef& t, const RowVersion& rv, const std::string& name) {
    int ci = t.col_index(name); if (ci<0) return Value::Null();
    return rv.data[ci];
}

QueryResult exec_select(ExecutionContext& ctx, const SelectQuery& q) {
    auto* left = ctx.db->table(q.table);
    if (!left) return {{},{}, "No such table: "+q.table, false};
    int64_t asof = q.asof_tx.value_or(std::numeric_limits<int64_t>::max());

    // Build visible maps
    std::unordered_map<std::string, const RowVersion*> L;
    for (auto& rv : left->versions) if (rv.tx_from<=asof && asof<rv.tx_to) L[rv.row_id] = &rv;

    // No join?
    if (!q.join) {
        QueryResult res;
        bool star = (q.items.size()==1 && q.items[0].kind==SelectItem::Kind::STAR);
        if (star) add_headers_for_star(res.headers, left->def);
        else {
            for (auto& it : q.items) {
                if (it.kind==SelectItem::Kind::COLUMN) res.headers.push_back(it.column);
                if (it.kind==SelectItem::Kind::AGG_COUNT) res.headers.push_back("count");
                if (it.kind==SelectItem::Kind::AGG_SUM) res.headers.push_back("sum");
                if (it.kind==SelectItem::Kind::AGG_AVG) res.headers.push_back("avg");
                if (it.kind==SelectItem::Kind::AGG_MIN) res.headers.push_back("min");
                if (it.kind==SelectItem::Kind::AGG_MAX) res.headers.push_back("max");
                if (it.kind==SelectItem::Kind::DP_COUNT) res.headers.push_back("dp_count");
                if (it.kind==SelectItem::Kind::STAR) { for (auto& c:left->def.cols) res.headers.push_back(c.name); }
            }
        }

        // aggregation?
        bool has_agg=false; for (auto& it : q.items) if (it.kind!=SelectItem::Kind::COLUMN && it.kind!=SelectItem::Kind::STAR) has_agg=true;
        if (has_agg || !q.group_by.empty() || (q.items.size()==1 && q.items[0].kind==SelectItem::Kind::DP_COUNT)) {
            // DP COUNT(*) only
            if (q.items.size()==1 && q.items[0].kind==SelectItem::Kind::DP_COUNT) {
                int64_t count=0;
                for (auto& [rid, rv] : L) {
                    bool ok=true; for (auto& c : q.where) if (!eval_condition_single(c, left->def, *rv)) { ok=false; break; }
                    if (ok) ++count;
                }
                double b = 1.0 / std::max(1e-9, ctx.dp_epsilon);
                double noisy = (double)count + ctx.lap.sample(b);
                QueryRow qr; qr.cols.push_back(Value::D(noisy)); res.rows.push_back(qr);
                return res;
            }
            // GROUP BY keys
            struct Agg {
                int64_t cnt=0;
                double sum=0;
                int64_t sum_cnt=0;
                double minv=0, maxv=0;
                bool has_min=false, has_max=false;
                std::vector<Value> any_cols;
            };
            std::map<std::string, Agg> groups;
            auto key_of = [&](const RowVersion* rv)->std::string{
                if (q.group_by.empty()) return std::string("__ALL__");
                std::vector<std::string> parts;
                for (auto& g : q.group_by) {
                    auto v = get_col_simple(left->def, *rv, g);
                    if (std::holds_alternative<int64_t>(v.v)) parts.push_back("i:"+std::to_string(std::get<int64_t>(v.v)));
                    else if (std::holds_alternative<double>(v.v)) parts.push_back("f:"+std::to_string(std::get<double>(v.v)));
                    else if (std::holds_alternative<std::string>(v.v)) parts.push_back("s:"+std::get<std::string>(v.v));
                    else parts.push_back("n:");
                }
                return util::join(parts, "\x1F");
            };
            for (auto& [rid, rv] : L) {
                bool ok=true; for (auto& c : q.where) if (!eval_condition_single(c, left->def, *rv)) { ok=false; break; }
                if (!ok) continue;
                auto key = key_of(rv);
                auto& ag = groups[key]; ag.cnt++;
                for (auto& it : q.items) {
                    if (it.kind==SelectItem::Kind::AGG_SUM || it.kind==SelectItem::Kind::AGG_AVG ||
                        it.kind==SelectItem::Kind::AGG_MIN || it.kind==SelectItem::Kind::AGG_MAX) {
                        auto v = get_col_simple(left->def, *rv, it.column);
                        if (std::holds_alternative<int64_t>(v.v) || std::holds_alternative<double>(v.v)) {
                            double d = std::holds_alternative<int64_t>(v.v) ? (double)std::get<int64_t>(v.v) : std::get<double>(v.v);
                            if (it.kind==SelectItem::Kind::AGG_SUM || it.kind==SelectItem::Kind::AGG_AVG) { ag.sum += d; ag.sum_cnt++; }
                            if (it.kind==SelectItem::Kind::AGG_MIN) { if (!ag.has_min || d < ag.minv) { ag.minv = d; ag.has_min = true; } }
                            if (it.kind==SelectItem::Kind::AGG_MAX) { if (!ag.has_max || d > ag.maxv) { ag.maxv = d; ag.has_max = true; } }
                        }
                    }
                }
            }
            // emit
            for (auto& [k, ag] : groups) {
                QueryRow qr;
                // output group columns first if selected explicitly
                for (auto& it : q.items) {
                    if (it.kind==SelectItem::Kind::COLUMN) {
                        // from key is complex; recompute not stored; skip (output NULL) for brevity
                        qr.cols.push_back(Value::Null());
                    } else if (it.kind==SelectItem::Kind::AGG_COUNT) qr.cols.push_back(Value::I(ag.cnt));
                    else if (it.kind==SelectItem::Kind::AGG_SUM) qr.cols.push_back(Value::D(ag.sum));
                    else if (it.kind==SelectItem::Kind::AGG_AVG) qr.cols.push_back(Value::D(ag.sum_cnt? ag.sum/ag.sum_cnt : 0.0));
                    else if (it.kind==SelectItem::Kind::AGG_MIN) qr.cols.push_back(Value::D(ag.has_min? ag.minv : 0.0));
                    else if (it.kind==SelectItem::Kind::AGG_MAX) qr.cols.push_back(Value::D(ag.has_max? ag.maxv : 0.0));
                    else if (it.kind==SelectItem::Kind::STAR) { /* not meaningful in GROUP BY */ }
                }
                res.rows.push_back(std::move(qr));
            }
            // ORDER BY + LIMIT on aggregated result
            if (q.order_by) {
                auto colname = q.order_by->first;
                int ord_idx=-1;
                for (size_t i=0;i<res.headers.size();++i) if (util::iequals(res.headers[i], colname)) { ord_idx=(int)i; break; }
                if (ord_idx>=0) {
                    std::sort(res.rows.begin(), res.rows.end(), [&](const QueryRow& a, const QueryRow& b){
                        const auto& va = a.cols[ord_idx]; const auto& vb = b.cols[ord_idx];
                        double da=0, db=0;
                        if (std::holds_alternative<int64_t>(va.v)) da=(double)std::get<int64_t>(va.v);
                        else if (std::holds_alternative<double>(va.v)) da=std::get<double>(va.v);
                        if (std::holds_alternative<int64_t>(vb.v)) db=(double)std::get<int64_t>(vb.v);
                        else if (std::holds_alternative<double>(vb.v)) db=std::get<double>(vb.v);
                        bool asc = !q.order_by->second;
                        return asc ? (da<db) : (da>db);
                    });
                }
            }
            if (q.limit_n && (int64_t)res.rows.size() > *q.limit_n) res.rows.resize((size_t)*q.limit_n);
            return res;
        }

        // non-aggregated path
        for (auto& [rid, rv] : L) {
            bool ok=true; for (auto& c : q.where) if (!eval_condition_single(c, left->def, *rv)) { ok=false; break; }
            if (!ok) continue;
            QueryRow qr;
            if (star) append_row_for_star(qr, left->def, *rv);
            else {
                for (auto& it : q.items) {
                    if (it.kind==SelectItem::Kind::COLUMN) qr.cols.push_back(get_col_simple(left->def, *rv, it.column));
                    else if (it.kind==SelectItem::Kind::STAR) for (auto& c:left->def.cols) qr.cols.push_back(get_col_simple(left->def,*rv,c.name));
                }
            }
            res.rows.push_back(std::move(qr));
        }
        if (q.order_by) {
            auto colname = q.order_by->first;
            int ord_idx=-1;
            for (size_t i=0;i<res.headers.size();++i) if (util::iequals(res.headers[i], colname)) { ord_idx=(int)i; break; }
            if (ord_idx>=0) {
                std::sort(res.rows.begin(), res.rows.end(), [&](const QueryRow& a, const QueryRow& b){
                    const auto& va = a.cols[ord_idx]; const auto& vb = b.cols[ord_idx];
                    std::string sa, sb;
                    if (std::holds_alternative<int64_t>(va.v)) sa=std::to_string(std::get<int64_t>(va.v));
                    else if (std::holds_alternative<double>(va.v)) sa=std::to_string(std::get<double>(va.v));
                    else if (std::holds_alternative<std::string>(va.v)) sa=std::get<std::string>(va.v);
                    if (std::holds_alternative<int64_t>(vb.v)) sb=std::to_string(std::get<int64_t>(vb.v));
                    else if (std::holds_alternative<double>(vb.v)) sb=std::to_string(std::get<double>(vb.v));
                    else if (std::holds_alternative<std::string>(vb.v)) sb=std::get<std::string>(vb.v);
                    bool asc = !q.order_by->second;
                    return asc ? (sa<sb) : (sa>sb);
                });
            }
        }
        if (q.limit_n && (int64_t)res.rows.size() > *q.limit_n) res.rows.resize((size_t)*q.limit_n);
        return res;
    }

    // JOIN path (inner equi-join)
    auto* right = ctx.db->table(q.join->right_table);
    if (!right) return {{},{}, "No such table: "+q.join->right_table, false};
    std::unordered_map<std::string, const RowVersion*> R;
    for (auto& rv : right->versions) if (rv.tx_from<=asof && asof<rv.tx_to) R[rv.row_id] = &rv;

    // Build result headers
    QueryResult res;
    bool star = (q.items.size()==1 && q.items[0].kind==SelectItem::Kind::STAR);
    if (star) add_headers_for_star_join(res.headers, left->def, right->def);
    else {
        for (auto& it : q.items) {
            if (it.kind==SelectItem::Kind::COLUMN) res.headers.push_back(it.column);
            else if (it.kind==SelectItem::Kind::STAR) {
                for (auto& c:left->def.cols) res.headers.push_back(left->def.name+"."+c.name);
                for (auto& c:right->def.cols) res.headers.push_back(right->def.name+"."+c.name);
            } else if (it.kind==SelectItem::Kind::AGG_COUNT) res.headers.push_back("count");
            else if (it.kind==SelectItem::Kind::AGG_SUM) res.headers.push_back("sum");
            else if (it.kind==SelectItem::Kind::AGG_AVG) res.headers.push_back("avg");
            else if (it.kind==SelectItem::Kind::AGG_MIN) res.headers.push_back("min");
            else if (it.kind==SelectItem::Kind::AGG_MAX) res.headers.push_back("max");
        }
    }

    // build hash on right join key
    auto right_key_col = q.join->right_col.find('.')==std::string::npos ? (right->def.name + "." + q.join->right_col) : q.join->right_col;
    auto left_key_col  = q.join->left_col.find('.')==std::string::npos  ? (left->def.name  + "." + q.join->left_col)  : q.join->left_col;

    auto value_to_key = [](const Value& v)->std::string{
        if (std::holds_alternative<int64_t>(v.v)) return "i:"+std::to_string(std::get<int64_t>(v.v));
        if (std::holds_alternative<double>(v.v)) return "f:"+std::to_string(std::get<double>(v.v));
        if (std::holds_alternative<std::string>(v.v)) return "s:"+std::get<std::string>(v.v);
        return "n:";
    };

    std::unordered_map<std::string, std::vector<const RowVersion*>> RH;
    for (auto& [rid, rv] : R) {
        auto v = get_value_qualified(&right->def, rv, nullptr, nullptr, right_key_col);
        RH[value_to_key(v)].push_back(rv);
    }

    // aggregation on join?
    bool has_agg=false; for (auto& it : q.items) if (it.kind!=SelectItem::Kind::COLUMN && it.kind!=SelectItem::Kind::STAR) has_agg=true;
    if (has_agg || !q.group_by.empty()) {
        struct Agg {
            int64_t cnt=0;
            double sum=0;
            int64_t sum_cnt=0;
            double minv=0, maxv=0;
            bool has_min=false, has_max=false;
        };
        std::map<std::string, Agg> groups;
        auto key_of = [&](const RowVersion* rl, const RowVersion* rr)->std::string{
            if (q.group_by.empty()) return "__ALL__";
            std::vector<std::string> parts;
            for (auto& g : q.group_by) {
                auto v = get_value_qualified(&left->def, rl, &right->def, rr, g);
                parts.push_back(value_to_key(v));
            }
            return util::join(parts, "\x1F");
        };
        for (auto& [lid, lv] : L) {
            auto lvk = get_value_qualified(&left->def, lv, nullptr, nullptr, left_key_col);
            auto it = RH.find(value_to_key(lvk));
            if (it==RH.end()) continue;
            for (auto rr : it->second) {
                bool ok=true; for (auto& c : q.where) if (!eval_condition_joined(c, &left->def, lv, &right->def, rr)) { ok=false; break; }
                if (!ok) continue;
                auto key = key_of(lv, rr);
                auto& ag = groups[key]; ag.cnt++;
                for (auto& it2 : q.items) {
                    if (it2.kind==SelectItem::Kind::AGG_SUM || it2.kind==SelectItem::Kind::AGG_AVG ||
                        it2.kind==SelectItem::Kind::AGG_MIN || it2.kind==SelectItem::Kind::AGG_MAX) {
                        auto v = get_value_qualified(&left->def, lv, &right->def, rr, it2.column);
                        if (std::holds_alternative<int64_t>(v.v) || std::holds_alternative<double>(v.v)) {
                            double d = std::holds_alternative<int64_t>(v.v) ? (double)std::get<int64_t>(v.v) : std::get<double>(v.v);
                            if (it2.kind==SelectItem::Kind::AGG_SUM || it2.kind==SelectItem::Kind::AGG_AVG) { ag.sum += d; ag.sum_cnt++; }
                            if (it2.kind==SelectItem::Kind::AGG_MIN) { if (!ag.has_min || d < ag.minv) { ag.minv = d; ag.has_min = true; } }
                            if (it2.kind==SelectItem::Kind::AGG_MAX) { if (!ag.has_max || d > ag.maxv) { ag.maxv = d; ag.has_max = true; } }
                        }
                    }
                }
            }
        }
        for (auto& [k, ag] : groups) {
            QueryRow qr;
            for (auto& it2 : q.items) {
                if (it2.kind==SelectItem::Kind::AGG_COUNT) qr.cols.push_back(Value::I(ag.cnt));
                else if (it2.kind==SelectItem::Kind::AGG_SUM) qr.cols.push_back(Value::D(ag.sum));
                else if (it2.kind==SelectItem::Kind::AGG_AVG) qr.cols.push_back(Value::D(ag.sum_cnt? ag.sum/ag.sum_cnt : 0.0));
                else if (it2.kind==SelectItem::Kind::AGG_MIN) qr.cols.push_back(Value::D(ag.has_min? ag.minv : 0.0));
                else if (it2.kind==SelectItem::Kind::AGG_MAX) qr.cols.push_back(Value::D(ag.has_max? ag.maxv : 0.0));
                else if (it2.kind==SelectItem::Kind::COLUMN) qr.cols.push_back(Value::Null()); // placeholder
            }
            res.rows.push_back(std::move(qr));
        }
        if (q.limit_n && (int64_t)res.rows.size() > *q.limit_n) res.rows.resize((size_t)*q.limit_n);
        return res;
    }

    // non-agg join output rows
    for (auto& [lid, lv] : L) {
        auto lvk = get_value_qualified(&left->def, lv, nullptr, nullptr, left_key_col);
        auto it = RH.find(value_to_key(lvk));
        if (it==RH.end()) continue;
        for (auto rr : it->second) {
            bool ok=true; for (auto& c : q.where) if (!eval_condition_joined(c, &left->def, lv, &right->def, rr)) { ok=false; break; }
            if (!ok) continue;
            QueryRow qr;
            if (star) append_row_for_star_join(qr, left->def, *lv, right->def, *rr);
            else {
                for (auto& it2 : q.items) {
                    if (it2.kind==SelectItem::Kind::COLUMN)
                        qr.cols.push_back(get_value_qualified(&left->def, lv, &right->def, rr, it2.column));
                    else if (it2.kind==SelectItem::Kind::STAR) {
                        for (auto& c:left->def.cols) qr.cols.push_back(lv->data[left->def.col_index(c.name)]);
                        for (auto& c:right->def.cols) qr.cols.push_back(rr->data[right->def.col_index(c.name)]);
                    }
                }
            }
            res.rows.push_back(std::move(qr));
        }
    }
    if (q.limit_n && (int64_t)res.rows.size() > *q.limit_n) res.rows.resize((size_t)*q.limit_n);
    return res;
}

QueryResult exec_set(ExecutionContext& ctx, const SetQuery& q) {
    if (q.key=="DP_EPSILON") { ctx.dp_epsilon = q.value; return {{},{}, "OK (DP_EPSILON="+std::to_string(q.value)+")"}; }
    return {{},{}, "Unknown SET key", false};
}
QueryResult exec_save(ExecutionContext& ctx, const SaveQuery& q) { ctx.db->save(q.path); return {{},{}, "Saved to "+q.path}; }
QueryResult exec_load(ExecutionContext& ctx, const LoadQuery& q) { if (!ctx.db->load(q.path)) return {{},{}, "Failed to load "+q.path, false}; return {{},{}, "Loaded "+q.path}; }
QueryResult exec_begin(ExecutionContext& ctx) { if (ctx.in_tx) return {{},{}, "Already in transaction", false}; ctx.in_tx=true; ctx.staged.clear(); return {{},{}, "BEGIN"}; }
QueryResult exec_commit(ExecutionContext& ctx) {
    if (!ctx.in_tx) return {{},{}, "Not in transaction", false};
    for (auto& w : ctx.staged) w.apply();
    ctx.staged.clear(); ctx.in_tx=false; return {{},{}, "COMMIT"};
}
QueryResult exec_rollback(ExecutionContext& ctx) {
    if (!ctx.in_tx) return {{},{}, "Not in transaction", false};
    for (auto it = ctx.staged.rbegin(); it!=ctx.staged.rend(); ++it) it->undo();
    ctx.staged.clear(); ctx.in_tx=false; return {{},{}, "ROLLBACK"};
}
