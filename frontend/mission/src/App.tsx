import { TopBar } from '@/components/TopBar'
import { SensorCharts } from '@/components/SensorCharts'
import { OrbitGlobe } from '@/components/OrbitGlobe'
import { LaserPanel } from '@/components/LaserPanel'
import { SystemPanel } from '@/components/SystemPanel'
import { useTelemetry } from '@/hooks/useTelemetry'

function App() {
  const { latest, history, state, packetCount } = useTelemetry(180)

  return (
    <div className="hud-scanlines relative flex min-h-screen flex-col">
      <TopBar packet={latest} state={state} packetCount={packetCount} />

      <main className="grid flex-1 grid-cols-1 gap-3 p-3 xl:grid-cols-[320px_1fr_320px]">
        {/* LEFT column */}
        <div className="flex flex-col gap-3">
          <SensorCharts history={history} latest={latest} />
        </div>

        {/* CENTER column */}
        <div className="flex flex-col gap-3">
          <OrbitGlobe history={history} latest={latest} />
        </div>

        {/* RIGHT column */}
        <div className="flex flex-col gap-3">
          <LaserPanel history={history} latest={latest} />
          <SystemPanel packet={latest} state={state} packetCount={packetCount} />
        </div>
      </main>

      <footer className="border-t border-hud-border bg-hud-panel/60 px-4 py-1.5 text-[10px] uppercase tracking-[0.3em] text-hud-fg-dim">
        <span>satsim · mission control · v1.0 ·</span>{' '}
        <span className="text-hud-cyan">ws://localhost:8000/ws/telemetry</span>
      </footer>
    </div>
  )
}

export default App
