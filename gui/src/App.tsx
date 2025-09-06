import { useEffect } from "react"
import Toolbar from "./components/Toolbar"
import SqlEditor from "./components/Editor"
import ResultGrid from "./components/ResultGrid"
import Controls from "./components/Controls"
import HistoryPanel from "./components/HistoryPanel"
import SidePanel from "./components/SidePanel"
import { useStore } from "./store"

export default function App() {
  const run = useStore(s => s.run)
  useEffect(() => { /* warm health */ }, [])
  return (
    <div className="min-h-full">
      <header className="sticky top-0 z-10 backdrop-blur bg-white/70 border-b border-gray-100">
        <div className="max-w-7xl mx-auto">
          <Toolbar onOpen={()=>{}} onSave={()=>{}}/>
        </div>
      </header>
      <main className="max-w-7xl mx-auto p-4 grid lg:grid-cols-[18rem_1fr] gap-4">
        <SidePanel/>
        <div className="flex flex-col gap-4">
          <SqlEditor/>
          <Controls/>
          <ResultGrid/>
          <HistoryPanel/>
        </div>
      </main>
      <footer className="text-center text-xs text-gray-400 py-4">LatticeDB Studio • localhost:7070</footer>
    </div>
  )
}
