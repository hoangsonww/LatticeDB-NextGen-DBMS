import { useStore } from "../store";
import {
  Clock,
  RotateCcw,
  CheckCircle,
  XCircle,
  Star,
  Copy,
  Trash2,
} from "lucide-react";
import { useState } from "react";

export default function HistoryPanel() {
  const { history, setSql, darkMode, addToFavorites, favorites } = useStore(
    (s) => ({
      history: s.history,
      setSql: s.setSql,
      darkMode: s.darkMode,
      addToFavorites: s.addToFavorites,
      favorites: s.favorites,
    }),
  );

  const [hoveredItem, setHoveredItem] = useState<number | null>(null);

  const formatTime = (timestamp: number) => {
    const date = new Date(timestamp);
    const now = new Date();
    const diff = now.getTime() - date.getTime();

    if (diff < 60000) return "Just now";
    if (diff < 3600000) return `${Math.floor(diff / 60000)}m ago`;
    if (diff < 86400000) return `${Math.floor(diff / 3600000)}h ago`;
    return date.toLocaleDateString();
  };

  const copyToClipboard = (sql: string) => {
    navigator.clipboard.writeText(sql);
  };

  return (
    <div
      className={`card transition-all duration-300 ${
        darkMode ? "bg-gray-950 border-gray-800" : "bg-white border-gray-200"
      }`}
    >
      {/* Header */}
      <div
        className={`flex items-center justify-between px-4 py-2 border-b transition-colors duration-300 ${
          darkMode ? "border-gray-800 bg-black" : "border-gray-200 bg-gray-50"
        }`}
      >
        <div className="flex items-center gap-3">
          <div
            className={`p-1 rounded ${darkMode ? "bg-gray-900" : "bg-white"}`}
          >
            <Clock size={14} className="text-indigo-500" />
          </div>
          <div>
            <h3
              className={`font-medium text-sm ${darkMode ? "text-gray-200" : "text-gray-900"}`}
            >
              Query History
            </h3>
            <p
              className={`text-xs ${darkMode ? "text-gray-400" : "text-gray-500"}`}
            >
              {history.length} recent queries
            </p>
          </div>
        </div>

        {history.length > 0 && (
          <button
            className={`p-2 rounded-lg transition-all duration-200 hover:scale-105 ${
              darkMode
                ? "hover:bg-gray-700 text-gray-400 hover:text-gray-200"
                : "hover:bg-gray-100 text-gray-500 hover:text-gray-700"
            }`}
            title="Clear history"
          >
            <Trash2 size={14} />
          </button>
        )}
      </div>

      {/* History Content */}
      <div className="max-h-48 overflow-auto">
        {history.length === 0 ? (
          <div
            className={`p-6 text-center ${darkMode ? "text-gray-500" : "text-gray-400"}`}
          >
            <Clock
              size={32}
              className={`mx-auto mb-2 ${darkMode ? "text-gray-600" : "text-gray-300"}`}
            />
            <div className="text-sm">No history yet</div>
            <div className="text-xs mt-1">
              Run some queries to see them here
            </div>
          </div>
        ) : (
          <div className="p-2">
            {history.map((h, i) => (
              <div
                key={i}
                className={`group relative rounded-lg p-3 mb-2 transition-all duration-200 hover:shadow-md ${
                  darkMode
                    ? "hover:bg-gray-800/60 border border-gray-800/60"
                    : "hover:bg-gray-50 border border-gray-200/50"
                }`}
                onMouseEnter={() => setHoveredItem(i)}
                onMouseLeave={() => setHoveredItem(null)}
              >
                <div className="flex items-start gap-3">
                  {/* Status Icon */}
                  <div className="flex-shrink-0 mt-0.5">
                    {h.result?.ok ? (
                      <CheckCircle size={14} className="text-green-500" />
                    ) : h.result ? (
                      <XCircle size={14} className="text-red-500" />
                    ) : (
                      <Clock
                        size={14}
                        className={darkMode ? "text-gray-500" : "text-gray-400"}
                      />
                    )}
                  </div>

                  {/* Query Content */}
                  <div className="flex-1 min-w-0">
                    <div className="flex items-center gap-2 mb-1">
                      <span
                        className={`text-xs ${darkMode ? "text-gray-400" : "text-gray-500"}`}
                      >
                        {formatTime(h.ts)}
                      </span>
                      {h.result && (
                        <span
                          className={`text-xs px-2 py-0.5 rounded-full ${
                            h.result.ok
                              ? darkMode
                                ? "bg-green-900/30 text-green-400"
                                : "bg-green-100 text-green-600"
                              : darkMode
                                ? "bg-red-900/30 text-red-400"
                                : "bg-red-100 text-red-600"
                          }`}
                        >
                          {h.result.ok ? "Success" : "Error"}
                        </span>
                      )}
                    </div>

                    <div
                      className={`text-sm font-mono leading-relaxed ${
                        darkMode ? "text-gray-300" : "text-gray-700"
                      }`}
                    >
                      <pre className="whitespace-pre-wrap break-words">
                        {h.sql}
                      </pre>
                    </div>

                    {h.result?.message && (
                      <div
                        className={`text-xs mt-2 ${
                          h.result.ok
                            ? darkMode
                              ? "text-gray-400"
                              : "text-gray-500"
                            : darkMode
                              ? "text-red-400"
                              : "text-red-600"
                        }`}
                      >
                        {h.result.message}
                      </div>
                    )}
                  </div>

                  {/* Action Buttons */}
                  <div
                    className={`flex items-center gap-1 transition-opacity duration-200 ${
                      hoveredItem === i ? "opacity-100" : "opacity-0"
                    }`}
                  >
                    <button
                      onClick={() => copyToClipboard(h.sql)}
                      className={`p-1.5 rounded transition-all duration-200 hover:scale-110 ${
                        darkMode
                          ? "hover:bg-gray-600 text-gray-400 hover:text-gray-200"
                          : "hover:bg-gray-200 text-gray-500 hover:text-gray-700"
                      }`}
                      title="Copy query"
                    >
                      <Copy size={12} />
                    </button>

                    {!favorites.includes(h.sql.trim()) && (
                      <button
                        onClick={() => addToFavorites(h.sql.trim())}
                        className={`p-1.5 rounded transition-all duration-200 hover:scale-110 ${
                          darkMode
                            ? "hover:bg-gray-600 text-gray-400 hover:text-yellow-400"
                            : "hover:bg-gray-200 text-gray-500 hover:text-yellow-500"
                        }`}
                        title="Add to favorites"
                      >
                        <Star size={12} />
                      </button>
                    )}

                    <button
                      onClick={() => setSql(h.sql)}
                      className={`p-1.5 rounded transition-all duration-200 hover:scale-110 ${
                        darkMode
                          ? "hover:bg-blue-900/30 text-gray-400 hover:text-blue-400"
                          : "hover:bg-blue-100 text-gray-500 hover:text-blue-600"
                      }`}
                      title="Load in editor"
                    >
                      <RotateCcw size={12} />
                    </button>
                  </div>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}
