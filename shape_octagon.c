#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef OCTAGON_DEBUG
# include <stdio.h>
#endif
#include <math.h>

#include "constant.h"
#include "extern.h"


/** Set the offset from perfect octagon-ness.  The larger this number,	**/
/** the larger the horizontal/vertical octagon sides, the larger the	**/
/** squares.  Unfortunately, the circular erase implies that diagonal	**/
/** direction vectors won't get drawn all the way to the diagonal cell	**/
/** edges if OCTAGON_OFFSET > 0.					**/

#define OCTAGON_OFFSET		1
/* FIXME: calculate this with more precision and make an entry in constant.h */
#define OCTAGON_AREA_FACTOR	1.0987


/******************************************************************************
  octagon_set_dimensions (shape, cell_size, side)

  Set all internal values of <shape> of <side>, given canonical size of
  <cell_size>.  This includes the various dimensions and offsets, polygonal
  points, direction vectors, town and troop size mappings, and drawing
  method.
******************************************************************************/
void
octagon_set_dimensions (shape_type *shape, int cell_size, int side)
{
  int offset;

  /** Set cell side to normalize cell area to cell_size*cell_size **/

  cell_size = (OCTAGON_AREA_FACTOR * cell_size + 0.5);
 
  /** Make sure cell size is odd **/

  if (cell_size%2 == 0)
    cell_size -= 1;

  /** Set offset from corner to vertex **/

  offset = cell_size/2 - OCTAGON_OFFSET -
		(int)(((double)(cell_size))/(2.0 * tan ((3.0*CONST_PI/8.0))));

  /** Set all the relevant dimensions and coordinates **/

  shape->side =			cell_size;

  shape->center_bound.x =	cell_size/2;
  shape->center_bound.y =	cell_size/2;
  shape->size_bound.x =		cell_size;
  shape->size_bound.y =		cell_size;

  shape->center_erase.x =	cell_size/2 - 1;
  shape->center_erase.y =	cell_size/2 - 1;
  shape->corner_erase.x =	1;
  shape->corner_erase.y =	1;
  shape->size_erase.x =		2*shape->center_erase.x + 1;
  shape->size_erase.y =		2*shape->center_erase.y + 1;

  shape->center_vertex.x =	cell_size/2 - offset;
  shape->center_vertex.y =	cell_size/2;
  shape->corner_vertex.x =	offset;
  shape->corner_vertex.y =	0;

  shape->center_rectangle.x =	(int)(CONST_SQ2D2*shape->center_erase.x);
  shape->center_rectangle.y =	(int)(CONST_SQ2D2*shape->center_erase.y);
  shape->size_rectangle.x =	cell_size - offset - 4*OCTAGON_OFFSET;
  shape->size_rectangle.y =	cell_size - offset - 4*OCTAGON_OFFSET;

  shape->helper.x =		cell_size - 2*offset - 1;
  shape->helper.y =		cell_size - 2*offset - 1;

  shape->circle_bound =		shape->center_erase.y;
  shape->area =			cell_size*cell_size;

  shape->direction_count =	8;
  shape->direction_factor =	24/shape->direction_count;
  shape->angle_offset =		67.5;
  shape->use_secondary =	FALSE;

  /** Define polygon points **/

  shape->point_count =		9;
  shape->points[0].x =		0;
  shape->points[0].y =		0;
  shape->points[1].x =		cell_size - 2*offset - 1;
  shape->points[1].y =		0;
  shape->points[2].x =		offset;
  shape->points[2].y =		offset;
  shape->points[3].x =		0;
  shape->points[3].y =		cell_size - 2*offset - 1;
  shape->points[4].x =		-offset;
  shape->points[4].y =		offset;
  shape->points[5].x =		-(cell_size - 2*offset - 1);
  shape->points[5].y =		0;
  shape->points[6].x =		-offset;
  shape->points[6].y =		-offset;
  shape->points[7].x =		0;
  shape->points[7].y =		-(cell_size - 2*offset - 1);
  shape->points[8].x =		offset;
  shape->points[8].y =		-offset;

  shape->troop_shape =		SHAPE_CIRCLE;
  shape->erase_shape =		SHAPE_CIRCLE;

  shape_set_draw_method (shape, side, TRUE);

  shape_set_growth (shape);

  shape_set_troops (shape);

  shape_set_arrows (shape, 0);
}



/******************************************************************************
  octagon_set_center (cell, shape1, shape2, side)

  Set the center position of <cell> of <side> with shapes given by <shape1> and
  <shape2>, taking into account any row- and column-based shifts.
  Things get kind of messy.
******************************************************************************/
void
octagon_set_center (cell_type *cell, shape_type *shape1, shape_type *shape2, int side)
{
  if (cell->y%2 == 0)
  {
    cell->y_center[side] = (cell->y/2) *
			(shape1->side + shape1->helper.y - 1) + shape1->side/2;

    if (cell->x%2 == 0)
      cell->x_center[side] = (cell->x/2) *
			(shape1->side + shape1->helper.x - 1) + shape1->side/2;
    else
      cell->x_center[side] = (cell->x/2) *
			(shape1->side + shape1->helper.x - 1) +
			shape1->side + shape2->side/2 - 1;
  }
  else
  {
    cell->y_center[side] = (cell->y/2) * (shape1->side + shape1->helper.y - 1) +
			shape1->side + shape2->side/2 - 1;

    if (cell->x%2 == 0)
      cell->x_center[side] = (cell->x/2) *
			(shape1->side + shape1->helper.x - 1) + shape1->side/2;
    else
      cell->x_center[side] = (cell->x/2) *
			(shape1->side + shape1->helper.x - 1) +
			shape1->side/2 + shape2->side/2 + shape1->side/2;
  }

  /** Set the shape index corresponding to octagon or square **/

  if (cell->y%2 == 0)
  {
    if (cell->x%2 == 0)
      cell->shape_index = 0;
    else
      cell->shape_index = 1;
  }
  else
  {
    if (cell->x%2 == 0)
      cell->shape_index = 1;
    else
      cell->shape_index = 0;
  }
}



/******************************************************************************
  octagon_set_horizons (shape1, shape2)

  Set the even and odd horizon arrays for <shape1> and <shape2>.  Since a
  strict rectangular grid is used for this tiling, the square tiling algorithm
  is used for horizon determination for both shapes.
******************************************************************************/
void
octagon_set_horizons (shape_type *shape1, shape_type *shape2)
{
  square_set_horizons (shape1);
  square_set_horizons (shape2);
}



/******************************************************************************
  octagon_set_connections ()

  Set the intercell pointers for the given tiling.
******************************************************************************/
void
octagon_set_connections (void)
{
  int i, j;

  cell_type *cell;

  /** Set cell connections as if they were all square **/

  square_set_connections ();

  /** For each cell, establish connections, making sure to exclude	**/
  /** connections across board edges.					**/

  for (j=0; j<Config->board_y_size; j++)
  {
    for (i=0; i<Config->board_x_size; i++)
    {
      cell = CELL2(i,j);

      if (cell->shape_index == 0)
      {
        cell->connect[OCT_RIGHT] = cell->connect[SQUARE_RIGHT];
        cell->connect[OCT_DOWN] = cell->connect[SQUARE_DOWN];
        cell->connect[OCT_LEFT] = cell->connect[SQUARE_LEFT];
        cell->connect[OCT_UP] = cell->connect[SQUARE_UP];

        if (i == Config->board_x_size-1 || j == 0)
          cell->connect[OCT_RIGHT_UP] = cell;
        else
          cell->connect[OCT_RIGHT_UP] = CELL2(i+1,j-1);

        if (i == Config->board_x_size-1 || j == Config->board_y_size-1)
          cell->connect[OCT_RIGHT_DOWN] = cell;
        else
          cell->connect[OCT_RIGHT_DOWN] = CELL2(i+1,j+1);

        if (i == 0 || j == 0)
          cell->connect[OCT_LEFT_UP] = cell;
        else
          cell->connect[OCT_LEFT_UP] = CELL2(i-1,j-1);

        if (i == 0 || j == Config->board_y_size-1)
          cell->connect[OCT_LEFT_DOWN] = cell;
        else
          cell->connect[OCT_LEFT_DOWN] = CELL2(i-1,j+1);
      }
    }
  }

  /** If wrapping is allowed, set connections across board edges **/

  if (Config->enable_all[OPTION_WRAP])
  {
    if (Config->board_x_size%2 == 0)
    {
      for (j=0; j<Config->board_y_size; j+=2)
      {
        cell = CELL2(0,j);

        if (j != 0)
          cell->connect[OCT_LEFT_UP] = CELL2(Config->board_x_size-1,j-1);

        if (j != Config->board_y_size-1)
          cell->connect[OCT_LEFT_DOWN] = CELL2(Config->board_x_size-1,j+1);

        if (j != Config->board_y_size-1)
        {
          cell = CELL2(Config->board_x_size-1,j+1);

          cell->connect[OCT_RIGHT_UP] = CELL2(0,j);

          if (j+2 < Config->board_y_size)
            cell->connect[OCT_RIGHT_DOWN] = CELL2(0,j+2);
        }
      }
    }

    if (Config->board_y_size%2 == 0)
    {
      for (i=0; i<Config->board_x_size; i+=2)
      {
        cell = CELL2(i,0);

        if (i != 0)
          cell->connect[OCT_LEFT_UP] = CELL2(i-1,Config->board_y_size-1);

        if (i != Config->board_x_size-1)
          cell->connect[OCT_RIGHT_UP] = CELL2(i+1,Config->board_y_size-1);

        if (i != Config->board_x_size-1)
        {
          cell = CELL2(i+1,Config->board_y_size-1);

          cell->connect[OCT_LEFT_DOWN] = CELL2(i,0);

          if (i+2 < Config->board_x_size)
            cell->connect[OCT_RIGHT_DOWN] = CELL2(i+2,0);
        }
      }
    }
  }
}



/******************************************************************************
  octagon_set_selects (shape1, shape2, select, side)

  Set the selection chart which indicates the basic unit of tiling.  For each
  position in the chart, indicate the offset from the canonical cell location. 
	 	 _________________
  	-1,-1	| /      \  1,-1 | 
 		|/        \ _____|
 		|          |     | 
 		|   0,0    | 1,0 | 
 		|          |_____|
 		|\        /      |
 		| \______/       |
   	-1,1	|  |     |       |
 		|  | 0,1 |  1,1  |
 		|__|_____|_______|
******************************************************************************/
void
octagon_set_selects (shape_type *shape1, shape_type *shape2, select_type *select, int side)
{
  int x, y,
      y_offset,
      xlimit, ylimit;

  select->dimension.x =		shape1->size_bound.x + shape2->size_bound.x-1;
  select->dimension.y =		shape1->size_bound.y + shape2->size_bound.y-1;
  select->multiplier.x =	2;
  select->multiplier.y =	2;

  select->offset.x =		1;
  select->offset.y =		1;

  /** Set the default offsets **/

  for (y=0; y<select->dimension.y; y++)
    for (x=0; x<select->dimension.x; x++)
    {
      select->matrix[y][x].x =	0;
      select->matrix[y][x].y =	0;
    }

  /** Progressing from top to bottom, set various offsets **/

  for (y=0; y<shape1->corner_vertex.x; y++)
  {
    xlimit = shape1->corner_vertex.x - y;

    for (x=0; x<xlimit; x++)
    {
      select->matrix[y][x].x =	-1;
      select->matrix[y][x].y =	-1;
    }

    xlimit = shape1->size_bound.x - shape1->corner_vertex.x + y;

    for (x=xlimit; x<select->dimension.x; x++)
    {
      select->matrix[y][x].x =	1;
      select->matrix[y][x].y =	-1;
    }
  }

  ylimit = shape1->size_bound.y - shape1->corner_vertex.x;

  for (; y<ylimit; y++)
  {
    for (x=shape1->size_bound.x; x<select->dimension.x; x++)
    {
      select->matrix[y][x].x =	1;
      select->matrix[y][x].y =	0;
    }
  }

  for (y_offset=0; y<shape1->size_bound.y; y++, y_offset++)
  {
    xlimit = y_offset;

    for (x=0; x<xlimit; x++)
    {
      select->matrix[y][x].x =	-1;
      select->matrix[y][x].y =	1;
    }

    xlimit = shape1->size_bound.x - y_offset;

    for (x=xlimit; x<select->dimension.x; x++)
    {
      select->matrix[y][x].x =	1;
      select->matrix[y][x].y =	1;
    }
  }

  for (; y<select->dimension.y; y++)
  {
    for (x=0; x<shape1->corner_vertex.x; x++)
    {
      select->matrix[y][x].x =	-1;
      select->matrix[y][x].y =	1;
    }

    xlimit = shape1->size_bound.x - shape1->corner_vertex.x;

    for (; x<xlimit; x++)
    {
      select->matrix[y][x].x =	0;
      select->matrix[y][x].y =	1;
    }

    for (; x<select->dimension.x; x++)
    {
      select->matrix[y][x].x =	1;
      select->matrix[y][x].y =	1;
    }
  }

#ifdef OCTAGON_DEBUG
  /** Keep this generic debugging display around for future use **/
  for (y=0; y<select->dimension.y; y++)
  {
    for (x=0; x<select->dimension.x; x++)
    {
      x_offset = select->matrix[y][x].x;
      y_offset = select->matrix[y][x].y;

      if (x_offset == 0 && y_offset == 0)
        printf (".");
      else if (x_offset == -1 && y_offset == -1)
        printf ("!");
      else if (x_offset == -1 && y_offset == 0)
        printf ("&");
      else if (x_offset == -1 && y_offset == 1)
        printf ("@");
      else if (x_offset == 0 && y_offset == 1)
        printf ("b");
      else if (x_offset == 0 && y_offset == -1)
        printf ("?");
      else if (x_offset == 1 && y_offset == -1)
        printf ("(");
      else if (x_offset == 1 && y_offset == 0)
        printf ("l");
      else if (x_offset == 1 && y_offset == 1)
        printf ("$");
    }
    printf ("\n");
  }
#endif
}



/******************************************************************************
  octagon_set_square_troops (shape1, shape2)

  Set the max_value and max_max_value fields of the square cell <shape2>, as
  determined by the octagonal cell <shape1>.
******************************************************************************/
void
octagon_set_square_troops (shape_type *shape1, shape_type *shape2)
{
  int i,
    max_size;

  for (i=0; i<shape1->max_max_value+2; i++)
    shape2->troop_to_size[i] = shape1->troop_to_size[i];

  max_size = (int)(TROOP_MAX_FRACTION * 2 * shape2->circle_bound);
  if (max_size > 2*shape2->circle_bound - TROOP_MIN_BUFFER)
    max_size = 2*shape2->circle_bound - TROOP_MIN_BUFFER;

  for (i=0; shape2->troop_to_size[i] < max_size; i++);

  shape2->max_value = i-1;
  shape2->max_max_value = i-1;

  shape_set_arrows (shape2, 0);
}
