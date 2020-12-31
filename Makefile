CC = m68k-atari-mint-gcc
CFLAGS = -nostdlib -s
LDFLAGS = -Wl,--oformat=binary

ROOT_MAX=442
BOOT_MAX=446

default: root.bin bootsect.bin sizecheck

clean:
	rm -f root.bin bootsect.bin

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

.PHONY: default clean sizecheck

%.bin: %.S
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

