# http
# Antonio Molina Gradischnig, 12421907

CC=gcc
CFLAGS=-std=c99 -pedantic -Wall -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L -g

.PHONY: all clean
all: server

server: server.c
	$(CC) -o $@ $(CFLAGS) $^

server.tar.gz: Makefile server.c http.h
	tar -cvzf $@ $^

clean:
	rm -rf server server.tar.gz
