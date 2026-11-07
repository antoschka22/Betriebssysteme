# Author: Antonio Molina Gradischnig (12421907)
# Program: quadtree

CC = gcc
DEFS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS = -std=c99 -pedantic -Wall -Wextra $(DEFS) -g
LDFLAGS = -lm

TARGET = quadtree
OBJS = quadtree.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

quadtree.o: quadtree.c
	$(CC) $(CFLAGS) -c quadtree.c -o quadtree.o

clean:
	rm -f $(TARGET) $(OBJS)