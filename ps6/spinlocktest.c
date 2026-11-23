#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "spinlock.h"


#define NUM_PROCS 16
#define NUM_ITER 1000000

typedef struct {
    volatile char lock;
    long long counter;
} shared_data_t;

int main() {
    // shared mem for parent and child processes
    shared_data_t *shared = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }

    shared->lock = 0;
    shared->counter = 0;

    for (int i = 0; i < NUM_PROCS; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(1);
        } else if (pid == 0) {
            for (long long j = 0; j < NUM_ITER; j++) {
// macro is given along when compiling
#ifdef USE_LOCK
                spin_lock(&shared->lock);
#endif
                shared->counter++;
#ifdef USE_LOCK
                spin_unlock(&shared->lock);
#endif
            }
            exit(0);
        }
    }
    for (int i = 0; i < NUM_PROCS; i++) {
        wait(NULL);
    }
    long long expected = (long long) NUM_PROCS * NUM_ITER;
    printf("Expected value: %lld\n", expected);
    printf("Actual value: %lld\n", shared->counter);
    if(expected == shared->counter) {
        printf("SUCCESS: Synchronization works!\n");
    }else {
        printf("FAILURE: Race condition occurred. Difference: %lld\n", expected-shared->counter);
    }

    munmap(shared, sizeof(shared_data_t));
    return 0;
}   