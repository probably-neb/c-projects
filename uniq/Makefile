CC = gcc
CFLAGS = -Wall -Werror -ansi -pedantic -g

all: uniq

uniq: uniq.o
	$(CC) $(CFLAGS) -o $@ $^

tryLab02: uniq
	//home/pn-cs357/demos/tryLab02

test: uniq
	./uniq < test1 > me.test1
	uniq < test1 > unix.test1
	diff -a me.test1 unix.test1

clean: 
	rm *.o || true
	rm *.test1 || true
	rm *.out || true
