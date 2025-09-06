import Editor from "@monaco-editor/react"
import { useEffect, useRef } from "react"
import { useStore } from "../store"

export default function SqlEditor() {
  const sql = useStore(s => s.sql)
  const setSql = useStore(s => s.setSql)
  const run = useStore(s => s.run)
  const ref = useRef<any>()
  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "enter") {
        e.preventDefault(); run()
      }
    }
    window.addEventListener("keydown", handler)
    return () => window.removeEventListener("keydown", handler)
  }, [run])
  return (
    <div className="card p-2 h-[320px]">
      <Editor
        onMount={(editor) => (ref.current = editor)}
        defaultLanguage="sql"
        value={sql}
        onChange={(v) => setSql(v || "")}
        options={{
          minimap: { enabled: false },
          fontSize: 14,
          lineNumbers: "on",
          roundedSelection: true,
          scrollBeyondLastLine: false,
          automaticLayout: true
        }}
        height="100%"
      />
    </div>
  )
}
