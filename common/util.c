#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <poll.h>
#include <arpa/inet.h>

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