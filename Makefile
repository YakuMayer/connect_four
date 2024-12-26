CC = gcc
CFLAGS = -Wall -O2
TARGET = server

all: $(TARGET)

$(TARGET): server.o game.o
	$(CC) $(CFLAGS) -o $(TARGET) server.o game.o

server.o: server.c game.h
	$(CC) $(CFLAGS) -c server.c

game.o: game.c game.h
	$(CC) $(CFLAGS) -c game.c

clean:
	rm -f *.o $(TARGET)
