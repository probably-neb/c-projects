/*
 * Mush: A Minimally Usefull Shell.
 * Mush has various builtins:
 * cd: change directory
 * pid: print the process id of the shell
 * exit: exit the shell
 * history [-n num]: print the history of commands. if
 *                   the -n flag is given only prints a maximum
 *                   of the integer value num commands.
 *                   If two instances of the shell are running the
 *                   history is shared.
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <mush.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>

/* many programmers before me have probably wished they could write a macro to
 * do this */
#define fit_in(society) ((society)*2)
#define out_fit(n) ((n)*2 + 1)
#define max(a, b) ((a) > (b) ? (a) : (b))
#define PROMPT "8-P "
/* builtins */
#define BI_CD "cd"
#define BI_EXMUSH "exit"
#define BI_HIST "history"
#define BI_PID "pid"
#define BI_EXEC "exec"
/* signal that mush should exit after the completion command */
#define EXMUSH 256
/* according to the interweb this is how it is traditionally calculated */
#define SIG_EXIT_NO(sig) (128 + (sig))
#define HISTBUFSIZE getpagesize()
#define MAXHISTOFS (HISTBUFSIZE - 1)
#define HISTFILENAME "mush_history"
#define EXMUSHMSG "exiting...\n"

struct sigaction sigintdfl;

/* history file and offset to current position in said file.
 * History file operates as a ring buffer, overwriting previous contents when
 * the list becomes too large to avoid making a huge history file */
char *histfile = NULL;
int histofs = 0;
mode_t histmode = S_IRUSR | S_IWUSR;

/* most if not all of the actions done in response to signals are location
 * specific and not thread safe (read/write etc). Therefore sigint_recieved
 * is set so the actual handling can be done safely and where and when it
 * should be done */
volatile sig_atomic_t sigint_recieved = 0;

void sigint_catcher(int sig) { sigint_recieved = 1; }

/* helper function */
void freeifnn(void *buf) {
    if (buf != NULL) {
        free(buf);
    }
}

/* searches the given buffer backwards from the offset for char c,
 * returning the offset of the next instance of c or -1 if it reaches the
 * beginning of the buffer */
/* used almost exclusively in the storing of history */
int searchback(char *buf, int curofs, char c) {
    int ofs;
    for (ofs = curofs; ofs > -1; ofs--) {
        if (buf[ofs] == c) {
            break;
        }
    }
    return ofs;
}

void sync_history() {
    int t;
    if (histfile[histofs] != '\0') {
        for (t = histofs; t < HISTBUFSIZE; t++) {
            if (histfile[t] == '\0' && t != MAXHISTOFS) {
                break;
            } else if (t == MAXHISTOFS) {
                /* start looping again from beginning */
                t = 0;
            }
        }
        histofs = t;
    }
}

/* enum values to specify in or out using the actual flags the files will be
 * opened with. Helps the code be more verbose and clear */
enum in_or_out { in = O_RDONLY, out = O_WRONLY | O_CREAT | O_TRUNC };

/* if path is not null it opens it with the relevant flags based on whether it
 * will be used as an input or output redirect. Otherwise returns stdout or
 * stdin using same in/out redirect logic */
int redirect_or_default(char *path, enum in_or_out io) {
    int fd = -1, flags = io;
    switch (io) {
    case out: /* open in write mode (with extra flags) */
        fd = STDOUT_FILENO;
        break;
    case in: /* open in read mode */
        fd = STDIN_FILENO;
        break;
    default:
        fprintf(stderr, "Attemped to redirect neither here nor there\n");
    }
    if (path != NULL) {
        fd = open(path, flags);
    }
    return fd;
}

int fit_pipeline(pipeline p, int fittings[]) {
    int err = 0, i, plen = p->length - 1, tmp, *tmpipe;
    fittings[fit_in(0)] = redirect_or_default(p->stage[0].inname, in);
    if (fittings[fit_in(0)] < 0) {
        perror("failed to open input file");
        err--;
    }
    /* create pipes if needed */
    for (i = 0; i < plen && !err; i++) {
        tmpipe = fittings + out_fit(i);
        if (pipe(tmpipe) == -1) {
            perror("pipe");
            err--;
        }
        /* swap pipe ends just so the fittings array is arranged logically
         * process->[write end | read end] -> next process */
        tmp = tmpipe[1];
        tmpipe[1] = tmpipe[0];
        tmpipe[0] = tmp;

        if (fcntl(tmpipe[0], F_SETFL, O_CLOEXEC) ||
            fcntl(tmpipe[1], F_SETFL, O_CLOEXEC)) {
            perror("Set file flags");
        }
    }
    fittings[out_fit(plen)] = redirect_or_default(p->stage[plen].outname, out);
    if (fittings[out_fit(plen)] < 0) {
        perror("failed to open output file");
        err--;
    }
    return err;
}

/*
 * preps file descriptors pre exec.
 * Dups relevant stdin/out based on i and closes everything else
 */
int prep_fds(int i, int *fittings, int fittingslen) {
    int j, err = 0, in_fd = fit_in(i), out_fd = out_fit(i);

    if (fittings[in_fd] != STDIN_FILENO &&
        dup2(fittings[in_fd], STDIN_FILENO) < 0) {
        err--;
    }
    if (fittings[out_fd] != STDOUT_FILENO &&
        dup2(fittings[out_fd], STDOUT_FILENO) < 0) {
        err--;
    }
    if (err) {
        perror("dup");
    }

    /* close all fds in fittings */
    for (j = 0; j < fittingslen && !err; j++) {
        if (fittings[j] != STDIN_FILENO && fittings[j] != STDOUT_FILENO) {
            err += close(fittings[j]);
            fittings[j] = -1;
            if (err) {
                perror("close");
            }
        }
    }
    return err;
}

/* checks the first stage of pipeline for builtin commands.
 * Runs the corresponding builtin command if it recognizes one,
 * sending any output to the file outfd.
 * returns -1 on error, 0  no builtins were found and 1 if they were.
 * the special case is the exit builtin in which case this returns EXMUSH.
 * All builtins with the exception of exec are handled here. Exec is handled in
 * main however so it has access to argv */
int perform_builtins_if_present(pipeline p, int outfd) {
    char **cmd1 = p->stage[0].argv;
    int status = 0, i = 0, n = 0, tmpofs = 0;
    FILE *outst = NULL;
    outst = fdopen(outfd, "a");
    /* this macro serves to make this if else block more understandable */
#define cmdisbuiltin(BI) (strcmp(cmd1[0], (BI)) == 0)
    /* CD builtin */
    if ((status = cmdisbuiltin(BI_CD))) {
        if (chdir(cmd1[1]) < 0) {
            perror("change dir");
        }
    }
    /* exit builtin */
    else if (cmdisbuiltin(BI_EXMUSH)) {
        status = EXMUSH;
        fprintf(stderr, EXMUSHMSG);
    } else if ((status = cmdisbuiltin(BI_PID))) {
        fprintf(outst, "%d\n", getpid());
        fflush(outst);
    } else if ((status = cmdisbuiltin(BI_HIST))) {
        if (histfile != NULL) {
            sync_history();
            if (p->stage[0].argc == 3 && strcmp(cmd1[1], "-n") == 0) {
                n = atoi(cmd1[2]);
                if (n < 0) {
                    fprintf(outst, "\"The best way to predict your future is "
                                   "to create it.\"\n"
                                   "― Abraham Lincoln\n");
                    fflush(outst);
                    return status;
                }
                /* go backwards until we reach beginning or desired number of
                 * commands
                 */
                tmpofs = histofs - 2;
                for (i = 0; i < n && tmpofs > 0; i++) {
                    tmpofs = searchback(histfile, tmpofs - 1, '\n');
                    if (tmpofs == 0) {
                        break;
                    }
                }
                tmpofs++;
            } else {
                n = MAXHISTOFS;
                tmpofs = 0;
            }
            fprintf(outst, "%s", histfile + tmpofs);
            if (i < n) {
                /* loop to back and repeat this time until disired number or
                 * where we started in the loop above */
                tmpofs = MAXHISTOFS;
                for (i = 0; i <= n && tmpofs > histofs; i++) {
                    tmpofs = searchback(histfile, tmpofs, '\n');
                }
                tmpofs++;
                fprintf(outst, "%s", histfile + tmpofs);
            }
        }
    }

    return status;
}

/*
 * TODO:
 * [x] fork/exec pattern
 * [x] waiting
 * [x] builtins
 *    [x] cd
 *    [x] exit
 *    [x] debug arg
 *    [ ] exec
 *    [ ] history
 * [x] prompts
 * [x] sigints
 *    - if reading input abandon line
 *    - if execing wait for all children to exit
 * [ ] batch mode (change input output file for mush)
 */
/* carries out the command line pointed to by biddingptr.
 * Assumes biddingptr points to a string that was malloced by readLongString.
 * Free's bidding after completion and sets it to NULL.
 * returns the exit status of the first program with a non-zero exit status,
 * or zero if all exit correctly. and EXMUSH if mush should exit after this
 * command
 */
int carryout(char **biddingptr, int debug) {
    char *bidding = NULL;
    pipeline p = NULL;
    struct clstage *stage = NULL;
    int *fittings = NULL;
    pid_t *pids = NULL;
    pid_t pid;
    int i, err = 0, status, first_prog = 0, tmpofs;

    /* if we have a sigint here we skip setting up children */
    if (!(err = sigint_recieved) && biddingptr != NULL && *biddingptr != NULL) {
        bidding = *biddingptr;
        p = crack_pipeline(bidding);
        if (p == NULL) {
            fprintf(stderr, "Failed to parse command line. Try again\n");
            goto cleanup;
        }
        fittings = (int *)malloc(2 * p->length * sizeof(int));
        pids = (pid_t *)malloc(p->length * sizeof(pid_t));
        if (fittings == NULL || pids == NULL) {
            perror("malloc");
            goto cleanup;
        }
        err += fit_pipeline(p, fittings);
    } else {
        goto cleanup;
    }

    if (!err && debug) {
        print_pipeline(stderr, p);
        for (i = 0; i < 2 * p->length; i++) {
            printf("%d, ", fittings[i]);
        }
        fflush(stdout);
    }

    /* will set first_prog to 1 if a builtin was performed.
     * future loops will use first_prog as the index of the first
     * program they should fork and exec
     * (therefore avoiding execing the builtin which may or may not actually be
     * a program)*/
    if (!err && (err = perform_builtins_if_present(p, out_fit(0)))) {
        if (err == 1) {
            if (debug) {
                fprintf(stderr,
                        "Ran Builtin \"%s\"\nPipeline now begins at stage 1\n",
                        p->stage[0].argv[0]);
            }
            first_prog = 1;
            err = 0;
        } else if (err == EXMUSH) {
            if (debug) {
                fprintf(stderr, "Recieved exit command\n");
            }
            goto cleanup;
        } else {
            perror("failed to perform builtin");
            err--;
        }
    }

    /* begin exec/fork */
    for (i = first_prog; i < p->length && !err; i++) {
        pid = fork();
        if (pid < 0) {
            perror("fork");
            err--;
        } else if (pid == 0) {
            /* child */
            stage = &p->stage[i];
            if (!err) {
                if (!(err += prep_fds(i, fittings, 2 * p->length))) {
                    if (debug) {
                        fprintf(stderr,
                                "Executing \"%s\"\n"
                                ">>>>>>\n",
                                stage->argv[0]);
                    }
                    /* reset sighandler */
                    if (sigaction(SIGINT, &sigintdfl, NULL) == -1) {
                        perror("Failed to restore sigaction for children");
                    }
                    execvp(stage->argv[0], stage->argv);
                }
                /* if we execute this code exec failed */
                perror(stage->argv[0]);
                err = EXMUSH;
                goto cleanup;
            }
        } else if (pid > 0) {
            /* parent */
            pids[i] = pid;
        }
    }
    /* we are the parent and have executed all children (yikes) */

    /* now that children have access to their relevant fd's, close all parent
     * copies of fds */
    /* don't use first_prog here because we need to close all fds, even ones
     * created for processes that ended up being builtins */
    for (i = 0; i < 2 * p->length && !err; i++) {
        if (fittings[i] != STDIN_FILENO && fittings[i] != STDOUT_FILENO) {
            if (err += close(fittings[i])) {
                perror("closing child input");
            }
        }
    }

    if (!err) {
        for (i = first_prog; i < p->length; i++) {
            pid = waitpid(pids[i], &status, 0);
            if (!WIFEXITED(status)) {
                i++;
                continue;
            }
            if (pid < -1) {
                perror("wait");
            }
            if (!err) {
                err = WEXITSTATUS(status);
            }
            /* TODO: what to do if child exited with non - zero status */
        }
    }

    /* Store bidding to history file */
    if (histfile != NULL && histofs != -1 && bidding != NULL &&
        (tmpofs = strlen(bidding) + 1) < MAXHISTOFS) {
        sync_history();
        if (histofs + tmpofs >= MAXHISTOFS || histofs == 0) {
            histofs = 0;
        }
        strcpy(histfile + histofs, bidding);
        histofs += tmpofs;
        if (histfile[histofs - 2] != '\n') {
            histfile[histofs - 1] = '\n';
        } else {
            histofs--;
        }

        status = 0;
        tmpofs = histofs;
        do {
            status = (histfile[tmpofs] != '\n' && histfile[tmpofs] != '\0');
            histfile[tmpofs] = '\0';
        } while (status && tmpofs < MAXHISTOFS);
    }
cleanup:
    if (debug) {
        fprintf(stderr,
                "<<<<<<\n"
                "\nbeginning cleanup. exit code: %d\n",
                err);
    }
    fflush(stdout);
    freeifnn(fittings);
    freeifnn(pids);
    freeifnn(bidding);
    if (biddingptr != NULL) {
        *biddingptr = NULL;
    }

    if (p != NULL) {
        free_pipeline(p);
    }
    yylex_destroy();
    return err;
}

int main(int argc, char *argv[]) {
    /* char* bidding = NULL; */
    char *bidding = NULL;
    int err = 0;
    int debug = 0;

    FILE *mushin = stdin;
    struct sigaction sigintaction;
    char expath[PATH_MAX];
    ssize_t expathlen = 0;
    int histfd;
    if (argc == 2) {
        if ((!strcmp(argv[1], "-d") || !strcmp(argv[1], "--debug"))) {
            debug = 1;
        }
        /* else try opening the arg as a batch file */
        else {
            mushin = fopen(argv[1], "r");
            if (mushin == NULL) {
                fprintf(stderr, "Unable to open batch file: %s\n", argv[1]);
                if (errno) {
                    perror(argv[1]);
                }
                exit(EXIT_FAILURE);
            }
        }
    }

    /* setup history */
    /* only linux gets to have history because this is only a minimally usefull
     * shell */
    /* if not linux or for some other reason /proc/self/exe doesn't work
     * no history for you either. I'd rather not make history
     * files somewhere besides where the executable is */
    if ((expathlen = readlink("/proc/self/exe", expath, PATH_MAX)) > 0) {
        if (debug) {
            fprintf(stderr, "found executable at: %s\n", expath);
        }
        /* get basename and append history file name */
        expathlen = searchback(expath, expathlen - 1, '/');
        if (strlen(HISTFILENAME) + expathlen > PATH_MAX) {
            errno = ENAMETOOLONG;
            histfd = -1;
        } else {
            strcpy(expath + expathlen + 1, HISTFILENAME);
            if (debug) {
                fprintf(stderr, "Attempting to open history file at: %s\n",
                        expath);
            }
            histfd = open(expath, O_RDWR, histmode);
        }
        if (debug && errno) {
            fprintf(stderr,
                    "Failed to open existing file. errno: %d. histmode: %o\n",
                    errno, histmode);
        }
        if (histfd >= 0 || errno == ENOENT) {
            if ((histfd = open(expath, O_RDWR | O_CREAT, histmode)) == -1) {
                perror("Failed to create history file");
                err--;
            }
            if (!err) {
                histfile = (char *)calloc(HISTBUFSIZE, sizeof(char));
                if (histfile == NULL ||
                    (write(histfd, histfile, HISTBUFSIZE) < 0)) {
                    perror("Failed to fill history file");
                }
                freeifnn(histfile);
                histfile = NULL;
            }
        } else {
            perror("Failed to open existing history file");
            err--;
        }
        if (!err && (histofs = lseek(histfd, MAXHISTOFS, SEEK_SET)) < 0) {
            if (debug) {
                fprintf(stderr, "Failed to seek to end of file\n");
            }
            err--;
        }
        if (!err) {
            histfile = (char *)mmap(NULL, HISTBUFSIZE, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, histfd, 0);
            if (histfile == MAP_FAILED) {
                if (debug) {
                    fprintf(stderr, "Failed to mmap file\n");
                }
                err--;
            }
        }
        if (err) {
            fprintf(stderr, "Failed to open history file. Abstaining from "
                            "history for now\n");
            histofs = -1;
        } else {
            if (debug) {
                fprintf(stderr, "Successfully opened history file\n");
            }
            if (close(histfd) == -1) {
                perror("close old history file descriptor");
            }
            histofs = searchback(histfile, MAXHISTOFS, '\n');
            /* if this is a new history file we start at the beginning */
            if (histofs < 0) {
                histofs = 0;
            }
        }
    }

    memset(&sigintaction, 0, sizeof(struct sigaction));
    memset(&sigintdfl, 0, sizeof(struct sigaction));

    sigintaction.sa_handler = sigint_catcher;
    sigintdfl.sa_handler = SIG_DFL;

    /* could have just as well used SIG_DFL */
    if (sigaction(SIGINT, &sigintaction, &sigintdfl) == -1) {
        perror("sigaction failed");
        fprintf(stderr, "Warning: There is a risk of memory leak if a SIGINT "
                        "recieved during "
                        "execution of shell commands\n");
    }

    /* run commands until told to stop */
    while (1) {
        if (sigint_recieved) {
            putc('\n', stdout);
            sigint_recieved = 0;
        }
        if (isatty(STDIN_FILENO)) {
            fprintf(stdout, PROMPT);
            fflush(stdout);
        }
        bidding = readLongString(mushin);
        if (!(bidding == NULL || strlen(bidding) == 0)) {
            err = carryout(&bidding, debug);
        }
        if (err == EXMUSH || feof(mushin)) {
            break;
        }
    }
    if (histfile != NULL) {
        munmap(histfile, HISTBUFSIZE);
    }
    exit(err);
}
