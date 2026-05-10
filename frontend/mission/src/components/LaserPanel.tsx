import { useMemo } from 'react'
import { Panel } from './Panel'
import { StatCell } from './StatCell'
import { cn, formatBer, formatNumber } from '@/lib/utils'
import type { TelemetryPacket } from '@/types'
import {
  Area,
  AreaChart,
  CartesianGrid,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts'

interface LaserPanelProps {
  history: TelemetryPacket[]
  latest: TelemetryPacket | null
}

export function LaserPanel({ history, latest }: LaserPanelProps) {
  const data = useMemo(
    () =>
      history.map((p) => ({
        t: p.laser.timestamp_ms,
        signal: p.laser.signal_strength,
        active: p.laser.link_active ? 1 : 0,
      })),
    [history],
  )

  const active = latest?.laser.link_active ?? false
  const ber = latest?.laser.bit_error_rate ?? 0
  const berAccent: 'green' | 'amber' | 'red' =
    ber < 1e-6 ? 'green' : ber < 1e-3 ? 'amber' : 'red'

  return (
    <Panel
      title="Optical Downlink"
      subtitle={active ? 'LINK ACTIVE' : 'LINK DOWN'}
      accent={active ? 'green' : 'red'}
    >
      <div className="flex items-center gap-3 pb-3">
        <div
          className={cn(
            'flex h-9 w-9 items-center justify-center rounded-full border-2',
            active
              ? 'border-hud-green text-hud-green hud-pulse'
              : 'border-hud-red text-hud-red',
          )}
        >
          <span className="text-[18px] leading-none">◉</span>
        </div>
        <div className="flex-1">
          <div
            className={cn(
              'text-[15px] font-bold tracking-[0.3em]',
              active ? 'text-hud-green' : 'text-hud-red hud-blink',
            )}
          >
            {active ? '◤ ACTIVE ◢' : '◤ DOWN ◢'}
          </div>
          <div className="text-[10px] uppercase tracking-widest text-hud-fg-dim">
            {active ? 'optical carrier locked' : 'no carrier · check atm/eclipse'}
          </div>
        </div>
      </div>

      <div className="grid grid-cols-2 gap-2 pb-3">
        <StatCell
          label="Signal"
          value={formatNumber(latest?.laser.signal_strength ?? 0, 1)}
          unit="dBm"
          accent={active ? 'cyan' : 'red'}
        />
        <StatCell
          label="BER"
          value={formatBer(ber)}
          accent={berAccent}
        />
        <StatCell
          label="Atm. Loss"
          value={formatNumber(latest?.laser.atmospheric_loss ?? 0, 1)}
          unit="dB"
          accent="amber"
        />
        <StatCell
          label="Ping"
          value={String(latest?.laser.ping_ms ?? 0)}
          unit="ms"
          accent="violet"
        />
      </div>

      <div className="rounded-sm border border-hud-border/60 bg-hud-bg/40 p-1">
        <div className="px-2 py-1 text-[10px] uppercase tracking-[0.3em] text-hud-fg-dim">
          Signal strength · last {data.length}
        </div>
        <ResponsiveContainer width="100%" height={90}>
          <AreaChart data={data} margin={{ top: 4, right: 6, left: -22, bottom: 0 }}>
            <defs>
              <linearGradient id="signalGrad" x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" stopColor="#22d3ee" stopOpacity={0.6} />
                <stop offset="100%" stopColor="#22d3ee" stopOpacity={0} />
              </linearGradient>
            </defs>
            <CartesianGrid stroke="#1c2530" strokeDasharray="2 4" />
            <XAxis dataKey="t" hide />
            <YAxis
              tick={{ fill: '#6b7d8f', fontSize: 9, fontFamily: 'ui-monospace' }}
              stroke="#1c2530"
              domain={[-100, 0]}
              tickFormatter={(v) => `${v}`}
            />
            <Tooltip
              contentStyle={{
                background: '#0b0f14',
                border: '1px solid #1c2530',
                borderRadius: 2,
                fontSize: 11,
                fontFamily: 'ui-monospace',
              }}
              labelStyle={{ color: '#6b7d8f' }}
              itemStyle={{ color: '#22d3ee' }}
              formatter={(v) => `${Number(v).toFixed(1)} dBm`}
              labelFormatter={() => 'signal'}
            />
            <Area
              type="monotone"
              dataKey="signal"
              stroke="#22d3ee"
              fill="url(#signalGrad)"
              strokeWidth={1.4}
              isAnimationActive={false}
            />
          </AreaChart>
        </ResponsiveContainer>
      </div>
    </Panel>
  )
}
