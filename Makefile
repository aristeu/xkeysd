CFLAGS:=-g -DUINPUT_FILE=\"/dev/uinput\"
all: xkeysd test

xkeysd: input.o xkeysd.o
	gcc -g -lconfig -o xkeysd xkeysd.o input.o 

test: input.o test.o
	gcc -g -o test test.o input.o

