#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "constant.h"
#include "extern.h"


#define DIAMOND_AREA_FACTOR  CONST_2DSQ2

/******************************************************************************
  diamond_set_dimensions (shape, cell_size, side)

  Set all internal values of <shape> of <side>, given canonical size of
  <cell_size>.  This includes the various dimensions and offsets, polygonal
  points, direction vectors, town and troop size mappings, and drawing
  method.
******************************************************************************/
void
diamond_set_dimensions (shape_type *shape, int cell_size, int side)
{
  int half_cell_size;


  /** Set cell side to normalize cell area to cell_size*cell_size **/
 
  cell_size = (DIAMOND_AREA_FACTOR * cell_size + 0.5); 

  /** Make all cells have odd dimension **/

  if (cell_size%2 == 0)
    cell_size -= 1;

  half_cell_size = cell_size/2;

  /** Set all the relevant dimensions and coordinates **/

  shape->side =			cell_size;

  shape->center_bound.x =	half_cell_size;
  shape->center_bound.y =	half_cell_size;
  shape->size_bound.x =		cell_size;
  shape->size_bound.y =		cell_size;

  shape->center_erase.x =	(int)(half_cell_size * CONST_SQ2D2 + 0.5) - 1;
  shape->center_erase.y =	(int)(half_cell_size * CONST_SQ2D2 + 0.5) - 1;
  shape->corner_erase.x =	(int)(half_cell_size * CONST_SQ2D2 + 0.5) + 1;
  shape->corner_erase.y =	(int)(half_cell_size * CONST_SQ2D2 + 0.5) + 1;
  shape->size_erase.x =		2*shape->center_erase.x;
  shape->size_erase.y =		2*shape->center_erase.y;

  shape->center_vertex.x =	0;
  shape->center_vertex.y =	half_cell_size;
  shape->corner_vertex.x =	half_cell_size;
  shape->corner_vertex.y =	0;

  shape->center_rectangle.x =	(int)(CONST_SQ2D2*shape->center_erase.x);
  shape->center_rectangle.y =	(int)(CONST_SQ2D2*shape->center_erase.y);
  shape->size_rectangle.x =	2*shape->center_rectangle.x;
  shape->size_rectangle.y =	2*shape->center_rectangle.y;

  shape->circle_bound =		shape->center_erase.x;
  shape->area =			cell_size*cell_size/2;

  shape->direction_count =	4;
  shape->direction_factor =	24/shape->direction_count;
  shape->angle_offset =		0;
  shape->use_secondary =	TRUE;

  /** Define polygon points **/

  shape->point_count =		5;
  shape->points[0].x =		0;
  shape->points[0].y =		0;
  shape->points[1].x =		half_cell_size;
  shape->points[1].y =		half_cell_size;
  shape->points[2].x =		-half_cell_size;
  shape->points[2].y =		half_cell_size;
  shape->points[3].x =		-half_cell_size;
  shape->points[3].y =		-half_cell_size;
  shape->points[4].x =		half_cell_size;
  shape->points[4].y =		-half_cell_size;

  shape->troop_shape =		SHAPE_CIRCLE;
  shape->erase_shape =		SHAPE_CIRCLE;

  shape_set_draw_method (shape, side, TRUE);

  shape_set_growth (shape);

  shape_set_troops (shape);

  shape_set_arrows (shape, 0);
}



/******************************************************************************
  diamond_set_center (cell, shape, side)

  Set the center position of <cell> of <side> with <shape>, taking into
  account any row- and column-based shifts.
******************************************************************************/
void
diamond_set_center (cell_type *cell, shape_type *shape, int side)
{
  cell->y_center[side] = cell->y * (shape->side-1) + shape->side/2;
  cell->x_center[side] = cell->x * (shape->side/2) + shape->side/2;

  if (cell->x%2 == 1)
    cell->y_center[side] += shape->side/2;
}



/******************************************************************************
  diamond_set_horizons (shape)

  Set the even and odd horizon arrays for <shape>.
******************************************************************************/
void
diamond_set_horizons (shape_type *shape)
{
  int i, j, k,
      xbase_even, ybase_even,
      xbase_odd, ybase_odd,
      half,
      index,
      limit,
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
 
    cell_even = cell_even->connect[SQUARE_UP];
    cell_odd = cell_odd->connect[SQUARE_UP];

    /** Circle around the base cell **/

    for (j=0; j<5; j++)
    {
      /** For each unit away from base cell, need to do extra move **/

      direction_index = (SQUARE_LEFT+j)%4;

      if (j == 0 || j == 4)
        limit = i + 1;
      else
        limit = 2*i + 2;

      for (k=0; k<limit; k++)
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
  diamond_set_connections ()

  Set the intercell pointers for the given tiling.
******************************************************************************/
void
diamond_set_connections (void)
{
  int i, j,
      even;

  cell_type *cell;

  /** For each cell, establish connections without crossing edges.  The **/
  /** problem with diamond connections is that depending on the row,    **/
  /** 2-D indexing changes.                                             **/

  for (j=0; j<Config->board_y_size; j++)
  {
    even = TRUE;

    for (i=0; i<Config->board_x_size; i++)
    {
      cell = CELL2(i,j);

      if (even)
      {
        if (j == 0)
          cell->connect[SQUARE_UP] = cell;
        else if (i == Config->board_x_size-1)
          cell->connect[SQUARE_UP] = cell;
        else
          cell->connect[SQUARE_UP] = CELL2(i+1,j-1);

        if (i == Config->board_x_size-1)
          cell->connect[SQUARE_RIGHT] = cell;
        else
          cell->connect[SQUARE_RIGHT] = CELL2(i+1,j);

        if (i == 0)
          cell->connect[SQUARE_DOWN] = cell;
        else
          cell->connect[SQUARE_DOWN] = CELL2(i-1,j);

        if (j == 0)
          cell->connect[SQUARE_LEFT] = cell;
        else if (i == 0)
          cell->connect[SQUARE_LEFT] = cell;
        else
          cell->connect[SQUARE_LEFT] = CELL2(i-1,j-1);
      }
      else
      {
        if (i == Config->board_x_size-1)
          cell->connect[SQUARE_UP] = cell;
        else
          cell->connect[SQUARE_UP] = CELL2(i+1,j);

        if (j == Config->board_y_size-1)
          cell->connect[SQUARE_RIGHT] = cell;
        else if (i == Config->board_x_size-1)
          cell->connect[SQUARE_RIGHT] = cell;
        else
          cell->connect[SQUARE_RIGHT] = CELL2(i+1,j+1);

        if (j == Config->board_y_size-1)
          cell->connect[SQUARE_DOWN] = cell;
        else
          cell->connect[SQUARE_DOWN] = CELL2(i-1,j+1);

        cell->connect[SQUARE_LEFT] = CELL2(i-1,j);
      }

      even = !even;
    }
  }  

  /** If wrapping is allowed, set connections across board edges **/
 
  if (Config->enable_all[OPTION_WRAP])
  {
    /*
     * odd width:
     *     /\  /\  /\  /\  /\
     * e1 / @\/@@\/@@\/@@\/@ \
     *    \  /\  /\  /\  /\  /
     *  o1 \/  \/  \/  \/  \/
     *     /\  /\  /\  /\  /\
     * e2 /  \/  \/  \/  \/  \
     *    \  /\  /\  /\  /\  /
     *  o2 \/  \/  \/  \/  \/         L/\U
     *      \@@/\@@/\@@/\@@/          D\/R
     *       \/  \/  \/  \/   
     *
     * even width:
     *     /\  /\  /\  /\    
     * e1 /@.\/..\/..\/..\
     *    \@ /\  /\  /\  /\   (. means same connection as odd board)
     *  o1 \/  \/  \/  \/ @\
     *     /\  /\  /\  /\ @/ 
     * e2 /@ \/  \/  \/  \/
     *    \@ /\  /\  /\  /\   
     *  o2 \/  \/  \/  \/ @\
     *      \../\../\../\.@/
     *       \/  \/  \/  \/
     *
     * Note the bottom right connection can point to the to left or
     * right but must always be connected to something.
     */

    /** Vertical wrapping works for any board size **/

    /** Do top row (actually even cells of top row) **/

    for (i=0; i<Config->board_x_size; i+=2)
    {
      cell = CELL2(i,0);

      if (i!=0)
        cell->connect[SQUARE_LEFT] = CELL2(i-1,Config->board_y_size-1);

      if (i!=Config->board_x_size-1)
        cell->connect[SQUARE_UP] = CELL2(i+1,Config->board_y_size-1);
    }

    /** Do bottom row (actually odd cells of bottom row) **/

    for (i=1; i<Config->board_x_size; i+=2)
    {
      cell = CELL2(i,Config->board_y_size-1);

      cell->connect[SQUARE_RIGHT] = CELL2(i+1,0);
      cell->connect[SQUARE_DOWN] = CELL2(i-1,0); /* may be changed below if even */
    }

    /** Only do horizontal wrapping if even board width **/

    if (Config->board_x_size%2 == 0)
    {
      for (j=0; j<Config->board_y_size; j++)
      {
        /** Do left column **/

        cell = CELL2(0,j);

        cell->connect[SQUARE_DOWN] = CELL2(Config->board_x_size-1,j);

        if (j!=0)
          cell->connect[SQUARE_LEFT] = CELL2(Config->board_x_size-1,j-1);
        else
          cell->connect[SQUARE_LEFT] = CELL2(Config->board_x_size-1,Config->board_y_size-1);

        /** Do right column **/

        cell = CELL2(Config->board_x_size-1,j);

        if (j!=Config->board_y_size-1)
          cell->connect[SQUARE_RIGHT] = CELL2(0,j+1);
        else
          cell->connect[SQUARE_RIGHT] = CELL2(0,0);

        cell->connect[SQUARE_UP] = CELL2(0,j);
      }
    }
  }  
}



/******************************************************************************
  diamond_set_selects (shape, select, side)

  Set the selection chart which indicates the basic unit of tiling.  For each
  position in the chart, indicate the offset from the canonical cell location. 
                ________
                |  /\  |
        (-1,-1) | /  \ | (1,-1)
                |/0,0 \|
                |\    /|
        (-1,0)  | \  / | (1,0)
                |__\/__|
******************************************************************************/
void
diamond_set_selects (shape_type *shape, select_type *select, int side)
{
  int x, y,
      half_side;

  select->dimension.x =		shape->size_bound.x;
  select->dimension.y =		shape->size_bound.y;
  select->multiplier.x =	2;
  select->multiplier.y =	1;
  select->offset.x =		1;
  select->offset.y =		1;

  /** Set default offsets **/

  for (y=0; y<select->dimension.y; y++)
    for (x=0; x<select->dimension.x; x++)
    {
      select->matrix[y][x].x =	0;
      select->matrix[y][x].y =	0;
    }

  half_side = select->dimension.x/2;

  /** Moving from top to bottom, set the four corner offsets **/

  for (y=0; y<half_side; y++)
  {
    for (x=0; x<half_side-y; x++)
    {
      select->matrix[y][x].x = -1;
      select->matrix[y][x].y = -1;
    }

    for (x=shape->size_bound.x/2+1+y; x<shape->size_bound.x; x++)
    {
      select->matrix[y][x].x = 1;
      select->matrix[y][x].y = -1;
    }

    for (x=0; x<=y; x++)
    {
      select->matrix[half_side+1+y][x].x = -1;
      select->matrix[half_side+1+y][x].y = 0;
    }

    for (x=shape->size_bound.x-y-1; x<shape->size_bound.x; x++)
    {
      select->matrix[half_side+1+y][x].x = 1;
      select->matrix[half_side+1+y][x].y = 0;
    }
  }
}
