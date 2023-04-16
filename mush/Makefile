CC = gcc
CFLAGS = -Wall -Werror -pedantic-errors -Wno-format-truncation
HOST = $(shell hostname)
CFLAGS += -I/home/neb/poly/q3_2022/csc-357/Given/Mush/libmush/include
LDFLAGS = -L/home/neb/poly/q3_2022/csc-357/Given/Mush/libmush/lib64
DEFAULT = debug
ifneq ($(HOST),nebtop)
	CFLAGS += -I/home/pn-cs357/Given/Mush/libmush/include
	LDFLAGS = -L/home/pn-cs357/Given/Mush/libmush/lib64
	DEFAULT = all
endif
# CFLAGS += -I/home/neb/poly/q3_2022/csc-357/Given/Mush
# LDFLAGS = -L/home/neb/poly/q3_2022/csc-357/Given/Mush/lib64
LDLIBS = -lmush 

default: $(DEFAULT)

debug: CFLAGS += -g -Wno-unused-variable
debug: mush2

all: CFLAGS += -O2
all: mush2

mush2: mush.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

DATE=$(shell date +%F\ %T)
git: exclean debug
	git commit -am "auto make commit $(DATE)" || true
	git push || true

clean: 
	rm -f *.out
	rm -f *.o

exclean: clean
	rm -f mymush2

macros:
	gcc -dM -E mymush2.c
