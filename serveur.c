#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "match.h"

void affiche_connexion(struct sockaddr_in6 adrclient){
  char adr_buf[INET6_ADDRSTRLEN];
  memset(adr_buf, 0, sizeof(adr_buf));
  
  inet_ntop(AF_INET6, &(adrclient.sin6_addr), adr_buf, sizeof(adr_buf));
  printf("adresse client : IP: %s port: %d\n", adr_buf, ntohs(adrclient.sin6_port));
}


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


int main(int argc, char** args) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <port>\n", args[0]);
    exit(1);
  }

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
  address_sock.sin6_port = htons(atoi(args[1]));
  address_sock.sin6_addr=in6addr_any;
  
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

  //*** le serveur accepte une connexion et cree la socket de communication avec le client ***
  struct sockaddr_in6 adrclient;
  memset(&adrclient, 0, sizeof(adrclient));
  socklen_t size=sizeof(adrclient);
  int sockclient = accept(sock, (struct sockaddr *) &adrclient, &size);
  if(sockclient == -1){
    perror("probleme socket client");
    exit(1);
  }	   

  affiche_connexion(adrclient);

  while(1){
    comunication(sockclient);
  }


  //*** fermeture socket client ***
  close(sockclient);

  //*** fermeture socket serveur ***
  close(sock);
  
  return 0;
}