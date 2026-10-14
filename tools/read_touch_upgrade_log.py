"""Read boot upgrade results without resetting the device or interrupting I2C."""
import argparse
from pathlib import Path
import time
import serial

parser = argparse.ArgumentParser()
parser.add_argument("--port", default="COM1310")
parser.add_argument("--seconds", type=int, default=45)
args = parser.parse_args()
output = Path(__file__).resolve().parents[1] / "build/touch-upgrade-boot.log"
deadline = time.monotonic() + args.seconds
device = serial.Serial(port=None, baudrate=115200, timeout=0.25)
device.dtr = False
device.rts = False
device.port = args.port
device.open()
captured = bytearray()
with device, output.open("wb") as log:
    while time.monotonic() < deadline:
        chunk = device.read(device.in_waiting or 1)
        if chunk:
            captured.extend(chunk)
            log.write(chunk)
            log.flush()
        if b"board ready display=" in captured:
            break
text = captured.decode("utf-8", errors="replace")
for line in text.splitlines():
    if any(tag in line for tag in ("touch_upgrade", "metalio_touch:", "TP RST", "board ready", "panic", "Guru Meditation")):
        print(line)
print(f"Boot log: {output}")
if "upgrade failed" in text:
    raise SystemExit(2)
if "updated and verified V03" not in text and "already current; skipped" not in text:
    print("No explicit upgrade result captured; do not infer upgrade success.")
    raise SystemExit(3)
