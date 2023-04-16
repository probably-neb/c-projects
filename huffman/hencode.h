#include "huffman.h"

#ifndef HENCODE_H
#define HENCODE_H
void hencode(int infd, int outfd);

char **generateCharacterEncodings(HuffmanNode **hnodetable, HuffmanNode *htree,
                                  int *indextable);

void findPathsToLeafNodes(HuffmanNode *htree, int *indextable,
                          char **encodingstable, char *currentpath);

void prepareHeaderInfo(int *charFreqTable, HuffmanNode **hnodetable);

HuffmanNode *createHuffmanTreeFromNodeList(HuffmanNode **hnodetable,
                                           int hnodetablelen);

HuffmanNode *combineHuffmanNodes(HuffmanNode *left, HuffmanNode *right);

int compareHuffmanNodes(const void *hufptrptr1, const void *hufptrptr2);

int compareHuffmanNodesAlphabetically(const void *hufptrptr1,
                                      const void *hufptrptr2);

int *getCharFreqTableFromFile(int fd);
#endif /* HENCODE_H */
