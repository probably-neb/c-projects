CC = gcc

DCFLAGS = -Wall -Werror -ansi -pedantic -g

fast: fw.o
	$(CC) -O3 -o fw fw.o

fw: fw.o
	$(CC) $(DCFLAGS) -o $@ $^

profile: fw.o
	$(CC) -pg -O -o fw $^
	./fw test2
	gprof fw gmon.out

clean:
	rm -f **.o
	rm -f vgcore*
	rm -f *.out

show-macros:
	gcc -dM -E fw.c
