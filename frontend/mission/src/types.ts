export interface SensorData {
  gyro_x: number
  gyro_y: number
  gyro_z: number
  accel_x: number
  accel_y: number
  accel_z: number
  temperature: number
  timestamp_ms: number
}

export interface OrbitData {
  latitude: number
  longitude: number
  altitude_km: number
  velocity_ms: number
  inclination: number
  true_anomaly: number
  timestamp_ms: number
}

export interface LaserLinkData {
  link_active: boolean
  signal_strength: number
  bit_error_rate: number
  atmospheric_loss: number
  ping_ms: number
  timestamp_ms: number
}

export interface TelemetryPacket {
  packet_id: number
  timestamp_ms: number
  sensor: SensorData
  orbit: OrbitData
  laser: LaserLinkData
  battery_voltage: number
  system_status: number
}

export type ConnectionState = 'connecting' | 'open' | 'closed' | 'error'
