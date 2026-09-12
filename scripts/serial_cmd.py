#!/usr/bin/env python3
"""Queue one console line for scripts/serial_capture.py to send: serial_cmd.py info"""
import os, sys
with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "serial_cmd.txt"), "a") as f:
    f.write(" ".join(sys.argv[1:]) + "\n")
print("[cmd] queued:", " ".join(sys.argv[1:]))
