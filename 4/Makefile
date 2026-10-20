# @file Makefile
# @author Antonio Molina Gradischnig
# @date 2026-20-10
# @brief Makefile for the Feedback Arc Set project (supervisor and generator)

CC = gcc
DEFS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS = -std=c99 -pedantic -Wall $(DEFS) -g
LDFLAGS = -pthread -lrt

.PHONY: all clean

all: supervisor generator

supervisor: supervisor.o
	$(CC) -o $@ $^ $(LDFLAGS)

generator: generator.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c common.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf *.o supervisor generator