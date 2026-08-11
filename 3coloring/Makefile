# Project: 3coloring
# Author: Antonio Molina Gradischnig, 12421907

PROGRAMS = supervisor generator
CC = gcc
CFLAGS = -std=c99 -pedantic -Wall -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L -g
LDFLAGS = -lm -pthread -lrt
OBJECTS_ALL = $(patsubst %.c, %.o, $(wildcard *.c))
OBJECTS = $(filter-out $(patsubst %,%.o,$(PROGRAMS)), $(OBJECTS_ALL))

all: $(OBJECTS) $(PROGRAMS)

%.o: %.c $(wildcard *.h)
	$(CC) $(CFLAGS) -c $< -o $@ $(LDFLAGS)

$(PROGRAMS): %: %.o $(wildcard *.o)
	$(CC) $(CFLAGS) -o $@ $< $(OBJECTS) $(LDFLAGS)

clean:
	rm -vf $(wildcard *.o) $(PROGRAMS) $(wildcard *.txt)

testcases: $(wildcard *.c) $(wildcard *.h) all
	./tests.sh;

docs: $(wildcard *.c) $(wildcard *.h)
	doxygen
