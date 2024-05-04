#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <poll.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>
#include "util.h"

pthread_t *launch_thread(void *(*start_routine)(void *), void *arg) {
    pthread_t *thread = malloc(sizeof(pthread_t)); // Consider storing somewhere

    int r = pthread_create(thread, NULL, start_routine, arg);
    if (r != 0) {
        perror("pthread_create");
        return NULL;
    }

    return thread;
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
    int just_sent = send(fd, src + sent, n - sent, flags);

    if (just_sent == -1) {
      perror("send");
      return -1;
    }

    sent += just_sent;
  }

  return sent;
}

int write_loop_udp(int fd, void * src, int n, struct sockaddr_in6 * dest_addr, socklen_t dest_addr_len) {
  int sent = 0;

  while(sent != n) {
    int just_sent = sendto(fd, src + sent, n - sent, 0, (struct sockaddr *) dest_addr, dest_addr_len);

    if (just_sent == -1) {
      perror("sendto");

      // print address
      char str[INET6_ADDRSTRLEN];
      inet_ntop(AF_INET6, &(dest_addr->sin6_addr), str, INET6_ADDRSTRLEN);
      printf("Address: %s\n", str);
  
      return -1;
    } else {
      // #ifdef VERBOSE
      // char address[INET6_ADDRSTRLEN];
      // inet_ntop(AF_INET6, &(dest_addr->sin6_addr), address, INET6_ADDRSTRLEN);
      // int port = ntohs(dest_addr->sin6_port);

      // DEBUG_PRINTF("Sent %d bytes to %s:%d\n", just_sent, address, port);
      // #endif
    }

    sent += just_sent;
  }

  return sent;
}