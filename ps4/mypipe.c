#define _POSIX_C_SOURCE 200809L


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

// ensure variable is always loaded from RAM
volatile sig_atomic_t sigusr1_count = 0;

void handle_sigusr1(int sig) {
    sigusr1_count++;
}

struct PartialWrite {
    int size;
    int count;
};

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <write-size> <iterations> [restart]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // convert arguments into ascii 
    int write_size = atoi(argv[1]);
    int iterations = atoi(argv[2]);
    int restart = 0;

    if (argc == 4 && strcmp(argv[3], "restart") == 0) {
        restart = 1;
    }

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("Error occured when creating a pipe.");
        exit(EXIT_FAILURE);
    }

    pid_t reader_pid = fork();
    if (reader_pid == -1) {
        perror("Error while forking the reader.");
        exit(EXIT_FAILURE);
    } 

    // reader child process
    if (reader_pid == 0) {
        close(pipe_fd[1]);
        printf("[Reader] Started \n");
        char buffer[555];
        // safe distribution of reads in a map
        int histogram[556] = {0};
        long long total_bytes_read = 0;
        ssize_t bytes_read;

        while ((bytes_read = read(pipe_fd[0], buffer, 555)) > 0) {
            histogram[bytes_read]++;
            total_bytes_read += bytes_read;
        }
        
        if(bytes_read == 0) {
            printf("Read returned EOF\n");
        } else {
            perror("Reader: Unexpected error while reading.");
        }

        for (int i = 0; i <= 555; i++) {
            if (histogram[i] > 0) {
                printf("%d reads of %d bytes\n", histogram[i], i);
            }
        }

        printf("%lld total bytes read\n", total_bytes_read);

        close(pipe_fd[0]);
        exit(EXIT_SUCCESS);
    }

    pid_t writer_pid = fork();
    if(writer_pid == -1) {
        perror("Error while forking the writer");
        exit(EXIT_FAILURE);
    }

    // writer child process
    if(writer_pid == 0) {
        close(pipe_fd[0]);
        printf("[Writer] Started. (size: %d, iterations: %d, restart: %d)\n",
            write_size, iterations, restart);
        struct sigaction sa;
        // clean possibly polluted data
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = handle_sigusr1;
        if (restart) {
            sa.sa_flags = SA_RESTART;
        }
        // adapt new rules for SIGUSR1
        sigaction(SIGUSR1, &sa, NULL);
        int eintr_count = 0;
        int full_writes = 0;

        long long total_written = 0;

        // maps the partial write bytes to their frequency
        struct PartialWrite partials[1000];
        int num_partials = 0;

        char *buffer = malloc(write_size);
        // fill buffer with arbritrary bytes to write
        memset(buffer, 'A', write_size);
        for (int i = 0; i < iterations; i++) {
            ssize_t res = write(pipe_fd[1], buffer, write_size);
            if (res == write_size) {
                full_writes++;
                total_written += res;
            } else if (res == -1) {
                if(errno == EINTR) {
                    eintr_count++;
                } else {
                    perror("[Writer] Unexpected error while writing.");
                    exit(EXIT_FAILURE);
                }
            } else if(res > 0){
                total_written += res;
                int found = 0;
                for(int p = 0; p < num_partials; p++) {
                    if(partials[p].size == res) {
                        partials[p].count++;
                        found = 1;
                        break;
                    }
                }
                if(!found && num_partials < 1000) {
                    partials[num_partials].size = res;
                    partials[num_partials].count = 1;
                    num_partials++;
                }
            }
        }
        // prevent signals from interruping print commands
        signal(SIGUSR1, SIG_IGN);
        printf("%d EINTRs\n", eintr_count);
        printf("%d SIGUSR1 handled\n", sigusr1_count);

        for (int p = 0; p < num_partials; p++) {
            printf("%d writes of %d bytes\n", partials[p].count, partials[p].size);
        }
        printf("%d writes of %d bytes\n", full_writes, write_size);
        printf("%lld total bytes written\n", total_written);

        free(buffer);
        close(pipe_fd[1]);
        exit(EXIT_SUCCESS);
    }  

    close(pipe_fd[0]);
    close(pipe_fd[1]);

    int signals_sent = 0;
    struct timespec delay;
    delay.tv_sec = 0;
    delay.tv_nsec = 50000;

    printf("[Parent] started. Writer (PID: %d) will be disturbed...\n", writer_pid);
    // give write process a head start to avoid race condition
    usleep(10000);
    while (1) {
        // fix: check if writer has become a zombie process
        if(waitpid(writer_pid, NULL, WNOHANG) == writer_pid) {
            printf("Writer is done\n");
            break;
        }
        if (kill(writer_pid, SIGUSR1) == -1) {
            if (errno == ESRCH) {
                printf("kill returned ESRCH, writer is done\n");
                break;
            }
        }
        signals_sent++;
        nanosleep(&delay, NULL);
    }

    printf("%d SIGUSR1 sent\n", signals_sent);

    waitpid(reader_pid, NULL, 0);
    waitpid(writer_pid, NULL, 0);
    printf("[Parent] Both children are finished. Program terminated.\n");

    return 0;
}