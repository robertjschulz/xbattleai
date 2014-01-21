#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* ai2.c rev0.2 - Pierre Bureau - 05/2001 */
/*
 * Second algorithm to handle computer players.
 *
 */


/* TODO:
 * - handle fog of war
 * - handle special options such as guns, para, etc.
 *
 */

#include <stdio.h>
#ifdef HAVE_STDDEF_H
# include <stddef.h>
#else
# ifndef NULL
#  define NULL ((void *)0)
# endif
#endif
#ifdef STDC_HEADERS
# include <stdlib.h>
#else
# ifdef HAVE_MALLOC_H
#  include <malloc.h>
# endif
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
# ifdef HAVE_MEMORY_H
#  include <memory.h>
# endif
#endif

#include "constant.h"
#include "extern.h"

#include "ai2.h"
#include "ai2_if.h"

#undef AI_STATUS_DEBUG
#undef AI_ATTACK_DEBUG
#undef AI_LEARNING_DEBUG

/* Globals */

/* tables simplifying calls to set_move_xxx()*/
#ifdef UNUSED
static const int dirset4[4][4]={{1,0,0,0},
	                        {0,1,0,0},
                                {0,0,1,0},
                                {0,0,0,1}};
#endif

#ifdef UNUSED
static const int routeback4[4][4]={{0,0,1,0},
                                   {0,0,0,1},
                                   {1,0,0,0},
                                   {0,1,0,0}};
#endif

static const int routebackind4[4]={2,3,0,1};

static const int dirset[6][6]={{1,0,0,0,0,0},
                               {0,1,0,0,0,0},
                               {0,0,1,0,0,0},
                               {0,0,0,1,0,0},
                               {0,0,0,0,1,0},
                               {0,0,0,0,0,1}};

static const int routeback6[6][6]={{0,0,0,1,0,0},
                                   {0,0,0,0,1,0},
                                   {0,0,0,0,0,1},
                                   {1,0,0,0,0,0},
                                   {0,1,0,0,0,0},
                                   {0,0,1,0,0,0}};

static const int routebackind6[6]={3,4,5,0,1,2};

static const int * routebackind;


#ifdef UNUSED
static const int ** routeback;
static const int ** dirset;
#endif

/**********************************************/

/* Struct for attack route algorithm */

typedef struct route_link {
    cell_type * cell;
    struct route_link * next;
}RouteLink;

/* Struct to store info for an attack */
typedef struct attack_info {
    cell_type * final_target;
    cell_type * target;
    unsigned previous; /* previous resistance */
    unsigned source;   /* number of supply cells */
    int timer;
}AttackInfo;

/*Struct to store all Ai players info*/
typedef struct player_info {
    /* Ai speed parameters */
    unsigned orga_rate;
    unsigned devel_rate;
    unsigned attack_rate;

    /* Ai skill parameters */
    float    min_att_str; /* can be less than 1 for suicidal Ai */
    float    min_op_str;
    unsigned max_perim_range;
    unsigned max_attack_range;
    float    min_mean_value; /* current mean value to target */
    float    extend_ratio;

    /* First turn flag */
    unsigned first_turn;

    /* Generic info */
    unsigned previous_ter;
    unsigned max_cell_value;
    
    /* Attack info */
    unsigned max_attack;     /* max number of authorized attack */
    unsigned cur_max_attack; /* current number of authorized attack */
    unsigned cur_attack;     /* effective number of current attack */
    unsigned max_op_attack;  /* number of authorized opportunity attack */
    unsigned cur_op_attack;  /* number of current opportunity attack */
    AttackInfo attack_list[ABS_MAX_ATTACK];
    AttackInfo op_attack_list[ABS_MAX_ATTACK];
    
    /* reorganisation info */
    unsigned cur_level;
    unsigned level_size;
    cell_type * level1[MAX_TABLESIZE];
    cell_type * level2[MAX_TABLESIZE];
    cell_type ** l1;
    cell_type ** l2;

    /* Counters */
    unsigned orga_count;
    unsigned devel_count;
    unsigned attack_count;
}PlayerInfo;
 
static PlayerInfo * playerlist[MAX_PLAYERS];


/* File local prototypes */
static unsigned ScanReserve(cell_type * cell,unsigned depth,int side);
#ifdef UNUSED
static int FindOwner(cell_type * cell);
#endif
static unsigned FindTerritoryStatus(cell_type ** territory,int side);
static unsigned FindFreeBorder(cell_type ** celltable,int side);
static unsigned FindBorder(cell_type ** celltable,int side);
static unsigned FindPeriph(cell_type ** periphlist,cell_type ** cell_list,
		unsigned len, int side);
static unsigned FindFront(cell_type ** frontlist,cell_type ** cell_list,
		unsigned length, int side);
static void ResetStatus(int side);
static void ResetAttackRoute(int side);
static unsigned FindNextLevel(cell_type ** leveltab,unsigned length,
		cell_type ** nextlevel,unsigned level,int side);
static void ConnectLevel(cell_type ** leveltab,unsigned length,
		unsigned level,int side);
static void ResetLevel(int side);
static void FindDoubleRoute(cell_type ** territory, unsigned t_len);
static void ResetScanConnect(int side);
static void FindRouteTo(cell_type * cell,int side,cell_type * tgt);
static unsigned FindAttackRouteTo(cell_type * cell,RouteLink * origin,
		unsigned length,int side);
static void MarkAttackRouteTo(cell_type * cell, unsigned route_value, int side);
static void ScanEnemy(cell_type ** borderlist,unsigned length,int side);
static void ScanPerimeter(cell_type ** borderlist,unsigned length,int side);
static int MoveTo(cell_type * cell,int side,unsigned condition);
static int CheckOpAttack(cell_type * cell,unsigned reserve_lim,int side);
static int AbortOpAttack(AttackInfo * attack,unsigned reserve_lim,int side);
static unsigned Attack(cell_type * cell,unsigned limit,int side);
static void UnAttack(cell_type * cell,int side);
static int AbortAttack(AttackInfo * attack,unsigned reserve_lim,int side);
static float AttackValue(cell_type * cell, int side);
static int CheckDirectAttack(cell_type * cell,unsigned reserve_lim,int side);
static int CheckIndirectAttack(cell_type * cell,unsigned reserve_lim,int side);
#ifdef WITH_AI2_LEARNING
static unsigned FindMeanValue(cell_type ** territory,unsigned len,int side);
static void LearnParameter(PlayerInfo * player,float mean,unsigned ter,int side);
#endif



/*************************************************************/
/* Set up the AI in a new map cell.
 *
 * Returns zero on success.
 */

int cell_init_algo2(cell_type *cell) {
    cell->ai2 = malloc(sizeof(ai2_type));
    cell->ai2->need_level = (unsigned*)(malloc(sizeof(unsigned)*(Config->side_count)));
    cell->ai2->status = (unsigned*)(malloc(sizeof(unsigned)*(Config->side_count)));
    cell->ai2->route_status = (unsigned*)(malloc(sizeof(unsigned)*(Config->side_count)));
    cell->ai2->tgt_status = (unsigned*)(malloc(sizeof(unsigned)*(Config->side_count)));
    cell->ai2->route_resist = (unsigned*)(malloc(sizeof(unsigned)*(Config->side_count)));
    cell->ai2->route_tgt  = (cell_type**)(malloc(sizeof(cell_type*)*(Config->side_count)));
    cell->ai2->feed = 0;
#ifdef WITH_BASE_SIDE
    cell->base_side= -1; /* FIXME: why is this here */
#endif

    return 0; /* FIXME: check malloc()s for failure */
}


/*************************************************************/
/* Free the AI resources in a map cell.
 */

void cell_free_algo2(cell_type *cell) {
    free(cell->ai2->route_tgt);
    free(cell->ai2->route_resist);
    free(cell->ai2->tgt_status);
    free(cell->ai2->route_status);
    free(cell->ai2->status);
    free(cell->ai2->need_level);
    free(cell->ai2);
}


/*************************************************************/
/* Set up the AI player for a new side.
 *
 * Returns zero on success.
 */

int init_algo2(int side,float skill) {
    int width,height,x,y;
    cell_type * cell;
    ai2_type * ai2;
    int iskill;

    iskill=skill;
    
    width = Config->board_x_size;
    height = Config->board_y_size;

    /** Init some cell's value */
    for(x=0;x<width;x++)
        for(y=0;y<height;y++) {

            cell = Board->cells[y][x];
            if(!cell) continue;
	    ai2 = cell->ai2;

            ai2->status[side]=C_DULL;
            ai2->route_status[side]=C_DULL;
            ai2->tgt_status[side]=C_DULL;
            ai2->route_tgt[side]=NULL;
            ai2->need_level[side]=L_NONE;
            ai2->reserve/*[side]*/=0;
            ai2->connect_scan=0;
            ai2->front_scan=0;
            ai2->tmp_scan=0;
        }
    
    /** Init Player info struct **/
    playerlist[side]=(PlayerInfo*)malloc(sizeof(PlayerInfo));
    playerlist[side]->first_turn=1;
    playerlist[side]->cur_level=0;
    playerlist[side]->level_size=0;
    playerlist[side]->previous_ter=0;

    /** Init Ai skill data **/

    switch(iskill)
    {
    case PACIFIST :
        /* Pacifist - average development rate but hardly attacks */
        playerlist[side]->orga_rate=5;
        playerlist[side]->attack_rate=20;
        playerlist[side]->devel_rate=10;
        playerlist[side]->extend_ratio=1.2;
        playerlist[side]->max_perim_range=5;
        playerlist[side]->max_attack_range=3;
        playerlist[side]->min_att_str=1.9;
        playerlist[side]->min_op_str=1.9;
        playerlist[side]->cur_attack=0;
        playerlist[side]->max_attack=1;
        playerlist[side]->cur_op_attack=0;
        playerlist[side]->max_op_attack=1;
        break;
    case TRAINEE :
        /* Trainee - slow development, average attack skill, good organisation */
        playerlist[side]->orga_rate=1;
        playerlist[side]->attack_rate=10;
        playerlist[side]->devel_rate=15;
        playerlist[side]->extend_ratio=1.5;
        playerlist[side]->max_perim_range=5;
        playerlist[side]->max_attack_range=3;
        playerlist[side]->min_att_str=1.25;
        playerlist[side]->min_op_str=1.9;
        playerlist[side]->cur_attack=0;
        playerlist[side]->max_attack=2;
        playerlist[side]->cur_op_attack=0;
        playerlist[side]->max_op_attack=1;
        break;
    case COMMANDER :
        /* Commander - slow devlopment, good attack skill, good organisation */
        playerlist[side]->orga_rate=1;
        playerlist[side]->attack_rate=5;
        playerlist[side]->devel_rate=15;
        playerlist[side]->extend_ratio=1.5;
        playerlist[side]->max_perim_range=5;
        playerlist[side]->max_attack_range=3;
        playerlist[side]->min_att_str=1.5;
        playerlist[side]->min_op_str=1.9;
        playerlist[side]->cur_attack=0;
        playerlist[side]->max_attack=3;
        playerlist[side]->cur_op_attack=0;
        playerlist[side]->max_op_attack=2;
        break;
    case ELITE :
        /* Elite - good development, good attack skill, good organisation */
        playerlist[side]->orga_rate=1;
        playerlist[side]->attack_rate=2;
        playerlist[side]->devel_rate=10;
        playerlist[side]->extend_ratio=1;
        playerlist[side]->max_perim_range=5;
        playerlist[side]->max_attack_range=4;
        playerlist[side]->min_att_str=1.5;
        playerlist[side]->min_op_str=1.9;
        playerlist[side]->cur_attack=0;
        playerlist[side]->max_attack=3;
        playerlist[side]->cur_op_attack=0;
        playerlist[side]->max_op_attack=3;
        break;
    case QUICK :
        /* Quick - fast extension, average attack */
        playerlist[side]->orga_rate=5;
        playerlist[side]->attack_rate=10;
        playerlist[side]->devel_rate=5;
        playerlist[side]->extend_ratio=0.5;
        playerlist[side]->max_perim_range=7;
        playerlist[side]->max_attack_range=3;
        playerlist[side]->min_att_str=1.25;
        playerlist[side]->min_op_str=1.9;
        playerlist[side]->cur_attack=0;
        playerlist[side]->max_attack=2;
        playerlist[side]->cur_op_attack=0;
        playerlist[side]->max_op_attack=1;
        break;
    case PSYCO :
        /* Psyco - average development, frenetic attacks, Poor organisation */
        playerlist[side]->orga_rate=20;
        playerlist[side]->attack_rate=1;
        playerlist[side]->devel_rate=10;
        playerlist[side]->extend_ratio=1.2;
        playerlist[side]->min_op_str=1.9;
        playerlist[side]->max_perim_range=5;
        playerlist[side]->max_attack_range=3;
        playerlist[side]->min_att_str=0.5;
        playerlist[side]->min_op_str=1.9;
        playerlist[side]->cur_attack=0;
        playerlist[side]->max_attack=5;
        playerlist[side]->cur_op_attack=0;
        playerlist[side]->max_op_attack=5;
        break;
    case BARBARIAN :
        /* Barbarian - fast development, fast attack, Poor organisation */
        playerlist[side]->orga_rate=20;
        playerlist[side]->attack_rate=1;
        playerlist[side]->devel_rate=5;
        playerlist[side]->extend_ratio=1;
        playerlist[side]->max_perim_range=5;
        playerlist[side]->max_attack_range=3;
        playerlist[side]->min_att_str=1;
        playerlist[side]->min_op_str=1.9;
        playerlist[side]->cur_attack=0;
        playerlist[side]->max_attack=5;
        playerlist[side]->cur_op_attack=0;
        playerlist[side]->max_op_attack=3;
        break;
    case ULTIMATE :
        /* The Ultimate fighter */
        playerlist[side]->orga_rate=1;
        playerlist[side]->attack_rate=1;
        playerlist[side]->devel_rate=5;
        playerlist[side]->extend_ratio=1;
        playerlist[side]->max_perim_range=5;
        playerlist[side]->max_attack_range=5;
        playerlist[side]->min_att_str=1.75;
        playerlist[side]->min_op_str=1.9;
        playerlist[side]->cur_attack=0;
        playerlist[side]->max_attack=5;
        playerlist[side]->cur_op_attack=0;
        playerlist[side]->max_op_attack=5;
        break;
    case TEST :
        /* The Ultimate fighter */
        playerlist[side]->orga_rate=1;
        playerlist[side]->attack_rate=1;
        playerlist[side]->devel_rate=10;
        playerlist[side]->extend_ratio=1.2;
        playerlist[side]->max_perim_range=5;
        playerlist[side]->max_attack_range=5;
        playerlist[side]->min_att_str=1;
        playerlist[side]->min_op_str=1.9;
        playerlist[side]->cur_attack=0;
        playerlist[side]->max_attack=1;
        playerlist[side]->cur_op_attack=0;
        playerlist[side]->max_op_attack=0;
        break;
    default:
        printf("Warning - Unknow Ai skill, backup to default\n");
        /* Average settings */
        playerlist[side]->orga_rate=5;
        playerlist[side]->attack_rate=5;
        playerlist[side]->devel_rate=10;
        playerlist[side]->extend_ratio=1.5;
        playerlist[side]->max_perim_range=5;
        playerlist[side]->max_attack_range=3;
        playerlist[side]->min_att_str=1.5;
        playerlist[side]->min_op_str=1.9;
        playerlist[side]->cur_attack=0;
        playerlist[side]->max_attack=3;
        playerlist[side]->cur_op_attack=0;
        playerlist[side]->max_op_attack=2;
    }

#ifdef WITH_AI2_LEARNING
    playerlist[side]->cur_max_attack=0;
#else
    playerlist[side]->cur_max_attack=playerlist[side]->max_attack;
#endif
    
    return 0;
}


/*************************************************************/
/* Free the AI resources for a side.
 */
void free_algo2(int side) {
    free(playerlist[side]);
}


/*********************************************/
/* To estimate the reserve for a given cell.
 * This function use recursive call to estimate reserve
 * for the supply network up to MAX_RESERVE_DEPTH distance.
 * When the network is modified (some routes are added or deleted),
 * reserve will change.
 */
static unsigned ScanReserve(cell_type * cell,unsigned depth,int side)
{
    unsigned int i;
    cell_type * scan_cell;
    unsigned reserve=0;
    register s_char smove;
    
    if(!cell)
        return 0;

    smove=cell->move;

    cell->ai2->reserve/*[side]*/=0;
    
    /* If depth not too big, Scan all connected cells */
    if( depth < MAX_RESERVE_DEPTH )
        for(i=0;i<Config->direction_count;i++)
        {
            scan_cell=cell->connect[i];

            if(scan_cell != cell &&
               scan_cell->dir[routebackind[i]] &&
               scan_cell->side == side)
                reserve+=ScanReserve(scan_cell,depth+1,side);
        }

    /* Add the cell's own value */
    reserve+=cell->value[side];
    
    /* Store the cell's total reserve for future use
     * and divide the reserve by the number of leaving route
     */
    if(smove)
    {
        cell->ai2->reserve/*[side]*/=reserve/smove;
        return reserve/smove;
    }

    cell->ai2->reserve/*[side]*/=reserve;
    return reserve;
}


#ifdef UNUSED
/******************************************/
/* Find owner of a given cell
 * Return id of the owner.or -1 if cell is unoccupied
 * Usefull for cells with fight.
 */
static int FindOwner(cell_type * cell)
{
    int owner;
    unsigned strength,i;
    
    /* find cell's owner */
    strength = 0; owner = -1;
    for(i=0;i<Config->side_count;i++)
        if(cell->value[i] > strength) {
            strength = cell->value[i];
            owner = i;
        }
    return owner;
}
#endif


/*************************************************************/
/* Find and list all occupied cells.
 * celltable must be allocated and big enough to store the
 * entire list.
 * The status for the following cells is also set:
 *   - Peripheral cells (cells adjacent to free cells only)
 *   - Front cells (cells adjacent to enemy occupied cell)
 *   - Border cells (occupied cells adjacent to territory)
 *   - Free Border cells (free cells adjacent to territory)
 *
 * Returns the number of occupied cells.
 */

static unsigned FindTerritoryStatus(cell_type ** territory,int side)
{
    cell_type * scan_cell;
    cell_type * cell;
    int owner;
    unsigned x,y,j,n=0;
    int isfront=0;
    int isperiph=0;
    

    for(x=0;x<Config->board_x_size;x++)
        for(y=0;y<Config->board_y_size;y++)
            if(Board->cells[y][x]->side==side)
            {
                cell=Board->cells[y][x];

                for(j=0;j<Config->direction_count;j++)
                {
                    scan_cell=cell->connect[j];

                    if(scan_cell->level < 0)
                        continue;

                    owner=scan_cell->side;

                    if(owner == SIDE_NONE)
                    {
                        scan_cell->ai2->status[side]=C_FREE_BORDER;
                        isperiph=1;
                    }
                    else
                    {
                        if(owner != side)
                        {
                            scan_cell->ai2->status[side]=C_BORDER;
                            isfront=1;
                        }
                    }
                }
        
                if(isfront)
                {
                    cell->ai2->status[side]=C_FRONT;
                    territory[n]=cell;
                    n++;
                    isfront=0;
                    isperiph=0;
                } else {
                    if(isperiph)
                    {
                        cell->ai2->status[side]=C_PERIPH;
                        territory[n]=cell;
                        n++;
                        isperiph=0;
                    }
                    else
                    {
                        cell->ai2->status[side]=C_DULL;
                        territory[n]=cell;
                        n++;
                    }
                }
            }
    return n;
}


/**************************************************************/
/* Find and list all free border cells.
 * celltable must be allocated and big enough to store the
 * entire list.
 *
 * Returns the number of free border cells.
 */
static unsigned FindFreeBorder(cell_type ** celltable,int side)
{
    unsigned x,y,n=0;
    
    for(x=0;x<Config->board_x_size;x++)
        for(y=0;y<Config->board_y_size;y++)
            if(Board->cells[y][x]->ai2->status[side]==C_FREE_BORDER)
            {
                celltable[n]=Board->cells[y][x];
                n++;
            }
    return n;
}


/***********************************************/
/* Find and list all border cells (free or not)
 * celltable must be allocated and big enough to store the
 * entire list.
 *
 * Returns the number of border cells.
 */
static unsigned FindBorder(cell_type ** celltable,int side)
{
    unsigned x,y,n=0;
    unsigned status;
    
    for(x=0;x<Config->board_x_size;x++)
        for(y=0;y<Config->board_y_size;y++)
        {
            status=Board->cells[y][x]->ai2->status[side];
            
            if(status == C_BORDER ||
               status == C_FREE_BORDER)
            {
                celltable[n]=Board->cells[y][x];
                n++;
            }
        }
    return n;
}


/********************************************************/
/* Find and list all peripheral cells (Front or Periph)
 *
 * Returns number of listed cells
 */
static unsigned FindPeriph(cell_type ** periphlist,cell_type ** cell_list,
                    unsigned len, int side)
{
    unsigned i,n=0;
    unsigned status;
    
    for(i=0;i<len;i++)
    {
        status=cell_list[i]->ai2->status[side];

        if(status == C_FRONT ||
           status == C_PERIPH)
        {
            periphlist[n]=cell_list[i];
            n++;
        }
    }

    return n;
}


/***********************************************************/
/* Find and list all front cells (adjacent to enemy)
 *
 * Returns number of listed cells
 */
static unsigned FindFront(cell_type ** frontlist,cell_type ** cell_list,
                        unsigned length,int side)
{
    unsigned i,n=0;
    
    for(i=0;i<length;i++)
        if(cell_list[i]->ai2->status[side] == C_FRONT)
        {
            frontlist[n]=cell_list[i];
            n++;
        }
    
    return n;
}


/**************************************************************/
/* Reset status for the board's cells and for the given player
 *
 */
static void ResetStatus(int side)
{
    unsigned x,y;
    cell_type * cell;
    ai2_type * ai2;
    
    for(x=0;x<Config->board_x_size;x++)
        for(y=0;y<Config->board_y_size;y++)
        {
            cell=Board->cells[y][x];
	    ai2 = cell->ai2;
            
            switch(ai2->route_status[side])
            {
            case C_ROUTE_TO_TGT:
                /* Reset if target is occupied */
                if(ai2->route_tgt[side]->side != SIDE_NONE)
                    ai2->route_status[side]=C_DULL;
                break;
                
            case C_ATTACK_SUPPLY:
                /* Reset cells if attacked target is taken */
                if(cell->side != side ||
                   (ai2->route_tgt[side] &&
                   ai2->route_tgt[side]->ai2->tgt_status[side] != C_TGT_UNDER_ATTACK)/*||
                   cell->ai2->status[side] != C_FRONT*/ )
                {
#ifdef AI_ATTACK_DEBUG
                    fprintf(stderr,"rem attack:(%d,%d)\n",x,y);
#endif
                    ai2->route_tgt[side]=NULL;
                    ai2->route_status[side]=C_DULL;
                }
                break;
                
            case C_ROUTE_TO_ATTACK:
                /*Don't touch attack routes*/
                break;
                
            default:
                if(cell->side == side)
                    ai2->route_status[side]=C_DULL;
            }
            
            if(ai2->tgt_status[side]==C_DIRECT_TGT ||
               ai2->tgt_status[side]==C_INDIRECT_TGT)
                /* Reset if target is occupied */
                if(cell->side != SIDE_NONE)
                    ai2->tgt_status[side]=C_DULL;
 
            ai2->status[side]= C_DULL;
            ai2->front_scan=0;
        }
}


static void ResetAttackRoute(int side)
{
    unsigned x,y;
    ai2_type * ai2;
    
    for(x=0;x<Config->board_x_size;x++)
        for(y=0;y<Config->board_y_size;y++)
        {
            ai2=Board->cells[y][x]->ai2;

            /* Reset routes */
            if(ai2->route_status[side] == C_ROUTE_TO_ATTACK)
            {
                ai2->route_status[side]=C_DULL;
                ai2->route_tgt[side]=NULL;
            }

            /* Reset all target */
            if(ai2->tgt_status[side] == C_INDIRECT_ATTACK ||
               ai2->tgt_status[side] == C_DIRECT_ATTACK)
                ai2->tgt_status[side]=C_DULL;
        }
}


/****************************************************/
/* Find the next need level, the next level includes
 * all cells, that are not part of a level, and connect to
 * the given level list
 *
 */
static unsigned FindNextLevel(cell_type ** leveltab,unsigned length,
                       cell_type ** nextlevel,
                       unsigned level,int side)
{
    unsigned i,j,n=0;
    cell_type * scan_cell;
    ai2_type * ai2;
    
    for(i=0;i<length;i++)
        for(j=0;j<Config->direction_count;j++)
        {
            scan_cell=leveltab[i]->connect[j];
	    ai2 = scan_cell->ai2;
            if(scan_cell->side == side &&
               ai2->need_level[side] == L_NONE &&
               ai2->route_status[side] != C_ROUTE_TO_TGT &&
               ai2->tgt_status[side] != C_INDIRECT_TGT &&
               ai2->route_status[side] != C_ROUTE_TO_ATTACK)
            {
                ai2->need_level[side]=level;
                nextlevel[n]=scan_cell;
                n++;
            }
        }
    
    return n;
}


/***************************************************/
/* Switch on routes from cells of the given level to cells of
 * lower level;
 *
 */
static void ConnectLevel(cell_type ** leveltab,unsigned length,
			unsigned level,int side)
{
    unsigned i,j;
    cell_type * scan_cell;
    int dir[MAX_DIRECTIONS]={0,0,0,0};
    
    for(i=0;i<length;i++)
    {
        for(j=0;j<Config->direction_count;j++)
        {
            scan_cell=leveltab[i]->connect[j];
            if(scan_cell->side == side &&
               scan_cell->ai2->need_level[side] == level-1)
            {
                dir[j]=1;
                /* increase the feed value for the connected cell */
                leveltab[i]->connect[j]->ai2->feed++;
#ifdef ONE_CONNECT_PER_LEVEL
                break;
#endif
            }
        }
        set_move_on(leveltab[i],dir,0);
        draw_multiple_cell (leveltab[i]);

        for(j=0;j<Config->direction_count;j++)
            dir[j]=0;
    }
}


static void ResetLevel(int side)
{
    unsigned x,y;
    ai2_type * ai2;
    
    for(x=0;x<Config->board_x_size;x++)
        for(y=0;y<Config->board_y_size;y++)
        {
            ai2=Board->cells[y][x]->ai2;

            if(ai2->status[side] == C_FRONT)
                ai2->need_level[side]=L_FRONT;
            else
                ai2->need_level[side]=L_NONE;
        }
}


/*******************************************************/
/* Scan board to switch off "double" routes
 *
 */
static void FindDoubleRoute(cell_type ** territory, unsigned t_len)
{
    unsigned i,j;
    cell_type * scan_cell;
    
    for(i=0;i<t_len;i++)
        for(j=0;j<Config->direction_count;j++)
        {
            scan_cell=territory[i]->connect[j];
            if(territory[i]->dir[j] &&
               scan_cell->dir[routebackind[j]])
            {
                set_move_off(territory[i],dirset[j],0);
                set_move_off(scan_cell,dirset[routebackind[j]],0);
                /* Adjust feed values */
                scan_cell->ai2->feed--;
                territory[i]->ai2->feed--;
            }
        }
}


/***********************************************************/
static void ResetScanConnect(int side)
{
    unsigned x,y;
    ai2_type * ai2;
    
    for(x=0;x<Config->board_x_size;x++)
        for(y=0;y<Config->board_y_size;y++)
        {
            ai2=Board->cells[y][x]->ai2;
            if(ai2->status[side] == C_FRONT ||
               ai2->status[side] == C_PERIPH )
                ai2->connect_scan=0;
            else
                ai2->connect_scan=C_SCAN_FLAG;
        }
}


/*************************************************************/
/* Find a route to the given cell using the connect_scan
 * level value.
 */
static void FindRouteTo(cell_type * cell,int side,cell_type * tgt)
{
    unsigned j;
    cell_type * scan_cell;
    
    for(j=0;j<Config->direction_count;j++)
    {
        scan_cell=cell->connect[j];
        if(scan_cell->ai2->status[side] == C_FRONT ||
           scan_cell->ai2->status[side] == C_PERIPH ||
           scan_cell->ai2->tgt_status[side] == C_DIRECT_TGT)
            return;
        if(scan_cell->ai2->connect_scan == (cell->ai2->connect_scan-1))
        {
            if(scan_cell->ai2->route_status[side] != C_ROUTE_TO_TGT)
            {
                scan_cell->ai2->route_status[side]=C_ROUTE_TO_TGT;
                scan_cell->ai2->route_tgt[side]=tgt;
            }
            FindRouteTo(scan_cell,side,tgt);
            return;
        }
    }
}


/*************************************************************/
/* Find a route to the given enemy cell using the connect_scan
 * level value.
 * Route may use other enemy cells or free cells.
 *
 * Returns, total amoutn of enemy troop encountered on the route
 */

static unsigned FindAttackRouteTo(cell_type * cell,RouteLink * origin,
                           unsigned length,int side)
{
    unsigned j;
    cell_type * scan_cell;
    RouteLink * scan_link;
    RouteLink newlink;
    unsigned best=~0;
    unsigned tmp;

    if(length < MAX_ROUTE_LEN)
    {
        /* Scan all cell connected with given cell */
        for(j=0;j<Config->direction_count;j++)
        {
            scan_cell=cell->connect[j];

            /* Checking for border cells with self-connection */
            if(cell == scan_cell)
                goto jump;

            /* First check if the cell has not been already visited */
            scan_link=origin;

            while(scan_link)
            {
                if(scan_cell == scan_link->cell)
                    goto jump;
                scan_link=scan_link->next;
            }

            /* If the given cell is connected to front, Stop reccursion */
            if(scan_cell->ai2->status[side] == C_FRONT ||
               scan_cell->ai2->status[side] == C_PERIPH)
            {
                cell->ai2->attack_route=scan_cell;
                return cell->value[cell->side];
            }
            else {
                /* Launch reccursion on cells with proper scan_connect */
                if(scan_cell->ai2->connect_scan != C_SCAN_FLAG &&
                   scan_cell->ai2->connect_scan >= cell->ai2->connect_scan)
                {
                    /* Update the route link with the new cell */
                    newlink.cell=cell;
                    newlink.next=origin;
                    tmp=FindAttackRouteTo(scan_cell,&newlink,length+1,side);

                    if(tmp != ~0 &&
                       tmp + cell->value[cell->side]< best)
                    {
                        best=tmp + cell->value[cell->side];
                        cell->ai2->attack_route=scan_cell;
                }
                }
            }

        jump:
            ;
        }
    }
    return best;
}


/***********************************************************/
/* After a successful FindAttackRouteTo(), this function marks
 * all the cells of the best route found as C_ROUTE_TO_ATTACK.
 * It also set the starting point for this route, cell->ai2->route_resist
 */
static void MarkAttackRouteTo(cell_type * cell, unsigned route_value, int side)
{
    unsigned length=0;
    cell_type * scan_cell;

    scan_cell=cell->ai2->attack_route;

#ifdef AI_ATTACK_DEBUG
    fprintf(stderr,"(%d,%d)<-",cell->x,cell->y);
#endif
    
    while( scan_cell->ai2->status[side] != C_FRONT &&
           scan_cell->ai2->status[side] != C_PERIPH &&
           length < MAX_ROUTE_LEN +5 )
    {
        scan_cell->ai2->route_status[side]=C_ROUTE_TO_ATTACK;
#ifdef AI_ATTACK_DEBUG
        fprintf(stderr,"(%d,%d)<-",scan_cell->x,scan_cell->y);
#endif
        scan_cell->ai2->route_resist[side]=route_value;
        scan_cell->ai2->route_tgt[side]=cell;
        scan_cell=scan_cell->ai2->attack_route;
        length++;
    }
#ifdef AI_ATTACK_DEBUG
    fprintf(stderr,":%d\n",route_value);
#endif
}


/***************************************************/
/* Scan the enemy territory to find valuable cells such as
 * towns or weak cells.
 * The scan depth in enemy territory is MAX_ATTACK_RANGE
 */
static void ScanEnemy(cell_type ** borderlist,unsigned length,int side)
{
    int scan_level=-3;
    unsigned i,j,k,levlen=0;
    unsigned n=0;
    unsigned best_route;
    cell_type * level1[MAX_LEVLEN];
    cell_type * level2[MAX_LEVLEN];
    cell_type ** levptr1;
    cell_type ** levptr2;
    cell_type ** tmpptr;
    cell_type * scan_cell;
    cell_type * test_cell;

    ResetScanConnect(side);

    for(i=0;i<length;i++)
        borderlist[i]->ai2->connect_scan=-1;
    
    for(i=0;i<length;i++)
    {
        test_cell=borderlist[i];
 
        if(test_cell->growth >= TOWN_MIN &&
           test_cell->side != SIDE_NONE &&
           test_cell->ai2->tgt_status[side] != C_TGT_UNDER_ATTACK)
            test_cell->ai2->tgt_status[side]=C_DIRECT_ATTACK;

        for(j=0;j<Config->direction_count;j++)
        {
            scan_cell=test_cell->connect[j];
            if(scan_cell->side != side &&
               scan_cell->ai2->connect_scan == C_SCAN_FLAG &&
               scan_cell->level >= 0)
            {
                scan_cell->ai2->connect_scan=-2;
                level1[levlen]=scan_cell;
                levlen++;
            }
        }
    }

    levptr1=level1;
    levptr2=level2;

    for(k=0;k<playerlist[side]->max_attack_range;k++)
    {
        for(i=0;i<levlen;i++)
        {
            test_cell=levptr1[i];

            if(test_cell->growth >= TOWN_MIN &&
               test_cell->side != SIDE_NONE)
            {
                if(test_cell->ai2->tgt_status[side] != C_TGT_UNDER_ATTACK)
                    test_cell->ai2->tgt_status[side]=C_INDIRECT_ATTACK;
                best_route=FindAttackRouteTo(test_cell,NULL,0,side);
                if(best_route != ~0)
                    MarkAttackRouteTo(test_cell,best_route,side);
            }

            for(j=0;j<Config->direction_count;j++)
            {
                scan_cell=test_cell->connect[j];
                if(scan_cell->side != side &&
                   scan_cell->ai2->connect_scan == C_SCAN_FLAG &&
                   scan_cell->level >= 0)
                {
                    levptr2[n]=scan_cell;
                    scan_cell->ai2->connect_scan=scan_level;
                    n++;
                }
            }
        }
        levlen=n;
        tmpptr=levptr1;
        levptr1=levptr2;
        levptr2=tmpptr;
        scan_level--;
        n=0;
    }
}


/***********************************************/
/* Scan the closest cells around territory to find the
 * more valuable cells.
 */
static void ScanPerimeter(cell_type ** borderlist,unsigned length,int side)
{
    unsigned scan_level=3;
    unsigned i,j,k,levlen=0;
    unsigned n=0;
    cell_type * level1[MAX_LEVLEN];
    cell_type * level2[MAX_LEVLEN];
    cell_type ** levptr1;
    cell_type ** levptr2;
    cell_type ** tmpptr;
    cell_type * scan_cell;
    cell_type * test_cell;
    
    
    ResetScanConnect(side);

    for(i=0;i<length;i++)
        borderlist[i]->ai2->connect_scan=1;

    for(i=0;i<length;i++)
    {
        test_cell=borderlist[i];
        
        if(test_cell->growth >= TOWN_MIN)
            test_cell->ai2->tgt_status[side]=C_DIRECT_TGT;

        for(j=0;j<Config->direction_count;j++)
        {
            scan_cell=test_cell->connect[j];
            if(scan_cell->side == SIDE_NONE &&
               scan_cell->ai2->connect_scan == C_SCAN_FLAG &&
               scan_cell->level >= 0)
            {
                scan_cell->ai2->connect_scan=2;
                level1[levlen]=scan_cell;
              levlen++;
            }
        }
    }

    levptr1=level1;
    levptr2=level2;

    for(k=0;k<playerlist[side]->max_perim_range;k++)
    {
        for(i=0;i<levlen;i++)
        {
            test_cell=levptr1[i];

            if(test_cell->growth >= TOWN_MIN)
            {
                test_cell->ai2->tgt_status[side]=C_INDIRECT_TGT;
                FindRouteTo(test_cell,side,test_cell);
            }

            for(j=0;j<Config->direction_count;j++)
            {
                scan_cell=test_cell->connect[j];
                if(scan_cell->side == SIDE_NONE &&
                   scan_cell->ai2->connect_scan == C_SCAN_FLAG &&
                   scan_cell->level >= 0)
                {
                    levptr2[n]=scan_cell;
                    scan_cell->ai2->connect_scan=scan_level;
                    n++;
                }
            }
        }
        levlen=n;
        tmpptr=levptr1;
        levptr1=levptr2;
        levptr2=tmpptr;
        scan_level++;
        n=0;
    }
}


/*************************************/
/* Search and toggle on a route to the given
 * cell from any side controlled cell with a reserve > condition.
 *
 * Return 1 if the move is enable
 * Return 0therwise
 */
static int MoveTo(cell_type * cell,int side,unsigned condition)
{
    unsigned j,i;
    cell_type * scan_cell;
    int dir[MAX_DIRECTIONS];

    for(j=0;j<Config->direction_count;j++)
    {
        for(i=0;i<Config->direction_count;i++) dir[i] = 0;
        
        scan_cell=cell->connect[j];
        if(scan_cell->side == side &&
           scan_cell->ai2->reserve > condition)
        {
            dir[routebackind[j]]=1;
            set_move_on(scan_cell,dir,0);
            cell->ai2->feed++;
            return 1;
        }
    }
    
    return 0;
}


/**********************************************************/
/* Check if opportunity attack to the given cell is possible .
 * available troops must overcome the target cell value
 *
 * Returns 1 if attack is valid
 * Returns 0 if attack is not valid
 */
static int CheckOpAttack(cell_type * cell,unsigned reserve_lim,int side)
{
    unsigned available_troop=0;
    unsigned j;
    cell_type * scan_cell;

    for(j=0;j<Config->direction_count;j++)
    {
        scan_cell=cell->connect[j];
        if(scan_cell->side == side &&
           scan_cell->ai2->route_status[side] != C_ATTACK_SUPPLY)
        {
            /* Note: Not sure if it is better to use the reserve
             * or the cell's value only.
             */
#if 0
            available_troop+=scan_cell->ai2->reserve-reserve_lim;
#endif
            available_troop+=scan_cell->value[side]-reserve_lim;
        }
    }

#ifdef AI_ATTACK_DEBUG
    fprintf(stderr,"op(%d,%d): r=%u,t=%u\n",
            cell->x,cell->y,cell->value[cell->side],available_troop);
#endif
    
    if(available_troop > (float)cell->value[cell->side] * playerlist[side]->min_op_str)
        return 1;

    return 0;
}


/**********************************************************/
/* Check if the given opportunity attack should continue or not.
 *
 * Return 0 if attack should continue
 * Return 1 if attack should abort
 */
static int AbortOpAttack(AttackInfo * attack,unsigned reserve_lim,int side)
{
    unsigned available_troop=0;
    unsigned resist=0;
    unsigned j;
    cell_type * scan_cell;
    cell_type * target;

    target=attack->target;

    resist=target->value[target->side];

    /* Check available troops */
    for(j=0;j<Config->direction_count;j++)
    {
        scan_cell=target->connect[j];
        /* Remove routes from depleted cells */
/*       if(scan_cell->side == side )
            if(scan_cell->value[side] < (current_player->max_cell_value/2))
                set_move_off(scan_cell,dirset[routebackind[j]],0);
            else
               set_move_on(scan_cell,dirset[routebackind[j]],0);
               */
        
        if(scan_cell->side == side &&
           scan_cell->dir[routebackind[j]])
        {
            /* Do we want to use the reserve here ??? */
            available_troop+=scan_cell->value[side]-reserve_lim;
        }
    }

    if((float)available_troop > (float)resist * playerlist[side]->min_op_str)
        attack->timer-=20;
    else
        attack->timer+=10;

    /* Check resistance evolution */
    if(resist < attack->previous)
        attack->timer-=10;
    else
        attack->timer+=30;

    /* Increase timer anyway */
    attack->timer++;

    if(attack->timer < 0)
        attack->timer=0;

#ifdef AI_ATTACK_DEBUG
    fprintf(stderr,"abort_op(%d,%d)->(%d,%d): r=%u,t=%u,timer=%u\n",
            target->x,target->y,
            attack->final_target->x,attack->final_target->y,
            resist,available_troop,attack->timer);
#endif

    attack->previous=resist;
    
    /* Check if attack is timed out */
    if(attack->timer > MAX_ATT_TIMER)
        return 1;

    return 0;
}


/**********************************************************/
/* Launch attack to the given cell. All connected cell with
 * a reserve level over the limit will be set to attack.
 */
static unsigned Attack(cell_type * cell,unsigned limit,int side)
{
    unsigned j;
    cell_type * scan_cell;
    unsigned src=0;

    for(j=0;j<Config->direction_count;j++)
    {
        scan_cell=cell->connect[j];
        if(scan_cell->side == side &&
           scan_cell->ai2->route_status[side] != C_ATTACK_SUPPLY &&
           scan_cell->ai2->reserve > limit)
        {
            if(cell->ai2->tgt_status[side] == C_TGT_UNDER_ATTACK)
                scan_cell->ai2->route_tgt[side]=cell;
            else
                scan_cell->ai2->route_tgt[side]=cell->ai2->route_tgt[side];
            
            scan_cell->ai2->route_status[side]=C_ATTACK_SUPPLY;
            set_move_on(scan_cell,dirset[routebackind[j]],0);
            src++;
        }
    }

    return src;
}


/**********************************************************/
/* Disable attack to the given cell. All connected route to
 * the cell are switched off.
 */
static void UnAttack(cell_type * cell,int side)
{
    unsigned j;
    cell_type * scan_cell;

    for(j=0;j<Config->direction_count;j++)
    {
        scan_cell=cell->connect[j];
        if(scan_cell->side == side)
        {
#if 0
            scan_cell->ai2->route_tgt[side]=NULL;
#endif
            scan_cell->ai2->route_status[side]=C_DULL;
            set_move_off(scan_cell,dirset[routebackind[j]],0);
        }
    }
}


/**********************************************************/
/* Check if the given attack should continue or not.
 *
 * Return 0 if attack should continue
 * Return 1 if attack should abort
 */
static int AbortAttack(AttackInfo * attack,unsigned reserve_lim,int side)
{
    unsigned available_troop=0;
    unsigned resist=0;
    unsigned j;
    cell_type * scan_cell;
    cell_type * target;

    target=attack->target;

    if(attack->final_target == target)
        resist=target->value[target->side];
    else
        resist=target->ai2->route_resist[side];

    /* Check available troops */
    for(j=0;j<Config->direction_count;j++)
    {
        scan_cell=target->connect[j];
        /* Remove routes from depleted cells */
#if 0
      if(scan_cell->side == side)
            if( scan_cell->value[side] < (current_player->max_cell_value/2))
                set_move_off(scan_cell,dirset[routebackind[j]],0);
            else
                set_move_on(scan_cell,dirset[routebackind[j]],0);
#endif
        
        if(scan_cell->side == side &&
           scan_cell->dir[routebackind[j]])
        {
            available_troop+=scan_cell->ai2->reserve-reserve_lim;
        }
    }

    if((float)available_troop > (float)resist * playerlist[side]->min_att_str)
        attack->timer-=20;
    else
        attack->timer+=10;

    /* Check resistance evolution */
    if(resist < attack->previous)
        attack->timer-=10;
    else
        attack->timer+=30;

    /* Increase timer anyway */
    attack->timer++;

    if(attack->timer < 0)
        attack->timer=0;

#ifdef AI_ATTACK_DEBUG
    fprintf(stderr,"abort(%d,%d)->(%d,%d): r=%u,t=%u,timer=%u\n",
            target->x,target->y,
            attack->final_target->x,attack->final_target->y,
            resist,available_troop,attack->timer);
#endif

    attack->previous=resist;
    
    /* Check if attack is timed out */
    if(attack->timer > MAX_ATT_TIMER)
        return 1;

    return 0;
}


/**********************************************************/
/* Returns the value for a given attack
 * The higer the value is, the more interesting is the cell to
 * attack
 */
static float AttackValue(cell_type * cell, int side)
{
    float value;
    float resist;
    float growth;

    if(cell->ai2->tgt_status[side] == C_DIRECT_ATTACK)
    {
        resist=cell->value[cell->side];
        growth=cell->growth;
    }
    else
    {
        resist=cell->ai2->route_resist[side];
        growth=cell->ai2->route_tgt[side]->growth;
    }

    if(resist)
        value=growth/resist;
    else
        value=MAX_ATT_VAL;
    
    return value;
}


/**********************************************************/
/* Check if attacking the given cell is possible. To be used
 * only for direct attack.
 * available troops must overcome the route resistance
 *
 * Returns 1 if attack is valid
 * Returns 0 if attack is not valid
 */
static int CheckDirectAttack(cell_type * cell,unsigned reserve_lim,int side)
{
    unsigned available_troop=0;
    unsigned resist=0;
    unsigned j;
    cell_type * scan_cell;
    
    resist=cell->value[cell->side];
    
    for(j=0;j<Config->direction_count;j++)
    {
        scan_cell=cell->connect[j];
        if(scan_cell->side == side &&
           scan_cell->ai2->route_status[side] != C_ATTACK_SUPPLY)
        {
            available_troop+=scan_cell->ai2->reserve-reserve_lim;
        }
    }

#ifdef AI_ATTACK_DEBUG
    fprintf(stderr,"(%d,%d): r=%u,t=%u\n",
            cell->x,cell->y,resist,available_troop);
#endif

    if(available_troop > (float)resist * playerlist[side]->min_att_str)
        return 1;

    return 0;
}


/**********************************************************/
/* Check if attacking the given cell is possible. To be used
 * only for indirect attack.
 * available troops must overcome the route resistance
 *
 * Returns 1 if attack is valid
 * Returns 0 if attack is not valid
 */

static int CheckIndirectAttack(cell_type * cell,unsigned reserve_lim,int side)
{
    unsigned available_troop=0;
    unsigned resist=0;
    unsigned j;
    cell_type * scan_cell;
    
    resist=cell->ai2->route_resist[side];
    
    for(j=0;j<Config->direction_count;j++)
    {
        scan_cell=cell->connect[j];
        if(scan_cell->side == side &&
           scan_cell->ai2->route_status[side] != C_ATTACK_SUPPLY)
        {
            available_troop+=scan_cell->ai2->reserve-reserve_lim;
        }
    }

#ifdef AI_ATTACK_DEBUG
    fprintf(stderr,"(%d,%d): r=%u,t=%u\n",
            cell->x,cell->y,resist,available_troop);
#endif

    if(available_troop > (float)resist * playerlist[side]->min_att_str)
        return 1;

    return 0;
}


#ifdef WITH_AI2_LEARNING
/**********************************************************/
/* Find the average cell's value for a territory
 */
static unsigned FindMeanValue(cell_type ** territory,unsigned len,int side)
{
    unsigned i;
    unsigned total=0;

    for(i=0;i<len;i++)
        total+=territory[i]->value[side];

    if(len)
        return total/len;

    return 0;
}
#endif


#ifdef WITH_AI2_LEARNING
/**********************************************************/
/* Main routine for learning
 */
static void LearnParameter(PlayerInfo * player,float mean,unsigned ter,int side)
{
    /*
     * TODO:
     *  -use a dynamic minimum reserve level to attack
     *  -use a dynamic min_att_str depending on how successful
     *   attacks are
     */

    if(player->previous_ter < ter)
    {
        /* Terrain is growing */
        if(player->min_mean_value > 0)
            player->min_mean_value-=0.1;
    }
    else
    {
        /* Terrain is reducing or stable */
        if(player->previous_ter == ter &&
           player->cur_max_attack == 0)
        {
            if(player->min_mean_value > 0)
                player->min_mean_value-=0.1;
        }
        else
        {
            if(player->min_mean_value < player->max_cell_value)
                player->min_mean_value+=0.1;
        }
    }
    player->previous_ter=ter;

    /* Update number of attack */
    if(mean > player->min_mean_value)
    {
        /* If possible increase number of attack */
        if(player->cur_max_attack < player->max_attack )
            player->cur_max_attack++;

        /* Decrease min_op_str */
        if(player->min_op_str > 1)
            player->min_op_str-=0.1;

    }
    else
    {
        /* If possible decrease number of attack */
        if(player->cur_max_attack > 0)
            player->cur_max_attack--;

        /* Increase min_op_str */
        if(player->min_op_str < 3)
            player->min_op_str+=0.2;
    }
}
#endif

/*******************************/
/*    The main AI function     */
/*******************************/

void ai_algo2(int side)
{
    cell_type * territory[MAX_TABLESIZE];
    cell_type * freeborder[MAX_TABLESIZE];
    cell_type * frontlist[MAX_TABLESIZE];
    cell_type * periphlist[MAX_TABLESIZE];
    cell_type * borderlist[MAX_TABLESIZE];
    cell_type ** tmp;

    cell_type * scan_cell;
    
    unsigned i,j;
#ifdef AI_STATUS_DEBUG
    unsigned x,y;
#endif
    unsigned ter,freeb,front,border,periph;

    int tmp_calc;

    PlayerInfo * currentplayer;
    
    /* For the attack routine */
    cell_type * best_attack;
    float value,best_value;
    AttackInfo * scan_attack;

    /* Get the current player info */
    currentplayer=playerlist[side];
    
    /*
     * We need to do this init here because the direction_count
     * is not set when init_algo2 is called.
     */
    if(currentplayer->first_turn)
    {
        currentplayer->max_cell_value=
            Board->shapes[side][Board->cells[0][0]->shape_index]->max_value;

        currentplayer->min_mean_value=currentplayer->max_cell_value/2;
        
        switch(Config->direction_count)
        {
        case 4 :
            /*dirset=dirset4;
             routeback=routeback4;*/
            routebackind=routebackind4;
            break;
        case 6 :
            /*dirset=dirset6;
             routeback=routeback6;*/
            routebackind=routebackind6;
            break;
        default:
            printf("Error - Ai algo 2 doesn't support this tile\n");
            exit(0);
        }
    }

    /* Find the territory for this player */
    ter=FindTerritoryStatus(territory,side);
    
    /* Find Free border cells and border cells */
    freeb=FindFreeBorder(freeborder,side);
    border=FindBorder(borderlist,side);
    front=FindFront(frontlist,territory,ter,side);
    periph=FindPeriph(periphlist,territory,ter,side);


    /* On first turn always scan perimeter and Reserve */
    if(currentplayer->first_turn)
    {
        for(i=0;i<ter;i++)
            ScanReserve(territory[i],0,side);
        ScanPerimeter(freeborder,freeb,side);
        currentplayer->first_turn=0;
    }

    /***** Scan reserve for each front cell *****/
    for(i=0;i<periph;i++)
    {
        ScanReserve(periphlist[i],0,side);
    }

    /***** Run the defensive routine - the only permanent routine *****/
    for(i=0;i<front;i++)
    {
        /* Scan the front */
        scan_cell=frontlist[i];
        
        /* Cell is attacked remove leaving routes except attack supplies */

        /* Register the attacked cell */
        
        /* Monitor defense evolution */

        /* Low reserve for defense surrender on this cell */

        /* Low attacking force, counter attack */

        /* High reserve around, call for reenforcement */
        
        
        /* Remove leaving routes except:
         *     -Route to a target
         *     -Attack supplies
         *     -Last feeding routes
         */
        if(scan_cell->ai2->route_status[side] != C_ATTACK_SUPPLY &&
           scan_cell->ai2->route_status[side] != C_ROUTE_TO_TGT )
            for(j=0;j<Config->direction_count;j++)
                if(scan_cell->connect[j]->ai2->feed > 1 ||
                   (scan_cell->connect[j]->side != side &&
                    scan_cell->connect[j]->side != SIDE_FIGHT))
                {
                    scan_cell->connect[j]->ai2->feed--;
                    set_move_off(scan_cell,dirset[j],0);
                    draw_multiple_cell (scan_cell);
                }
    }
    
    /***** Run the Reorganisation routine  - can take several cycles *****/
    if( currentplayer->orga_count > currentplayer->orga_rate)
    {
        if(currentplayer->level_size)
        {
            currentplayer->level_size=
                FindNextLevel(currentplayer->l1,currentplayer->level_size,
                              currentplayer->l2,currentplayer->cur_level,side);
            if(!(currentplayer->level_size))
            {
                currentplayer->orga_count=0;
                /* Try to remove useless route when all levels are connected */
                FindDoubleRoute(territory,ter);
            }

            ConnectLevel(currentplayer->l2,currentplayer->level_size,
                         currentplayer->cur_level,side);
            tmp=currentplayer->l1;
            currentplayer->l1=currentplayer->l2;
            currentplayer->l2=tmp;
            currentplayer->cur_level++;
        }
        else
        {
            ResetLevel(side);
            
            /* Connect the first level */
            currentplayer->level_size=
                FindNextLevel(frontlist,front,currentplayer->level1,2,side);
            ConnectLevel(currentplayer->level1,
                         currentplayer->level_size,2,side);
            currentplayer->l1=currentplayer->level1;
            currentplayer->l2=currentplayer->level2;;
            currentplayer->cur_level=3;
        }
    }

    /***** Run the development routine *****/
    if(freeb && currentplayer->devel_count > currentplayer->devel_rate)
    {
        /* Compute the minimum reserve needed to extend */
        tmp_calc=
            currentplayer->max_cell_value*currentplayer->extend_ratio;
        
        /* Look for valuable free cells */
        ScanPerimeter(freeborder,freeb,side);
        /* Move to valuable free cells*/
        for(i=0;i<freeb;i++)
        {
            if(freeborder[i]->ai2->tgt_status[side] == C_DIRECT_TGT ||
               freeborder[i]->ai2->route_status[side] == C_ROUTE_TO_TGT ||
               freeborder[i]->ai2->tgt_status[side] == C_INDIRECT_TGT
              )
                MoveTo(freeborder[i],side,0);
            else
                MoveTo(freeborder[i],side,tmp_calc);

        }
        currentplayer->devel_count=0;
    }

    /***** Run the attack routine *****/
    if(border &&
       currentplayer->attack_count > currentplayer->attack_rate)
    {
        /* Try to use offensive options */
        if (Config->enable[OPTION_SCUTTLE][side])
            for(i=0;i<ter;i++)
            {
                scan_cell=territory[i];

                if(scan_cell->angle > 0
#ifdef WITH_BASE_SIDE
                   &&
                   scan_cell->base_side != side &&
                   scan_cell->base_side != -1
#endif
		   )
                    run_scuttle (scan_cell);
            }

        /*
         * TODO:
         *   - Use a better way of detecting if a new ScanEnemy is needed
         */

        /* If needed reset Attack route status and Find new routes */
        if(currentplayer->cur_attack == 0)
        {
            ResetAttackRoute(side);
            ScanEnemy(borderlist,border,side);
        }

#ifdef WITH_AI2_LEARNING
        /* Learning new parameters */
        LearnParameter(currentplayer,
                       (float)FindMeanValue(territory,ter,side),
                       ter,
                       side);
#endif

#ifdef AI_LEARNING_DEBUG
        fprintf(stderr,"mean:%u cur:%u/%u min:%.2f opstr:%.2f(%u/%u)\n",
                FindMeanValue(territory,ter,side),
                currentplayer->cur_attack,
                currentplayer->cur_max_attack,
                currentplayer->min_mean_value,
                currentplayer->min_op_str,
                currentplayer->cur_op_attack,
                currentplayer->max_op_attack);
#endif

        /* Check if player can launch new attack */
        if(currentplayer->cur_attack < currentplayer->cur_max_attack)
        {
            best_value=0;
            best_attack=NULL;

            /* Look for the most valuable route to attack
             * aiming at Target not already under attack
             */
            for(i=0;i<border;i++)
            {
                scan_cell=borderlist[i];
                /* Look at Direct target */
                if(scan_cell->ai2->tgt_status[side] == C_DIRECT_ATTACK &&
                   CheckDirectAttack(scan_cell,0,side))
                {
                    value=AttackValue(scan_cell,side);
                    if(value > best_value)
                    {
                        best_value=value;
                        best_attack=scan_cell;
                    }
                }
                /* Look at distant target */
                if(scan_cell->ai2->route_status[side] == C_ROUTE_TO_ATTACK &&
                   scan_cell->ai2->route_tgt[side]->ai2->tgt_status[side] == C_INDIRECT_ATTACK &&
                   CheckIndirectAttack(scan_cell,0,side))
                {
                    value=AttackValue(scan_cell,side);
                    if(value > best_value)
                    {
                        best_value=value;
                        best_attack=scan_cell;
                    }
                }
            }
            /* Launch the most valuable attack */
            if(best_attack)
            {
                scan_attack=&(currentplayer->attack_list[currentplayer->cur_attack]);

                scan_attack->target=best_attack;
                scan_attack->timer=0;
                scan_attack->previous=~0;
                /* Mark target as under attack */
                if(best_attack->ai2->tgt_status[side] == C_DIRECT_ATTACK)
                {
                    best_attack->ai2->tgt_status[side]=C_TGT_UNDER_ATTACK;
                    scan_attack->final_target=best_attack;
                }
                else
                {
                    best_attack->ai2->route_tgt[side]->ai2->tgt_status[side]=C_TGT_UNDER_ATTACK;
                    scan_attack->final_target=best_attack->ai2->route_tgt[side];
                }
                scan_attack->source=Attack(best_attack,0,side);

                currentplayer->cur_attack++;
            }
        }
        
        /* Check if player can launch new opportunity attack */
        if(currentplayer->cur_op_attack < currentplayer->max_op_attack)
        {
            for(i=0;i<border;i++)
            {
                scan_cell=borderlist[i];
                
                if(currentplayer->cur_op_attack < currentplayer->max_op_attack &&
                   scan_cell->ai2->tgt_status[side] != C_TGT_UNDER_ATTACK &&
                   scan_cell->ai2->status[side] == C_BORDER &&
                   CheckOpAttack(scan_cell,0,side))
                {
                    scan_attack=&(currentplayer->op_attack_list[currentplayer->cur_op_attack]);

                    scan_attack->target=scan_cell;
                    scan_attack->timer=0;
                    scan_attack->previous=~0;
                    scan_attack->final_target=scan_cell;

                    scan_cell->ai2->tgt_status[side]=C_TGT_UNDER_ATTACK;

                    scan_attack->source=Attack(scan_cell,0,side);
                    currentplayer->cur_op_attack++;
                }
            }
        }

        currentplayer->attack_count=0;
    }

    /* Monitor attacks evolution */
    for(i=0;i<currentplayer->cur_attack;i++)
    {
        /* Note: Each time an attack is canceled or completed,
         *       the last attack in the list is moved and
         *       skipped. We could work arround that problem.
         *       Later...
         */

        scan_attack=&(currentplayer->attack_list[i]);

        /* Abort attack with little chance of success */
        if(AbortAttack(scan_attack,0,side))
        {

#ifdef AI_ATTACK_DEBUG
            fprintf(stderr,"abort\n");
#endif
            UnAttack(scan_attack->target,side);
            scan_attack->final_target->ai2->tgt_status[side]=C_DULL;

            /* Copy last attack in place of the canceled one */
            tmp_calc=currentplayer->cur_attack-1;

            if(tmp_calc != i)
                memcpy(scan_attack,
                       &(currentplayer->attack_list[tmp_calc]),
                       sizeof(AttackInfo));

            currentplayer->cur_attack--;

        } else {
            /* Change attack route if a better route is found */

            /*
             * TODO
             */

            /* Maintain successful attacks */
            if(scan_attack->target != scan_attack->final_target &&
               scan_attack->target->side == side)
            {
                /* Look for next cell to attack */
                for(j=0;j<Config->direction_count;j++)
                {
                    scan_cell=scan_attack->target->connect[j];
                    if(scan_cell == scan_attack->final_target ||
                       (scan_cell->ai2->route_status[side] == C_ROUTE_TO_ATTACK &&
                        scan_cell->ai2->route_tgt[side] == scan_attack->final_target))
                    {
                        /* update attack & suspend attack if reserves are low */
                        if(scan_attack->source=Attack(scan_cell,0,side))
                        {
                            /* Reset the abort timer */
                            scan_attack->timer=0;
                            scan_attack->target=scan_cell;
                        }
#ifdef AI_ATTACK_DEBUG
                        else
                            fprintf(stderr,"suspend\n");
#endif
                        /*
                         * DO we need the following break ???
                         */
#if 0
                        break;
#endif
                    }
                }
            }

            /* Clear completed attack */
            if(scan_attack->final_target->side == side)
            {
                scan_attack->final_target->ai2->tgt_status[side]=C_DULL;
                /* Set the feed value for the conquered cell */
                scan_attack->final_target->ai2->feed=scan_attack->source;
                    
                /* Copy last attack in place of the completed one */
                tmp_calc=currentplayer->cur_attack-1;
                if(tmp_calc != i)
                    memcpy(scan_attack,
                           &(currentplayer->attack_list[tmp_calc]),
                           sizeof(AttackInfo));

                currentplayer->cur_attack--;
            }
        }
    }

    /* Monitor opportunity attacks evolution */
    for(i=0;i<currentplayer->cur_op_attack;i++)
    {
        scan_attack=&(currentplayer->op_attack_list[i]);

        /* Abort attack with little chance of success */
        if(AbortOpAttack(scan_attack,0,side))
        {
            UnAttack(scan_attack->target,side);
            scan_attack->final_target->ai2->tgt_status[side]=C_DULL;

            /* Copy last attack in place of the canceled one */
            tmp_calc=currentplayer->cur_op_attack-1;

            if(tmp_calc != i)
                memcpy(scan_attack,
                       &(currentplayer->op_attack_list[tmp_calc]),
                       sizeof(AttackInfo));

#ifdef WITH_AI2_LEARNING
            /* Slowly increase the minimum attack ratio */
            if(currentplayer->min_op_str < 3)
                currentplayer->min_op_str+=0.2;
#endif
            
            currentplayer->cur_op_attack--;

        } else {
            /* Clear completed attack */
            if(scan_attack->final_target->side == side)
            {
                scan_attack->final_target->ai2->tgt_status[side]=C_DULL;
                /* Set the feed value for the conquered cell */
                scan_attack->final_target->ai2->feed=scan_attack->source;

                /* Copy last attack in place of the completed one */
                tmp_calc=currentplayer->cur_op_attack-1;
                if(tmp_calc != i)
                    memcpy(scan_attack,
                           &(currentplayer->op_attack_list[tmp_calc]),
                           sizeof(AttackInfo));
#ifdef WITH_AI2_LEARNING
                /* Slowly decrease the minimum attack ratio */
                if(currentplayer->min_op_str > 1)
                    currentplayer->min_op_str-=0.2;
#endif

                currentplayer->cur_op_attack--;
            }
        }
    }

    ResetStatus(side);

    currentplayer->orga_count++;
    currentplayer->devel_count++;
    currentplayer->attack_count++;


#ifdef AI_STATUS_DEBUG
    for(y=0;y<Config->board_y_size;y++)
    {
        for(x=0;x<Config->board_x_size;x++)
        {
            if(Board->cells[y][x]->ai2->route_status[side]!=C_DULL)
                fprintf(stderr,"%u.",Board->cells[y][x]->ai2->route_status[side]);
            else
            {
                if(Board->cells[y][x]->ai2->tgt_status[side]!=C_DULL)
                    fprintf(stderr,"%u.",Board->cells[y][x]->ai2->tgt_status[side]);
                else
                    fprintf(stderr,"%u.",Board->cells[y][x]->status[side]);
            }
#ifdef AI_EXTRA_DEBUG
            fprintf(stderr,"%u.",Board->cells[y][x]->need_level[side]);
#endif
        }

        fprintf(stderr," ----- ");
        
        for(x=0;x<Config->board_x_size;x++)
            fprintf(stderr,"%u.",Board->cells[y][x]->side);

        fprintf(stderr,"\n");
    }
#endif

#if (defined(AI_STATUS_DEBUG) || defined(AI_ATTACK_DEBUG))
    fprintf(stderr,"****\n");
#endif

}
