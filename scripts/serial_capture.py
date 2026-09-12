#!/usr/bin/env python3
"""Single owner of the HPlayer0 serial port: prints the log, relays console commands.

    python3 scripts/serial_capture.py [seconds] >> bench.log &
    python3 scripts/serial_cmd.py info          # queues a line for the capture to send

One process must own the port: a second opener drops DTR on close and the firmware then
discards everything it writes (USBCDC only writes while DTR is up). Commands are appended
to a file next to this script (serial_cmd.txt) and sent from here.
"""
import glob, os, sys, time
import serial

secs = float(sys.argv[1]) if len(sys.argv) > 1 else 3600
CMD = os.path.join(os.path.dirname(os.path.abspath(__file__)), "serial_cmd.txt")
deadline = time.time() + secs


def ports():
    return (glob.glob('/dev/serial/by-id/usb-Hemisphere*')
            + glob.glob('/dev/serial/by-id/usb-Espressif*') + glob.glob('/dev/ttyACM*'))


def open_port():
    while time.time() < deadline:
        for p in ports():
            try:
                s = serial.Serial(p, 115200, timeout=0.2)
                s.dtr = True
                s.rts = False
                print(f"[capture] opened {p}", flush=True)
                s.write(b"\n")   # the first line after an open can arrive mangled: spend it
                return s
            except Exception:
                time.sleep(0.3)
    return None


ser = open_port()
if ser is None:
    print("[capture] no port")
    sys.exit(1)
buf = b""
while time.time() < deadline:
    try:
        if os.path.exists(CMD) and os.path.getsize(CMD) > 0:
            with open(CMD, "r+") as f:
                lines = f.read().splitlines()
                f.truncate(0)
            for l in lines:
                if l.strip():
                    ser.write((l.strip() + "\n").encode())
                    ser.flush()
                    print(f"[capture] > {l.strip()}", flush=True)
        data = ser.read(4096)
    except Exception as e:
        print(f"[capture] port error {e}, reopening", flush=True)
        try:
            ser.close()
        except Exception:
            pass
        time.sleep(1)
        ser = open_port()
        if ser is None:
            break
        continue
    if data:
        buf += data
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            print(line.decode('utf-8', 'replace').rstrip(), flush=True)
