#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

void genere_entete(uint16_t codereq, uint16_t id, uint16_t eq, uint16_t *entete){
    *entete = (codereq << 3) | (id << 1) | eq;
}

int connexion(int port, char* addr){
    //*** creation de la socket ***
    int fdsock = socket(PF_INET6, SOCK_STREAM, 0);
    if(fdsock == -1){
        perror("creation socket");
        return -1;
    }

    //*** creation de l'adresse du destinataire (serveur) ***
    struct sockaddr_in6 address_sock;
    memset(&address_sock, 0,sizeof(address_sock));
    address_sock.sin6_family = AF_INET6;
    address_sock.sin6_port = htons(port);
    inet_pton(AF_INET6, addr, &address_sock.sin6_addr);

    //*** demande de connexion au serveur ***
    int r = connect(fdsock, (struct sockaddr *) &address_sock, sizeof(address_sock));
    if(r == -1){
        perror("echec de la connexion");
        close(fdsock);
        return -1;
    }

    return fdsock;
}


int main(int argc, char** args){
    int fdsock;
    int port = atoi(args[1]);
    char* addr = args[2];

    if((fdsock = connexion(port, addr)) < 0){
        perror("connexion");
        return 1;
    }

    printf("connection réussit\n");

    uint16_t entete;
    uint16_t codereq; // reste à voir comment mettre les valeurs dedans (peut etre avec le truc graphique)
    uint16_t id; // reste à voir comment mettre les valeurs dedans (peut etre avec le truc graphique)
    uint16_t eq; // reste à voir comment mettre les valeurs dedans (peut etre avec le truc graphique)
    genere_entete(codereq,id,eq,&entete);

    //*** envoie d'un message ***
    int ecrit = send(fdsock, &entete, sizeof(uint16_t), 0);
    if(ecrit <= 0){
        perror("erreur ecriture");
        exit(3);
    }

    return 0;
}