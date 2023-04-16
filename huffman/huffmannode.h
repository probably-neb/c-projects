#ifndef HUFFMANNODE_H
#define HUFFMANNODE_H
typedef struct HuffmanNode {
    unsigned char ch;
    unsigned int count;
    /* bool used when sorting to put newly
     * combined nodes before nodes with same frequency */
    unsigned int newcombinednode;
    struct HuffmanNode *left;
    struct HuffmanNode *right;
} HuffmanNode;
#endif /* HUFFMANNODE_H */
