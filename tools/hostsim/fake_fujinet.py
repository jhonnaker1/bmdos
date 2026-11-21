#!/usr/bin/env python3
"""A stand-in FujiNet-PC for the PC simulator's FujiNet test (run_tests.sh).

FujiNet-PC built for RS-232, with BoIP on, listens for an emulator's
serial port on TCP and then carries the serial line's bytes both ways
(fujinet-firmware's lib/hardware/BoIPChannel.cpp, lib/bus/rs232). This
listens the same way, on 127.0.0.1:PORT, takes the simulator's connection,
and answers each byte that comes in with its case flipped, printing what
it got.

  fake_fujinet.py PORT [SECONDS]

Part of BMDOS. See LICENSE for licensing details.
"""
import socket, sys, time

port = int(sys.argv[1])
seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 60
end = time.time() + seconds
server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server.bind(("127.0.0.1", port))
server.listen(1)
server.settimeout(seconds)
print("fake_fujinet: listening on %d" % port, flush=True)
try:
    s, _ = server.accept()
except OSError:
    print("fake_fujinet: no connection", flush=True)
    sys.exit(1)
print("fake_fujinet: connected", flush=True)
got = b""
s.settimeout(0.2)
while time.time() < end:
    try:
        data = s.recv(256)
    except socket.timeout:
        continue
    except OSError:
        break
    if not data:
        break
    got += data
    print("fake_fujinet: got %r" % got, flush=True)
    s.sendall(data.swapcase())
print("fake_fujinet: done, got %r" % got, flush=True)
