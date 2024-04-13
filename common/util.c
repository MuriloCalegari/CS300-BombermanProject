#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <poll.h>

int launch_thread(void *(*start_routine)(void *), void *arg) {
    pthread_t *thread = malloc(sizeof(pthread_t)); // Consider storing somewhere

    int r = pthread_create(thread, NULL, start_routine, arg);
    if (r != 0) {
        perror("pthread_create");
        return -1;
    }

    return 0;
}

void sockets_to_pollfds(int *sockets, int nb_sockets, struct pollfd *pollfds) {
    for (int i = 0; i < nb_sockets; i++) {
        pollfds[i].fd = sockets[i];
        pollfds[i].events = POLLIN;
    }
}