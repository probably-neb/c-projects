#include "huffmannode.h"
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const size_t sizeofHuffmanNodePtr = sizeof(HuffmanNode *);
const size_t sizeofHuffmanNode = sizeof(HuffmanNode);
const int CHARFREQTABLESIZE = 256; /* # of possible chars */
/* macro because used for non dynamically allocated arrays */
#define CHUNKSIZE 4000 /* ARBITRARY */
const int TRUE = 1;
const int FALSE = 0;
const char *hencodeusage = "hencode infile [ outfile ]";
const char *hdecodeusage = "hdecode [ ( infile | - ) [ outfile ] ]";

HuffmanNode *constructHuffmanNode(unsigned char ch, int count,
                                  HuffmanNode *left, HuffmanNode *right) {
    HuffmanNode *hnode = (HuffmanNode *)malloc(sizeofHuffmanNode);
    hnode->ch = ch;
    hnode->count = count;
    /* default to false. Relies on other functions to set this as needed */
    hnode->newcombinednode = FALSE;
    hnode->left = left;
    hnode->right = right;
    return hnode;
}

HuffmanNode **parseCharFreqTable(unsigned int charFreqTable[],
                                 int hnodetablelen) {
    int i, index = 0;
    unsigned char ch;
    HuffmanNode *temphnode = NULL;
    HuffmanNode **hnodetable = NULL;
    /* count plus one to add null terminator */
    hnodetable =
        (HuffmanNode **)malloc(sizeof(HuffmanNode *) * (hnodetablelen + 1));
    if (hnodetable == NULL) {
        return NULL;
    }

    for (i = 0; i < CHARFREQTABLESIZE; i++) {
        if (charFreqTable[i] != 0) {
            ch = (unsigned char)i;
            temphnode = constructHuffmanNode(ch, charFreqTable[i], NULL, NULL);
            if (temphnode == NULL) {
                return NULL;
            }
            hnodetable[index++] = temphnode;
        }
    }
    hnodetable[index] = NULL;
    return hnodetable;
}
int comphufchars(HuffmanNode *h1, HuffmanNode *h2) {
    return ((unsigned char)h1->ch) - ((unsigned char)h2->ch);
}

int compareHuffmanNodesAlphabetically(const void *hufptrptr1,
                                      const void *hufptrptr2) {
    int diff;
    HuffmanNode **h1ptr = (HuffmanNode **)hufptrptr1;
    HuffmanNode **h2ptr = (HuffmanNode **)hufptrptr2;
    HuffmanNode *h1, *h2;
    if ((h1ptr == NULL || *h1ptr == NULL) ||
        (h2ptr == NULL || *h2ptr == NULL)) {
        /* error here because all ints are valid comparisons */
        error(1, EINVAL, "Cannot compare NULL HuffmanNodes");
    }
    h1 = *h1ptr;
    h2 = *h2ptr;

    diff = comphufchars(h1, h2);
    return diff;
}

/* returns the less likely character or lexicographically
 * lower character in case of a tie in count */
/* assumes its parameters are pointers to huffman node pointers */
int compareHuffmanNodes(const void *hufptrptr1, const void *hufptrptr2) {
    int diff;
    HuffmanNode **h1ptr = (HuffmanNode **)hufptrptr1;
    HuffmanNode **h2ptr = (HuffmanNode **)hufptrptr2;
    HuffmanNode *h1, *h2;
    if ((h1ptr == NULL || *h1ptr == NULL) ||
        (h2ptr == NULL || *h2ptr == NULL)) {
        /* error here because all ints are valid comparisons */
        error(1, EINVAL, "Cannot compare NULL HuffmanNodes");
    }
    h1 = *h1ptr;
    h2 = *h2ptr;

    diff = (h1->count - h2->count);
    if (diff == 0) {
        /* if one of the nodes is new then we should return it */
        if (h1->newcombinednode == TRUE) {
            diff = -1;
        } else if (h2->newcombinednode == TRUE) {
            diff = 1;
        }
        /* else compare by character alphabetically */
        else {
            diff = comphufchars(h1, h2);
        }
    }
    return diff;
}

/* assumes it's parameter is null terminated */
void sortHuffmanNodes(HuffmanNode **hnodetable, const int hnodetablelen,
                      int (*comparator)(const void *, const void *)) {
    qsort(hnodetable, hnodetablelen, sizeofHuffmanNodePtr, comparator);
}

/* sorts alphabetically by node character */
void sortHuffmanNodeTableAlphabetically(HuffmanNode **hnodetable,
                                        const int hnodetablelen) {
    sortHuffmanNodes(hnodetable, hnodetablelen,
                     compareHuffmanNodesAlphabetically);
}

/* default sort method. min sort comparing nodes how they are compared
 * when creating a the huffman tree:
 * primarly by frequency falling back to alphabetically by character */
void sortHuffmanNodeTable(HuffmanNode **hnodetable, const int hnodetablelen) {
    sortHuffmanNodes(hnodetable, hnodetablelen, compareHuffmanNodes);
}

/* assumes left parameter and right parameter are which of the new
 * nodes children each param should be
 * i.e. does no logic deciding which child should be which
 * just makes new node and sets childnodes "new" variable to false */
HuffmanNode *combineHuffmanNodes(HuffmanNode *left, HuffmanNode *right) {
    /* HuffmanNode * new; */
    int combinedcount = (left->count) + (right->count);
    HuffmanNode *combo =
        constructHuffmanNode(left->ch, combinedcount, left, right);
    left->newcombinednode = FALSE;
    right->newcombinednode = FALSE;
    combo->newcombinednode = TRUE;
    return combo;
}

/* assumes the hnodetable it recieves is sorted and null terminated */
/* DOES NOT MODIFY ORIGINAL TABLE */
/* DOES NOT GENERATE ENCODING */
HuffmanNode *createHuffmanTreeFromNodeList(HuffmanNode **hnodetable,
                                           int hnodetablelen) {
    /* copy to preserve hnodetable */
    HuffmanNode **queue =
        (HuffmanNode **)malloc((hnodetablelen + 1) * sizeofHuffmanNodePtr);
    void *copyresult =
        memcpy(queue, hnodetable, (hnodetablelen + 1) * sizeofHuffmanNodePtr);
    HuffmanNode **head;

    /* used for verbosity when peeking */
    HuffmanNode *left = NULL;
    HuffmanNode *right = NULL;
    HuffmanNode *combo = NULL;

    if (copyresult == NULL) {
        return NULL;
    }
    /* following while loops through this series of steps */
    /* peek top 2 */
    /* combine top 2*/
    /* set combination and queue to spot of second pop */
    /* sort queue (making it a priority queue) */
    /* repeat until second peek is NULL (end of queue) */
    sortHuffmanNodeTable(queue, hnodetablelen);
    head = queue;
    while ((left = *head) != NULL && (right = *(head + 1)) != NULL) {
        combo = combineHuffmanNodes(left, right);
        head += 1;
        hnodetablelen--;
        *head = combo;
        sortHuffmanNodeTable(head, hnodetablelen);
    }

    /* will return null if *head was ever null */
    return *head;
}

/* gcc not recognizing filno as function at compile time? */
int fileno(FILE *stream);

/*
 * both of the open*File functions take a single char ['e' | 'd']
 * corresponding to whether to open with hencode or hdecode logic
 * respectively
 */
int openOutFile(char encodeordecode, char *path) {
    int outfd = -1;
    if (encodeordecode != 'e' && encodeordecode != 'd') {
        error(0, EINVAL, "invalid flag for openInFile\n\
        Usage:\n\
        %s\n%s\n",
              hencodeusage, hdecodeusage);
        return -1;
    }
    if (path == NULL)
        outfd = fileno(stdout);
    else {
        outfd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
    }
    if (outfd == -1) {
        perror("Failed to open outfile");
    }
    return outfd;
}

int openInFile(char encodeordecode, char *path) {
    int infd = -1;
    if (encodeordecode != 'e' && encodeordecode != 'd') {
        error(0, EINVAL, "invalid flag for openInFile\n\
        Usage:\n\
        %s\n%s\n",
              hencodeusage, hdecodeusage);
        return -1;
    }
    if (path == NULL || strcmp(path, "-") == 0) {
        switch (encodeordecode) {
        case 'e':
            errno = EINVAL;
            error(0, EINVAL,
                  "hencode: hencode requires an infile.\nUsage: %s\n",
                  hencodeusage);
            return -1;
        case 'd':
            infd = fileno(stdin);
        }
    } else {
        infd = open(path, O_RDONLY);
    }
    if (infd == -1) {
        perror("Failed to open infile");
    }
    return infd;
}
