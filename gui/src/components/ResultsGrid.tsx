import { useMemo } from "react";
import { useStore } from "../store";
import {
  Table2,
  CheckCircle,
  XCircle,
  Clock,
  Download,
  Copy,
} from "lucide-react";

function cell(v: any, darkMode: boolean) {
  if (v === null || v === undefined)
    return (
      <span
        className={`italic ${darkMode ? "text-gray-500" : "text-gray-400"}`}
      >
        NULL
      </span>
    );
  if (Array.isArray(v))
    return (
      <span
        className={`font-mono text-xs ${darkMode ? "text-blue-400" : "text-blue-600"}`}
      >
        [{v.join(", ")}]
      </span>
    );
  if (typeof v === "string")
    return (
      <span className={darkMode ? "text-green-400" : "text-green-600"}>
        '{v}'
      </span>
    );
  if (typeof v === "number")
    return (
      <span
        className={`font-mono ${darkMode ? "text-yellow-400" : "text-orange-600"}`}
      >
        {v}
      </span>
    );
  return (
    <span className={darkMode ? "text-gray-300" : "text-gray-700"}>
      {String(v)}
    </span>
  );
}

export default function ResultGrid() {
  const { result, executing, darkMode } = useStore((s) => ({
    result: s.result,
    executing: s.executing,
    darkMode: s.darkMode,
  }));

  const empty =
    !result || (result.headers.length === 0 && result.rows.length === 0);
  const rows = useMemo(() => result?.rows ?? [], [result]);

  const copyTableData = () => {
    if (!result?.rows?.length) return;
    const csvData = [
      result.headers.join("\t"),
      ...result.rows.map((row) => row.map((cell) => String(cell)).join("\t")),
    ].join("\n");
    navigator.clipboard.writeText(csvData);
  };

  const exportToCsv = () => {
    if (!result?.rows?.length) return;
    const csvData = [
      result.headers.join(","),
      ...result.rows.map((row) =>
        row
          .map((cell) =>
            typeof cell === "string"
              ? `"${cell.replace(/"/g, '""')}"`
              : String(cell),
          )
          .join(","),
      ),
    ].join("\n");

    const blob = new Blob([csvData], { type: "text/csv" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = "query_results.csv";
    a.click();
    URL.revokeObjectURL(url);
  };

  return (
    <div
      className={`card transition-all duration-300 ${
        darkMode ? "bg-gray-950 border-gray-800" : "bg-white border-gray-200"
      }`}
    >
      {/* Results Header */}
      <div
        className={`flex items-center justify-between px-4 py-2 border-b transition-colors duration-300 ${
          darkMode ? "border-gray-800 bg-black" : "border-gray-200 bg-gray-50"
        }`}
      >
        <div className="flex items-center gap-3">
          <div
            className={`p-1 rounded ${darkMode ? "bg-gray-900" : "bg-white"}`}
          >
            <Table2 size={14} className="text-green-500" />
          </div>
          <div>
            <h3
              className={`font-medium text-sm ${darkMode ? "text-gray-200" : "text-gray-900"}`}
            >
              Query Results
            </h3>
            {result && (
              <div className="flex items-center gap-2 text-xs">
                {result.ok ? (
                  <>
                    <CheckCircle size={12} className="text-green-500" />
                    <span
                      className={darkMode ? "text-gray-400" : "text-gray-500"}
                    >
                      {result.message || `${rows.length} rows returned`}
                    </span>
                  </>
                ) : (
                  <>
                    <XCircle size={12} className="text-red-500" />
                    <span className="text-red-500">{result.message}</span>
                  </>
                )}
              </div>
            )}
            {executing && (
              <div className="flex items-center gap-2 text-xs">
                <Clock size={12} className="text-blue-500 animate-spin" />
                <span className={darkMode ? "text-gray-400" : "text-gray-500"}>
                  Executing query...
                </span>
              </div>
            )}
          </div>
        </div>

        {result?.rows?.length > 0 && (
          <div className="flex items-center gap-2">
            <button
              onClick={copyTableData}
              className={`p-2 rounded-lg transition-all duration-200 hover:scale-105 ${
                darkMode
                  ? "hover:bg-gray-700 text-gray-400 hover:text-gray-200"
                  : "hover:bg-gray-100 text-gray-500 hover:text-gray-700"
              }`}
              title="Copy as TSV"
            >
              <Copy size={14} />
            </button>
            <button
              onClick={exportToCsv}
              className={`p-2 rounded-lg transition-all duration-200 hover:scale-105 ${
                darkMode
                  ? "hover:bg-gray-700 text-gray-400 hover:text-gray-200"
                  : "hover:bg-gray-100 text-gray-500 hover:text-gray-700"
              }`}
              title="Export as CSV"
            >
              <Download size={14} />
            </button>
          </div>
        )}
      </div>

      {/* Results Content */}
      <div className="overflow-auto max-h-[300px]">
        {executing ? (
          <div className="flex items-center justify-center py-12">
            <div
              className={`text-center ${darkMode ? "text-gray-400" : "text-gray-500"}`}
            >
              <div className="animate-spin w-8 h-8 border-2 border-blue-500/20 border-t-blue-500 rounded-full mx-auto mb-3"></div>
              <div>Executing query...</div>
            </div>
          </div>
        ) : empty ? (
          <div className="flex items-center justify-center py-8">
            <div
              className={`text-center ${darkMode ? "text-gray-500" : "text-gray-400"}`}
            >
              <Table2
                size={32}
                className={`mx-auto mb-2 ${darkMode ? "text-gray-600" : "text-gray-300"}`}
              />
              <div className="text-sm">Run a query to see results</div>
              <div className="text-xs mt-1">Results will appear here</div>
            </div>
          </div>
        ) : result?.ok ? (
          <div className="p-4">
            <div className="overflow-x-auto">
              <table className="w-full text-sm">
                {result?.headers?.length ? (
                  <thead>
                    <tr>
                      {result.headers.map((h, i) => (
                        <th
                          key={i}
                          className={`text-left font-semibold py-3 px-3 border-b-2 sticky top-0 transition-colors duration-300 ${
                            darkMode
                              ? "border-gray-600 bg-gray-900 text-gray-200"
                              : "border-gray-300 bg-gray-50 text-gray-900"
                          }`}
                        >
                          {h}
                        </th>
                      ))}
                    </tr>
                  </thead>
                ) : null}
                <tbody>
                  {rows.map((r, i) => (
                    <tr
                      key={i}
                      className={`transition-colors duration-200 hover:${
                        darkMode ? "bg-gray-800/50" : "bg-gray-50"
                      } ${i % 2 === 1 ? (darkMode ? "bg-gray-900/50" : "bg-gray-25") : ""}`}
                    >
                      {r.map((c, j) => (
                        <td
                          key={j}
                          className={`py-2 px-3 align-top border-b transition-colors duration-300 ${
                            darkMode ? "border-gray-700/50" : "border-gray-100"
                          }`}
                        >
                          {cell(c, darkMode)}
                        </td>
                      ))}
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        ) : (
          <div className="p-4">
            <div
              className={`rounded-lg p-4 border ${
                darkMode
                  ? "bg-red-900/20 border-red-800 text-red-400"
                  : "bg-red-50 border-red-200 text-red-700"
              }`}
            >
              <div className="flex items-center gap-2 mb-2">
                <XCircle size={16} />
                <span className="font-semibold">Query Error</span>
              </div>
              <div className="font-mono text-sm">{result?.message}</div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
