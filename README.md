# EmuTOS hard disk boot-loader

## Purpose

The EmuTOS hard disk boot-loader enables you to boot EmuTOS from a hard disk (or hard disk replacement such as Ultrasatan or Gigafile) without needing any 3rd party hard disk driver. This makes it easy to use EmuTOS as the default OS on your Atari. No hardware modifications (such as replacing ROMs) is required.

## About EmuTOS

[EmuTOS](https://emutos.sourceforge.io/) is a free operating system mainly, but not only, for the Atari ST and its successors. Compared to Atari TOS, it adds additional hardware support, more languages, and a wider range of supported systems, among other things.

## Supported configurations

The EmuTOS hard disk boot-loader is supported on all Atari hardware that can run EmuTOS: Atari ST, MegaST, STe, MegaSTe, TT, and Falcon. It can boot from...

* ACSI hard disks
* IDE hard disks
* SCSI hard disks

Note: TOS 1.x can only boot from ACSI disks. See [below](#supported-hard-disk-interfaces-by-atari-tos-version) for a detailed compatibility overview.

The EmuTOS hard disk boot-loader works with all partitioning schemes supported by EmuTOS:

* Atari-style partitions (also known as "TOS-compatible")
* PC-style partitions (e.g., created on Windows, with FAT16 file system)
* TOS- and Windows-compatible partitions (created, e.g., by HDDRIVER)

## Installation

There are three ways to install the EmuTOS hard disk boot-loader.

1. **Using a ready-made image file**
   Two hard disk image files are [provided for download](https://nightly.link/czietz/emutos-bootloader/workflows/build/master/emutos-bootloader.zip): `sdcard-ide.img` is for installation on IDE devices (without Smartswap), `sdcard-acsi.img` is for all other interfaces: ACSI, SCSI, IDE *with* Smartswap. Writing the appropriate image to a disk (or SD card, CF card, ...) at least 256 MB in size makes the disk bootable. It also creates a TOS- and PC-compatible partition for easy data transfer. Windows users can, for example, use [Win32 Disk Imager](https://sourceforge.net/projects/win32diskimager/) to write the image file.

   *Note*: All data on the target disk is erased by writing the image file.

   *Note:* Currently, the disk image is only provided with an (US-)English version of EmuTOS. See below for updating/replacing it.

2. **Using the EmuTOS-based installer**
   The boot-loader can be installed from a running version of EmuTOS using the [provided installer](https://nightly.link/czietz/emutos-bootloader/workflows/build/master/emutos-bootloader.zip). Copy `EMUTOS.PRG` (in the language of your choice) as well as `INSTALL.PRG` and `INSTALL.RSC` to your Atari and start EmuTOS by running `EMUTOS.PRG`. Note that merely running the PRG version of EmuTOS does not modify your system in any way. Therefore, you can try EmuTOS as extensively as you like before installing the boot-loader.
   Verify that EmuTOS has recognized your first hard disk partition as drive C:. To proceed with the installation, start `INSTALL.PRG`. Locate your copy of `EMUTOS.PRG` and install it to drive C:. Reboot.

   *Note:* By installing the EmuTOS hard disk boot-loader in this way all files on your hard disk are retained. However, any existing hard disk driver (e.g., AHDI, HDDRIVER, ...) is uninstalled and replaced by the EmuTOS boot-loader.

3. **Using the Python script installer** (for advanced users)
   A Python script is provided mainly for developers or for (semi-)automated deployment of the EmuTOS hard disk boot-loader. Additionally, it can use `mcopy` from [GNU mtools](https://www.gnu.org/software/mtools/) to also copy `EMUTOS.PRG` to the disk. The script needs to be called as follows:

   ```
   install.py [-ideswap] /dev/sdX path/to/emutos.prg
   ```

   On most operating systems, root or administrator rights are required. Replace `/dev/sdX`  with the device name of the target disk (or SD card, CF card, ...). Replace `path/to/emutos.prg` with the path to `EMUTOS.PRG`  (in the language of your choice). Pass the optional `-ideswap` argument if the target disk (or card) is to be used as IDE device (without Smartswap) in the Atari.

   *Note:* By installing the EmuTOS hard disk boot-loader in this way all files on your hard disk are retained. However, any existing hard disk driver (e.g., AHDI, HDDRIVER, ...) is deinstalled and replaced by the EmuTOS boot-loader.

   *Note:* Be extra-careful to specify the **correct** target device. There is no prompt to confirm your choice!

## Updating EmuTOS

To update the installed version of EmuTOS, simply replace `C:\EMUTOS.SYS`  on the target disk with a copy of `EMUTOS.PRG` (in the language of your choice and renamed to `EMUTOS.SYS`), e.g., from a current [snapshot of EmuTOS](https://sourceforge.net/projects/emutos/files/snapshots/).

## Supported hard disk interfaces by Atari TOS version

To launch the EmuTOS hard disk boot-loader, TOS on your Atari needs to be able to auto-boot from the respective hard disk interface. This is the same requirement as booting a hard disk driver.

|         | ACSI      | IDE                 | SCSI      |
| :------ | --------- | ------------------- | --------- |
| TOS 1.x | supported | -                   | -         |
| TOS 2.x | supported | supported           | -         |
| TOS 3.x | supported | needs a patched TOS | supported |
| TOS 4.x | -         | supported           | supported |

## License

The EmuTOS hard disk boot-loader is distributed under the MIT license. EmuTOS is distributed under GPL.
