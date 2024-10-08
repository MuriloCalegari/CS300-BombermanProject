#include <poll.h>

#ifdef DEBUG
    #define DEBUG_PRINTF(...) printf("DEBUG: " __VA_ARGS__)
#else
    #define DEBUG_PRINTF(...) do {} while (0)
#endif

#ifdef VERBOSE
    #define VERBOSE_PRINTF(...) printf("VERBOSE: " __VA_ARGS__)
#else
    #define VERBOSE_PRINTF(...) do {} while (0)
#endif

#ifdef _WIN64
   //define something for Windows (64-bit)
#elif _WIN32
   //define something for Windows (32-bit)
#elif __APPLE__
    #include "TargetConditionals.h"
    #if TARGET_OS_IPHONE && TARGET_OS_SIMULATOR
        // define something for simulator
        // (although, checking for TARGET_OS_IPHONE should not be required).
    #elif TARGET_OS_IPHONE && TARGET_OS_MACCATALYST
        // define something for Mac's Catalyst
    #elif TARGET_OS_IPHONE
        // define something for iphone  
        #define TARGET_OS_OSX 1
    #else
        // define something for OSX
    #endif
#elif __linux
    // linux
#elif __unix // all unices not caught above
    // Unix
#elif __posix
    // POSIX
#endif

pthread_t *launch_thread_with_mode(void *(*start_routine)(void *), void *arg, int mode);
pthread_t *launch_thread(void *(*start_routine)(void *), void *arg);

int read_loop(int fd, void * dst, int n, int flags);
int write_loop(int fd, void * src, int n, int flags);
int write_loop_udp(int fd, void * src, int n, struct sockaddr_in6 * dest_addr, socklen_t dest_addr_len);
int determine_if_index();

int poll_loop(struct pollfd *, nfds_t, int);
int accept_loop(int, struct sockaddr *, socklen_t *);

typedef enum LOG_LEVEL {
    LOG_VERBOSE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} LOG_LEVEL;

void print_log(LOG_LEVEL level, const char *format, ...);

void connect_stderr_to_debug_file(char *program_name);
void print_log_prefixed(const LOG_LEVEL level, int should_print_prefix, const char *message, ...);