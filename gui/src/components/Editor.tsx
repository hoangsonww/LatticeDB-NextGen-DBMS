import Editor from "@monaco-editor/react";
import { useEffect, useRef, useState } from "react";
import { useStore } from "../store";
import { Code, Star, StarOff, Maximize2, Minimize2 } from "lucide-react";

export default function SqlEditor() {
  const {
    sql,
    setSql,
    run,
    darkMode,
    favorites,
    addToFavorites,
    removeFromFavorites,
  } = useStore();
  const ref = useRef<any>();
  const [isFullscreen, setIsFullscreen] = useState(false);
  const [lines, setLines] = useState(1);

  const isFavorited = favorites.includes(sql.trim());

  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "enter") {
        e.preventDefault();
        run();
      }
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "m") {
        e.preventDefault();
        setIsFullscreen((prev) => !prev);
      }
    };
    window.addEventListener("keydown", handler);
    return () => window.removeEventListener("keydown", handler);
  }, [run]);

  useEffect(() => {
    setLines(sql.split("\n").length);
  }, [sql]);

  const toggleFavorite = () => {
    if (isFavorited) {
      removeFromFavorites(sql.trim());
    } else {
      addToFavorites(sql.trim());
    }
  };

  return (
    <div
      className={`card transition-all duration-300 flex flex-col overflow-hidden ${
        isFullscreen ? "fixed inset-4 z-50 h-[calc(100vh-2rem)]" : "h-[200px]"
      } ${darkMode ? "bg-gray-950 border-gray-800" : "bg-white border-gray-200"}`}
    >
      {/* Editor Header */}
      <div
        className={`flex items-center justify-between px-4 py-2 border-b transition-colors duration-300 ${
          darkMode ? "border-gray-800 bg-black" : "border-gray-200 bg-gray-50"
        }`}
      >
        <div className="flex items-center gap-3">
          <div
            className={`p-1 rounded ${darkMode ? "bg-gray-900" : "bg-white"}`}
          >
            <Code size={14} className="text-blue-500" />
          </div>
          <div>
            <h3
              className={`font-medium text-sm ${darkMode ? "text-gray-200" : "text-gray-900"}`}
            >
              SQL Editor
            </h3>
            <p
              className={`text-xs ${darkMode ? "text-gray-400" : "text-gray-500"}`}
            >
              {lines} lines • Ctrl+Enter to run
            </p>
          </div>
        </div>

        <div className="flex items-center gap-2">
          {sql.trim() && (
            <button
              onClick={toggleFavorite}
              className={`p-2 rounded-lg transition-all duration-200 hover:scale-105 ${
                isFavorited
                  ? "text-yellow-500 hover:text-yellow-400"
                  : darkMode
                    ? "text-gray-400 hover:text-yellow-400 hover:bg-gray-700"
                    : "text-gray-400 hover:text-yellow-500 hover:bg-gray-100"
              }`}
              title={isFavorited ? "Remove from favorites" : "Add to favorites"}
            >
              {isFavorited ? (
                <Star size={16} fill="currentColor" />
              ) : (
                <Star size={16} />
              )}
            </button>
          )}

          <button
            onClick={() => setIsFullscreen((prev) => !prev)}
            className={`p-2 rounded-lg transition-all duration-200 hover:scale-105 ${
              darkMode
                ? "text-gray-400 hover:text-gray-200 hover:bg-gray-700"
                : "text-gray-500 hover:text-gray-700 hover:bg-gray-100"
            }`}
            title={
              isFullscreen ? "Exit fullscreen (Ctrl+M)" : "Fullscreen (Ctrl+M)"
            }
          >
            {isFullscreen ? <Minimize2 size={16} /> : <Maximize2 size={16} />}
          </button>
        </div>
      </div>

      {/* Editor */}
      <div className="flex-1 overflow-hidden">
        <Editor
          onMount={(editor) => (ref.current = editor)}
          defaultLanguage="sql"
          value={sql}
          onChange={(v) => setSql(v || "")}
          theme={darkMode ? "vs-dark" : "light"}
          options={{
            minimap: { enabled: isFullscreen },
            fontSize: 14,
            lineNumbers: "on",
            roundedSelection: true,
            scrollBeyondLastLine: false,
            automaticLayout: true,
            padding: { top: 16, bottom: 16 },
            suggestOnTriggerCharacters: true,
            acceptSuggestionOnEnter: "on",
            tabCompletion: "on",
            // @ts-ignore
            wordBasedSuggestions: true,
            contextmenu: true,
            mouseWheelZoom: true,
            smoothScrolling: true,
            cursorBlinking: "smooth",
            // @ts-ignore
            cursorSmoothCaretAnimation: true,
            fontLigatures: true,
            bracketPairColorization: { enabled: true },
            guides: {
              bracketPairs: true,
              indentation: true,
            },
            renderWhitespace: "selection",
            renderLineHighlight: "line",
            selectOnLineNumbers: true,
            glyphMargin: true,
            folding: true,
            foldingHighlight: true,
            showFoldingControls: "always",
          }}
          height="100%"
        />
      </div>
    </div>
  );
}
