import { useEffect, useRef, useState, useCallback } from "react"

interface WebSocketState {
  lastMessage: string | null
  isConnected: boolean
}

const MAX_RECONNECT_DELAY = 30_000
const BASE_RECONNECT_DELAY = 1_000

export function useWebSocket(url: string): WebSocketState {
  const [lastMessage, setLastMessage] = useState<string | null>(null)
  const [isConnected, setIsConnected] = useState(false)
  const wsRef = useRef<WebSocket | null>(null)
  const reconnectAttemptRef = useRef(0)
  const reconnectTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null)

  const connect = useCallback(() => {
    if (
      wsRef.current?.readyState === WebSocket.OPEN ||
      wsRef.current?.readyState === WebSocket.CONNECTING
    ) return

    const protocol = window.location.protocol === "https:" ? "wss:" : "ws:"
    const wsUrl = `${protocol}//${window.location.host}${url}`
    const ws = new WebSocket(wsUrl)

    ws.onopen = () => {
      setIsConnected(true)
      reconnectAttemptRef.current = 0
    }

    ws.onmessage = (event: MessageEvent) => {
      setLastMessage(String(event.data))
    }

    ws.onclose = () => {
      setIsConnected(false)
      wsRef.current = null

      const delay = Math.min(
        BASE_RECONNECT_DELAY * Math.pow(2, reconnectAttemptRef.current),
        MAX_RECONNECT_DELAY,
      )
      reconnectAttemptRef.current += 1
      reconnectTimerRef.current = setTimeout(connect, delay)
    }

    ws.onerror = () => {
      ws.close()
    }

    wsRef.current = ws
  }, [url])

  useEffect(() => {
    connect()

    return () => {
      if (reconnectTimerRef.current) {
        clearTimeout(reconnectTimerRef.current)
      }
      if (wsRef.current) {
        wsRef.current.onclose = null
        wsRef.current.close()
      }
    }
  }, [connect])

  return { lastMessage, isConnected }
}
