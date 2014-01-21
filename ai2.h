#ifndef INCLUDED_AI2_H
#define INCLUDED_AI2_H

/* ai2.h - Pierre Bureau - 05/2OO1 */
/* Internal include file for AI algorithm 2. */

#include "extern.h"

struct ai2_struct
{
  unsigned     reserve;      /* cell's available reserve troops */
  unsigned    *need_level;   /* cell's need for troop */
  cell_type   *attack_route; /* next cell for attacking route */
  unsigned    *status;       /* cell's status for ai purpose */
  unsigned    *route_status; /* routing status */
  unsigned    *tgt_status;   /* targeting status */
  unsigned    *route_resist; /* resistance for this attack route */
  cell_type  **route_tgt;    /* target for a passing route */
    
  int          connect_scan;
  s_char       front_scan;
  s_char       tmp_scan;

  /* P.Bureau - number of vectors arriving to the cell for a given player */
  s_char       feed;
};

/*
 * Ai skill level names
 */

#define PACIFIST 1
#define TRAINEE 2
#define COMMANDER 3
#define ELITE 4
#define PSYCO 5
#define QUICK 6
#define BARBARIAN 8
#define ULTIMATE 10
#define TEST 11

/*
 * Status List
 */
#define C_DULL        0 /* uninteresting cell */
#define C_FREE_BORDER 1 /* free border cell */
#define C_BORDER      2 /* occupied border cell */
#define C_FRONT       3 /* front territory cell (adjacent to enemy) */
#define C_PERIPH      4 /* peripheral cell not adjacent to enemy */
/*
 * Routing status list
 */
#define C_ROUTE_TO_ATTACK 6 /* route to enemy cell to attack */
#define C_ATTACK_SUPPLY   7 /* cell is supplying an attack */
#define C_ROUTE_TO_TGT   11 /* route to a target */
/*
 * Target status list
 */
#define C_DIRECT_ATTACK     8 /* enemy cell to attack */
#define C_INDIRECT_ATTACK   5 /* distant enemy cell to attack */
#define C_DIRECT_TGT        9 /* direct target (free cell) */
#define C_INDIRECT_TGT     10 /* distant target (free cell) */
#define C_TGT_UNDER_ATTACK 12 /* enemy cell under attack */
/*
 * Need Level flags
 */
#define L_FRONT 1
#define L_NONE 0

/*
 * AI configuration
 */

#define MAX_RESERVE_DEPTH 3

#define MAX_ROUTE_LEN 5

#define ABS_MAX_ATTACK 10
#define MAX_ATT_TIMER 150

#define MAX_ATT_VAL 10000

#define C_SCAN_FLAG 999

#define MAX_VICTORY_TIMER 10

#undef FIGHT_TO_DEATH
#undef ONE_CONNECT_PER_LEVEL

/* Some tables length */
#define MAX_LEVLEN 1000
#define MAX_TABLESIZE MAX_BOARDSIZE*MAX_BOARDSIZE

#endif /* INCLUDED_AI2_H */
