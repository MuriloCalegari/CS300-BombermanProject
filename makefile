CC = gcc
CFLAGS = -Wall -Wextra -g -DVERBOSE -Wno-address-of-packed-member
LIBS = -lncurses

S = server
C = client
COMMON = common

SRCS_COMMON = $(COMMON)/util.c \
				ncurses/ncurses.c

SRCS_SERVER = $(S)/network.c \
				$(S)/match.c \
				$(S)/server_utils.c

SRCS_CLIENT = $(C)/network.c

OBJS_COMMON = $(patsubst %.c,%.o,$(SRCS_COMMON))
OBJS_SERVER = $(patsubst %.c,%.o,$(SRCS_SERVER))
OBJS_CLIENT = $(patsubst %.c,%.o,$(SRCS_CLIENT))

all: client serveur

client: $(OBJS_CLIENT) $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o run_client $(OBJS_CLIENT) $(OBJS_COMMON) client.c $(LIBS)

serveur: $(OBJS_SERVER) $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o serveur $(OBJS_SERVER) $(OBJS_COMMON) serveur.c $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	rm -rf run_client serveur $(OBJS_COMMON) $(OBJS_SERVER) $(OBJS_CLIENT) client.dSYM serveur.dSYM run_client.dSYM