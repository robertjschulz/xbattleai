#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
#else
# ifdef HAVE_MALLOC_H
#  include <malloc.h>
# endif
#endif
#include <math.h>

#include "constant.h"

/**** x include files ****/
#include <X11/Xlib.h> /* for XPoint */

#include "extern.h"


static void shape_set_chart (shape_type *shape);
static void shape_set_single_arrow (int length, int aux_length, int use_split,
	       		int angle, XPoint arrow_source[], XPoint arrow_dester[]);
static void shape_set_single_march (int length, int angle,
			     XPoint march_source[], XPoint march_dester[]);

/******************************************************************************
  shape_initialize ()

  Initialize all the shapes necessary for a given tiling method. Set all
  positions, connection, horizons, selection and direction charts.
******************************************************************************/
void
shape_initialize (void)
{
  int i, j,
      side;
  shape_type *shape, *shape2;

  /** Create an initialize the basic shape structures **/

  for (side=0; side<Config->side_count; side++)
  {
    Board->shapes[side][0] = (shape_type *)(malloc(sizeof(shape_type)));

    /** Set maximum values **/

    Board->shapes[side][0]->max_value = Config->max_value[side];
    Board->shapes[side][0]->max_max_value = Config->max_max_value;

    switch (Config->tile_type)
    {
      case TILE_HEX:

        hex_set_dimensions (Board->shapes[side][0],
				Config->cell_size[side], side);
        shape_set_chart (Board->shapes[side][0]);
        Board->shape_count = 1;
        break;

      /** Octagon tiling interleaves octagons (shape index 0) and small	**/
      /** squares (shape index 1).					**/

      case TILE_OCTAGON:

        octagon_set_dimensions (Board->shapes[side][0],
				Config->cell_size[side], side);

        Board->shapes[side][1] = (shape_type *)(malloc(sizeof(shape_type)));

        Board->shapes[side][1]->max_value = Board->shapes[side][0]->max_value;
        Board->shapes[side][1]->max_max_value =
				Board->shapes[side][0]->max_max_value;

        square_set_dimensions (Board->shapes[side][1],
				Board->shapes[side][0]->helper.x+2, side, TRUE);

        octagon_set_square_troops
			(Board->shapes[side][0], Board->shapes[side][1]);

        shape_set_chart (Board->shapes[side][0]);
        shape_set_chart (Board->shapes[side][1]);
        Board->shape_count = 2;
        break;

      case TILE_DIAMOND:

        diamond_set_dimensions (Board->shapes[side][0],
				Config->cell_size[side], side);
        shape_set_chart (Board->shapes[side][0]);
        Board->shape_count = 1;
        break;

      /** Triangle tiling interleaves triangles with points up (shape	**/
      /** index 0) and triangles with points down (shape index 1).	**/

      case TILE_TRIANGLE:

        triangle_set_dimensions (Board->shapes[side][0],
				Config->cell_size[side], side, TRUE);

        Board->shapes[side][1] = (shape_type *)(malloc(sizeof(shape_type)));

        Board->shapes[side][1]->max_value = Board->shapes[side][0]->max_value;
        Board->shapes[side][1]->max_max_value =
				Board->shapes[side][0]->max_max_value;

        triangle_set_dimensions (Board->shapes[side][1],
				Config->cell_size[side], side, FALSE);
        shape_set_chart (Board->shapes[side][0]);
        shape_set_chart (Board->shapes[side][1]);
        Board->shape_count = 2;
        break;

      case TILE_SQUARE:

        square_set_dimensions (Board->shapes[side][0],
				Config->cell_size[side], side, FALSE);
        shape_set_chart (Board->shapes[side][0]);
        Board->shape_count = 1;
        break;
    }
  }

  /** Set each cell's position and shape index, dependent upon its	**/
  /** position in the board grid.					**/

  for (j=0; j<Config->board_y_size; j++)
  {
    for (i=0; i<Config->board_x_size; i++)
    {
      for (side=0; side<Config->side_count; side++)
      {
        switch (Config->tile_type)
        {
          case TILE_HEX:
            hex_set_center (CELL2(i,j), Board->shapes[side][0], side);
            break;

          case TILE_OCTAGON:
            octagon_set_center (CELL2(i,j), Board->shapes[side][0],
				Board->shapes[side][1], side);
            break;

          case TILE_DIAMOND:
            diamond_set_center (CELL2(i,j), Board->shapes[side][0], side);
            break;

          case TILE_TRIANGLE:
            triangle_set_center (CELL2(i,j), Board->shapes[side][0],
				Board->shapes[side][1], side);
            break;

          case TILE_SQUARE:
            square_set_center (CELL2(i,j), Board->shapes[side][0], side);
            break;
        }
      }
    }
  }

  /** Initialize pointers to neighboring cells **/

  switch (Config->tile_type)
  {
    case TILE_HEX:
      hex_set_connections ();
      break;

    case TILE_OCTAGON:
      octagon_set_connections ();
      break;

    case TILE_DIAMOND:
      diamond_set_connections ();
      break;

    case TILE_TRIANGLE:
      triangle_set_connections ();
      break;

    case TILE_SQUARE:
      square_set_connections ();
      break;
  }

  /** Initialize horizon arrays **/

  if (Config->enable_all[OPTION_HORIZON])
  {
    for (side=0; side<Config->side_count; side++)
    {
      switch (Config->tile_type)
      {
        case TILE_HEX:
          hex_set_horizons (Board->shapes[side][0]);
          break;

        case TILE_OCTAGON:
          octagon_set_horizons (Board->shapes[side][0],
				Board->shapes[side][1]);
          break;

        case TILE_DIAMOND:
          diamond_set_horizons (Board->shapes[side][0]);
          break;

        case TILE_TRIANGLE:
          triangle_set_horizons (Board->shapes[side][0], TRUE);
          triangle_set_horizons (Board->shapes[side][1], FALSE);
          break;

        case TILE_SQUARE:
          square_set_horizons (Board->shapes[side][0]);
          break;
      }
    }
  }

  /** Initialize selection grid which is used to determine which cell	**/
  /** corresponds to an arbitrary (x,y) coordinate.			**/

  for (side=0; side<Config->side_count; side++)
  {
    Config->selects[side] = (select_type *)(malloc(sizeof(select_type)));

    switch (Config->tile_type)
    {
      case TILE_HEX:
        hex_set_selects (Board->shapes[side][0], Config->selects[side], side);
        break;

      case TILE_OCTAGON:
        octagon_set_selects (Board->shapes[side][0],
			Board->shapes[side][1], Config->selects[side], side);
        break;

      case TILE_DIAMOND:
        diamond_set_selects (Board->shapes[side][0],
			Config->selects[side], side);
        break;

      case TILE_TRIANGLE:
        triangle_set_selects (Board->shapes[side][0],
			Config->selects[side], side);
        break;

      case TILE_SQUARE:
        square_set_selects (Board->shapes[side][0],
			Config->selects[side], side);
        break;
    }
  }

  /** Initialize size of playing board.  Depending on the tiling method	**/
  /** this may involve some complexities involving even and odd board	**/
  /** dimenions.							**/

  for (side=0; side<Config->side_count; side++)
  {
    shape = Board->shapes[side][0];
    shape2 = Board->shapes[side][1];

    switch (Config->tile_type)
    {
      case TILE_HEX:

        Board->size[side].x =
                (Config->board_x_size-1)*3*(shape->side/2) + 2*shape->side;
        Board->size[side].y = Config->board_y_size*shape->size_bound.y +
                                shape->center_bound.y + 1;
        break;

      case TILE_OCTAGON:

        Board->size[side].x = (Config->board_x_size/2) *
		 (shape->size_bound.x + shape2->size_bound.x - 2) +
		 (Config->board_x_size%2) * (shape->size_bound.x - 1) +
		 (1 - Config->board_x_size%2) * shape->corner_vertex.x + 1;

        Board->size[side].y = (Config->board_y_size/2) *
		 (shape->size_bound.y + shape2->size_bound.y - 2) +
		 (Config->board_y_size%2) * (shape->size_bound.y - 1) +
		 (1 - Config->board_y_size%2) * shape->corner_vertex.x + 1;
        break;

      case TILE_DIAMOND:

        Board->size[side].x = (Config->board_x_size+1) * (shape->side/2) + 1;
        Board->size[side].y = Config->board_y_size * (shape->side-1) +
					shape->side/2 + 1;
        break;

      case TILE_TRIANGLE:

        Board->size[side].x = (Config->board_x_size/2) *
			(shape->size_bound.x - 1);
        Board->size[side].y = Config->board_y_size * shape->size_bound.y + 1;

        if (Config->board_x_size%2)
          Board->size[side].x += shape->size_bound.x - 1;
        else
          Board->size[side].x += shape->size_bound.x/2;
        break;

      case TILE_SQUARE:

        if (Config->enable[OPTION_GRID][side])
        {  
          Board->size[side].x = Config->board_x_size * (shape->side-1) + 1;
          Board->size[side].y = Config->board_y_size * (shape->side-1) + 1;
        }  
        else
        {   
          Board->size[side].x = Config->board_x_size * shape->side + 1;
          Board->size[side].y = Config->board_y_size * shape->side + 1;
        }
        break;
    }
  }
}


/******************************************************************************
  shape_free ()

  Free resources allocated in shape_initialize().
******************************************************************************/
void
shape_free (void)
{
  int side,shape;

  for (side=0; side<Config->side_count; side++)
  {
    free(Config->selects[side]);
    for (shape=0; shape<Board->shape_count; shape++)
      free(Board->shapes[side][shape]);
  }
}


/******************************************************************************
  shape_set_draw_method (shape, side, disallow_pixmap)

  Based on OPTION_DRAW, as specified in the command line (or by default),
  set up the correct drawing method for <side>'s <shape>.  If <disallow_pixmap>
  then don't allow the DRAW_PIXMAP method (which is really only valid for
  rectangular cells).
******************************************************************************/
void
shape_set_draw_method (shape_type *shape, int side, int disallow_pixmap)
{

  /** Send warning message if user tries invalid method **/

  if (disallow_pixmap &&
		Config->value_int[OPTION_DRAW][side] == DRAW_PIXMAP)
  {
    throw_warning ("Cannot use DRAW_PIXMAP method, using DRAW_SIMPLE", NULL);
    Config->value_int[OPTION_DRAW][side] = DRAW_SIMPLE;
  }

  switch (Config->value_int[OPTION_DRAW][side])
  {
    /** 1 erase, 0 copy **/

    case DRAW_SIMPLE:

      shape->copy_method =	COPY_NONE;
      shape->erase_method =	ERASE_DRAW;
      break;

    /** 1 erase, 1 copy **/

    case DRAW_BACKING:

      shape->copy_method =	COPY_BACK;
      shape->erase_method =	ERASE_DRAW;
      break;

    /** 0 erase, 2 copy **/

    case DRAW_PIXMAP:

      shape->copy_method =	COPY_PIXMAP;
      shape->erase_method =	ERASE_NONE;
      break;

    /** 1 erase, 2 copy **/

    case DRAW_WINDOW:

      shape->copy_method =	COPY_WINDOW;
      shape->erase_method =	ERASE_DRAW;
      break;

    /** 0 erase, 4 copy **/

    case DRAW_MASKING:

      shape->copy_method =	COPY_WINDOW;
      shape->erase_method =	ERASE_MASK;
      break;
  }
}



/******************************************************************************
  shape_set_growth (shape)

  Set the array for <shape> which maps a town's growth factor into a radius,
  based on <shape->circle_bound>.  Might want to change this to use some
  fraction of <shape->center_erase.x>.
******************************************************************************/
void
shape_set_growth (shape_type *shape)
{
  int i,
      min_radius,
      max_radius;

  double growth_step,
         growth_radius;

  /** Compute the minimum and maximum radii **/

  min_radius = (int)(TOWN_MIN_FRACTION * shape->circle_bound);
  if (min_radius < TOWN_MIN_RADIUS)
    min_radius = TOWN_MIN_RADIUS;
  max_radius = (int)(TOWN_MAX_FRACTION * shape->circle_bound);

  /** Compute the floating point step between each growth factor **/

  growth_step = ((double)(max_radius - min_radius))/
				(TOWN_MAX - TOWN_MIN);
  growth_radius = TOWN_MIN_RADIUS;

  /** For each valid growth factor, set mapping **/

  for (i=TOWN_MIN; i<=TOWN_MAX; i++)
  {
    shape->growth_to_radius[i] = (int)(growth_radius + 0.5);
    growth_radius += growth_step;
  }
}



/******************************************************************************
  shape_set_troops (shape)

  Set the array for <shape> which maps the number of troops into a radius
  based on <shape->circle_bound>.  Might want to change this to use some
  fraction of <shape->center_erase.x>.
******************************************************************************/
void
shape_set_troops (shape_type *shape)
{
  int i,
      min_size,
      max_size;

  double troop_step,
         troop_size,
         full;

  /** Compute the minimum and maximum radii **/

  min_size = (int)(TROOP_MIN_FRACTION * 2 * shape->circle_bound);
  max_size = (int)(TROOP_MAX_FRACTION * 2 * shape->circle_bound);

  if (min_size < TROOP_MIN_SIZE)
    min_size = TROOP_MIN_SIZE;
  if (max_size > 2*shape->circle_bound - TROOP_MIN_BUFFER)
    max_size = 2*shape->circle_bound - TROOP_MIN_BUFFER;

  /** Compute the floating point step between each troop **/

  troop_step = ((double)(max_size - min_size))/shape->max_max_value;
  troop_size = min_size;

  /** For each valid number of troops (and then some), set mapping **/

  if (Config->enable_all[OPTION_AREA])
  {
    full = ((double)shape->max_max_value)/((shape->max_max_value+4)*
                        (shape->max_max_value+4));

    for (i=0; i<=shape->max_max_value+2; i++)
      shape->troop_to_size[i] =  (int)(max_size * sqrt (full*i) + 0.5);
  }
  else
  {
    shape->troop_to_size[0] = 0;
    for (i=1; i<=shape->max_max_value+2; i++)
    {
      shape->troop_to_size[i] =  (int)(troop_size + 0.5);
      troop_size += troop_step;
    }
  }
}



/******************************************************************************
  shape_set_chart (shape)

  Compute <shape->chart>, which determines how arbitrary (x,y) coordinates
  map into directions.  This routine assumes that directions are evenly
  distributed (by angle) around <shape->center_bound> and that direction 0
  begins at <shape->angle_offset>.
******************************************************************************/
static void
shape_set_chart (shape_type *shape)
{
  int x, y,
      i, j, k,
      x_limit, y_limit,
      is_done,
      int_angle, mod_angle,
      sector_angle;

  double angle,
         base_angle,
         vertex_angle,
         secondary_angle,
         diff_angle;

  /** Set the angle subtended by each side **/

  sector_angle = 360/shape->direction_count;

  /** Set the upper limits on x and y offsets **/

  x_limit = shape->size_bound.x - shape->center_bound.x;
  y_limit = shape->size_bound.y - shape->center_bound.y;

  /** Step through each point in the cell bounding box.  Note that y	**/
  /** must be negated to accound for the fact that on the screen, y 	**/
  /** values increase going down.					**/

  for (y = -shape->center_bound.y, j=0; y <= y_limit; y++, j++)
  {
    for (x = -shape->center_bound.x, i=0; x <= x_limit; x++, i++)
    {
      /** Determine the angle from the center of the cell to the position **/

      if (x == 0)
      {
        if (y < 0)
          base_angle = 90.0;
        else
          base_angle = 270.0;
      }
      else
      {
        base_angle = atan2 ((double)-y, (double)x);
        base_angle = base_angle * 180.0 / CONST_PI;
      }

      /** Subtract offset to Nth vertex **/

      angle = base_angle - shape->angle_offset;

      /** Put angle in range (0,360) **/
      
      if (angle < 0)
        angle = 360.0 + angle;
      else if (angle >= 360.0)
        angle = angle - 360.0;

      /** Round angle into one of shape->direction_count directions **/

      int_angle = (int)(angle);
      mod_angle = int_angle/sector_angle;

      /** Based on rounded angle, set vector direction **/

      shape->chart[i][j][0] = mod_angle;

      /** Now we must set the "secondary" angles which determine the	**/
      /** location of corner clicks which yield dual vectors.		**/

      /** Set the angle to the counterclockwise vertex of the 0 side **/

      angle = base_angle - shape->angle_offset - sector_angle;

      /** Put angle in range (0,360) **/
      
      if (angle < 0)
        angle = 360.0 + angle;
      else if (angle >= 360.0)
        angle = angle - 360.0;

      /** Compute the range of the secondary angle **/

      secondary_angle = ((double)sector_angle)/4.0;
      vertex_angle = 0.0;

      /** Step through each vertex and see if angle falls within the	**/
      /** secondary angle of it.					**/

      for (k=0, is_done=FALSE; k<shape->direction_count && !is_done; k++)
      {
        diff_angle = angle - vertex_angle;

        if (diff_angle > 180.0)
          diff_angle = diff_angle - 360.0;

        if (diff_angle < 0 && diff_angle > -secondary_angle)
        {
          shape->chart[i][j][1] = (k + 1)%(shape->direction_count);
          is_done = TRUE;
        }
        else if (diff_angle >= 0 && diff_angle < secondary_angle)
        {
          shape->chart[i][j][1] = k;
          is_done = TRUE;
        }

        vertex_angle += sector_angle;        
      }

      /** If point doesn't fall within angle, set to null **/

      if (!is_done)
        shape->chart[i][j][1] = -1;

      /** If position is closer than <Config->center_size> to the	**/
      /** center of the cell, eliminate all directions.			**/

      if (x*x + y*y < 2*Config->center_size*Config->center_size)
      {
        shape->chart[i][j][0] = -1;
        shape->chart[i][j][1] = -1;
      }
    }
  }

  /** Uncomment this stuff if you want to print out primary and/or	**/
  /** secondary direction charts.					**/

/**
  printf ("\n");
  for (y = -shape->center_bound.y, j=0; y <= y_limit; y++, j++)
  {
    for (x = -shape->center_bound.x, i=0; x <= x_limit; x++, i++)
    {
      if (shape->chart[i][j][0] < 0)
        printf (".");
      else
        printf ("%1d", shape->chart[i][j][0]);
    }
    printf ("\n");
  }
  printf ("\n");
  printf ("\n");

  for (y = -shape->center_bound.y, j=0; y <= y_limit; y++, j++)
  {
    for (x = -shape->center_bound.x, i=0; x <= x_limit; x++, i++)
    {
      if (shape->chart[i][j][1] < 0)
        fprintf (stderr, ".");
      else
        fprintf (stderr, "%1d", shape->chart[i][j][1]);
    }
    fprintf (stderr, "\n");
  }
  printf ("\n");
**/
}



/******************************************************************************
  shape_set_arrows (shape, offset)

  Assuming that <shape> is a regular polygon, set the coordinates for all its
  direction and marching vectors.  If dealing with an oblique angle, subtract
  <offset> from the arrow length.  This is currently used for octagon
  diagonals, when OCTAGON_OFFSET can warp the octagon such that it is not
  a regular polygon.
******************************************************************************/
void
shape_set_arrows (shape_type *shape, int offset)
{
  int i, k,
      angle, angle_step, angle_base,
      width,
      troop_size,
      max_troop_size;

  /** Determine the step between vector angles and the starting angle **/

  angle_step = 360/shape->direction_count;
  angle_base = (int)(shape->angle_offset + (180.0/shape->direction_count) + 0.5);

  /** Have to set full-length and half-length direction vectors **/

  for (i=0; i<2; i++)
  {
    /** Set length of direction vector **/

    if (i == 0)
      width = shape->center_erase.x;
    else
      width = shape->center_erase.x/2;

    /** For each direction **/

    angle = angle_base;
    for (k=0; k<shape->direction_count; k++)
    {
      /** Set direction vector, subtracting offset when appropriate **/

      if (i == 1 || angle == 0 || angle == 90 || angle == 180 ||
			angle == 270 || angle == 360)
        shape_set_single_arrow (width, 0, FALSE, angle,
			shape->arrow_source[k][i], shape->arrow_dester[k][i]);
      else
        shape_set_single_arrow (width-offset, 0, FALSE, angle,
			shape->arrow_source[k][i], shape->arrow_dester[k][i]);

      /** Increment angle **/

      angle += angle_step;
      if (angle >= 360)
        angle -= 360;
    }
  }

  /** Set length of direction vector **/

  width = shape->center_erase.x;
  max_troop_size = shape->troop_to_size[shape->max_max_value+2];

  for (i=0; i<=max_troop_size/2; i++)
  {
    /** Set inset vector length **/

    troop_size = i;

    /** For each direction **/

    angle = angle_base;
    for (k=0; k<shape->direction_count; k++)
    {
      /** Set direction vector, subtracting offset when appropriate **/

      if (i>=MAX_MAXVAL)
        fprintf(stderr,"BUG: going to go outside of arrow_source_x, size=%d dir=%d angle=%d\n",i,k,angle);

      if (i == 1 || angle == 0 || angle == 90 || angle == 180 ||
			angle == 270 || angle == 360)
        shape_set_single_arrow (width, troop_size, TRUE, angle,
		shape->arrow_source_x[i][k], shape->arrow_dester_x[i][k]);
      else
        shape_set_single_arrow (width-offset, troop_size, TRUE, angle,
		shape->arrow_source_x[i][k], shape->arrow_dester_x[i][k]);

      /** Increment angle **/

      angle += angle_step;
      if (angle > 360)
        angle -= 360;
    }
  }

  /** For each direction **/

  angle = angle_base;
  for (k=0; k<shape->direction_count; k++)
  {
    /** Set march vector, subtracting offset when appropriate **/

    if (i == 1 || angle == 0 || angle == 90 || angle == 180 ||
			angle == 270 || angle == 360)
      shape_set_single_march (width, angle,
		shape->march_source[k], shape->march_dester[k]);
    else
      shape_set_single_march (width-offset, angle,
		shape->march_source[k], shape->march_dester[k]);

    /** Increment angle **/

    angle += angle_step;
    if (angle > 360)
      angle -= 360;
  }
}



/******************************************************************************
  shape_set_single_arrow (length, aux_length,
		use_split, angle, arrow_source, arrow_dester)

  Set endpoint coordinates for a single direction vector of <length> and
  <angle>, using inset length of <aux_length> if <use_split>.  <arrow_source>
  and <arrow_dester> are the coordinate arrays to be used.
******************************************************************************/
static void
shape_set_single_arrow (int length, int aux_length,
		int use_split, int angle, XPoint arrow_source[], XPoint arrow_dester[])
{
  int x_offset, y_offset,
      x_aux_offset, y_aux_offset;

  double rad_angle;

  /** Handle horizontal and vertical vectors explicitly to ensure that	**/
  /** rounding errors in floating point conversion don't hurt things.	**/

  switch (angle)
  {
    case 0:
      x_offset =		 length;
      y_offset =		 0;
      x_aux_offset =		 aux_length;
      y_aux_offset =		 0;
      break;

    case 90:
      x_offset =		 0;
      y_offset =		-length;
      x_aux_offset =		 0;
      y_aux_offset =		-aux_length;
      break;

    case 180:
      x_offset =		-length;
      y_offset =		 0;
      x_aux_offset =		-aux_length;
      y_aux_offset =		 0;
      break;

    case 270:
      x_offset =		 0;
      y_offset =		 length;
      x_aux_offset =		 0;
      y_aux_offset =		 aux_length;
      break;

    /** Handle generic angle, determining (xoffset, y_offset) endpoints **/
  
    default:
      rad_angle =		 ((double)angle) * CONST_PI/180.0;
      x_offset =		 (int)(cos(rad_angle) * length);
      y_offset =		-(int)(sin(rad_angle) * length);
      x_aux_offset =		 (int)(cos(rad_angle) * aux_length);
      y_aux_offset =		-(int)(sin(rad_angle) * aux_length);
      break;
  }

  /** If <use_split> just set middle vector and inset and exit **/

  if (use_split)
  {
    arrow_source[0].x =		 0;
    arrow_source[0].y =		 0;
    arrow_dester[0].x =		 x_aux_offset;
    arrow_dester[0].y =		 y_aux_offset;

    arrow_source[1].x =		 x_aux_offset;
    arrow_source[1].y =		 y_aux_offset;
    arrow_dester[1].x =		 x_offset;
    arrow_dester[1].y =		 y_offset;

    return;
  }

  /** Set middle vector **/
  
  arrow_source[1].x =		 0;
  arrow_source[1].y =		 0;
  arrow_dester[1].x =		 x_offset;
  arrow_dester[1].y =		 y_offset;

  /** Set the flanking vectors, handling special cases explicitly **/

  /** If vertical vector **/

  if (x_offset == 0)
  {
    arrow_source[0].x =		-1;
    arrow_source[0].y =		 0;
    arrow_dester[0].x =		-1;
    arrow_dester[0].y =		 y_offset;

    arrow_source[2].x =		 1;
    arrow_source[2].y =		 0;
    arrow_dester[2].x =		 1;
    arrow_dester[2].y =		 y_offset;
  }
  else if (y_offset == 0)
  {
    /** Else if horizontal vector **/

    arrow_source[0].x =		  0;
    arrow_source[0].y =		 -1;
    arrow_dester[0].x =		  x_offset;
    arrow_dester[0].y =		 -1;

    arrow_source[2].x =		  0;
    arrow_source[2].y =		  1;
    arrow_dester[2].x =		  x_offset;
    arrow_dester[2].y =		  1;
  }
  else if (angle == 45 || angle == 135 || angle == 225 || angle == 315)
  {
    /** Else if diagonal vector **/

    arrow_source[0].x =		  0;
    arrow_source[0].y =		  (y_offset < 0) ? -1 : 1;
    arrow_dester[0].x =		  x_offset + ((x_offset < 0) ? 1 : -1);
    arrow_dester[0].y =		  y_offset;

    arrow_source[2].x =		  (x_offset < 0) ? -1 : 1;
    arrow_source[2].y =		  0;
    arrow_dester[2].x =		  x_offset;
    arrow_dester[2].y =		  y_offset + ((y_offset < 0) ? 1 : -1);
  }
  else
  {
    /** Else generic vector **/

    arrow_source[0].x =		  (x_offset < 0) ? 1 : -1;
    arrow_source[0].y =		  0;
    arrow_dester[0].x =		  x_offset + ((x_offset < 0) ? 1 : -1);
    arrow_dester[0].y =		  y_offset;

    arrow_source[2].x =		  0;
    arrow_source[2].y =		  (y_offset < 0) ? 1 : -1;
    arrow_dester[2].x =		  x_offset;
    arrow_dester[2].y =		  y_offset + ((y_offset < 0) ? 1 : -1);
  }
}



/******************************************************************************
  shape_set_single_march (length, angle, march_source, march_dester)

  Set endpoint coordinates for a single march vector of <length> and <angle>,
  using <march_source> and <march_dester> as coordinate arrays.
******************************************************************************/
static void
shape_set_single_march (int length, int angle, XPoint march_source[], XPoint march_dester[])
{
  int x_offset, y_offset,
      x_aux_offset, y_aux_offset,
      multer;

  double rad_angle=0.0,
         rad_aux_angle;

  /** Handle horizontal and vertical vectors explicitly to ensure that	**/
  /** rounding errors in floating point conversion don't hurt things.	**/

  switch (angle)
  {
    case 0:
      x_offset =		 length;
      y_offset =		 0;
      break;

    case 90:
      x_offset =		 0;
      y_offset =		-length;
      break;

    case 180:
      x_offset =		-length;
      y_offset =		 0;
      break;

    case 270:
      x_offset =		 0;
      y_offset =		 length;
      break;

    /** Handle generic angle, determining (xoffset, y_offset) endpoints **/
  
    default:
      if (angle%45 != 0)
        length -= 1;
      rad_angle = ((double)angle) * CONST_PI/180.0;
      x_offset =		 (int)(cos(rad_angle) * length);
      y_offset =		-(int)(sin(rad_angle) * length);
      break;
  }

  /** If vertical vector **/

  if (x_offset == 0)
  {
    march_source[0].x =		-2;
    march_source[0].y =		 0;
    march_dester[0].x =		-2;
    march_dester[0].y =		 y_offset;

    march_source[1].x =		-3;
    march_source[1].y =		 0;
    march_dester[1].x =		-3;
    march_dester[1].y =		 y_offset;

    march_source[2].x =		 2;
    march_source[2].y =		 0;
    march_dester[2].x =		 2;
    march_dester[2].y =		 y_offset;

    march_source[3].x =		 3;
    march_source[3].y =		 0;
    march_dester[3].x =		 3;
    march_dester[3].y =		 y_offset;
  }
  else if (y_offset == 0)
  {
    /** Else if horizontal vector **/

    march_source[0].x =		  0;
    march_source[0].y =		 -2;
    march_dester[0].x =		  x_offset;
    march_dester[0].y =		 -2;

    march_source[1].x =		  0;
    march_source[1].y =		 -3;
    march_dester[1].x =		  x_offset;
    march_dester[1].y =		 -3;

    march_source[2].x =		  0;
    march_source[2].y =		  2;
    march_dester[2].x =		  x_offset;
    march_dester[2].y =		  2;

    march_source[3].x =		  0;
    march_source[3].y =		  3;
    march_dester[3].x =		  x_offset;
    march_dester[3].y =		  3;
  }
  else if (angle == 45 || angle == 135 || angle == 225 || angle == 315)
  {
    /** Else if diagonal vector **/

    if ((x_offset < 0 && y_offset < 0) || (x_offset > 0 && y_offset > 0))
      multer = -1;
    else
      multer = 1;

    march_source[0].x =		  2;
    march_source[0].y =		  2*multer;
    march_dester[0].x =		  2 + x_offset;
    march_dester[0].y =		  2*multer + y_offset;

    march_source[1].x =		  3;
    march_source[1].y =		  3*multer;
    march_dester[1].x =		  3 + x_offset;
    march_dester[1].y =		  3*multer + y_offset;

    march_source[2].x =		 -2;
    march_source[2].y =		 -2*multer;
    march_dester[2].x =		 -2 + x_offset;
    march_dester[2].y =		 -2*multer + y_offset;

    march_source[3].x =		 -3;
    march_source[3].y =		 -3*multer;
    march_dester[3].x =		 -3 + x_offset;
    march_dester[3].y =		 -3*multer + y_offset;
  }
  else
  {
    /** Else generic vector, define orthogonal points **/

    rad_aux_angle =		 rad_angle + CONST_PI/2.0;
    x_aux_offset =		 (int)(cos(rad_aux_angle) * 3);
    y_aux_offset =		-(int)(sin(rad_aux_angle) * 3);

    march_source[0].x =		  x_aux_offset;
    march_source[0].y =		  y_aux_offset;
    march_dester[0].x =		  x_aux_offset + x_offset;
    march_dester[0].y =		  y_aux_offset + y_offset;

    march_source[2].x =		 -x_aux_offset;
    march_source[2].y =		 -y_aux_offset;
    march_dester[2].x =		 -x_aux_offset + x_offset;
    march_dester[2].y =		 -y_aux_offset + y_offset;

    march_source[1].x =		  march_source[0].x +
					((x_offset < 0) ? 1 : -1);
    march_source[1].y =		  march_source[0].y;
    march_dester[1].x =		  march_dester[0].x +
					((x_offset < 0) ? 1 : -1);
    march_dester[1].y =		  march_dester[0].y;

    march_source[3].x =		  march_source[2].x -
					((x_offset < 0) ? 1 : -1);
    march_source[3].y =		  march_source[2].y;
    march_dester[3].x =		  march_dester[2].x -
					((x_offset < 0) ? 1 : -1);
    march_dester[3].y =		  march_dester[2].y;
  }
}
