import { useMemo } from 'react'
import {
  CartesianGrid,
  Line,
  LineChart,
  ReferenceLine,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts'
import { Panel } from './Panel'
import { StatCell } from './StatCell'
import { formatNumber } from '@/lib/utils'
import type { TelemetryPacket } from '@/types'

interface SensorChartsProps {
  history: TelemetryPacket[]
  latest: TelemetryPacket | null
}

const COLORS = {
  x: '#22d3ee',
  y: '#a78bfa',
  z: '#4ade80',
  temp: '#fbbf24',
}

interface ChartPoint {
  t: number
  x: number
  y: number
  z: number
  temp: number
}

function MiniLineChart({
  data,
  series,
  yDomain,
}: {
  data: ChartPoint[]
  series: { key: keyof ChartPoint; color: string; label: string }[]
  yDomain?: [number | 'auto' | 'dataMin' | 'dataMax', number | 'auto' | 'dataMin' | 'dataMax']
}) {
  return (
    <ResponsiveContainer width="100%" height={140}>
      <LineChart data={data} margin={{ top: 4, right: 6, left: -22, bottom: 0 }}>
        <CartesianGrid stroke="#1c2530" strokeDasharray="2 4" />
        <XAxis
          dataKey="t"
          tick={{ fill: '#6b7d8f', fontSize: 9, fontFamily: 'ui-monospace' }}
          tickFormatter={(v) => `${(v / 1000).toFixed(0)}s`}
          stroke="#1c2530"
          interval="preserveStartEnd"
        />
        <YAxis
          tick={{ fill: '#6b7d8f', fontSize: 9, fontFamily: 'ui-monospace' }}
          stroke="#1c2530"
          domain={yDomain ?? ['auto', 'auto']}
          tickFormatter={(v) => Number(v).toFixed(1)}
        />
        <ReferenceLine y={0} stroke="#1c2530" />
        <Tooltip
          contentStyle={{
            background: '#0b0f14',
            border: '1px solid #1c2530',
            borderRadius: 2,
            fontSize: 11,
            fontFamily: 'ui-monospace',
          }}
          labelStyle={{ color: '#6b7d8f' }}
          itemStyle={{ color: '#c8d6e5' }}
          labelFormatter={(v) => `t=${(Number(v) / 1000).toFixed(2)}s`}
          formatter={(v) => Number(v).toFixed(3)}
        />
        {series.map((s) => (
          <Line
            key={s.key}
            type="monotone"
            dataKey={s.key}
            stroke={s.color}
            strokeWidth={1.4}
            dot={false}
            isAnimationActive={false}
            name={s.label}
          />
        ))}
      </LineChart>
    </ResponsiveContainer>
  )
}

export function SensorCharts({ history, latest }: SensorChartsProps) {
  const data = useMemo<ChartPoint[]>(
    () =>
      history.map((p) => ({
        t: p.sensor.timestamp_ms,
        x: p.sensor.gyro_x,
        y: p.sensor.gyro_y,
        z: p.sensor.gyro_z,
        temp: p.sensor.temperature,
      })),
    [history],
  )

  const accelData = useMemo(
    () =>
      history.map((p) => ({
        t: p.sensor.timestamp_ms,
        x: p.sensor.accel_x,
        y: p.sensor.accel_y,
        z: p.sensor.accel_z,
        temp: 0,
      })),
    [history],
  )

  return (
    <div className="flex flex-col gap-3">
      <Panel title="IMU · Gyroscope" subtitle="deg/s" accent="cyan">
        <div className="mb-2 grid grid-cols-3 gap-2">
          <StatCell label="GYRO X" value={formatNumber(latest?.sensor.gyro_x ?? 0)} unit="°/s" accent="cyan" />
          <StatCell label="GYRO Y" value={formatNumber(latest?.sensor.gyro_y ?? 0)} unit="°/s" accent="violet" />
          <StatCell label="GYRO Z" value={formatNumber(latest?.sensor.gyro_z ?? 0)} unit="°/s" accent="green" />
        </div>
        <MiniLineChart
          data={data}
          series={[
            { key: 'x', color: COLORS.x, label: 'X' },
            { key: 'y', color: COLORS.y, label: 'Y' },
            { key: 'z', color: COLORS.z, label: 'Z' },
          ]}
        />
      </Panel>

      <Panel title="IMU · Accelerometer" subtitle="m/s²" accent="violet">
        <div className="mb-2 grid grid-cols-3 gap-2">
          <StatCell label="ACC X" value={formatNumber(latest?.sensor.accel_x ?? 0)} unit="m/s²" accent="cyan" />
          <StatCell label="ACC Y" value={formatNumber(latest?.sensor.accel_y ?? 0)} unit="m/s²" accent="violet" />
          <StatCell label="ACC Z" value={formatNumber(latest?.sensor.accel_z ?? 0)} unit="m/s²" accent="green" />
        </div>
        <MiniLineChart
          data={accelData}
          series={[
            { key: 'x', color: COLORS.x, label: 'X' },
            { key: 'y', color: COLORS.y, label: 'Y' },
            { key: 'z', color: COLORS.z, label: 'Z' },
          ]}
        />
      </Panel>

      <Panel title="Thermal" subtitle="°C · solar panel" accent="amber">
        <div className="mb-2 grid grid-cols-1 gap-2">
          <StatCell
            label="TEMP"
            value={formatNumber(latest?.sensor.temperature ?? 0, 2)}
            unit="°C"
            accent="amber"
          />
        </div>
        <MiniLineChart
          data={data}
          series={[{ key: 'temp', color: COLORS.temp, label: 'Temp' }]}
        />
      </Panel>
    </div>
  )
}
