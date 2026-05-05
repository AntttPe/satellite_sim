import asyncio
import json
import struct
import time
from typing import Optional, Callable
import threading

# ── Dlaczego asyncio zamiast threading? ──────────────────
# FastAPI jest async — działa na event loop (jak JavaScript).
# Threading = wiele wątków OS, mutex potrzebny wszędzie.
# Asyncio = jeden wątek, wiele coroutines, przełączają się
# przy await. Lżejsze i bezpieczniejsze dla I/O bound tasks.
#
# ALE: C++ socket to blokujące I/O — będziemy czytać
# w osobnym wątku OS i wrzucać do asyncio queue.
# To klasyczny bridge pattern: thread → async queue → FastAPI

class SatelliteBridge:
    def __init__(self, host: str = "127.0.0.1", port: int = 9000):
        self.host = host
        self.port = port

        # ── Aktualny stan symulatora ──────────────────────
        # Słownik zamiast klasy — elastyczniejszy dla JSON
        self.latest_telemetry: Optional[dict] = None
        self.packet_count: int = 0
        self.start_time: float = time.time()
        self.running: bool = False

        # ── asyncio.Queue — bezpieczna komunikacja ────────
        # thread → queue → websocket clients
        # asyncio.Queue jest thread-safe gdy używasz
        # loop.call_soon_threadsafe() do wrzucania z wątku OS
        self._telemetry_queue: asyncio.Queue = asyncio.Queue(maxsize=100)

        # ── Callbacks dla WebSocket broadcastu ───────────
        # Lista funkcji wywoływanych gdy przychodzi nowy pakiet
        self._subscribers: list[Callable] = []

    def subscribe(self, callback: Callable):
        """Zarejestruj callback wywoływany przy każdym nowym pakiecie."""
        self._subscribers.append(callback)

    def unsubscribe(self, callback: Callable):
        """Wyrejestruj callback (np. gdy WebSocket się rozłączy)."""
        if callback in self._subscribers:
            self._subscribers.remove(callback)

    async def _notify_subscribers(self, data: dict):
        """Wywołaj wszystkie zarejestrowane callbacki."""
        for callback in self._subscribers[:]:  # kopia listy — bezpieczna iteracja
            try:
                await callback(data)
            except Exception as e:
                print(f"[BRIDGE] Subscriber error: {e}")
                self.unsubscribe(callback)

    def _parse_telemetry_line(self, line: str) -> Optional[dict]:
        """
        Parsuj linię JSON z C++.
        C++ będzie wysyłać: {"packet_id":1,"timestamp_ms":1001,...}\n
        """
        line = line.strip()
        if not line:
            return None
        try:
            return json.loads(line)
        except json.JSONDecodeError as e:
            print(f"[BRIDGE] Parse error: {e} | line: {line[:50]}")
            return None

    async def _read_from_socket(self, reader: asyncio.StreamReader):
        """
        Czytaj dane z C++ przez TCP socket.
        asyncio.StreamReader.readline() = czekaj na \\n, nie blokuj event loop.
        """
        print(f"[BRIDGE] Połączono z C++ symulatorem")
        self.running = True

        while self.running:
            try:
                # readline() czyta do \n — C++ musi kończyć każdy JSON newline'em
                line_bytes = await asyncio.wait_for(
                    reader.readline(),
                    timeout=10.0   # 10s bez danych = coś poszło nie tak
                )

                if not line_bytes:
                    print("[BRIDGE] C++ zamknął połączenie")
                    break

                line = line_bytes.decode('utf-8')
                data = self._parse_telemetry_line(line)

                if data:
                    self.latest_telemetry = data
                    self.packet_count += 1
                    # Powiadom wszystkich WebSocket subscribers
                    await self._notify_subscribers(data)

            except asyncio.TimeoutError:
                print("[BRIDGE] WARN: timeout — brak danych z C++ przez 10s")
            except Exception as e:
                print(f"[BRIDGE] Błąd odczytu: {e}")
                break

        self.running = False
        print("[BRIDGE] Rozłączono od C++")

    async def start_server(self):
        """
        Uruchom TCP server czekający na połączenie od C++.
        C++ jest klientem, Python jest serwerem — C++ łączy się do nas.
        """
        print(f"[BRIDGE] Słucham na {self.host}:{self.port}...")

        async def handle_connection(reader: asyncio.StreamReader,
                                    writer: asyncio.StreamWriter):
            addr = writer.get_extra_info('peername')
            print(f"[BRIDGE] Połączenie od {addr}")
            try:
                await self._read_from_socket(reader)
            finally:
                writer.close()
                await writer.wait_closed()

        # asyncio.start_server = async odpowiednik socket.bind/listen/accept
        server = await asyncio.start_server(
            handle_connection,
            self.host,
            self.port
        )

        async with server:
            await server.serve_forever()

    def get_status(self) -> dict:
        return {
            "running": self.running,
            "uptime_ms": int((time.time() - self.start_time) * 1000),
            "packet_count": self.packet_count,
            "last_update": time.time()
        }

# ── Singleton — jedna instancja na cały proces ───────────
# Importując bridge z innych plików zawsze dostajemy TEN SAM obiekt.
# To odpowiednik zmiennej globalnej ale kontrolowany.
bridge = SatelliteBridge(host="127.0.0.1", port=9000)