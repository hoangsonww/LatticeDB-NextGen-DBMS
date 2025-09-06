import { useStore } from "../store"
import { Clock, RotateCcw } from "lucide-react"

export default function HistoryPanel() {
  const history = useStore(s => s.history)
  const setSql = useStore(s => s.setSql)
  return (
    <div className="card p-3">
      <div className="flex items-center gap-2 mb-2">
        <Clock size={16}/> <div className="font-semibold">History</div>
      </div>
      <div className="max-h-48 overflow-auto text-sm">
        {history.length===0 ? <div className="text-gray-400">No history yet.</div> :
          history.map((h,i)=>(
            <div key={i} className="flex items-start gap-2 py-2 border-b last:border-b-0">
              <button className="tab" onClick={()=>setSql(h.sql)}><RotateCcw size={14}/></button>
              <div className="flex-1">
                <div className="text-gray-500 text-xs">{new Date(h.ts).toLocaleString()}</div>
                <pre className="whitespace-pre-wrap">{h.sql}</pre>
              </div>
            </div>
          ))
        }
      </div>
    </div>
  )
}
