
#ifndef ARCHIVE_H
#define ARCHIVE_H
#include <stddef.h>
#include <stdint.h>

#include "bool.h"
#include "header.h"

#ifndef BLOCK_SIZE
#define BLOCK_SIZE 512
#endif /* BLOCK_SIZE */

#ifndef OCTAL_BASE
#define OCTAL_BASE 8
#endif /* OCTAL_BASE */

/* used for easily accessing options */
struct opts {
    bool verbose;
    bool strict;
};

unsigned long int next_highest_multiple(unsigned long int bytes,
                                        unsigned int factor);

int print_archive_contents(char *archive, struct opts opts, char **searchterms,
                           int numsearchterms);

int extract_archive_contents(char *archive, struct opts opts,
                             char **searchterms, int numsearchterms);

int create_archive(char *archive, struct opts opts, char **searchterms,
                   int numsearchterms);

int insert_octal(int n, char *where, size_t cap, bool strict);

uint32_t extract_octal(const char *where, size_t cap, bool strict);

uint32_t extract_special_int(const char *where, int len);

char *nextslash(char *path);

int ensure_trailing_slash(char *p, size_t plen);

int create_file(char pathbuf[NAME_SIZE + PREFIX_SIZE + 1], Header *h, int *fd,
                bool strict);
#endif /* ARCHIVE_H */
