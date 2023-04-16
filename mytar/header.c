/*
 * Header.c handles all header functionality.
 * reading headers from archives, inserting and extracting
 * information from headers, and formatting the path/prefix.
 * the corresponding header.h declares many macros and constants
 * related to the spec
 */
#include "header.h"

#include <errno.h>
#include <error.h>
#include <grp.h>
#include <pwd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "archive.h"
#include "bool.h"

/* used when calculating chksum and treating all bytes in chksum
 * block as spaces */

const uint32_t CHKSUM_AS_SPACES = (((unsigned int)' ') * CHKSUM_SIZE);
#define MIN(a, b) ((a) == (b) ? 0 : ((a) > (b) ? (a) : (b)))
/* whether ch is end of full path. true when '/' (dir) or '\0' (file) */
int isendoffullpath(char *p) {
    return (isterm(*p) || *p == '/' || isterm(p[1]) || p[1] == '/');
}

/* assumes buff has enough space for the default perms string (MODE_WIDTH+1) */
int parse_perms(const Header *h, char buf[], bool strict) {
    int i = 0;
    int mode = 0;
    int access_perms_array[] = ACCESS_PERMS_ARRAY;
    strcpy(buf, DEFAULT_PERMS_STR);
    /* first bit */
    switch (*(h->typeflag)) {
    case TYPEFLAG_DIRECTORY:
        buf[i] = PERM_DIRECTORY_FILETYPE;
        break;
    case TYPEFLAG_SYMBOLIC_LINK:
        buf[i] = PERM_SYMLINK_FILETYPE;
        break;
    case TYPEFLAG_REGULAR_FILE:
    case TYPEFLAG_REGULAR_FILE_ALT:
        /* do nothing */
        break;
    default:
        /* doesn't break just leaves buf as is */
        fprintf(stderr, "Unacceptable typeflag: %c\n", *(h->typeflag));
    }
    mode = extract_octal(h->mode, MODE_SIZE, strict);
    /* loops through ACCESS_PERMS_ARRAY which holds the flags for each char in
     * perms string in proper order */
    for (i = 1; i < PERMS_WIDTH; i++) {
        /* execute is always third endex of user,group,other perms */
        if (mode & access_perms_array[i]) {
            if (i % MODE_EXEC_INDEX == 0) {
                buf[i] = EXEC_PERM;
                continue;
            }
            /*
             * at index 4 i % 2 == 0 but index actually corresponds to read.
             * instead of using MODE_WRITE_INDEX (=2) we check if the next
             * index corresponds to exec as this will always result in the
             * correct action
             */
            else if ((i + 1) % MODE_EXEC_INDEX == 0) {
                buf[i] = WRITE_PERM;
                continue;
            }
            /* else */
            buf[i] = READ_PERM;
        }
    }
    return 1;
}

/* prints the permission, group/user, size, mtime info.
 * DOES NOT PRINT FILENAME OR NEWLINE */
int print_header_info_verbose(Header *h, bool strict) {
    char permsbuf[PERMS_WIDTH + 1], owngroupbuf[OWNGROUP_WIDTH + 1],
        mtimebuf[MTIME_WIDTH + 1];
    time_t mtime = extract_octal(h->mtime, MTIME_SIZE, strict);
    size_t size = extract_octal(h->size, SIZE_SIZE, strict);
    struct tm *mtime_st = NULL;
    int err = 0;
    mtime_st = localtime(&mtime);
    if (mtime < 0 || size < 0 || mtime_st == NULL) {
        error_at_line(0, errno, __func__, __LINE__, "%s\n",
                      "Failed to read time and size info from archive\n");
        err--;
        goto cleanup;
    }

    parse_perms(h, permsbuf, strict);
    strftime(mtimebuf, sizeof(mtimebuf), "%Y-%m-%d %H:%M", mtime_st);

    /* TODO: format stuff for max chars in string corresponding to NAME
     * and PREFIX _SIZE */
    snprintf(owngroupbuf, sizeof(owngroupbuf), "%s%s%s", h->uname, "/",
             h->gname);
    printf("%-*s %-*s %*lu %-*s ", PERMS_WIDTH, permsbuf, OWNGROUP_WIDTH,
           owngroupbuf, SIZE_WIDTH, size, MTIME_WIDTH, mtimebuf);
    /* no newline because we will print out filename regardless of
     * verbosity */
cleanup:
    return err;
}

/* splits a filepath contained int completepath into separate prefix and name
 * blocks unless the complete filepath can fit into name path then it just copys
 * it there */
char *splitpath(const char completepath[], size_t lenpath,
                char prefixbuf[PREFIX_SIZE], char namebuf[NAME_SIZE]) {
    size_t ofs = 0;
    if (lenpath == 0) {
        lenpath = strlen(completepath);
    }
    if (lenpath > NAME_SIZE) {
        if (lenpath > (NAME_SIZE + PREFIX_SIZE)) {
            goto tolong;
        }
        /* where name begins */
        /* ofs = lenpath - NAME_SIZE; */
        for (ofs = (lenpath - NAME_SIZE); ofs < lenpath && ofs < PREFIX_SIZE;
             ofs++) {
            if (*(completepath + ofs) == '/') {
                ofs++;
                break;
            }
        }
        if (ofs > PREFIX_SIZE) {
        tolong:
            fprintf(stderr, "mytar: File path too long.\nFile: %s\n",
                    completepath);
            return NULL;
        }
    } else {
        ofs = 0;
    }
    strcpy(namebuf, completepath + ofs);
    /* ofs > 0 if there is path to put into the prefix */
    if (ofs > 0) {
        /* ofs - 1 to avoid copying trailing slash */
        memcpy(prefixbuf, completepath, ofs - 1);
    }
    return (char *)completepath + ofs;
}

/* assumes the buf it recieves is big enough to store full path.
 * clears the buffer before use */
char *joinpath(const Header *h, char buf[PREFIX_SIZE + NAME_SIZE]) {
    /* the strlen is a clean trick. uses boolean to either give width of 1
     * or zero (t/f respectively) which will decide whether slash is printed
     * or not */
    memset(buf, 0, PREFIX_SIZE + NAME_SIZE);
    snprintf(buf, PREFIX_SIZE + NAME_SIZE, "%.*s%.*s%.*s", PREFIX_SIZE,
             h->prefix, (strlen(h->prefix) != 0), "/", NAME_SIZE, h->name);
    return buf;
}

size_t mystr_len(char *s) {
    size_t i = 0;
    while (!isterm(i++)) {
        /* do nothing */
    }
    return i;
}

/*
 * returns true or false whether the path in this header begins with needle
 * and needle matches a full path.
 * i.e. needle= name/subdir would not match path= name/subdirectory/
 * but needle = name/subdirectory or name/subdirectory/ would
 */
int pathbegwith(char pathbuf[NAME_SIZE + PREFIX_SIZE], const char *needle) {
    size_t plen = strlen(pathbuf);
    size_t needlen = strlen(needle);
    if (needlen > plen || !isendoffullpath(pathbuf + needlen - 1)) {
        return false;
    }
    return !strncmp(pathbuf, needle, needlen);
}

/* computes checksum from header. returns zero if
 * header is empty */
uint32_t computechksum(Header *header) {
    int i;
    uint32_t sum = 0;
    unsigned char *headeraschars = (unsigned char *)header;

    for (i = 0; i < BLOCK_SIZE; i++) {
        if (i == CHKSUM_OFFSET) {
            /* instead of looping and multiplying during run time
             * just use macro */
            sum += CHKSUM_AS_SPACES;
            i += CHKSUM_SIZE;
        }
        /* when not in chksum add unsigned int representation */
        sum += headeraschars[i];
    }
    /* sum will be zero on empty header */
    /* potential problem: corrupted header with non empty chksum block only
     * will be viewed as completely empty */
    return (sum == CHKSUM_AS_SPACES ? 0 : sum);
}

/*
 * takes a pointer to a archive file in memory.
 * (almost certianly memory mapped)
 * as well as an offset: where to read from,
 * and mmapsize: used to figure out where the end is.
 */
int read_header_block(const char *arkmmap, size_t offset, size_t mmapsize,
                      Header *h, bool strict) {
    /* ssize_t sizeread; */
    /* size_t size; */
    enum header_validity validity;

    /* verify header in arkmmap before copying to buffer */
    Header *mmapedheader = (Header *)(arkmmap + offset);
    /* offset should never be more than two blocks from end of file */
    if (offset + 2 * BLOCK_SIZE > mmapsize) {
        errno = EFAULT;
        return -1;
    }
    validity = verify_header(mmapedheader, strict);
    offset += BLOCK_SIZE;

    if (validity == valid) {
        /* found valid header copy to header buf
         * and return new offset */
        memcpy(h, mmapedheader, BLOCK_SIZE);
        return offset;
    } else if (validity == invalid) {
        fprintf(stderr, "Invalid Archive\n");
        errno = EINVAL;
    } else if (validity == empty) {
        mmapedheader = (Header *)(arkmmap + offset);
        if (verify_header(mmapedheader, strict) == empty) {
            return 0;
        } else {
            /* this is a rare case but why not check */
            fprintf(stderr,
                    "Corrupted Archive. Empty header before non-empty Header");
            errno = EINVAL;
        }
    }
    /* invalid */
    return -1;
}

enum header_validity verify_header(Header *h, bool strict) {
    uint32_t calcchksum;
    char expecvnum[] = VERSION_NUM;
    char namebuf[PREFIX_SIZE + NAME_SIZE];
    /* check checksum */
    calcchksum = computechksum(h);
    /* checks if chksum parsed as normal int is equal to calculated chksum
     * and if that fails checks if its equal to chksum parsed as special int */
    if (extract_octal(h->chksum, CHKSUM_SIZE, strict) == calcchksum ||
        extract_special_int(h->chksum, CHKSUM_SIZE == calcchksum)) {
        if (calcchksum == 0) {
            return empty;
        }
        /* compare size MAGIC_SIZE - 1 to ignore terminator */
        if (memcmp(h->magic, USTAR, MAGIC_SIZE - 1) == 0) {
            if (strict) {
                /* ensure terminator in header */
                if (!isterm(h->magic[MAGIC_SIZE - 1])) {
                    fprintf(stderr, "%s: magic number not terminated\n",
                            joinpath(h, namebuf));
                    return invalid;
                }
                if (memcmp(h->version, expecvnum, 2) != 0) {
                    fprintf(stderr, "%s: version num %.*s not \"00\"\n",
                            joinpath(h, namebuf), VERSION_SIZE, h->version);
                    return invalid;
                }
            }
            return valid;
        } else {
            fprintf(stderr, "%s: found magic \"%.*s\" expected \"%s\"\n",
                    joinpath(h, namebuf), MAGIC_SIZE, h->magic, USTAR);
        }
    } else {
        fprintf(stderr, "%s: checksum invalid\n", joinpath(h, namebuf));
    }
    return invalid;
}

int setup_common_header(Header *h, char *filepath, struct stat *st,
                        bool strict) {
    struct passwd *u;
    struct group *gr;
    /* name */
    size_t lenpath = strlen(filepath);
    int err = 0, val, perrno = errno;
    errno = 0;
#define erratline                                                              \
    if (errno) {                                                               \
        error_at_line(0, errno, __func__, __LINE__,                            \
                      "Error filling field\nval = %dl", val);                  \
    }

    /* will split complete path into prefix and name */
    splitpath(filepath, lenpath, h->prefix, h->name);

    /* mode */
    val = st->st_mode;
    err += insert_octal(val, h->mode, MODE_SIZE, strict);

    if (err < 1) {
        /* uid */
        val = st->st_uid;
        err += insert_octal(val, h->uid, UID_SIZE, strict);
    }
    if (err < 1) {
        /* gid */
        val = st->st_gid;
        err += insert_octal(val, h->gid, GID_SIZE, strict);
    }
    /* size (but only if reg) */
    if (err < 1 && S_ISREG(st->st_mode)) {
        val = st->st_size;
        err += insert_octal(val, h->size, SIZE_SIZE, strict);
    }
    if (err < 1) {
        /* mtime */
        val = st->st_mtime;
        err += insert_octal(val, h->mtime, MTIME_SIZE, strict);
    }
    /* chksum calculated at very end */
    /* typeflag already done */
    /* linkname is uniq to link */
    if (err < 1) {
        val = 0;
        /* magic */
        strcpy(h->magic, USTAR);
    }
    if (err < 1) {
        val = 0;
        /* version num. memcpy to ignore null terminator byte */
        memcpy(h->version, VERSION_NUM, VERSION_SIZE);
    }
    if (err < 1) {
        val = 0;
        /* uname */
        u = getpwuid(st->st_uid);
        strncpy(h->uname, u->pw_name, UNAME_SIZE);
    }
    if (err < 1) {
        val = 0;
        /* gname */
        gr = getgrgid(st->st_gid);
        strncpy(h->gname, gr->gr_name, GNAME_SIZE);
    }
    if (err < 1) {
        /* major */
        val = major(st->st_dev);
        err += insert_octal(val, h->devmajor, DEVMAJOR_SIZE, strict);
    }
    if (err < 1) {
        /* minor */
        val = minor(st->st_dev);
        err += insert_octal(val, h->devminor, DEVMINOR_SIZE, strict);
    }

    if (err < 0) {
        fprintf(
            stderr,
            "File: %s has a value that cannot be archived because it "
            "is too large and negative. File will still be archived but field "
            "will be skipped\n",
            filepath);
        err = 0;
    }
    errno = perrno;
    return err;
}
