"""ASCII protocol encoder/decoder."""

import re
from typing import Optional, Tuple


class ProtocolError(Exception):
    pass


def encode(cmd: str, arg: str = "") -> str:
    if arg:
        return f"{cmd} {arg}"
    return cmd


def decode(line: str) -> Tuple[str, str]:
    """Return (status, body) for OK/ERR lines."""
    line = line.strip()
    if line.startswith("OK"):
        return "OK", line[2:].strip()
    if line.startswith("ERR"):
        m = re.match(r"ERR\s+(\d+)\s+(.*)", line)
        if m:
            raise ProtocolError(f"Device error {m.group(1)}: {m.group(2)}")
        raise ProtocolError(f"Device error: {line}")
    if line.startswith("#"):
        return "STREAM", line[1:].strip()
    if line.startswith("FILE"):
        return "FILE", line[4:].strip()
    return "RAW", line


def expect_ok(line: str, msg: str = "") -> str:
    status, body = decode(line)
    if status != "OK":
        raise ProtocolError(f"Expected OK, got {status}: {line}")
    return body
