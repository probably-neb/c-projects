#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define HALF_SECOND_MILLI 500000
#define USAGE "usage: timeit <seconds>\n"
#define TIK_STR "Tick..."
#define TOK_STR "Tock\n"
#define TIMES_UP "Time's up!\n"

void justkeepswimming(int signum) { return; }

int main(int argc, char *argv[]) {
    struct timeval it_timer;
    struct itimerval timer;
    struct sigaction tik_action;
    char *tail = NULL;
    char *str_flip_flop[] = {TIK_STR, TOK_STR};
    /* number of half seconds left to wait */
    int h_secs = 0;
    sigset_t emptyset;
    if (sigemptyset(&emptyset) == -1) {
        perror("Empty sigset");
        return -1;
    }

    if (argc != 2) {
        fprintf(stderr, "Single arg specifying time required\n%s", USAGE);
        return -1;
    }

    h_secs = 2 * strtoul(argv[1], &tail, 0);
    if (tail == NULL || *tail != '\0') {
        fprintf(stderr, "%s: malformed time.\n%s", argv[1], USAGE);
        return -1;
    }

    /*
     * setup it_timer
     */
    it_timer.tv_sec = 0;
    it_timer.tv_usec = HALF_SECOND_MILLI;

    /*
     * setup timer.
     * use it_timer to make alarm go off every 0.5 seconds repeatedly
     */
    timer.it_interval = it_timer;
    timer.it_value = it_timer;

    if ((setitimer(ITIMER_REAL, &timer, NULL)) == -1) {
        perror("timer");
        return -1;
    }

    /*
     * setup sig handler (justkeepswimming)
     */
    tik_action.sa_handler = justkeepswimming;
    tik_action.sa_mask = emptyset;
    tik_action.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &tik_action, NULL);

    while (h_secs != 0) {
        printf("%s", str_flip_flop[h_secs & 1]);
        fflush(stdout);
        sigsuspend(&emptyset);
        h_secs--;
    }
    printf("%s", TIMES_UP);
    return 0;
}
