import { useStore } from "./store";
import {
  Moon,
  Sun,
  Shield,
  Database,
  Star,
  Activity,
  ArrowLeft,
} from "lucide-react";
import { useEffect } from "react";
import { useNavigate } from "react-router-dom";

export default function Settings() {
  const {
    darkMode,
    toggleDarkMode,
    favorites,
    history,
    tables,
    dpEpsilon,
    setDp,
  } = useStore();
  const navigate = useNavigate();

  useEffect(() => {
    document.title = "Settings - LatticeDB Studio";
  }, []);

  return (
    <div
      className={`min-h-screen ${darkMode ? "bg-black text-gray-100" : "bg-white text-gray-900"}`}
    >
      <header
        className={`border-b ${darkMode ? "bg-black border-gray-800" : "bg-white border-gray-200"}`}
      >
        <div className="max-w-6xl mx-auto px-6 py-4 flex items-center gap-4">
          <button
            onClick={() => navigate("/")}
            className={`p-2 rounded hover:bg-opacity-10 hover:bg-gray-500`}
          >
            <ArrowLeft size={20} />
          </button>
          <h1 className="text-2xl font-bold">Settings</h1>
        </div>
      </header>

      <main className="max-w-6xl mx-auto px-6 py-8">
        <div className="space-y-12">
          {/* Theme */}
          <section>
            <h2 className="text-xl font-semibold mb-6">Theme</h2>
            <div className="flex items-center justify-between py-4">
              <div>
                <div className="font-medium">Appearance</div>
                <div
                  className={`text-sm mt-1 ${darkMode ? "text-gray-400" : "text-gray-600"}`}
                >
                  Choose between light and dark theme
                </div>
              </div>
              <button
                onClick={toggleDarkMode}
                className={`px-6 py-3 rounded-lg flex items-center gap-3 font-medium ${
                  darkMode
                    ? "bg-gray-900 hover:bg-gray-800 text-gray-200"
                    : "bg-gray-100 hover:bg-gray-200 text-gray-800"
                }`}
              >
                {darkMode ? <Sun size={18} /> : <Moon size={18} />}
                <span>Switch to {darkMode ? "Light" : "Dark"} Mode</span>
              </button>
            </div>
          </section>

          <hr className={darkMode ? "border-gray-800" : "border-gray-200"} />

          {/* Privacy */}
          <section>
            <h2 className="text-xl font-semibold mb-6">Privacy</h2>
            <div className="py-4">
              <div className="flex items-center gap-3 mb-3">
                <Shield size={20} className="text-blue-500" />
                <span className="font-medium">Differential Privacy (ε)</span>
              </div>
              <div
                className={`text-sm mb-4 ${darkMode ? "text-gray-400" : "text-gray-600"}`}
              >
                Lower values provide stronger privacy guarantees but may reduce
                accuracy
              </div>
              <div className="flex items-center gap-6">
                <span
                  className={`text-sm ${darkMode ? "text-gray-500" : "text-gray-500"}`}
                >
                  0.1 (High Privacy)
                </span>
                <input
                  type="range"
                  min={0.1}
                  max={5}
                  step={0.1}
                  value={dpEpsilon}
                  onChange={(e) => setDp(parseFloat(e.target.value))}
                  className={`flex-1 h-2 rounded-lg appearance-none cursor-pointer ${
                    darkMode
                      ? "bg-gray-800 [&::-webkit-slider-thumb]:bg-blue-500"
                      : "bg-gray-200 [&::-webkit-slider-thumb]:bg-blue-600"
                  } [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:h-5 [&::-webkit-slider-thumb]:w-5 [&::-webkit-slider-thumb]:rounded-full`}
                />
                <span
                  className={`text-sm ${darkMode ? "text-gray-500" : "text-gray-500"}`}
                >
                  5.0 (Low Privacy)
                </span>
                <div
                  className={`ml-4 px-3 py-1 rounded font-mono text-lg font-bold ${
                    darkMode
                      ? "bg-gray-900 text-blue-400"
                      : "bg-gray-100 text-blue-600"
                  }`}
                >
                  {dpEpsilon.toFixed(1)}
                </div>
              </div>
            </div>
          </section>

          <hr className={darkMode ? "border-gray-800" : "border-gray-200"} />

          {/* Statistics */}
          <section>
            <h2 className="text-xl font-semibold mb-6">Usage Statistics</h2>
            <div className="grid grid-cols-3 gap-8">
              <div className="flex items-center gap-4">
                <Database size={32} className="text-blue-500" />
                <div>
                  <div className="text-3xl font-bold">{tables.length}</div>
                  <div
                    className={`text-sm ${darkMode ? "text-gray-400" : "text-gray-600"}`}
                  >
                    Database Tables
                  </div>
                </div>
              </div>
              <div className="flex items-center gap-4">
                <Star size={32} className="text-yellow-500" />
                <div>
                  <div className="text-3xl font-bold">{favorites.length}</div>
                  <div
                    className={`text-sm ${darkMode ? "text-gray-400" : "text-gray-600"}`}
                  >
                    Saved Queries
                  </div>
                </div>
              </div>
              <div className="flex items-center gap-4">
                <Activity size={32} className="text-green-500" />
                <div>
                  <div className="text-3xl font-bold">{history.length}</div>
                  <div
                    className={`text-sm ${darkMode ? "text-gray-400" : "text-gray-600"}`}
                  >
                    Query History
                  </div>
                </div>
              </div>
            </div>
          </section>

          <hr className={darkMode ? "border-gray-800" : "border-gray-200"} />

          {/* About */}
          <section>
            <h2 className="text-xl font-semibold mb-6">About</h2>
            <div className="py-4">
              <h3 className="text-2xl font-bold mb-2">LatticeDB Studio</h3>
              <p
                className={`mb-4 ${darkMode ? "text-gray-300" : "text-gray-700"}`}
              >
                Version 1.0.0
              </p>
              <p
                className={`mb-6 ${darkMode ? "text-gray-400" : "text-gray-600"}`}
              >
                A modern database management interface with support for temporal
                queries, differential privacy, and vector operations. Built for
                developers who need powerful database tools with a clean,
                intuitive interface.
              </p>
              <div
                className={`text-sm ${darkMode ? "text-gray-500" : "text-gray-500"}`}
              >
                © 2024 LatticeDB. Built with React, TypeScript, Vite, and
                Tailwind CSS.
              </div>
            </div>
          </section>
        </div>
      </main>
    </div>
  );
}
