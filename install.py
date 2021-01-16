#!/usr/bin/python3

# Copyright (c) 2021 Christian Zietz
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE. 

import sys
import struct

ROOT_FILE = "root.bin"
BOOT_FILE = "bootsect.bin"

if len(sys.argv)<2:
    sys.exit("Usage: %s <hd-image>" % sys.argv[0])


with open(sys.argv[1], "r+b") as f:

    # INSTALL ROOT SECTOR at offset 0
    with open(ROOT_FILE, "rb") as c:
        code = c.read(442)
        f.write(code)

    # FIND FIRST PARTITION
    f.seek(0)
    mbr = f.read(512)
    magic = struct.unpack("<H", mbr[510:])[0] # Intel byte order
    if magic == 0xAA55:
        print("Detected DOS partition table (non-byteswapped)")
        lba = struct.unpack("<L", mbr[0x1BE+8:0x1BE+12])[0]
        swap = False
        print("First partition at LBA = %d" % lba)
        if lba == 0:
            sys.error("Illegal partition table")
        # mark as bootable
        f.seek(0x1BE)
        f.write(b'\x80')
    elif magic == 0x55AA:
        print("Detected DOS partition table (byteswapped)")
        lba = struct.unpack(">HH", mbr[0x1BE+8:0x1BE+12])
        lba = lba[1]*256 + lba[0]
        swap = True
        print("First partition at LBA = %d" % lba)
        if lba == 0:
            sys.error("Illegal partition table")
        # mark as bootable
        f.seek(0x1BF)
        f.write(b'\x80')
    else:
        print("Assuming Atari partition table")
        lba = struct.unpack(">L", mbr[0x1C6+4:0x1C6+8])[0]
        swap = False
        print("First partition at LBA = %d" % lba)
        if (lba == 0) or ((mbr[0x1C6] & 0x7f != 1)): # check partition flag
            sys.error("Illegal partition table")
        # mark as bootable
        f.seek(0x1C6)
        f.write(b'\x81')

    # MAKE ROOT SECTOR BOOTABLE (by correcting checksum)
    f.seek(442)
    f.write(struct.pack(">H", 0))
    f.seek(0)
    mbr = f.read(512)
    sum = 0
    for word in struct.iter_unpack(">H", mbr): # Motorola byte order
        sum += word[0]
    sum = 0x1234 - sum
    sum = sum % 65536 # always positive in Python
    f.seek(442)
    f.write(struct.pack(">H", sum))
    
    # INSTALL BOOT SECTOR
    offset = lba * 512 # Offset of boot sector
    if swap:
        endian = "<"
    else:
        endian = ">"

    f.seek(offset + 0)
    f.write(struct.pack(endian+"HH", 0xe900, 0x603a))     # asl.b #4,d0, bra.s 0x3e (MS-DOS/Windows expects the first byte to be 0xE9/0xEB)

    with open(BOOT_FILE, "rb") as c:
        f.seek(offset + 0x3e)
        code = bytearray(c.read(446))
        # byte-swap code if required
        if swap:
            #code = struct.pack("<%dH"%(len(code)/2), *struct.unpack(">%dH"%(len(code)/2), code))
            code[0::2],code[1::2] = code[1::2],code[0::2]
        f.write(code)
    
    # MAKE BOOT SECTOR BOOTABLE (by correcting checksum)
    f.seek(offset + 508)
    f.write(struct.pack(endian+"H", 0))
    f.seek(offset + 0)
    boot = f.read(512)
    sum = 0
    for word in struct.iter_unpack(endian+"H", boot):
        sum += word[0]

    sum = 0x1234 - sum
    sum = sum % 65536 # always positive in Python
    f.seek(offset + 508)
    f.write(struct.pack(endian+"H", sum))
    