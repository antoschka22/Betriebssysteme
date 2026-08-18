# http
# Antonio Molina Gradischnig, 12421907

CC=gcc
CFLAGS=-std=c99 -pedantic -Wall -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L -g

.PHONY: all clean
all: client

client: client.c
	$(CC) -o $@ $(CFLAGS) $^

client.tar.gz: Makefile client.c http.h
	tar -cvzf $@ $^

clean:
	rm -rf client client.tar.gz
