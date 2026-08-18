# makefile for myexpand
# Antonio Molina Gradischnig, 12421907

.PHONY: all clean

COMMAND_NAME := forksort

OBJECT_FILES := $(patsubst %.c, %.o, $(wildcard *.c))
HEADER_FILES := $(wildcard *.h)

CC := gcc
CFLAGS:= -std=c99 -pedantic -Wall -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L -g

all: $(COMMAND_NAME)

$(COMMAND_NAME): $(OBJECT_FILES)

%.o: %.c $(HEADER_FILES)
	$(CC) $(CFLAGS) -c $<

run: $(COMMAND_NAME)
	./$(COMMAND_NAME)

clean:
	rm -rf $(COMMAND_NAME) *.o
