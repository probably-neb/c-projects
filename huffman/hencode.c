/*
 * HENCODE
 * steps for hencode are written in hencode function
 */
#include <arpa/inet.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
/* defines a huffman node */
#include "huffman.h"
#ifdef DEBUG
#include "printfuncs.h"
#endif
#include <string.h>

/* takes a file descriptor and reads the file byte by byte
 * recording the frequencies in an int[] of length CHARFREQTABLESIZE
 * the byte value corresponds to its index in the array
 * throws error if the read fails and promises to return a table
 * with non present bytes (characters) counts being 0 */
unsigned int *getCharFreqTableFromFile(int fd, int *hnodetablelen) {
    ssize_t actualbufsize = 0;
    unsigned char chunk[CHUNKSIZE];
    unsigned char index = 0;
    /* one slot for each character */
    unsigned int *charFreqTable =
        (unsigned int *)calloc(CHARFREQTABLESIZE, sizeof(unsigned int));
    int numchars = 0, i;

    while ((actualbufsize = read(fd, chunk, CHUNKSIZE)) != 0) {
        if (actualbufsize == -1) {
            return NULL;
        }
        for (i = 0; i < actualbufsize; ++i) {
            index = (unsigned char)chunk[i];
            if (charFreqTable[index] == 0) {
                numchars++;
            }
            charFreqTable[index]++;
        }
    }
    *hnodetablelen = numchars;
    return charFreqTable;
}

/*
 * prepareHeaderInfo sorts the hnode table alphabetically and
 * converts the frequencies of present characters
 * int the char freq table to their index in the hnode table
 * (post alphabetically sorting them)
 * the effect is that charFreqTable now holds the indexes of
 * a characters code in the code table
 * and the hnode in the hnode table. Hence the name
 */
int prepareHeaderInfo(unsigned int *charFreqTable, HuffmanNode **hnodetable,
                      const int hnodetablelen) {
    int i, pos = 0;
    if (hnodetable == NULL || charFreqTable == NULL || hnodetablelen == 0) {
        return 0;
    }
    sortHuffmanNodeTableAlphabetically(hnodetable, hnodetablelen);
    /* printHuffmanNodeTable(hnodetable); */
    for (i = 0; i < CHARFREQTABLESIZE; i++) {
        if (charFreqTable[i] != 0) {
            charFreqTable[i] = pos;
            pos++;
        } else
            charFreqTable[i] = -1;
    }
    return 1;
}

/*
 * recursive function to dfs htree finding leaf nodes
 * while keeping track of current path, updating encodingstable
 * based on index of leaf node character found with indextable
 * assumes htree is not NULL
 */
int findPathsToLeafNodes(HuffmanNode *htree, unsigned int *indextable,
                         char **encodingstable, char *currentpath,
                         int pathlen) {
    int tableindex, status = 1;
    /* bools for repeated use and clarity */
    int hasleftchild = (htree != NULL ? htree->left != NULL : -1);
    int hasrightchild = (htree != NULL ? htree->right != NULL : -1);
    int isleafnode = !(hasleftchild && hasrightchild);
    unsigned char ch;
    char *finalpath;
    /* null htree */
    if (hasleftchild == -1 || hasrightchild == -1) {
        return 0;
    }
    if (currentpath == NULL) {
        /* max depth is 255 with 256 chars + 1 for null terminator */
        currentpath = (char *)malloc(CHARFREQTABLESIZE);
        if (currentpath == NULL) {
            return 0;
        }
        currentpath[0] = '\0';
    }
    if (isleafnode) {
        ch = htree->ch;
        tableindex = indextable[ch];
        /* pathlen + 1 is safe because we check if pathlen+1 < CHARFREQTABLESIZE
         */
        if ((finalpath = (char *)malloc(pathlen + 1)) == NULL) {
            return 0;
        }
        if (strcpy(finalpath, currentpath) == NULL) {
            status = 0;
        }
        encodingstable[tableindex] = finalpath;
    } else {
        /* increment pathlen before writing 0 or 1
         * to reinforce pathlen only modified once */
        if ((++pathlen) < CHARFREQTABLESIZE) {
            if (hasleftchild) {
                /* appends 0 to new path */
                currentpath[pathlen - 1] = '0';
                currentpath[pathlen] = '\0';
                status =
                    findPathsToLeafNodes(htree->left, indextable,
                                         encodingstable, currentpath, pathlen);
                if (!status)
                    return status;
            }
            if (hasrightchild) {
                currentpath[pathlen - 1] = '1';
                currentpath[pathlen] = '\0';
                status =
                    findPathsToLeafNodes(htree->right, indextable,
                                         encodingstable, currentpath, pathlen);
                if (!status) {
                    return 0;
                }
            }
        } else {
            fprintf(stderr, "Maximum depth (255) of tree exceeded.\n"
                            "Failure to correctly construct tree is likely.\n");
            status = 0;
        }
    }
    return status;
}

/*
 * assumes the hnodetable is in the order the resulting char
 * table should be in, and that hnodetable is null terminated
 */
char **generateCharacterEncodings(HuffmanNode **hnodetable,
                                  const int hnodetablelen, HuffmanNode *htree,
                                  unsigned int *indextable) {
    char **encodingstable =
        (char **)malloc((hnodetablelen + 1) * sizeof(char *));
    int status =
        findPathsToLeafNodes(htree, indextable, encodingstable, NULL, 0);
    if (!status) {
        return NULL;
    }
    return encodingstable;
}

/*
 * assumes the info it is passed is up to date
 * (meaning it has been through prepareHeaderInfo())
 */
int encodeHeaderToFile(HuffmanNode **hnodetable, const int hnodetablelen,
                       unsigned int *indextable, int outfd) {
    /* specifying each num bytes for verbosity */
    int bytesin_uniq_charcount = 1;
    int bytes_char = 1;
    int bytesin_charcount = 4;
    /* (hnodetablelen =  uniq chars) */
    int numchars = hnodetablelen;

    /* numchars - 1 | [  c[n]    count c[n] ] * numchars */
    /*     uint8_t       | [  char     uint32_t  ] */
    int bytesin_header =
        bytesin_uniq_charcount + ((bytes_char + bytesin_charcount) * numchars);
    char *headerbytes = (char *)malloc((size_t)bytesin_header);
    int index = 0, i;
    HuffmanNode *cur;
    uint32_t charcount;

    headerbytes[index++] = (uint8_t)(numchars - 1);
    for (i = 0; i < hnodetablelen; i++) {
        cur = hnodetable[i];
        headerbytes[index++] = cur->ch;
        charcount = (uint32_t)(cur->count);
        /* printf("h: %u\n", charcount); */
        charcount = htonl(charcount);
        /* printf("n: %u\n", charcount); */
        /* headerbytes[index++] = (charcount >> 24) & 0xFF; /1* b1 *1/ */
        /* headerbytes[index++] = (charcount >> 16) & 0xFF; /1* b2 *1/ */
        /* headerbytes[index++] = (charcount >> 8) & 0xFF;  /1* b3 *1/ */
        /* headerbytes[index++] = charcount & 0xFF;         /1* b4 *1/ */
        memcpy(&(headerbytes[index]), &charcount, bytesin_charcount);
        if (headerbytes == NULL) {
            return 0;
        }
        index += 4;
    }
    if (write(outfd, headerbytes, bytesin_header) == -1) {
        return 0;
    }
    free(headerbytes);
    return 1;
}

int encodeMessageToFile(HuffmanNode **hnodetable, const int hnodetablelen,
                        char **encodingstable, HuffmanNode *htree,
                        unsigned int *indextable, int infd, int outfd) {
    char codeschunk[CHUNKSIZE];
    char messagechunk[CHUNKSIZE];
    unsigned char ch, codechar;
    char *code;
    int index = 0, bitindex = 7, readsize = 0, i, j = 0, flag = 1;
    /* 1 or 0 chars write nothing.
     * all info for one char is included in header */
    if (hnodetablelen == 0 || hnodetablelen == 1) {
        goto write;
    }
    /* sanity seek to beginning of file */
    if (lseek(infd, 0, SEEK_SET) == -1) {
        return 0;
    }
    /* set first code char to zero */
    codeschunk[0] = 0;
    /* while there is still message left to decode  */
    while ((readsize = read(infd, messagechunk, CHUNKSIZE)) != 0) {
        if (readsize == -1) {
            return 0;
        }

        /* for character in message */
        for (i = 0; i < readsize; i++) {
            ch = messagechunk[i];
            code = encodingstable[indextable[ch]];

            /* encode into bit in messagechunk for character in encoding */
            for (j = 0; (codechar = code[j]) != '\0'; j++) {
                /* encode to current bit */
                if (codechar == '1') {
                    codeschunk[index] |= (flag << bitindex);
                }
                if (bitindex == 0) {
                    index++;
                    if (index == CHUNKSIZE) {
                        /* don't "goto write;" here because
                         * need to keep reading */
                        if (write(outfd, codeschunk, index) == -1) {
                            return 0;
                        }
                        index = 0;
                    }

                    codeschunk[index] = '\0';
                    bitindex = 7;
                } else {
                    bitindex--;
                }
            }
        }
    }
    /*
     * this checks that we are not on a new, unwritten too byte
     * because if the msb has been written too bitindex != 7
     * will automatically be padded since chars are set to 0
     * before being written too
     */
    if (bitindex != 7)
        index++;
write:
    /* this will not write anything if index is 0 */
    if (write(outfd, codeschunk, index) == -1) {
        return 0;
    }
    return 1;
}

int hencode(int infd, int outfd) {
    unsigned int *charFreqTable = NULL;
    /* charFreqTable will become index table. simple name change for clarity */
    unsigned int *indextable = NULL;
    char **encodingstable = NULL;
    HuffmanNode **hnodetable = NULL;
    int hnodetablelen = 0, status = 0;
    HuffmanNode *htree = NULL;

    /*
     * STEP 1:
     * generate character frequency table
     */
    charFreqTable = getCharFreqTableFromFile(infd, &hnodetablelen);
    if (charFreqTable == NULL) {
        goto err;
    }
    if (hnodetablelen == 0) {
        goto encode;
    }
#ifdef DEBUG
    printTable(charFreqTable, 256);
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
    if (htree == NULL) {
        goto err;
    }
    /* one char. Requires htree to be created to
     * retrieve char and count */
    if (hnodetablelen == 1) {
        /* do not have to worry about encoding message afterwards
         * as it will just write nothing because hnodetablelen is 1 */
        goto encodeheader;
    }
#ifdef DEBUG
    printHuffmanTree(htree);
#endif

    /*
     * STEP 4:
     * this step is presetup for generating codes
     * pass freq table and hnode table to prepareHeaderInfo
     * prepareHeaderInfo sorts the hnode table alphabetically and
     * converts the frequencies of present characters
     * int the char freq table to their index in the hnode table
     * (post alphabetically sorting them)
     */
    status = prepareHeaderInfo(charFreqTable, hnodetable, hnodetablelen);
    if (!status) {
        goto err;
    }
#ifdef DEBUG
    printTable(charFreqTable, CHARFREQTABLESIZE);
#endif
    /* renaming for clarity */
    indextable = charFreqTable;
    /*
     * STEP 5:
     * now use all previously gathered information to generate
     * encodings table
     * encodings table stores paths as character representations
     * of 1 and 0 to later be parsed into respective bits
     * allows for easier debugging and completely avoiding
     * the how to figure out the end of a code in bits question
     */
    encodingstable = generateCharacterEncodings(hnodetable, hnodetablelen,
                                                htree, indextable);

#ifdef DEBUG
    /* prints in format required by lab03 */
    printEncodingsTableHex(encodingstable, indextable);
#endif

    /*
     * WE ARE NOW READY TO ENCODE FILE
     * we have:
     * htree: binary tree of huffman nodes properly ordered and combined
     *        so leaf nodes are characters from original message
     * hnode table: alphabetically sorted list of HuffmanNodes for
     *              each character in original message.
     * encoding table: path to get to each char in hnode table in htree
     * indextable: using char as index will yield the index
     *             of the node in hnode table (for count) and
     *             encoding in encoding table for the char
     */

    /*
     * ENCODING STEP 1: write header
     */
encodeheader:
    status = encodeHeaderToFile(hnodetable, hnodetablelen, indextable, outfd);

encode:
    encodeMessageToFile(hnodetable, hnodetablelen, encodingstable, htree,
                        indextable, infd, outfd);

#ifdef DEBUG
    printEncodedFilePretty(outfd);
#endif

    /* error if failed */
    if (close(infd) == -1) {
        goto err;
    }
    /* continue if failed */
    if (close(outfd) != -1) {
        return 1;
    }
err:
    perror("hencode");
    return 0;
}

int main(int argc, char *argv[]) {
    char *infile = NULL;
    char *outfile = NULL;
    int infd, outfd;
    switch (argc) {
    case 2:
        infile = argv[1];
    case 3:
        infile = argv[1];
        outfile = argv[2];
    }
    if ((infd = openInFile('e', infile)) == -1) {
        return -1;
    }
    if ((outfd = openOutFile('e', outfile)) == -1) {
        return -1;
    }
    /* hencode uses normal true/false so return inverse */
    return !hencode(infd, outfd);
}
