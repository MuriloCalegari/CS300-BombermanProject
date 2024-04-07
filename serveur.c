#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "match.h"
#include "context.h"

#define HEIGHT 16
#define WIDTH 16

int wait_for_next_player(int socket);
void affiche_connexion(struct sockaddr_in6 adrclient);
int prepare_socket_and_listen(int port);
int read_loop(int fd, void * dst, size_t n, int flags);
int write_loop(int fd, void * dst, size_t n, int flags);

int comunication(int sockclient){
  if(sockclient >= 0){

    NewMatchMessage resp;
    
    //reception d'un message
    ActionMessage *buf;
    int r = recv(sockclient, &buf, sizeof(buf), 0);
    switch(GET_CODEREQ(&buf->message_header)){
      case 1:
        SET_CODEREQ(&resp.header, 9);
        SET_ID(&resp.header, ENCODE_PLAYER());
        SET_EQ(&resp.header, 0);
        // TODO UDP , MDIFF ADRMDIDD
        break;
      case 2:
        SET_CODEREQ(&resp.header, 10);
        SET_ID(&resp.header, ENCODE_PLAYER());
        SET_EQ(&resp.header, 0);
        break;
    }

    //envoie du message
    int s = send(sockclient, &resp, sizeof(resp), 0);
    if(s <= 0){
      perror("send");
      return 2;
    }

  }
  return 0;
}

/*
  The main thread is responsible for taking in new gamers in and setting up their matches,
  while matches themselves are executed in separate threads.
*/

int main(int argc, char** args) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <port>\n", args[0]);
    exit(1);
  }

  int sock = prepare_socket_and_listen(args[1]);

  Match *current_match_4_opponents = NULL;
  Match *current_match_2_teams = NULL;

  while(1){
    printf("Waiting for next player to connect...\n");

    int client_socket = wait_for_next_player(sock);
    printf("Player connected. Reading first message from player\n");

    JoinMatchRequest join_request;
    
    read_loop(client_socket, &join_request, sizeof(JoinMatchRequest), 0);
    handle_new_join_request(client_socket, join_request, &current_match_4_opponents, &current_match_2_teams);

    // TODO handle starting a new match on a separate thread if we have enough players

    // comunication(client_socket);
  }


  //*** fermeture socket client ***
  close(client_socket);

  //*** fermeture socket serveur ***
  close(sock);
  
  return 0;
}

void handle_new_join_request(int client_socket, JoinMatchRequest join_request, Match **current_4_opponents, Match **current_2_teams) {
    switch(GET_CODEREQ(&(join_request.header))) { // TODO intellisense saying this is wrong?
      case NEW_MATCH_4_OPPONENTS:
        printf("Player wants to join a match with 4 opponents.");
        if(*current_4_opponents == NULL) {
          printf(" There are no current pending matches, so we'll start one");
          Match *new_match = malloc(sizeof(Match));
          memset(new_match, 0, sizeof(Match));

          new_match->mode = FOUR_OPPONENTS_MODE;
          new_match->players_count = 1;
          new_match->players[0] = 0;
          new_match->sockets_tcp[0] = client_socket;

          // TODO set multicast addr. Need to make kind of unique

          new_match->height = HEIGHT;
          new_match->width = WIDTH;
          new_match->grid = malloc(HEIGHT * WIDTH * sizeof(uint8_t));

          pthread_mutex_init(&new_match->mutex, 0);

          *current_4_opponents = new_match;
        } else {
          Match *current_match = *current_4_opponents;

          pthread_mutex_lock(&current_match->mutex); // Not strictly necessary

          int current_player = current_match->players_count + 1;

          // Update current match status
          current_match->players_count++;
          current_match->players[current_player] = current_match->players_count;
          current_match->sockets_tcp[current_player] = client_socket;

          pthread_mutex_unlock(&current_match->mutex);
        }
        break;
      case NEW_MATCH_2_TEAMS:
        // TODO
        break;
      // TODO handle CLIENT_READY_TO_PLAY messages
      // So I think we should just treat this as a regular header instead of doing the 
      // whole JoinMatchRequest struct? Since we always need to read the header first before we decide
      // what to do next.
    }
}

Match* start_new_match() {
  Match *new_match = (Match*) malloc(sizeof(Match));
  memset(new_match, 0, sizeof(new_match));
}

void *tcp_player_handler(void *arg) {
  handle_player()
}

int prepare_socket_and_listen(int port) {
  //*** creation de la socket serveur ***
  int sock = socket(PF_INET6, SOCK_STREAM, 0);
  if(sock < 0){
    perror("creation socket");
    exit(1);
  }

  //*** creation de l'adresse du destinataire (serveur) ***
  struct sockaddr_in6 address_sock;
  memset(&address_sock, 0, sizeof(address_sock));
  address_sock.sin6_family = AF_INET6;
  address_sock.sin6_port = htons(port);
  address_sock.sin6_addr = in6addr_any;
  
  //*** on lie la socket au port PORT ***
  int r = bind(sock, (struct sockaddr *) &address_sock, sizeof(address_sock));
  if (r < 0) {
    perror("erreur bind");
    exit(2);
  }

  //*** Le serveur est pret a ecouter les connexions sur le port PORT ***
  r = listen(sock, 0);
  if (r < 0) {
    perror("erreur listen");
    exit(2);
  }
}

// Read and write in a loop
int read_loop(int fd, void * dst, size_t n, int flags) {
  int pending = n;
  int received = 0;

  while(received != n) {
    received += recv(fd, dst + received, n - received, 0);
  }

  return received;
}

int write_loop(int fd, void * dst, size_t n, int flags) {
  int pending = n;
  int received = 0;

  while(received != n) {
    received += recv(fd, dst + received, n - received, 0);
  }

  return received;
}

int wait_for_next_player(int socket) {
    //*** le serveur accepte une connexion et cree la socket de communication avec le client ***
    struct sockaddr_in6 client_address;
    memset(&client_address, 0, sizeof(client_address));
    socklen_t size=sizeof(client_address);
    int client_socket = accept(socket, (struct sockaddr *) &client_address, &size);
    if(client_socket == -1){
      perror("probleme socket client");
      exit(1);
    }	   

    affiche_connexion(client_address);

    return client_socket;
}

void affiche_connexion(struct sockaddr_in6 adrclient){
  char adr_buf[INET6_ADDRSTRLEN];
  memset(adr_buf, 0, sizeof(adr_buf));
  
  inet_ntop(AF_INET6, &(adrclient.sin6_addr), adr_buf, sizeof(adr_buf));
  printf("Client connected with : IP: %s port: %d\n", adr_buf, ntohs(adrclient.sin6_port));
}