import asyncio
import json
from contextlib import asynccontextmanager
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.middleware.cors import CORSMiddleware

from models import SimConditions, SystemStatus
from bridge import bridge

# ── WebSocket manager — zarządza połączonymi klientami ───
# Gdy React dashboard otwiera WebSocket, dodajemy go tutaj.
# Gdy przychodzi telemetria z C++, broadcastujemy do wszystkich.
class WebSocketManager:
    def __init__(self):
        # Zbiór aktywnych połączeń WebSocket
        self.active_connections: list[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)
        print(f"[WS] Nowe połączenie. Aktywnych: {len(self.active_connections)}")

    def disconnect(self, websocket: WebSocket):
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)
        print(f"[WS] Rozłączono. Aktywnych: {len(self.active_connections)}")

    async def broadcast(self, data: dict):
        """Wyślij dane do wszystkich połączonych klientów."""
        if not self.active_connections:
            return
        message = json.dumps(data)
        # Iteruj po kopii listy — disconnect może modyfikować oryginał
        for websocket in self.active_connections[:]:
            try:
                await websocket.send_text(message)
            except Exception:
                self.disconnect(websocket)

ws_manager = WebSocketManager()

# ── Lifespan — startup/shutdown aplikacji ────────────────
# @asynccontextmanager to nowoczesny sposób (FastAPI 0.93+)
# na uruchamianie zadań przy starcie i sprzątanie przy zamknięciu.
# Zastępuje stare @app.on_event("startup")
@asynccontextmanager
async def lifespan(app: FastAPI):
    # ── STARTUP ──────────────────────────────────────────
    print("[API] Startuje FastAPI bridge...")

    # Zarejestruj broadcast jako subscriber w bridge
    # Gdy C++ wyśle pakiet → bridge → ws_manager.broadcast → React
    bridge.subscribe(ws_manager.broadcast)

    # Uruchom TCP server w tle — nie blokuje FastAPI
    # asyncio.create_task = "uruchom tę coroutine równolegle"
    # To jak Thread ale lżejszy i bezpieczniejszy dla async kodu
    asyncio.create_task(bridge.start_server())

    print("[API] Bridge uruchomiony. Czekam na C++...")
    yield  # ← tu aplikacja działa

    # ── SHUTDOWN ─────────────────────────────────────────
    print("[API] Zamykanie...")
    bridge.running = False

app = FastAPI(
    title="Satellite Sim API",
    description="Bridge między C++ FreeRTOS symulatorem a React dashboardem",
    version="1.0.0",
    lifespan=lifespan
)

# ── CORS — zezwól React (port 5173) na dostęp do API ─────
# Bez tego przeglądarka blokuje requesty między portami.
# W produkcji użyłbyś konkretnych origins, nie "*".
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# ════════════════════════════════════════════════════════
# REST ENDPOINTS
# ════════════════════════════════════════════════════════

@app.get("/")
async def root():
    return {"status": "Satellite Sim API running", "version": "1.0.0"}

@app.get("/status", response_model=SystemStatus)
async def get_status():
    """Status systemu — czy C++ jest połączony, ile pakietów."""
    return bridge.get_status()

@app.get("/telemetry")
async def get_telemetry():
    """Ostatni pakiet telemetryczny — polling fallback gdy WebSocket niedostępny."""
    if bridge.latest_telemetry is None:
        raise HTTPException(
            status_code=503,
            detail="Brak danych — C++ symulator nie połączony"
        )
    return bridge.latest_telemetry

@app.get("/telemetry/orbit")
async def get_orbit():
    """Tylko dane orbitalne z ostatniego pakietu."""
    if bridge.latest_telemetry is None:
        raise HTTPException(status_code=503, detail="Brak danych")
    orbit = bridge.latest_telemetry.get("orbit")
    if orbit is None:
        raise HTTPException(status_code=404, detail="Brak danych orbitalnych")
    return orbit

@app.get("/telemetry/laser")
async def get_laser():
    """Tylko dane łącza laserowego."""
    if bridge.latest_telemetry is None:
        raise HTTPException(status_code=503, detail="Brak danych")
    laser = bridge.latest_telemetry.get("laser")
    if laser is None:
        raise HTTPException(status_code=404, detail="Brak danych lasera")
    return laser

@app.post("/sim/conditions")
async def set_conditions(conditions: SimConditions):
    """
    Ustaw warunki symulacji z Sim Control panelu.
    Aktualizuje g_sim_conditions w C++ przez socket (TODO Faza 4).
    Na razie zapisuje lokalnie i zwraca potwierdzenie.
    """
    # Zapisz tylko pola które zostały wysłane (nie-None)
    # model_dump(exclude_none=True) = pomiń pola z wartością None
    updates = conditions.model_dump(exclude_none=True)

    if not updates:
        raise HTTPException(status_code=400, detail="Brak pól do aktualizacji")

    # TODO: wyślij updates do C++ przez osobny socket/command channel
    # Na razie zwracamy co dostaliśmy — Faza 4 doda prawdziwy zapis
    print(f"[API] Nowe warunki symulacji: {updates}")

    return {
        "status": "accepted",
        "updated_fields": list(updates.keys()),
        "values": updates
    }

@app.get("/sim/conditions")
async def get_conditions():
    """Zwróć aktualne warunki symulacji."""
    # Odczytaj z bridge który ma referencję do g_sim_conditions
    # Na razie zwracamy defaults — Faza 4 doda sync z C++
    return {
        "cloud_coverage": 0.2,
        "atmospheric_scint": 0.1,
        "solar_irradiance": 1361.0,
        "eclipse": False,
        "orbit_altitude_km": 550.0,
        "orbit_inclination": 53.0
    }

# ════════════════════════════════════════════════════════
# WEBSOCKET ENDPOINT
# ════════════════════════════════════════════════════════

@app.websocket("/ws/telemetry")
async def websocket_telemetry(websocket: WebSocket):
    """
    WebSocket endpoint — React łączy się tutaj i dostaje
    live stream danych z C++ w czasie rzeczywistym.

    Flow:
    C++ → TCP socket → bridge → WebSocket → React Dashboard
    """
    await ws_manager.connect(websocket)

    # Wyślij od razu ostatni pakiet jeśli mamy (initial state)
    if bridge.latest_telemetry:
        await websocket.send_text(json.dumps(bridge.latest_telemetry))

    try:
        # Czekaj na wiadomości od klienta (ping/pong, komendy)
        # Bez tej pętli połączenie od razu by się zamknęło
        while True:
            data = await websocket.receive_text()
            # Możesz tu obsłużyć komendy od React (np. "ping")
            if data == "ping":
                await websocket.send_text('{"type":"pong"}')
    except WebSocketDisconnect:
        ws_manager.disconnect(websocket)