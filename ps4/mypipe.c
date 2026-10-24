#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

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
                printf("%d reads of %d bytes\n", histogram[i]);
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
        // TODO phase 3 später

        close(pipe_fd[1]);
        exit(EXIT_SUCCESS);
    }  

    close(pipe_fd[0]);
    close(pipe_fd[1]);

    printf("[Parent] started. Writer (PID: %d) will be disturbed...\n", writer_pid);

    // TODO:

    waitpid(reader_pid, NULL, 0);
    waitpid(writer_pid, NULL, 0);
   
    printf("[Parent] Both children are finished. Program terminated.\n");

    return 0;
}