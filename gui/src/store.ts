// gui/src/store.ts
import { create } from 'zustand'

export type QueryResult = { ok: boolean; message: string; headers: string[]; rows: any[][] }

type State = {
  sql: string
  executing: boolean
  result?: QueryResult
  history: { ts: number; sql: string; result?: QueryResult }[]
  dpEpsilon: number
  txAsOf?: number
  setSql: (s: string) => void
  run: (sql?: string) => Promise<void>
  setDp: (e: number) => void
  setTx: (tx?: number) => void
}

export const useStore = create<State>((set, get) => ({
  sql: "SELECT 1;",
  executing: false,
  history: [],
  dpEpsilon: 1.0,
  txAsOf: undefined,
  setSql: (s) => set({ sql: s }),
  run: async (sql?: string) => {
    const state = get()
    const payload = { sql: (sql ?? state.sql).trim() + (state.dpEpsilon ? `\nSET DP_EPSILON = ${state.dpEpsilon};` : "") }
    set({ executing: true })
    try {
      const r = await fetch("http://127.0.0.1:7070/query", {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload)
      })
      const j = (await r.json()) as QueryResult
      set(s => ({ result: j, history: [{ ts: Date.now(), sql: payload.sql, result: j }, ...s.history].slice(0, 100) }))
    } catch (e:any) {
      set(s => ({ result: { ok: false, message: e?.message ?? "request failed", headers: [], rows: [] }, history: [{ ts: Date.now(), sql: payload.sql }, ...s.history]}))
    } finally {
      set({ executing: false })
    }
  },
  setDp: (e) => set({ dpEpsilon: e }),
  setTx: (tx) => set({ txAsOf: tx }),
}))
