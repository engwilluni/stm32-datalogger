"""Binary record decoder and CSV exporter."""

import struct
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import List, BinaryIO, TextIO, Tuple

SDLG_MAGIC = b"SDLG"
REC_SIZE = 12

REC_TYPE_ADC = 0
REC_TYPE_GPIO = 1
REC_TYPE_MARKER = 2
REC_TYPE_LOSS = 3


def crc8(buf: bytes) -> int:
    crc = 0x00
    for b in buf:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x31) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def crc16(buf: bytes) -> int:
    crc = 0xFFFF
    for b in buf:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def crc32(buf: bytes) -> int:
    """Same MSB-first algorithm used in firmware (not IEEE reflected)."""
    crc = 0xFFFFFFFF
    for b in buf:
        crc ^= b << 24
        for _ in range(8):
            crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF if crc & 0x80000000 else (crc << 1) & 0xFFFFFFFF
    return crc ^ 0xFFFFFFFF


@dataclass
class LogHeader:
    magic: bytes
    version: int
    rec_size: int
    epoch_base: int
    session_id: int
    flags: int
    fw_ver: int
    crc16: int


@dataclass
class LogRecord:
    t_sec: int
    t_ms: int
    type: int
    chan: int
    value: int
    seq: int
    crc8: int

    def type_name(self) -> str:
        return {REC_TYPE_ADC: "ADC", REC_TYPE_GPIO: "GPIO", REC_TYPE_MARKER: "MARKER", REC_TYPE_LOSS: "LOSS"}.get(
            self.type, "UNKNOWN"
        )

    def timestamp(self) -> datetime:
        return datetime(2000, 1, 1, tzinfo=timezone.utc) + __import__("datetime").timedelta(
            seconds=self.t_sec, milliseconds=self.t_ms
        )


def decode_header(data: bytes) -> LogHeader:
    if len(data) < 32:
        raise ValueError("Header too short")
    magic, version, rec_size, epoch_base, session_id, flags, fw_ver, crc16_val = struct.unpack(
        "<4sBBIBBHQ", data[:20]
    )
    # crc16 is last 2 bytes
    crc16_val = struct.unpack("<H", data[30:32])[0]
    return LogHeader(magic, version, rec_size, epoch_base, session_id, flags, fw_ver, crc16_val)


def decode_record(data: bytes) -> LogRecord:
    if len(data) != REC_SIZE:
        raise ValueError(f"Record size must be {REC_SIZE}")
    t_sec, t_ms, typ, chan, value, seq, crc = struct.unpack("<IHBBHBB", data)
    return LogRecord(t_sec, t_ms, typ, chan, value, seq, crc)


def read_binary_file(path: str) -> Tuple[LogHeader, List[LogRecord]]:
    with open(path, "rb") as f:
        raw = f.read()
    if len(raw) < 32:
        raise ValueError("File too short")
    header = decode_header(raw[:32])
    if header.magic != SDLG_MAGIC:
        raise ValueError(f"Bad magic: {header.magic}")
    if header.rec_size != REC_SIZE:
        raise ValueError(f"Unsupported record size: {header.rec_size}")

    records: List[LogRecord] = []
    offset = 32
    expected_seq = None
    while offset + REC_SIZE <= len(raw):
        rec = decode_record(raw[offset : offset + REC_SIZE])
        if crc8(raw[offset : offset + 11]) != rec.crc8:
            raise ValueError(f"CRC8 mismatch at offset {offset}")
        records.append(rec)
        if expected_seq is not None and rec.seq != expected_seq:
            print(f"Warning: sequence gap at offset {offset}")
        expected_seq = (rec.seq + 1) & 0xFF
        offset += REC_SIZE
    return header, records


def export_csv(records: List[LogRecord], fp: TextIO) -> None:
    fp.write("timestamp,type,chan,value\n")
    for r in records:
        ts = r.timestamp().isoformat(timespec="milliseconds")
        fp.write(f"{ts},{r.type_name()},{r.chan},{r.value}\n")
