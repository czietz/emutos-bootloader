CC = m68k-atari-mint-gcc
CFLAGS = -nostdlib -s -O2

install.prg: install.c install.h install.rsc root.h bootsect.h
ifdef LIBCMINI
	$(CC) $(CFLAGS) -I$(LIBCMINI)/include -L$(LIBCMINI)/build -o $@ $(LIBCMINI)/build/crt0.o $< -lcmini -lgcc -lgem
else
	@echo "Define LIBCMINI to point to the libcmini directory" && exit 1
endif

root.h: ../root.bin
	xxd -i $< $@

bootsect.h: ../bootsect.bin
	xxd -i $< $@

clean:
	rm -f install.prg

.PHONY: clean
