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
// D3: start sector of partition
// D4: device ID (to be passed to loader function)
// D5: byteswap flag (to be passed to loader function)
// A5: loader function
// must only return in case of error
// must preserve D7 in case of error

#define BOOTSECTOROFFS(x) (x-0x3E+start)(pc)
#define BPS_L BOOTSECTOROFFS(0xB)
#define BPS_H BOOTSECTOROFFS(0xC)
#define SPC BOOTSECTOROFFS(0xD)
#define RES BOOTSECTOROFFS(0xE)
#define RDS_L BOOTSECTOROFFS(0x11)
#define RDS_H BOOTSECTOROFFS(0x12)
#define SPF BOOTSECTOROFFS(0x16)
#define START BOOTSECTOROFFS(0x1C)

#define DOS2ATARI(x)    ror.w #8,x

#define PRG_HEADER_SIZE 0x1C
#define START_CLUS      0x1A

#define BP_tbase    0x8
#define BP_tlen     0xC
#define BP_dbase    0x10
#define BP_dlen     0x14
#define BP_bbase    0x18
#define BP_blen     0x1C

#define PH_tlen     0x2
#define PH_dlen     0x6
#define PH_blen     0xA

// assumed to be at relative address 0x3E
start:
    move.l  d7,-(sp)
    
    // allocate sector buffer
    pea     512.w       // used to load physical sectors only
    move.w  #72,-(sp)
    trap    #1
    addq.l  #6,sp
    tst.l   d0
    jbmi    failed
    move.l  d0,a3
    
    // calculate some values based on boot sector
    // start of root directory
    move.w  RES,d6      // count of reserved sectors
    DOS2ATARI(d6)
    jbsr    log2phys
    move.l  d0,d7       // physical sector of first FAT
    move.w  SPF,d0
    DOS2ATARI(d0)
    add.w   d0,d6
    add.w   d0,d6       // assume 2(!) FATs
    jbsr    log2phys
    move.l  d0,d3
    move.l  d0,a6
    
    // calculate number of physical sectors for root dir
    moveq   #0,d1
    move.b  RDS_H,d1
    lsl.w   #8,d1
    move.b  RDS_L,d1
    lsr.w   #4,d1       // 16 dir entries per 512 bytes
    // first data sector
    add.l   d1,a6

next_rootdirsect:
    // load root directory
    moveq   #1,d2       // just one sector at a time
    jsr     (a5)
    move.l  a3,a2
    moveq   #15,d1      // 16 dir entries per 512 bytes

next_file:
    // find filename
    moveq   #10,d0
    move.l  a2,a0
    lea     filename(pc),a1
cmp_filename:
    cmp.b   (a0)+,(a1)+
    dbne    d0,cmp_filename
    jbeq    found_filename
    lea     (32)(a2),a2
    dbra    d1,next_file
    
    addq    #1,d3
    cmp.l   a6,d3
    jbmi    next_rootdirsect

failed:
    move.l  (sp)+,d7
    rts

found_filename:
    move.w  START_CLUS(a2),d6 // start cluster
    DOS2ATARI(d6)
    
    // print message for user
    pea     loading(pc)
    move.w  #9,-(sp)
    trap    #1
    addq.l  #6,sp

    // create basepage
    pea     0.w         // env
    pea     sectcache(pc)    // cmdline: points to zeros
    pea     0.w         // filename
    pea     0x4B0005    // Pexec mode 5
    trap    #1
    lea     0x10(sp),sp
    tst.l   d0
    jbmi     failed
    move.l  d0,-(sp)    // store address of Basepage
    
    move.l  d0,a4
    lea     (0x100-PRG_HEADER_SIZE)(a4),a4     // dirty trick: use last bytes of basepage for PRG header

    // load file following cluster chains
loadfile:
    jbsr    loadcluster
    tst.w   d0
    jbne    loadfile_failed
    jbsr    getnextcl
    move.w  d0,d6       // next cluster
    jbne    loadfile    // d0 is 0 for EOF/error


    // fill in basepage
    move.l  (sp)+,a0
    lea     (0x100-PRG_HEADER_SIZE)(a0),a1 // pointer prg header
    lea     0x100(a0),a2    // pointer to segment starts
    // TEXT
    move.l  a2,BP_tbase(a0)
    move.l  PH_tlen(a1),BP_tlen(a0)
    add.l   PH_tlen(a1),a2
    // DATA
    move.l  a2,BP_dbase(a0)
    move.l  PH_dlen(a1),BP_dlen(a0)
    add.l   PH_dlen(a1),a2
    // BSS
    move.l  a2,BP_bbase(a0)
    move.l  PH_blen(a1),BP_blen(a0)
    
    // relocate: A2 points to relocation table
    lea     0x100(a0),a1    // start of text segment
    move.l  a1,d1
    move.l  (a2)+,d0
    jbeq    reloc_done       // empty relocation table
    add.l   d0,a1
    moveq   #0,d0
next_reloc:
    add.l   d1,(a1)         // relocate
next_byte:
    move.b  (a2)+,d0        // get next entry
    jbeq    reloc_done
    add.l   d0,a1
    cmpi.b  #1,d0
    jbne    next_reloc
    lea     253(a1),a1      // advance by 254 bytes, but 1 has been added before
    jbra    next_byte
    
loadfile_failed:
    move.l  (sp)+,d0        // discard address of basepage
    jbra    failed

    // launch program
reloc_done:
    pea     0.w             // env
    pea     (a0)            // basepage
    pea     0.w             // cmdline
    pea     0x4B0004        // Pexec mode 4
    trap    #1
    
// with EmuTOS we should never return here!
    lea     0x10(sp),sp
    jbra    failed

// convert logical sector to absolute physical sector
// D3: physical start sector of partition
// D6: logical sector number (WORD!)
// returns: D0: phys. sector number (LONG)
log2phys:
    // physical sectors per logical sector
    moveq   #0,d0
    move.b  BPS_H,d0
    lsr.w   #1,d0
    mulu.w  d6,d0
    add.l   d3,d0
    rts

// load a cluster of a file
// D6: cluster number
// A6: physical sector number of data / first cluster
// A4: buffer (modified upon return)
// returns: D0 != 0 in case of error
loadcluster:
    movem.l a3/d3/d7,-(sp)
    moveq   #0,d7
    moveq   #0,d3
    move.b  BPS_H,d3
    lsr.w   #1,d3       // physical sectors per logical sector
    move.b  SPC,d7      // logical sectors per cluster
    mulu.w  d3,d7       // physical sectors per cluster

    move.w  d6,d3
    subq    #2,d3       // first cluster is cluster #2
    mulu.w  d7,d3
    add.l   a6,d3

    move.l  a4,a3
    move.w  d7,d2       // number of sectors
    jsr     (a5)
    tst.w   d0
    jbne    loadclus_err
    mulu.w  #0x200,d7   // number of bytes loaded
    adda.l  d7,a4

loadclus_err:
    movem.l (sp)+,a3/d3/d7
    rts

// gets the next cluster in cluster chain
// D6: cluster number
// D7: phys. start sector of FAT
// A3: scratch buffer for FAT
// returns: D0 = next cluster (0 in case of EOF/error)
getnextcl:
    move.l  d3,-(sp)
    
    // calculate phys. sector to lookup
    moveq   #0,d3
    move.w  d6,d3
    lsr.w   #8,d3    // assume FAT16: 256 cluster entries fit in a sector
    add.l   d7,d3

    lea     sectcache(pc),a0
    cmp.l   (a0),d3 // have sector in cache?
    jbeq    getnext_lookup
    // load new FAT sector
    move.l  d3,(a0) // mark sector as cached
    moveq   #1,d2   // one physical sector
    jsr     (a5)
    tst.w   d0
    jbne    getnextcl_err

getnext_lookup:
    // calculate index into sector
    move.w  d6,d0
    add.w   d6,d0
    andi.w  #0x1ff,d0
    
    // get next cluster entry
    move.w  (d0.w,a3),d0
    DOS2ATARI(d0)
    cmpi.w  #0xFFF7,d0  // 0xFFF7: bad cluster, 0xFFF8-0xFFFF: EOF
    jbcc    getnextcl_err

    move.l  (sp)+,d3
    rts

getnextcl_err:
    moveq   #0,d0
    move.l  (sp)+,d3
    rts

loading:
    .ascii "Load "
filename:
    .ascii "EMUTOS  SYS"
    .even
sectcache:
    .dc.l  0
