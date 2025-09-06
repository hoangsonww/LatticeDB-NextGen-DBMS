import { Play, Save, FolderOpen, Database, History, Wand2 } from "lucide-react"
import { useStore } from "../store"

export default function Toolbar({ onOpen, onSave }: { onOpen: () => void; onSave: () => void }) {
  const run = useStore(s => s.run)
  return (
    <div className="flex items-center gap-2 p-2">
      <button className="btn" onClick={() => run()}><Play size={16}/> Run (Ctrl+Enter)</button>
      <button className="tab" onClick={onOpen}><FolderOpen size={16}/> Open</button>
      <button className="tab" onClick={onSave}><Save size={16}/> Save</button>
      <div className="ml-auto flex items-center gap-2">
        <span className="text-gray-500 text-sm hidden md:flex items-center gap-1"><Database size={16}/> LatticeDB Studio</span>
        <span className="text-gray-400 text-sm hidden md:flex items-center gap-1"><History size={16}/> History</span>
        <span className="text-gray-400 text-sm hidden md:flex items-center gap-1"><Wand2 size={16}/> Smart Hints</span>
      </div>
    </div>
  )
}
