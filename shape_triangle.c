#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "constant.h"
#include "extern.h"


/* FIXME: calculate this with more precision and make an entry in constant.h */
#define TRIANGLE_AREA_FACTOR	1.5197

/******************************************************************************
  triangle_set_dimensions (shape, cell_size, side, point_up)

  Set all internal values of <shape> of <side>, given canonical size of
  <cell_size>.  This includes the various dimensions and offsets, polygonal
  points, direction vectors, town and troop size mappings, and drawing
  method.  If <point_up>, triangle should ... point up.
******************************************************************************/
void
triangle_set_dimensions (shape_type *shape, int cell_size, int side, int point_up)
{
  int height, center,
    half_cell_size;

  /** Set cell side to normalize cell area to cell_size*cell_size **/
 
  cell_size = (TRIANGLE_AREA_FACTOR * cell_size + 0.5); 
 
  /** Make all cells have odd dimension **/

  if (cell_size%2 == 0)
    cell_size -= 1;

  height = (int)(CONST_SQ3D2 * cell_size + 0.5);
  center = (int)(CONST_SQ3D6 * cell_size + 0.5);

  half_cell_size = cell_size/2;

  /** Set all the relevant dimensions and coordinates **/

  shape->side =			cell_size;

  shape->center_bound.x =	half_cell_size;
  if (point_up)
    shape->center_bound.y =	height - center;
  else
    shape->center_bound.y =	center;
  shape->size_bound.x =		cell_size;
  shape->size_bound.y =		height;

  shape->center_erase.x =	center - 2;
  shape->center_erase.y =	center - 2;
  shape->corner_erase.x =	shape->center_bound.x - shape->center_erase.x;
  shape->corner_erase.y =	shape->center_bound.y - shape->center_erase.y;
  shape->size_erase.x =		2*shape->center_erase.x;
  shape->size_erase.y =		2*shape->center_erase.y + 1;

  if (point_up)
  {
    shape->center_vertex.x =	0;
    shape->center_vertex.y =	cell_size - center;
    shape->corner_vertex.x =	half_cell_size;
    shape->corner_vertex.y =	0;
  }
  else
  {
    shape->center_vertex.x =	shape->center_bound.x;
    shape->center_vertex.y =	shape->center_bound.y;
    shape->corner_vertex.x =	0;
    shape->corner_vertex.y =	0;
  }

  shape->center_rectangle.x =	(int)(CONST_SQ2D2*shape->center_erase.x);
  shape->center_rectangle.y =	(int)(CONST_SQ2D2*shape->center_erase.y);
  shape->size_rectangle.x =	2*shape->center_rectangle.x;
  shape->size_rectangle.y =	2*shape->center_rectangle.y;

  shape->circle_bound =		shape->center_erase.x;
  shape->area =			cell_size*cell_size/2;

  shape->direction_count =	3;
  shape->direction_factor =	24/shape->direction_count;
  if (point_up)
    shape->angle_offset =	90;
  else
    shape->angle_offset =	30;
  shape->use_secondary =	FALSE;

  /** Define polygon points **/

  if (point_up)
  {
    shape->point_count =	4;
    shape->points[0].x =	0;
    shape->points[0].y =	0;
    shape->points[1].x =	half_cell_size;
    shape->points[1].y =	height;
    shape->points[2].x =	-cell_size+1;
    shape->points[2].y =	0;
    shape->points[3].x =	half_cell_size;
    shape->points[3].y =	-height;
  }
  else
  {
    shape->point_count =	4;
    shape->points[0].x =	0;
    shape->points[0].y =	0;
    shape->points[1].x =	cell_size-1;
    shape->points[1].y =	0;
    shape->points[2].x =	-half_cell_size;
    shape->points[2].y =	height;
    shape->points[3].x =	-half_cell_size;
    shape->points[3].y =	-height;
  }


  shape->troop_shape =		SHAPE_CIRCLE;
  shape->erase_shape =		SHAPE_CIRCLE;

  shape_set_draw_method (shape, side, TRUE);

  shape_set_growth (shape);

  shape_set_troops (shape);

  shape_set_arrows (shape, 0);
}



/******************************************************************************
  triangle_set_center (cell, shape1, shape2, side)

  Set the center position of <cell> of <side> with triangle shapes <shape1> and
  <shape2>, taking into account any row- and column-based shifts.
******************************************************************************/
void
triangle_set_center (cell_type *cell, shape_type *shape1, shape_type *shape2, int side)
{
  cell->y_center[side] = cell->y * shape1->size_bound.y;
  cell->x_center[side] = (cell->x/2) * (shape1->size_bound.x-1) +
		shape1->center_bound.x;

  cell->shape_index = (cell->y%2 + cell->x%2)%2;

  if (cell->x%2)
    cell->x_center[side] += shape1->center_bound.x;

  if (cell->shape_index == 0)
    cell->y_center[side] += shape1->center_bound.y;
  else
    cell->y_center[side] += shape2->center_bound.y;
}



/******************************************************************************
  triangle_set_horizons (shape, point_up)

  Set the even and odd horizon arrays for <shape>.  If <point_up>,  triangle
  points up, else it points down.
******************************************************************************/
void
triangle_set_horizons (shape_type *shape, int point_up)
{
  int i, j,
      range, count,
      index;

  index = 0;

  /** Step through each distance less than the view_range **/

  for (range=1; range<=Config->view_range_max; range++)
  {
    if (point_up)
      count = range;
    else
      count = range + 1;

    for (j = -range; j<=range; j++)
    {
      for (i = -count; i<=count; i++)
      {
        if (i == -count || i == count || j == -range || j == range ||
			i == -count+1 || i == count-1)
        {
          shape->horizon_even[index][0] = i;
          shape->horizon_even[index][1] = j;
          shape->horizon_odd[index][0] = i;
          shape->horizon_odd[index][1] = j;

          index++;
        }
      }

      if (point_up)
      {
        if (j < 0)
          count += 1;
        else if (j > 0)
          count -= 1;
      }
      else
      {
        if (j < -1)
          count += 1;
        else if (j >= 0)
          count -= 1;
      }
    }

    /** Set number of cells within given range **/

    shape->horizon_counts[range] = index;
  }

  /** Set 0 horizon, just in case **/

  shape->horizon_counts[0] = 0;
}



/******************************************************************************
  triangle_set_connections ()

  Set the intercell pointers for the given tiling.
******************************************************************************/
void
triangle_set_connections (void)
{
  int i, j;

  cell_type *cell;

  /** For each cell, establish connections without crossing edges.  The **/
  /** problem with triangle connections is that depending on the row,	**/
  /** 2-D indexing changes.						**/

  for (j=0; j<Config->board_y_size; j++)
  {
    for (i=0; i<Config->board_x_size; i++)
    {
      cell = CELL2(i,j);

      if (cell->shape_index == 0)
      {
        if (i == Config->board_x_size-1)
          cell->connect[TRI_RIGHT_UP] = cell;
        else
          cell->connect[TRI_RIGHT_UP] = CELL2(i+1,j);

        if (j == Config->board_y_size-1)
          cell->connect[TRI_DOWN] = cell;
        else
          cell->connect[TRI_DOWN] = CELL2(i,j+1);

        if (i == 0)
          cell->connect[TRI_LEFT_UP] = cell;
        else
          cell->connect[TRI_LEFT_UP] = CELL2(i-1,j);
      }
      else
      {
        if (i == Config->board_x_size-1)
          cell->connect[TRI_RIGHT_DOWN] = cell;
        else
          cell->connect[TRI_RIGHT_DOWN] = CELL2(i+1,j);

        if (j == 0)
          cell->connect[TRI_UP] = cell;
        else
          cell->connect[TRI_UP] = CELL2(i,j-1);

        if (i == 0)
          cell->connect[TRI_LEFT_DOWN] = cell;
        else
          cell->connect[TRI_LEFT_DOWN] = CELL2(i-1,j);
      }
    }
  }  

  /** If wrapping is allowed, set connections across board edges **/
 
  if (Config->enable_all[OPTION_WRAP])
  {
    /** Only do vertical wrapping if even board height **/

    if (Config->board_y_size%2 == 0)
    {
      for (i=0; i<Config->board_x_size; i++)
      {
        cell = CELL2(i,0);

        if (cell->shape_index == 1)
          cell->connect[TRI_UP] = CELL2(i,Config->board_y_size-1);

        cell = CELL2(i,Config->board_y_size-1);

        if (cell->shape_index == 0)
          cell->connect[TRI_DOWN] = CELL2(i,0);
      }
    }

    /** Only do horizontal wrapping if even board width **/

    if (Config->board_x_size%2 == 0)
    {
      for (j=0; j<Config->board_y_size; j++)
      {
        cell = CELL2(0,j);

        if (cell->shape_index == 0)
          cell->connect[TRI_LEFT_UP] = CELL2(Config->board_x_size-1,j);
        else
          cell->connect[TRI_LEFT_DOWN] = CELL2(Config->board_x_size-1,j);

        cell = CELL2(Config->board_x_size-1,j);

        if (cell->shape_index == 0)
          cell->connect[TRI_RIGHT_UP] = CELL2(0,j);
        else
          cell->connect[TRI_RIGHT_DOWN] = CELL2(0,j);
      }
    }
  }  
}



/******************************************************************************
  triangle_set_selects (shape, select, side)

  Set the selection chart which indicates the basic unit of tiling.  For each
  position in the chart, indicate the offset from the canonical cell location. 
		__________
		|   /\   |
 	 (-1,0)	|  /  \  | (1,0)
		| /0,0 \ |
		|/______\|
		|\      /|
		| \0,1 / |
 	 (-1,1)	|  \  /  | (1,1)
		|___\/___|
******************************************************************************/
void
triangle_set_selects (shape_type *shape, select_type *select, int side)
{
  int x, y,
      y_offset,
      x_limit;
  double fraction;

  select->dimension.x =		shape->size_bound.x;
  select->dimension.y =		2*shape->size_bound.y + 1;
  select->multiplier.x =	2;
  select->multiplier.y =	2;
  select->offset.x =		1;
  select->offset.y =		0;

  /** Set the default values **/

  for (y=0; y<select->dimension.y; y++)
    for (x=0; x<select->dimension.x; x++)
    {
      select->matrix[y][x].x =	0;
      select->matrix[y][x].y =	0;
    }

  fraction = 1.0/shape->size_bound.y;

  /** Progressing from top to bottom, set the various offsets **/

  for (y=0; y<shape->size_bound.y; y++)
  {
    x_limit = shape->corner_vertex.x -
		(int)(fraction * y * shape->corner_vertex.x);

    for (x=0; x<x_limit; x++)
    {
      select->matrix[y][x].x = -1;
      select->matrix[y][x].y = 0;
    }

    x_limit = shape->corner_vertex.x +
		(int)(fraction * y * shape->corner_vertex.x);

    for (; x<x_limit; x++)
    {
      select->matrix[y][x].x =	0;
      select->matrix[y][x].y =	0;
    }

    x_limit = select->dimension.x;

    for (; x<x_limit; x++)
    {
      select->matrix[y][x].x = 1;
      select->matrix[y][x].y = 0;
    }
  }

  for (; y<select->dimension.y; y++)
  {
    y_offset = y - shape->size_bound.y;

    x_limit = (int)(fraction * y_offset * shape->corner_vertex.x);

    for (x=0; x<x_limit; x++)
    {
      select->matrix[y][x].x = -1;
      select->matrix[y][x].y =  1;
    }

    x_limit = select->dimension.x - 1 -
		(int)(fraction * y_offset * shape->corner_vertex.x);

    for (; x<x_limit; x++)
    {
      select->matrix[y][x].x = 0;
      select->matrix[y][x].y = 1;
    }

    x_limit = select->dimension.x;

    for (; x<x_limit; x++)
    {
      select->matrix[y][x].x = 1;
      select->matrix[y][x].y = 1;
    }
  }
}
