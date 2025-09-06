import { useEffect, useState } from "react"
import { health } from "../lib/api"
import { Database, SignalHigh, SignalLow } from "lucide-react"

export default function SidePanel() {
  const [ok, setOk] = useState(false)
  useEffect(() => { health().then(setOk) }, [])
  return (
    <aside className="w-72 hidden lg:flex flex-col gap-3">
      <div className="card p-3">
        <div className="flex items-center gap-2">
          <Database className="text-primary" size={18}/>
          <div className="font-semibold">Connection</div>
          <div className="ml-auto flex items-center gap-1 text-sm">
            {ok ? <SignalHigh className="text-emerald-500" size={16}/> : <SignalLow className="text-rose-500" size={16}/>}
            {ok ? "Healthy" : "Offline"}
          </div>
        </div>
        <div className="text-xs text-gray-500 mt-2">Server: http://127.0.0.1:7070</div>
      </div>
      <div className="card p-3">
        <div className="font-semibold mb-2">Schema</div>
        <div className="text-sm text-gray-500">Schema browsing will appear here.</div>
      </div>
    </aside>
  )
}
