#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "sim_api.h"


inline uint64_t get_real_clock() {
#if defined(__i386__)
    uint64_t ret;
    __asm__ __volatile__("rdtsc" : "=A" (ret));
#elif defined(__x86_64__)
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    uint64_t ret = ( (uint64_t)lo)|( ((uint64_t)hi)<<32 );
	ret = (uint64_t) ((double)ret / 2.2);
#else 
	timespec * tp = new timespec;
    clock_gettime(CLOCK_REALTIME, tp);
    uint64_t ret = tp->tv_sec * 1000000000 + tp->tv_nsec;
#endif
	return ret;
}

inline uint64_t gettime() {
	timespec * tp = new timespec;
    clock_gettime(CLOCK_REALTIME, tp);
    uint64_t ret = tp->tv_sec * 1000000000 + tp->tv_nsec;
	return ret;
}

// #define SYSCALL

int main() {
    volatile int i; // 'volatile' to prevent compiler optimizations
    uint64_t start, end, startt, endd;
    
    startt = SimGetEmuTime();
#ifdef SYSCALL
    start = gettime();
#else
    start = get_real_clock();
#endif

    for (i = 0; i < 1000000; i++) {
        // Empty loop doing nothing but iterating
    }
#ifdef SYSCALL
    end = gettime();
#else 
    end = get_real_clock();
#endif

    endd = SimGetEmuTime();
    printf("OS Time: %lf s, Simulated Time: %lf s\n", (end - start) / (1000000000.0), (endd - startt) / (1000000000.0));
    return 0;
}
