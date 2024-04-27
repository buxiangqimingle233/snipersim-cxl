#include <stdlib.h>
#include <time.h>
#include <cstdio>
#include "sim_api.h"
#include <iostream>

#define SIZE (1 << 14) // Adjust this value to change the amount of data written


void clflush_opt(void *addr) {
  asm volatile(".byte 0x66; clflush %0" : "+m"(*(volatile char *)(addr)));
}

int main() {
    int* data = (int*) malloc(SIZE * sizeof(int));
    if (data == NULL) {
        printf("Failed to allocate memory\n");
        return 1; // Failed to allocate memory
    }
    // srand(time(NULL));
    
    SimAccessCXLType3();
    for (size_t i = 0; i < SIZE; i++) {
        data[i] = rand(); // Write random data to memory
        // std::cout << reinterpret_cast<unsigned long>(&data[i]) << std::endl;
        SimSyncWrite((unsigned long)&data[i], sizeof(int)); 
        if (i % 64 == 0) {
            SimFlushWQ();
        }
        // __builtin___clear_cache(data + i, data + i + 1);
        // clflush_opt(data + i);
    }
    SimAccessLocal();

    free(data);
    return 0;
}