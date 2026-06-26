"""Serial transport layer with auto-detection."""

import serial
import serial.tools.list_ports
from typing import Optional

VID_PID = (0x1209, 0x0001)
DEFAULT_BAUD = 115200
TIMEOUT = 0.5


class TransportError(Exception):
    pass


class Transport:
    def __init__(self, port: Optional[str] = None, baud: int = DEFAULT_BAUD):
        self.ser: Optional[serial.Serial] = None
        if port is None:
            port = self.auto_detect()
        self.port = port
        self.baud = baud

    @staticmethod
    def auto_detect() -> str:
        for p in serial.tools.list_ports.comports():
            if p.vid == VID_PID[0] and p.pid == VID_PID[1]:
                return p.device
        raise TransportError(f"No device with VID/PID {VID_PID[0]:04X}:{VID_PID[1]:04X} found")

    def open(self) -> "Transport":
        self.ser = serial.Serial(self.port, self.baud, timeout=TIMEOUT)
        self.ser.flushInput()
        self.ser.flushOutput()
        return self

    def close(self) -> None:
        if self.ser:
            self.ser.close()
            self.ser = None

    def send(self, data: bytes) -> None:
        if self.ser is None:
            raise TransportError("Port not open")
        self.ser.write(data)
        self.ser.flush()

    def send_line(self, line: str) -> None:
        self.send((line + "\n").encode("ascii"))

    def read_line(self, timeout: Optional[float] = None) -> str:
        if self.ser is None:
            raise TransportError("Port not open")
        old_timeout = self.ser.timeout
        if timeout is not None:
            self.ser.timeout = timeout
        try:
            raw = self.ser.readline()
        finally:
            self.ser.timeout = old_timeout
        if not raw:
            raise TransportError("Timeout reading line")
        return raw.decode("ascii", errors="replace").strip()

    def read_exact(self, n: int, timeout: Optional[float] = None) -> bytes:
        if self.ser is None:
            raise TransportError("Port not open")
        old_timeout = self.ser.timeout
        if timeout is not None:
            self.ser.timeout = timeout
        try:
            data = self.ser.read(n)
        finally:
            self.ser.timeout = old_timeout
        if len(data) != n:
            raise TransportError(f"Expected {n} bytes, got {len(data)}")
        return data

    def __enter__(self) -> "Transport":
        self.open()
        return self

    def __exit__(self, *args) -> None:
        self.close()
