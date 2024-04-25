CC = gcc
CFLAGS = -Wall -Wextra -g -DDEBUG
LIBS = -lncurses

S = server
C = client
COMMON = common

client: client.c ncurses.o network_client.o
	$(CC) $(CFLAGS) -o run_client ncurses.o network_client.o client.c $(LIBS)

server: serveur.c serveur.o network_server.o util.o match.o server_utils.o
	$(CC) $(CFLAGS) -o serveur serveur.o network_server.o util.o match.o server_utils.o

serveur.o: serveur.c
	$(CC) $(CFLAGS) -o serveur.o -c serveur.c

network_server.o: $S/network.c
	$(CC) $(CFLAGS) -o network_server.o -c $S/network.c

network_client.o: $S/network.c
	$(CC) $(CFLAGS) -o network_client.o -c $C/network.c

match.o:  $S/match.c
	$(CC) $(CFLAGS) -o match.o -c $S/match.c

util.o : $(COMMON)/util.c
	$(CC) $(CFLAGS) -o util.o -c $(COMMON)/util.c

server_utils.o: $S/server_utils.c
	$(CC) $(CFLAGS) -o server_utils.o -c $S/server_utils.c

ncurses.o: ncurses/ncurses.c
	$(CC) $(CFLAGS) -o ncurses.o -c ncurses/ncurses.c

.PHONY: clean
clean:
	rm -rf run_client serveur serveur.o network_client.o network_server.o util.o match.o ncurses.o server_utils.o client.dSYM serveur.dSYM run_client.dSYM
