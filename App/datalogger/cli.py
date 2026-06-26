"""Command-line interface for the datalogger."""

import argparse
import io
import sys
from pathlib import Path

from .transport import Transport
from .protocol import encode, expect_ok, decode
from .download import receive
from .records import read_binary_file, export_csv
from .timesync import pc_epoch


def _cmd(port: str, cmd: str, arg: str = "", timeout: float = 2.0) -> str:
    with Transport(port) as ser:
        ser.send_line(encode(cmd, arg))
        return ser.read_line(timeout=timeout)


def main() -> None:
    parser = argparse.ArgumentParser(description="STM32 datalogger CLI")
    parser.add_argument("--port", "-p", help="Serial port (auto-detect if omitted)")
    sub = parser.add_subparsers(dest="command")

    sub.add_parser("info", help="Read *IDN?")
    sub.add_parser("status", help="Read STAT?")

    p_time = sub.add_parser("time", help="Read or set RTC time")
    p_time.add_argument("--set", action="store_true", help="Set RTC to PC time")

    sub.add_parser("start", help="Start logging")
    sub.add_parser("stop", help="Stop logging")

    p_rate = sub.add_parser("rate", help="Set sample rate in ms")
    p_rate.add_argument("ms", type=int)

    p_thr = sub.add_parser("thr", help="Set ADC threshold alarm")
    p_thr.add_argument("ch", type=int)
    p_thr.add_argument("lo", type=int)
    p_thr.add_argument("hi", type=int)

    p_cfg = sub.add_parser("cfg", help="Read or write CONFIG.TXT")
    p_cfg.add_argument("kv", nargs="?", help="key=value to set")

    sub.add_parser("ls", help="List files")

    p_get = sub.add_parser("get", help="Download a file")
    p_get.add_argument("name")
    p_get.add_argument("--out", "-o", required=True)

    p_del = sub.add_parser("del", help="Delete a file")
    p_del.add_argument("name")

    p_format = sub.add_parser("format", help="Format SD card")

    p_stream = sub.add_parser("stream", help="Enable/disable live stream")
    p_stream.add_argument("state", choices=["on", "off"])

    p_msc = sub.add_parser("msc", help="Enable/disable USB mass-storage mode")
    p_msc.add_argument("state", choices=["on", "off"])

    p_export = sub.add_parser("export", help="Export binary log to CSV")
    p_export.add_argument("bin")
    p_export.add_argument("csv")

    args = parser.parse_args()
    if args.command is None:
        parser.print_help()
        return

    port = args.port

    try:
        if args.command == "info":
            print(_cmd(port, "*IDN?"))
        elif args.command == "status":
            print(_cmd(port, "STAT?"))
        elif args.command == "time":
            if args.set:
                print(_cmd(port, "TIME", str(pc_epoch())))
            else:
                print(_cmd(port, "TIME?"))
        elif args.command == "start":
            print(_cmd(port, "LOG", "START"))
        elif args.command == "stop":
            print(_cmd(port, "LOG", "STOP"))
        elif args.command == "rate":
            print(_cmd(port, "RATE", str(args.ms)))
        elif args.command == "thr":
            print(_cmd(port, "THR", f"{args.ch} {args.lo} {args.hi}"))
        elif args.command == "cfg":
            if args.kv:
                print(_cmd(port, "CFG", args.kv))
            else:
                print(_cmd(port, "CFG?"))
        elif args.command == "ls":
            with Transport(port) as ser:
                ser.send_line("LS")
                while True:
                    line = ser.read_line(timeout=5.0)
                    status, body = decode(line)
                    if status == "OK":
                        break
                    print(line)
        elif args.command == "get":
            out = io.BytesIO()
            with Transport(port) as ser:
                data = receive(ser, args.name, out, timeout=10.0)
            Path(args.out).write_bytes(data)
            print(f"Downloaded {len(data)} bytes to {args.out}")
        elif args.command == "del":
            print(_cmd(port, "DEL", args.name))
        elif args.command == "format":
            print(_cmd(port, "FORMAT", "YES"))
        elif args.command == "stream":
            print(_cmd(port, "STREAM", args.state.upper()))
        elif args.command == "msc":
            print(_cmd(port, "MSC", args.state.upper()))
        elif args.command == "export":
            _, records = read_binary_file(args.bin)
            with open(args.csv, "w", newline="") as fp:
                export_csv(records, fp)
            print(f"Exported {len(records)} records to {args.csv}")
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
