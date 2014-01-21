#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* Note. Changes made 2000-09-08 to incorporate calls to handle the AI, also
   the functions set_move_on,set_move_off,set_move etc.. will always be compiled
   (the AI needs them)
   -- Mathias Broxvall

   For changes: Copyright (c) 2000 - Mathias Broxvall
   other parts of file copyrighted as per instructions in file "COPYRIGHT"  
   */

#include <stdio.h>
  
#include "constant.h"
#include "extern.h"

#include "ai.h"


/** File local prototypes **/

static void update_cell (cell_type *cell1, cell_type *cell2);
static void update_cell_manage (cell_type *cell);
static void update_board_march (void);

int snd_fight_mod=0;

/******************************************************************************
  update_board ()

  Update the status of the entire gameboard and redraw those cells which have
  changed (or become visible to a new side).  Update order is randomly
  determined at the beginning of each update cycle.
******************************************************************************/
void
update_board (void)
{
  int i, k, l,
      player,
      side,
      enable_disrupt,
      swap_index,
      left,
      swapper[MAX_BOARDSIZE*MAX_BOARDSIZE],
      order[MAX_BOARDSIZE*MAX_BOARDSIZE];

  static unsigned int xrand=0;

  cell_type *cell;

#if USE_PAUSE
  if (Config->is_paused)
    return;
#endif

  /** reset chance of making an snd_fight sound */
  snd_fight_mod=snd_fight_mod/2;

  /** Determine victories **/

  victory_check();

  /** Call Ai algo main function for each side **/

  for (side=0; side<Config->side_count; side++)
    ai_algo_table[Config->ai_algo_id[side]](side);

  /** Update marching if necessary **/

  if (Config->enable_all[OPTION_MARCH])
    update_board_march ();

  /** Shuffle the randomization tables **/

  for (i=0; i<Board->cell_count; i++)
    swapper[i] = i;

  for (i=0,left=Board->cell_count; i<Board->cell_count; i++,left--)
  {
    swap_index = get_random (left);
    order[i] = swapper[swap_index];
    swapper[swap_index] = swapper[left-1];
  }

  /** Update each board cell **/

  for (i=0; i<Board->cell_count; i++)
  {
    cell = Board->list[order[i]];

    /** Do self-explanatory updates **/

    update_cell_growth (cell);

    if (cell->manage_update)
      update_cell_manage (cell);    
	
	update_cell_decay (cell);

    if (cell->side == SIDE_FIGHT)
      update_cell_fight (cell);

    /** Make sure disrupt doesn't mess up fighting square **/

    if (cell->side != SIDE_FIGHT && cell->side != SIDE_NONE)
    {
      enable_disrupt = Config->enable[OPTION_DISRUPT][cell->side];
      if (cell->value[cell->side] > 0)
        cell->age = 0;
    }
    else
    {
      enable_disrupt = FALSE;
      cell->age = 0;
    }
	
    /** If any direction vector is active **/

    if (cell->move) 
    {
      /** If there isn't a fight OR if disrupt disabled **/

      if (cell->side!=SIDE_FIGHT || !enable_disrupt)
      {
        /** Pick a random number from 0 to number of directions **/

        xrand = (xrand+1)%Config->direction_count;

        /** For each direction **/

        for (k=0; k<Config->direction_count; k++)
        {
          /** Pick pseudo-random (circularly selected) direction **/

          l = (k+xrand)%Config->direction_count;

          /** If there is movement, update the cells **/

          if (cell->dir[l])
            update_cell (cell, cell->connect[l]);
        }
      }
    }
  }

  /** Done with all updates, now redraw cells which have changed **/

  for (i=0; i<Board->cell_count; i++)
  {
    cell = Board->list[order[i]];

    /** Force cell to be outdated if change in value has occurred **/

    if (cell->side != SIDE_FIGHT && cell->side != SIDE_NONE)
    {
      if (cell->value[cell->side] != cell->old_value)
        cell->outdated = OUTDATE_ALL;

      cell->age++;

      if (Config->enable[OPTION_ERODE][cell->side])
        update_cell_erode (cell);
    }

    /** For each player, see if cell should be redrawn **/

    for (player=0; player<Config->player_count; player++)
    {
      side = Config->player_to_side[player];

      if (!XWindow[player]->open)
        continue;

      /** Redraw cell if outdated for everyone or for player's side **/

      if (cell->outdated == OUTDATE_ALL || cell->outdated == side)
      {
        if (is_visible (cell, side))
          draw_cell (cell, player, TRUE);
        else
          draw_cell (cell, player, FALSE);
      }
    }

    /** Remove cell's outdated status **/

    if (cell->outdated >= 0)
    {
      if (Config->enable_all[OPTION_STORE])
        store_draw_cell (cell, Config->fp);
      cell->outdated = OUTDATE_NONE;
    }

    cell->redraw_status = REDRAW_NORMAL;

    /** Save troop values for next update cycle **/

    if (cell->side != SIDE_FIGHT && cell->side != SIDE_NONE)
      cell->old_value = cell->value[cell->side];
    else
      cell->old_value = -1;
  }
}


#define MOVE_LAND_TO_LAND 0
#define MOVE_LAND_TO_SEA  1
#define MOVE_SEA_TO_LAND  2
#define MOVE_SEA_TO_SEA   3
#define TO_SEA            1
#define FROM_SEA          2

/******************************************************************************
  update_cell (cell1, cell2)

  Update changes in cells <cell1>  and <cell2> due to movement of troops from
  the first to the second.
******************************************************************************/
static void
update_cell (cell_type *cell1, cell_type *cell2)
{
  int i,
      enable_disrupt,
      side1, side2,
      surplus,
	nmove,
	movement_kind;

  short max_value;

  double doublenmove;

  /** Get sides **/
  side1 = cell1->side;
  side2 = cell2->side;

  /* What kind of land/sea movements are we making? */
  movement_kind=MOVE_LAND_TO_LAND;
  if(cell1->level<0) movement_kind |= FROM_SEA;
  if(cell2->level<0) movement_kind |= TO_SEA;

  /* We can only move disembark from harbours. TODO, player specific harbours? */
  if(movement_kind == MOVE_LAND_TO_SEA && (!Config->enable_all[OPTION_HARBOURS]))
	return;
  if(movement_kind == MOVE_LAND_TO_SEA && cell1->growth<TOWN_MIN)
	return;

  /** If source cell is fighting, use the side which originally owned	**/
  /** the cell (before fight started).  Hopefully, update_cell is never	**/
  /** called if disrupt enabled for a fighting cell.			**/

  if (side1 == SIDE_FIGHT && (side2 == SIDE_NONE || side2 == cell1->old_side))
    side1 = cell1->old_side;
  else if (side1 == SIDE_FIGHT)
    return;

  /** Set disruption flag **/
  enable_disrupt = Config->enable[OPTION_DISRUPT][side1];

  /** Compute how much source cell can afford to move **/
  surplus = cell1->value[side1] - cell1->lowbound;
  if (surplus <= 0)
    return;

  /** Compute the number of troops to move **/

  switch(movement_kind) {
  case MOVE_LAND_TO_LAND:
#ifdef WITH_HILLS_AND_FOREST
	doublenmove = (double)surplus *
	  Config->move_hinder[side1][cell2->forest_level] *
	  Config->move_shunt[side1][cell2->value[side1]] *
	  Config->move_moves[cell1->move] *
	  (1.0 - Config->move_slope[side1][cell2->level - cell1->level]);
	
#else
	doublenmove = (double)surplus *
	  Config->move_hinder[side1][cell2->level] *
	  Config->move_shunt[side1][cell2->value[side1]] *
	  Config->move_moves[cell1->move] *
	  (1.0 - Config->move_slope[side1][cell2->level - cell1->level]);
#endif
	break;
  case MOVE_LAND_TO_SEA:
	doublenmove = (double) surplus * 0.5;
	break;
  case MOVE_SEA_TO_LAND:
	doublenmove = (double) surplus * 0.6;
	break;
  case MOVE_SEA_TO_SEA:
	doublenmove = (double) surplus * 0.6;
	break;
  }

  nmove = (int)(doublenmove);

  /** Compute number of troops to actually move, using a probabalistic	**/
  /** approach for fractional troop movements.				**/

  max_value = Board->shapes[side1][cell2->shape_index]->max_value;
		  
  if (nmove > surplus)
    nmove = surplus;
  if (nmove == 0 && doublenmove > 0.0 && get_random(100) < (100.0*doublenmove))
    nmove = 1;
  if (nmove + cell2->value[side1] > max_value)
    nmove = max_value - cell2->value[side1];
  if (nmove <= 0)
    return;

  /** Update troops in source cell **/
  cell1->value[side1] = cell1->value[side1] - nmove;

  /** handle looses due to movement from sea to land **/
  /* 25% penalty for landing in a non harbour */
  if(movement_kind == MOVE_SEA_TO_LAND && cell2->growth < TOWN_MIN) nmove -= nmove / 4;
  /* 50% penalty for amphibious assaults */
  if(movement_kind == MOVE_SEA_TO_LAND && side1 != side2) nmove -= nmove / 2;

  /** Update troops in destination cell, depending upon status of cell **/
  if (side2 == SIDE_NONE && side1 != SIDE_FIGHT)
  {
    /** Moving into a previously empty cell **/
    cell2->side = side2 =	side1;
    cell2->value[side2] =	nmove;
    cell2->age =		0;

    /** 0utdate both source and destination cell **/

    cell1->outdated =		OUTDATE_ALL;
    cell2->outdated =		OUTDATE_ALL;
  }
  else if (side1 == side2 && side1 != SIDE_FIGHT)
  {
    /** Move into a cell occupied only by friendly troops **/

    cell2->value[side1] += nmove;
  }
  else if (side1 != side2 && side2 != SIDE_NONE && side2 != SIDE_FIGHT)
  {
    /** Move into a cell occupied only by enemy troops **/

    cell2->side =		SIDE_FIGHT;
    cell2->manage_update =	FALSE;
    cell2->value[side1] =	cell2->value[side1] + nmove;
    cell2->old_side =		side2;

    /** Knock out enemy direction vectors if disrupt enabled **/

    if (enable_disrupt)
      for (i=0; i<Config->direction_count; i++)
        cell2->dir[i] = 0;

    cell1->outdated =		OUTDATE_ALL;
    cell2->outdated =		OUTDATE_ALL;
  }
  else if (side2 == SIDE_FIGHT)
  {
    /** Move into a cell occupied by both friendly and enemy troops **/

    cell2->value[side1] += nmove;

    cell1->outdated =		OUTDATE_ALL;
    cell2->outdated =		OUTDATE_ALL;
  }

  /** Outdate neighboring cells if using horizon and ownership changed **/

  if (cell1->value[side1] == 0)
    update_cell_horizon (cell1, side1);
  if (cell2->value[side1] == nmove)
    update_cell_horizon (cell2, side1);
}



/******************************************************************************
  update_cell_manage (cell)    

  Update <cell> if that cell has some latent managed activity, which only
  can happen when OPTION_MANAGE is set. 
******************************************************************************/
static void
update_cell_manage (cell_type *cell)
{
  /** If movement vector has been issued, cancel manage **/

  if (cell->move > 0)
  {
    if (cell->manage_dir >= 0)
    {
      if (cell->move != 1 || cell->dir[cell->manage_dir] == 0)
      {
        cell->manage_update = FALSE;
        return;
      }
    }
    else
    {
      cell->manage_update = FALSE;
      return;
    }
  }

  /** If no one owns the cell, or fight has occurred, cancel manage **/

  if (cell->side == SIDE_NONE || cell->side == SIDE_FIGHT)
  {
    cell->manage_update = FALSE;
    return;
  }

  /** Find out which type of self-explanatory management is going on **/

  switch (cell->manage_update)
  {
    case MANAGE_CONSTRUCTION:

      run_build (cell, cell->side);
      if (cell->angle == ANGLE_FULL)
        cell->manage_update = FALSE;
      break;

    case MANAGE_ARTILLERY:

	  if(get_random(100) < 40)
		snd_gun();
      run_shoot (cell, 0, cell->manage_x, cell->manage_y,
				FALSE, TRUE);
      break;

    case MANAGE_PARATROOP:

	  if(get_random(100) < 40)
		snd_para();
      run_shoot (cell, 0, cell->manage_x, cell->manage_y,
				FALSE, FALSE);
      break;

    case MANAGE_FILL:

      /** If there are enough troops to do the fill **/

      if (cell->value[cell->side] > Config->value_int_all[OPTION_FILL_COST])
      {
        /** Set temporary direction vector if appropriate **/

        if (cell->manage_dir >= 0)
        {
          cell->dir[cell->manage_dir] = 1;
          cell->move = 1; 
        }

        /** Fill cell (or cell pointed to by manage_dir) **/

        run_fill (cell);

        /** If cell was filling itself **/

        if (cell->manage_dir < 0)
        {
          if (Config->enable_all[OPTION_HILLS] ||
				Config->enable_all[OPTION_FOREST])
          {
            /** Turn off management if reached fill limit **/

            if (cell->level == Config->level_max)
              cell->manage_update = FALSE;
          }
          else
            cell->manage_update = FALSE;
        }
      }
      else
      {
        /** Else turn off management if destination cell has hit limit **/

        if (cell->manage_dir >= 0 &&
		cell->connect[cell->manage_dir]->level == 0)
          cell->manage_update = FALSE;
      }
      break;

    case MANAGE_DIG:

      /** If there are enough troops to do the fill **/

      if (cell->value[cell->side] > Config->value_int_all[OPTION_DIG_COST])
      {
        /** Set temporary direction vector if appropriate **/

        if (cell->manage_dir >= 0)
        {
          cell->dir[cell->manage_dir] = 1;
          cell->move = 1; 
        }

        /** Dig cell (or cell pointed to by manage_dir) **/

        run_dig (cell);

        /** If cell was digging itself **/

        if (cell->manage_dir < 0)
        {
          if (cell->level == 0)
            cell->manage_update = FALSE;
        }
        else
        {
          /** cell was digging neighbor **/

          if (cell->connect[cell->manage_dir]->level == Config->level_min)
            cell->manage_update = FALSE;
        }
      }
      break;
  }
}



/******************************************************************************
  update_cell_growth (cell)

  Compute the troop growth of <cell> and update the cell accordingly.
******************************************************************************/
void
update_cell_growth (cell_type *cell)
{
  int side,
#ifdef WITH_BASE_SIDE
      base_side,
#endif
      growth;

  short max_value;

  /** Get side **/

#ifdef WITH_BASE_SIDE
  base_side=cell->base_side;
#endif

  side = cell->side;
  if (side == SIDE_NONE || side == SIDE_FIGHT)
    return;

  max_value = Board->shapes[side][cell->shape_index]->max_value;

  growth = cell->growth;

  /** If cell is capable of growing troops **/

#ifdef WITH_BASE_SIDE
  if (growth > 0 &&
      cell->value[side] < max_value &&
      (base_side == side || base_side == -1))
#else
  if (growth > 0 && cell->value[side] < max_value)
#endif
  {
    /** If it's a town, it should never erode **/

    if (growth >= TOWN_MIN)
      cell->age = 0;

    /** If growth over 100, it's a super town which can produce more	**/
    /** than 1 troop per turn, specifically it produces:		**/
    /**		(int)(growth/100) + pr(x<(growth%100))			**/

    if (growth > 100)
    {
      cell->value[side] += growth/100;
      growth = growth%100;
    }

    /** Check to see if cell should produce a troop **/

    if (growth > get_random (100))
      cell->value[side]++;

    /** Throw away troops over maximum cell capacity **/

    if (cell->value[side] > max_value)
      cell->value[side] = max_value;
  }
}



/******************************************************************************
  update_cell_decay (cell)

  Compute the troop decay of <cell> and update the cell accordingly.
******************************************************************************/
void
update_cell_decay (cell_type *cell)
{
  int side,i;

  double thresh;

  /** Get side **/

  side = cell->side;
  if (side == SIDE_NONE || side == SIDE_FIGHT) 
    return;

  /** Check to see if troops should probabalistically decay **/

  /* note. changed decay formula slightly so that when decay is enabled it is encouraged
	 to concetrate the troops rather than spreading them out! */
  /* thresh = Config->value_double[OPTION_DECAY][side] * cell->value[side]/2*/ 

  if(cell->level<0)	
	thresh = cell->level * -15;
  else
	thresh = Config->value_double[OPTION_DECAY][side] * 10;

  while(thresh > 0) {
	if (get_random(100) < thresh)
	  cell->value[side] -= 1;
	thresh -= 100;
  }

  /** If value decays below zero, outdate horizon **/
  if (cell->value[side] <= 0)
  {
    cell->value[side] = 0;
    update_cell_horizon (cell, side);

	/*cell->side = SIDE_NONE;*/
	/*for (i=0; i<Config->direction_count; i++)
	  cell->dir[i] = 0;*/
	/*cell->outdated=OUTDATE_ALL;*/
  }
}



/******************************************************************************
  update_cell_erode (cell)

  Check to see if <cell>'s direction vectors should be eroded by time.
  Erosion can occur only if a cell has been occupied by the same side for
  some number of cycles greater than Config->value_int[OPTION_ERODE].  If
  the cell is older than that, then erosion will occur with probability
  given by <Config->value_int[OPTION_ERODE]>.
******************************************************************************/
void
update_cell_erode (cell_type *cell)
{
  int i;

  if (cell->side == SIDE_NONE)
    return;

  if (cell->age > Config->value_int[OPTION_ERODE][cell->side])
  {
    if (get_random (100) < Config->value_int[OPTION_ERODE_THRESH][cell->side])
    {
      for (i=0; i<Config->direction_count; i++)
        cell->dir[i] = 0;
      cell->move = 0;
      cell->age = 0;
      cell->outdated = OUTDATE_ALL;
      if (cell->value[cell->side] == 0)
        cell->side = SIDE_NONE;
    }
  }
}



/******************************************************************************
  update_cell_fight (cell)

  Updates <cell> which is occupied by more than one side.  The outcome of the
  battle is determined probablistically as a non-linear function of the
  relative strengths of the troops.
******************************************************************************/
void
update_cell_fight (cell_type *cell)
{
  int j, k, l,
      side,
      feeble_side,
      count, pos,
      side_loss_int,
      versus[MAX_SIDES];

  double side_ratio[MAX_SIDES],
         feeble_ratio,
         side_loss[MAX_SIDES];

  cell_type *near_cell;
  shape_type *shape,
             *near_shape;

  /** Compute the number of attacking forces against each side **/

  for (side=0; side<Config->side_count; side++)
  {
    versus[side] = 0;
    for (j=0; j<Config->side_count; j++)
    {
      if (j != side)
      {
        if (cell->value[j] > 0)
          versus[side] += cell->value[j];
      }
    }
  }

  /** Compute how badly each side is outnumbered **/

  feeble_side = SIDE_NONE;
  feeble_ratio = 0.0;

  for (side=0; side<Config->side_count; side++)
  {
    if (cell->value[side] > 0)
    {
      side_ratio[side] = (double)versus[side]/((double)cell->value[side]);

      /** Keep track of who is being beaten the worst **/

      if (side_ratio[side] > feeble_ratio)
      {
        feeble_side = side;
        feeble_ratio = side_ratio[side];
      }
    }
  }

  /** If there is at least one side AND the feeblest side has NOSPIGOT	**/
  /** enabled AND the feebleness ratio is above the threshold.		**/

  if (feeble_side != SIDE_NONE &&
		Config->enable[OPTION_NOSPIGOT][feeble_side] &&
		feeble_ratio > Config->value_int[OPTION_NOSPIGOT][feeble_side])
  {
    shape = Board->shapes[feeble_side][cell->shape_index];

    /** For each possible direction **/

    for (k=0; k<shape->direction_count; k++)
    {
      near_cell = cell->connect[k];

      /** If nearby cell owned by same side **/

      if (near_cell->side == feeble_side)
      {
        near_shape = Board->shapes[feeble_side][near_cell->shape_index];

        /** For each nearby cell direction **/

        for (l=0; l<near_shape->direction_count; l++)
        {
          /** If direction vector points back to cell **/

          if (near_cell->dir[l] && near_cell->connect[l] == cell)
          {
            near_cell->dir[l] = 0;
            near_cell->move--;
            near_cell->outdated = OUTDATE_ALL;
          }
        }
      }
    }
  }

  /** For each side **/

  for (side=0, count=0; side<Config->side_count; side++)
	{
	  /** If the side does occupy the cell **/
	  
	  if (cell->value[side] > 0)
		{
		  /** Compute the troop losses **/
		  
		  side_loss[side] = ((side_ratio[side]*side_ratio[side]) - 1.0 +
							 0.02*get_random(100)) * Config->value_int[OPTION_FIGHT][side];
		  if (side_loss[side] < 0.0)
			side_loss[side] = 0.0;

		  
		  /** play gunfire with probability proportional to loss **/
		  if(get_random(500+snd_fight_mod) < side_loss[side]) {
			/* tries to limit number of simultaneous snd_fight sounds */
			snd_fight_mod += 100;
			snd_fight();
		  }

      /** Subtract off the loss for that side **/

      side_loss_int = (int)(side_loss[side]+0.5);
      if (side_loss_int < cell->value[side])
      {
        cell->value[side] -= side_loss_int;

        /** Keep a count of non-zero sides **/

        count++;
        pos = side;
      }
      else
      {
        cell->value[side] = 0;

        /** A side has been wiped out, so update horizon **/

        update_cell_horizon (cell, side);
      }
    }
  }

  /** If there is a single winner, give the cell to that side **/

  if (count == 1)
  {
    if (cell->old_side != pos) {
      update_cell_clean (cell);
      if(cell->growth > TOWN_MIN)
		/* Someone has conquered a town */
		snd_town();
    }
    else
      cell->age = 0;
    
    cell->side = pos;  
      
  }
  else if (count == 0)
  {
    /** Else if everyone is dead **/

    update_cell_clean (cell);
  }

  /** Outdate cell for everyone **/

  cell->outdated = OUTDATE_ALL;
}



/******************************************************************************
  update_board_march ()

  Updates the marching cells across the whole board.
******************************************************************************/
void
update_board_march (void)
{
  int i,
      x,
      side;

  cell_type *cell, *cell2;

  /** For each cell on the board **/

  for (x=0; x<Board->cell_count; x++)
  {
    cell = CELL(x);
    side = cell->side; 

    /** If the cell is actively undergoing march (or was) **/

    if (cell->any_march == MARCH_ACTIVE)
    {
      if (side > SIDE_VALID_LIMIT || side != cell->march_side)
        cell->any_march = FALSE;
    }
    else if (cell->any_march == MARCH_PASSIVE)
    {
      if (side < SIDE_VALID_LIMIT && side == cell->march_side)
        cell->any_march = MARCH_ACTIVE;
    }

    /** If cell is actively undergoing march **/

    if (Config->enable[OPTION_MARCH][side] && cell->any_march==MARCH_ACTIVE)
    {
      /** Increment the march counter **/

      cell->march_count += 1;

      /** If march counter has reached march threshold **/

      if (cell->march_count >= Config->value_int[OPTION_MARCH][side] &&
				cell->value[side] > 0)
      {

		snd_march();

        /** Remove march command **/

        cell->any_march = FALSE;
        cell->outdated = OUTDATE_ALL;

        /** If forcing march, override normal direction vectors **/

        if (cell->march_type[side] == MOVE_FORCE)
        {
          /** Eliminate normal direction vectors **/

          for (i=0; i<Config->direction_count; i++)
            cell->dir[i] = 0;

          /** If not a HALT command, set march vector **/

          if (cell->march_type[side] != MARCH_HALT)
          {
            cell->dir[cell->march_dir[side]] = 1;
            cell->move = 1;
          }
          else
            cell->move = 0;
        }
        else if (cell->march_type[side] != MARCH_HALT)
        {
          /** Else OR march vector in with direction vectors **/

          if (!cell->dir[cell->march_dir[side]])
            cell->move++;
          cell->dir[cell->march_dir[side]] = 1;
        }

        /** If not a HALT command **/

        if (cell->march_dir[side] != MARCH_HALT)
        {
          cell2 = cell->connect[cell->march_dir[side]];

          /** If destination cell is invalid (as at edge or sea) **/

          if (cell2 == cell || cell2->level < 0)
          {
            cell->any_march = FALSE;
            cell->march[side] = FALSE;
            cell->march_count = 0;
          }
          else if (cell2->side == SIDE_NONE || cell2->side == side)
          {
            cell2->outdated = OUTDATE_ALL;

            /** Else if destination unoccupied or friendly **/

            if (cell2->march[side] == MARCH_PASSIVE) 
            {
              /** Move into cell with passive march command **/

              cell2->any_march = MARCH_TEMP;
              cell2->march[side] = FALSE;
              cell2->march_side = side;
              cell2->march_count = 0;
            }
            else
            {
              /** Move into cell with no march command **/

              cell2->any_march = MARCH_TEMP;
              cell2->march_type[side] = cell->march_type[side];
              cell2->march_side = side;
              cell2->march[side] = FALSE;
              cell2->march_count = 0;

              if (cell->shape_index == cell2->shape_index)
                cell2->march_dir[side] = cell->march_dir[side];
              else
              {
                cell2->march_dir[side] =
		   (Board->shapes[side][cell->shape_index]->direction_factor*
		   cell->march_dir[side])/
		   Board->shapes[side][cell2->shape_index]->direction_factor;
              }
            }
          }
        }
      }
    }

    /** If there are troops, but not in a fight **/

    if (side < SIDE_VALID_LIMIT && cell->value[side])
    {
      /** If cell is has a passive march **/

      if (cell->march[side] == MARCH_PASSIVE)
      {
        /** Make march active if troops have arrived **/

        cell->any_march = MARCH_ACTIVE;
        cell->march[side] = FALSE;
        cell->march_side = side;
        cell->march_count = 0;
      }
    }
  }

  /** Change any TEMP marches to active.  Didn't set to active before	**/
  /** because of the possibility of a chain reaction.			**/

  for (x=0; x<Board->cell_count; x++)
    if (CELL(x)->any_march == MARCH_TEMP)
      CELL(x)->any_march = MARCH_PASSIVE;
}



/******************************************************************************
  int
  is_visible (cell, active_side)

  Checks whether <cell> is visible by <active_side>, based on the horizon
  being used by that side.
******************************************************************************/
int
is_visible (cell_type *cell, int active_side)
{
  int i,
      x, y,
      count,
      xpos, ypos;

  shape_type *shape;

  /** Return TRUE if player owns cell or horizon disabled **/

  if (!Config->enable[OPTION_HORIZON][active_side])
    return (TRUE);
  if (cell->side == active_side)
    return (TRUE);

  shape = Board->shapes[active_side][cell->shape_index];

  x = cell->x;
  y = cell->y;

  /** Step through each cell specified by the shape structure **/

  count = shape->horizon_counts[Config->view_range[active_side]];

  for (i=0; i<count; i++)
  {
    /** Construct the appropriate even or odd grid indices **/

    if (x%2 == 0)
    {
      xpos = x + shape->horizon_even[i][0];
      ypos = y + shape->horizon_even[i][1];
    }
    else
    {
      xpos = x + shape->horizon_odd[i][0];
      ypos = y + shape->horizon_odd[i][1];
    }

    /** Adjust the indices to reflect wrap-around or edge cut-off **/

    if (Config->enable[OPTION_WRAP][active_side])
    {
      xpos = (xpos < 0) ? Config->board_x_size + xpos : xpos;
      xpos = (xpos > Config->board_x_size - 1) ?
			xpos - Config->board_x_size : xpos;
      ypos = (ypos < 0) ? Config->board_y_size + ypos : ypos;
      ypos = (ypos > Config->board_y_size - 1) ?
			ypos - Config->board_y_size : ypos;
    }
    else
    {
      if (xpos < 0 || xpos >= Config->board_x_size ||
		ypos < 0 || ypos >= Config->board_y_size)
        continue;
    }

    /** If destination square is occupied by <active_side> **/

    if (CELL2(xpos,ypos)->value[active_side] > 0)
      return (TRUE);
  }

  /** If we got this far, cell is not visible **/

  return (FALSE);
}



/******************************************************************************
  update_cell_horizon (cell, current_side)

  Outdates all cells around <cell> so that they will get redrawn (or at least
  checked for necessary redrawing) during the next update cycle.
******************************************************************************/
void
update_cell_horizon (cell_type *base_cell, int current_side)
{
  int i,
      ipos, jpos,
      x, y,
      range,
      count,
      active_side;

  cell_type *cell;
  shape_type *shape;

  if (!Config->enable_all[OPTION_HORIZON])
    return;

  x = base_cell->x;
  y = base_cell->y;

  /** Fix range and side indication **/

  if (current_side == OUTDATE_ALL)
  {
    range = Config->view_range_max;
    shape = Board->shapes[0][0];
  }
  else if (current_side < Config->side_count)
  {
    range = Config->view_range[current_side]; 
    shape = Board->shapes[current_side][base_cell->shape_index];
  }
  else
    return;

  count = shape->horizon_counts[range];

  for (i=0; i<count; i++)
  {
    /** Assign grid indices appropriately for even and odd columns **/

    if (x%2 == 0)
    {
      ipos = x + shape->horizon_even[i][0];
      jpos = y + shape->horizon_even[i][1];
    }
    else
    {
      ipos = x + shape->horizon_odd[i][0];
      jpos = y + shape->horizon_odd[i][1];
    }

    /** Correct grid indices for wrapping or edge cut-off **/

    if (Config->enable_all[OPTION_WRAP])
    {
      ipos = (ipos<0) ? Config->board_x_size+ipos : ipos;
      ipos = (ipos>Config->board_x_size-1) ?
			ipos - Config->board_x_size : ipos;
      jpos = (jpos<0) ? Config->board_y_size+jpos : jpos;
      jpos = (jpos>Config->board_y_size-1) ?
			jpos - Config->board_y_size : jpos;
    }
    else
    {
      if (ipos < 0 || ipos > Config->board_x_size-1 ||
		jpos < 0 || jpos > Config->board_y_size-1)
        continue;
    }

    cell = CELL2(ipos,jpos);

    /** Outdate target cells **/

    active_side = cell->side;

    /** If cell occupied by any enemy (including SIDE_FIGHT) **/ 

    if (active_side != SIDE_NONE && active_side != current_side)
    {
      if (cell->outdated == OUTDATE_NONE)
        cell->outdated = current_side;
      else if (cell->outdated != current_side)
        cell->outdated = OUTDATE_ALL;
    }
    else if (Config->enable_all[OPTION_LOCALMAP] && active_side == SIDE_NONE)
    {
      /** Else if cell is empty and LOCALMAP enabled **/

      if (cell->outdated == OUTDATE_NONE)
        cell->outdated = current_side;
      else if (cell->outdated != current_side)
        cell->outdated = OUTDATE_ALL;
    }
    else if (active_side == SIDE_NONE && cell->growth >= TOWN_MIN)
    {
      /** Else if cell is empty and contains a town **/

      if (cell->outdated == OUTDATE_NONE)
        cell->outdated = current_side;
      else if (cell->outdated != current_side)
        cell->outdated = OUTDATE_ALL;
    }
    else if (current_side != OUTDATE_ALL)
    {
      /** Else if only a single side should be outdated **/

      /** If cell has never been seen by current_side and is empty **/

      if (!cell->seen[current_side] && active_side == SIDE_NONE)
        cell->outdated = current_side;
    }

    /** Flag cell as seen by current_side **/
      
    if (current_side != OUTDATE_ALL)
      cell->seen[current_side] = TRUE;
  }
}



/******************************************************************************
  update_cell_clean (cell)

  Reinitializes certain key elements of <cell> after catastrophic event.
******************************************************************************/
void
update_cell_clean (cell_type *cell)
{
  int k;

  cell->side =		SIDE_NONE;
  cell->side_count =	0;
  cell->move =		0;
  cell->age =		0;
  cell->lowbound =	0;
  for (k=0; k<Config->direction_count; k++)
    cell->dir[k] =	0;
}



/*#if USE_ALT_MOUSE*/
/******************************************************************************
  set_move_on (cell, dir, count)

  Turn on <cell>'s direction vectors indicated by <dir>.
******************************************************************************/
void
set_move_on (cell_type *cell, const int dir[MAX_DIRECTIONS], const int count)
{
  int i;
  shape_type *shape;

  shape = Board->shapes[0][cell->shape_index];

  cell->move = 0;

  for (i=0; i<shape->direction_count; i++)
  {
    if (dir[i])
      cell->dir[i] = 1;
    cell->move += cell->dir[i];
  }
  cell->manage_update = FALSE;
}



/******************************************************************************
  set_move_off (cell, dir, count)

  Turn off <cell>'s direction vectors indicated by <dir>.
******************************************************************************/
void
set_move_off (cell_type *cell, const int dir[MAX_DIRECTIONS], const int count)
{
  int i;
  shape_type *shape;

  shape = Board->shapes[0][cell->shape_index];

  cell->move = 0;
  for (i=0; i<shape->direction_count; i++)
  {
    if (dir[i])
      cell->dir[i] = 0;
    cell->move += cell->dir[i];
  }

  cell->manage_update = FALSE;
}
/*#else*/
/******************************************************************************
  set_move (cell, dir, factor)

  Toggle <cell>'s direction vectors indicated by <dir>, correcting for
  differences in cell shapes if <factor>.
******************************************************************************/
void
set_move (cell_type *cell, const int dir[MAX_DIRECTIONS], const int factor)
{
  int i, map,
      last_map;
  shape_type *shape;

  shape = Board->shapes[0][cell->shape_index];

  /** If <factor> must correct for different shapes **/

  if (factor)
  {
    last_map = -1;
    for (i=0; i<Config->direction_count; i++)
    {
      if (dir[i])
      {
        /** Perform shape correction **/

        map = (factor * i)/shape->direction_factor;

        if (map == last_map)
          continue;

        if (cell->dir[map])
          cell->dir[map] = 0;
        else
          cell->dir[map] = 1;

        last_map = map;
      }
    }
  }
  else
  {
    for (i=0; i<shape->direction_count; i++)
    {
      if (dir[i])
      {
        if (cell->dir[i])
          cell->dir[i] = 0;
        else
          cell->dir[i] = 1;
      }
    }
  }

  cell->move = 0;
  for (i=0; i<shape->direction_count; i++)
  {
    if (cell->dir[i])
      cell->move++;
  }

  cell->manage_update = FALSE;
}



/******************************************************************************
  set_move_force (cell, dir, factor)

  Force all <cell>'s direction vectors to those indicated by <dir>, correcting
  for differences in cell shapes if <factor>.
******************************************************************************/
void
set_move_force (cell_type *cell, const int dir[MAX_DIRECTIONS], const int factor)
{
  int i, map;

  shape_type *shape;

  shape = Board->shapes[0][cell->shape_index];

  /** If <factor> must correct for different shapes **/

  if (factor)
  {
    for (i=0; i<shape->direction_count; i++)
      cell->dir[i] = 0;

    for (i=0; i<Config->direction_count; i++)
    {
      /** Perform shape correction **/

      map = (factor * i)/shape->direction_factor;

      if (dir[i])
        cell->dir[map] = 1;
    }
  }
  else
  {
    for (i=0; i<shape->direction_count; i++)
    {
      if (dir[i])
        cell->dir[i] = 1;
      else
        cell->dir[i] = 0;
    }
  }

  cell->move = 0;
  for (i=0; i<shape->direction_count; i++)
  {
    if (cell->dir[i])
      cell->move++;
  }

  cell->manage_update = FALSE;
}
/*#endif*/
