#include "huffman.h"
#include <arpa/inet.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef DEBUG
#include "printfuncs.h"
#endif /* PRINTFUNCS_H */

unsigned int *decodeHeaderToFreqTable(int filedes, int *hnodetablelen) {
    uint8_t numchars = 0;
    unsigned char ch[1] = {'\0'};
    uint32_t count = 0;
    int i;
    unsigned int *charFreqTable =
        (unsigned int *)calloc(CHARFREQTABLESIZE, sizeof(unsigned int));
    if (charFreqTable == NULL) {
        return NULL;
    }
    if (read(filedes, &numchars, 1) < 0) {
        return NULL;
    }
    *hnodetablelen = numchars + 1;
    /* printf("count: %d\n", numchars); */
    for (i = 0; i <= numchars; i++) {
        if (read(filedes, ch, 1) < 0) {
            return NULL;
        }
        if (read(filedes, &count, 4) < 0) {
            return NULL;
        }
        count = ntohl(count);
        /* printf("%*s : %u\n", 4, printCh(*ch), ntohl(count)); */
        charFreqTable[*ch] = count;
    }
    return charFreqTable;
}

/*
 * assumes infiles current index is the end of the header and the start
 * of the encoded message and that the htree is not a leaf node
 * writes message to output
 */
int decodeInfileMessageToOutfile(int infd, int outfd, HuffmanNode *htree) {
    char encodingschunk[CHUNKSIZE];
    /* root htree node will always contain total count of chars
     * (or be null)*/
    int totalnumchars = (htree != NULL ? htree->count : 0);
    char *messagechunk = (char *)malloc(sizeof(char) * totalnumchars);
    HuffmanNode *cur = htree;
    int i, iscurrentbit1, isleafnode, bitindex = 7, index = 0, status = -1,
                                      flag = 1;
    /* Edge case: empty file */
    if (totalnumchars == 0) {
        goto write;
    }

    isleafnode = (cur->left == NULL && cur->right == NULL);
    /* Edge case: single character */
    if (isleafnode) {
        memset(messagechunk, htree->ch, totalnumchars);
        goto write;
    }

    while ((status = read(infd, encodingschunk, CHUNKSIZE)) != 0) {
        if (status < 0) {
            return 0;
        }
        for (i = 0; i < status; i++) {
            /* ternary operator ensures bitindex is not decremented
             * if previous node was leaf node and we did not step through code
             */
            for (bitindex = 7; bitindex >= 0; bitindex--) {
                if (!isleafnode) {
                    /* will be true if bit at bitindex is 1 */
                    iscurrentbit1 = ((encodingschunk[i] >> bitindex) & flag);
                    /* step in correct direction based on if current bit is one
                     */
                    cur = (iscurrentbit1 ? cur->right : cur->left);
                }
                isleafnode = (cur->left == NULL && cur->right == NULL);

                /* (assuming file is properly encoded) */
                if (isleafnode) {
                    messagechunk[index++] = cur->ch;
                    if (index == totalnumchars) {
                        goto write;
                    }
                    cur = htree;
                    /* update isleafnode for subsequent loop */
                    isleafnode = FALSE;
                }
            }
        }
        flag = 1;
    }
write:
    status = write(outfd, messagechunk, totalnumchars);
    if (status == -1) {
        status = 0;
    } else {
        status = 1;
    }
    return status;
}

int fileno(FILE *stream);

int hdecode(int infd, int outfd) {
    unsigned int *charFreqTable = NULL;
    HuffmanNode **hnodetable = NULL;
    int hnodetablelen = 0, status;
    HuffmanNode *htree = NULL;
    /*
     * STEP 1:
     * parse charFreqTable from inputfile header
     */
    charFreqTable = decodeHeaderToFreqTable(infd, &hnodetablelen);
    if (charFreqTable == NULL) {
        goto err;
    }
    if (hnodetablelen == 0) {
        /* empty file or 1 character. Don't need any functions before
         * write func*/
        goto write;
    }
#ifdef DEBUG
    printTable(charFreqTable, CHARFREQTABLESIZE);
#endif
    /*
     * STEP 2:
     * using freq table create table of huffnman nodes
     * for only characters that appear in the file
     * (count > 0)
     */
    hnodetable = parseCharFreqTable(charFreqTable, hnodetablelen);
    if (hnodetable == NULL) {
        goto err;
    }
#ifdef DEBUG
    printHuffmanNodeTable(hnodetable);
#endif

    /*
     * STEP 3:
     * generate tree from hnode table
     * notably this does not modify the hnode table
     */
    htree = createHuffmanTreeFromNodeList(hnodetable, hnodetablelen);
    if (hnodetablelen == 1) {
        goto write;
    }
    /* if hnodetablelen != 1 and htree is NULL  */
    if (htree == NULL) {
        goto err;
    }
    /* one char. Requires htree to be created to
     * retrieve char */
#ifdef DEBUG
    printHuffmanTree(htree);
#endif

    /*
     * STEP 4:
     * using h tree and encodings from infile
     * writing characters at leafnodes to outfile
     */
write:
    status = decodeInfileMessageToOutfile(infd, outfd, htree);
    if (!status) {
        goto err;
    }
    if (close(infd) == -1 || close(outfd) == -1) {
        goto err;
    }
    /* DONE :) */
    return 1;

err:
    perror("hdecode");
    return 0;
}

int main(int argc, char *argv[]) {
    char *infile, *outfile;
    int infd, outfd;
    switch (argc) {
    case 1:
        infile = NULL;
        outfile = NULL;
        break;
    case 2:
        infile = argv[1];
        outfile = NULL;
        break;
    case 3:
        infile = argv[1];
        outfile = argv[2];
        break;
    default:
        errno = E2BIG;
        perror(argv[0]);
        exit(errno);
    }
    if ((infd = openInFile('d', infile)) == -1 ||
        (outfd = openOutFile('d', outfile)) == -1) {
        return 0;
    }
    /* hdecode uses normal true/false so return inverse */
    return !hdecode(infd, outfd);
}
