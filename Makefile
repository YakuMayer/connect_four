# Makefile
CC = gcc
CFLAGS = -Wall -Wextra -O2

all: server

server: server.o game.o
	$(CC) $(CFLAGS) -o server server.o game.o

server.o: server.c game.h
	$(CC) $(CFLAGS) -c server.c

game.o: game.c game.h
	$(CC) $(CFLAGS) -c game.c

clean:
	rm -f *.o server
