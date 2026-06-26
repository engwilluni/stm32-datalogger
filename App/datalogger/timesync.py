"""Synchronize device RTC with PC time."""

import time
from datetime import datetime, timezone


def pc_epoch_ms() -> float:
    return datetime.now(timezone.utc).timestamp()


def pc_epoch() -> int:
    """Return y2k epoch seconds (seconds since 2000-01-01 UTC)."""
    return int(pc_epoch_ms() - 946684800.0)
