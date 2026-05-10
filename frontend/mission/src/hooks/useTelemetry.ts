import { useEffect, useRef, useState } from 'react'
import type { ConnectionState, TelemetryPacket } from '@/types'

const WS_URL = import.meta.env.VITE_WS_URL ?? 'ws://localhost:8000/ws/telemetry'
const RECONNECT_DELAY_MS = 2000

export interface TelemetryStream {
  latest: TelemetryPacket | null
  history: TelemetryPacket[]
  state: ConnectionState
  packetCount: number
}

export function useTelemetry(historySize = 120): TelemetryStream {
  const [latest, setLatest] = useState<TelemetryPacket | null>(null)
  const [state, setState] = useState<ConnectionState>('connecting')
  const [packetCount, setPacketCount] = useState(0)
  const historyRef = useRef<TelemetryPacket[]>([])
  const [, force] = useState(0)

  useEffect(() => {
    let ws: WebSocket | null = null
    let reconnectTimer: number | null = null
    let cancelled = false

    const connect = () => {
      if (cancelled) return
      setState('connecting')
      ws = new WebSocket(WS_URL)

      ws.onopen = () => setState('open')

      ws.onmessage = (event) => {
        try {
          const packet = JSON.parse(event.data) as TelemetryPacket
          setLatest(packet)
          setPacketCount((n) => n + 1)
          const next = [...historyRef.current, packet]
          if (next.length > historySize) next.splice(0, next.length - historySize)
          historyRef.current = next
          force((n) => n + 1)
        } catch (err) {
          console.warn('[ws] parse error', err)
        }
      }

      ws.onerror = () => setState('error')

      ws.onclose = () => {
        setState('closed')
        if (!cancelled) {
          reconnectTimer = window.setTimeout(connect, RECONNECT_DELAY_MS)
        }
      }
    }

    connect()

    return () => {
      cancelled = true
      if (reconnectTimer) window.clearTimeout(reconnectTimer)
      ws?.close()
    }
  }, [historySize])

  return {
    latest,
    history: historyRef.current,
    state,
    packetCount,
  }
}
