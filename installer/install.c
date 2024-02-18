/* Copyright (c) 2021 - 2024 Christian Zietz

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.

 */

#include <string.h>
#include <osbind.h>
#include <gem.h>
#include <mint/sysvars.h>
#include "install.h"
#include "root.h"
#include "bootsect.h"

#define MAX_PATH 1024
#define MAX_FILE (8+1+3)

#define PRG_MAGIC 0x601A
#define ETOS_MAGIC "ETOS"
#define EMUTOS_MIN_SIZE 120000ul

#define DRIVE_C 2
#define BPB_FAT16 1
#define COOKIE_XHDI 0x58484449L

#define WINDOWS_DIRTY_FLAG 0x25
#define BOOTCODE_START 0x3E
#define BOOTCODE_CHKSUM 508
#define ROOTCODE_START 0
#define ROOTCODE_CHKSUM 442

#define ROOTCODE_MAGICOFFS 510
#define ROOTCODE_MSDOSMAGIC 0x55AA

#define PARTTAB_MSDOS 0x1BE
#define PARTTAB_ATARI 0x1C6

char emutos_prg[MAX_PATH+1] = {0};
char buffer[4096] __attribute__ ((aligned (2)));

/* Search in memory. */
static const char* memmem(const char* mem, size_t mem_len, const char *needle, size_t needle_len)
{
    size_t j;

    if (needle_len > mem_len) {
        return NULL;
    }

    for (j=0; j <= mem_len-needle_len; j++)
    {
        if (memcmp(&mem[j], needle, needle_len) == 0) {
            return &mem[j];
        }
    }

    return NULL;
}

/* 
 * A (loose) check that file is a valid version of EmuTOS.PRG.
 * Returns 0 if the file does not exist / cannot be opened.
 * Returns 1 if the file (probably) is a version of EmuTOS.PRG.
 * Returns -1 if the file is something else.
*/
static int is_emutos_prg(const char* fname)
{
        long handle, res;
        /* Check that file exists / can be opened */
        handle = Fopen(fname, 0);
        if (handle <= 0) {
            return 0;
        }

        /* Loose check that it's a version of EmuTOS.PRG */
        res = Fread(handle, sizeof(buffer), buffer);

        if (res != sizeof(buffer)) {
            Fclose(handle);
            return -1;
        }

        /* Reject files that are too small */
        res = Fseek(0, handle, 2);
        Fclose(handle);
        if (res < EMUTOS_MIN_SIZE) {
            return -1;
        }

        /* Reject file that is *not* a PRG */
        if (*(unsigned short*)buffer != PRG_MAGIC) {
            return -1;
        }

        /* Reject a file that does not contain the "ETOS" magic in the first 4k */
        if (!memmem(buffer, sizeof(buffer), ETOS_MAGIC, strlen(ETOS_MAGIC))) {
            return -1;
        }

        return 1;
}

/* Let's the user choose a file to install.
 * Returns 0 if the file does not exist / cannot be opened / user cancelled.
 * Returns 1 if the selected file (probably) is a version of EmuTOS.PRG.
 * Returns -1 if the selected file is something else.
*/
static int locate_emutos(void)
{
    long res;
    short exbutton = 0;
    char fname[MAX_FILE+1] = {0};
    char *ptr;

    char *text;
    rsrc_gaddr(R_STRING, ST_SELECT, &text);

    strcpy(emutos_prg, "A:\\EMU*.PRG");
    emutos_prg[0] += Dgetdrv(); /* select current drive */

    wind_update(BEG_MCTRL);
    res = fsel_exinput(emutos_prg, fname, &exbutton, text);
    wind_update(END_MCTRL);

    if ((res > 0) && (exbutton > 0)) {
        /* No error and user clicked OK */

        /* Build file name from returns from fsel_exinput. */
        ptr = strrchr(emutos_prg, '\\');
        if (!ptr) {
            ptr = emutos_prg;
        } else {
            ptr++;
        }
        strcpy(ptr, fname);

        return is_emutos_prg(emutos_prg);
    }

    return 0;
}

/* Calculate the missing amount to reach a checksum of 0x1234. */
static unsigned short checksum(const char* buffer)
{
    const unsigned short* buf = (const unsigned short*)buffer;
    unsigned short checksum = 0;
    int k;

    for (k=0; k<256; k++) {
        checksum += buf[k];
    }
    return 0x1234 - checksum;
}

#define SWPL(x) ((((x) & 0x000000FFul) << 24) | (((x) & 0x0000FF00ul) << 8) | (((x) & 0x00FF0000ul) >> 8) | (((x) & 0xFF000000ul) >> 24))

typedef struct {
    unsigned char active;
    unsigned char filler[7]; /* CHS and type */
    unsigned long start; /* little endian */
    unsigned long size;  /* little endian */
} __attribute__((packed)) MSDOS_PART;

typedef struct {
    unsigned char active;
    unsigned char id[3];
    unsigned long start;
    unsigned long size;
} __attribute__((packed)) ATARI_PART;

/* Finds partition corresponding to start sector part_start and marks it as active/bootable */
static int mark_partition_active(char* rootsect, long part_start)
{
    int retval = 0;
    int k;
    ATARI_PART* ap;
    MSDOS_PART* mp;
    
    if (*(unsigned short*)(rootsect+ROOTCODE_MAGICOFFS) == ROOTCODE_MSDOSMAGIC) {

        /* Scan MSDOS partition table */
        mp = (MSDOS_PART*)(rootsect+PARTTAB_MSDOS);
        for (k=0; k<4; k++) {
            if (SWPL(mp->start) == part_start) {
                mp->active |= 0x80; /* mark as bootable */
                retval = 1;
                break;
            }
            mp++;
        }

    } else {

        /* Assume and scan Atari partition table */
        ap = (ATARI_PART*)(rootsect+PARTTAB_ATARI);
        for (k=0; k<4; k++) {
            if (ap->start == part_start) {
                ap->active |= 0x80; /* mark as bootable */
                retval = 1;
                break;
            }
            ap++;
        }
        
    }

    return retval;
}

/* Install EMUTOS.SYS, boot sector and root sector */
static int install_emutos(const char* fname)
{
    HDINFO** punptr = (HDINFO**)0x516L; /* Note: definition in MiNTLib is wrong */
    _BPB *bpb;
    long inhand, outhand;
    long res;
    unsigned short dev;
    unsigned long part_start;

    /* Check requirements for loader: FAT16 */

    bpb = Getbpb(DRIVE_C);
    if (!bpb || !(bpb->bflags & BPB_FAT16)) {
        return 0;
    }

    /* Copy file to C:\EMUTOS.SYS */

    inhand = Fopen(fname, 0);
    if (inhand <= 0) {
        return 0;
    }

    outhand = Fcreate("C:\\EMUTOS.SYS", 0);
    if (outhand <= 0) {
        Fclose(inhand);
        return 0;
    }

    while (1) {
        res = Fread(inhand, sizeof(buffer), buffer);
        if (res == 0) {
            /* EOF */
            break;
        } else if (res < 0) {
            /* Error */
            Fclose(outhand);
            Fclose(inhand);
            return 0;
        }

        if (Fwrite(outhand, res, buffer) != res) {
            /* Error */
            Fclose(outhand);
            Fclose(inhand);
            return 0;
        }
    }

    Fclose(outhand);
    Fclose(inhand);

    /* Install boot sector */

    res = Rwabs(0, buffer, 1, 0, DRIVE_C);
    if (res < 0) {
        return 0;
    }
    /* asl.b #5,d0; sub.w a5,d0; bra.s 0x3e
     * MS-DOS/Windows expects the first byte to be 0xE9/0xEB
     * a fairly common SD-to-IDE adapter checks for 0xEB .. 0x90
     */
    memcpy(buffer,"\xeb\x00\x90\x4d\x60\x38", 6);
    *(buffer+WINDOWS_DIRTY_FLAG) = 0;   /* clear Windows "dirty" flag */
    memcpy(buffer+BOOTCODE_START, ___bootsect_bin, ___bootsect_bin_len);
    *(unsigned short *)(buffer+BOOTCODE_CHKSUM) += checksum(buffer);
    res = Rwabs(2 | 1, buffer, 1, 0, DRIVE_C);
    if (res < 0) {
        return 0;
    }

    /* Install root sector */
    
    res = Super(0ul);
    dev = (*punptr)->v_p_un[DRIVE_C]; /* device number for drive */
    part_start = (*punptr)->pstart[DRIVE_C];
    Super(res);
    if (dev & 0x80) {
        /* Invalid flag for PUN */
        return 0;
    }

    dev += 2; /* As required by Rwabs in physical mode */
    
    /* Find partition table entry corresponding to drive C: and mark active */
    res = Rwabs(8 | 0, buffer, 1, 0, dev);
    if (res < 0) {
        return 0;
    }
    res = mark_partition_active(buffer, part_start);
    if (!res) {
        return 0;
    }
    res = Rwabs(8 | 1, buffer, 1, 0, dev);
    if (res < 0) {
        return 0;
    }
    
    /* Install actual code: this requires non-byteswapped access, like TOS would do during boot */
    res = Rwabs((1<<7) | 8 | 0, buffer, 1, 0, dev); /* Don't byteswap: Needs EmuTOS extension */
    if (res < 0) {
        return 0;
    }
    memcpy(buffer+ROOTCODE_START, ___root_bin, ___root_bin_len);
    *(unsigned short *)(buffer+ROOTCODE_CHKSUM) += checksum(buffer);
    res = Rwabs((1<<7) | 8 | 1, buffer, 1, 0, dev); /* Don't byteswap: Needs EmuTOS extension */
    if (res < 0) {
        return 0;
    }

    return 1;
}

static int alertbox(int default_btn, int id)
{
    char *text;
    rsrc_gaddr(R_STRING, id, &text);
    return form_alert(default_btn, text);
}

int main(void)
{
    int exit = 0;
    OBJECT *menu;
    short int msg[8];

    long res;

    aes_global[0] = 0;
    appl_init();
    if (!gl_ap_version) {
        Cconws("Cannot be run from AUTO folder!\r\n");
        return 1;
    }

    res = rsrc_load(RSC_NAME ".rsc");
    if (res == 0) {
        form_alert(1, "[1][Failed loading RSC file!][ Exit ]");
        appl_exit();
        return 1;
    }

    /* Show menu and set mouse cursor */
    rsrc_gaddr(R_TREE, MNU_MAIN, &menu);
    menu_bar(menu, 1);
    graf_mouse(ARROW, 0);

    /* Main event loop */
    while (!exit) {
        evnt_mesag(msg);
        if (msg[0] == MN_SELECTED) {
            switch (msg[4]) {

                case MNU_ABOUT:
                    alertbox(1, AL_ABOUT);
                    break;

                case MNU_LOCATE:
                    res = locate_emutos();
                    if (res > 0) {
                        /* Valid EmuTOS.PRG selected */
                        menu_ienable(menu, MNU_INSTALL, 1);
                    } else if (res < 0) {
                        /* Wasn't a valid EmuTOS.PRG */
                        alertbox(1, AL_NOEMUTOS);
                        menu_ienable(menu, MNU_INSTALL, 0);
                    } else {
                        /* User did not select a file */
                        menu_ienable(menu, MNU_INSTALL, 0);
                    }
                    break;

                case MNU_INSTALL:
                    res = alertbox(2, AL_INSTALL);
                    if (res == 1) {
                        /* User answered yes? */
                        res = install_emutos(emutos_prg);
                        if (!res) {
                            alertbox(1, AL_INSTALLERR);
                        } else {
                            alertbox(1, AL_SUCCESS);
                        }
                    }
                    break;

                case MNU_QUIT:
                    exit = 1;
                    break;

                default:
                    break;
            }

            /* Reset menu selection state */
            menu_tnormal(menu, msg[4], 1);
            menu_tnormal(menu, msg[3], 1);
        }
    }

    menu_bar(menu, 0);
    rsrc_free();
    appl_exit();
    return 0;
}
