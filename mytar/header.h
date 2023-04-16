/*
 * Defines a large amount of definitions and consts
 * related to the spec of the header
 * as well as an unpadded struct representing the header
 */

/* this ifndef wraps entire header */
#ifndef HEADER_H
#define HEADER_H

#include <stddef.h>
#include <stdint.h>
/* for mode defs (if they are defined) */
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <sys/stat.h>

#include "bool.h"

#ifndef HEADER_STRUCT
#define HEADER_STRUCT
/*
 * #define [Field Name]_OFFSET [Octet Offset]
 * #define [Field Name]_SIZE   [Length (in Octets)]
 */
/* I LOVE REGEXP! */
#define NAME_OFFSET 0
#define NAME_SIZE 100
#define MODE_OFFSET 100
#define MODE_SIZE 8
#define UID_OFFSET 108
#define UID_SIZE 8
#define GID_OFFSET 116
#define GID_SIZE 8
#define SIZE_OFFSET 124
#define SIZE_SIZE 12
#define MTIME_OFFSET 136
#define MTIME_SIZE 12
#define CHKSUM_OFFSET 148
#define CHKSUM_SIZE 8
#define TYPEFLAG_OFFSET 156
#define TYPEFLAG_SIZE 1
#define LINKNAME_OFFSET 157
#define LINKNAME_SIZE 100
#define MAGIC_OFFSET 257
#define MAGIC_SIZE 6
#define VERSION_OFFSET 263
#define VERSION_SIZE 2
#define UNAME_OFFSET 265
#define UNAME_SIZE 32
#define GNAME_OFFSET 297
#define GNAME_SIZE 32
#define DEVMAJOR_OFFSET 329
#define DEVMAJOR_SIZE 8
#define DEVMINOR_OFFSET 337
#define DEVMINOR_SIZE 8
#define PREFIX_OFFSET 345
#define PREFIX_SIZE 155
/* to make struct 512 bytes long */
#define PAD_SIZE 12
#define BLOCK_SIZE 512
#define BYTE_SIZE 8

typedef struct __attribute__((__packed__)) Header {
    char name[NAME_SIZE];
    char mode[MODE_SIZE];
    char uid[UID_SIZE];
    char gid[GID_SIZE];
    char size[SIZE_SIZE];
    char mtime[MTIME_SIZE];
    char chksum[CHKSUM_SIZE];
    char typeflag[TYPEFLAG_SIZE];
    char linkname[LINKNAME_SIZE];
    char magic[MAGIC_SIZE];
    char version[VERSION_SIZE];
    char uname[UNAME_SIZE];
    char gname[GNAME_SIZE];
    char devmajor[DEVMAJOR_SIZE];
    char devminor[DEVMINOR_SIZE];
    char prefix[PREFIX_SIZE];
    char pad[PAD_SIZE];
} Header;
#endif /* HEADER_STRUCT */

/* permissions */
#define PERMS_WIDTH 10
#define OWNGROUP_WIDTH 17
#define SIZE_WIDTH 8
#define MTIME_WIDTH 16
#define DEFAULT_PERMS_STR "----------"
#define PERM_DIRECTORY_FILETYPE 'd'
#define PERM_SYMLINK_FILETYPE 'l'
#define READ_PERM 'r'
#define WRITE_PERM 'w'
#define EXEC_PERM 'x'

#ifndef MODES
#define MODES
/* mode */
/* removed I_S prefix for compatability with prexisting definitions */
#define MODE_READ_INDEX 1
#define MODE_WRITE_INDEX 2
#define MODE_EXEC_INDEX 3
/* constants usually defined in sys/stat.h */
#ifndef S_ISUID
#define S_ISUID 04000
#endif
#ifndef S_ISGID
#define S_ISGID 02000
#endif
#ifndef S_ISVTX
#define S_ISVTX 01000
#endif
#ifndef S_IRUSR
#define S_IRUSR 00400
#endif
#ifndef S_IWUSR
#define S_IWUSR 00200
#endif
#ifndef S_IXUSR
#define S_IXUSR 00100
#endif
#ifndef S_IRGRP
#define S_IRGRP 00040
#endif
#ifndef S_IWGRP
#define S_IWGRP 00020
#endif
#ifndef S_IXGRP
#define S_IXGRP 00010
#endif
#ifndef S_IROTH
#define S_IROTH 00004
#endif
#ifndef S_IWOTH
#define S_IWOTH 00002
#endif
#ifndef S_IXOTH
#define S_IXOTH 00001
#endif
/* S_I just to keep with the naming convention */
#define S_IXALL (S_IXUSR | S_IXGRP | S_IXOTH)
#define S_IRWALL (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH)
/* used in ACCESS_PERMS_ARRAY as placeholder for filetype.
 * makes ACCESS_PERMS_ARRAY have correct width */
#define TYPEFLAG_PLACEHOLDER 0
/* array of user,group,other modes in order as they will be used when displaying
 */
#endif /* MODES */

#ifndef ACCESS_PERMS_ARRAY
#define ACCESS_PERMS_ARRAY                                                     \
    {                                                                          \
        TYPEFLAG_PLACEHOLDER, S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP,     \
            S_IXGRP, S_IROTH, S_IWOTH, S_IXOTH                                 \
    }
#endif /* ACCESS_PERMS_ARRAY */

#ifndef TYPEFLAGS
#define TYPEFLAGS
/* typeflag */
#define TYPEFLAG_REGULAR_FILE '0'
#define TYPEFLAG_REGULAR_FILE_ALT '\0'
#define TYPEFLAG_SYMBOLIC_LINK '2'
#define TYPEFLAG_DIRECTORY '5'
#endif /* TYPEFLAGS */

/* must remember to check this */

#ifndef USTAR
/* magic (ustar) */
#define USTAR "ustar"
#endif /* USTAR */

#ifndef VERSION_NUM
/* version */
/* null terminated when it shouldnt be */
/* usages must take this into account */
#define VERSION_NUM "00"
#endif /* VERSION_NUM */

/* 13 character long because that is the max octal width in header */
/* used in case of int to large to fit in field with bit 31 set */
#define UNARCHIVEABLEOCTAL "7777777777777"

/*
 * FUNCTIONS/MACROS
 */
/* returns whether ch is terminating byte because for this assignment it
 * can be ' ' (space) or '\0' (null terminator) */
#define isterm(ch) (((ch) == ' ' || (ch) == '\0') ? 1 : 0)

int read_header_block(const char *arkmmap, size_t offset, size_t mmapsize,
                      Header *headerbuf, bool strict);
uint32_t computechksum(Header *header);
int pathbegwith(char pathbuf[PREFIX_SIZE + NAME_SIZE], const char *needle);
int parse_perms(const Header *h, char permsbuf[], bool strict);
char *joinpath(const Header *h, char buf[PREFIX_SIZE + NAME_SIZE]);
char *splitpath(const char completepath[], size_t lenpath,
                char prefixbuf[PREFIX_SIZE], char namebuf[NAME_SIZE]);
int setup_common_header(Header *h, char *filepath, struct stat *st,
                        bool strict);
int print_header_info_verbose(Header *h, bool strict);
enum header_validity { invalid, valid, empty };
enum header_validity verify_header(Header *h, bool strict);

#endif /* HEADER_H */
