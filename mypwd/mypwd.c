#include <dirent.h>
#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/* for declaring ints that will be used as booleans */
typedef int bool;

/* enough for lots of parents. Will append more as needed */
#define DOTDOTS                                                                \
    "../../../../../../../../../../../../../../../../../../../../../../../../" \
    "../../../../../../../../../../../../../../../../../../../../../../../../" \
    "../../../../../../../../../../../../../../../"
#define DOTDOTLEN 3
#ifndef PATH_MAX
#define PATH_MAX 2048
#endif
/* just so there is absolutely no magic numbers this macro is
 * used when allocating 1 extra space for a null terminator byte */
#define TERM 1
#define PATH_TOO_LONG_ERR "Path too long"
#define TRAILINGSLASH '/'
#define ROOTPATH "/"
#define CURRENTDIR "."
#define TMPDOTDOTNAMEBUFSIZE 100
#define CONTINUEFOREVERANDEVER 1

int lstat(const char *file, struct stat *buf);

int main(int argc, char *argv[]) {
    /* dotdot string allocated on stack.
     * will be copied to heap if necessary to add more */
    const char dotdotstack[] = DOTDOTS;
    size_t dotdotlen = strlen(dotdotstack);
    /* dotdot points to end (null term byte) of dotdotstack */
    char *dotdots = (char *)(dotdotstack + dotdotlen);
    /* this will always point to the head of the current dotdot
     * string even after its reallocated */
    char *dotdotstr = (char *)dotdotstack;
    /* says whether weve already alloced for dotdots
     * set as false because the first dotdotstr is on the stack */
    bool dotdotalloced = 0;
    size_t pathblocklen = PATH_MAX + TERM, namelen;
    const char *pathblock;
    char *path;
    struct stat st;
    DIR *dirstream;
    struct dirent *dent;
    ino_t rootino, curino, parentino, dino;
    dev_t rootdev, curdev, parentdev, ddev;
    /* path is a pointer to the head of the current path, initialized as the end
     * of the pathblock */
    bool mountpoint;
    char *tmpdotdots = NULL;
    char *tmpdotdotname;
    size_t tmpdotdotsnamelencap;
    size_t tmpdotdotslen;

    pathblock = (char *)malloc(pathblocklen);
    if (pathblock == NULL) {
        perror(argv[0]);
        exit(errno);
    }
    /* make path point to end of pathblock */
    path = (char *)(pathblock + pathblocklen - TERM);
    *path = '\0'; /* set term */

    /* store root ino and device num */
    if (lstat(ROOTPATH, &st) == -1)
        return errno;
    rootino = st.st_ino;
    rootdev = st.st_dev;

    /* intialize current dev and ino to look for */
    if (lstat(CURRENTDIR, &st) == -1)
        return errno;
    curino = st.st_ino;
    curdev = st.st_dev;

    while (CONTINUEFOREVERANDEVER) {
        /* this chunk adds more dotdots as needed */
        if ((dotdots - dotdotstr) <= DOTDOTLEN) {
            if (!dotdotalloced) {
                tmpdotdots = (char *)malloc((dotdotlen * 2));
            } else {
                tmpdotdots = (char *)realloc(dotdotstr, dotdotlen * 2);
            }
            if (tmpdotdots == NULL) {
                perror(argv[0]);
                exit(errno);
            }
            /* copy dotdotstr as string to first half of allocated
             * chunk */
            strcpy(tmpdotdots, dotdotstr);
            /* copy dotdotstr again to second half - 1 to overwrite
             * the null byte of the first block */
            strcpy(tmpdotdots + dotdotlen - 1, dotdotstr);
            /* set dotdots to correct place in second block  */
            dotdots = (char *)(tmpdotdots + dotdotlen + (dotdots - dotdotstr));
            dotdotstr = tmpdotdots;
            tmpdotdots = NULL;
            dotdotlen *= 2;
        }
        dotdots -= DOTDOTLEN;
        dirstream = opendir(dotdots);
        if (lstat(dotdots, &st) == -1)
            return errno;
        parentdev = st.st_dev;
        parentino = st.st_ino;
        mountpoint = curdev != parentdev;
        /* insert slash */
        *(--path) = TRAILINGSLASH;
        /* this is down here instead of in while condition
         * to get slash */
        if ((curino == rootino && curdev == rootdev)) {
            closedir(dirstream);
            break;
        }

        while ((dent = readdir(dirstream)) != NULL) {
            dino = dent->d_ino;
            ddev = curdev;
            if (strcmp(".", dent->d_name) == 0 ||
                strcmp("..", dent->d_name) == 0) {
                continue;
            }
            if (mountpoint && dent->d_ino != curino) {
                /* this code block allocates space for and creates a path using
                 * the current dotdot string and the directories name. It then
                 * sets dino and ddev based on the results of statting that
                 * path. this is because the dirent will not hold the correct
                 * inode number in the case that it is the mountpoint for a
                 * filesystem */
                if (tmpdotdots == NULL) {
                    tmpdotdotslen =
                        (size_t)(strlen(dotdots) + TMPDOTDOTNAMEBUFSIZE);
                    tmpdotdots = (char *)malloc(tmpdotdotslen);
                    tmpdotdotname = (char *)(tmpdotdots + tmpdotdotslen -
                                             TMPDOTDOTNAMEBUFSIZE);
                    strcpy(tmpdotdots, dotdots);
                    tmpdotdotsnamelencap = TMPDOTDOTNAMEBUFSIZE;
                }
                if (strlen(dent->d_name) >= tmpdotdotsnamelencap) {
                    tmpdotdotsnamelencap =
                        (TMPDOTDOTNAMEBUFSIZE +
                         (strlen(dent->d_name) + TMPDOTDOTNAMEBUFSIZE));
                    tmpdotdots = (char *)realloc(
                        tmpdotdots, tmpdotdotslen + tmpdotdotsnamelencap);
                }
                strcpy(tmpdotdotname, dent->d_name);
                /* stat created path */
                if (lstat(tmpdotdots, &st) == -1) {
                    perror(argv[0]);
                    return errno;
                }
                /* update the values we're about to check */
                dino = st.st_ino;
                ddev = st.st_dev;
            }
            if (dino == curino && ddev == curdev) {
                break;
            }
        }
        namelen = strlen(dent->d_name);
        /* thank you for letting us do this */
        if ((path - pathblock) <= namelen)
            error(1, 1, "%s\n", PATH_TOO_LONG_ERR);
        path -= namelen;
        memcpy(path, dent->d_name, namelen);
        curdev = parentdev;
        curino = parentino;
        tmpdotdotname = NULL;
        if (tmpdotdots != NULL)
            free(tmpdotdots);
        tmpdotdots = NULL;
        closedir(dirstream);
    }

    printf("%s\n", path);
    free((void *)pathblock);
    return 0;
}
