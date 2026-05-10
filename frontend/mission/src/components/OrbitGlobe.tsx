import { useMemo, useRef } from 'react'
import { Canvas, useFrame } from '@react-three/fiber'
import { OrbitControls, Stars } from '@react-three/drei'
import * as THREE from 'three'
import { Panel } from './Panel'
import { StatCell } from './StatCell'
import { formatNumber } from '@/lib/utils'
import type { TelemetryPacket } from '@/types'

interface OrbitGlobeProps {
  history: TelemetryPacket[]
  latest: TelemetryPacket | null
}

const EARTH_RADIUS = 1
const ALTITUDE_EXAGGERATION = 0.45

function latLonToVec3(latDeg: number, lonDeg: number, radius: number): THREE.Vector3 {
  const phi = (90 - latDeg) * (Math.PI / 180)
  const theta = (lonDeg + 180) * (Math.PI / 180)
  return new THREE.Vector3(
    -radius * Math.sin(phi) * Math.cos(theta),
    radius * Math.cos(phi),
    radius * Math.sin(phi) * Math.sin(theta),
  )
}

function EarthSphere() {
  const groupRef = useRef<THREE.Group>(null)
  useFrame((_, delta) => {
    if (groupRef.current) groupRef.current.rotation.y += delta * 0.04
  })
  return (
    <group ref={groupRef}>
      {/* Solid base */}
      <mesh>
        <sphereGeometry args={[EARTH_RADIUS, 48, 48]} />
        <meshStandardMaterial
          color="#0a1a2b"
          emissive="#06121d"
          emissiveIntensity={0.6}
          roughness={0.95}
          metalness={0.1}
        />
      </mesh>
      {/* Wireframe graticule */}
      <mesh>
        <sphereGeometry args={[EARTH_RADIUS * 1.001, 24, 18]} />
        <meshBasicMaterial color="#1c4b6e" wireframe transparent opacity={0.55} />
      </mesh>
      {/* Equator highlight */}
      <mesh rotation={[Math.PI / 2, 0, 0]}>
        <torusGeometry args={[EARTH_RADIUS * 1.002, 0.003, 8, 96]} />
        <meshBasicMaterial color="#22d3ee" transparent opacity={0.55} />
      </mesh>
      {/* Atmosphere glow */}
      <mesh>
        <sphereGeometry args={[EARTH_RADIUS * 1.05, 48, 48]} />
        <meshBasicMaterial color="#22d3ee" transparent opacity={0.06} side={THREE.BackSide} />
      </mesh>
    </group>
  )
}

function Satellite({ position }: { position: THREE.Vector3 }) {
  const ref = useRef<THREE.Mesh>(null)
  useFrame((state) => {
    if (!ref.current) return
    const t = state.clock.getElapsedTime()
    ref.current.scale.setScalar(1 + Math.sin(t * 4) * 0.15)
  })
  return (
    <group position={position}>
      <mesh ref={ref}>
        <sphereGeometry args={[0.025, 12, 12]} />
        <meshBasicMaterial color="#fbbf24" />
      </mesh>
      <mesh>
        <ringGeometry args={[0.04, 0.05, 32]} />
        <meshBasicMaterial color="#fbbf24" transparent opacity={0.4} side={THREE.DoubleSide} />
      </mesh>
    </group>
  )
}

function GroundTrack({ history }: { history: TelemetryPacket[] }) {
  const points = useMemo(() => {
    if (history.length < 2) return null
    const arr: THREE.Vector3[] = []
    let prevLon = history[0].orbit.longitude
    for (const p of history) {
      // Avoid wrap-around artifacts: split if longitude jumps > 180
      if (Math.abs(p.orbit.longitude - prevLon) > 180) {
        return null // simple bail: skip line if recent wrap — keep trail clean
      }
      prevLon = p.orbit.longitude
      arr.push(latLonToVec3(p.orbit.latitude, p.orbit.longitude, EARTH_RADIUS * 1.005))
    }
    return arr
  }, [history])

  const geometry = useMemo(() => {
    if (!points || points.length < 2) return null
    const g = new THREE.BufferGeometry().setFromPoints(points)
    return g
  }, [points])

  if (!geometry) return null
  return (
    <line>
      <primitive object={geometry} attach="geometry" />
      <lineBasicMaterial color="#22d3ee" transparent opacity={0.85} linewidth={2} />
    </line>
  )
}

function SatRadial({ position }: { position: THREE.Vector3 }) {
  // Line from earth center surface intercept to satellite
  const points = useMemo(() => {
    const surface = position.clone().normalize().multiplyScalar(EARTH_RADIUS * 1.002)
    return [surface, position]
  }, [position])
  const geometry = useMemo(() => new THREE.BufferGeometry().setFromPoints(points), [points])
  return (
    <line>
      <primitive object={geometry} attach="geometry" />
      <lineDashedMaterial color="#fbbf24" transparent opacity={0.5} dashSize={0.05} gapSize={0.03} />
    </line>
  )
}

export function OrbitGlobe({ history, latest }: OrbitGlobeProps) {
  const satPos = useMemo(() => {
    if (!latest) return new THREE.Vector3(EARTH_RADIUS * 1.45, 0, 0)
    const r = EARTH_RADIUS + ALTITUDE_EXAGGERATION * (latest.orbit.altitude_km / 6371)
    return latLonToVec3(latest.orbit.latitude, latest.orbit.longitude, r)
  }, [latest])

  return (
    <Panel title="Orbit · Ground Track" subtitle="LEO · 3D view" accent="cyan" className="min-h-[420px]">
      <div className="relative h-[360px] w-full overflow-hidden rounded-sm border border-hud-border/60 bg-black/40">
        <Canvas camera={{ position: [3, 1.6, 3], fov: 38 }}>
          <ambientLight intensity={0.45} />
          <directionalLight position={[5, 3, 5]} intensity={1.1} color="#a5d8ff" />
          <directionalLight position={[-4, -2, -3]} intensity={0.25} color="#22d3ee" />
          <Stars radius={50} depth={60} count={2200} factor={3} fade speed={0.4} />
          <EarthSphere />
          <GroundTrack history={history} />
          <Satellite position={satPos} />
          <SatRadial position={satPos} />
          <OrbitControls
            enablePan={false}
            enableZoom
            minDistance={2}
            maxDistance={8}
            autoRotate={false}
          />
        </Canvas>
        {/* HUD overlay tick marks */}
        <div className="pointer-events-none absolute inset-0">
          <div className="absolute left-2 top-2 text-[10px] uppercase tracking-[0.3em] text-hud-cyan/70">
            NAV ◤ LEO 550KM
          </div>
          <div className="absolute right-2 top-2 text-[10px] uppercase tracking-[0.3em] text-hud-fg-dim">
            drag · rotate · scroll · zoom
          </div>
          <div className="absolute bottom-2 left-2 text-[10px] uppercase tracking-[0.3em] text-hud-fg-dim">
            ground-track · {history.length} samples
          </div>
        </div>
      </div>

      <div className="mt-3 grid grid-cols-2 gap-2 sm:grid-cols-4">
        <StatCell
          label="Latitude"
          value={`${formatNumber(latest?.orbit.latitude ?? 0, 2)}°`}
          accent="cyan"
        />
        <StatCell
          label="Longitude"
          value={`${formatNumber(latest?.orbit.longitude ?? 0, 2)}°`}
          accent="cyan"
        />
        <StatCell
          label="Altitude"
          value={formatNumber(latest?.orbit.altitude_km ?? 0, 1)}
          unit="km"
          accent="green"
        />
        <StatCell
          label="Velocity"
          value={formatNumber(latest?.orbit.velocity_ms ?? 0, 0)}
          unit="m/s"
          accent="violet"
        />
        <StatCell
          label="Inclination"
          value={`${formatNumber(latest?.orbit.inclination ?? 0, 1)}°`}
          accent="amber"
        />
        <StatCell
          label="True Anomaly"
          value={`${formatNumber(latest?.orbit.true_anomaly ?? 0, 2)}°`}
          accent="amber"
        />
        <StatCell
          label="Orbital T"
          value={formatNumber(orbitalPeriodSec(latest?.orbit.altitude_km ?? 550) / 60, 1)}
          unit="min"
          accent="violet"
        />
        <StatCell
          label="Packet"
          value={`#${latest?.packet_id ?? 0}`}
          accent="default"
        />
      </div>
    </Panel>
  )
}

function orbitalPeriodSec(altKm: number): number {
  const MU = 3.986e14
  const r = (6371 + altKm) * 1000
  return 2 * Math.PI * Math.sqrt((r * r * r) / MU)
}
