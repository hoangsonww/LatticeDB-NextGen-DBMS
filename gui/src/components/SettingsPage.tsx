import { useStore } from "../store";
import {
  X,
  Moon,
  Sun,
  Database,
  Star,
  Activity,
  Shield,
  Zap,
  ChevronLeft,
} from "lucide-react";

export default function SettingsPage({ onClose }: { onClose: () => void }) {
  const {
    darkMode,
    toggleDarkMode,
    favorites,
    history,
    tables,
    dpEpsilon,
    setDp,
  } = useStore();

  return (
    <div
      className={`fixed inset-0 z-50 ${darkMode ? "bg-black" : "bg-gray-50"}`}
    >
      {/* Header */}
      <div
        className={`border-b ${
          darkMode ? "bg-gray-950 border-gray-800" : "bg-white border-gray-200"
        }`}
      >
        <div className="max-w-4xl mx-auto px-6 py-4 flex items-center justify-between">
          <div className="flex items-center gap-3">
            <button
              onClick={onClose}
              className={`p-2 rounded-lg transition-colors ${
                darkMode
                  ? "hover:bg-gray-800 text-gray-400"
                  : "hover:bg-gray-100 text-gray-600"
              }`}
            >
              <ChevronLeft size={20} />
            </button>
            <h1
              className={`text-xl font-semibold ${
                darkMode ? "text-gray-100" : "text-gray-900"
              }`}
            >
              Settings
            </h1>
          </div>
          <button
            onClick={onClose}
            className={`p-2 rounded-lg transition-colors ${
              darkMode
                ? "hover:bg-gray-800 text-gray-400"
                : "hover:bg-gray-100 text-gray-600"
            }`}
          >
            <X size={20} />
          </button>
        </div>
      </div>

      {/* Content */}
      <div className="max-w-4xl mx-auto px-6 py-8 space-y-8">
        {/* Appearance */}
        <section>
          <h2
            className={`text-lg font-semibold mb-4 ${
              darkMode ? "text-gray-100" : "text-gray-900"
            }`}
          >
            Appearance
          </h2>
          <div
            className={`rounded-lg border p-6 ${
              darkMode
                ? "bg-gray-950 border-gray-800"
                : "bg-white border-gray-200"
            }`}
          >
            <div className="flex items-center justify-between">
              <div>
                <div
                  className={`font-medium ${darkMode ? "text-gray-200" : "text-gray-900"}`}
                >
                  Theme
                </div>
                <div
                  className={`text-sm mt-1 ${darkMode ? "text-gray-400" : "text-gray-500"}`}
                >
                  Choose your preferred color scheme
                </div>
              </div>
              <button
                onClick={toggleDarkMode}
                className={`px-4 py-2 rounded-lg flex items-center gap-2 transition-colors ${
                  darkMode
                    ? "bg-gray-800 hover:bg-gray-700 text-gray-200"
                    : "bg-gray-100 hover:bg-gray-200 text-gray-800"
                }`}
              >
                {darkMode ? <Sun size={16} /> : <Moon size={16} />}
                <span>{darkMode ? "Light" : "Dark"}</span>
              </button>
            </div>
          </div>
        </section>

        {/* Privacy */}
        <section>
          <h2
            className={`text-lg font-semibold mb-4 ${
              darkMode ? "text-gray-100" : "text-gray-900"
            }`}
          >
            Privacy
          </h2>
          <div
            className={`rounded-lg border p-6 ${
              darkMode
                ? "bg-gray-950 border-gray-800"
                : "bg-white border-gray-200"
            }`}
          >
            <div className="flex items-start justify-between">
              <div className="flex-1">
                <div
                  className={`font-medium flex items-center gap-2 ${
                    darkMode ? "text-gray-200" : "text-gray-900"
                  }`}
                >
                  <Shield size={16} className="text-blue-500" />
                  Differential Privacy
                </div>
                <div
                  className={`text-sm mt-1 ${darkMode ? "text-gray-400" : "text-gray-500"}`}
                >
                  Epsilon value for privacy protection (lower = more privacy)
                </div>
                <div className="mt-4 flex items-center gap-4">
                  <input
                    type="range"
                    min={0.1}
                    max={5}
                    step={0.1}
                    value={dpEpsilon}
                    onChange={(e) => setDp(parseFloat(e.target.value))}
                    className={`flex-1 h-2 rounded-lg appearance-none cursor-pointer ${
                      darkMode
                        ? "bg-gray-700 [&::-webkit-slider-thumb]:bg-blue-500"
                        : "bg-gray-200 [&::-webkit-slider-thumb]:bg-blue-600"
                    } [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:h-4 [&::-webkit-slider-thumb]:w-4 [&::-webkit-slider-thumb]:rounded-full`}
                  />
                  <div
                    className={`w-12 text-right font-mono text-sm ${
                      darkMode ? "text-gray-300" : "text-gray-700"
                    }`}
                  >
                    {dpEpsilon.toFixed(1)}
                  </div>
                </div>
              </div>
            </div>
          </div>
        </section>

        {/* Statistics */}
        <section>
          <h2
            className={`text-lg font-semibold mb-4 ${
              darkMode ? "text-gray-100" : "text-gray-900"
            }`}
          >
            Statistics
          </h2>
          <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
            <div
              className={`rounded-lg border p-6 ${
                darkMode
                  ? "bg-gray-950 border-gray-800"
                  : "bg-white border-gray-200"
              }`}
            >
              <div className="flex items-center gap-3 mb-2">
                <Database size={20} className="text-blue-500" />
                <div
                  className={`text-2xl font-bold ${darkMode ? "text-gray-100" : "text-gray-900"}`}
                >
                  {tables.length}
                </div>
              </div>
              <div
                className={`text-sm ${darkMode ? "text-gray-400" : "text-gray-500"}`}
              >
                Database Tables
              </div>
            </div>

            <div
              className={`rounded-lg border p-6 ${
                darkMode
                  ? "bg-gray-950 border-gray-800"
                  : "bg-white border-gray-200"
              }`}
            >
              <div className="flex items-center gap-3 mb-2">
                <Star size={20} className="text-yellow-500" />
                <div
                  className={`text-2xl font-bold ${darkMode ? "text-gray-100" : "text-gray-900"}`}
                >
                  {favorites.length}
                </div>
              </div>
              <div
                className={`text-sm ${darkMode ? "text-gray-400" : "text-gray-500"}`}
              >
                Favorite Queries
              </div>
            </div>

            <div
              className={`rounded-lg border p-6 ${
                darkMode
                  ? "bg-gray-950 border-gray-800"
                  : "bg-white border-gray-200"
              }`}
            >
              <div className="flex items-center gap-3 mb-2">
                <Activity size={20} className="text-green-500" />
                <div
                  className={`text-2xl font-bold ${darkMode ? "text-gray-100" : "text-gray-900"}`}
                >
                  {history.length}
                </div>
              </div>
              <div
                className={`text-sm ${darkMode ? "text-gray-400" : "text-gray-500"}`}
              >
                Query History
              </div>
            </div>
          </div>
        </section>

        {/* About */}
        <section>
          <h2
            className={`text-lg font-semibold mb-4 ${
              darkMode ? "text-gray-100" : "text-gray-900"
            }`}
          >
            About
          </h2>
          <div
            className={`rounded-lg border p-6 ${
              darkMode
                ? "bg-gray-950 border-gray-800"
                : "bg-white border-gray-200"
            }`}
          >
            <div className="flex items-start gap-4">
              <div
                className={`p-3 rounded-lg ${
                  darkMode ? "bg-gray-900" : "bg-gray-50"
                }`}
              >
                <Zap size={24} className="text-blue-500" />
              </div>
              <div className="flex-1">
                <div
                  className={`font-semibold text-lg mb-1 ${
                    darkMode ? "text-gray-100" : "text-gray-900"
                  }`}
                >
                  LatticeDB Studio
                </div>
                <div
                  className={`text-sm ${darkMode ? "text-gray-400" : "text-gray-500"}`}
                >
                  Version 1.0.0
                </div>
                <div
                  className={`text-sm mt-3 ${darkMode ? "text-gray-400" : "text-gray-600"}`}
                >
                  A modern database management interface with support for
                  temporal queries, differential privacy, and vector operations.
                </div>
                <div
                  className={`text-xs mt-4 ${darkMode ? "text-gray-500" : "text-gray-400"}`}
                >
                  Built with React, TypeScript, Vite, and Tailwind CSS
                </div>
              </div>
            </div>
          </div>
        </section>
      </div>
    </div>
  );
}
