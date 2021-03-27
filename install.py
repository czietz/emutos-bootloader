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


# This installer needs to consider three scenarios:

# 1. ACSI / SCSI device or IDE device on an interface with Smartswap:
#    In these cases, there is no difference in byte order between byte order
#    on the Atari and the PC, i.e., there is just a single byte order. Thus:
#    - Partition table: single byte order.
#    - Root sector code: single byte order.
#    - Boot sector data and code: single byte order.

# 2. IDE device (without Smartswap), PC-compatible partitioning:
#    In this case, the partition table and boot sector are in PC byte order,
#    otherwise, they would not be readable. The root sector code, however,
#    needs to be in Atari byte order (=swapped), to be executable. Thus:
#    - Partition table: PC byte order.
#    - Root sector code: Atari byte order (=swapped from PC perspective).
#    - Boot sector data and code: PC byte order.

# 3. IDE device (without Smartswap), Atari-compatible partitioning:
#    In this case, everything is in Atari byte order, therefore unreadable
#    on the PC.
#    - Partition table: Atari byte order (=swapped from PC perspective).
#    - Root sector code: Atari byte order (=swapped from PC perspective).
#    - Boot sector data and code: Atari byte order (=swapped from PC perspective).

# Further complication I: Scenario 3 can be auto-detected by checking the
# partition table. However, scenarios 1 and 2 cannot be distinguished from each
# other. Users need to tell the installer if they intend to use the disk image
# as IDE device (without Smartswap).

# Further complication II: Scenarios 1 and 3 could use either an PC-style or an
# Atari-style partition table. In contrast, in scenario 2 -- which is intended
# for PC compatibility -- an Atari-style partition table does not make sense.

# Further complication III: In scenario 3 (where the entire file system is
# byte-swapped from the PC's perspective), 'mcopy' cannot install EMUTOS.SYS
# into the image. Users will have to copy it themselves.


import sys
import struct
import os
import subprocess

ROOT_FILE = "root.bin"
BOOT_FILE = "bootsect.bin"

if len(sys.argv)<2:
    sys.exit("Usage: %s [-ideswap] <hd-image> [emutos.prg]" % sys.argv[0])

# Scenario 2 (see above): IDE with PC-compatible partitioning
if sys.argv[1] == "-ideswap":
    ideswap = True
    del sys.argv[1] # skip this argument
else:
    ideswap = False

with open(sys.argv[1], "r+b") as f:

    # FIND FIRST PARTITION AND DETERMINE SCENARIO
    f.seek(0)
    mbr = f.read(512)
    # PC MBR magic
    pcmagic = struct.unpack("<H", mbr[510:])[0] # Intel byte order
    # Atari partition IDs for non-byte-swapped and byte-swapped scenarios
    atarimagic  = mbr[0x1C7:0x1CA]
    atarimagic2 = mbr[0x1C6:0x1C7] + mbr[0x1C9:0x1CA] + mbr[0x1C8:0x1C9] 
    if pcmagic == 0xAA55:
        print("Detected DOS partition table (non-byteswapped)")
        lba = struct.unpack("<L", mbr[0x1BE+8:0x1BE+12])[0]
        print("First partition at LBA = %d" % lba)
        if lba == 0:
            sys.error("Illegal partition table")
        # mark as bootable
        f.seek(0x1BE)
        f.write(b'\x80')
        # could be either scenario 1 or 2, let user decide
        if ideswap:
            scenario = 2
        else:
            scenario = 1
    elif pcmagic == 0x55AA:
        print("Detected DOS partition table (byteswapped)")
        lba = struct.unpack(">HH", mbr[0x1BE+8:0x1BE+12])
        lba = lba[1]*256 + lba[0]
        print("First partition at LBA = %d" % lba)
        if lba == 0:
            sys.error("Illegal partition table")
        # mark as bootable
        f.seek(0x1BF)
        f.write(b'\x80')
        # this is scenario 3
        scenario = 3
    elif atarimagic == b"BGM" or atarimagic == b"GEM":
        print("Detected Atari partition table (non-byteswapped)")
        lba = struct.unpack(">L", mbr[0x1C6+4:0x1C6+8])[0]
        print("First partition at LBA = %d" % lba)
        if (lba == 0) or ((mbr[0x1C6] & 0x7f != 1)): # check partition flag
            sys.error("Illegal partition table")
        # mark as bootable
        f.seek(0x1C6)
        f.write(b'\x81')
        # scenario 2 is unplausible with an Atari partition table
        scenario = 1
    elif atarimagic2 == b"BGM" or atarimagic2 == b"GEM":
        print("Detected Atari partition table (byteswapped)")
        lba = struct.unpack("<HH", mbr[0x1C6+4:0x1C6+8])
        lba = lba[0]*256 + lba[1]
        print("First partition at LBA = %d" % lba)
        if (lba == 0) or ((mbr[0x1C7] & 0x7f != 1)): # check partition flag
            sys.error("Illegal partition table")
        # mark as bootable
        f.seek(0x1C7)
        f.write(b'\x81')
        # this is scenario 3
        scenario = 3
    else:
        sys.exit("ERROR: No supported partition detected!")

    print("DEBUG: scenario %d" % scenario)

    # INSTALL ROOT SECTOR at offset 0
    f.seek(0)
    with open(ROOT_FILE, "rb") as c:
        code = bytearray(c.read(442))
        # swap code for IDE devices
        if scenario >= 2:
            code[0::2],code[1::2] = code[1::2],code[0::2]
        f.write(code)

    # MAKE ROOT SECTOR BOOTABLE (by correcting checksum)
    if scenario >= 2:
        endian = "<"
    else:
        endian = ">"
    f.seek(442)
    f.write(struct.pack(endian+"H", 0))
    f.seek(0)
    mbr = f.read(512)
    sum = 0
    for word in struct.iter_unpack(endian+"H", mbr): # Motorola byte order
        sum += word[0]
    sum = 0x1234 - sum
    sum = sum % 65536 # always positive in Python
    f.seek(442)
    f.write(struct.pack(endian+"H", sum))
    
    # INSTALL BOOT SECTOR
    offset = lba * 512 # Offset of boot sector
    if scenario == 3:
        endian = "<"
    else:
        endian = ">"

    f.seek(offset + 0)
    f.write(struct.pack(endian+"HH", 0xe900, 0x603a))     # asl.b #4,d0, bra.s 0x3e (MS-DOS/Windows expects the first byte to be 0xE9/0xEB)

    with open(BOOT_FILE, "rb") as c:
        f.seek(offset + 0x3e)
        code = bytearray(c.read(446))
        # byte-swap code if required
        if scenario == 3:
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

if len(sys.argv) >= 3:
    if scenario == 3:
        sys.exit("Copying EmuTOS.PRG is only supported on images where the filesystem is in PC byte order")
    print("Copying '%s'" %  sys.argv[2])
    try:
        # Use Mtools mcopy: -o to overwrite existing file, -i to specify image file and offset, mtools_skip_check to relax FAT checks
        myenv = os.environ
        myenv["mtools_skip_check"] = "1"
        subprocess.run(["mcopy", "-o", "-i", "%s@@%d" % (sys.argv[1], offset), sys.argv[2], "::/EMUTOS.SYS"], check=True, env = myenv)
    except FileNotFoundError:
        sys.exit("Copying EmuTOS.PRG requires mcopy from GNU Mtools")
    except subprocess.CalledProcessError:
        sys.exit("ERROR while copying EmuTOS.PRG")
