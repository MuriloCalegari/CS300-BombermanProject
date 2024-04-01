CC = gcc
CFLAGS = -Wall -Wextra -g

client: client.c
	$(CC) $(CFLAGS) -o client client.c
	$(CC) $(CFLAGS) -o serveur serveur.c

.PHONY: clean
clean:
	rm -f client serveur