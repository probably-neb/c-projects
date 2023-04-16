/* declares boolean enum for verbose true/false operations
 * and typedefs a new type bool for declaring integers
 * that will be used as booleans */
#ifndef BOOL_H
#define BOOL_H
/* for using ints as booleans (verbosely) */
enum boolenum { false, true };
typedef enum boolenum bool;
#endif /* BOOL_H */
