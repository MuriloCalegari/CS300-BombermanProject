#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void affiche_connexion(struct sockaddr_in6 adrclient){
  char adr_buf[INET6_ADDRSTRLEN];
  memset(adr_buf, 0, sizeof(adr_buf));
  
  inet_ntop(AF_INET6, &(adrclient.sin6_addr), adr_buf, sizeof(adr_buf));
  printf("Client connected with : IP: %s port: %d\n", adr_buf, ntohs(adrclient.sin6_port));
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

int prepare_socket_and_listen(int port) {
  printf("Binding TCP socket to port %d\n", port);
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

  return sock;
}

int setup_udp_listening_socket(int udp_port) {
  printf("Binding UDP socket to port %d\n", udp_port);
  int sock = socket(AF_INET6, SOCK_DGRAM, 0);
  if(sock < 0) {
    perror("socket");
    exit(-1);
  }

  struct sockaddr_in6 addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin6_family = AF_INET6;
  addr.sin6_addr = in6addr_any;
  addr.sin6_port = htons(udp_port);

  if(bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    perror("bind");
    exit(-1);
  }

  // Get ifindex
  
  // printf("Activating SO_REUSEADDR\n");
  // int ok = 1;
  // if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &ok, sizeof(ok)) < 0) {
  //     perror("Error setting SO_REUSE_ADDR");
  //     close(sock);
  //     return 1;
  // }
  
  return sock;
}