#
# Projekt: IOS projekt 2
# Autor:   Radim Kubis, xkubis03
# Datum:   14.4.2012
#
# Pouziti:
#   - preklad:      make
#   - spustit       make run
#   - zabalit:      make pack
#   - vycistit:     make clean
#

NAME=readerWriter

CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic -pthread

ALLFILES=readerWriter.c Makefile

$(NAME): readerWriter.c
	$(CC) $(CFLAGS) readerWriter.c -o $(NAME)

.PHONY: run pack clean

run:
	./readerWriter 1 1 1 0 0 -

pack:
	tar -cvzf $(NAME).tar.gz $(ALLFILES)

clean:
	rm -f $(NAME)
