CC = gcc
CFLAGS = -Wall -Werror -ansi -pedantic-errors

all: CFLAGS += -O2
all: hencode hdecode

debug: CFLAGS += -DDEBUG -g
debug: debughe debughd

debughe: hencode.o printfuncs.o huffman.o
	$(CC) $(CFLAGS) -o hencode $^

debughd: hdecode.o printfuncs.o huffman.o
	$(CC) $(CFLAGS) -o hdecode $^

htable: hencode.o huffman.o
	$(CC) $(CFLAGS) -o htable $^

hencode: hencode.o huffman.o
	$(CC) $(CFLAGS) -o $@ $^

hdecode: hdecode.o huffman.o
	$(CC) $(CFLAGS) -o $@ $^

printfuncs: printfuncsmain.o printfuncs.o huffman.o
	$(CC) -o $@ $^

clean: 
	rm -f *.out
	rm -f *.o

exclean: clean
	rm -f hencode hdecode printfuncs htable

macros:
	gcc -dM -E main.c

verify:
	grep -n '.\{79\}' {*.c,*.h} || true
