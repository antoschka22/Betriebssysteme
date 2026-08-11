# Author: Antonio Molina Gradischnig, 12421907
# Program Name: mygrep
CC := gcc
CFLAGS := -std=c99 -pedantic -Wall -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L -g

HEADERS := $(wildcard *.h)
OBJECTS := $(patsubst %.c, %.o, $(wildcard *.c))

.PHONY: all clean
all: mygrep
%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<
mygrep: $(OBJECTS)

mygrep.tar.gz: decode_argv.c decode_argv.h error_message.c error_message.h grep.c grep.h mygrep.c Makefile
	tar czf $@ $^

clean:
	rm -f mygrep *.o 
