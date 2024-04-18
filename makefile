CC = gcc
CFLAGS = -Wall -Wextra -g
LIBS = -lncurses

S = server
COMMON = common

client: client.c ncurses.o
	$(CC) $(CFLAGS) -o run_client ncurses.o client.c $(LIBS)

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

ncurses.o: ncurses/ncurses.c
	$(CC) $(CFLAGS) -o ncurses.o -c ncurses/ncurses.c

.PHONY: clean
clean:
	rm -rf run_client serveur serveur.o network.o util.o match.o ncurses.o client.dSYM serveur.dSYM run_client.dSYM
