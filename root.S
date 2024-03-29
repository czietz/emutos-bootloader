// Copyright (c) 2021 Christian Zietz

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

// called with:
// D3: 'DMAr' if TOS supports DMAread() function
// D4: device ID if TOS supports DMAread() function
// D7: ACSI device ID in bits 7-5
// must only return in case of error
// must preserve D7 in case of error

#define drvbits 0x4c2
#define sysbase 0x4f2
#define bootdev 0x446
#define flock   0x43e
#define hz200   0x4ba

#define dma_mode 0xFFFF8606
// offsets to dma_mode
#define dma_data -2
#define dma_low  7
#define dma_mid  5
#define dma_high 3
#define gpip     0xFFFFFA01

#define Kbshift 0x000BFFFF
#define Kb_BitAlt 3
#define DMAread 42
#define DMAr    0x444D4172
#define MSDOS   0x55AA
#define MSDOS_S 0xAA55
#define BOOT_CHECK 0x1234

#define PT_signature (start+0x1fe)

#define PT_A_flag    (start+0x1c6)
#define PT_A_start   0x4
#define PT_A_length  0xc

#define PT_M_flag    (start+0x1be)
#define PT_M_start   0x8
#define PT_M_length  0x10

#define buffer       (start+0x200)

start:
    // clear drive bit for C: in case booting fails
    bclr    #('C'-'A'),(drvbits+3).w    // drvbits is a LONG
    
    // skip booting if Alt key is pressed
    pea     Kbshift
    trap    #13
    addq.l  #4,sp
    btst    #Kb_BitAlt,d0
    jbne    failed
    
    // Select whether to use XBIOS DMAread or our own ACSI loader
    lea     xbiosload(pc),a5
    cmpi.l  #DMAr,d3            // magic number of DMAread is supported
    jbne    use_own_loader
    // Ataris DMAread is *extremely* slow, hence only
    // use it for IDE & SCSI where we do not have our own loader
    cmpi.w  #7,d4
    jbhi    findpartition
    move.w  d4,d7
    lsl.w   #5,d7               // put device ID in bits 7-5

use_own_loader:
    move.w  d7,d4               // device ID in bits 7-5
    lea     acsiload(pc),a5

    // determine type of partition table
findpartition:
    moveq   #0,d5               // byteswap marker
    moveq   #3,d0               // number of partitions to check - 1
    lea     PT_signature(pc),a0
    cmpi.w  #MSDOS,(a0)
    jbeq    msdospart
    cmpi.w  #MSDOS_S,(a0)
    jbeq    msdospart_swapped
    
    // handle Atari-type partition
    lea     (PT_A_flag-PT_A_length)(pc),a0
atari_next:
    lea     PT_A_length(a0),a0
    cmpi.b  #0x81,(a0)          // active partition?
    dbeq    d0,atari_next
    jbne    failed              // no partition found
    move.l  PT_A_start(a0),d3   // start sector
    jbra    load_boot

    // handle MS-DOS-type partition (unswapped)
msdospart:
    lea     (PT_M_flag-PT_M_length)(pc),a0
msdos_next:
    lea     PT_M_length(a0),a0
    cmpi.b  #0x80,(a0)          // active partition?
    dbeq    d0,msdos_next
    jbne    failed              // no partition found
    move.l  PT_M_start(a0),d3   // start sector
    ror.w   #8,d3               // byte-swap start sector number
    swap    d3
    ror.w   #8,d3
    jbra    load_boot

    // handle MS-DOS-type partition (swapped)
msdospart_swapped:
    st      d5                  // byteswap marker
    lea     (PT_M_flag-PT_M_length)(pc),a0
msdos_s_next:
    lea     PT_M_length(a0),a0
    cmpi.b  #0x80,1(a0)         // active partition? (offset to to byte swap)
    dbeq    d0,msdos_s_next
    jbne    failed              // no partition found
    move.l  PT_M_start(a0),d3   // start sector
    swap    d3                  // byte-order is alredy correct, only swap words
    
    // load boot sector, check for 0x1234 checksum, execute
load_boot:
    lea     buffer(pc),a3
    moveq   #1,d2               // one sector
    jsr     (a5)
    tst.w   d0
    jbne    failed
    clr.b   0x25(a3)            // clear Windows "dirty" flag that would corrupt the checksum
    move.w  #255,d1             // note that D0.w is guaranteed to be 0
boot_sum:
    add.w   (a3)+,d0
    dbra    d1,boot_sum
    cmpi.w  #BOOT_CHECK,d0
    jbne    failed              // bootsector is not executable
    jsr     buffer(pc)
    
failed:
    move.l  sysbase.w,a0
    cmpi.w  #0x1987,0x1a(a0)    // prevent endless loop for TOS 1.02
    jbcc    failed2
    move.w  #0xe0,d7
failed2:
    clr.w   bootdev.w           // so TOS does not try to boot from C:
    rts

    // use DMAread to load a sector
    // D2: number of sectors to load (max. 128)
    // D3: sector number
    // D4: device ID (passed by TOS)
    // D5: byte-swap flag
    // A3: buffer
    // returns: status in D0
    // trashes A0-A2,D0-D2
xbiosload:
    move.w  d4,-(sp)
    move.l  a3,-(sp)
    move.w  d2,-(sp)        // number of sectors
    move.l  d3,-(sp)
    move.w  #DMAread,-(sp)
    trap    #14
    move.w  6(sp),d2        // restore D2 trashed by XBIOS
    lea     0xe(sp),sp
    // do byte swap if required
    tst.w   d0              // no swapping in case DMAread failed
    jbne    load_end
    tst.w   d5              // byte-swap flag
    jbeq    load_end
swap_sector:
    move.l  a3,a0
    mulu.w  #256,d2         // number of *WORDS* that were read
    subq    #1,d2           // for DBRA
next_swap:
    move.w  (a0),d1
    ror.w   #8,d1
    move.w  d1,(a0)+
    dbra    d2,next_swap
load_end:
    rts

    // load a sector from ACSI device
    // D2: number of sectors to load (max. 255)
    // D3: sector number
    // A3: buffer
    // D4: device ID (in bits 7-5, as passed by TOS)
    // returns: status in D0 (-1 in case of timeout)
    // trashes A0-A2,D0-D2
acsiload:
    st      flock.w
    
    // according to Ataris ACSI guide some devices require an additional wait
del2ticks:
    moveq.l #2,d1       // wait 2 ticks between commands
    add.l   hz200.w,d1
del2ticks_wait:
    cmp.l   hz200.w,d1
    jbge    del2ticks_wait
    
    lea     dma_mode.w,a0
    lea     dma_data(a0),a1

    movem.l d3/a3,-(sp)
    // write buffer address to DMA
    move.b  7(sp),dma_low(a0)
    move.b  6(sp),dma_mid(a0)
    move.b  5(sp),dma_high(a0)
    // store sector number in cmd buffer
    lea     cmd_buf(pc),a2
    move.b  d4,d0       // device ID in bits 7-5
    andi.b  #0xe0,d0
    ori.b   #0x08,d0    // READ cmd
    move.b  d0,1(a2)
    move.b  1(sp),5(a2)
    move.b  2(sp),9(a2)
    move.b  3(sp),13(a2)
    move.b  d2,17(a2)   // number of sectors
    addq.l  #8,sp
    // reset DMA and switch to read mode
    move.w  #0x198,(a0)
    move.w  #0x98,(a0)
    move.w  d2,(a1)     // number of sectors
    
    // start command
    move.w  #0x88,(a0)
    moveq   #5,d2
next_cmd_byte:
    move.l  (a2)+,(a1)

    // wait for completion or timeout
    move.l  hz200.w,d1
    addi.l  #600,d1     // wait 600 ticks = 3 seconds
del3sec:
    btst    #5,gpip.w
    jbeq    exit_wait
    cmp.l   hz200.w,d1
    jbge    del3sec
    moveq   #-1,d0
exit_wait:

    jbne    acsi_end    // (Non-)Zero flag is set depending on error condition
    dbra    d2,next_cmd_byte

    move.w  #0x8a,(a0)  // select status byte
    move.w  (a1),d0     // read status byte
    and.w   #0xFF,d0    // mask undefined bits

acsi_end:
    sf      flock.w
    rts


cmd_buf:
    dc.l    0x0008008A  // READ command (device ID gets inserted here)
    dc.l    0x0000008A  // sector number...
    dc.l    0x0000008A  // ... gets inserted ...
    dc.l    0x0000008A  // ... here
    dc.l    0x0001008A  // one sector (gets changed)
    dc.l    0x0000000A  // last cmd byte and start xfer
