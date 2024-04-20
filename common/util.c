#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <poll.h>

pthread_t *launch_thread(void *(*start_routine)(void *), void *arg) {
    pthread_t *thread = malloc(sizeof(pthread_t)); // Consider storing somewhere

    int r = pthread_create(thread, NULL, start_routine, arg);
    if (r != 0) {
        perror("pthread_create");
        return NULL;
    }

    return thread;
}