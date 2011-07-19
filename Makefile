DEBUG:=
CFLAGS:=$(DEBUG) -DUINPUT_FILE=\"/dev/uinput\"
all: xkeysd test

xkeysd: input.o xkeysd.o
	gcc $(DEBUG) -lconfig -o xkeysd xkeysd.o input.o 

test: input.o test.o
	gcc $(DEBUG) -o test test.o input.o

clean:
	rm -f test xkeysd *.o
