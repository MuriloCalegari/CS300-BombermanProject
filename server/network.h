void affiche_connexion(struct sockaddr_in6 adrclient);
int wait_for_next_player(int socket);
int prepare_socket_and_listen(int port);
int read_loop(int fd, void * dst, int n, int flags);
int write_loop(int fd, void * dst, int n, int flags);