from pydantic import BaseModel
from typing import Optional

# ── Pydantic BaseModel — kluczowa biblioteka FastAPI ─────
# Pydantic automatycznie:
#   1. Waliduje typy (float zamiast str = błąd 422)
#   2. Serializuje do JSON (model.model_dump())
#   3. Deserializuje z JSON (Model(**json_data))
# To odpowiednik struct z C++ ale z walidacją w runtime

class SensorData(BaseModel):
    gyro_x: float
    gyro_y: float
    gyro_z: float
    accel_x: float
    accel_y: float
    accel_z: float
    temperature: float
    timestamp_ms: int

class OrbitData(BaseModel):
    latitude: float
    longitude: float
    altitude_km: float
    velocity_ms: float
    inclination: float
    true_anomaly: float
    timestamp_ms: int

class LaserLinkData(BaseModel):
    link_active: bool
    signal_strength: float
    bit_error_rate: float
    atmospheric_loss: float
    ping_ms: int
    timestamp_ms: int

class TelemetryPacket(BaseModel):
    packet_id: int
    timestamp_ms: int
    sensor: SensorData
    orbit: OrbitData
    laser: LaserLinkData
    battery_voltage: float
    system_status: int        # 0=OK 1=WARNING 2=ERROR

# ── Warunki symulacji — wejście z Sim Control ────────────
# Optional[float] = float LUB None — pole nie musi być wysłane
# Wtedy zachowujemy aktualną wartość zamiast nadpisywać
class SimConditions(BaseModel):
    cloud_coverage: Optional[float] = None      # 0.0 - 1.0
    atmospheric_scint: Optional[float] = None   # 0.0 - 1.0
    solar_irradiance: Optional[float] = None    # W/m²
    eclipse: Optional[bool] = None
    orbit_altitude_km: Optional[float] = None
    orbit_inclination: Optional[float] = None

class SystemStatus(BaseModel):
    running: bool
    uptime_ms: int
    packet_count: int
    last_update: float        # unix timestamp