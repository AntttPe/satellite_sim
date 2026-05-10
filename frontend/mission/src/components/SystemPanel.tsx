import { Panel } from './Panel'
import { StatCell } from './StatCell'
import { cn, formatUptime } from '@/lib/utils'
import type { ConnectionState, TelemetryPacket } from '@/types'

interface SystemPanelProps {
  packet: TelemetryPacket | null
  state: ConnectionState
  packetCount: number
}

const TASKS = [
  { name: 'SensorTask', period: '100ms', prio: 2 },
  { name: 'OrbitTask', period: '1000ms', prio: 2 },
  { name: 'LaserTask', period: '500ms', prio: 2 },
  { name: 'TelemetryTask', period: 'event', prio: 1 },
  { name: 'SocketSender', period: '1000ms', prio: 1 },
  { name: 'WatchdogTask', period: '500ms', prio: 3 },
]

export function SystemPanel({ packet, state, packetCount }: SystemPanelProps) {
  const link = state === 'open'

  return (
    <Panel title="System Health" subtitle="FreeRTOS · POSIX" accent="green">
      <div className="grid grid-cols-2 gap-2 pb-3">
        <StatCell
          label="Uptime"
          value={packet ? formatUptime(packet.timestamp_ms) : '—'}
          accent="cyan"
        />
        <StatCell
          label="Packets"
          value={packetCount.toString()}
          accent="green"
        />
        <StatCell
          label="Battery"
          value={`${(packet?.battery_voltage ?? 0).toFixed(2)}V`}
          accent={
            (packet?.battery_voltage ?? 0) >= 7.5
              ? 'green'
              : (packet?.battery_voltage ?? 0) >= 6.5
                ? 'amber'
                : 'red'
          }
        />
        <StatCell
          label="Watchdog"
          value={link ? 'ARMED' : 'NO SIG'}
          accent={link ? 'green' : 'red'}
        />
      </div>

      <div className="rounded-sm border border-hud-border/60 bg-hud-bg/40">
        <div className="border-b border-hud-border/60 px-2 py-1 text-[10px] uppercase tracking-[0.3em] text-hud-fg-dim">
          Task Table
        </div>
        <ul className="divide-y divide-hud-border/40">
          {TASKS.map((t) => (
            <li
              key={t.name}
              className="grid grid-cols-[1fr_auto_auto] items-center gap-3 px-2 py-1.5 text-[11px]"
            >
              <div className="flex items-center gap-2">
                <span
                  className={cn(
                    'h-1.5 w-1.5 rounded-full',
                    link ? 'bg-hud-green hud-pulse' : 'bg-hud-fg-dim',
                  )}
                />
                <span className="text-hud-fg">{t.name}</span>
              </div>
              <span className="text-hud-fg-dim">{t.period}</span>
              <span className="text-hud-cyan">P{t.prio}</span>
            </li>
          ))}
        </ul>
      </div>
    </Panel>
  )
}
