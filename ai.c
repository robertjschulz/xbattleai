#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* Copyright 2000 - Mathias Broxvall */

#include "constant.h"
#include "extern.h" /* for cell_type */
#include "ai.h"



/***********************************************/
/* dummy AI for human players */

static int cell_init_dummy(cell_type *cell)
{
    return 0;
}

static void cell_free_dummy(cell_type *cell)
{
}

static int init_dummy(int side, float skill)
{
    return 0;
}

static void free_dummy(int side)
{
}

static void ai_dummy(int side)
{
}
/***********************************************/


/**********************************************/
/*
 * P.Bureau - Generic code to handle mutiple algo
 * For new algorithm, declare init function and algo
 * function in ai.h, increase AI_MAX_ALGO, and modify
 * both tables below with new functions.
 */

/* table for cell init functions */
int (*ai_cell_init_table[AI_MAX_ALGO])(cell_type *cell)={
    cell_init_dummy,
    cell_init_algo1,
    cell_init_algo2
};

/* table for cell free functions */
void (*ai_cell_free_table[AI_MAX_ALGO])(cell_type *cell)={
    cell_free_dummy,
    cell_free_algo1,
    cell_free_algo2
};

/* table for side init functions */
int (*ai_init_table[AI_MAX_ALGO])(int side, float skill)={
    init_dummy,
    init_algo1,
    init_algo2
};

/* table for side free functions */
void (*ai_free_table[AI_MAX_ALGO])(int side)={
    free_dummy,
    free_algo1,
    free_algo2
};

/* table for algo functions */
void (*ai_algo_table[AI_MAX_ALGO])(int side)={
    ai_dummy,
    ai_algo1,
    ai_algo2
};

/***********************************************/

