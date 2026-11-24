#!/usr/bin/env python3
"""Makes a 1.44 MB FAT12 floppy image with one file on it, for the PC
simulator's disk image test (run_tests.sh, test usbimage).

make_test_img.py ZIP NAME IMAGE: copies NAME (such as BEEP.COM) out of
ZIP (bmdostest.zip, from make_test_zip.py) onto a new floppy IMAGE. No
mtools or mkfs needed.

Part of BMDOS. See LICENSE for licensing details.
"""
import struct, sys, zipfile

zip_path, name, out = sys.argv[1], sys.argv[2], sys.argv[3]
data = zipfile.ZipFile(zip_path).read(name)

SECTOR = 512
SECTORS = 2880           # 80 tracks, 2 heads, 18 sectors
FAT_SECTORS = 9
ROOT_ENTRIES = 224
ROOT_SECTORS = ROOT_ENTRIES * 32 // SECTOR
DATA_START = 1 + 2 * FAT_SECTORS + ROOT_SECTORS
clusters = (len(data) + SECTOR - 1) // SECTOR
assert 1 <= clusters < 2000

img = bytearray(SECTORS * SECTOR)

# Boot sector with the BIOS parameter block (DOS 4 extended)
boot = bytearray(SECTOR)
boot[0:3] = b"\xEB\x3C\x90"
boot[3:11] = b"BMDOS   "
struct.pack_into("<HBHBHHBHHHLL", boot, 11,
                 SECTOR, 1, 1, 2, ROOT_ENTRIES, SECTORS, 0xF0, FAT_SECTORS,
                 18, 2, 0, 0)
struct.pack_into("<BBBL11s8s", boot, 36, 0, 0, 0x29, 0x0B3D05, b"BMDOSTEST  ", b"FAT12   ")
boot[62] = 0xF4                      # hlt, should it be booted
boot[510:512] = b"\x55\xAA"
img[0:SECTOR] = boot

# FAT12: entries 0 and 1, then the file's clusters, one after another
fat = [0xFF0, 0xFFF] + [c + 1 for c in range(3, clusters + 2)] + [0xFFF]
fat_bytes = bytearray(FAT_SECTORS * SECTOR)
for i in range(0, len(fat), 2):
    a = fat[i]
    b = fat[i + 1] if i + 1 < len(fat) else 0
    o = i * 3 // 2
    fat_bytes[o] = a & 0xFF
    fat_bytes[o + 1] = (a >> 8) | ((b & 0xF) << 4)
    fat_bytes[o + 2] = b >> 4
for n in range(2):
    o = (1 + n * FAT_SECTORS) * SECTOR
    img[o:o + len(fat_bytes)] = fat_bytes

# Root directory: the volume label, then the file
root = (1 + 2 * FAT_SECTORS) * SECTOR
base, ext = name.upper().split(".")
date = ((2026 - 1980) << 9) | (11 << 5) | 24
img[root:root + 32] = struct.pack("<11sB10sHHHL", b"BMDOSTEST  ", 0x08, b"", 0, date, 0, 0)
img[root + 32:root + 64] = struct.pack("<11sB10sHHHL", (base.ljust(8) + ext.ljust(3)).encode(),
                                       0x20, b"", 0, date, 2, len(data))
img[DATA_START * SECTOR:DATA_START * SECTOR + len(data)] = data

open(out, "wb").write(img)
