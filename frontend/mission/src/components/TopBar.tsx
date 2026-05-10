import { useEffect, useState } from 'react'
import { cn, formatUptime } from '@/lib/utils'
import type { ConnectionState, TelemetryPacket } from '@/types'

interface TopBarProps {
  packet: TelemetryPacket | null
  state: ConnectionState
  packetCount: number
}

const STATE_LABEL: Record<ConnectionState, { text: string; color: string }> = {
  connecting: { text: 'LINK · ACQUIRING', color: 'text-hud-amber' },
  open: { text: 'LINK · ONLINE', color: 'text-hud-green' },
  closed: { text: 'LINK · LOST', color: 'text-hud-red' },
  error: { text: 'LINK · ERROR', color: 'text-hud-red' },
}

const STATUS_LABEL: Record<number, { text: string; color: string }> = {
  0: { text: 'NOMINAL', color: 'text-hud-green' },
  1: { text: 'WARNING', color: 'text-hud-amber' },
  2: { text: 'ERROR', color: 'text-hud-red' },
}

export function TopBar({ packet, state, packetCount }: TopBarProps) {
  const [now, setNow] = useState(() => Date.now())
  useEffect(() => {
    const i = window.setInterval(() => setNow(Date.now()), 1000)
    return () => window.clearInterval(i)
  }, [])

  const link = STATE_LABEL[state]
  const status = packet ? STATUS_LABEL[packet.system_status] ?? STATUS_LABEL[0] : null
  const battery = packet?.battery_voltage ?? 0
  const batteryColor =
    battery >= 7.5 ? 'text-hud-green' : battery >= 6.5 ? 'text-hud-amber' : 'text-hud-red'

  return (
    <header className="relative grid grid-cols-[1fr_auto_1fr] items-center border-b border-hud-border bg-hud-panel/70 px-4 py-2">
      {/* LEFT */}
      <div className="flex items-center gap-6 text-[11px] uppercase tracking-[0.2em] text-hud-fg-dim">
        <span className="flex items-center gap-2">
          <span className={cn('h-2 w-2 rounded-full', state === 'open' ? 'bg-hud-green hud-pulse' : 'bg-hud-red')} />
          <span className={cn('font-semibold', link.color)}>{link.text}</span>
        </span>
        <span>
          PACKETS · <span className="text-hud-fg">{packetCount.toString().padStart(6, '0')}</span>
        </span>
        <span>
          T+ <span className="text-hud-fg">{packet ? formatUptime(packet.timestamp_ms) : '00:00:00'}</span>
        </span>
      </div>

      {/* CENTER */}
      <div className="flex items-baseline gap-3">
        <span className="text-hud-cyan text-[12px] tracking-[0.4em]">▮▮ SATSIM</span>
        <h1 className="text-base font-bold uppercase tracking-[0.45em] text-hud-fg">
          Mission Control
        </h1>
        <span className="text-hud-cyan text-[12px] tracking-[0.4em]">▮▮</span>
      </div>

      {/* RIGHT */}
      <div className="flex items-center justify-end gap-6 text-[11px] uppercase tracking-[0.2em] text-hud-fg-dim">
        <span>
          STATUS ·{' '}
          {status ? <span className={cn('font-semibold', status.color)}>{status.text}</span> : <span className="text-hud-fg-dim">—</span>}
        </span>
        <span>
          BATT · <span className={cn('font-semibold', batteryColor)}>{battery.toFixed(2)}V</span>
        </span>
        <span className="font-semibold text-hud-fg">
          {new Date(now).toISOString().slice(11, 19)} UTC
        </span>
      </div>
    </header>
  )
}
