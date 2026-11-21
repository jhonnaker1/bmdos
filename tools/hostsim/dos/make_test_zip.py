#!/usr/bin/env python3
"""Makes bmdostest.zip, the PC simulator's test content (run_tests.sh).

Two tiny DOS programs, assembled by hand here (no assembler needed), so
the tests need no game:

  BEEP.COM  sounds the PC speaker at 1 kHz (the timer's channel 2) until
            a key is pressed, then prints BEEP DONE.
  JOY.COM   waits for the joystick's first button (port 201h, bit 4 low),
            then prints FIRE PRESSED.
  SERIAL.COM sends BMDOS out of COM1 (the BIOS's INT 14h), then waits for
            five bytes to come back in and prints them (the FujiNet test:
            fake_fujinet.py answers each byte with its case flipped).

Part of BMDOS. See LICENSE for licensing details.
"""
import sys, zipfile

BEEP = bytes([
    0xB0, 0xB6, 0xE6, 0x43,        # mov al,0B6h / out 43h,al: channel 2, square wave
    0xB8, 0xA9, 0x04,              # mov ax,1193 (1193182 / 1000 Hz)
    0xE6, 0x42, 0x88, 0xE0, 0xE6, 0x42,  # out 42h,al / mov al,ah / out 42h,al
    0xE4, 0x61, 0x0C, 0x03, 0xE6, 0x61,  # in al,61h / or al,3 / out 61h,al: speaker on
    0xB4, 0x00, 0xCD, 0x16,        # mov ah,0 / int 16h: wait for a key
    0xE4, 0x61, 0x24, 0xFC, 0xE6, 0x61,  # speaker off
    0xB4, 0x09, 0xBA, 0x25, 0x01,  # mov ah,9 / mov dx,msg (0125h)
    0xCD, 0x21, 0xC3,              # int 21h / ret
]) + b"BEEP DONE\r\n$"
assert BEEP.index(b"BEEP") == 0x25

JOY = bytes([
    0xBA, 0x01, 0x02,              # 100: mov dx,201h
    0xEC,                          # 103: in al,dx
    0xA8, 0x10,                    # 104: test al,10h: button 1 (low when held)
    0x75, 0xF8,                    # 106: jnz 100h
    0xB4, 0x09, 0xBA, 0x10, 0x01,  # 108: mov ah,9 / mov dx,0110h
    0xCD, 0x21, 0xC3,              # 10D: int 21h / ret
]) + b"FIRE PRESSED\r\n$"
assert JOY.index(b"FIRE") == 0x10

SERIAL = bytes([
    0xBE, 0x27, 0x01,              # 100: mov si,msg (0127h)
    0xAC,                          # 103: lodsb
    0x3C, 0x24,                    # 104: cmp al,'$'
    0x74, 0x08,                    # 106: je 110h
    0xB4, 0x01, 0x31, 0xD2,        # 108: mov ah,1 / xor dx,dx (COM1)
    0xCD, 0x14,                    # 10C: int 14h: send AL
    0xEB, 0xF3,                    # 10E: jmp 103h
    0xB9, 0x05, 0x00,              # 110: mov cx,5
    0xB4, 0x02, 0x31, 0xD2,        # 113: mov ah,2 / xor dx,dx
    0xCD, 0x14,                    # 117: int 14h: receive into AL
    0xF6, 0xC4, 0x80,              # 119: test ah,80h (timed out)
    0x75, 0xF5,                    # 11C: jnz 113h: wait on
    0x88, 0xC2, 0xB4, 0x02,        # 11E: mov dl,al / mov ah,2
    0xCD, 0x21,                    # 122: int 21h: print it
    0xE2, 0xED,                    # 124: loop 113h
    0xC3,                          # 126: ret
]) + b"BMDOS$"
assert SERIAL.index(b"BMDOS") == 0x27

out = sys.argv[1] if len(sys.argv) > 1 else "bmdostest.zip"
with zipfile.ZipFile(out, "w") as z:
    z.writestr("BEEP.COM", BEEP)
    z.writestr("JOY.COM", JOY)
    z.writestr("SERIAL.COM", SERIAL)
