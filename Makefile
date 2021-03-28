CC = m68k-atari-mint-gcc
CFLAGS = -nostdlib -s
LDFLAGS = -Wl,--oformat=binary

# Root sector:
#   code starts at offset 0, 
#   (DOS) partition table starts at offset 0x1BC = 444,
#   bytes 442-443 are used to set the correct checksum.
ROOT_MAX=442

# Boot sector:
#   code starts at offset 0x3E = 62,
#   bytes 508-509 are used to set the correct checksum,
#   bytes 510-511 are reserved for 0xAA55 magic number.
BOOT_MAX=446

default: root.bin bootsect.bin sizecheck

clean:
	-rm -f root.bin bootsect.bin emutos.sys sdcard-acsi.img sdcard-ide.img

# Check that maximum binary size is not exceeded
sizecheck:
	@if [ `cat root.bin | wc -c` -gt $(ROOT_MAX) ]; then \
		echo "Root sector binary must be max. $(ROOT_MAX) bytes!"; \
		exit 1; \
	fi
	@if [ `cat bootsect.bin | wc -c` -gt $(BOOT_MAX) ]; then \
		echo "Boot sector binary must be max. $(BOOT_MAX) bytes!"; \
		exit 1; \
	fi

.PHONY: default clean sizecheck sdcard-images

%.bin: %.S
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

emutos.sys:
	-rm -rf emutos-temp/
	git clone --depth=1 https://github.com/emutos/emutos.git emutos-temp
	make -j2 -C emutos-temp prg UNIQUE=us
	mv emutos-temp/emutosus.prg emutos.sys
	upx -qq emutos.sys

sdcard-images: sdcard-acsi.img sdcard-ide.img

sdcard-acsi.img: emutos.sys sdcard-template.bin default
	cp -f sdcard-template.bin $@
	./install.py $@ emutos.sys

sdcard-ide.img: emutos.sys sdcard-template.bin default
	cp -f sdcard-template.bin $@
	./install.py -ideswap $@ emutos.sys