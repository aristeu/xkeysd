CFLAGS:=-g
test: input.o test.o
	gcc -g -o test test.o input.o

