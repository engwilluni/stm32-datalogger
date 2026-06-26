import pytest

from datalogger.protocol import encode, decode, expect_ok, ProtocolError


def test_encode():
    assert encode("TIME?") == "TIME?"
    assert encode("RATE", "500") == "RATE 500"


def test_decode_ok():
    assert decode("OK 123.456") == ("OK", "123.456")
    assert decode("OK") == ("OK", "")


def test_decode_err():
    with pytest.raises(ProtocolError):
        decode("ERR 1 bad args")


def test_expect_ok():
    assert expect_ok("OK hello") == "hello"
    with pytest.raises(ProtocolError):
        expect_ok("ERR 5 io")
