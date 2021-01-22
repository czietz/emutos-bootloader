#include <string.h>
#include <osbind.h>
#include <gem.h>
#include <mint/cookie.h>
#include <mint/sysvars.h>
#include <mint/ostruct.h>
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

#define BOOTCODE_START 0x3E
#define BOOTCODE_CHKSUM 508
#define ROOTCODE_START 0
#define ROOTCODE_CHKSUM 442

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

    char **text;
    rsrc_gaddr(R_FRSTR, ST_SELECT, &text);

    strcpy(emutos_prg, "A:\\EMUTOS*.PRG");
    emutos_prg[0] += Dgetdrv(); /* select current drive */

    wind_update(BEG_MCTRL);
    res = fsel_exinput(emutos_prg, fname, &exbutton, *text);
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

/* Install EMUTOS.SYS, boot sector and root sector */
static int install_emutos(const char* fname)
{
    HDINFO** punptr = (HDINFO**)0x516L; /* Note: definition in MiNTLib is wrong */
    _BPB *bpb;
    long inhand, outhand;
    long res;
    unsigned short dev;

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
    /* asl.b #4,d0, bra.s 0x3e (MS-DOS/Windows expects the first byte to be 0xE9/0xEB) */
    memcpy(buffer,"\xe9\x00\x60\x3a", 4);
    memcpy(buffer+BOOTCODE_START, ___bootsect_bin, ___bootsect_bin_len);
    *(unsigned short *)(buffer+BOOTCODE_CHKSUM) += checksum(buffer);
    res = Rwabs(2 | 1, buffer, 1, 0, DRIVE_C);
    if (res < 0) {
        return 0;
    }

    /* Install root sector */
    
    res = Super(0ul);
    dev = (*punptr)->v_p_un[DRIVE_C]; /* device number for drive */
    Super(res);
    if (dev & 0x80) {
        /* Invalid flag for PUN */
        return 0;
    }

    dev += 2; /* As required by Rwabs in physical mode */
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

int main(void)
{
    int exit = 0;
    OBJECT *menu;
    char **text;
    short int msg[8];

    long res;

    aes_global[0] = 0;
    appl_init();
    if (!gl_ap_version) {
        Cconws("Cannot be run from AUTO folder!\r\n");
        return 1;
    }

    res = rsrc_load("install.rsc");
    if (res == 0) {
        form_alert(1, "[1][Failed loading RSC file!][ Exit ]");
        appl_exit();
        return 1;
    }

    /* Show menu and set mouse cursor */
    rsrc_gaddr(R_TREE, MNU_MAIN, &menu);
    menu_bar(menu, 100);
    menu_bar(menu, 1);
    graf_mouse(ARROW, 0);

    /* Main event loop */
    while (!exit) {
        evnt_mesag(msg);
        if (msg[0] == MN_SELECTED) {
            switch (msg[4]) {

                case MNU_ABOUT:
                    rsrc_gaddr(R_FRSTR, AL_ABOUT, &text);
                    form_alert(1, *text);
                    break;

                case MNU_LOCATE:
                    res = locate_emutos();
                    if (res > 0) {
                        /* Valid EmuTOS.PRG selected */
                        menu_ienable(menu, MNU_INSTALL, 1);
                    } else if (res < 0) {
                        /* Wasn't a valid EmuTOS.PRG */
                        rsrc_gaddr(R_FRSTR, AL_NOEMUTOS, &text);
                        form_alert(1, *text);
                        menu_ienable(menu, MNU_INSTALL, 0);
                    } else {
                        /* User did not select a file */
                        menu_ienable(menu, MNU_INSTALL, 0);
                    }
                    break;

                case MNU_INSTALL:
                    rsrc_gaddr(R_FRSTR, AL_INSTALL, &text);
                    res = form_alert(2, *text);
                    if (res == 1) {
                        /* User answered yes? */
                        res = install_emutos(emutos_prg);
                        if (!res) {
                            rsrc_gaddr(R_FRSTR, AL_INSTALLERR, &text);
                            form_alert(1, *text);
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
