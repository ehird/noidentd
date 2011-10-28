CC=gcc
CFLAGS=-std=c99 -Wall -Wextra -pedantic -Os
PREFIX=/usr/local

.PHONY: all install uninstall clean

all: noidentd

noidentd: noidentd.c
	$(CC) $(CFLAGS) $< -o $@

install:
	install -D -m755 noidentd $(PREFIX)/sbin/noidentd

uninstall:
	rm $(PREFIX)/sbin/noidentd

clean:
	rm -f noidentd
