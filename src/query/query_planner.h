#pragma once

#include "sql_parser.h"
#include "../catalog/catalog_manager.h"
#include "../types/schema.h"
#include <memory>
#include <vector>

namespace latticedb {

enum class PlanNodeType {
    INVALID,
    TABLE_SCAN,
    INDEX_SCAN,
    PROJECTION,
    FILTER,
    NESTED_LOOP_JOIN,
    HASH_JOIN,
    SORT,
    LIMIT,
    INSERT,
    UPDATE,
    DELETE,
    AGGREGATE
};

class PlanNode {
public:
    PlanNodeType type;
    std::shared_ptr<Schema> output_schema;
    std::vector<std::unique_ptr<PlanNode>> children;

    explicit PlanNode(PlanNodeType t) : type(t) {}
    virtual ~PlanNode() = default;

    virtual std::string to_string(int indent = 0) const = 0;
};

class TableScanPlanNode : public PlanNode {
public:
    table_oid_t table_oid;
    std::string table_name;
    std::unique_ptr<Expression> predicate;

    TableScanPlanNode(table_oid_t oid, const std::string& name, std::shared_ptr<Schema> schema)
        : PlanNode(PlanNodeType::TABLE_SCAN), table_oid(oid), table_name(name) {
        output_schema = schema;
    }

    std::string to_string(int indent = 0) const override;
};

class IndexScanPlanNode : public PlanNode {
public:
    index_oid_t index_oid;
    std::string index_name;
    table_oid_t table_oid;
    std::unique_ptr<Expression> predicate;

    IndexScanPlanNode(index_oid_t idx_oid, const std::string& idx_name, table_oid_t tbl_oid)
        : PlanNode(PlanNodeType::INDEX_SCAN), index_oid(idx_oid),
          index_name(idx_name), table_oid(tbl_oid) {}

    std::string to_string(int indent = 0) const override;
};

class ProjectionPlanNode : public PlanNode {
public:
    std::vector<std::unique_ptr<Expression>> expressions;

    ProjectionPlanNode(std::vector<std::unique_ptr<Expression>> exprs, std::shared_ptr<Schema> schema)
        : PlanNode(PlanNodeType::PROJECTION), expressions(std::move(exprs)) {
        output_schema = schema;
    }

    std::string to_string(int indent = 0) const override;
};

class FilterPlanNode : public PlanNode {
public:
    std::unique_ptr<Expression> predicate;

    FilterPlanNode(std::unique_ptr<Expression> pred, std::unique_ptr<PlanNode> child)
        : PlanNode(PlanNodeType::FILTER), predicate(std::move(pred)) {
        children.push_back(std::move(child));
        output_schema = children[0]->output_schema;
    }

    std::string to_string(int indent = 0) const override;
};

class SortPlanNode : public PlanNode {
public:
    std::vector<OrderByClause> order_bys;

    SortPlanNode(std::vector<OrderByClause> orders, std::unique_ptr<PlanNode> child)
        : PlanNode(PlanNodeType::SORT), order_bys(std::move(orders)) {
        children.push_back(std::move(child));
        output_schema = children[0]->output_schema;
    }

    std::string to_string(int indent = 0) const override;
};

class LimitPlanNode : public PlanNode {
public:
    int limit;

    LimitPlanNode(int lim, std::unique_ptr<PlanNode> child)
        : PlanNode(PlanNodeType::LIMIT), limit(lim) {
        children.push_back(std::move(child));
        output_schema = children[0]->output_schema;
    }

    std::string to_string(int indent = 0) const override;
};

class InsertPlanNode : public PlanNode {
public:
    table_oid_t table_oid;
    std::string table_name;
    std::vector<std::vector<Value>> values;

    InsertPlanNode(table_oid_t oid, const std::string& name, std::vector<std::vector<Value>> vals)
        : PlanNode(PlanNodeType::INSERT), table_oid(oid), table_name(name), values(std::move(vals)) {}

    std::string to_string(int indent = 0) const override;
};

class UpdatePlanNode : public PlanNode {
public:
    table_oid_t table_oid;
    std::string table_name;
    std::vector<std::pair<std::string, Value>> assignments;

    UpdatePlanNode(table_oid_t oid, const std::string& name,
                   std::vector<std::pair<std::string, Value>> assigns,
                   std::unique_ptr<PlanNode> child)
        : PlanNode(PlanNodeType::UPDATE), table_oid(oid), table_name(name),
          assignments(std::move(assigns)) {
        children.push_back(std::move(child));
    }

    std::string to_string(int indent = 0) const override;
};

class DeletePlanNode : public PlanNode {
public:
    table_oid_t table_oid;
    std::string table_name;

    DeletePlanNode(table_oid_t oid, const std::string& name, std::unique_ptr<PlanNode> child)
        : PlanNode(PlanNodeType::DELETE), table_oid(oid), table_name(name) {
        children.push_back(std::move(child));
    }

    std::string to_string(int indent = 0) const override;
};

class QueryPlanner {
private:
    Catalog* catalog_;

public:
    explicit QueryPlanner(Catalog* catalog) : catalog_(catalog) {}

    std::unique_ptr<PlanNode> plan_query(const ParsedQuery& query);

private:
    std::unique_ptr<PlanNode> plan_select(const SelectQuery& query);
    std::unique_ptr<PlanNode> plan_insert(const InsertQuery& query);
    std::unique_ptr<PlanNode> plan_update(const UpdateQuery& query);
    std::unique_ptr<PlanNode> plan_delete(const DeleteQuery& query);

    std::unique_ptr<PlanNode> optimize_plan(std::unique_ptr<PlanNode> plan);

    std::unique_ptr<PlanNode> push_down_filter(std::unique_ptr<PlanNode> plan,
                                              std::unique_ptr<Expression> filter);

    bool can_use_index(const Expression& predicate, const std::string& table_name,
                      index_oid_t* index_oid);

    std::shared_ptr<Schema> infer_schema(const std::vector<std::unique_ptr<Expression>>& expressions,
                                        const Schema& input_schema);

    double estimate_cardinality(const PlanNode& node);
    double estimate_cost(const PlanNode& node);
};

}