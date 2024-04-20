#ifdef DEBUG
    #define DEBUG_PRINTF(...) printf("DEBUG: " __VA_ARGS__)
#else
    #define DEBUG_PRINTF(...) do {} while (0)
#endif

pthread_t *launch_thread(void *(*start_routine)(void *), void *arg);

int read_loop(int fd, void * dst, int n, int flags);
int write_loop(int fd, void * src, int n, int flags);
int write_loop_udp(int fd, void * src, int n, struct sockaddr_in6 * dest_addr, socklen_t dest_addr_len);