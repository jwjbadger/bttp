CC = gcc
CFLAGS = -Wall -Wpedantic -o server

server:
	$(CC) main.c $(CFLAGS)

clean:
	rm server
