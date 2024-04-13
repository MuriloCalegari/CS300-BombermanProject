int launch_thread(void *(*start_routine)(void *), void *arg);
void sockets_to_pollfds(int *sockets, struct pollfd *pollfds, int nb_sockets);