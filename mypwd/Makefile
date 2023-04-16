CC = gcc
CFLAGS = -Wall -Werror -ansi -pedantic -g

mypwd: mypwd.o
	$(CC) $(CFLAGS) -o $@ $^

clean: 
	rm -f *.out
	rm -f *.o

verify:
	grep -n '.{79}' {*.c,*.h} || true
