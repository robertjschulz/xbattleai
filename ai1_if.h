#ifndef INCLUDED_AI1_IF_H
#define INCLUDED_AI1_IF_H

/* ai1_if.h */
/* Interface to AI algorithm 1 */

#include "extern.h" /* for cell_type */

/** Exported prototypes **/
extern int cell_init_algo1(cell_type *cell);
extern void cell_free_algo1(cell_type *cell);
extern int init_algo1(int side,float skill);
extern void free_algo1(int side);
extern void ai_algo1(int side);

#endif
