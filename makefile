CC = gcc
CFLAGS = -Wall -Wextra -g

client: client.c
	$(CC) $(CFLAGS) -o client client.c

.PHONY: clean
clean:
	rm -f client