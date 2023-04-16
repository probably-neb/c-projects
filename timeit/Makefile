CC = gcc
CFLAGS = -Wall -Werror -pedantic -g

timeit: timeit.o
	$(CC) $(CFLAGS) -o $@ $^

clean: 
	rm -f *.o
	rm -f *.out
	rm -f timeit

verify:
	grep -n '.{79}' {*.c,*.h} || true
