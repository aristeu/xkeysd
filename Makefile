DEBUG:=
CFLAGS:=$(DEBUG) -DUINPUT_FILE=\"/dev/uinput\"
SYSCONFDIR:=etc
SBINDIR:=sbin
DESTDIR:=/usr/local
all: xkeysd test

xkeysd: input.o xkeysd.o
	gcc $(DEBUG) -lconfig -o xkeysd xkeysd.o input.o 

test: input.o test.o
	gcc $(DEBUG) -o test test.o input.o

install: xkeysd
	mkdir -p $(DESTDIR)/$(SBINDIR) $(DESTDIR)/$(SYSCONFDIR)
	cp xkeysd $(DESTDIR)/$(SBINDIR)
	cp sample.conf $(DESTDIR)/$(SYSCONFDIR)/xkeysd.conf
clean:
	rm -f test xkeysd *.o
