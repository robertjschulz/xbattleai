#ifndef INCLUDED_AI1_H
#define INCLUDED_AI1_H

/* Copyright 2000 - Mathias Broxvall */
/* Internal include file for AI algorithm 1. */

#include "constant.h"

struct ai1_struct
{
    double values[3],tmp_values[3],feedback_values[MAX_PLAYERS][3];
    char visited, already_moved;
    int dist;
};

#define false 0
#define true  1

/******************/
/*  AI constants  */
/******************/

#define TROOP_VALUE 0
#define CELL_VALUE 1
#define THREAT 2
#define NUM_AI_VALUES 3

#define MOVEMENT_NONE          0
#define MOVEMENT_NORMAL_ON     1
#define MOVEMENT_NORMAL_OFF    2
#define MOVEMENT_CHARGE        3
#define MOVEMENT_ABORT_ATTACK  4
#define MOVEMENT_GUNNERY       5

#define DECAY_OWN_STRENGTH_INVERSE  1
#define LAND_TO_LAND                2
#define HARBOUR_TO_SEA              4
#define LAND_TO_SEA                 8
#define SEA_TO_SEA                 16
#define SEA_TO_HARBOUR             32
#define SEA_TO_LAND                64

#define DECAY_NORMAL                0
#define AMPHIBIOUS_LS               LAND_TO_LAND|HARBOUR_TO_SEA|SEA_TO_SEA|SEA_TO_HARBOUR|SEA_TO_LAND
#define AMPHIBIOUS_SL               LAND_TO_LAND|HARBOUR_TO_SEA|SEA_TO_SEA|SEA_TO_HARBOUR|LAND_TO_SEA
#define ANY_TO_ANY                  126
#define LAND_TO                     LAND_TO_LAND|HARBOUR_TO_SEA|LAND_TO_SEA
#define SEA_TO                      SEA_TO_SEA|SEA_TO_HARBOUR|SEA_TO_LAND

/*****************************************************************
   Explanation of skillLevel:
   Zero for every human player otherwise computer controlled.
   Skill level is first scaled by a constant (to make the scale
   nicer for beginners), if the scaled skill level is less than
   one it represents the likelyhood every "turn" that AI will
   be allowed to move. Ie, scaled skill level 0.5 means 50% chance
   to move every turn. If skill level is greater than one the AI 
   can make as many moves as the interger part and optionally one
   more with the fractional part as likelyhood. Ie, skill level 1.5 
   means 50% chance to make two moves, otherwise only one move.
******************************************************************/

#define MAX_GUNNERY_TARGETS 32
typedef struct s_AIData
{
  float skillLevel;

  /* These are the new gunnery targets calculated this turn */
  int new_num_gunnery_targets,new_gunnery_targets[MAX_GUNNERY_TARGETS][3];

  /* These are the targets provided earlier turns */
  int num_gunnery_targets,gunnery_targets[MAX_GUNNERY_TARGETS][3];
} AIData;

#endif /* INCLUDED_AI1_H */
