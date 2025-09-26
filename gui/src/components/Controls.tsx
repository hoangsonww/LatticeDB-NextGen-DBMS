import { useStore } from "../store";
import { Settings2, Shield, Clock, Zap } from "lucide-react";

export default function Controls() {
  const { dp, setDp, tx, setTx, darkMode, mockMode } = useStore((s) => ({
    dp: s.dpEpsilon,
    setDp: s.setDp,
    tx: s.txAsOf,
    setTx: s.setTx,
    darkMode: s.darkMode,
    mockMode: s.mockMode,
  }));

  return (
    <div
      className={`card transition-all duration-300 ${
        darkMode ? "bg-gray-950 border-gray-800" : "bg-white border-gray-200"
      }`}
    >
      {/* Header */}
      <div
        className={`flex items-center gap-3 px-4 py-2 border-b transition-colors duration-300 ${
          darkMode ? "border-gray-800 bg-black" : "border-gray-200 bg-gray-50"
        }`}
      >
        <div className={`p-1 rounded ${darkMode ? "bg-gray-900" : "bg-white"}`}>
          <Settings2 size={14} className="text-purple-500" />
        </div>
        <div>
          <h3
            className={`font-medium text-sm ${darkMode ? "text-gray-200" : "text-gray-900"}`}
          >
            Query Controls
          </h3>
          <p
            className={`text-xs ${darkMode ? "text-gray-400" : "text-gray-500"}`}
          >
            Query parameters
          </p>
        </div>
      </div>

      {/* Controls Grid */}
      <div className="p-3 grid grid-cols-1 xl:grid-cols-3 gap-4">
        {/* Differential Privacy */}
        <div className="space-y-2">
          <div className="flex items-center gap-2">
            <Shield size={14} className="text-blue-500" />
            <label
              className={`text-xs font-medium ${darkMode ? "text-gray-300" : "text-gray-700"}`}
            >
              Differential Privacy ε
            </label>
          </div>
          <div className="flex items-center gap-3">
            <input
              className={`flex-1 h-2 rounded-lg appearance-none cursor-pointer transition-all duration-200 ${
                darkMode
                  ? "bg-gray-700 [&::-webkit-slider-thumb]:bg-blue-500"
                  : "bg-gray-200 [&::-webkit-slider-thumb]:bg-blue-600"
              } [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:h-4 [&::-webkit-slider-thumb]:w-4 [&::-webkit-slider-thumb]:rounded-full [&::-webkit-slider-thumb]:shadow-md hover:[&::-webkit-slider-thumb]:scale-110`}
              type="range"
              min={0.1}
              max={5}
              step={0.1}
              value={dp}
              onChange={(e) => setDp(parseFloat(e.target.value))}
            />
            <div
              className={`w-12 text-right text-sm font-mono ${
                darkMode ? "text-gray-300" : "text-gray-700"
              }`}
            >
              {dp.toFixed(1)}
            </div>
          </div>
          <p
            className={`text-xs ${darkMode ? "text-gray-500" : "text-gray-400"}`}
          >
            Lower values provide more privacy
          </p>
        </div>

        {/* Time Travel */}
        <div className="space-y-2">
          <div className="flex items-center gap-2">
            <Clock size={14} className="text-green-500" />
            <label
              className={`text-xs font-medium ${darkMode ? "text-gray-300" : "text-gray-700"}`}
            >
              Time Travel (TX as-of)
            </label>
          </div>
          <input
            className={`w-full px-3 py-2 rounded-lg border text-sm transition-all duration-200 focus:ring-2 focus:ring-green-500/20 focus:border-green-500 ${
              darkMode
                ? "bg-gray-700 border-gray-600 text-gray-200 placeholder-gray-400"
                : "bg-gray-50 border-gray-300 text-gray-900 placeholder-gray-500"
            }`}
            placeholder="e.g. 42"
            value={tx ?? ""}
            onChange={(e) =>
              setTx(e.target.value ? parseInt(e.target.value, 10) : undefined)
            }
          />
          <p
            className={`text-xs ${darkMode ? "text-gray-500" : "text-gray-400"}`}
          >
            Query historical data at transaction{" "}
            <code
              className={`px-1 py-0.5 rounded text-xs ${
                darkMode
                  ? "bg-gray-700 text-green-400"
                  : "bg-gray-100 text-green-600"
              }`}
            >
              #{tx || "latest"}
            </code>
          </p>
        </div>

        {/* Vector Helper */}
        <div className="space-y-2">
          <div className="flex items-center gap-2">
            <Zap size={14} className="text-orange-500" />
            <label
              className={`text-xs font-medium ${darkMode ? "text-gray-300" : "text-gray-700"}`}
            >
              Vector Operations
            </label>
          </div>
          <div
            className={`p-3 rounded-lg border transition-colors duration-300 ${
              darkMode
                ? "bg-gray-700/30 border-gray-600"
                : "bg-gray-50 border-gray-200"
            }`}
          >
            <div
              className={`text-xs ${darkMode ? "text-gray-400" : "text-gray-600"}`}
            >
              Available functions:
            </div>
            <div className="mt-1 space-y-1">
              <code
                className={`block text-xs ${darkMode ? "text-orange-400" : "text-orange-600"}`}
              >
                VECTOR_DISTANCE(a, b)
              </code>
              <code
                className={`block text-xs ${darkMode ? "text-orange-400" : "text-orange-600"}`}
              >
                VECTOR_COSINE_SIMILARITY(a, b)
              </code>
            </div>
          </div>
          <p
            className={`text-xs ${darkMode ? "text-gray-500" : "text-gray-400"}`}
          >
            {mockMode
              ? "Mock data includes vector columns"
              : "Requires vector-enabled columns"}
          </p>
        </div>
      </div>
    </div>
  );
}
