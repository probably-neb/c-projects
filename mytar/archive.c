/*
 * archive.c handles all of the archive related functionality of mytar.
 * It lists, reads, extracts, and creates archives.
 * i.e. handles each of the 'ctx' options.
 * consists of said handlers and their helper functions.
 * Uses header.c for header related functionality.
 */
#include "archive.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <grp.h>
#include <math.h>
#include <pwd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#include "bool.h"
#include "header.h"

/* for verbosely declaring file descriptors */
typedef int fd_t;

/* num of blocks of BLOCK_SIZE in write buffer */
#define EMPTYBLOCKSATEND 2

/*
 * Both print_archive_contents and extract_archive_contents call
 * the loop through archive funtion just with a boolean
 * telling it whether to extract or not
 */
int loop_through_archive_contents(char *archive, struct opts opts, bool extract,
                                  char **searchterms, int numsearchterms) {
    Header h;
    unsigned long int size = 0, skipamount = 0;
    int i, err = 0;
    bool search = !(searchterms == NULL || numsearchterms == 0),
         searchmatch = false, foundsomething = false, list = !extract;
    char pathbuf[PREFIX_SIZE + NAME_SIZE + 1];

    struct stat st;
    fd_t ark = 0, fd = -1;
    char *arkmmap = NULL;
    ssize_t offset = 0, arksize = 0;
    /* to avoid side effects */

    if ((ark = open(archive, O_RDONLY)) == -1 || fstat(ark, &st) == -1 ||
        (arkmmap = mmap(NULL, (arksize = st.st_size), PROT_READ, MAP_PRIVATE,
                        ark, 0)) == MAP_FAILED) {
        /* critical error. return to caller */
        error_at_line(0, errno, __func__, __LINE__,
                      "Failed to open archive %s\n", archive);
        goto ret;
    }

    while (!err && (offset = read_header_block(arkmmap, offset, arksize, &h,
                                               opts.strict)) > 0) {
        joinpath(&h, pathbuf);

        size = extract_octal(h.size, SIZE_SIZE, opts.strict);
        /* if there are search terms to look for */
        if (search) {
            for (i = 0; i < numsearchterms; i++) {
                if ((searchmatch = pathbegwith(pathbuf, searchterms[i]))) {
                    break;
                }
            }
        }
        /* if we are printing any entry we find or current header matched a
         * search term */
        if (!search || searchmatch) {
            if (extract) {
                if (errno) {
                    error_at_line(0, errno, __func__, __LINE__,
                                  "Found error before creating file\n");
                }
                err += create_file(pathbuf, &h, &fd, opts.strict);
            }
            if (opts.verbose && list) {
                err += print_header_info_verbose(&h, opts.strict);
            }
            if (opts.verbose || list) {
                /* regardless of verbose for list.
                 * only for verbose when extract */
                printf("%s\n", pathbuf);
                searchmatch = false;
            }
            foundsomething = true;
        }
        /* else: valid */
        if (!err && *h.typeflag == TYPEFLAG_REGULAR_FILE && size != 0) {
            /* seek ahead correct number of bytes */
            skipamount = next_highest_multiple(size, BLOCK_SIZE);
            if (offset + skipamount <= st.st_size) {
                if (extract && fd != -1) {
                    if ((write(fd, arkmmap + offset, size)) == -1) {
                        error_at_line(0, errno, __func__, __LINE__,
                                      "Failed to write to file\n");
                        err = errno;
                    }
                    close(fd);
                    fd = -1;
                }
                offset += skipamount;
            }
        }
    }

    if (!err && offset == -1) {
        /* error reading header */
        err = errno;
    }
    if (!err && !foundsomething) {
        if (numsearchterms > 1) {
            /* "not found message" only printed with only one search item */
            errno = EINVAL;
        } else {
            errno = ENOENT;
            if (search) {
                fprintf(stderr, "%s: %s: Not found in archive\n", "mytar",
                        searchterms[0]);
            }
        }
        err = errno;
    }
ret:
    if (arkmmap != NULL) {
        munmap(arkmmap, arksize);
    }
    return err;
}

/*
 * LIST ARCHIVE MODE HANDLER
 * TODO: when printing errors print filename (especially invalid archive)
 */
int print_archive_contents(char *archive, struct opts opts, char **searchterms,
                           int numsearchterms) {
    /* just to explicitly say what the bool is doing in loop function call */
    bool extract;
    return loop_through_archive_contents(archive, opts, (extract = false),
                                         searchterms, numsearchterms);
}

/*
 * EXTRACT MODE HANDLER
 */
int extract_archive_contents(char *archive, struct opts opts,
                             char **searchterms, int numsearchterms) {
    /* just to explicitly say what the bool is doing in loop function call */
    bool extract;
    return loop_through_archive_contents(archive, opts, (extract = true),
                                         searchterms, numsearchterms);
}

int archive_file(FILE *archive, char filepath[PREFIX_SIZE + NAME_SIZE],
                 Header *h, struct stat *st, struct opts *opts);

/*
 * CREATE MODE HANDLER
 */
int create_archive(char *archive, struct opts opts, char **searchterms,
                   int numsearchterms) {
    /* these do not have to be initialized on every recursive call of archive
     * file so instead they are passed. Helps keep code cleaner, and avoids
     * declaring each buffer on every recursion */
    FILE *ark;
    /* used for storing working path in search */
    char filepath[PREFIX_SIZE + NAME_SIZE];
    /* file header */
    Header h;
    struct stat st;
    int i;
    if ((ark = fopen(archive, "w")) == NULL) {
        error_at_line(0, errno, __func__, __LINE__, "Tarfile %s not found\n",
                      archive);
    }
    memset(filepath, 0, PREFIX_SIZE + NAME_SIZE);
    memset(&h, 0, sizeof(Header));

    for (i = 0; i < numsearchterms; i++) {
        /* store filename in pathbuf */
        strcpy(filepath, searchterms[i]);
        archive_file(ark, filepath, &h, &st, &opts);
#ifdef DEBUG
        fprintf(stderr, "Archiving %s\n", searchterms[i]);
#endif
    }
    /* clear header */
    memset(&h, 0, sizeof(Header));
    /* two empty blocks */
    for (i = 0; i < EMPTYBLOCKSATEND * BLOCK_SIZE; i++) {
        fputc(0, ark);
    }
    fclose(ark);
    return 0;
}

/* archives file if of type file or symlink. If dir, recurses on the
 * dirs contents storing all children. */
int archive_file(FILE *archive, char filepath[PREFIX_SIZE + NAME_SIZE],
                 Header *h, struct stat *st, struct opts *opts) {
    int fd = -1;
    DIR *dstr = NULL;
    struct dirent *dent = NULL;
    char *npathbeg, tf;
    size_t lenpath = 0;
    mode_t m;
    FILE *reg = NULL;
    int ch = -1, cnt = 0, err = 0, nexblockstart, preverrno;
    /* following if also sets ffd(search term file descriptor) and
        stats it */
    preverrno = errno;
    fd = open(filepath, O_RDONLY | O_NOFOLLOW);
    if (fd == -1 || fstat(fd, st) == -1) {
        /*
         * because of O_NOFOLLOW errno will be set to ELOOP if
         * file is a symbolic link.
         * in this case we lstat the file and continue
         */
        if (!(errno == ELOOP && (lstat(filepath, st) != -1))) {
            fprintf(stderr, "mytar: " /* no newline so perror appends */);
            error_at_line(0, errno, __func__, __LINE__, "File %s not found\n",
                          filepath);
            /* just skip failed files */
            goto cleanup;
        }
    }
    errno = preverrno;

    /* do typeflag first so we can also use it to see if we have an unsupported
     * filetype */
    m = st->st_mode; /* because lazy */
    if (S_ISLNK(m)) {
        if (st->st_size > LINKNAME_SIZE) {
            fprintf(stderr, "Link name too long for link: %s\n", filepath);
            goto cleanup;
            ;
        }
        if (readlink(filepath, h->linkname, LINKNAME_SIZE) == -1) {
            error_at_line(0, errno, __func__, __LINE__,
                          "Failed to read linkname for link: %s\n", filepath);
            goto cleanup;
        }
        tf = TYPEFLAG_SYMBOLIC_LINK;
    } else if (S_ISREG(m)) {
        tf = TYPEFLAG_REGULAR_FILE;
    } else if (S_ISDIR(m)) {
        tf = TYPEFLAG_DIRECTORY;
        ensure_trailing_slash(filepath, 0);
    } else {
        fprintf(stderr, "mytar: %s: unsupported filetype\n", filepath);
        goto cleanup;
    }
    *h->typeflag = tf;

    if (opts->verbose) {
        printf("%s\n", filepath);
    }

    /* common header setup handles its own errors */
    setup_common_header(h, filepath, st, opts->strict);

    /* must do chksum last */
    /* chksum */
    insert_octal(computechksum(h), h->chksum, CHKSUM_SIZE, opts->strict);

    /* write header to archive */
    if (fwrite(h, BLOCK_SIZE, 1, archive) < 0) {
        goto cleanup;
    }
    /* clear header here instead of before to use as potential block of 0's
     * when writing data */
    memset(h, 0, BLOCK_SIZE);

    /* write data if regular file */
    if (S_ISREG(m)) {
        if ((reg = fdopen(fd, "r")) == NULL) {
            error_at_line(0, errno, __func__, __LINE__,
                          "Failed to open file: %s\n", filepath);
        }
        while ((ch = fgetc(reg)) != EOF) {
            fputc(ch, archive);
            cnt++;
        }
        if (cnt != (int)(st->st_size)) {
            fprintf(
                stderr,
                "Mismached read size (%d) and stat size (%d) for file: %s\n",
                cnt, (int)st->st_size, filepath);
        }

        nexblockstart = next_highest_multiple(cnt, BLOCK_SIZE);
        /* write zeros to end of block */
        while (cnt != nexblockstart) {
            fputc(0, archive);
            cnt++;
        }
        /* fclose(reg); */
        /* reg = NULL; */
    }

    /* recurse through subfiles file is a dir */
    if (S_ISDIR(m)) {
        /* TODO: append trailing slash to name if dir */
        if ((dstr = fdopendir(fd)) == NULL || errno) {
            error_at_line(0, errno, __func__, __LINE__,
                          "Failed to open dir: %s\n", filepath);
            /* just return if we failed to open dir */
            goto cleanup;
        }
        /* lenpath & npathbeg are used to set null byte after current dirs path
         * which cuts the dirs entries paths that get appended off.
         * i.e.
         * filepath: "home/"
         * entry in home dir: ".config/"
         * resulting path on recusive step: "home/.config/"
         * after recursive call: "home/'\0'config/
         * so it is usable for next recursive call to write over
         * the contents of the previous"
         */
        lenpath = strlen(filepath);
        npathbeg = (char *)(filepath + lenpath);
        /* set errno as zero to distinguish end of dir stream from error */
        errno = 0;

        while ((dent = readdir(dstr)) != NULL) {
            if (dent->d_name[0] == '.' &&
                ((dent->d_name[1] == '.' && dent->d_name[2] == '\0') ||
                 dent->d_name[1] == '\0')) {
                continue;
            }
            /* uses base dir (not recursed entrys) pathlen to cut recursed dir
             * off
             */
            lenpath = ensure_trailing_slash(filepath, lenpath);
            /* fprintf(stderr, "npathbeg: %s\n filepath: %s\n", npathbeg,
             * filepath);
             */
            strcpy(npathbeg, dent->d_name);
            /* assume recursive call handles errors */
            archive_file(archive, filepath, h, st, opts);
        }
        if (errno) {
            error_at_line(0, errno, __func__, __LINE__,
                          "Failed to read directory %s\n", filepath);
        }
    }

cleanup:
    if (dstr != NULL) {
        closedir(dstr);
    } else if (reg != NULL) {
        fclose(reg);
        reg = NULL;
    } else if (fd != -1 && fcntl(fd, F_GETFD) != -1) {
        close(fd);
        fd = -1;
    }
    return err;
}

unsigned long int next_highest_multiple(unsigned long int n,
                                        unsigned int factor) {
    return (((n - 1) | (factor - 1)) + 1);
}

/* creates (but does not write too) all files (of supported filetypes) in
 * pathbuf including parent directories. updates the file descriptor pointed
 * to by fd with the opened file if a reg file is created. returns 0 on
 * success, -1 otherwise */
int create_file(char pathbuf[NAME_SIZE + PREFIX_SIZE + 1], Header *h, int *fd,
                bool strict) {
    size_t pathlen = strlen(pathbuf);
    int err = 0;
    mode_t mode = 0;
    char *index = NULL;
    int preverrno = errno;
    bool isdir = (*h->typeflag == TYPEFLAG_DIRECTORY);
    struct utimbuf htimes;
    struct stat st;

    errno = 0;
    if (isdir) {
        /* remove trailing slash if dir.
         * switch statement later will incorporate more information than
         * our simple make directories loop */
        if (pathbuf[pathlen - 1] == '/') {
            pathbuf[pathlen - 1] = '\0';
        }
    }

    if (pathlen == 0) {
        /* assumed this is never the case but check anyway */
        err--;
        errno = EINVAL;
        error_at_line(0, errno, __func__, __LINE__, "Recieved empty path\n");
    }
    mode = S_IRWALL | S_IXALL; /* all perms */

    index = pathbuf;
    /* ensure any parent directories exist */
    for (index = pathbuf; !err && !isterm(*index); index++) {
        if (*index != '/') {
            continue;
        }
        *index = '\0';
        /* fprintf(stderr, "fp:%s\nns:%s\n", pathbuf, nslash); */
        /* make dir if it doesn't exist */
        /* don't break when dir already exists */
        /* fprintf(stderr, "Creating dir: %s\n", pathbuf); */
        if ((mkdir(pathbuf, mode) == -1) && errno != EEXIST) {
            error_at_line(0, errno, __func__, __LINE__,
                          "failed to create dir: %s\n", pathbuf);
            err--;
            break;
        }
        *index = '/';
    }
    /* regular file or link */
    if (!err) {
        /* if (S_IXALL & (extract_octal(h->mode, MODE_SIZE, strict))) { */
        /*   mode &= ~S_IXALL; */
        /* } */
        switch (*h->typeflag) {
        case TYPEFLAG_REGULAR_FILE_ALT: /* fall through */
        case TYPEFLAG_REGULAR_FILE:
            if ((*fd = open(pathbuf, O_WRONLY | O_CREAT | O_TRUNC, mode)) ==
                -1) {
                error_at_line(0, errno, __func__, __LINE__,
                              "Failed to create file: %s\n", pathbuf);
                err--;
            }
            break;
        case TYPEFLAG_DIRECTORY:
            if (mkdir(pathbuf, mode) == -1 && errno != EEXIST) {
                error_at_line(0, errno, __func__, __LINE__,
                              "Failed to create dir: %s\n", pathbuf);
                err--;
            } else {
                /* maintain errno jic */
                errno = preverrno;
            }
            break;
        case TYPEFLAG_SYMBOLIC_LINK:
            if (symlink(h->linkname, pathbuf) == -1) {
                error_at_line(0, errno, __func__, __LINE__,
                              "Failed to create symlink: %s\n", pathbuf);
                err--;
            }
            break;
        default:
            /* not an error just skip */
            fprintf(stderr, "%s: Unsupported file type: %c\n", pathbuf,
                    *h->typeflag);
        }
    }
    /* replace trailing slash */
    if (isdir) {
        ensure_trailing_slash(pathbuf, pathlen);
    }
    /* restore mtime */
    if (!err) {
        if (lstat(pathbuf, &st) == -1) {
            error_at_line(0, errno, __func__, __LINE__,
                          "Failed to read created file: %s", pathbuf);
        }
        htimes.actime = st.st_atime; /* dont mess with access time */
        htimes.modtime = extract_octal(h->mtime, MTIME_SIZE, strict);
        if (utime(pathbuf, &htimes) == -1) {
            error_at_line(
                0, errno, __func__, __LINE__,
                "Failed to set correct modification time for file: %s",
                pathbuf);
        }
    }
    errno = preverrno;
    return err;
}

/* courtesy of professor Nico. */
int insert_special_int(char *where, size_t size, int32_t val) {
    /* For interoperability with GNU tar. GNU seems to
     * set the high-order bit of the first byte, then
     * treat the rest of the field as a binary integer
     * in network byte order.
     * Insert the given integer into the given field
     * using this technique. Returns 0 on FAILURE, nonzero
     * otherwise
     */
    int err = 0;
    if (val < 0 || (size < sizeof(val))) {
        /* if it's negative, bit 31 is set and we can't use the flag
        if len is too small, we can't write it. Either way, we're
        * done.
        */
        err++;
    } else {
        /* game on....*/
        memset(where, 0, size); /* Clear out the buffer */
        *(int32_t *)(where + size - sizeof(val)) =
            htonl(val); /* place the int */
        *where |= 0x80; /* set that high-order bit */
    }
    return err;
}

/* follows naming convention of other c int conversion functions
 * stores an ascii string representation of the unsigned int in octal form
 * in the buffer with size cap which is interpreted as the amount of digits
 * that can be stored. if n is to large to store as an octal string in cap
 * number of digits inserts as special int according to the logic stated in
 * insert_special_int if insert_special_int fails because bit 31 is
 * already 1.
 * returns -1 if UNARCHIVEABLEOCTAL, >0 if err, and 0 if no err */
int insert_octal(int n, char *where, size_t cap, bool strict) {
    /* SIZE_SIZE is biggest value cap will be in header */
    /* unsigned char buf[SIZE_SIZE]; */
    /* calculate log8 of n to see if we have enough bits */
    int err = 0;
    char buf[SIZE_SIZE + 1]; /* size is largest octal buf */
    memset(where, 0, cap);
    if (snprintf(buf, cap, "%o", n) > cap &&
        (strict || insert_special_int(where, cap, n) == -1)) {
        snprintf(where, cap, "%s", UNARCHIVEABLEOCTAL);
        fprintf(stderr,
                "mytar: value %d is to large to fit in an octal number of size "
                "%d. Skipping\n",
                n, (int)cap);
        err--;
    } else {
        memmove(where, buf, cap);
    }
    return err;
}

/* extracts an int val from where parsed as an unsigned ascii octal string,
 * or, if the last bit of where is 1 and strict is true it is extracted as
 * a binary number.
 * returns 0 if where matches UNARCHIVEABLEOCTAL, -1 on failure */
uint32_t extract_octal(const char *where, size_t cap, bool strict) {
    int val = 0;
    if ((val = memcmp(where, UNARCHIVEABLEOCTAL, cap - 1))) {
        /* extract_special_int will check if bit 31 is 1 */
        if (strict || (val = extract_special_int(where, cap)) < 0) {
            val = strtoul(where, NULL, OCTAL_BASE);
        }
    }
    return val;
}

uint32_t extract_special_int(const char *where, int len) {
    /* For interoperability with GNU tar. GNU seems to
     * set the high-order bit of the first byte, then
     * treat the rest of the field as a binary integer
     *in network byte order.
     * I don't know for sure if it's a 32 or 64-bit int, but for
     * this version, we'll only support 32. (well, 31)
     * returns the integer on success, -1 on failure.
     * In spite of the name of htonl(), it converts int32_t
     */
    int32_t val = -1;
    if ((len >= sizeof(val)) && (where[0] & 0x80)) {
        /* the top bit is set and we have space
         * extract the last four bytes */
        val = *(int32_t *)(where + len - sizeof(val));
        val = ntohl(val); /* convert to host byte order */
    }
    return val;
}

/* assumes it's input is terminated.
 * returns a pointer to the next '/' character
 * or end of string if not found.
 * therefore checking if the returned pointer points to a terminating
 * character will indicate whether a slash was found */
char *nextslash(char *path) {
    char *p = path;
    while (!isterm(*p) && *(++p) != '/') {
        /* do nothing */
    }
    return p;
}

/* if char in path at plen or just before is not '/' make it '/'.
 * returns the resulting pathlen. */
int ensure_trailing_slash(char *p, size_t plen) {
    if (plen == 0) {
        plen = strlen(p);
    }
    if (p[plen] != '/' && p[plen - 1] != '/') {
        p[plen] = '/';
        p[++plen] = '\0';
    }
    return plen;
}
