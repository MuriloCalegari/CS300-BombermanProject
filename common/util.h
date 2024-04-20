#ifdef DEBUG
    #define DEBUG_PRINTF(...) printf("DEBUG: " __VA_ARGS__)
#else
    #define DEBUG_PRINTF(...) do {} while (0)
#endif

pthread_t *launch_thread(void *(*start_routine)(void *), void *arg);