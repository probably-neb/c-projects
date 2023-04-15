#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    pid_t pid;
    int status = EXIT_SUCCESS;
    /* greet the world */
    printf("Hello world!\n");
    pid = fork();
    if (pid == 0) {
        /* This is the child process.  Execute the command. */
        printf("This is the child! pid %d\n", getpid());
        exit(EXIT_SUCCESS);
    } else if (pid < 0) {
        /* The fork failed.  Report failure.  */
        status = EXIT_FAILURE;
    } else {
        printf("This is the parent! pid %d\n", getpid());
        /* Wait for the child to complete. */
        if (waitpid(pid, &status, 0) != pid) {
            status = EXIT_FAILURE;
        }
    }
    printf("This is the parent... Signing off. pid %d\n", getpid());

    return status;
}
