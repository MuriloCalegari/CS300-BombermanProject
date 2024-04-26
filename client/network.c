#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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