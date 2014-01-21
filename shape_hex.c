#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "constant.h"
#include "extern.h"


/**                                                    **/
/**   |        |                                       **/
/**   |<--cs-->|                                       **/
/**   |        |                                       **/
/**   __________ ___                                   **/
/**   |        |  ^                                    **/
/**   |        |  |     cs = cell_size                 **/
/**   |        |  cs    squarearea = cs*cs             **/
/**   |        |  |                                    **/
/**   |________| _v_                                   **/
/**                                                    **/
/**                                                    **/
/** A = 180*(n-2)/n = 120                              **/
/** B = A/2 = 60                                       **/
/** C = 180-(90+B) = 30                |side/2|        **/
/**                                    |<---->|        **/
/**   |           |                    |      |        **/
/**   |<--side--->|              ___ 90______60        **/
/**   |   _____   |               ^    |''''/ \        **/
/**   |  /     \  |      sqrt(3)* |    |'''/   \       **/
/**   | /       \ |      (side/2) |    |'''/  -.\      **/
/**    /    90___\                |    |''/   /|       **/
/**    \      |''/B               |    |'/   /         **/
/**     \     |'/                 |    |'/  /          **/
/**      \____|/                 _v_   |/  /side       **/
/**      A     C                      30\|/            **/
/**                                      \-            **/
/**                                                    **/
/**                        |          |                **/
/**        _____           |<--side-->|                **/
/**     /||     ||\        |          |                **/
/**    / ||     || \       ___________  ___            **/
/**   / w||     ||y \      |z  ./|    |  ^             **/
/**   === |  t  | ===      | ./w |    |  | sqrt(3)*    **/
/**   \ x||     ||z /      |/____| t  |  | (side/2)    **/
/**    \ ||     || /       |\. x |    |  |             **/
/**     \||_____||/        |y \. |    |  |             **/
/**                        |____\|____| _v_            **/
/**                                                    **/
/**                                                    **/
/**       squarearea = cs^2                            **/
/**       hexarea = side*side*sqrt(3)/2                **/
/**       squarearea = hexarea                         **/

/**       cs^2 = ((side^2)*sqrt(3))/2                  **/

/**       cs = sqrt(((side^2)*sqrt(3))/2)
 *        cs^2 = ((side^2)*sqrt(3))/2
 *        cs^2 * 2 = (side^2)*sqrt(3)
 *        ( cs^2 * 2 ) / sqrt(3) = side^2
 *        sqrt ( ( cs^2 * 2 ) / sqrt(3) ) = side
 **/
/**                                                    **/
/**  ????  .62040323940139973262   ????                **/
/* FIXME: calculate this with more precision and make an entry in constant.h */
/*#define HEX_AREA_FACTOR	0.6204*/
#define HEX_AREA_FACTOR	0.62040323940139973262



/******************************************************************************
  hex_set_dimensions (shape, cell_size, side)

  Set all internal values of <shape> of <side>, given canonical size of
  <cell_size>.  This includes the various dimensions and offsets, polygonal
  points, direction vectors, town and troop size mappings, and drawing
  method.
******************************************************************************/
void
hex_set_dimensions (shape_type *shape, int cell_size, int side)
{
  int cell_side;

  /** Set cell side to normalize cell area to cell_size*cell_size **/

  cell_side = (HEX_AREA_FACTOR * cell_size + 0.5);

  /** Make sure hexagon side is even **/

  if (cell_side%2 == 1)
    cell_side -= 1;

  /** Set all the relevant dimensions and coordinates **/

  shape->side =			cell_side;

  shape->center_bound.x =	cell_side; /* width */
  shape->center_bound.y =	(int)(CONST_SQ3D2 * shape->side + 0.5);
  shape->size_bound.x =		2 * shape->center_bound.x;
  shape->size_bound.y =		2 * shape->center_bound.y;

  /** Full circle works well for -cell: 0-35, 40-51, 56-79, 84-95       **/
  /** Which translates to cell_size: 0-16, 20-24, 28-38, 42-46          **/ 
  /** So, in summary, we have NOT working: 18, 26, 40, 48, 50           **/

  /* FIXME: up direction vectors sometime leave a one-pixel-high chunk
   * probably only in even-height cells.  Not sure how to fix that. */
  shape->center_erase.x =	shape->center_bound.y - 1;
  shape->center_erase.y =	shape->center_bound.y - 1;
  shape->corner_erase.x =	cell_side/2 + 1;
  shape->corner_erase.y =	1;
  shape->size_erase.x =		2 * shape->center_erase.x - 1;
  shape->size_erase.y =		2 * shape->center_erase.y + 1;

  shape->center_vertex.x =	cell_side/2;
  shape->center_vertex.y =	shape->center_bound.y;
  shape->corner_vertex.x =	cell_side/2;
  shape->corner_vertex.y =	0;

  shape->center_rectangle.x =	cell_side/2-2;
  shape->center_rectangle.y =	shape->center_bound.y-2;
  shape->size_rectangle.x =	2 * shape->center_rectangle.x;
  shape->size_rectangle.y =	2 * shape->center_rectangle.y;

  shape->circle_bound =		shape->center_erase.y;

  shape->area =			shape->side * shape->size_bound.y +
					shape->side * shape->size_bound.y;

  shape->direction_count =	6;
  shape->direction_factor =	24/shape->direction_count;
  shape->angle_offset =		60;
  shape->use_secondary =	FALSE;

  /** Define polygon points **/

  shape->point_count =		7;
  shape->points[0].x =		0;
  shape->points[0].y =		0;
  shape->points[1].x =		shape->side;
  shape->points[1].y =		0;
  shape->points[2].x =		shape->side/2;
  shape->points[2].y =		shape->center_bound.y;
  shape->points[3].x =		-(shape->side/2);
  shape->points[3].y =		shape->center_bound.y;
  shape->points[4].x =		-shape->side;
  shape->points[4].y =		0;
  shape->points[5].x =		-(shape->side/2);
  shape->points[5].y =		-shape->center_bound.y;
  shape->points[6].x =		shape->side/2;
  shape->points[6].y =		-shape->center_bound.y;

  shape->troop_shape =		SHAPE_CIRCLE;
  shape->erase_shape =		SHAPE_CIRCLE;

  shape_set_draw_method (shape, side, TRUE);

  shape_set_growth (shape);

  shape_set_troops (shape);

  shape_set_arrows (shape, 0);
}



/******************************************************************************
  hex_set_center (cell, shape, side)

  Set the center position of <cell> of <side> with <shape>, taking into
  account any row- and column-based shifts.
******************************************************************************/
void
hex_set_center (cell_type *cell, shape_type *shape, int side)
{
  cell->x_center[side] = shape->side + cell->x*((3*shape->side)/2);

  if (cell->x%2 == 0)
    cell->y_center[side] = cell->y*shape->size_bound.y + shape->center_bound.y;
  else
    cell->y_center[side] = cell->y*shape->size_bound.y + shape->size_bound.y;
}



/******************************************************************************
  hex_set_horizons (shape)

  Set the even and odd horizon arrays for <shape>.
******************************************************************************/
void
hex_set_horizons (shape_type *shape)
{
  int i, j, k,
      xbase_even, ybase_even,
      xbase_odd, ybase_odd,
      half,
      index,
      direction_index;

  cell_type *cell_even,
            *cell_odd;

  /** Pick out a cell close to the middle of the board in even column **/

  half = Config->board_x_size/2;
  if (half%2 == 0)
  {
    cell_even = CELL2 (half, Config->board_y_size/2);
    cell_odd = CELL2 (half+1, Config->board_y_size/2);
  }
  else
  {
    cell_even = CELL2 (half+1, Config->board_y_size/2);
    cell_odd = CELL2 (half, Config->board_y_size/2);
  }

  xbase_even = cell_even->x;
  ybase_even = cell_even->y;

  xbase_odd = cell_odd->x;
  ybase_odd = cell_odd->y;

  index = 0;

  /** Step through each distance less than the view_range **/

  for (i=0; i<Config->view_range_max; i++)
  {
    /** Starting with the cell directly above the base cell **/

    cell_even = cell_even->connect[HEX_UP];
    cell_odd = cell_odd->connect[HEX_UP];

    /** Circle around the base cell **/

    for (j=0; j<6; j++)
    {
      /** For each unit away from base cell, need to do extra move **/

      direction_index = (HEX_LEFT_DOWN+j)%6;

      for (k=0; k<(i+1); k++)
      {
        /** Change even cells and set indexing array **/

        cell_even = cell_even->connect[direction_index];

        shape->horizon_even[index][0] = cell_even->x - xbase_even;
        shape->horizon_even[index][1] = cell_even->y - ybase_even;

        /** Change odd cells and set indexing array **/

        cell_odd = cell_odd->connect[direction_index];

        shape->horizon_odd[index][0] = cell_odd->x - xbase_odd;
        shape->horizon_odd[index][1] = cell_odd->y - ybase_odd;

        index++;
      }
    }

    /** Set number of cells within given range **/

    shape->horizon_counts[i+1] = index;
  }

  /** Set 0 horizon, just in case **/

  shape->horizon_counts[0] = 0;
}



/******************************************************************************
  hex_set_connections ()

  Set the intercell pointers for the given tiling.
******************************************************************************/
void
hex_set_connections (void)
{
  int i, j,
      even;

  /** For each cell, establish connections without crossing edges.  The	**/
  /** problem with hex connections is that depending on the row,	**/
  /** 2-D indexing changes.						**/

  for (j=0; j<Config->board_y_size; j++)
  {
    even = TRUE;

    for (i=0; i<Config->board_x_size; i++)
    {
      if (j != 0)
        CELL2(i,j)->connect[HEX_UP] = CELL2(i,j-1);
      else
        CELL2(i,j)->connect[HEX_UP] = CELL2(i,j);

      if (j != Config->board_y_size-1)
        CELL2(i,j)->connect[HEX_DOWN] = CELL2(i,j+1);
      else
        CELL2(i,j)->connect[HEX_DOWN] = CELL2(i,j);

      if (i != 0)
      {
        if (j != 0)
        {
          if (even)
            CELL2(i,j)->connect[HEX_LEFT_UP] = CELL2(i-1,j-1);
          else
            CELL2(i,j)->connect[HEX_LEFT_UP] = CELL2(i-1,j);
        }
        else if (!even)
          CELL2(i,j)->connect[HEX_LEFT_UP] = CELL2(i-1,j);
        else
          CELL2(i,j)->connect[HEX_LEFT_UP] = CELL2(i,j);

        if (j != Config->board_y_size-1)
        {
          if (even)
            CELL2(i,j)->connect[HEX_LEFT_DOWN] = CELL2(i-1,j);
          else
            CELL2(i,j)->connect[HEX_LEFT_DOWN] = CELL2(i-1,j+1);
        }
        else if (even)
          CELL2(i,j)->connect[HEX_LEFT_DOWN] = CELL2(i-1,j);
        else
          CELL2(i,j)->connect[HEX_LEFT_DOWN] = CELL2(i,j);
      }
      else
      {
        CELL2(i,j)->connect[HEX_LEFT_UP] = CELL2(i,j);
        CELL2(i,j)->connect[HEX_LEFT_DOWN] = CELL2(i,j);
      }

      if (i != Config->board_x_size-1)
      {
        if (j != 0)
        {
          if (even)
            CELL2(i,j)->connect[HEX_RIGHT_UP] = CELL2(i+1,j-1);
          else
            CELL2(i,j)->connect[HEX_RIGHT_UP] = CELL2(i+1,j);
        }
        else if (!even)
          CELL2(i,j)->connect[HEX_RIGHT_UP] = CELL2(i+1,j);
        else
          CELL2(i,j)->connect[HEX_RIGHT_UP] = CELL2(i,j);

        if (j != Config->board_y_size-1)
        {
          if (even)
            CELL2(i,j)->connect[HEX_RIGHT_DOWN] = CELL2(i+1,j);
          else
            CELL2(i,j)->connect[HEX_RIGHT_DOWN] = CELL2(i+1,j+1);
        }
        else if (even)
          CELL2(i,j)->connect[HEX_RIGHT_DOWN] = CELL2(i+1,j);
        else
          CELL2(i,j)->connect[HEX_RIGHT_DOWN] = CELL2(i,j);

      }
      else
      {
        CELL2(i,j)->connect[HEX_RIGHT_UP] = CELL2(i,j);
        CELL2(i,j)->connect[HEX_RIGHT_DOWN] = CELL2(i,j);
      }

      even = !even;
    }
  }

  /** If wrapping is allowed, set connections across board edges **/

  if (Config->enable_all[OPTION_WRAP])
  {
    for (i=0; i<Config->board_x_size; i++)
    {
      CELL2(i,0)->connect[HEX_UP] = CELL2(i,Config->board_y_size-1);
      CELL2(i,Config->board_y_size-1)->connect[HEX_DOWN] = CELL2(i,0);

      if (i%2 == 1)
      {
        if (i!=0)
          CELL2(i,Config->board_y_size-1)->connect[HEX_LEFT_DOWN] =
			CELL2(i-1,0);

        if (i!=Config->board_x_size-1)
          CELL2(i,Config->board_y_size-1)->connect[HEX_RIGHT_DOWN] =
			CELL2(i+1,0);
      }
      else
      {
        if (i!=0)
          CELL2(i,0)->connect[HEX_LEFT_UP] =
			CELL2(i-1,Config->board_y_size-1);

        if (i!=Config->board_x_size-1)
          CELL2(i,0)->connect[HEX_RIGHT_UP] =
			CELL2(i+1,Config->board_y_size-1);
      }
    }

    if (Config->board_x_size%2 == 0)
    {
      for (j=0; j<Config->board_y_size; j++)
      {
        if (j==0)
          CELL2(0,j)->connect[HEX_LEFT_UP] =
			CELL2(Config->board_x_size-1,Config->board_y_size-1);
        else
          CELL2(0,j)->connect[HEX_LEFT_UP] =
			CELL2(Config->board_x_size-1,j-1);

        CELL2(0,j)->connect[HEX_LEFT_DOWN] =
			CELL2(Config->board_x_size-1,j);

        CELL2(Config->board_x_size-1,j)->connect[HEX_RIGHT_UP] =
			CELL2(0,j);

        if (j == Config->board_y_size-1)
          CELL2(Config->board_x_size-1,j)->connect[HEX_RIGHT_DOWN] =
			CELL2(0,0);
        else
          CELL2(Config->board_x_size-1,j)->connect[HEX_RIGHT_DOWN] =
			CELL2(0,j+1);
      }
    }
    else
    {
    }
  }
}



/******************************************************************************
  hex_set_selects (shape, select, side) 

  Set the selection chart which indicates the basic unit of tiling.  For each
  position in the chart, indicate the offset from the canonical cell location. 
		___________________
		|  /      \        |
 	(-1,-1)	| /        \ (1,-1)|
		|/  (0,0)   \______|
		|\          /      |
 	(-1,0)	| \        / (1,0) |
		|__\______/________|
******************************************************************************/
void
hex_set_selects (shape_type *shape, select_type *select, int side)
{
  int x, y,
      x_limit;

  double fraction;
 
  select->dimension.x =		shape->size_bound.x + shape->side;
  select->dimension.y =		shape->size_bound.y;
  select->multiplier.x =	2;
  select->multiplier.y =	1;
  select->offset.x =		0;
  select->offset.y =		0;  

  /** Set default offsets **/

  for (y=0; y<select->dimension.y; y++)
    for (x=0; x<select->dimension.x; x++)
    {
      select->matrix[y][x].x =	0;
      select->matrix[y][x].y =	0;
    }

  fraction = 1.0/shape->center_bound.y;

  /** Progress from top to bottom, setting the various cell offsets **/

  for (y=0; y<=shape->center_bound.y; y++)
  {
    x_limit = shape->corner_vertex.x -
			(int)(fraction*y*shape->corner_vertex.x);

    for (x=0; x<x_limit; x++)
    {
      select->matrix[y][x].x =	-1;
      select->matrix[y][x].y =	-1;
    }

    x_limit = shape->corner_vertex.x + shape->side +
			(int)(fraction*y*shape->corner_vertex.x);

    for (x=x_limit; x<select->dimension.x; x++)
    {
      select->matrix[y][x].x =	1;
      select->matrix[y][x].y =	-1;
    }
  }


  for (; y<select->dimension.y; y++)
  {
    x_limit = shape->corner_vertex.x +
		(int)(fraction*(y-select->dimension.y)*shape->corner_vertex.x);

    for (x=0; x<x_limit; x++)
    {
      select->matrix[y][x].x =	-1;
      select->matrix[y][x].y =	0;
    }

    x_limit = shape->corner_vertex.x + shape->side +
		(int)(fraction*(select->dimension.y-y)*shape->corner_vertex.x);

    for (x=x_limit; x<select->dimension.x; x++)
    {
      select->matrix[y][x].x =	1;
      select->matrix[y][x].y =	0;
    }
  }
}
