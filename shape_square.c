#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "constant.h"
#include "extern.h"


/******************************************************************************
  square_set_dimensions (shape, cell_size, side)

  Set all internal values of <shape> of <side>, given canonical size of
  <cell_size>.  This includes the various dimensions and offsets, polygonal
  points, direction vectors, town and troop size mappings, and drawing
  method.
******************************************************************************/
void
square_set_dimensions (shape_type *shape, int cell_size, int side, int use_circle)
{

  /** Make all cells have odd dimension **/

  if (cell_size%2 == 0)
    cell_size -= 1;

  /** Set all the relevant dimensions and coordinates **/

  shape->side =			cell_size;

  shape->center_bound.x =	cell_size/2;
  shape->center_bound.y =	cell_size/2;
  shape->center_vertex.x =	cell_size/2;
  shape->center_vertex.y =	cell_size/2;

  if (Config->enable[OPTION_GRID][side])
  {
    shape->center_rectangle.x =	cell_size/2 - 1;
    shape->center_rectangle.y =	cell_size/2 - 1;
    shape->center_erase.x =	cell_size/2 - 1;
    shape->center_erase.y =	cell_size/2 - 1;
  }
  else
  {
    shape->center_erase.x =	cell_size/2;
    shape->center_erase.y =	cell_size/2;
    shape->center_rectangle.x =	cell_size/2;
    shape->center_rectangle.y =	cell_size/2;
  }

  if (Config->enable[OPTION_GRID][side])
  {
    shape->corner_erase.x =	1;
    shape->corner_erase.y =	1;
  }
  else
  {
    shape->corner_erase.x =	0;
    shape->corner_erase.y =	0;
  }
  shape->corner_vertex.x =	0;
  shape->corner_vertex.y =	0;

  shape->size_bound.x =		cell_size;
  shape->size_bound.y =		cell_size;
  if (Config->enable[OPTION_GRID][side])
  {
    shape->size_erase.x =	cell_size - 2;
    shape->size_erase.y =	cell_size - 2;
    shape->size_rectangle.x =	cell_size - 2;
    shape->size_rectangle.y =	cell_size - 2;
  }
  else
  {
    shape->size_erase.x =	cell_size;
    shape->size_erase.y =	cell_size;
    shape->size_rectangle.x =	cell_size;
    shape->size_rectangle.y =	cell_size;
  }

  shape->circle_bound =		shape->center_erase.y;
  shape->area =			cell_size*cell_size;

  shape->direction_count =	4;
  shape->direction_factor =	24/shape->direction_count;
  shape->angle_offset =		45;
  shape->use_secondary =	TRUE;

  /** Define polygon points **/

  shape->point_count =		5;
  shape->points[0].x =		0;
  shape->points[0].y =		0;
  shape->points[1].x =		shape->side-1;
  shape->points[1].y =		0;
  shape->points[2].x =		0;
  shape->points[2].y =		shape->side-1;
  shape->points[3].x =		-(shape->side-1);
  shape->points[3].y =		0;
  shape->points[4].x =		0;
  shape->points[4].y =		-(shape->side-1);

  if (use_circle)
  {
    shape->troop_shape =	SHAPE_CIRCLE;
    shape->erase_shape =	SHAPE_CIRCLE;
  }
  else
  {
    shape->troop_shape =	SHAPE_SQUARE;
    shape->erase_shape =	SHAPE_SQUARE;
  }

  shape_set_draw_method (shape, side, FALSE);

  shape_set_growth (shape);

  shape_set_troops (shape);

  shape_set_arrows (shape, 0);
}



/******************************************************************************
  square_set_center (cell, shape, side)

  Set the center position of <cell> of <side> with <shape>, taking into
  account any row- and column-based shifts.
******************************************************************************/
void
square_set_center (cell_type *cell, shape_type *shape, int side)
{
  if (Config->enable[OPTION_GRID][side])
  {
    cell->x_center[side] = cell->x * (shape->side-1) + shape->side/2;
    cell->y_center[side] = cell->y * (shape->side-1) + shape->side/2;
  }
  else
  {
    cell->x_center[side] = cell->x * shape->side + shape->side/2;
    cell->y_center[side] = cell->y * shape->side + shape->side/2;
  }
}



/******************************************************************************
  square_set_horizons (shape)

  Set the even and odd horizon arrays for <shape>.
******************************************************************************/
void
square_set_horizons (shape_type *shape)
{
  int i, j,
      index,
      range;

  index = 0;

  /** For each possible horizon range **/

  for (range=1; range<=Config->view_range_max; range++)
  {
    /** For each point in a square of side 2*range+1 **/

    for (i = -range; i<=range; i++)
      for (j = -range; j<=range; j++)
      {
        /** If cell on edge of current square **/

        if (i == -range || i == range || j == -range || j == range)
        {
          shape->horizon_even[index][0] = i;
          shape->horizon_even[index][1] = j;
          shape->horizon_odd[index][0] = i;
          shape->horizon_odd[index][1] = j;

          index++;
        }
      }

    /** Set number of cells within given range **/

    shape->horizon_counts[range] = index;
  }

  /** Set 0 horizon, just in case **/

  shape->horizon_counts[0] = 0;
}



/******************************************************************************
  square_set_connections ()

  Set the intercell pointers for the given tiling.
******************************************************************************/
void
square_set_connections (void)
{
  int i, j;

  /** For each cell, establish connections, making sure to exclude	**/
  /** connections across board edges.					**/

  for (j=0; j<Config->board_y_size; j++)
  {
    for (i=0; i<Config->board_x_size; i++)
    {
      if (j != 0)
        CELL2(i,j)->connect[SQUARE_UP] = CELL2(i,j-1);
      else
        CELL2(i,j)->connect[SQUARE_UP] = CELL2(i,j);

      if (j != Config->board_y_size-1)
        CELL2(i,j)->connect[SQUARE_DOWN] = CELL2(i,j+1);
      else
        CELL2(i,j)->connect[SQUARE_DOWN] = CELL2(i,j);

      if (i != 0)
        CELL2(i,j)->connect[SQUARE_LEFT] = CELL2(i-1,j);
      else
        CELL2(i,j)->connect[SQUARE_LEFT] = CELL2(i,j);

      if (i != Config->board_x_size-1)
        CELL2(i,j)->connect[SQUARE_RIGHT] = CELL2(i+1,j);
      else
        CELL2(i,j)->connect[SQUARE_RIGHT] = CELL2(i,j);
    }
  }

  /** If wrapping is allowed, set connections across board edges **/

  if (Config->enable_all[OPTION_WRAP])
  {
    for (i=0; i<Config->board_x_size; i++)
    {
      CELL2(i,0)->connect[SQUARE_UP] = CELL2(i,Config->board_y_size-1);
      CELL2(i,Config->board_y_size-1)->connect[SQUARE_DOWN] = CELL2(i,0);
    }

    for (j=0; j<Config->board_y_size; j++)
    {
      CELL2(0,j)->connect[SQUARE_LEFT] = CELL2(Config->board_x_size-1,j);
      CELL2(Config->board_x_size-1,j)->connect[SQUARE_RIGHT] = CELL2(0,j);
    }
  }
}



/******************************************************************************
  square_set_selects (shape, select, side)

  Set the selection chart which indicates the basic unit of tiling.  For each
  position in the chart, indicate the offset from the canonical cell location. 
  A trivial operation for square tiling.
******************************************************************************/
void
square_set_selects (shape_type *shape, select_type *select, int side)
{
  int x, y;

  select->dimension.x =		shape->size_bound.x;
  select->dimension.y =		shape->size_bound.y;
  select->multiplier.x =	1;
  select->multiplier.y =	1;

  if (Config->enable[OPTION_GRID][side])
  {
    select->offset.x =		1;
    select->offset.y =		1;
  }
  else
  {
    select->offset.x =		0;
    select->offset.y =		0;
  }

  for (y=0; y<shape->size_bound.y; y++)
    for (x=0; x<shape->size_bound.x; x++)
    {
      select->matrix[y][x].x =	0;
      select->matrix[y][x].y =	0;
    }
}
