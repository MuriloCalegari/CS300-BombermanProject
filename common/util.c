#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <stdarg.h>
#include <sys/errno.h>
#include "util.h"

pthread_t *launch_thread(void *(*start_routine)(void *), void *arg) {
    return launch_thread_with_mode(start_routine, arg, PTHREAD_CREATE_JOINABLE);
}

/**
 * Launches a new thread.
 *
 * @param start_routine The function to be executed by the thread.
 * @param arg The argument to be passed to the start_routine function.
 * @return A dynamically allocated pointer to the pthread_t identifier for the new thread.
 ** Must be freed by the caller.
 */
pthread_t *launch_thread_with_mode(void *(*start_routine)(void *), void *arg, int mode) {
    pthread_t *thread = malloc(sizeof(pthread_t));

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, mode);

    int r = pthread_create(thread, &attr, start_routine, arg);
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
        int just_sent = recv(fd, dst + received, n - received, flags);

        if(just_sent < 0) {
            if(errno == EINTR) {
                continue;
            }
            perror("recv");
            return -1;
        }

        received += just_sent;
    }

    return received;
}

int write_loop(int fd, void * src, int n, int flags) {
    int sent = 0;

    while(sent != n) {
        int just_sent = send(fd, src + sent, n - sent, flags);

        if (just_sent == -1) {
            if(errno == EINTR) {
                continue;
            }

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
            if(errno == EINTR) {
                continue;
            }

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

void vprint_log_prefixed(const LOG_LEVEL level, int should_print_prefix, const char *message, va_list args) {
    switch(level) {
        case LOG_VERBOSE:
#ifdef VERBOSE
            if(should_print_prefix) {
                fprintf(stderr, "VERBOSE: ");
            }
            vfprintf(stderr, message, args);
            fflush(stderr);
#endif
            break;
        case LOG_DEBUG:
#if defined(VERBOSE) || defined(DEBUG)
            if(should_print_prefix) {
                fprintf(stderr, "DEBUG: ");
            }
            vfprintf(stderr, message, args);
            fflush(stderr);
#endif
            break;
        case LOG_INFO:
#if defined(VERBOSE) || defined(DEBUG) || defined(INFO)
            if(should_print_prefix) {
                fprintf(stderr, "INFO: ");
            }
            vfprintf(stderr, message, args);
            fflush(stderr);
#endif
            break;
        case LOG_WARNING:
#if defined(VERBOSE) || defined(DEBUG) || defined(INFO) || defined(WARNING)
            if(should_print_prefix) {
                fprintf(stderr, "WARNING: ");
            }
            vfprintf(stderr, message, args);
            fflush(stderr);
#endif
            break;
        case LOG_ERROR:
            if (should_print_prefix) {
                fprintf(stderr, "ERROR: ");
                vfprintf(stderr, message, args);
                fflush(stderr);
            }
            break;
    }
}

void print_log_prefixed(const LOG_LEVEL level, int should_print_prefix, const char *message, ...) {
    va_list args;
    va_start(args, message);

    vprint_log_prefixed(level, should_print_prefix, message, args);

    va_end(args);
}

void print_log(const LOG_LEVEL level, const char *message, ...) {
    va_list args;
    va_start(args, message);

    vprint_log_prefixed(level, 1, message, args);

    va_end(args);
}

void connect_stderr_to_debug_file(char *program_name) {
    char *suffix = "_log.txt";
    char *filename = malloc(strlen(program_name) + strlen(suffix) + 1);

    sprintf(filename, "%s%s", program_name, suffix);

    freopen(filename, "w", stderr);
}