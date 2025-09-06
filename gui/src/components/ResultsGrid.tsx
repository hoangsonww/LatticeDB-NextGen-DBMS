import { useMemo } from "react"
import { useStore } from "../store"

function cell(v: any) {
  if (v === null || v === undefined) return <span className="text-gray-400">NULL</span>
  if (Array.isArray(v)) return <span>[{v.join(", ")}]</span>
  if (typeof v === "string") return <span className="text-ink">'{v}'</span>
  return <span>{String(v)}</span>
}

export default function ResultGrid() {
  const result = useStore(s => s.result)
  const empty = !result || (result.headers.length===0 && result.rows.length===0)
  const rows = useMemo(() => result?.rows ?? [], [result])
  return (
    <div className="card p-3 overflow-auto h-[340px]">
      {empty ? (
        <div className="h-full grid place-items-center text-gray-400">Run a query to see results</div>
      ) : (
        <>
          <div className="text-sm text-gray-500 mb-2">{result?.ok ? result?.message || "OK" : "Error"}</div>
          <table className="w-full text-sm">
            {result?.headers?.length ? (
              <thead>
              <tr>
                {result.headers.map((h, i) => (
                  <th key={i} className="text-left font-semibold py-2 border-b border-gray-200">{h}</th>
                ))}
              </tr>
              </thead>
            ) : null}
            <tbody>
            {rows.map((r, i) => (
              <tr key={i} className="odd:bg-gray-50">
                {r.map((c, j) => (
                  <td key={j} className="py-1 pr-3 align-top">{cell(c)}</td>
                ))}
              </tr>
            ))}
            </tbody>
          </table>
        </>
      )}
    </div>
  )
}
