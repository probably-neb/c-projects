CC = gcc
CFLAGS = -Wall -Werror -pedantic-errors -Wno-format-truncation

# all: CFLAGS += -O2
# all: mytar

debug: CFLAGS += -DDEBUG -g
debug: mytar

mytar: mytar.o header.o archive.o
	$(CC) $(CFLAGS) -o $@ $^
	cp ./mytar ~/.local/bin/

printfuncs: printfuncsmain.o printfuncs.o 
	$(CC) -o $@ $^

git: exclean debug
	git commit -am "auto make commit $(DATE)" || true
	git push || true

DATE=$(shell date +%F\ %T)
test: git
	ssh bkunkle@unix1.csc.calpoly.edu '( cd now/ && ./tryAsgn4 )' > tryAsgn4.out

clean: 
	rm -f *.out
	rm -f *.o

exclean: clean
	rm -f mytar

macros:
	gcc -dM -E mytar.c
