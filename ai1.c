#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* Copyright 2000 - Mathias Broxvall */

/*
TODO: ANOTHER AI VALUE = ENEMY_STRENGTH PROPAGATED THROUGH ENEMY PIPES??
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
#include <math.h>

#include "constant.h"
#include "extern.h"

#include "ai1.h"
#include "ai1_if.h"

/***********************************************/


/** File local globals **/
static AIData aiData[MAX_PLAYERS];
static int currentSide;


/** File local prototypes **/
static void propagate_positive_ai_value(int flags,cell_type *cell,int valueKind,
		double value,double fallof,int steps);
static void propagate_negative_ai_value(int flags,cell_type *cell,int valueKind,
		double value,double fallof,int steps);
static void propagate_cleanup(int flags,cell_type *cell,int valueKind,int steps);
static void propagate_ai_value(int flags,cell_type *cell,int valueKind,double value,int radius);
static void graph_distance1(cell_type *source,int steps);
static int graph_distance(cell_type *source,cell_type *dest,int max_range);


/**********************************/
/* Propagation of positive values */
/**********************************/

static void propagate_positive_ai_value(int flags,cell_type *cell,int valueKind,
				 double value,double fallof,int steps) 
{
  double tmp_value;
  int i,rec;

  tmp_value = cell->ai1->values[valueKind] + value;
  if(cell->ai1->visited == 0 || tmp_value > cell->ai1->tmp_values[valueKind]) {
	cell->ai1->tmp_values[valueKind] = tmp_value;
	cell->ai1->visited = 1;
	value -= fallof;
	if(cell->level<0 && !(flags & SEA_TO)) return;
	else if(cell->level>=0 && !(flags & LAND_TO)) return;
	if(flags & DECAY_OWN_STRENGTH_INVERSE) {
	  value *= 1.0 - cell->value[currentSide] / (double) Config->max_value[currentSide];
	  fallof *= 1.0 - cell->value[currentSide] / (double) Config->max_value[currentSide];
	}
	if(steps > 0)
	  for(i=0;i<Config->direction_count;i++)
		if(cell->connect[i] && cell->connect[i] != cell) {
		  if(cell->level<0 && cell->connect[i]->level<0)
			rec = flags & SEA_TO_SEA;
		  else if(cell->level<0 && cell->connect[i]->level>=0)
			rec = cell->connect[i]->growth>=TOWN_MIN ? flags&SEA_TO_HARBOUR : flags&SEA_TO_LAND;
		  else if(cell->level>=0 && cell->connect[i]->level<0)
			rec = cell->growth>=TOWN_MIN ? flags&HARBOUR_TO_SEA : flags&LAND_TO_SEA;
		  else if(cell->level>=0 && cell->connect[i]->level>=0)
			rec = flags & LAND_TO_LAND;
		  if(rec)
			propagate_positive_ai_value(flags,cell->connect[i],valueKind,value,fallof,steps-1);
		}	
  }
}

/**********************************/
/* Propagation of negative values */
/**********************************/

static void propagate_negative_ai_value(int flags,cell_type *cell,int valueKind,
				 double value,double fallof,int steps) 
{
  double tmp_value;
  int i,rec;

  tmp_value = cell->ai1->values[valueKind] + value;
  if(cell->ai1->visited == 0 || tmp_value < cell->ai1->tmp_values[valueKind]) {
	cell->ai1->tmp_values[valueKind] = tmp_value;
	cell->ai1->visited = 1;
	if(cell->level<0 && !(flags & SEA_TO)) return;
	else if(cell->level>=0 && !(flags & LAND_TO)) return;
	value -= fallof;
	if(flags & DECAY_OWN_STRENGTH_INVERSE) {
	  value *= 1.0 - cell->value[currentSide] / (double) Config->max_value[currentSide];
	  fallof *= 1.0 - cell->value[currentSide] / (double) Config->max_value[currentSide];
	}
	if(steps > 0)
	  for(i=0;i<Config->direction_count;i++)
		if(cell->connect[i] && cell->connect[i] != cell) {
		  if(cell->level<0 && cell->connect[i]->level<0)
			rec = flags & SEA_TO_SEA;
		  else if(cell->level<0 && cell->connect[i]->level>=0)
			rec = cell->connect[i]->growth>=TOWN_MIN ? flags&SEA_TO_HARBOUR : flags&SEA_TO_LAND;
		  else if(cell->level>=0 && cell->connect[i]->level<0)
			rec = cell->growth>=TOWN_MIN ? flags&HARBOUR_TO_SEA : flags&LAND_TO_SEA;
		  else if(cell->level>=0 && cell->connect[i]->level>=0)
			rec = flags & LAND_TO_LAND;
		  if(rec)
			propagate_negative_ai_value(flags,cell->connect[i],valueKind,value,fallof,steps-1);
		}
  }
}

/***********************/
/*     Cleaning up     */
/***********************/

static void propagate_cleanup(int flags,cell_type *cell,int valueKind,int steps) {
  int i,rec;

  if(cell->ai1->visited) {
	cell->ai1->visited = 0;
	if(valueKind == -1)
	  cell->ai1->dist = 0;
	else
	  cell->ai1->values[valueKind] = cell->ai1->tmp_values[valueKind];
	if(steps > 0)
	  for(i=0;i<Config->direction_count;i++)
		if(cell->connect[i] && cell->connect[i] != cell) {
		  if(cell->level<0 && cell->connect[i]->level<0)
			rec = flags & SEA_TO_SEA;
		  else if(cell->level<0 && cell->connect[i]->level>=0)
			rec = cell->connect[i]->growth>=TOWN_MIN ? flags&SEA_TO_HARBOUR : flags&SEA_TO_LAND;
		  else if(cell->level>=0 && cell->connect[i]->level<0)
			rec = cell->growth>=TOWN_MIN ? flags&HARBOUR_TO_SEA : flags&LAND_TO_SEA;
		  else if(cell->level>=0 && cell->connect[i]->level>=0)
			rec = flags & LAND_TO_LAND;
		  if(rec)			
			propagate_cleanup(flags,cell->connect[i],valueKind,steps-1);
		}
  }
}

/*******************************************/
/* Main entrypoint of propagation routines */
/*******************************************/

static void propagate_ai_value(int flags,cell_type *cell,int valueKind,double value,int radius) {

    if(value > 0)
        propagate_positive_ai_value(flags,cell,valueKind,value,value/radius,radius);
    else
        propagate_negative_ai_value(flags,cell,valueKind,value,value/radius,radius);
    propagate_cleanup(flags,cell,valueKind,radius);
}

/********************************************************/
/* graph_distance: Finds the distance between two cells */
/********************************************************/

static void graph_distance1(cell_type *source,int steps) {
  int i,nd;
  cell_type *cell;

  nd = source->ai1->dist + 1;
  source->ai1->visited = 1;
  if(steps == 0) return;

  for(i=0;i<Config->direction_count;i++) {
    cell = source->connect[i];
    if(cell == NULL || cell == source) continue;
    if(cell->ai1->dist == 0 || nd < cell->ai1->dist) {
      cell->ai1->dist = nd;
      graph_distance1(cell,steps-1);
    }
  }
}

static int graph_distance(cell_type *source,cell_type *dest,int max_range) {
  int d;

  source->ai1->dist = 1;
  source->ai1->visited = 1;
  graph_distance1(source,max_range+1);
  d = dest->ai1->dist - 1;
  propagate_cleanup(ANY_TO_ANY,source,-1,max_range);
  return d;
}


/********************************/
/* Init function for ai algo 1  */
/********************************/

int init_algo1(int side,float skill)
{
    int width,height,x,y,i;
    cell_type *cell;

    aiData[side].skillLevel = skill * 1.0;
    width = Config->board_x_size;
    height = Config->board_y_size;

    for(x=0;x<width;x++)
        for(y=0;y<height;y++) {
            cell = Board->cells[y][x];
            if(!cell) continue;
            for(i=0;i<NUM_AI_VALUES;i++)
                cell->ai1->feedback_values[side][i] = 0.0;
        }

    aiData[side].new_num_gunnery_targets = 0;

    return 0;
}


/********************************/
/* Free function for ai algo 1  */
/********************************/

void free_algo1(int side)
{
    /** Nothing to free **/
}


/*******************************/
/*    The main AI function     */
/*******************************/

/*static int tick=0;*/

void ai_algo1(int side) {

  int x,y,i,j,k,outgoing,range,X,Y;
  int strength,enemy_strength,owner,this_strength,total_strength;
  cell_type *cell,*cell2;

  int width,height;
  int dir[MAX_DIRECTIONS];
  int num_actions;

  double best_preference,this_preference,total_preference,threat,f1;
  cell_type *source_cell, *target_cell;
  int direction;
  int movement_kind;
  char flag1,flag2;

  if(aiData[side].skillLevel == 0) return;
  num_actions=(int)aiData[side].skillLevel;
  if(aiData[side].skillLevel - num_actions > get_random(100)/100.0) num_actions++;
  if(num_actions == 0) return;

  currentSide = side;
  width = Config->board_x_size;
  height = Config->board_y_size;
  
  /* Distinguish between new and old gunnery targets */  
  aiData[side].num_gunnery_targets = aiData[side].new_num_gunnery_targets;
  aiData[side].new_num_gunnery_targets = 0;
  for(i=0;i<MAX_GUNNERY_TARGETS;i++)
    for(j=0;j<3;j++)
      aiData[side].gunnery_targets[i][j] = aiData[side].new_gunnery_targets[i][j];
  
  for(x=0;x<width;x++)
    for(y=0;y<height;y++) {
      cell = Board->cells[y][x];
      cell->ai1->visited = 0;
      cell->ai1->already_moved = 0;
      cell->ai1->dist = 0;
      for(i=0;i<NUM_AI_VALUES;i++) {
		cell->ai1->values[i] = cell->ai1->feedback_values[side][i];
		cell->ai1->feedback_values[side][i] = cell->ai1->tmp_values[i] = 0.0;
      }
	}


  for(x=0;x<width;x++)
    for(y=0;y<height;y++) {
      cell = Board->cells[y][x];

      /* Handle incomplete knowledge :-) */
      if(!cell->seen[side]) {
		propagate_ai_value(AMPHIBIOUS_LS | DECAY_NORMAL,cell,TROOP_VALUE,1.0,4);
		continue;
      }

      if(cell->level > 0)
		/* Make high hills valuable */
		propagate_ai_value(LAND_TO_LAND | DECAY_NORMAL,cell,CELL_VALUE,5.0*cell->level*cell->level,2);

      strength = 0; owner = -1;
      for(i=0;i<Config->side_count;i++)
		if(cell->value[i] > strength) {
		  strength = cell->value[i];
		  owner = i;
		}

      if(strength == 0) {
		/* Cell is unoccupied */

		if(cell->growth >= TOWN_MIN) {
		  /* It's an empty town */
		  propagate_ai_value(AMPHIBIOUS_LS|DECAY_NORMAL,cell,TROOP_VALUE,50.0,15);
		  propagate_ai_value(AMPHIBIOUS_LS|DECAY_NORMAL,cell,TROOP_VALUE,50.0,10);
		  propagate_ai_value(AMPHIBIOUS_LS|DECAY_NORMAL,cell,TROOP_VALUE,50.0,5);
		  propagate_ai_value(AMPHIBIOUS_LS|DECAY_NORMAL,cell,TROOP_VALUE,50.0,2);

		  propagate_ai_value(HARBOUR_TO_SEA|SEA_TO_SEA|SEA_TO_HARBOUR|DECAY_NORMAL,cell,TROOP_VALUE,90.0,18);
		  propagate_ai_value(HARBOUR_TO_SEA|SEA_TO_SEA|SEA_TO_HARBOUR|DECAY_NORMAL,cell,TROOP_VALUE,160.0,8);
		  propagate_ai_value(HARBOUR_TO_SEA|SEA_TO_SEA|SEA_TO_HARBOUR|DECAY_NORMAL,cell,TROOP_VALUE,60.0,3);
		  propagate_ai_value(HARBOUR_TO_SEA|SEA_TO_SEA|SEA_TO_HARBOUR|DECAY_NORMAL,cell,TROOP_VALUE,50.0,2);
		} else if(cell->growth > 0) {
		  /* It's an empty farm */
		  if(cell->level >= 0) {
			propagate_ai_value(LAND_TO_LAND | DECAY_NORMAL,cell,TROOP_VALUE,cell->growth*1.0+1.0,4);
			cell->ai1->values[TROOP_VALUE] += cell->growth*1.0 + 1.0;
			}
		} else {
		  if(cell->level >= 0) {
			/* It's an empty land hex, not a farm. */
			for(j=0;j<Config->direction_count;j++)
			  if(cell->dir[j]) break;
			if(j == Config->direction_count)			
			  cell->ai1->values[TROOP_VALUE] -= 1.0 + 10.0 * Config->value_double[OPTION_DECAY][side];
		  } else {
			/* It's a sea squarex */
			for(j=0;j<Config->direction_count;j++)
			  if(cell->dir[j]) break;
			if(j == Config->direction_count)			
			  cell->ai1->values[TROOP_VALUE] -= 5.0;
		  }
		}
      } else if(owner == side) {
		/* AI is the dominant owner */

		/* From here on, strength represent total strength
		   of other sides in cell */
		for(i=0,strength=0;i<Config->side_count;i++)
		  if(i!=side) strength += cell->value[i];
	
		if(strength > 0) {
		  /* Cell is under attack (or we are winning an attack) */
		} else {
		  /* Cell owned by AI */

		  /* Count outgoing vectors */
		  for(j=0,outgoing=0;j<Config->direction_count;j++)
			if(cell->dir[j]) outgoing++;
	  
		  /* Cells with many outgoing vectors have an increased troop_value */
		  /*cell->ai1->values[TROOP_VALUE] += outgoing*outgoing*outgoing * 10.0;*/

		  /* The cellvalue of a cell is decreased when many troops are present, this is in
			 order to evacuate crowded cells */
		  f1 = ((cell->value[side] * 1.0 / Config->max_value[side]) - 0.5) / 0.5;
		  if(f1 > 0)
			cell->ai1->values[TROOP_VALUE] -= 10.0 / (1.01-f1); /*exp(pow(f1,5.0)*15.0);*/

		  /* Cells where we are strong are not threaned */
		  propagate_ai_value(LAND_TO_LAND | DECAY_NORMAL,cell,THREAT,-2.0 * f1,2);

		  if(cell->growth >= TOWN_MIN) {
			/* It's a city owned by us alone */
			
			/* All cities should have a reserve of about 20% */
			if(cell->value[side] < Config->max_value[side] * 0.2)
			  cell->ai1->values[TROOP_VALUE] += 100.0 * (Config->max_value[side] * 0.2 - cell->value[side]);
			
			/* Owned towns have a high value */
			propagate_ai_value(LAND_TO_LAND | DECAY_NORMAL,cell,CELL_VALUE,8.0,3);
		  } else if(cell->growth > 0) {
			/* It's a farm */
			/*propagate_ai_value(LAND_TO_LAND | DECAY_NORMAL,cell,CELL_VALUE,0.5,2);*/
		  }
		}
      } else {
		/* Someone else is the dominant owner */

		if(cell->growth >= TOWN_MIN) {
		  /* It's an enemy city */

		  /* Enemy towns should be occupied */
		  propagate_ai_value(LAND_TO_LAND|DECAY_NORMAL,cell,TROOP_VALUE,50.0,5);
		  propagate_ai_value(AMPHIBIOUS_LS|DECAY_NORMAL,cell,TROOP_VALUE,50.0,15);
		  propagate_ai_value(AMPHIBIOUS_LS|DECAY_NORMAL,cell,TROOP_VALUE,50.0,10);
		  propagate_ai_value(AMPHIBIOUS_LS|DECAY_NORMAL,cell,TROOP_VALUE,50.0,6);
		  propagate_ai_value(AMPHIBIOUS_LS|DECAY_NORMAL,cell,TROOP_VALUE,40.0,4);
		  propagate_ai_value(AMPHIBIOUS_LS|DECAY_NORMAL,cell,TROOP_VALUE,20.0,2);
		  /*propagate_ai_value(HARBOUR_TO_SEA|SEA_TO_SEA|SEA_TO_HARBOUR|DECAY_NORMAL,cell,TROOP_VALUE,200.0,30);*/
		  propagate_ai_value(HARBOUR_TO_SEA|SEA_TO_SEA|SEA_TO_HARBOUR|DECAY_NORMAL,cell,TROOP_VALUE,100.0,10);
		  /*propagate_ai_value(HARBOUR_TO_SEA|SEA_TO_SEA|SEA_TO_HARBOUR|DECAY_NORMAL,cell,TROOP_VALUE,100.0,3);*/
		  cell->ai1->values[TROOP_VALUE] += 20.0;
		  
		  /* They are also threatening */
		  /*propagate_ai_value(LAND_TO_LAND|DECAY_OWN_STRENGTH_INVERSE,cell,THREAT,2.0,4);*/

		  /* Enemies constitute a threat */
		  propagate_ai_value(AMPHIBIOUS_SL|DECAY_OWN_STRENGTH_INVERSE,
							 cell,THREAT,strength*4.0/Config->max_value[side],4);
		} else {
		  /* It's a normal enemy occupied hex */
		  
		  /* Weak enemy cells should be attacked */
		  /* hmmm... should this be measred against the AI's maxvalue or the owners? */
		  if(strength < Config->max_value[side] * 0.2)
			cell->ai1->values[TROOP_VALUE] += 20.0;

		  /* All enemy cells should be attacked, but not as important... hmm good idea? */
		  propagate_ai_value(LAND_TO_LAND|SEA_TO_SEA|DECAY_NORMAL,cell,TROOP_VALUE,strength*2.0/Config->max_value[side]+2.0,3);
	    
		  /* Enemies constitute a threat */
		  propagate_ai_value(AMPHIBIOUS_SL|DECAY_OWN_STRENGTH_INVERSE,
							 cell,THREAT,strength*4.0/Config->max_value[side],3);
		}
      }
    }

  /* Handle threats. */
  for(x=0;x<width;x++)
    for(y=0;y<height;y++) {
      cell = Board->cells[y][x];

      /* Limit the threats into none or 1-10 */
      threat = cell->ai1->values[THREAT];
      if(threat > 0.0) threat += 1.0;
      if(threat > 10.0) threat = 10.0;

      /* Threatened cells should increase probability of concentrating troops */
      /*      cell->ai1->values[TROOP_VALUE] += cell->value[side]*5.0/Config->max_value[side] * threat; */
	  
      /* In the intersection of valuable and threatened cells
		 the troop value should be increased */
      cell->ai1->values[TROOP_VALUE] += cell->ai1->values[CELL_VALUE] * threat * 10.0;
    }

#ifdef DEBUG_AI_1
  printf("Cell values\n");
  for(y=0;y<height;y++) {
    for(x=0;x<width;x++) {
	  cell = Board->cells[y][x];
	  printf("  %2.2f",cell->ai1->values[CELL_VALUE]);
	}
    printf("\n");
	}

  printf("Threat values\n");
  for(y=0;y<height;y++) {
    for(x=0;x<width;x++) {
	  cell = Board->cells[y][x];
	  printf("  %2.2f",cell->ai1->values[THREAT]);
	}
    printf("\n");
	}

  printf("Troop values\n");
  for(y=0;y<height;y++) {
    for(x=0;x<width;x++) {
	  cell = Board->cells[y][x];
	  printf("  %2.2f",cell->ai1->values[TROOP_VALUE]);
	}
    printf("\n");
	}
#endif
  for ( ; num_actions > 0; num_actions--) {

    best_preference = 0.0;
    movement_kind = MOVEMENT_NONE; 
    X=Y=-1;
    for(x=0;x<width;x++)
      for(y=0;y<height;y++) {
		cell = Board->cells[y][x];

		if(cell->level < 0 && (!Config->enable_all[OPTION_HARBOURS])) continue;
	
		/*                                  */
		/* Consider shooting at some target */
		/*                                  */
		/*
#ifdef ENABLE_AI_GUNNERY
		if(cell->ai1->already_moved == 0 &&
		   Config->enable[OPTION_ARTILLERY][side] &&
		   cell->side == side) {
		  for(i=0;i<aiData[side].num_gunnery_targets;i++) {
			range = graph_distance(cell,
								   Board->cells[aiData[side].gunnery_targets[i][1]]
								   [aiData[side].gunnery_targets[i][0]],
								   Config->enable[OPTION_ARTILLERY][side]);
			if(range > 1 && cell->value[cell->side] > Config->value_int_all[OPTION_ARTILLERY_COST]) {
			  this_preference = (double) cell->value[side] * aiData[side].gunnery_targets[i][2] * 1.0;
			  if(this_preference > best_preference) {
				best_preference = this_preference;
				movement_kind = MOVEMENT_GUNNERY;
				X = aiData[side].gunnery_targets[i][0];
				Y = aiData[side].gunnery_targets[i][1];
				source_cell = cell;
			  }
			}
		  }		  
		  }
#endif
		
		/*                                  */
		/* Consider normal movement actions */
		/*                                  */

		if(cell->side == side)
		  for(j=0;j<Config->direction_count;j++) {
			cell2 = cell->connect[j];
			if(cell2 == cell || cell2 == NULL) continue;
			if(cell->ai1->already_moved) continue;
			if(cell->level < 0 && (!Config->enable_all[OPTION_HARBOURS])) continue;
			
			/* only consider moving to the see from *harbours* */
			if(cell2->level < 0 && cell->level>=0 && cell->growth < TOWN_MIN) continue;

			strength = 0; owner = -1;
			for(i=0;i<Config->side_count;i++)
			  if(cell2->value[i] > strength) {
				strength = cell2->value[i];
				owner = i;
			  }	    
			for(i=0,enemy_strength=0;i<Config->side_count;i++)
			  if(i!=side) enemy_strength += cell->value[i];
			
			if(cell2->ai1->values[TROOP_VALUE] > cell->ai1->values[TROOP_VALUE]) {

			  /* we don't do attacks here */
			  if(cell2->side != side && 
				 cell2->side != SIDE_NONE &&
				 cell2->side != SIDE_VOID) continue;

			  if(cell->dir[j] != 1) {
				
				/* If target is unoccupied or own cell use normal movement operations */
				this_preference = (cell2->ai1->values[TROOP_VALUE] - 
								   cell->ai1->values[TROOP_VALUE]) * (float) cell->value[side];

				if(this_preference > best_preference) {
				  /*		  printf("commiting to move from %d,%d to %d,%d with pref %f(%f)\n",
							  x,y,cell2->x,cell2->y,this_preference,best_preference); */
				  movement_kind = MOVEMENT_NORMAL_ON;
				  best_preference = this_preference;
				  source_cell = cell;
				  target_cell = cell2;
				  direction = j;		  
				} 
			  }
			} else if(cell->dir[j] != 0) {
			  this_preference = (cell->ai1->values[TROOP_VALUE] - cell2->ai1->values[TROOP_VALUE]) * 
				(Config->max_value[side] - cell->value[side]) * 2.0;
			  if(this_preference > best_preference) {
				movement_kind = MOVEMENT_NORMAL_OFF;
				best_preference = this_preference;
				source_cell = cell;
				target_cell = cell2;
				direction = j;
			  }
			}
		  }

		/*                                     */
		/* Now, consider assaults on this cell */
		/*                                     */
		
		strength = 0; owner = -1;
		for(i=0;i<Config->side_count;i++)
		  if(cell->value[i] > strength) {
			strength = cell->value[i];
			owner = i;
		  }	
		if(cell->side == side ||
		   cell->side == SIDE_NONE ||
		   cell->side == SIDE_VOID) continue;
		for(i=0,enemy_strength=0;i<Config->side_count;i++)
		  if(i!=side) enemy_strength += cell->value[i];
		if(enemy_strength == 0) continue;
		
		/* Add to the enemy strength for each enemy cell with incomming vectors here */
		for(j=0;j<Config->direction_count;j++) {
		  cell2 = cell->connect[j];
		  if(cell2 == cell || cell2 == NULL) continue;
		  for(k=0;k<Config->direction_count;k++)
			if(cell2->dir[k] && cell2->connect[k] == cell) break;	      
		  if(k == Config->direction_count) break;
		  if(cell2->side != side && 
			 cell2->side != SIDE_NONE &&
			 cell2->side != SIDE_VOID &&
			 cell2->side != SIDE_FIGHT)
			enemy_strength += 1.0 * cell2->value[cell2->side] + 0.2;
		}

		/* Calculate the odds of winning if we assult this cell, this is defined
		   as the sum of our strength in each neighbouring cell which has a preference
		   to attack this cell divided by the enemy strength in this cell. Neighbouring
		   cells which already has outgoing vectors get their strength halfed for these
		   calculations. 
		   Next, the preference of attack this cell is the sum of the cell's troop value 
		   difference of each neighbouring cell contributing to the attack (halfed for 
		   each other commitment) multiplied by the odds squared and scaled by a constant. 
		   !NOT TRUE ANYMORE!
		*/
	
		total_preference = 0;
		strength = 0;
		flag1 = false; /* are we already attacking cell1 ? */
		for(j=0;j<Config->direction_count;j++) {
		  cell2 = cell->connect[j];
		  if(cell2 == cell || cell2 == NULL) continue;
		  if(cell->level<0 && cell2->level >= 0 && cell2->growth<TOWN_MIN) continue;
		  this_strength = cell2->value[side];
		  if(this_strength <= 0) continue;
		  this_preference = cell->ai1->values[TROOP_VALUE] - cell2->ai1->values[TROOP_VALUE];
		  if(this_preference <= 0) continue;
		  
		  flag2 = true;	  
		  for(k=0;k<Config->direction_count;k++)
			if(cell2->dir[k] && cell2->connect[k] == cell) flag2 = false;
		  
		  /*	    if(cell2->dir[k]) {
					this_strength *= 0.75;
					this_preference *= 0.75;
					}
		  */
		  
		  strength += this_strength;
		  total_preference += this_preference;
		  flag1 |= flag2;
		}
		total_strength = strength;
		total_preference *= strength;
		strength = strength / enemy_strength;
		
		if(flag1 && strength > 0.6) {
		  total_preference *= strength * strength;
		  
		  if(total_preference > best_preference) {
			best_preference = total_preference;
			movement_kind = MOVEMENT_CHARGE;
			target_cell = cell;
			source_cell = NULL;
			direction = -1;
		  }
		  continue;
		} 
		
		if(flag1) {
		  /* Put a feedback on attacking this cell, ie. increase this cells value and
			 the surrounding cells by a factor proportinal to the likely hood that
			 we could win this battle (ie. our strength/their strength) */
		  
		  f1 = (total_preference / enemy_strength) * 10.0;
		  cell->ai1->feedback_values[side][TROOP_VALUE] += f1;
		  for(j=0;j<Config->direction_count;j++) {
			cell2 = cell->connect[j];
			if(cell2 == cell || cell2 == NULL) continue;
			cell2->ai1->feedback_values[side][TROOP_VALUE] += f1 * 0.5;
		  }
		  
		  /* Also, make this cell a potential target for gunnery attacks, if allowed, and
			 if we are not making the attack currently. */	  
		  if(strength < 0.6 &&
			 Config->enable[OPTION_ARTILLERY][side] && 
			 aiData[side].new_num_gunnery_targets < MAX_GUNNERY_TARGETS) {
			for(j=0;j<aiData[side].new_num_gunnery_targets;j++)
			  if(x == aiData[side].new_gunnery_targets[j][0] &&
				 y == aiData[side].new_gunnery_targets[j][1]) break;
			if(j == aiData[side].new_num_gunnery_targets) {
			  j = aiData[side].new_num_gunnery_targets++;
			  aiData[side].new_gunnery_targets[j][0] = x;
			  aiData[side].new_gunnery_targets[j][1] = y;
			  aiData[side].new_gunnery_targets[j][2] = f1;
			}
		  }
		}
		
		if(cell->side == SIDE_FIGHT && strength < 0.5) {
		  /* No point in attack if we are too weak. Preference of aborting attack
			 is proportional to the number of cells attacking this cell. (MODIFIED) */
		  total_preference = 0;
		  for(j=0;j<Config->direction_count;j++) {
			cell2 = cell->connect[j];
			if(cell2 == cell || cell2 == NULL) continue;
			if(cell2->value[side] == 0) continue;
			for(k=0;k<Config->direction_count;k++)
			  if(cell2->connect[k] == cell) break;	    
			if(cell2->dir[k])
			  total_preference += 1000000.0;
		  }
		  
		  if(total_preference > best_preference) {
			best_preference = total_preference;
			movement_kind = MOVEMENT_ABORT_ATTACK;
			target_cell = cell;
			source_cell = NULL;
			direction = -1;
		  }
		}
      }
	
    /*    printf("Action: %d, pref: %f\n",movement_kind,best_preference);  */
    if(movement_kind == MOVEMENT_NONE) {
      /* No action to be taken?? */
      /*      printf("no action to be taken for side %d\n",side); */
      return;
    }
    
    for(j=0;j<Config->direction_count;j++) dir[j] = 0;
    
    switch(movement_kind) {
    case MOVEMENT_NORMAL_ON:
      dir[direction]=1;
      set_move_on(source_cell,dir,0);
      source_cell->ai1->already_moved = 1;
      break;
    case MOVEMENT_NORMAL_OFF:
      dir[direction]=1;
      set_move_off(source_cell,dir,0);
      source_cell->ai1->already_moved = 1;
      break;
    case MOVEMENT_CHARGE:
      for(j=0;j<Config->direction_count;j++) {
		source_cell = target_cell->connect[j];
		if(target_cell->level<0 && source_cell->level>=0 && source_cell->growth<TOWN_MIN) continue;
		if(source_cell == target_cell || source_cell == NULL) continue;
		if(source_cell->side != side) continue;
		if(source_cell->ai1->values[TROOP_VALUE] > target_cell->ai1->values[TROOP_VALUE]) continue;
		for(k=0;k<Config->direction_count;k++)
		  if(source_cell->connect[k] == target_cell) break;
		if(k==Config->direction_count)
		  printf("ERROR 42!!\n"); /* FIXME: this happens with shape=diamond... probably a bug in diamond */
		dir[k]=1;
		set_move_force(source_cell,dir,0);
		dir[k]=0;
      }
      break;
    case MOVEMENT_ABORT_ATTACK:
	  /*printf("%d is aborting!\n",side);*/
      for(j=0;j<Config->direction_count;j++) {
	source_cell = target_cell->connect[j];
	if(source_cell == target_cell || source_cell == NULL) continue;
	if(source_cell->value[side] == 0) continue;

	for(k=0;k<Config->direction_count;k++)
	  if(source_cell->connect[k] == target_cell) break;
	if(k==Config->direction_count)
	  printf("ERROR 43!!\n");
	if(!source_cell->dir[k]) continue;
	dir[k]=1;
	set_move_off(source_cell,dir,0);
	dir[k]=0;
      }
      break;
    case MOVEMENT_GUNNERY:
      /*      printf("SHOOTING from %d,%d to %d,%d\n",source_cell->x,source_cell->y,X,Y); */
      source_cell->ai1->already_moved = 1;      
      run_shoot(source_cell,side,X,Y,0,TRUE);
      break;
    }
  }
}


/****************************/
/* Initialize AI for a cell */
/****************************/

int cell_init_algo1(cell_type *cell)
{
    int valnum;

    cell->ai1 = malloc(sizeof(ai1_type));
    for (valnum=0; valnum<NUM_AI_VALUES; valnum++) {
	  cell->ai1->values[valnum] = 0;
	  cell->ai1->tmp_values[valnum] = 0;
    }
    cell->ai1->visited = 0;
    cell->ai1->already_moved = 0;
    cell->ai1->dist = 0;
    /* other values initialized in the player init */
    return 0;
}


/**********************/
/* Free AI for a cell */
/**********************/

void cell_free_algo1(cell_type *cell)
{
    free(cell->ai1);
}
