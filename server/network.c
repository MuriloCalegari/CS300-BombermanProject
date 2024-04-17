#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

// Read and write in a loop
int read_loop(int fd, void * dst, int n, int flags) {
  int received = 0;

  while(received != n) {
    received += recv(fd, dst + received, n - received, flags);
  }

  return received;
}

int write_loop(int fd, void * src, int n, int flags) {
  int sent = 0;

  while(sent != n) {
    sent += send(fd, src + sent, n - sent, flags);
  }

  return sent;
}

int write_loop_udp(int fd, void * src, int n, struct sockaddr_in6 * dest_addr, socklen_t dest_addr_len) {
  int sent = 0;

  while(sent != n) {
    sent += sendto(fd, src + sent, n - sent, 0, (struct sockaddr *) dest_addr, dest_addr_len);
  }

  return sent;
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
  
  return sock;
}