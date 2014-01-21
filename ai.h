#ifndef INCLUDED_AI_H
#define INCLUDED_AI_H

/* ai.h */

#include "extern.h" /* for cell_type */

#include "ai1_if.h"
#include "ai2_if.h"

/** Number of valid AIs (the +1 is for the dummy AI) **/
#define AI_MAX_ALGO (2+1)


/** Generic AI tables **/


/** Per-cell init function table **/
extern int (*ai_cell_init_table[AI_MAX_ALGO])(cell_type *cell);

/** Per-cell free function table **/
extern void (*ai_cell_free_table[AI_MAX_ALGO])(cell_type *cell);


/** Per-side init function table **/
extern int (*ai_init_table[AI_MAX_ALGO])(int side, float skill);

/** Per-side free function table **/
extern void (*ai_free_table[AI_MAX_ALGO])(int side);


/** Per-side algorithm function table **/
extern void (*ai_algo_table[AI_MAX_ALGO])(int side);


#endif /* INCLUDED_AI_H */
