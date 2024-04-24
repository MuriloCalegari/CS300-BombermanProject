#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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


void convertEndian(uint8_t tab[16]) {
    uint8_t temp;
    int i;

    // Swap bytes in array to get little endian
    for (i = 0; i < 8; ++i) {
        temp = tab[i];
        tab[i] = tab[15 - i];
        tab[15 - i] = temp;
    }
}