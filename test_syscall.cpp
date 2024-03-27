#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>
#include "include/sim_api.h"

int main() {
    struct timespec ts;
    printf("%ld", sizeof(timespec));
    for (int i = 0; i < 10; ++i) {
        if (syscall(SYS_clock_gettime, CLOCK_REALTIME, &ts) == -1) {
            perror("syscall");
            return -1;
        }
        printf("Current time: %ld.%09ld seconds\n", ts.tv_sec, ts.tv_nsec);
    }
    
    
    printf("ptr: %p\n", &ts);
    for (int i = 0; i < 10; ++i) {
        printf("Current time: %ld nanoseconds\n", SimGetEmuTime());
    }
    // for (int i = 0; i < 10; ++i) {
    //     timespec * tp = new timespec;
    //     clock_gettime(CLOCK_REALTIME, tp);
    // }
    // Use syscall to invoke SYS_clock_gettime

    return 0;
}
