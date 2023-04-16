CC = gcc
CFLAGS = -Wall -Werror -pedantic-errors -Wno-format-truncation
HOST = $(shell hostname)
CFLAGS += -I/home/neb/poly/q3_2022/csc-357/Given/Talk
LDFLAGS = -L/home/neb/poly/q3_2022/csc-357/Given/Talk/lib64
DEFAULT = debug
ifneq ($(HOST),nebtop)
	CFLAGS += -I/home/pn-cs357/Given/Talk
	LDFLAGS = -L/home/pn-cs357/Given/Talk/lib64
	DEFAULT = all
endif
# CFLAGS += -I/home/neb/poly/q3_2022/csc-357/Given/Talk
# LDFLAGS = -L/home/neb/poly/q3_2022/csc-357/Given/Talk/lib64
LDLIBS = -ltalk -lncurses

default: $(DEFAULT)

debug: CFLAGS += -DDEBUG -g -Wno-unused-variable
debug: mytalk

all: CFLAGS += -O2
all: mytalk

mytalk: mytalk.o 
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

DATE=$(shell date +%F\ %T)
git: exclean debug
	git commit -am "auto make commit $(DATE)" || true
	git push || true

clean: 
	rm -f *.out
	rm -f *.o

exclean: clean
	rm -f mytalk

macros:
	gcc -dM -E mytalk.c
