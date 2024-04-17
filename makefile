CC = gcc
CFLAGS = -Wall -Wextra -g

S = server
COMMON = common

client: client.c
	$(CC) $(CFLAGS) -o client client.c

server: serveur.c serveur.o network.o util.o match.o
	$(CC) $(CFLAGS) -o serveur serveur.o network.o util.o match.o

serveur.o: serveur.c
	$(CC) $(CFLAGS) -o serveur.o -c serveur.c

network.o: $S/network.c
	$(CC) $(CFLAGS) -o network.o -c $S/network.c

match.o:  $S/match.c
	$(CC) $(CFLAGS) -o match.o -c $S/match.c

util.o : $(COMMON)/util.c
	$(CC) $(CFLAGS) -o util.o -c $(COMMON)/util.c

.PHONY: clean
clean:
	rm -rf client serveur serveur.o network.o util.o match.o client.dSYM /serveur.dSYM