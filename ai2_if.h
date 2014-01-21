#ifndef INCLUDED_AI2_IF_H
#define INCLUDED_AI2_IF_H

/* ai2_if.h - Pierre Bureau - 05/2OO1 */
/* Interface include file for ai2. */

#include "extern.h" /* for cell_type */

/** Exported prototypes **/
extern int cell_init_algo2(cell_type *cell);
extern void cell_free_algo2(cell_type *cell);
extern int init_algo2(int side,float skill);
extern void free_algo2(int side);
extern void ai_algo2(int side);

#endif /* INCLUDED_AI2_IF_H */
