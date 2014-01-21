#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
#endif

#include "constant.h"
#include "extern.h"


#define VICTORY_DEBUG 0

/*
 * TODO: more complex victories (alies, objectives)
 */

/* Check if a side has won the game */


/** File local variables **/
static int forces[MAX_SIDES];
static int lands[MAX_SIDES];
static int victoryflag[MAX_SIDES]; /* for the old algorithm */
static int victorytimer[MAX_SIDES];
static int total_forces, total_lands, map_lands;
static int use_traditional_victory;
static int total_time;


/** File local prototypes **/
static void calculate_sizes(void);
static int traditional_victory(void);


static void
calculate_sizes(void)
{
  int i, x, y, side, value;
  
  for (i=0; i<Config->side_count; i++) {
    forces[i]=0;
    lands[i]=0;
  }

  for (x=0;x<Config->board_x_size;x++)
    for (y=0;y<Config->board_y_size;y++) {
      side=Board->cells[y][x]->side;
      
      switch (side) {
      case SIDE_NONE:
	break;
      case SIDE_FIGHT:
	for (i=0; i<Config->side_count; i++) {
	  value = Board->cells[y][x]->value[i];
	  forces[i] += value;
	  if (value > 1) {
	    victoryflag[i]=1;
	  }
	}
	break;
      default:
        /* side is alone */
        /* P.Bureau - check if value > 1 */
        value = Board->cells[y][x]->value[side];
	forces[side] += value;
        lands[side]++;
        if (value > 1) {
          victoryflag[side]=1;
        }
	break;
      }
    }

  total_lands=0;
  total_forces=0;
  for (i=0; i<Config->side_count; i++) {
    total_forces += forces[i];
    total_lands += lands[i];
  }
}


static int
traditional_victory(void) {
  int i, tmp_calc, winner;
  
  winner=0;
  tmp_calc=0;
  
  for (i=0;i<Config->side_count;i++)
    if (victoryflag[i]) {
      tmp_calc++;
      victoryflag[i]=0;
      winner=i;
    }

  if (tmp_calc == 1)
    return winner;

  return SIDE_NONE;
}


void
victory_add_position(int side, short x, short y, int enable, char *string)
{
  int num;

  num = Config->victory_positions[side];

  if (enable)
  {
    if (num >= MAX_VICTORY_POSITIONS) {
      throw_error ("Too many victory positions", NULL);
    }
    Config->victory_position[side][num][0] = x;
    Config->victory_position[side][num][1] = y;
    Config->victory_positions[side]++;
	Config->victory_string_position[side][num] = string;
  }
  else
  {
    unsigned int old,new;

    /** Remove this position if it is already in the list **/
    for (old=0,new=0; old<num; old++)
    {
      if (Config->victory_position[side][old][0] == x &&
          Config->victory_position[side][old][1] == y)
	continue;
      Config->victory_position[side][old][0] = Config->victory_position[side][new][0];
      Config->victory_position[side][old][1] = Config->victory_position[side][new][1];
      Config->victory_string_position[side][old] = Config->victory_string_position[side][new];
      new++;
    }
    Config->victory_positions[side] = new;
  }
}


void
victory_defaults(void)
{
  int side;

  for (side=0; side<MAX_SIDES; side++)
    Config->victory_positions[side] = 0; /* start with no positions */
}


void
victory_init(void)
{
  int i;

  use_traditional_victory=0;
  for (i=0;i<Config->side_count;i++) {
    Config->victory_traditional[i] =
      Config->enable[OPTION_VICTORY_TRADITIONAL][i];
    if (Config->victory_traditional[i])
      use_traditional_victory = 1;
    Config->victory_timeout[i] =
      Config->value_int[OPTION_VICTORY_TIMEOUT][i];
    Config->victory_army_ratio[i] =
      Config->value_double[OPTION_VICTORY_ARMY_RATIO][i] / 100.0;
    Config->victory_land_ratio[i] =
      Config->value_double[OPTION_VICTORY_LAND_RATIO][i] / 100.0;
  }
  
  total_time=0;
  map_lands=Config->board_x_size * Config->board_y_size;
  /* printf("map_lands: %d\n", map_lands); */
}


void
victory_check(void)
{
  int winner, side, i, n, npos, x, y;
  int have_winner_now, is_winner_now;
  int winners[MAX_SIDES];
  char *winner_string=NULL;

  is_winner_now=0;


  total_time++;
  
  for (side=0; side<Config->side_count; side++)
    winners[side]=0;
  
  /* calculate the board standings */
  calculate_sizes();
  
  if (use_traditional_victory) {
    winner=traditional_victory();
    if (winner != SIDE_NONE && Config->victory_traditional[winner]) {
      winners[winner]=1;
      if (VICTORY_DEBUG) fprintf(stderr, "traditional winner: %d\n", winner);
    }
  }
  
  have_winner_now=0;
  for (side=0; side<Config->side_count; side++) {

    /* player wins if she owns a ratio of all armies alive */
    if (Config->victory_army_ratio[side] != 0.0) {
      /* fprintf(stderr, "%d %d %f %f\n",
	      forces[side], total_forces,
	      forces[side]/(double)total_forces,
	      Config->victory_army_ratio[side]); */
      if (forces[side]/(double)total_forces >
	  Config->victory_army_ratio[side]){
	winners[side]=1;
	winner_string=Config->victory_string_army[side];
	if (VICTORY_DEBUG) fprintf(stderr, "army ratio winner: %d\n", side);
      }
    }

    /* player wins if she occupies a ratio of land */
    if (Config->victory_land_ratio[side] != 0.0) {
      /* fprintf(stderr, "%f %f\n",
		 lands[side]/(double)map_lands,
		 Config->victory_land_ratio[side]); */
      if (lands[side]/(double)map_lands >
		  Config->victory_land_ratio[side]) {
		winners[side]=1;
		winner_string=Config->victory_string_land[side];
		if (VICTORY_DEBUG) fprintf(stderr, "land ratio winner: %d\n", side);
      }
    }

    /* player wins after a time limit */
    if (Config->victory_timeout[side]) {
      /*fprintf(stderr, "%d %d\n",
	Config->victory_timeout[side], total_time);*/
      if (Config->victory_timeout[side] <= total_time) {
	winners[side]=1;
	winner_string=Config->victory_string_timeout[side];
	if (VICTORY_DEBUG) fprintf(stderr, "timeout winner: %d\n", side);
      }
    }
      
    /* player wins if she has all victory positions */
    n=Config->victory_positions[side];
    if (n) {
      npos=0;
      for (i=0; i < n; i++) {
		x=Config->victory_position[side][i][0];
		y=Config->victory_position[side][i][1];
		if (Board->cells[y][x]->side == side) {
		  npos++;
		  winner_string=Config->victory_string_position[side][i];
		}
      }
      if (npos == n || (npos>0 && Config->enable[OPTION_VICTORY_POSITION_ANY][side])) {
		winners[side]=1;
		if (VICTORY_DEBUG) fprintf(stderr, "position winner: %d\n", side);
      }
    }
    
    /* if player is winning, increase wictorytimer */
    if (winners[side]) {
      victorytimer[side]++;
    } else {
      victorytimer[side]=0;
    }
    /* wait a period before finally declaring the winner */
    if (victorytimer[side] > Config->value_int[OPTION_VICTORY_WAIT][side]) {
      have_winner_now++;
      is_winner_now=side;
    }
  }
  
  if (have_winner_now) {
    /* if for example all players have the same timeout,
       we may not have a single winner */
    if (have_winner_now==1) {
	  if(winner_string != NULL && strlen(winner_string)>0) {
		printf("%d\n%s\n",is_winner_now,winner_string);
		exit(0);
	  } else
		exit(is_winner_now+10);
    } else {
      exit(9);
    }
  }
}

