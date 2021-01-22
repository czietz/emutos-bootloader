CC = m68k-atari-mint-gcc
CFLAGS = -nostdlib -s -O2
LIBCMINI = /home/czietz/libcmini-experimental

install.prg: install.c install.h install.rsc root.h bootsect.h
	$(CC) $(CFLAGS) -I$(LIBCMINI)/include -L$(LIBCMINI)/build -o $@ $(LIBCMINI)/build/crt0.o $< -lcmini -lgcc -lgem

root.h: ../root.bin
	xxd -i $< $@

bootsect.h: ../bootsect.bin
	xxd -i $< $@

clean:
	rm -f install.prg

.PHONY: clean
