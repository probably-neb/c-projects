CC = gcc
CFLAGS = -Wall -Werror -ansi -pedantic -g

detab: detab.o
	$(CC) $(CFLAGS) -o $@ $^

test: detab
	./detab < test1 > test1out
	diff -s test1out test1sol
	
clean:
	rm *.o
