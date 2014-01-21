#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif

/**** x include files ****/
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>

#include "constant.h"
#include "extern.h"


/******************************************************************************
  run_march (cell, player, side, x, y, button, tdir)

  Given a march command, prepare <cell> appropriately.
******************************************************************************/
void
run_march (cell_type *cell, int player, int side, int x, int y, int button, int tdir[MAX_DIRECTIONS])
{
  int i,
      is_set;

  /** Reset corner of bounding box (with OPTION_BOUND) **/

  Config->old_x[player] = -1;
  Config->old_y[player] = -1;

  /** Can't set march of a sea cell **/

  if (cell->level < 0)
    return;

  /** If <side> owns <cell>, set active march **/

  if (cell->side == side && cell->value[side] > 0)
  {
	snd_march();
    cell->any_march = MARCH_ACTIVE;
    cell->march_side = side;
    cell->march_count = 0;
  }
  else
  {
    /** Else set delayed march -- nothing happens until troops arrive **/

    cell->march[side] = MARCH_PASSIVE;
  }

  /** Set appropriate march direction vector (just one) **/

  if (button == Button1)
    cell->march_type[side] = MOVE_SET;
  else
    cell->march_type[side] = MOVE_FORCE;

  is_set = FALSE;
  for (i=0; i<Config->direction_count && !is_set; i++)
  {
    if (tdir[i])
    {
      cell->march_dir[side] = i;
      is_set = TRUE;
    }
  }

  /** If no directions set, set to HALT **/

  if (!is_set)
    cell->march_dir[side] = MARCH_HALT;

  /** Make sure cell gets redrawn **/

  cell->outdated = side;
}



/******************************************************************************
  run_attack (cell, side)

  Attack given <cell> from all adjacent cells owned by <side>.
******************************************************************************/
void
run_attack (cell_type *cell, int side)
{
  int i, k,
      index,
      tdir[MAX_DIRECTIONS];

  cell_type *cell2;

  shape_type *shape, *shape2;

  /** Make sure OPTION_ATTACK enabled **/

  if (!Config->enable[OPTION_ATTACK][side])
    return;

  /** Make sure there is an enemy in the destination <cell> **/

  if (cell->side == SIDE_NONE || cell->side == side)
    return;
  
  /** play attack sound **/
  snd_attack();

  /** Reset dummy direction vector array **/

  for (i=0; i<Config->direction_count; i++)
    tdir[i] = 0;

  /** Set the shape of the currect cell **/

  shape = Board->shapes[0][cell->shape_index];

  /** For each adjacent cell **/

  for (i=0; i<shape->direction_count; i++)
  {
    cell2 = cell->connect[i];

    /** If <side> owns the neighboring cell, set the direction vector **/ 

    if (cell2 && cell2 != cell && cell2->side == side)
    {
      /** If odd number of directions, do exhaustive search (triangles	**/
      /** use this), else directly set direction to opposite.		**/

      if (shape->direction_count%2)
      {
        shape2 = Board->shapes[0][cell2->shape_index];
        index = -1;
        for (k=0; k<shape2->direction_count && index<0; k++)
        {
          if (cell2->connect[k] == cell)
            index = k;
        }
      }
      else
        index = (i+shape->direction_count/2)%shape->direction_count;

      if (index < 0)
        return;

      /** Set vector position **/

      tdir[index] = 1;

      /** Set move, with or without direction correction (for shapes	**/
      /** with different direction_counts).				**/

      if (cell->shape_index == cell2->shape_index)
        set_move_force (cell2, tdir, 0);
      else
        set_move_force (cell2, tdir, shape->direction_factor);

      tdir[index] = 0;
    }
  }
}



/******************************************************************************
  run_zero (cell)

  Eliminate all movement vectors from <cell>.
******************************************************************************/
void
run_zero (cell_type *cell)
{
  int i,
      tdir[MAX_DIRECTIONS];

  for (i=0; i<Config->direction_count; i++)
    tdir[i] = 0;
  set_move_force (cell, tdir, 0);
}



/******************************************************************************
  run_dig (cell)

  Lower <cell> level by one unit.
******************************************************************************/
void
run_dig (cell_type *cell)
{
  int k;

  cell_type *cell2;

  /** Find destination cell **/

  cell->manage_dir = -1;
  for (k=0; k<Config->direction_count; k++)
  {
    if (cell->dir[k])
    {
      cell2 = cell->connect[k];
      cell->manage_dir = k;
    }
  }

  /** Exit routine and cancel managed dig if multiple direction arrows **/

  if (cell->move > 1)
  {
    if (cell->manage_update == MANAGE_DIG)
      cell->manage_update = FALSE;
    return;
  }

  /** Exit routine if insufficient troops **/

  if (cell->value[cell->side] <= Config->value_int_all[OPTION_DIG_COST])
  {
    return;
  }

  /** If no direction arrows, and not a town **/

  if (cell->move == 0 && cell->growth < TOWN_MIN)
  {
    /** Decrease the level **/

    cell->level -= 1; 

    /** Make sure cell type is set correctly **/

    if (cell->level < Config->level_min)
      cell->level = Config->level_min;

    /** May need to do special redraw of full cell polygon, as opposed	**/
    /** to just erasing troops, as may be normally done.		**/

    cell->redraw_status = REDRAW_FULL;

    /** Reset all cell values to make it a pristine cell **/

    if (cell->level < 0)
      cell->value[cell->side] = 0;
    else
      cell->value[cell->side] -= Config->value_int_all[OPTION_DIG_COST];

    if (cell->value[cell->side] == 0)
    {
      cell->angle = 0;
      cell->any_march = 0;
      update_cell_horizon (cell, cell->side);
      update_cell_clean (cell);
    }

    /** Make sure cell gets redrawn **/

    cell->outdated = OUTDATE_ALL;

    /** Reset managed update if cell is now a sea **/

    if (cell->level < 0)
      cell->manage_update = FALSE;
  }
  else if (cell->move == 1)
  {
    /** Else if directed into a single neighboring cell **/

    /** Find out which neighboring cell should be dug **/

    for (k=0; k<Config->direction_count; k++)
    {
      if (cell->dir[k])
        cell2 = cell->connect[k];
    }

    /** If destination cell does not belong to anyone **/

    if (cell2->side == SIDE_NONE && cell2->level > Config->level_min)
    {
      cell->value[cell->side] -= Config->value_int_all[OPTION_DIG_COST];

      if (cell->value[cell->side] == 0)
      {
        update_cell_horizon (cell, cell->side);
        update_cell_clean (cell);
      }

      cell->outdated = OUTDATE_ALL;

      cell2->level -= 1;
      cell2->outdated = OUTDATE_ALL;

      /** May need to do special redraw of full cell polygon **/

      cell2->redraw_status = REDRAW_FULL;
    }
  }
  else
  {
    if (cell->manage_update == MANAGE_DIG)
      cell->manage_update = FALSE;
  }
}



/******************************************************************************
  run_fill (cell)

  Raise <cell> level by one unit.
******************************************************************************/
void
run_fill (cell_type *cell)
{
  int k;

  cell_type *cell2;

  /** Find destination cell **/

  cell->manage_dir = -1;
  for (k=0; k<Config->direction_count; k++)
  {
    if (cell->dir[k])
    {
      cell2 = cell->connect[k];
      cell->manage_dir = k;
    }
  }

  /* FIXME: what if cell2 wasn't assigned? */

  /** Exit routine and cancel managed dig if multiple direction arrows **/

  if (cell->move > 1)
  {
    if (cell->manage_update == MANAGE_FILL)
      cell->manage_update = FALSE;

    return;
  }

  /** Exit routine if insufficient troops **/

  if (cell->value[cell->side] < Config->value_int_all[OPTION_FILL_COST])
    return;

  /** If a single direction is specified **/

  if (cell->move == 1)
  {
    /** If destination cell unoccupied **/

    if (cell2->side == SIDE_NONE && cell2->level < Config->level_max)
    {
      /** Increase level **/

      cell2->level += 1;

      /** Need to do special redraw of full cell polygon **/

      cell2->redraw_status = REDRAW_FULL;

      /** Empty out source cell and make sure it's redrawn **/

      cell->value[cell->side] -= Config->value_int_all[OPTION_FILL_COST];
      cell->outdated = OUTDATE_ALL;

      /** Make sure destination cell gets redrawn **/

      cell2->outdated = OUTDATE_ALL;
    }
  }
  else if (Config->enable_all[OPTION_HILLS] ||
			Config->enable_all[OPTION_FOREST])
  {
    /** If cell not already at highest possible elevation **/

    if (cell->level < Config->level_max)
    {
      /** Raise cell elevation **/

      cell->level += 1;

      /** Empty out cell and make sure it's redrawn **/

      cell->value[cell->side] -= Config->value_int_all[OPTION_FILL_COST];
      cell->outdated = OUTDATE_ALL;

      /** May need to do special redraw of full cell polygon **/

      cell->redraw_status = REDRAW_FULL;
    }
  }
  else
  {
    if (cell->manage_update == MANAGE_FILL)
      cell->manage_update = FALSE;
  }
}



/******************************************************************************
  run_scuttle (cell)

  Scuttle the town/base located within <cell>.
******************************************************************************/
void
run_scuttle (cell_type *cell)
{
  int value, max_value,
      force, max_force,
      total_cost;

  /** If there aren't enough troops for scuttle, just exit **/

  if (cell->value[cell->side] <
		Config->value_int[OPTION_SCUTTLE_COST][cell->side])
    return;

  /** If there aren't any city segments **/

  if (cell->angle == 0)
    return;

  /** If cell is already a full-fledged town **/

  if (cell->growth >= TOWN_MIN && !Config->enable[OPTION_BUILD][cell->side])
  {
    /** Determine maximum settings **/

    max_value = (int)(cell->growth - TOWN_MIN + 1);
    if (max_value%Config->value_int[OPTION_SCUTTLE][cell->side] == 0)
      max_force = max_value/Config->value_int[OPTION_SCUTTLE][cell->side];
    else
      max_force = max_value/Config->value_int[OPTION_SCUTTLE][cell->side] + 1;

    /** Decrease growth rate by (troops-1)/SCUTTLE_COST **/

    force = (cell->value[cell->side]-1)/
		Config->value_int[OPTION_SCUTTLE_COST][cell->side];

    if (force > max_force)
      force = max_force;

    value = force * Config->value_int[OPTION_SCUTTLE][cell->side];

    cell->growth -= value; 
    cell->old_growth = cell->growth;

    /** If growth rate too small, wipe out town **/

    if (cell->growth < TOWN_MIN)
    {
      value = cell->growth + value - TOWN_MIN;
      force = value/Config->value_int[OPTION_SCUTTLE][cell->side];

      cell->growth = 0;
      cell->old_growth = 0;
      cell->angle = 0;
    }

    total_cost = Config->value_int[OPTION_SCUTTLE_COST][cell->side] * force;
  }
  else if (Config->enable[OPTION_BUILD][cell->side] && cell->angle > 0)
  {
    /** Else if there is part of a town left to scuttle **/

    /** Give side credit for a build if town was previously built **/

    if (cell->angle == ANGLE_FULL)
      Config->stats[cell->side]->build_count--;

    /** Hack out a sector of the town **/

    cell->angle -= Config->value_int[OPTION_BUILD][cell->side];
    cell->growth = 0;

    /** If there is not much of the town left, wipe it out **/

    if (cell->angle < ANGLE_ROUND_DOWN)
    {
      cell->angle = 0; 
      cell->growth = 0; 
      cell->old_growth = 0; 
    }

    total_cost = Config->value_int[OPTION_SCUTTLE_COST][cell->side];
  }

  /** Decrease number of troops **/
    
  cell->value[cell->side] -= total_cost;
  if (cell->value[cell->side] <= 0)
  {
    update_cell_horizon (cell, cell->side);
    update_cell_clean (cell);
  }

  /** Make sure cell gets redrawn **/

  cell->outdated = OUTDATE_ALL;
}



/******************************************************************************
  run_build (cell, side)

  Build a town on <cell>, provided there are sufficient troops occupying it
  to create a town.
******************************************************************************/
void
run_build (cell_type *cell, int side)
{
  /** If sufficient troops and not already a full-fledged town **/

  if (cell->value[cell->side] > Config->value_int[OPTION_BUILD_COST][side] &&
			cell->angle < ANGLE_FULL)
  {
    /** If side has built too many cities, disallow further building **/

    if (Config->enable[OPTION_BUILD_LIMIT][cell->side] &&
		Config->stats[cell->side]->build_count >=
			Config->value_int[OPTION_BUILD_LIMIT][cell->side])
      return;

    /** Reset old_growth if it was never properly set **/

    if (cell->old_growth < TOWN_MIN)
      cell->old_growth = 100;

    /** Increase town by a sector **/

    cell->angle += Config->value_int[OPTION_BUILD][cell->side];

    /** If almost a full circle, give it the benefit of the doubt **/

    if (cell->angle >= ANGLE_ROUND_UP)
    {
      cell->angle = ANGLE_FULL; 
      cell->growth = cell->old_growth;

      Config->stats[cell->side]->build_count++;
    }

    /** Subtract appropriate level of troops **/

    cell->value[cell->side] -= Config->value_int[OPTION_BUILD_COST][side]; 
    cell->lowbound = 0;

    /** Make sure cell gets redrawn for all who can see it **/

    cell->outdated = OUTDATE_ALL;
  }
}



/******************************************************************************
  run_reserve (cell, player, side, text)

  Set the <cell> to reserve troops such that the amount never falls below some
  minimum due to troop movement.
******************************************************************************/
void
run_reserve (cell_type *cell, int player, int side, char text[])
{
  int status;

  char line[MAX_LINE];

  double val;

  shape_type *shape;


  /** Compute the number of troops which should be reserved **/

  shape = Board->shapes[side][cell->shape_index];
  val = (double)(text[0] - '0')*shape->max_value/10.0;
  cell->lowbound = (int)(val);

  /** Save the letter status of the cell **/

  status = XWindow[player]->draw_letter[player];
  strcpy (line, XWindow[player]->letter[player]);

  /** Draw the number on top of the cell **/ 

  XWindow[player]->draw_letter[player] = TRUE;
  text[1] = '\0';
  strcpy (XWindow[player]->letter[player], text);
  draw_cell (cell, player, TRUE);

  /** Restore the letter status of the cell **/

  strcpy (XWindow[player]->letter[player], line);
  XWindow[player]->draw_letter[player] = status;
}



/******************************************************************************
  run_shoot (cell, current_player, xpos, ypos, calculate, is_artillery)

  Shoot a shell or a paratroop.
******************************************************************************/
void
run_shoot (cell_type *cell, int current_player, int xpos, int ypos, int calculate, int is_artillery)
{
  int i,
      side,
      player,
      xdiff, ydiff,
      xdest, ydest,
      xfdest, yfdest,
      dest_side,
      source_side,
      cost, damage,
      winxsize, winysize;

  short max_value;

  double range;

  cell_type *dest_cell;

  /** Determine appropriate cost and damage values **/

  if (is_artillery)
  {
    cost = Config->value_int_all[OPTION_ARTILLERY_COST];
    damage = Config->value_int_all[OPTION_ARTILLERY_DAMAGE];
    range = Config->value_double[OPTION_ARTILLERY][cell->side] *
				 ARTILLERY_FACTOR;
  }
  else
  {
    cost = Config->value_int_all[OPTION_PARATROOPS_COST];
    damage = -Config->value_int_all[OPTION_PARATROOPS_DAMAGE];
    range = Config->value_double[OPTION_PARATROOPS][cell->side] *
				 PARATROOPS_FACTOR;
  }

  /** Set a convenience variable **/

  source_side = cell->side;

  /** Make sure there are sufficient troops to shoot **/

  if (cell->value[cell->side] < cost + 1)
    return;

  /** Compute direction and distance of shot package **/

  if (calculate)
  {
    /** Compute x and y input relative to cell center **/

    xdiff = xpos - cell->x_center[cell->side];
    ydiff = ypos - cell->y_center[cell->side];

    /** Compute x and y destination (in absolute coordinates) **/

    xfdest = xpos + xdiff * range;
    yfdest = ypos + ydiff * range;

    /** If wrap is allowed, wrap destination coordinates around screen **/

    if (Config->enable[OPTION_WRAP][source_side])
    {
      winxsize = XWindow[current_player]->size_play.x;
      winysize = XWindow[current_player]->size_play.y;
  
      if (xfdest > winxsize)
        xfdest -= winxsize;
      else if (xfdest < 0)
        xfdest = winxsize + xfdest;

      if (yfdest > winysize)
        yfdest -= winysize;
      else if (yfdest < 0)
        yfdest = winysize + yfdest;
    }

    /** Find the cell which the destination coordinates lie in **/

    dest_cell = get_cell (xfdest, yfdest, NULL, source_side, FALSE);

    /** If off the board, just exit **/
    
    if (dest_cell == NULL)
      return;

    /** Set destination values **/

    xdest = dest_cell->x;
    ydest = dest_cell->y;
  }
  else
  {
    /** Else argument provides destination cell **/

    xdest = xpos;
    ydest = ypos;
  }

  /** Set destination cell grid coordinates **/

  cell->manage_x = xdest;
  cell->manage_y = ydest;

  dest_cell = CELL2(xdest,ydest);
  dest_side = dest_cell->side;

  /** Subtract cost from troops **/

  cell->value[cell->side] -= cost;

  /** If using artillery **/

  if (is_artillery)
  {
    if (dest_side != SIDE_NONE && dest_side != SIDE_FIGHT)
    {
      /** Subtract off shell damage **/

      dest_cell->value[dest_side] -= damage;

      /** If occupant wiped out, clean up cell and redraw **/

      if (dest_cell->value[dest_side] <= 0)
      {
        update_cell_clean (dest_cell);
        dest_cell->manage_update = FALSE;

        dest_cell->march[dest_side] = FALSE;
        dest_cell->any_march = FALSE;
        dest_cell->value[dest_side] = 0;

        /** Outdate cell and surrounding cells **/

        update_cell_horizon (dest_cell, dest_side);
        dest_cell->outdated = OUTDATE_ALL;
      }
    }

    /** NOTE: artillery does not damage fighting troops **/
  }
  else
  {
    /** Else using paratroops **/

    max_value = Board->shapes[source_side][dest_cell->shape_index]->max_value;

    if (dest_cell->level >= 0)
    {
      if (dest_side == SIDE_NONE || dest_side == source_side)
      {
        dest_cell->side = source_side;
        dest_cell->age = 0;
        dest_cell->any_march = FALSE;
        dest_cell->value[source_side] -= damage;
        if (dest_cell->value[source_side] > max_value)
          dest_cell->value[source_side] = max_value;

        /** Outdate cell and surrounding cells **/

        if (dest_side == SIDE_NONE)
          update_cell_horizon (dest_cell, source_side);
        dest_cell->outdated = OUTDATE_ALL;
      }
      else if (dest_side != source_side && dest_side != SIDE_FIGHT)
      {
        dest_cell->value[source_side] -= damage;
        dest_cell->side = SIDE_FIGHT;
        dest_cell->manage_update = FALSE;

        if (Config->enable[OPTION_DISRUPT][source_side])
        {
          for (i=0; i<Config->direction_count; i++)
            dest_cell->dir[i] = 0;
          dest_cell->move = 0;
        }

        /** Outdate cell and surrounding cells **/

        update_cell_horizon (dest_cell, source_side);
        dest_cell->outdated = OUTDATE_ALL;
      }
      else if (dest_side == SIDE_FIGHT)
      {
        dest_cell->value[source_side] -= damage;
        if (dest_cell->value[source_side] > max_value)
          dest_cell->value[source_side] = max_value;
      }
    }
  }

  /** Redraw affected cells and shell burst (for all players!) **/

  for (player=0; player<Config->player_count; player++)
  {
    side = Config->player_to_side[player];
    if (!XWindow[player]->open)
      continue;

    /** If source cell is visible to that side, redraw it **/

    if (is_visible (cell, side))
    {
      if (Config->enable[OPTION_HIDDEN][side])
      {
        if (cell->side == side)
          draw_cell (cell, player, TRUE);
      }
      else
        draw_cell (cell, player, TRUE);
    }

    /** Store source cell draw to file (for player 0 only) **/ 

    if (Config->enable_all[OPTION_STORE] && !player)
      store_draw_cell (cell, Config->fp);

    /** If dest source cell is visible to that side, redraw it **/

    if (is_visible (dest_cell, side))
    {
      draw_cell (dest_cell, player, TRUE);

      /** Draw actual shell, and outdate destination cell so shell erased **/

      if (is_artillery)
        draw_shell (dest_cell, player, source_side);
      else
        draw_chute (dest_cell, player, source_side);

      CELL2(xdest,ydest)->outdated = OUTDATE_ALL;
      /* TODO. Place this somewhere better? */
      XFlush (XWindow[player]->display);
    }

    /** Store destnation cell draw and shell/para draw to file **/ 

    if (Config->enable_all[OPTION_STORE] && !player)
    {
      /** ALTER: do something to store_draw shell/chute **/

      store_draw_cell (dest_cell, Config->fp);
    }
  }
}

