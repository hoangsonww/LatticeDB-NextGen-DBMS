import { useStore } from "../store"

export default function Controls() {
  const dp = useStore(s => s.dpEpsilon)
  const setDp = useStore(s => s.setDp)
  const tx = useStore(s => s.txAsOf)
  const setTx = useStore(s => s.setTx)
  return (
    <div className="card p-3 grid md:grid-cols-3 gap-3">
      <div>
        <div className="text-xs text-gray-500 mb-1">Differential Privacy ε</div>
        <div className="flex items-center gap-3">
          <input className="w-full" type="range" min={0.1} max={5} step={0.1} value={dp} onChange={e => setDp(parseFloat(e.target.value))}/>
          <div className="w-16 text-right text-sm">{dp.toFixed(1)}</div>
        </div>
      </div>
      <div>
        <div className="text-xs text-gray-500 mb-1">Time Travel (TX as-of)</div>
        <div className="flex items-center gap-3">
          <input className="input" placeholder="e.g. 42" value={tx ?? ""} onChange={e => setTx(e.target.value? parseInt(e.target.value,10): undefined)}/>
        </div>
        <div className="text-xs text-gray-400 mt-1">Append <code>FOR SYSTEM_TIME AS OF TX {`<n>`}</code> to your query.</div>
      </div>
      <div>
        <div className="text-xs text-gray-500 mb-1">Vector Helper</div>
        <div className="text-sm text-gray-500">Use <code>DISTANCE(vec,[...]) &lt; τ</code> in WHERE.</div>
      </div>
    </div>
  )
}
