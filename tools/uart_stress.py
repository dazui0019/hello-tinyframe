"""UART RX stress sender. Run: uv run python tools/uart_stress.py --port COM5.

Data is an uninterrupted sequence 00..FF, starting at 00 on every run.
Reset the MCU before each run. No response is required from the MCU.
"""

import argparse
import json
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


BOUNDARY_LENGTHS = (1, 63, 64, 65, 127, 128, 129, 255, 256)
PATTERN = bytes(range(256))


def positive_int(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be greater than zero")
    return number


def nonnegative_float(value):
    import math
    number = float(value)
    if not math.isfinite(number) or number < 0:
        raise argparse.ArgumentTypeError("must be finite and nonnegative")
    return number


def make_data(offset, length):
    """Preserve the sequence across blocks, including short writes."""
    start = offset % 256
    return (PATTERN * ((start + length + 255) // 256))[start:start + length]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list-ports", action="store_true")
    parser.add_argument("--port", help="e.g. COM5")
    parser.add_argument("--baud", type=positive_int, default=115200)
    parser.add_argument("--duration", type=nonnegative_float, default=60,
                        help="continuous phase duration in seconds; 0 = boundaries only")
    parser.add_argument("--chunk", type=positive_int, default=4096,
                        help="continuous phase host write size (not a UART frame)")
    parser.add_argument("--gap-ms", type=nonnegative_float, default=0,
                        help="wait after each continuous block has drained")
    parser.add_argument("--boundary-gap-ms", type=nonnegative_float, default=50)
    parser.add_argument("--skip-boundaries", action="store_true")
    parser.add_argument("--startup-delay", type=nonnegative_float, default=1)
    parser.add_argument("--report", type=Path, help="optional JSON report path")
    args = parser.parse_args()

    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        parser.exit(2, "Missing pyserial: run uv sync, then uv run python tools/uart_stress.py\n")

    if args.list_ports:
        ports = list(list_ports.comports())
        for port in ports:
            print(f"{port.device}: {port.description}")
        if not ports:
            print("No serial ports found.")
        return 0
    if not args.port:
        parser.error("--port is required unless --list-ports is used")
    if args.chunk > 1024 * 1024:
        parser.error("--chunk must not exceed 1048576")

    report = {
        "started_utc": datetime.now(timezone.utc).isoformat(),
        "port": args.port,
        "baud": args.baud,
        "format": "8N1, no flow control",
        "pattern": "00..FF repeated, starting at 00",
        "duration_requested_s": args.duration,
        "chunk": args.chunk,
        "gap_ms": args.gap_ms,
        "boundary_bytes": 0,
        "host_written_bytes": 0,
        "status": "running",
    }
    started = None
    continuous_started = None
    continuous_ended = None
    continuous_base = 0

    def send_all(port, length):
        data = make_data(report["host_written_bytes"], length)
        sent = 0
        while sent < len(data):
            count = port.write(data[sent:])
            if count is None or count <= 0:
                raise RuntimeError("Serial write made no progress")
            sent += count
            report["host_written_bytes"] += count

    try:
        with serial.Serial(args.port, args.baud,
                           bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE,
                           stopbits=serial.STOPBITS_ONE, timeout=0.1,
                           write_timeout=5, xonxoff=False, rtscts=False,
                           dsrdtr=False) as port:
            print(f"{args.port} / {args.baud} / 8N1. Reset MCU before each run.")
            print("RX-only test: no echo expected. Ctrl+C stops the test.")
            time.sleep(args.startup_delay)
            started = time.monotonic()
            if not args.skip_boundaries:
                print("Boundary lengths:", BOUNDARY_LENGTHS)
                for length in BOUNDARY_LENGTHS:
                    send_all(port, length)
                    report["boundary_bytes"] += length
                    port.flush()
                    time.sleep(args.boundary_gap_ms / 1000)

            continuous_base = report["host_written_bytes"]
            continuous_started = time.monotonic()
            deadline = continuous_started + args.duration
            next_progress = continuous_started + 1
            while time.monotonic() < deadline:
                send_all(port, args.chunk)
                # Drain each block so host output buffering cannot create
                # an unbounded backlog or extend the test by many blocks.
                port.flush()
                if args.gap_ms:
                    time.sleep(args.gap_ms / 1000)
                now = time.monotonic()
                if now >= next_progress:
                    count = report["host_written_bytes"] - continuous_base
                    print(f"TX {report['host_written_bytes']} bytes; "
                          f"continuous {count / (now - continuous_started):.0f} B/s")
                    next_progress = now + 1
            continuous_ended = time.monotonic()
            report["status"] = "completed"
    except KeyboardInterrupt:
        report["status"] = "interrupted"
        print("\nInterrupted; pending host bytes may not have drained.")
    except (serial.SerialException, OSError, RuntimeError) as exc:
        report["status"] = "error"
        report["error"] = str(exc)
        print(f"Error: {exc}", file=sys.stderr)
    finally:
        now = time.monotonic()
        report["elapsed_s"] = round(now - started, 3) if started else 0
        report["continuous_bytes"] = (
            report["host_written_bytes"] - continuous_base
            if continuous_started is not None else 0
        )
        report["continuous_elapsed_s"] = (
            round((continuous_ended or now) - continuous_started, 3)
            if continuous_started is not None else 0
        )
        print(json.dumps(report, indent=2))
        print("Host writes do not prove MCU reception. After RX drains, compare")
        print("host_written_bytes with rx_stats.offered_bytes/written_bytes/read_bytes.")
        print("Expect dropped_bytes=0 and mismatch_count=0.")
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return {"completed": 0, "interrupted": 130, "error": 1}[report["status"]]


if __name__ == "__main__":
    sys.exit(main())
