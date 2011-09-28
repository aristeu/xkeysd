VERSION:=1.0
DEBUG:=
USE_LOCAL_INPUT_H:=0
CFLAGS:=$(DEBUG) -DUINPUT_FILE=\"/dev/uinput\" -DUSE_LOCAL_INPUT_H=$(USE_LOCAL_INPUT_H)
SYSCONFDIR:=etc
SBINDIR:=sbin
DESTDIR:=/usr/local
docdir:=$(DESTDIR)/share/doc/
all: xkeysd test


xkeysd: input.o xkeysd.o
	gcc $(DEBUG) -lconfig -o xkeysd xkeysd.o input.o 

test: input.o test.o
	gcc $(DEBUG) -o test test.o input.o

install: xkeysd
	mkdir -p $(DESTDIR)/$(SBINDIR)
	cp xkeysd $(DESTDIR)/$(SBINDIR)
archive:
	git archive --format=tar --prefix=xkeysd-$(VERSION)/ v$(VERSION) | bzip2 >xkeysd-$(VERSION).tar.bz2 
clean:
	rm -f test xkeysd *.o
