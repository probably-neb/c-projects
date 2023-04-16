#include "printfuncs.h"
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>

FILE *fdopen(int fd, const char *mode);

int main(int argc, char *argv[]) {
    int fd = openInFile('e', argv[1]);
    /* printEncodedFilePretty(fd); */
    getTreeFromCodes(fdopen(fd, "r"));
    return 0;
}
