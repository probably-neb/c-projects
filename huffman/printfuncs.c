#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <error.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "huffman.h"

/* ------------------------------ */
/*                                */
/*      Printing  Functions       */
/*                                */
/* ------------------------------ */

int asprintf(char **ptr, const char *template, ...);
char *printChHex(char ch);

/* helper function for printing chars (specifically non-graphical ones) */
char *printCh(unsigned char ch) {
    char *def;
    int asprintfret;
    switch (ch) {
    case '\n':
        return "\\n";
    case '\t':
        return "\\t";
    case '\r':
        return "\\r";
    case ' ':
        return "' '";
    default:
        if (!isgraph(ch))
            return printChHex(ch);
        asprintfret = asprintf(&def, "%c", ch);
        if (asprintfret < 0) {
            error(0, errno, "Failed to format character to string");
        }
        return def;
    }
}

char *printChHex(char ch) {
    char *hex;
    asprintf(&hex, "0x%02hx", (unsigned char)ch);
    return hex;
}

/* helper function for debugging */
void printTable(int *table, int tablelen) {
    int i;
    if (table == NULL || tablelen == 0) {
        fprintf(stderr, "Cannot print null or empty table\n");
        return;
    }
    printf("[ ");
    for (i = 0; i < (tablelen - 1); i++) {
        printf("(%s)*%d, ", printChHex(i), table[i]);
    }
    printf("%d ]\n", table[i]);
}

/* helper function for debugging */
void printEncodingsTable(char **table, unsigned int *indextable) {
    int i;
    printf("[ ");
    for (i = 0; i < 256; i++) {
        if (indextable[i] != -1)
            printf("(%s %s), ", printCh((char)i), table[indextable[i]]);
    }
    printf("nul ]\n");
}

/* helper function for debugging */
void printEncodingsTableHex(char **table, unsigned int *indextable) {
    int i;
    for (i = 0; i < CHARFREQTABLESIZE; i++) {
        if (indextable[i] != -1)
            printf("%s: %s\n", printChHex((char)i), table[indextable[i]]);
    }
}

void printHuffmanNodeTable(HuffmanNode **table) {
    int i;
    printf("[ ");
    for (i = 0; table[i] != NULL; i++) {
        printf("(%s %d), ", printCh(table[i]->ch), table[i]->count);
    }
    printf("nul ]\n");
}

/* helper function for debugging */
void printHuffmanNode(HuffmanNode *hnode, int depth) {
    if (hnode == NULL)
        return;
    printHuffmanNode(hnode->right, 8 + depth);
    printf("%*s%s:%d\n", depth, "", printCh(hnode->ch), hnode->count);
    printHuffmanNode(hnode->left, 8 + depth);
}

ssize_t getline(char **lineptr, size_t *n, FILE *stream);

/* this function was created to parse the correct output from demos
 * into a HuffmanNode tree so that it could be printed and compared */

HuffmanNode *getTreeFromCodes(FILE *instream) {
    char *line = NULL;
    char *delim = ": \n";
    char *hexstring, *path;
    size_t len = 0;
    int ch, i;
    HuffmanNode *htree = constructHuffmanNode('$', 0, NULL, NULL);
    HuffmanNode *cur;
    while (getline(&line, &len, instream) != -1) {
        hexstring = strtok(line, delim);
        path = strtok(NULL, delim);
        ch = (int)strtol(hexstring, NULL, 0);
        cur = htree;
        for (i = 0; path[i] != '\0'; i++) {
            switch (path[i]) {
            case '0':
                if (cur->left == NULL)
                    cur->left = constructHuffmanNode(0, 0, NULL, NULL);
                cur = cur->left;
                cur->count++;
                break;
            case '1':
                if (cur->right == NULL)
                    cur->right = constructHuffmanNode(0, 0, NULL, NULL);
                cur = cur->right;
                cur->count++;
                break;
            }
        }
        cur->ch = ch;
    }
    return htree;
}

/* helper function for debugging */
void printHuffmanTree(HuffmanNode *hnode) { printHuffmanNode(hnode, 0); }

void printHeaderFromFile(int filedes) {
    uint8_t numchars;
    unsigned char ch[1];
    uint32_t count;
    int i;
    lseek(filedes, 0, SEEK_SET);
    read(filedes, &numchars, 1);
    printf("%u", numchars);
    for (i = 0; i <= numchars; i++) {
        read(filedes, ch, 1);
        read(filedes, &count, 4);
        count = ntohl(count);
        printf(" | %s:%u", printCh(*ch), count);
    }
}

void printEncodedFilePretty(int filedes) {
    char codes[CHUNKSIZE];
    int i, readsize;
    printHeaderFromFile(filedes);
    printf("\n");
    while ((readsize = read(filedes, codes, CHUNKSIZE)) != 0 &&
           readsize != -1) {
        for (i = 0; i < readsize; i++) {
            printf(" %s ", printChHex(codes[i]));
        }
    }
    if (readsize)
        printf("readsize: %d\n", readsize);
}
