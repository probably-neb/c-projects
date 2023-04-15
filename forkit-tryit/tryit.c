#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define USAGE "usage: tryit command"
#define SUCCESS "succeeded"
#define FAILURE "exited with an error value"

int main(int argc, char *argv[]) {
    pid_t pid;
    int status;
    char *statusstr = FAILURE;

    if (argc != 2) {
        printf("%s\n", USAGE);
        exit(EXIT_SUCCESS);
    }
    pid = fork();
    if (pid == 0) {
        /* This is the child process.  Execute the command. */
        if (execl(argv[1], "", NULL) == -1) {
            perror(argv[1]);
            /* failure */
            exit(1);
        }
        /* success */
        exit(0);
    } else if (pid < 0) {
        /* The fork failed.  Report failure. */
        perror(argv[0]);
        exit(EXIT_FAILURE);
    } else {
        /* Wait for the child */
        if (waitpid(pid, &status, 0) != pid) {
            perror(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    if (status == 0) {
        statusstr = SUCCESS;
    }
    printf("Process %d %s.\n", pid, statusstr);
    return status;
}
