CC = m68k-atari-mint-gcc
CFLAGS = -nostdlib -s
LDFLAGS = -Wl,--oformat=binary

default: root.bin bootsect.bin

clean:
	rm -f root.bin bootsect.bin

.PHONY: default clean

%.bin: %.S
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

