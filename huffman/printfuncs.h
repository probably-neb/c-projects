#include "huffman.h"
#include <stdio.h>
#ifndef PRINTFUNCS_H
#define PRINTFUNCS_H
void printHuffmanTree(HuffmanNode *hnode);
void printHuffmanNode(HuffmanNode *hnode, int depth);
void printHuffmanNodeTable(HuffmanNode **table);
void printEncodingsTableHex(char **table, unsigned int *indextable);
void printEncodingsTable(char **table, unsigned int *indextable);
void printTable(unsigned int *table, int tablelen);
char *printCh(char ch);
char *printChHex(char ch);
void printEncodedFilePretty(int filedes);
int asprintf(char **ptr, const char *template, ...);
HuffmanNode *getTreeFromCodes(FILE *instream);
#endif /* PRINTFUNCS_H */
