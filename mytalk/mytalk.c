#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "talk.h"

#define CONNECTION_TIMEOUT -1 /* infinite */
#define PACKET_SIZE 100
#define CONFIRMATION_BUF_SIZE 4
#define REJEC_STR " declined connection.\n"
#define CONFIRMATION_OK "ok"
#define PEER_DISCONNECT_MSG "Connection closed.  ^C to terminate.\n"
#define PEER_DISCONNECT_MSG_LEN (strlen(PEER_DISCONNECT_MSG))
#define USAGESTR "Usage \"[ hostname ] port\".\n"
#define AUTOACCEPT 'a'
#define VERBOSE 'v'
#define NOWINDOW 'N'

typedef int sock_t;      /* for verbosity */
typedef uint16_t port_t; /* for verbosity */
typedef int bool_t;      /* for verbosity */

/* my or peer */
static struct opts {
    bool_t nowindow;
    int verbose;
    bool_t autoaccept;
} opts;

enum poll_return_values { Err = -1, Timedout = 0, Success = 1 };

/* global var for sock. Allows setting it to -1 to know when to shut it down
 * (i.e. when peer has disconnected) making cleanup function cleaner.
 * also don't have to pass it to every function in program because they all use
 * it.
 * while lazy, I think it makes the program better and more robust not less. */
sock_t sock = -1;

void sigint_fallthrough(int sig) { return; }

/* closes sock */
int cleanup() {
    int err = 0;
    if (sock > -1) {
        if ((shutdown(sock, 2)) == -1) {
            perror("shutdown");
            err--;
        }
        sock = -1;
    }
    if ((write_to_output(PEER_DISCONNECT_MSG, PEER_DISCONNECT_MSG_LEN)) ==
        ERR) {
        perror("write");
        err--;
    }
    /* try to ignore sigints just so process doesn't return with error.
     * if it doesn't work the process will have non zero exit status... not a
     * big deal */
    if ((signal(SIGINT, sigint_fallthrough)) == SIG_ERR) {
        /* ignore */
    }
    pause();
    if (!opts.nowindow) {
        stop_windowing();
    }
    return err;
}

int talk() {
    char packet[PACKET_SIZE];
    struct pollfd pfds[2];
    /* pointers to correct positions in pfds to avoid array indexing the
     * same way later because it's gross */
    struct pollfd *my = &pfds[0];
    struct pollfd *peer = &pfds[1];
    int ready = 0, stillopen = 1;
    bool_t ateof = FALSE;
    ssize_t bytes_read = 0;
    int events = POLLIN | POLLHUP | POLLERR | POLLNVAL, revents;
    /* set keepalive on sock */
    if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &stillopen, sizeof(int)) ==
        -1) {
        sock = -1;
        return -1;
    }
    peer->fd = sock;
    peer->events = events;
    my->fd = STDIN_FILENO;
    my->events = events;
    if (!opts.nowindow) {
        start_windowing();
    }
    while (!ateof) {
        ready = poll(pfds, 2, CONNECTION_TIMEOUT);
        switch (ready) {
        case Success: {
            /* check ourselves (if revents non zero)*/
            revents = my->revents;
            if (revents & POLLIN) {
                update_input_buffer();
                if ((ateof = has_hit_eof()) || has_whole_line()) {
                    do {
                        bytes_read = read_from_input(packet, PACKET_SIZE);
                        if (bytes_read == ERR) {
                            perror("read_from_input");
                            return -1;
                        }
                        if ((send(sock, packet, bytes_read, 0)) == -1) {
                            perror("send message");
                            return -1;
                        }
                    } while (bytes_read == PACKET_SIZE - 1);
                }
            }

            /* check peer (if revents non zero) */
            revents = peer->revents;
            if (revents & POLLIN) {
                do {
                    bytes_read = recv(sock, packet, PACKET_SIZE, 0);
                    switch (bytes_read) {
                    case -1:
                        perror("recieve");
                        return -1;
                    case 0:
                        return 0;
                    }
                    if ((write_to_output(packet, bytes_read)) == ERR) {
                        perror("write");
                        return -1;
                    }
                } while (bytes_read == PACKET_SIZE - 1);
            }
            if (revents & POLLHUP || revents & POLLNVAL) {
                /* other side closed */
                sock = -1;
                return 0;
            }
            if (revents & POLLERR) {
                return -1;
            }
        } break;
        case Timedout: {
            fprintf(stderr, "Connection to remote timed out\n");
        }
        case Err: {
            perror("remote");
        }
        default: {
            return -1;
        }
        }
    }
    return 0;
}

int setup_server(char *port) {
    fd_set set;
    char chunk[PACKET_SIZE], confirm[CONFIRMATION_BUF_SIZE];
    char *rejecstr, *hostname, *peer_uname;
    size_t peer_unamelen = 0, bytes_rec, rejectlen;
    bool_t confirmed;
    /* short of port in network byte order */
    port_t nsport = htons(atoi(port));
    /* copy val of sock */
    sock_t listener = sock;
    struct sockaddr_in sockaddr;
    /* might need to save in conn struct later */
    socklen_t peer_sockaddrlen = sizeof(struct sockaddr_in);

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = nsport;
    sockaddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listener, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
        perror("bind");
        fprintf(stderr, "%d\n", listener);
        return -1;
    }

    /*
     * listen for connections
     */
    FD_ZERO(&set);
    FD_SET(listener, &set);

    if (listen(listener, 1) == -1) {
        perror("listen");
    }
    printf("Awaiting connections at %s:%d\n", inet_ntoa(sockaddr.sin_addr),
           ntohs(nsport));

    sock = accept(listener, (struct sockaddr *)&sockaddr, &peer_sockaddrlen);
    if (sock == -1) {
        perror("recieving connection requrest");
        return -1;
    }
    if (opts.verbose) {
        printf("connection found!\n");
    }
    /* block while awaiting username */
    peer_uname = (char *)calloc(PACKET_SIZE, sizeof(char));
    if (peer_uname == NULL) {
        perror("malloc");
    }
    do {
        bytes_rec = recv(sock, chunk, PACKET_SIZE, 0);
        if (peer_unamelen == -1) {
            perror("recieve peer username");
            return -1;
        }
        peer_unamelen += bytes_rec;
        strcat(peer_uname, chunk);
    } while (bytes_rec == PACKET_SIZE &&
             (peer_uname = realloc(peer_uname, peer_unamelen + PACKET_SIZE)) !=
                 NULL);
    if (peer_uname == NULL) {
        perror("malloc space for peer uname");
        return -1;
    }
    fprint_to_output("Mytalk request from %s@%s. Accept (y/n): ", peer_uname,
                     inet_ntoa(sockaddr.sin_addr));
    free(peer_uname);
    if (opts.autoaccept) {
        confirmed = TRUE;
    } else {
        memset(confirm, 0, CONFIRMATION_BUF_SIZE);

        /* ask for confirmation */
        scanf("%3s", confirm);

        /* if it equals y[es]{'\0','\n'} it works */
        confirmed = (confirm[0] == 'y' &&
                     ((confirm[1] == '\0' || confirm[1] == '\n') ||
                      (confirm[1] == 'e' && confirm[2] == 's' &&
                       (confirm[3] == '\0' || confirm[1] == '\n'))));
    }
    if (confirmed) {
        if ((send(sock, CONFIRMATION_OK, 3, 0)) == -1) {
            perror("confirm connection");
            return -1;
        }
        if (opts.verbose) {
            printf("connecting to peer...\n");
        }
        return talk();
    } else {
        hostname = inet_ntoa(sockaddr.sin_addr);
        rejectlen = strlen(hostname) + strlen(REJEC_STR) + 1;
        rejecstr = malloc(rejectlen);
        if (rejecstr == NULL) {
            perror("rejection malloc");
            return -1;
        }
        strcpy(rejecstr, hostname);
        strcat(rejecstr, REJEC_STR);
        if (send(sock, rejecstr, rejectlen, 0) == -1) {
            perror("send rejection");
        }
        free(rejecstr);
        printf("Request denied.\n");
        return -1;
    }
    return 0;
}

int setup_client(char *hostname, char *port) {
    struct addrinfo hints, *res = NULL;
    char confirmation_buf[CONFIRMATION_BUF_SIZE];
    struct passwd *pwd_uid;
    int uid, ga_res;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;

    ga_res = getaddrinfo(hostname, port, &hints, &res);
    if (ga_res != 0 || res == NULL) {
        fprintf(stderr, "%s\n", gai_strerror(ga_res));
        return -1;
    }
    /* blindly connect to first result */
    if (opts.verbose) {
        /* give warning that multiple options were found */
        if (res->ai_next != NULL) {
            fprintf(stderr, "Warning: Multiple network options found. Please "
                            "be more specific\n");
        }
    }
    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        perror("connect");
        return -1;
    }
    freeaddrinfo(res);

    uid = getuid();
    pwd_uid = getpwuid(uid);
    if (pwd_uid == NULL) {
        perror("user id");
        return -1;
    }
    if ((send(sock, pwd_uid->pw_name, strlen(pwd->pw_name), 0)) == -1) {
        perror("send uname");
        return -1;
    }
    memset(confirmation_buf, 0, CONFIRMATION_BUF_SIZE);
    if ((recv(sock, confirmation_buf, CONFIRMATION_BUF_SIZE, 0)) == -1) {
        perror("recieve");
        return -1;
    }
    if (strcmp(confirmation_buf, CONFIRMATION_OK) != 0) {
        fprintf(stderr, "%s declined connection.\n", hostname);
        return -1;
    }

    return 0;
}

/* acts as switch statement between setting up client or server */
int setup(char *hostname, char *port) {
    if (hostname == NULL) {
        return setup_server(port);
    } else {
        return setup_client(hostname, port);
    }
}

int main(int argc, char *argv[]) {
    const char OPTARGS[] = {AUTOACCEPT, VERBOSE, NOWINDOW, '\0'};
    int c;
    char *hostname = NULL, *port = NULL;
    /* setup defaults */
    opts.verbose = 0;
    opts.autoaccept = FALSE;
    opts.nowindow = FALSE;

    while ((c = getopt(argc, argv, OPTARGS)) != -1) {
        switch (c) {
        case AUTOACCEPT:
            opts.autoaccept = TRUE;
            break;
        case VERBOSE:
            /* increment verbose for every occurance */
            opts.verbose++;
            break;
        case NOWINDOW:
            opts.nowindow = TRUE;
            break;
        case '?':
            if (isprint(optopt)) {
                c = optopt;
            }
            fprintf(stderr, "Unknown option -%c\n%s", optopt, USAGESTR);
            return EXIT_FAILURE;
            break;
        }
    }
    set_verbosity(opts.verbose);

    /* how many non flag arguments */
    switch (argc - optind) {
    case 1: /* server mode (just port)*/
    {
        port = argv[optind];
    } break;
    case 2: /* client mode (hostname and port)*/
    {
        hostname = argv[optind];
        port = argv[optind + 1];
    } break;
    default:
        fprintf(stderr, "Incorrect Number of args\n%s", USAGESTR);
        return EXIT_FAILURE;
    }

    /* setup local sock somewhere os decides.
     * client/server setup funcs will handle connecting it
     * to something */
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("creating local sock");
        return EXIT_FAILURE;
    }

    /* run setup */
    if (setup(hostname, port) < 0) {
        fprintf(stderr, "Failed to setup connection\n");
        return EXIT_FAILURE;
    }
    /* begin talk */
    if (talk() < 0) {
        fprintf(stderr, "Connection error\n");
        /* dont return we want to try cleanup */
    }
    if (!cleanup()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
