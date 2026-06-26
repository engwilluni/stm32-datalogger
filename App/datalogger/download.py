"""Framed download receiver."""

import struct
from typing import BinaryIO, Optional

from .transport import Transport
from .protocol import expect_ok
from .records import crc16, crc32


class DownloadError(Exception):
    pass


def _read_sof(ser: Transport) -> None:
    while True:
        b = ser.read_exact(1)
        if b == b"\x7e":
            return


def receive(ser: Transport, name: str, out: BinaryIO, timeout: Optional[float] = 10.0) -> bytes:
    """Download a file via GET command. Returns raw file bytes."""
    ser.send_line(f"GET {name}")
    line = ser.read_line(timeout=timeout)
    body = expect_ok(line)
    parts = body.split()
    if len(parts) != 2:
        raise DownloadError(f"Bad GET header: {body}")
    expected_size = int(parts[0])
    expected_crc32 = int(parts[1], 16)

    seq = 0
    received = 0
    while True:
        _read_sof(ser)
        hdr = ser.read_exact(6, timeout=timeout)
        frame_seq = struct.unpack("<I", hdr[:4])[0]
        length = struct.unpack("<H", hdr[4:6])[0]
        if length == 0:
            break
        payload = ser.read_exact(length, timeout=timeout)
        crc = struct.unpack(">H", ser.read_exact(2, timeout=timeout))[0]
        if crc16(payload) != crc:
            raise DownloadError(f"CRC16 mismatch at frame {frame_seq}")
        if frame_seq != seq:
            raise DownloadError(f"Sequence mismatch: expected {seq}, got {frame_seq}")
        out.write(payload)
        received += length
        seq += 1

    done = ser.read_line(timeout=timeout)
    if done.strip() != "DONE":
        raise DownloadError(f"Expected DONE, got {done}")

    if received != expected_size:
        raise DownloadError(f"Size mismatch: expected {expected_size}, got {received}")

    out.seek(0)
    data = out.read()
    if crc32(data) != expected_crc32:
        raise DownloadError("CRC32 mismatch")
    return data
