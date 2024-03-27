#include <stdlib.h>
#include <time.h>
#include <cstdio>

#define SIZE (1 << 20) // Adjust this value to change the amount of data written


void clflush_opt(void *addr) {
  asm volatile(".byte 0x66; clflush %0" : "+m"(*(volatile char *)(addr)));
}

int main() {
    int* data = (int*) malloc(SIZE * sizeof(int));
    if (data == NULL) {
        printf("Failed to allocate memory\n");
        return 1; // Failed to allocate memory
    }

    srand(time(NULL));

    for (size_t i = 0; i < SIZE; i++) {
        data[i] = rand(); // Write random data to memory
        // __builtin___clear_cache(data + i, data + i + 1);
        clflush_opt(data + i);
    }

    free(data);
    return 0;
}