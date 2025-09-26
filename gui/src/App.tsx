import { useEffect } from "react";
import Toolbar from "./components/Toolbar";
import SqlEditor from "./components/Editor";
import ResultsGrid from "./components/ResultsGrid";
import Controls from "./components/Controls";
import HistoryPanel from "./components/HistoryPanel";
import SidePanel from "./components/SidePanel";
import { useStore } from "./store";

export default function App() {
  const { darkMode, mockMode } = useStore((s) => ({
    darkMode: s.darkMode,
    mockMode: s.mockMode,
  }));

  useEffect(() => {
    /* warm health */
  }, []);

  return (
    <div
      className={`min-h-screen transition-colors duration-300 ${
        darkMode
          ? "bg-black text-gray-100"
          : "bg-gradient-to-br from-slate-50 via-blue-50 to-indigo-50 text-gray-900"
      }`}
    >
      <header
        className={`sticky top-0 z-20 backdrop-blur-md border-b transition-all duration-300 ${
          darkMode
            ? "bg-black/95 border-gray-800"
            : "bg-white/80 border-gray-200"
        }`}
      >
        <div className="max-w-7xl mx-auto">
          <Toolbar />
        </div>
      </header>

      <main className="max-w-full mx-auto p-4 grid lg:grid-cols-[22rem_1fr] gap-4 min-h-[calc(100vh-8rem)]">
        <aside className="lg:block hidden">
          <div className="sticky top-24 space-y-4">
            <SidePanel />
          </div>
        </aside>

        <div className="flex flex-col gap-3 min-w-0 overflow-hidden">
          <SqlEditor />
          <Controls />

          <ResultsGrid />
          <HistoryPanel />
        </div>
      </main>

      <footer
        className={`text-center text-xs py-6 border-t transition-colors duration-300 ${
          darkMode
            ? "text-gray-400 border-gray-800 bg-black"
            : "text-gray-500 border-gray-200 bg-white/50"
        }`}
      >
        <div className="flex items-center justify-center gap-2">
          <span>LatticeDB Studio</span>
          <span className="text-gray-400">•</span>
          <span className={mockMode ? "text-blue-500" : "text-green-500"}>
            {mockMode ? "Mock Mode" : "localhost:7070"}
          </span>
        </div>
      </footer>
    </div>
  );
}
