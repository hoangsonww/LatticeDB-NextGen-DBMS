import { useEffect, useState } from "react";
import { health } from "../lib/api";
import { useStore } from "../store";
import {
  Database,
  SignalHigh,
  SignalLow,
  Table,
  Key,
  Hash,
  Calendar,
  FileText,
  Star,
  StarOff,
  ChevronDown,
  ChevronRight,
  Copy,
  Play,
  BookOpen,
  Zap,
  BarChart3,
  Settings,
  Wifi,
  WifiOff,
} from "lucide-react";

export default function SidePanel() {
  const {
    tables,
    favorites,
    mockMode,
    toggleMockMode,
    setSql,
    darkMode,
    addToFavorites,
    removeFromFavorites,
  } = useStore();
  const [ok, setOk] = useState(false);
  const [expandedTables, setExpandedTables] = useState<Record<string, boolean>>(
    {},
  );
  const [activeTab, setActiveTab] = useState<
    "schema" | "templates" | "favorites"
  >("schema");

  useEffect(() => {
    if (mockMode) {
      setOk(true);
    } else {
      health().then(setOk);
    }
  }, [mockMode]);

  const toggleTable = (tableName: string) => {
    setExpandedTables((prev) => ({
      ...prev,
      [tableName]: !prev[tableName],
    }));
  };

  const getTypeIcon = (type: string) => {
    if (
      type.includes("INTEGER") ||
      type.includes("BIGINT") ||
      type.includes("DECIMAL")
    )
      return Hash;
    if (type.includes("VARCHAR") || type.includes("TEXT")) return FileText;
    if (type.includes("TIMESTAMP") || type.includes("DATE")) return Calendar;
    if (type.includes("VECTOR")) return BarChart3;
    return Database;
  };

  const getTypeColor = (type: string) => {
    if (
      type.includes("INTEGER") ||
      type.includes("BIGINT") ||
      type.includes("DECIMAL")
    )
      return "text-blue-600";
    if (type.includes("VARCHAR") || type.includes("TEXT"))
      return "text-green-600";
    if (type.includes("TIMESTAMP") || type.includes("DATE"))
      return "text-purple-600";
    if (type.includes("VECTOR")) return "text-orange-600";
    return "text-gray-600";
  };

  const queryTemplates = [
    {
      category: "Basic Queries",
      queries: [
        {
          name: "Select All Users",
          sql: "SELECT * FROM users LIMIT 10;",
          description: "Get first 10 users",
        },
        {
          name: "Count Records",
          sql: "SELECT COUNT(*) as total_records FROM users;",
          description: "Count total users",
        },
        {
          name: "Recent Orders",
          sql: "SELECT * FROM orders WHERE order_date >= DATE('2024-01-01') ORDER BY order_date DESC LIMIT 20;",
          description: "Orders from 2024",
        },
      ],
    },
    {
      category: "Advanced Analytics",
      queries: [
        {
          name: "User Age Distribution",
          sql: "SELECT \n  CASE \n    WHEN age < 25 THEN 'Young (< 25)'\n    WHEN age < 40 THEN 'Adult (25-40)'\n    ELSE 'Senior (40+)'\n  END as age_group,\n  COUNT(*) as count\nFROM users \nGROUP BY age_group\nORDER BY count DESC;",
          description: "Analyze user demographics",
        },
        {
          name: "Revenue by Status",
          sql: "SELECT \n  status,\n  COUNT(*) as order_count,\n  SUM(total_amount) as total_revenue,\n  AVG(total_amount) as avg_order_value\nFROM orders \nGROUP BY status\nORDER BY total_revenue DESC;",
          description: "Revenue analysis by order status",
        },
        {
          name: "Top Products",
          sql: "SELECT \n  p.name,\n  p.price,\n  p.category_id,\n  COUNT(*) as popularity_score\nFROM products p\nWHERE p.price > 50\nGROUP BY p.id, p.name, p.price, p.category_id\nORDER BY popularity_score DESC\nLIMIT 15;",
          description: "Most popular premium products",
        },
      ],
    },
    {
      category: "Vector & AI Queries",
      queries: [
        {
          name: "Vector Similarity Search",
          sql: "SELECT \n  id, name, profile_vector,\n  VECTOR_DISTANCE(profile_vector, [0.1, 0.2, 0.3, 0.4]) as similarity\nFROM users\nWHERE VECTOR_DISTANCE(profile_vector, [0.1, 0.2, 0.3, 0.4]) < 0.5\nORDER BY similarity ASC\nLIMIT 10;",
          description: "Find similar user profiles",
        },
        {
          name: "Product Embeddings",
          sql: "SELECT \n  name, price,\n  VECTOR_COSINE_SIMILARITY(embedding, [0.1, 0.2, ...]) as relevance\nFROM products\nWHERE VECTOR_COSINE_SIMILARITY(embedding, [0.1, 0.2, ...]) > 0.8\nORDER BY relevance DESC;",
          description: "Semantic product search",
        },
      ],
    },
  ];

  const formatRowCount = (count: number) => {
    if (count > 1000000) return `${(count / 1000000).toFixed(1)}M`;
    if (count > 1000) return `${(count / 1000).toFixed(1)}K`;
    return count.toString();
  };

  return (
    <div className="w-full flex flex-col gap-4 h-fit overflow-hidden">
      {/* Connection Status */}
      <div
        className={`card p-4 border-l-4 transition-all duration-300 ${
          darkMode
            ? "bg-gray-950 border-l-blue-400 text-gray-100 border-gray-800"
            : "bg-white border-l-blue-500 text-gray-900 border-gray-200"
        }`}
      >
        <div className="flex items-center gap-3">
          <div
            className={`p-2 rounded-lg shadow-sm transition-colors duration-300 ${
              darkMode ? "bg-gray-900" : "bg-white"
            }`}
          >
            <Database className="text-blue-500" size={20} />
          </div>
          <div className="flex-1">
            <div
              className={`font-semibold ${darkMode ? "text-gray-100" : "text-gray-900"}`}
            >
              LatticeDB Studio
            </div>
            <div className="flex items-center gap-2 text-sm">
              {ok ? (
                <>
                  <div className="flex items-center gap-1 text-emerald-500">
                    {mockMode ? <Wifi size={14} /> : <SignalHigh size={14} />}
                    {mockMode ? "Mock Mode" : "Connected"}
                  </div>
                </>
              ) : (
                <div className="flex items-center gap-1 text-rose-500">
                  <WifiOff size={14} />
                  Offline
                </div>
              )}
              <button
                onClick={toggleMockMode}
                className={`ml-auto px-2 py-1 text-xs rounded-md transition-all duration-200 hover:scale-105 ${
                  darkMode
                    ? "bg-gray-700 hover:bg-gray-600 text-gray-300"
                    : "bg-gray-200 hover:bg-gray-300 text-gray-700"
                }`}
                title={mockMode ? "Switch to Live Mode" : "Switch to Mock Mode"}
              >
                {mockMode ? "Mock" : "Live"}
              </button>
            </div>
          </div>
        </div>
        <div
          className={`text-xs mt-2 flex items-center gap-2 ${
            darkMode ? "text-gray-400" : "text-gray-600"
          }`}
        >
          <span>localhost:7070</span>
          <span className="text-gray-500">•</span>
          <span>{tables.length} tables</span>
        </div>
      </div>

      {/* Tab Navigation */}
      <div
        className={`card p-0 overflow-hidden transition-colors duration-300 ${
          darkMode ? "bg-gray-950 border-gray-800" : "bg-white border-gray-200"
        }`}
      >
        <div
          className={`flex border-b transition-colors duration-300 ${
            darkMode ? "border-gray-800 bg-black" : "border-gray-200 bg-gray-50"
          }`}
        >
          {[
            { key: "schema", label: "Schema", icon: Database },
            { key: "templates", label: "Templates", icon: BookOpen },
            { key: "favorites", label: "Favorites", icon: Star },
          ].map((tab) => {
            const Icon = tab.icon;
            const isActive = activeTab === tab.key;
            return (
              <button
                key={tab.key}
                onClick={() => setActiveTab(tab.key as any)}
                className={`flex-1 px-2 py-3 text-sm font-medium flex items-center gap-2 justify-center border-b-2 transition-all duration-200 hover:scale-105 ${
                  isActive
                    ? darkMode
                      ? "border-blue-400 text-blue-400 bg-gray-900/80"
                      : "border-blue-500 text-blue-600 bg-blue-50"
                    : darkMode
                      ? "border-transparent text-gray-400 hover:text-gray-200 hover:bg-gray-800/50"
                      : "border-transparent text-gray-600 hover:text-gray-900 hover:bg-gray-50"
                }`}
              >
                <Icon size={16} />
                <span className="text-xs whitespace-nowrap">{tab.label}</span>
              </button>
            );
          })}
        </div>

        <div className="h-96 overflow-y-auto">
          {activeTab === "schema" && (
            <div className="p-4 space-y-3">
              {tables.map((table) => {
                const isExpanded = expandedTables[table.name];
                return (
                  <div
                    key={table.name}
                    className={`border rounded-lg overflow-hidden transition-all duration-200 hover:shadow-md ${
                      darkMode
                        ? "border-gray-600 bg-gray-700/50"
                        : "border-gray-200 bg-white"
                    }`}
                  >
                    <div
                      className={`flex items-center gap-2 p-3 cursor-pointer transition-all duration-200 ${
                        darkMode
                          ? "bg-gray-700/30 hover:bg-gray-700/60"
                          : "bg-gray-50 hover:bg-gray-100"
                      }`}
                      onClick={() => toggleTable(table.name)}
                    >
                      <div className="transform transition-transform duration-200">
                        {isExpanded ? (
                          <ChevronDown size={16} />
                        ) : (
                          <ChevronRight size={16} />
                        )}
                      </div>
                      <Table
                        size={16}
                        className={darkMode ? "text-gray-400" : "text-gray-600"}
                      />
                      <span
                        className={`font-medium ${darkMode ? "text-gray-200" : "text-gray-900"}`}
                      >
                        {table.name}
                      </span>
                      <div className="ml-auto flex items-center gap-2 text-xs">
                        <span
                          className={
                            darkMode ? "text-gray-400" : "text-gray-500"
                          }
                        >
                          {formatRowCount(table.rowCount)} rows
                        </span>
                        <button
                          onClick={(e) => {
                            e.stopPropagation();
                            setSql(`SELECT * FROM ${table.name} LIMIT 20;`);
                          }}
                          className={`p-1 rounded transition-all duration-200 hover:scale-110 ${
                            darkMode
                              ? "hover:bg-blue-900/30 hover:text-blue-400"
                              : "hover:bg-blue-100 hover:text-blue-600"
                          }`}
                          title="Select from table"
                        >
                          <Play size={12} />
                        </button>
                      </div>
                    </div>

                    {isExpanded && (
                      <div
                        className={`border-t transition-colors duration-300 ${
                          darkMode ? "border-gray-600" : "border-gray-200"
                        }`}
                      >
                        {table.columns.map((column) => {
                          const TypeIcon = getTypeIcon(column.type);
                          return (
                            <div
                              key={column.name}
                              className={`flex items-center gap-2 p-2 text-sm transition-colors duration-200 ${
                                darkMode
                                  ? "hover:bg-gray-700/40"
                                  : "hover:bg-gray-50"
                              }`}
                            >
                              <TypeIcon
                                size={14}
                                className={getTypeColor(column.type)}
                              />
                              <span
                                className={`font-mono ${darkMode ? "text-gray-300" : "text-gray-900"}`}
                              >
                                {column.name}
                              </span>
                              {column.primary && (
                                <Key
                                  size={12}
                                  className="text-amber-500"
                                  title="Primary Key"
                                />
                              )}
                              <span
                                className={`text-xs ${getTypeColor(column.type)} ml-auto`}
                              >
                                {column.type}
                              </span>
                              {!column.nullable && (
                                <span
                                  className={`text-xs px-1 rounded ${
                                    darkMode
                                      ? "bg-red-900/30 text-red-400"
                                      : "bg-red-100 text-red-600"
                                  }`}
                                >
                                  NOT NULL
                                </span>
                              )}
                            </div>
                          );
                        })}
                      </div>
                    )}
                  </div>
                );
              })}
            </div>
          )}

          {activeTab === "templates" && (
            <div className="p-4 space-y-4">
              {queryTemplates.map((category) => (
                <div key={category.category}>
                  <h4
                    className={`font-semibold mb-3 flex items-center gap-2 ${
                      darkMode ? "text-gray-200" : "text-gray-900"
                    }`}
                  >
                    <Zap size={16} className="text-blue-500" />
                    {category.category}
                  </h4>
                  <div className="space-y-2">
                    {category.queries.map((query) => (
                      <div
                        key={query.name}
                        className={`border rounded-lg p-3 transition-all duration-200 hover:shadow-md ${
                          darkMode
                            ? "border-gray-600 bg-gray-700/30 hover:bg-gray-700/50"
                            : "border-gray-200 bg-white hover:bg-gray-50"
                        }`}
                      >
                        <div className="flex items-start gap-2">
                          <div className="flex-1">
                            <div
                              className={`font-medium text-sm ${darkMode ? "text-gray-200" : "text-gray-900"}`}
                            >
                              {query.name}
                            </div>
                            <div
                              className={`text-xs mt-1 ${darkMode ? "text-gray-400" : "text-gray-600"}`}
                            >
                              {query.description}
                            </div>
                          </div>
                          <div className="flex gap-1">
                            <button
                              onClick={() =>
                                navigator.clipboard.writeText(query.sql)
                              }
                              className={`p-1 rounded transition-all duration-200 hover:scale-110 ${
                                darkMode
                                  ? "hover:bg-gray-600 text-gray-400 hover:text-gray-200"
                                  : "hover:bg-gray-200 text-gray-600 hover:text-gray-800"
                              }`}
                              title="Copy to clipboard"
                            >
                              <Copy size={12} />
                            </button>
                            <button
                              onClick={() => setSql(query.sql)}
                              className={`p-1 rounded transition-all duration-200 hover:scale-110 ${
                                darkMode
                                  ? "hover:bg-blue-900/30 hover:text-blue-400"
                                  : "hover:bg-blue-100 hover:text-blue-600"
                              }`}
                              title="Load in editor"
                            >
                              <Play size={12} />
                            </button>
                          </div>
                        </div>
                      </div>
                    ))}
                  </div>
                </div>
              ))}
            </div>
          )}

          {activeTab === "favorites" && (
            <div className="p-4 space-y-3">
              {favorites.length === 0 ? (
                <div
                  className={`text-center py-8 ${darkMode ? "text-gray-400" : "text-gray-500"}`}
                >
                  <Star
                    size={48}
                    className={`mx-auto mb-3 ${darkMode ? "text-gray-600" : "text-gray-300"}`}
                  />
                  <div className="text-sm">No favorite queries yet</div>
                  <div className="text-xs mt-1">
                    Star queries to save them here
                  </div>
                </div>
              ) : (
                favorites.map((query, index) => (
                  <div
                    key={index}
                    className={`border rounded-lg p-3 transition-all duration-200 hover:shadow-md ${
                      darkMode
                        ? "border-gray-600 bg-gray-700/30 hover:bg-gray-700/50"
                        : "border-gray-200 bg-white hover:bg-gray-50"
                    }`}
                  >
                    <div className="flex items-start gap-2">
                      <div className="flex-1">
                        <div
                          className={`text-sm font-mono mb-2 line-clamp-3 ${
                            darkMode ? "text-gray-300" : "text-gray-700"
                          }`}
                        >
                          {query}
                        </div>
                      </div>
                      <div className="flex gap-1">
                        <button
                          onClick={() => removeFromFavorites(query)}
                          className={`p-1 rounded transition-all duration-200 hover:scale-110 ${
                            darkMode
                              ? "hover:bg-red-900/30 hover:text-red-400"
                              : "hover:bg-red-100 hover:text-red-600"
                          }`}
                          title="Remove from favorites"
                        >
                          <StarOff size={12} />
                        </button>
                        <button
                          onClick={() => navigator.clipboard.writeText(query)}
                          className={`p-1 rounded transition-all duration-200 hover:scale-110 ${
                            darkMode
                              ? "hover:bg-gray-600 text-gray-400 hover:text-gray-200"
                              : "hover:bg-gray-200 text-gray-600 hover:text-gray-800"
                          }`}
                          title="Copy to clipboard"
                        >
                          <Copy size={12} />
                        </button>
                        <button
                          onClick={() => setSql(query)}
                          className={`p-1 rounded transition-all duration-200 hover:scale-110 ${
                            darkMode
                              ? "hover:bg-blue-900/30 hover:text-blue-400"
                              : "hover:bg-blue-100 hover:text-blue-600"
                          }`}
                          title="Load in editor"
                        >
                          <Play size={12} />
                        </button>
                      </div>
                    </div>
                  </div>
                ))
              )}
            </div>
          )}
        </div>
      </div>

      {/* Quick Stats */}
      <div
        className={`card p-4 transition-all duration-300 ${
          darkMode ? "bg-gray-950 border-gray-800" : "bg-white border-gray-200"
        }`}
      >
        <div
          className={`text-sm font-semibold mb-3 ${darkMode ? "text-gray-200" : "text-gray-900"}`}
        >
          Database Stats
        </div>
        <div className="grid grid-cols-2 gap-3 text-xs">
          <div
            className={`rounded-lg p-3 text-center transition-all duration-200 hover:scale-105 ${
              darkMode ? "bg-gray-900" : "bg-white"
            }`}
          >
            <div className="font-semibold text-lg text-blue-500 mb-1">
              {tables
                .reduce((sum, table) => sum + table.rowCount, 0)
                .toLocaleString()}
            </div>
            <div className={darkMode ? "text-gray-400" : "text-gray-600"}>
              Total Rows
            </div>
          </div>
          <div
            className={`rounded-lg p-3 text-center transition-all duration-200 hover:scale-105 ${
              darkMode ? "bg-gray-900" : "bg-white"
            }`}
          >
            <div className="font-semibold text-lg text-green-500 mb-1">
              {tables.length}
            </div>
            <div className={darkMode ? "text-gray-400" : "text-gray-600"}>
              Tables
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
