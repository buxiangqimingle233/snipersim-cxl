#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "sim_api.h"

#define NUM_THREADS 4         // Number of threads
#define LIST_SIZE (1 << 28)   // Size of the shared list
#define MAX_LOOP  (1 << 10)

// Define a structure for each element in the list
typedef struct {
    char data[16];              // 16-byte data
    pthread_rwlock_t lock;      // Read-write lock for this element
} ListElement;

ListElement* sharedList; // Shared list
pthread_barrier_t startBarrier_;    // Barrier to synchronize thread start

void *threadWork(void *threadid) {
    long tid = (long)threadid;
    pthread_barrier_wait(&startBarrier_);
    for (int i = 0; i < MAX_LOOP; i++) {
        int listIndex = rand() % LIST_SIZE; // Choose a random index

        // Lock the selected list element for writing
        pthread_rwlock_wrlock(&sharedList[listIndex].lock);

        SimAccessCXLType3();
        char c = sharedList[listIndex].data[0]; // Read the first character of the data
        snprintf(sharedList[listIndex].data, 16, "T%ldI%d", tid, listIndex);
        // Critical section: modify the selected element
        // printf("Thread %ld writing to index %d.\n", tid, listIndex);
        // Generate some data to write - here we use the thread ID and index for demonstration
        // printf("0 %lu\n", (unsigned long)sharedList[listIndex].data);
        SimSyncWrite((unsigned long)sharedList[listIndex].data, sizeof(sharedList[listIndex].data));
        SimAccessReset();

        // Unlock the selected list element after writing
        pthread_rwlock_unlock(&sharedList[listIndex].lock);
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    long t;
    srand(time(NULL)); // Seed the random number generator
    sharedList = (ListElement*)malloc(LIST_SIZE * sizeof(ListElement)); // Allocate memory for the shared list
    // Initialize the shared list and its locks
    for(int i = 0; i < LIST_SIZE; i++) {
        memset(sharedList[i].data, 0, 16); // Clear data
        pthread_rwlock_init(&sharedList[i].lock, NULL); // Initialize the lock for each element
    }

    pthread_barrier_init(&startBarrier_, NULL, NUM_THREADS + 1);
    // Create the threads
    for(t = 0; t < NUM_THREADS; t++) {
        printf("In main: creating thread %ld\n", t);
        int rc = pthread_create(&threads[t], NULL, threadWork, (void *)t);
        if (rc) {
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }

    SimRoiStart();
    pthread_barrier_wait(&startBarrier_);

    // Wait for the threads to finish
    for(t = 0; t < NUM_THREADS; t++) {
        pthread_join(threads[t], NULL);
    }

    SimRoiEnd();
    // Print the list content
    for(int i = 0; i < LIST_SIZE; i++) {
        // printf("List index %d contains: %s\n", i, sharedList[i].data);
    }

    // Cleanup: Destroy the read-write locks
    for(int i = 0; i < LIST_SIZE; i++) {
        pthread_rwlock_destroy(&sharedList[i].lock);
    }
    printf("Main: program completed. Exiting.\n");

    pthread_exit(NULL);
    free(sharedList);
    return 0;
}
