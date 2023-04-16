#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*these are made up*/
/*initial capacity of our char array to store the line*/
#define INITIAL_LINE_CHARACTER_CAPACITY 100
/*how much to extend the char array by when we have hit capacity*/
#define LINE_CAPACITY_INCREASE 20

/*the size of the currentLine*/
/*used by main to know the size of the string to print out*/
/*this allows for null bytes in the text to be outputted*/
static int size = 0;

char *read_long_line(FILE *file) {
    const int initalCapacity = INITIAL_LINE_CHARACTER_CAPACITY;
    int capacity = initalCapacity;
    char *line = (char *)malloc(initalCapacity * sizeof(char));
    int ch;
    size = 0;

    /*return null if malloc failed to initialize line*/
    if (line == NULL)
        return NULL;

    /*assign ch to getc from file, stop when line ends either on EOF or \n */
    while ((ch = fgetc(file)) != '\n') {
        /*if eof then jump to end of while loop and append null byte then
         * return*/
        /*if (ch == EOF) {*/
        if (feof(file)) {
            /*case: eof after newline*/
            if (size == 0) {
                return NULL;
            }
            /* case: eof before newline
             * we could have text before EOF that we dont want to lose
             * so: break, append null byte, and return*/
            else {
                break;
            }
        }

        /*add ch to line and then increment size*/
        line[size++] = ch;

        if (size == capacity) {
            capacity += LINE_CAPACITY_INCREASE;
            line = (char *)realloc(line, capacity * sizeof(char));
            /*check if realloc returned null*/
            if (!line) {
                return NULL;
            }
            /* could return partial line if this was checked differently
             * but I think it's better to fail rather than
             * to return line as if it was read correctly */
        }
    }
    /*safe because size will be < capacity*/
    /*because the while loop stops when ch == \n the
     * \n is not used in the comparison as per the requirements*/
    line[size++] = '\n';
    /*be nice and reallocate line[] to its actual size*/
    /*no need to check if line is NULL because we'd return NULL anyway*/
    return (char *)realloc(line, size * sizeof(char));
}

/*used to compare size of currentLine to previousLine*/
int max(int a, int b) {
    /*could make comparison more efficient by
     * returning zero here and checking it in main
     * but it wouldn't be pretty :( */
    /* if (a == b) return 0; */
    if (a > b)
        return a;
    /*else*/
    return b;
}

int main(int argc, char *argv[]) {
    char *currentLine = NULL;
    char *previousLine = NULL;
    int size_previous;

    /*while successfully reading the next line*/
    while ((currentLine = read_long_line(stdin)) != NULL) {
        /*don't want to run strcmp on previousLine if it's null
         * also, && strcmp(cl, pl) will return 0 and therefore FALSE
         * if they are the same, true if they are different
         */
        if (previousLine == NULL /*<- true on first line*/
            /*second line (prev not null) and prev != current*/
            ||
            (previousLine != NULL
             /*use memcmp because strings arent null terminated*/
             && memcmp(currentLine, previousLine, max(size, size_previous)))) {

            /*use of fwrite allows us to use size and print out null bytes*/
            fwrite(currentLine, size, sizeof(char), stdout);

            /*currentLine is now the previousline
             * we should also forget (free) about (the old) previousLine*/
            free(previousLine);
            previousLine = currentLine;
            size_previous = size;
        }
        /*do nothing here because line is duplicate of the last*/
    }
    return 0;
}
