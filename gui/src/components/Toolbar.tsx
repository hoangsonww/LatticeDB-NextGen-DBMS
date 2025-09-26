import {
  Play,
  Save,
  FolderOpen,
  Database,
  Moon,
  Sun,
  Zap,
  Star,
  Settings,
} from "lucide-react";
import { useStore } from "../store";
import { useNavigate } from "react-router-dom";

export default function Toolbar() {
  const { run, executing, darkMode, toggleDarkMode, favorites, sql, setSql } =
    useStore((s) => ({
      run: s.run,
      executing: s.executing,
      darkMode: s.darkMode,
      toggleDarkMode: s.toggleDarkMode,
      favorites: s.favorites,
      sql: s.sql,
      setSql: s.setSql,
    }));

  const navigate = useNavigate();

  const handleOpen = () => {
    const input = document.createElement("input");
    input.type = "file";
    input.accept = ".sql,.txt";
    input.onchange = (e) => {
      const file = (e.target as HTMLInputElement).files?.[0];
      if (file) {
        const reader = new FileReader();
        reader.onload = (e) => {
          const content = e.target?.result as string;
          setSql(content);
        };
        reader.readAsText(file);
      }
    };
    input.click();
  };

  const handleSave = () => {
    if (!sql.trim()) return;
    const blob = new Blob([sql], { type: "text/sql" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = "query.sql";
    a.click();
    URL.revokeObjectURL(url);
  };

  return (
    <div
      className={`flex items-center gap-3 p-4 transition-colors duration-300 ${
        darkMode ? "text-gray-100" : "text-gray-900"
      }`}
    >
      {/* Main Actions */}
      <div className="flex items-center gap-2">
        <button
          className={`inline-flex items-center gap-2 px-4 py-2 rounded-lg font-medium transition-all duration-200 transform hover:scale-105 ${
            executing
              ? "bg-orange-500 text-white cursor-not-allowed opacity-75"
              : darkMode
                ? "bg-blue-600 hover:bg-blue-700 text-white shadow-lg hover:shadow-blue-500/20"
                : "bg-blue-600 hover:bg-blue-700 text-white shadow-lg hover:shadow-blue-500/20"
          }`}
          onClick={() => run()}
          disabled={executing}
        >
          {executing ? (
            <>
              <div className="animate-spin w-4 h-4 border-2 border-white/30 border-t-white rounded-full" />
              Executing...
            </>
          ) : (
            <>
              <Play size={16} />
              Run Query
            </>
          )}
        </button>

        <div className="hidden sm:flex items-center gap-1 ml-2">
          <button
            className={`p-2 rounded-lg transition-all duration-200 hover:scale-105 ${
              darkMode
                ? "hover:bg-gray-700 text-gray-300 hover:text-white"
                : "hover:bg-gray-100 text-gray-600 hover:text-gray-900"
            }`}
            onClick={handleOpen}
            title="Open File"
          >
            <FolderOpen size={18} />
          </button>

          <button
            className={`p-2 rounded-lg transition-all duration-200 hover:scale-105 ${
              darkMode
                ? "hover:bg-gray-700 text-gray-300 hover:text-white"
                : "hover:bg-gray-100 text-gray-600 hover:text-gray-900"
            }`}
            onClick={handleSave}
            title="Save File"
          >
            <Save size={18} />
          </button>
        </div>
      </div>

      <div className="flex-1" />

      {/* Status & Info */}
      <div className="hidden lg:flex items-center gap-4 text-sm">
        <div
          className={`flex items-center gap-2 px-3 py-1.5 rounded-full ${
            darkMode ? "bg-gray-800 text-gray-300" : "bg-white/60 text-gray-600"
          }`}
        >
          <Database size={16} className="text-blue-500" />
          <span className="font-medium">LatticeDB Studio</span>
        </div>

        {favorites.length > 0 && (
          <div
            className={`flex items-center gap-2 px-3 py-1.5 rounded-full ${
              darkMode
                ? "bg-gray-800 text-gray-300"
                : "bg-white/60 text-gray-600"
            }`}
          >
            <Star size={14} className="text-yellow-500" />
            <span>{favorites.length} saved</span>
          </div>
        )}

        <div
          className={`flex items-center gap-2 px-3 py-1.5 rounded-full ${
            darkMode ? "bg-gray-800 text-gray-300" : "bg-white/60 text-gray-600"
          }`}
        >
          <Zap size={14} className="text-green-500" />
          <span>Ready</span>
        </div>
      </div>

      {/* Controls */}
      <div className="flex items-center gap-2">
        <button
          onClick={toggleDarkMode}
          className={`p-2.5 rounded-lg transition-all duration-200 hover:scale-105 ${
            darkMode
              ? "bg-yellow-500/10 hover:bg-yellow-500/20 text-yellow-400 hover:text-yellow-300"
              : "bg-gray-200/60 hover:bg-gray-300/60 text-gray-600 hover:text-gray-800"
          }`}
          title={darkMode ? "Switch to light mode" : "Switch to dark mode"}
        >
          {darkMode ? <Sun size={18} /> : <Moon size={18} />}
        </button>

        <button
          onClick={() => navigate("/settings")}
          className={`p-2.5 rounded-lg transition-all duration-200 hover:scale-105 ${
            darkMode
              ? "hover:bg-gray-700 text-gray-400 hover:text-gray-200"
              : "bg-gray-200/60 hover:bg-gray-300/60 text-gray-600 hover:text-gray-800"
          }`}
          title="Settings"
        >
          <Settings size={18} />
        </button>
      </div>
    </div>
  );
}
