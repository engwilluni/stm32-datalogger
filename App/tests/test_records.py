import struct

from datalogger.records import crc8, crc16, crc32, decode_record, SDLG_MAGIC


def test_crc8():
    assert crc8(b"123456789") == 0xA2


def test_crc16():
    assert crc16(b"123456789") == 0x29B1


def test_crc32():
    assert crc32(b"123456789") == 0xFC891918


def test_decode_record():
    data = struct.pack("<IHBBHBB", 100, 500, 0, 14, 2048, 7, 0)
    crc = crc8(data[:11])
    data = data[:11] + struct.pack("B", crc)
    rec = decode_record(data)
    assert rec.t_sec == 100
    assert rec.t_ms == 500
    assert rec.type == 0
    assert rec.chan == 14
    assert rec.value == 2048
    assert rec.seq == 7
