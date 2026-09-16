export interface ChatMessage {
  sender: string
  content: string
  created_at: string
}

// In dev, Vite proxies `/api` -> http://localhost:8080 (see vite.config.ts).
// In prod, set VITE_API_BASE_URL, e.g. VITE_API_BASE_URL=http://server:8080
const API_BASE = (import.meta.env.VITE_API_BASE_URL as string | undefined)?.replace(/\/$/, '') ?? ''

export async function fetchMessages(): Promise<ChatMessage[]> {
  const res = await fetch(`${API_BASE}/api/messages`)
  if (!res.ok) throw new Error(`Failed to load messages (${res.status})`)
  const data = await res.json()
  if (!Array.isArray(data)) return []
  return data.filter(
    (m): m is ChatMessage =>
      typeof m?.sender === 'string' && 
      typeof m?.content === 'string' && 
      typeof m?.created_at === 'string',
  )
}

export async function sendMessage(sender: string, content: string): Promise<void> {
  const res = await fetch(`${API_BASE}/api/messages`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ sender, content }),
  })
  if (!res.ok) throw new Error(`Failed to send message (${res.status})`)
}
