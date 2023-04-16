/*
 * mytar.c parses the options to mytar and calls the correct function
 * in archive.c and prints usage information if mytar is called incorrectly
 */
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "archive.h"
#include "header.h"

#define ARGS_INDEX 1
#define FILENAME_INDEX 2
#define SEARCH_TERMS_INDEX 3

#define CREATEMODE 'c'
#define PRINTMODE 't'
#define EXTRACTMODE 'x'
#define VERBOSEOPT 'v'
#define STRICTOPT 'S'
#define FILENAME 'f'

const char *USAGESTR = "[ctxvS]f tarfile [file1 [ file2 [...] ] ]";

typedef int (*modefunction)(char *, struct opts, char **, int);

int main(int argc, char *argv[]) {
    char *filename = NULL;
    int i;
    char **searchterms = NULL;
    int numsearchterms = 0;
    /* modefunc corresponds to the function that executes one of [ctx] */
    modefunction modefunc = NULL;
    /* defined in archive.h */
    struct opts opts;
    /* if search terms are required. set based on parsed mode */
    bool reqsterms = false;
    opts.verbose = false;
    opts.strict = false;

    if (argc == 1) {
        fprintf(stderr, "%s: missing required args\nUsage: %s\n", argv[0],
                USAGESTR);
    }

    for (i = 0; i < strlen(argv[1]); i++) {
        switch (argv[1][i]) {
        case FILENAME:
            if (argc < FILENAME_INDEX) {
                error(1, EINVAL, "%s: Missing required archive name\n %s%s\n",
                      argv[0], argv[0], USAGESTR);
            }
            filename = argv[2];

            break;

        case PRINTMODE:
            if (modefunc != NULL) {
                goto more_than_one_mode_err;
            }
            modefunc = print_archive_contents;
            break;

        case CREATEMODE:
            if (modefunc != NULL) {
                goto more_than_one_mode_err;
            }
            modefunc = create_archive;
            reqsterms = true;
            break;

        case EXTRACTMODE:
            if (modefunc != NULL) {
                goto more_than_one_mode_err;
            }
            modefunc = extract_archive_contents;
            break;

        case VERBOSEOPT:
            opts.verbose = true;
            break;

        case STRICTOPT:
            opts.strict = true;
            break;

        default:
            error(1, EINVAL, "non acceptable arg\n");
        }
    }

    /* set searchterms regardless of mode */
    if (argc > SEARCH_TERMS_INDEX) {
        searchterms = &argv[SEARCH_TERMS_INDEX];
        numsearchterms = argc - SEARCH_TERMS_INDEX;
#ifdef DEBUG
        printf("argc: %d\nnumsearchterms: %d\n", argc, numsearchterms);
#endif
    }
    if (reqsterms && (searchterms == NULL || numsearchterms == 0)) {
        error(1, EINVAL, "%s: Cowardly refusing to create an empty archive\n%s",
              argv[0], USAGESTR);
    }

    /* execute the correct function based on mode */
    errno = 0;
    if (modefunc == NULL) {
        error(1, EINVAL,
              "%s: One of the 'ctx' errors is required.\n"
              "usage: %s %s",
              argv[0], argv[0], USAGESTR);
    }
    return modefunc(filename, opts, searchterms, numsearchterms);

more_than_one_mode_err:
    error(1, EINVAL,
          "%s: you may only choose one of the 'ctx' options.\n"
          "usage: %s %s",
          argv[0], argv[0], USAGESTR);
    /* this return will never be reached however make does not recognize error
     * as a valid end of function */
    return EINVAL;
}
