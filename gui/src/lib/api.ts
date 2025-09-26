export async function health(): Promise<boolean> {
  try {
    const r = await fetch("http://127.0.0.1:7070/health");
    const j = await r.json();
    return !!j.ok;
  } catch {
    return false;
  }
}
