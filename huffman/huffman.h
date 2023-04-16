#include "huffmannode.h"
#include <stdlib.h>

#ifndef HUFFMAN_H
#define HUFFMAN_H
extern const size_t sizeofHuffmanNodePtr;
extern const size_t sizeofHuffmanNode;
extern const int CHARFREQTABLESIZE;

#ifndef CHUNKSIZE
#define CHUNKSIZE 4000
#endif
extern const int TRUE;
extern const int FALSE;

int comphufchars(HuffmanNode *h1, HuffmanNode *h2);

HuffmanNode *constructHuffmanNode(unsigned char ch, int count,
                                  HuffmanNode *left, HuffmanNode *right);

HuffmanNode **parseCharFreqTable(unsigned int *charFreqTable,
                                 int hnodetablelen);

HuffmanNode *createHuffmanTreeFromNodeList(HuffmanNode **hnodetable,
                                           int hnodetablelen);

int compareHuffmanNodesAlphabetically(const void *hufptrptr1,
                                      const void *hufptrptr2);

/* returns the less likely character or lexicographically
 * lower character in case of a tie in count */
/* assumes its parameters are pointers to huffman node pointers
 * (i.e. type HuffmanNode **) */
int compareHuffmanNodes(const void *hufptrptr1, const void *hufptrptr2);

/* assumes it's parameter is null terminated */
void sortHuffmanNodes(HuffmanNode **hnodetable, const int hnodetablelen,
                      int (*comparator)(const void *, const void *));

/* sorts alphabetically by node character
 * calls sortHuffmanNodes with the alphabetical node comparator */
void sortHuffmanNodeTableAlphabetically(HuffmanNode **hnodetable,
                                        const int hnodetablelen);

HuffmanNode *combineHuffmanNodes(HuffmanNode *left, HuffmanNode *right);

/* assumes the hnodetable it recieves is sorted and null terminated */
/* DOES NOT MODIFY ORIGINAL TABLE */
/* DOES NOT GENERATE ENCODING */
HuffmanNode *createHuffmanTreeFromNodeList(HuffmanNode **hnodetable,
                                           int hnodetablelen);

/* default sort method. min sort comparing nodes how they are compared
 * when creating a the huffman tree:
 * primarly by frequency falling back to alphabetically by character
 * calls sortHuffmanNodes with the default node comparator function */
void sortHuffmanNodeTable(HuffmanNode **hnodetable, const int hnodetablelen);

int openInFile(char encodeordecode, char *path);
int openOutFile(char encodeordecode, char *path);
#endif /* HUFFMAN_H */
