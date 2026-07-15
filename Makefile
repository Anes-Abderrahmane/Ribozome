CC = gcc
CFLAGS = -Wall -Wextra -std=c11

all: main

main: main.c
	$(CC) $(CFLAGS) -o main main.c

clean:
	rm -f main
