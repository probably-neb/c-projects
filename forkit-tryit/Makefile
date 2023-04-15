CC = gcc
CFLAGS = -Wall -Werror -pedantic -g

all: forkit tryit

forkit: forkit.o
	$(CC) $(CFLAGS) -o $@ $^

tryit: tryit.o
	$(CC) $(CFLAGS) -o $@ $^

clean: 
	rm -f *.o
	rm -f *.out

verify:
	grep -n '.{79}' {*.c,*.h} || true
