int read_loop(int fd, void * dst, int n, int flags);
int write_loop(int fd, void * src, int n, int flags);
void convertEndian(uint8_t tab[16]);