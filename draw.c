#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
#endif
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


/** File local prototypes **/
static void draw_blank_cell (cell_type *cell, int player, int use_terrain);
static void draw_arrows (cell_type *cell, xwindow_type *xwindow, GC hue_on, GC hue_off,
	       int x_center, int y_center, int half_troop_size,
	       shape_type *shape, int is_empty);
static void draw_march_arrows (cell_type *cell, int march_side, xwindow_type *xwindow,
	       	int x_center, int y_center, shape_type *shape, int march_type);
static void draw_manager (cell_type *cell, xwindow_type *xwindow, GC hue_off, int x_center, int y_center);


/** File local variables **/
static char const Blank[] = "                                                                                                                                                                     ";


/******************************************************************************
  draw_board (current_player, justboard)

  Draw the entire blank board and all message strings for <current_player>.
******************************************************************************/
void
draw_board (int current_player, int justboard)
{
  draw_partial_board(current_player,0,0,10000,10000,justboard);
}


/******************************************************************************
  draw_partial_board (current_player, justboard)

  Draw the entire blank board and all message strings for <current_player>.
******************************************************************************/
void
draw_partial_board (int current_player, int xmin, int ymin, int xmax, int ymax, int justboard)
{
  int i,
      player, side,
      current_side;

  char line[MAX_TEXT];

  cell_type *cell;

  xwindow_type *xwindow;

  shape_type *shape;

  current_side = Config->player_to_side[current_player];
  xwindow = XWindow[current_player];
  shape = Board->shapes[current_side][0];

  /** If the window is open **/

  if (!xwindow->open)
    return;

  /** Redraw playing portion if redraw rect includes it **/

  if (!(xmax<xwindow->offset_play.x ||
        xmin>xwindow->offset_play.x+xwindow->size_play.x ||
        ymax<xwindow->offset_play.y ||
        ymin>xwindow->offset_play.y+xwindow->size_play.y))
  {
    int xpmin,ypmin,xpmax,ypmax;

    /** Limit board redraw to playing area **/

    if (xmin>xwindow->offset_play.x)
      xpmin = xmin;
    else
      xpmin = xwindow->offset_play.x;
    if (xmax<xwindow->offset_play.x+xwindow->size_play.x)
      xpmax = xmax;
    else
      xpmax = xwindow->offset_play.x+xwindow->size_play.x;
    if (ymin>xwindow->offset_play.y)
      ypmin = ymin;
    else
      ypmin = xwindow->offset_play.y;
    if (ymax<xwindow->offset_play.y+xwindow->size_play.y)
      ypmax = ymax;
    else
      ypmax = xwindow->offset_play.y+xwindow->size_play.y;

    /** Fill redraw area with hue_mark[1] **/

    XFillRectangle (xwindow->display, xwindow->window, xwindow->hue_mark[1],
                xpmin, ypmin, xpmax-xpmin, ypmax-ypmin);

    /** If using backing store, must draw blank cells then copy them    **/
    /** to the backing store, else we just set redraw_status to use     **/
    /** a REDRAW_FULL, which ensures that the whole cell (and grid)     **/
    /** will be rendered appropriately.                                 **/

    if (shape->copy_method == COPY_BACK ||
                shape->copy_method == COPY_WINDOW)
    {
      for (i=0; i<Board->cell_count; i++)
      {
        cell = CELL(i);
        shape = Board->shapes[current_side][cell->shape_index];

        /** Skip redraw if cell is completely out of redraw area **/

        if (xpmax<cell->x_center[current_side]-shape->center_bound.x ||
            xpmin>cell->x_center[current_side]+shape->center_bound.x ||
            ypmax<cell->y_center[current_side]-shape->center_bound.y ||
            ypmin>cell->y_center[current_side]+shape->center_bound.y)
          continue;

        cell->redraw_status = REDRAW_BLANK;
        draw_cell (cell, current_player, TRUE);

        if (Config->enable_all[OPTION_STORE] && current_player == 0)
          store_draw_cell (cell, Config->fp);

        if (shape->copy_method == COPY_BACK)
        {
          /** Copy the blank cell to the backing store **/

          XCopyArea (xwindow->display, xwindow->window,
                xwindow->backing_space, xwindow->hue_mark[0],
                cell->x_center[current_side] - shape->center_bound.x,
                cell->y_center[current_side] - shape->center_bound.y,
                shape->size_bound.x, shape->size_bound.y,
                cell->x_center[current_side] - shape->center_bound.x,
                cell->y_center[current_side] - shape->center_bound.y);
        }

        cell->redraw_status = REDRAW_NORMAL;
      }
    }

    /** Paint the individual cells (and troops) **/

    for (i=0; i<Board->cell_count; i++)
    {
      cell = CELL(i);
      shape = Board->shapes[current_side][cell->shape_index];

      /** Skip redraw if cell is completely out of redraw area **/

      if (xpmax<cell->x_center[current_side]-shape->center_bound.x ||
          xpmin>cell->x_center[current_side]+shape->center_bound.x ||
          ypmax<cell->y_center[current_side]-shape->center_bound.y ||
          ypmin>cell->y_center[current_side]+shape->center_bound.y)
        continue;

      /** Draw whole cell, unless we just did that above **/

      if (shape->copy_method != COPY_BACK &&
                shape->copy_method != COPY_WINDOW)
        cell->redraw_status = REDRAW_FULL;

      /* FIXME: is it safe to skip this and mark the redraw status, and if so, what should it be set to? */
      if (is_visible (cell, current_side))
        draw_cell (cell, current_player, TRUE);
      else
        draw_cell (cell, current_player, FALSE);

      if (Config->enable_all[OPTION_STORE] && current_player == 0)
        store_draw_cell (cell, Config->fp);

      cell->redraw_status = REDRAW_NORMAL;
    }
  }

  /** Display the messages **/

  if (!justboard &&
      !(xmax<xwindow->offset_text.x ||
        xmin>xwindow->offset_text.x+xwindow->size_text.x ||
        ymax<xwindow->offset_text.y ||
        ymin>xwindow->offset_text.y+xwindow->size_text.y))
  {
    XFillRectangle (xwindow->display, xwindow->window, xwindow->hue_terrain[0],
                xwindow->offset_text.x, xwindow->offset_text.y,
                xwindow->size_text.x, xwindow->size_text.y);

    for (player=0; player<Config->player_count; player++)
    {
      if (XWindow[player]->open)
      {
        side = Config->player_to_side[player];

        XDrawImageString (xwindow->display,
                xwindow->window,
                xwindow->hue[side],
                Config->text_offset,
                xwindow->text_y_pos[player],
                Blank, strlen(Blank));
        sprintf (line, "%s > %s", XWindow[player]->display_name,
			Config->message_single[player]);
        XDrawImageString (xwindow->display,
                xwindow->window,
                xwindow->hue[side],
                Config->text_offset,
                xwindow->text_y_pos[player],
                line, strlen(line));
      }
    }
  }
}



/******************************************************************************
  draw_multiple_cell (cell)

  Draw <cell> on every display (for which it should be visible).
******************************************************************************/
void
draw_multiple_cell (cell_type *cell)
{
  int player,
      side;

  /** For each player **/

  for (player=0; player<Config->player_count; player++)
  {
    if (!XWindow[player]->open)
      continue;

    /** Draw <cell> if visible to <player> **/

    side = Config->player_to_side[player];

    if (is_visible (cell, side))
    {
      if (!Config->enable[OPTION_HIDDEN][side] || cell->side == side)
        draw_cell (cell, player, TRUE);
    }
  }

  /** Store-draw if store enabled **/

  if (Config->enable_all[OPTION_STORE])
    store_draw_cell (cell, Config->fp);
}



/******************************************************************************
  draw_cell (cell, player, use_full)

  Draw individual <cell> on window of <player>.  If <use_full> then cell
  terrain is visible.
******************************************************************************/
void
draw_cell (cell_type *cell, int player, int use_full)
{
  int k,
      x_center, y_center,
      x_corner, y_corner,
      x_draw_center, y_draw_center,
      x_draw_corner, y_draw_corner,
      x_size, y_size,
      token_size, half_token_size,
      token2_size, half_token2_size,
      value,
      level,
      shape_index,
      erase_shape,
      copy_method,
      erase_method,
      side,
      side_first, side_second,
      value_first, value_second;

  Window drawable;

  static GC hue_on, hue_off;
  shape_type *shape;
  xwindow_type *xwindow;

  side = Config->player_to_side[player];

  /** If status is REDRAW_BLANK, just draw a blank cell and get out **/

  if (cell->redraw_status == REDRAW_BLANK)
  {
    if (Config->enable[OPTION_MAP][side] && !is_visible (cell, side))
      draw_blank_cell (cell, player, FALSE);
    else
      draw_blank_cell (cell, player, TRUE);
    return;
  }

  /** Set xwindow **/

  xwindow = XWindow[player];

  /** Set shape of cell **/

  shape_index = cell->shape_index;
  shape = Board->shapes[side][shape_index];

  /** Set temporary drawing and erasing methods **/

  if (cell->redraw_status == REDRAW_FULL)
  {
    /** Need to redraw whole polygon, not just erase center **/

    if (Config->enable[OPTION_SEA_BLOCK][side]
		&& cell->level < 0 && cell->level > Config->level_min)
    {
      /** Need to copy over masked pixmap to handle block seas **/

      erase_shape = SHAPE_POLYGON;
      copy_method = COPY_WINDOW;
      erase_method = ERASE_MASK;
    }
    else
    {
      erase_shape = SHAPE_POLYGON;
      copy_method = shape->copy_method;
      erase_method = shape->erase_method;
    }
  }
  else
  {
    erase_shape = shape->erase_shape;
    copy_method = shape->copy_method;
    erase_method = shape->erase_method;
  }

  /** Set temporary level **/

#ifdef WITH_HILLS_AND_FOREST
  if (use_full)
  {
      if (cell->level == 0 && cell->forest_level > 0)
          level = cell->forest_level;
      else
          level = cell->level;
  }
  else if (cell->seen[side] && !Config->enable[OPTION_LOCALMAP][side])
  {
      if (cell->level == 0 && cell->forest_level > 0)
          level = cell->forest_level;
      else
          level = cell->level;
  }
  else
  {
    level = Config->level_max+1;
    erase_shape = SHAPE_POLYGON;
  }
#else
  if (use_full)
    level = cell->level;
  else if (cell->seen[side] && !Config->enable[OPTION_LOCALMAP][side])
    level = cell->level;
  else
  {
    level = Config->level_max+1;
    erase_shape = SHAPE_POLYGON;
  }
#endif

  /** If cell "level" has changed since last draw, must redraw entire	**/
  /** polygon.	This is primarily used for -localmap, when cell flips	**/	
  /** between seen and unseen.						**/

  if (level != cell->draw_level[side])
  {
    erase_shape = SHAPE_POLYGON;
    cell->draw_level[side] = level;
  }

  /** Set cell coordinates (center of cell) **/

  x_center = cell->x_center[side];
  y_center = cell->y_center[side];

  /** Set cell coordinates (left corner of cell) **/

  x_corner = x_center - shape->center_bound.x;
  y_corner = y_center - shape->center_bound.y;

  /** Set cell size **/

  x_size = shape->size_bound.x;
  y_size = shape->size_bound.y;

  /** Determine how to do erase/draw **/

  if (copy_method == COPY_NONE || copy_method == COPY_BACK)
  {
    if (copy_method == COPY_BACK)
      drawable = xwindow->backing_space;
    else
      drawable = xwindow->window;

    x_draw_corner = x_corner;
    y_draw_corner = y_corner;

    x_draw_center = x_center;
    y_draw_center = y_center;
  }
  else
  {
    drawable = xwindow->work_space;

    x_draw_corner = 0;
    y_draw_corner = 0;

    x_draw_center = shape->center_bound.x;
    y_draw_center = shape->center_bound.y;
  }

  xwindow->drawable = drawable;

  /** Either copy cell (and surroundings) to the work space pixmap, or	**/
  /** copy a blank cell from the terrain pixmap to the work space, or	**/
  /** don't do any copies (just do drawing right on the screen or use	**/
  /** the backing store).						**/

  if (copy_method == COPY_WINDOW)
    XCopyArea (xwindow->display, xwindow->window, drawable,
			xwindow->hue_terrain[0],
			x_corner, y_corner, x_size, y_size,
			0, 0);
  else if (copy_method == COPY_PIXMAP)
  {
#ifdef WITH_HILLS_AND_FOREST
      if(cell->level == 0 && cell->forest_level)
          XCopyArea (xwindow->display, xwindow->forest[shape_index][level],
                     drawable, xwindow->hue_terrain[0],
                     0, 0, x_size, y_size,
                     0, 0);
      else
          XCopyArea (xwindow->display, xwindow->terrain[shape_index][level],
                     drawable, xwindow->hue_terrain[0],
                     0, 0, x_size, y_size,
                     0, 0);
#else
  XCopyArea (xwindow->display, xwindow->terrain[shape_index][level],
             drawable, xwindow->hue_terrain[0],
             0, 0, x_size, y_size,
             0, 0);
#endif
  }

  /** Erase old cell body **/

  if (erase_method == ERASE_DRAW)
  {
      GC erase_hue;

#ifdef WITH_HILLS_AND_FOREST
      if (cell->level == 0 && cell->forest_level)
        erase_hue = xwindow->hue_forest[level];
      else
        erase_hue = xwindow->hue_terrain[level];
#else
      erase_hue = xwindow->hue_terrain[level];
#endif

      if (erase_shape == SHAPE_CIRCLE)
      {
        /** Erase center portion **/

        XFillArc (xwindow->display, drawable,
                  erase_hue,
                  x_draw_center - shape->center_erase.x,
                  y_draw_center - shape->center_erase.y,
                  shape->size_erase.x, shape->size_erase.y,
                  0, ANGLE_FULL);

        /** Erase any remnents of the "battle cross" **/

        XDrawLine (xwindow->display, drawable, erase_hue,
                   x_draw_center - shape->center_rectangle.x,
                   y_draw_center - shape->center_rectangle.y,
                   x_draw_center + shape->center_rectangle.x,
                   y_draw_center + shape->center_rectangle.y);

        XDrawLine (xwindow->display, drawable, erase_hue,
                   x_draw_center - shape->center_rectangle.x,
                   y_draw_center + shape->center_rectangle.y,
                   x_draw_center + shape->center_rectangle.x,
                   y_draw_center - shape->center_rectangle.y);
      }
      else if (erase_shape == SHAPE_SQUARE)
      {
          XFillRectangle (xwindow->display, drawable,
                          erase_hue,
                          x_draw_center - shape->center_erase.x,
                          y_draw_center - shape->center_erase.y,
                          shape->size_erase.x, shape->size_erase.y);
      }
      else if (erase_shape == SHAPE_POLYGON)
      {
          shape->points[0].x = x_draw_corner + shape->corner_vertex.x;
          shape->points[0].y = y_draw_corner + shape->corner_vertex.y;

          /** ALTER: possible error here (northrup@onyx.slu.edu) **/
          /** ALTER: level=253 point_count='^G' copy_method=0 **/
          XFillPolygon (xwindow->display, drawable,
                        erase_hue,
                        shape->points, shape->point_count-1,
                        Convex, CoordModePrevious);

          if (Config->enable[OPTION_GRID][side])
              XDrawLines (xwindow->display, drawable,
                          xwindow->hue_mark[0],
                          shape->points, shape->point_count, CoordModePrevious);
          else
              XDrawLines (xwindow->display, drawable,
                          erase_hue,
                          shape->points, shape->point_count, CoordModePrevious);
      }
  }
  else if (erase_method == ERASE_MASK)
  {
    Pixmap erase_pm;

#ifdef WITH_HILLS_AND_FOREST
    if (cell->level == 0 && cell->forest_level)
      erase_pm = xwindow->forest[shape_index][level];
    else
      erase_pm = xwindow->terrain[shape_index][level];
#else
    erase_pm = xwindow->terrain[shape_index][level];
#endif

    /** Clear inside of polygon, keep outside the same **/

    XCopyArea (xwindow->display, erase_pm, drawable, xwindow->gc_clear,
                 0, y_size, x_size, y_size, 0, 0);

    /** OR in new polygon interior **/

    XCopyArea (xwindow->display, erase_pm, drawable, xwindow->gc_or,
                 0, 0, x_size, y_size, 0, 0);
  }

  /** If interior of cell can be seen **/

  if (use_full)
  {
    /** Set drawing side & value **/

    if (cell->side == SIDE_NONE)
    {
      hue_on = xwindow->hue_terrain[0];
      hue_off = xwindow->hue_mark[0];
      value = 0;
    }
    else if (cell->side == SIDE_FIGHT)
      value = 0;
    else
    {
      hue_on =  xwindow->hue[cell->side];
      hue_off = xwindow->hue_inverse[cell->side];
      value =   cell->value[cell->side];
    }
  
    /** compute troop size **/

    token_size = shape->troop_to_size[value];
    half_token_size = token_size/2;

    /** If cell is occupied by a single side **/

    if (cell->side != SIDE_NONE && cell->side != SIDE_FIGHT)
    {
      /** Draw troop **/

      if (shape->troop_shape == SHAPE_CIRCLE)
        XFillArc (xwindow->display, drawable, hue_on,
                x_draw_center - half_token_size, y_draw_center - half_token_size,
                token_size, token_size, 0, ANGLE_FULL);
      else if (shape->troop_shape == SHAPE_SQUARE)
        XFillRectangle (xwindow->display, drawable, hue_on,
                x_draw_center - half_token_size, y_draw_center - half_token_size,
                token_size, token_size);

      /** Show informative letter in center of cell **/
 
      if (xwindow->draw_letter[cell->side])
        XDrawString (xwindow->display, drawable, xwindow->gc_flip,
                x_draw_center - xwindow->char_width/2,
                y_draw_center + xwindow->char_height/2,
                xwindow->letter[cell->side], 1);

      /** Draw direction vectors **/

      if (!Config->enable[OPTION_HIDDEN][side] || cell->side == side)
      {
        if (value == 0)
          draw_arrows (cell, xwindow, hue_on, hue_off,
                x_draw_center, y_draw_center,
                half_token_size, shape, TRUE);
        else
          draw_arrows (cell, xwindow, hue_on, hue_off,
                x_draw_center, y_draw_center,
                half_token_size, shape, FALSE);
      }
    }
    else if (cell->side == SIDE_FIGHT)
    {
      /** Else cell has multiple occupants **/

      /** Find two largest troop components of cell **/

      side_first = 0;
      side_second = 0; 
      value_first = 0;
      value_second = 0;
      for (k=0; k<Config->side_count; k++)
      {
        if (cell->value[k] > value_first)
        {
          value_second = value_first; 
          side_second = side_first;

          value_first = cell->value[k];
          side_first = k;
        }
        else if (cell->value[k] > value_second)
        {
          value_second = cell->value[k];
          side_second = k;
        }
      }

      /** Determine the size and hues of the two troops **/

      hue_on = xwindow->hue[side_first];
      token_size = shape->troop_to_size[value_first];
      half_token_size = token_size/2;

      hue_off = xwindow->hue[side_second];
      token2_size = shape->troop_to_size[value_second];
      half_token2_size = token2_size/2;

      /** Draw the fighting troops **/

      if (shape->troop_shape == SHAPE_CIRCLE)
      {
        XFillArc (xwindow->display,drawable, hue_on,
                x_draw_center - half_token_size,
                y_draw_center - half_token_size,
                token_size, token_size, 0, ANGLE_FULL);
        XFillArc (xwindow->display,drawable, hue_off,
                x_draw_center - half_token2_size,
                y_draw_center - half_token2_size,
                token2_size, token2_size, 0, ANGLE_FULL);
      }
      else if (shape->troop_shape == SHAPE_SQUARE)
      {
        XFillRectangle (xwindow->display, drawable, hue_on,
                x_draw_center - half_token_size,
                y_draw_center - half_token_size,
                token_size, token_size);
        XFillRectangle (xwindow->display, drawable, hue_off,
                x_draw_center - half_token2_size,
                y_draw_center - half_token2_size,
                token2_size, token2_size);
      }

      /** Draw "battle cross" **/

      XDrawLine (xwindow->display, drawable, hue_on,
                x_draw_center - shape->center_rectangle.x,
                y_draw_center - shape->center_rectangle.y,
                x_draw_center + shape->center_rectangle.x,
                y_draw_center + shape->center_rectangle.y);

      XDrawLine (xwindow->display, drawable, hue_off,
                x_draw_center - shape->center_rectangle.x,
                y_draw_center + shape->center_rectangle.y,
                x_draw_center + shape->center_rectangle.x,
                y_draw_center - shape->center_rectangle.y);
    }
  }

  /** If cell is a town **/

  if (cell->angle > 0 && (use_full ||
        (Config->enable[OPTION_BASEMAP][side] && cell->seen[side])))
  {
    /** Set town color **/

#ifdef WITH_BASE_SIDE
      if(cell->base_side != -1)
          hue_off = xwindow->hue_base[cell->base_side];
      else
      {
          if (cell->side < SIDE_VALID_LIMIT)
              hue_off = xwindow->hue_inverse[cell->side];
          else
              hue_off = xwindow->hue_mark[0];
      }
#else
    if (cell->side < SIDE_VALID_LIMIT)
        hue_off = xwindow->hue_inverse[cell->side];
    else
        hue_off = xwindow->hue_mark[0];
#endif

    /** Set town size **/

    if (cell->angle < ANGLE_FULL)
      half_token_size = shape->growth_to_radius[cell->old_growth];
    else
      half_token_size = shape->growth_to_radius[cell->growth];

    token_size = half_token_size*2 + 1;

    XDrawArc (xwindow->display, drawable, hue_off,
                x_draw_center - half_token_size,
                y_draw_center - half_token_size,
                token_size, token_size, 0, cell->angle);

    XDrawArc (xwindow->display, drawable, hue_off,
                x_draw_center - half_token_size + 1,
                y_draw_center - half_token_size + 1,
                token_size - 2, token_size - 2, 0, cell->angle);
  }

  /** If cell is being managed **/

  if (cell->manage_update && use_full)
    draw_manager (cell, xwindow, hue_off, x_center, y_center);

  /** If cell is marching **/

  if (cell->any_march && cell->march_side == side)
    draw_march_arrows (cell, side, xwindow, 
                x_draw_center, y_draw_center, shape,
                MARCH_ACTIVE);
  if (cell->march[side])
    draw_march_arrows (cell, side, xwindow, 
                x_draw_center, y_draw_center, shape,
                cell->march[side]);

  /** Copy cell back to window, if necessary **/

  if (copy_method != COPY_NONE)
    XCopyArea (xwindow->display, drawable, xwindow->window,
                xwindow->hue_terrain[0],
                x_draw_corner, y_draw_corner,
                x_size, y_size, x_corner, y_corner);
}



/******************************************************************************
  draw_blank_cell (cell, player)

  Draw a single blank polygon.
******************************************************************************/
static void
draw_blank_cell (cell_type *cell, int player, int use_terrain)
{
  int side;

  xwindow_type *xwindow;
  shape_type *shape;

  xwindow = XWindow[player];
  side = Config->player_to_side[player];
  shape = Board->shapes[side][cell->shape_index];

  shape->points[0].x = cell->x_center[side] - shape->center_vertex.x;
  shape->points[0].y = cell->y_center[side] - shape->center_vertex.y;

  if (use_terrain)
  {
#ifdef WITH_HILLS_AND_FOREST
      if(cell->level == 0 && cell->forest_level)
          XFillPolygon (xwindow->display, xwindow->window,
                        xwindow->hue_forest[cell->forest_level],
                        shape->points, shape->point_count-1,
                        Convex, CoordModePrevious);
      else
          XFillPolygon (xwindow->display, xwindow->window,
                        xwindow->hue_terrain[cell->level],
                        shape->points, shape->point_count-1,
                        Convex, CoordModePrevious);
#else
          XFillPolygon (xwindow->display, xwindow->window,
                        xwindow->hue_terrain[cell->level],
                        shape->points, shape->point_count-1,
                        Convex, CoordModePrevious);
#endif
  }
  else
      XFillPolygon (xwindow->display, xwindow->window,
                    xwindow->hue_terrain[Config->level_max+1],
                    shape->points, shape->point_count-1,
                    Convex, CoordModePrevious);

  if (Config->enable[OPTION_GRID][side])
    XDrawLines (xwindow->display, xwindow->window,
                xwindow->hue_mark[0],
                shape->points, shape->point_count, CoordModePrevious);
  else
    XDrawLines (xwindow->display, xwindow->window,
                xwindow->hue_terrain[cell->level],
                shape->points, shape->point_count, CoordModePrevious);
}



/******************************************************************************
  draw_arrows (cell, xwindow, hue_on, hue_off, x_center, y_center,
                half_troop_size, shape, is_empty)

  Draw direction vectors for <cell> on <xwindow>, with color <hue_on> and
  inverse color <hue_off>.  The cell center is defined by the coords
  (<x_center>,<y_center>), with <half_troop_size> indicating just that.
  Current cell shape is <shape>.  If <is_empty>, just draw mini-vector.
******************************************************************************/
static void
draw_arrows (cell_type *cell, xwindow_type *xwindow, GC hue_on, GC hue_off,
                int x_center, int y_center, int half_troop_size,
                shape_type *shape, int is_empty)
{
  int dir;

  XPoint *source, *dester;

  /** For each possible direction **/

  for (dir=0; dir<Config->direction_count; dir++)
  {
    /** If direction vector active **/

    if (cell->dir[dir])
    {
      /** Determine which vector array should be used **/

      if (is_empty)
      {
        source = shape->arrow_source[dir][1];
        dester = shape->arrow_dester[dir][1];
      }
      else
      {
        source = shape->arrow_source[dir][0];
        dester = shape->arrow_dester[dir][0];
      }

      /** Draw direction vector (3 lines wide) **/

      /** Top or left edge of arrow **/

      XDrawLine (xwindow->display, xwindow->drawable, hue_on,
                x_center + source[0].x,
                y_center + source[0].y,
                x_center + dester[0].x,
                y_center + dester[0].y);

      /** Bottom or right edge of arrow **/

      XDrawLine (xwindow->display, xwindow->drawable, hue_on,
                x_center + source[2].x,
                y_center + source[2].y,
                x_center + dester[2].x,
                y_center + dester[2].y);

      /** Center of arrow **/

#if USE_STRIPE
      XDrawLine (xwindow->display, xwindow->drawable, hue_off,
                x_center + source[1].x,
                y_center + source[1].y,
                x_center + dester[1].x,
                y_center + dester[1].y);
#else
      XDrawLine (xwindow->display, xwindow->drawable, hue_on,
                x_center + source[1].x,
                y_center + source[1].y,
                x_center + dester[1].x,
                y_center + dester[1].y);
#endif


#if USE_INVERT 
      /** Draw inset vector with inverse hue in troop area **/

      if (!is_empty)
      {
        source = shape->arrow_source_x[half_troop_size][dir];
        dester = shape->arrow_dester_x[half_troop_size][dir];

        XDrawLine (xwindow->display, xwindow->drawable, hue_off,
                x_center + source[0].x,
                y_center + source[0].y,
                x_center + dester[0].x,
                y_center + dester[0].y);
      }
#endif
    }
  }
}


/******************************************************************************
  draw_march_arrows (cell, march_side, xwindow,
                x_center, y_center, shape, march_type)

  Draw march vectors for <cell> on <xwindow>, with color determined by
  <march_side>.  The cell center is defined by the coordinates
  (<x_center>,<y_center>).  Current cell shape is <shape>.  <march_type>
  indicates passive or active marching.
******************************************************************************/
static void
draw_march_arrows (cell_type *cell, int march_side, xwindow_type *xwindow,
                int x_center, int y_center, shape_type *shape, int march_type)
{
  int k;

  XPoint *source, *dester;

  /** If not a HALT command **/

  if (cell->march_dir[march_side] != MARCH_HALT)
  {
    /** Set direction **/

    k = cell->march_dir[march_side];

    source = shape->march_source[k];
    dester = shape->march_dester[k];

    /** Draw march vectors **/

    XDrawLine (xwindow->display, xwindow->drawable,
                xwindow->hue[march_side],
                x_center + source[0].x,
                y_center + source[0].y,
                x_center + dester[0].x,
                y_center + dester[0].y);
    XDrawLine (xwindow->display, xwindow->drawable,
                xwindow->hue[march_side],
                x_center + source[1].x,
                y_center + source[1].y,
                x_center + dester[1].x,
                y_center + dester[1].y);
    XDrawLine (xwindow->display, xwindow->drawable,
                xwindow->hue[march_side],
                x_center + source[2].x,
                y_center + source[2].y,
                x_center + dester[2].x,
                y_center + dester[2].y);
    XDrawLine (xwindow->display, xwindow->drawable,
                xwindow->hue[march_side],
                x_center + source[3].x,
                y_center + source[3].y,
                x_center + dester[3].x,
                y_center + dester[3].y);
  }

  /** Draw passive rectangle if appropriate **/

  if (march_type == MARCH_PASSIVE)
    XDrawRectangle (xwindow->display, xwindow->drawable,
                xwindow->hue[march_side],
                x_center - Config->march_size,
                y_center - Config->march_size,
                2*Config->march_size, 2*Config->march_size);
}



/******************************************************************************
  draw_manager (cell, xwindow, hue_off, x_center, y_center)

  Draw icons which indicate how <cell> is being managed.
******************************************************************************/
static void  
draw_manager (cell_type *cell, xwindow_type *xwindow, GC hue_off, int x_center, int y_center)
{

  switch (cell->manage_update)
  {
    case MANAGE_CONSTRUCTION:

      XDrawString (xwindow->display, xwindow->drawable, hue_off,
                x_center - 3*xwindow->char_width/2 + 1,
                y_center +xwindow->char_height/2, "BLD", 3);
      break;

    case MANAGE_ARTILLERY:

      XDrawString (xwindow->display, xwindow->drawable, hue_off,
                x_center - 3*xwindow->char_width/2 + 1,
                y_center +xwindow->char_height/2, "GUN", 3);
      break;

    case MANAGE_PARATROOP:

      XDrawString (xwindow->display, xwindow->drawable, hue_off, 
                x_center - 3*xwindow->char_width/2 + 1,
                y_center +xwindow->char_height/2, "PAR", 3);
      break;

    case MANAGE_FILL:

      XDrawString (xwindow->display, xwindow->drawable, hue_off, 
                x_center - 3*xwindow->char_width/2 + 1,
                y_center +xwindow->char_height/2, "FIL", 3);
      break;

    case MANAGE_DIG:

      XDrawString (xwindow->display, xwindow->drawable, hue_off, 
                x_center - 3*xwindow->char_width/2 + 1,
                y_center + xwindow->char_height/2, "DIG", 3);
      break;
  }
}



/******************************************************************************
  draw_shell (cell, player, source_side)

  Draw a shell in <cell> of hue given by <source_side> for <player>.
******************************************************************************/
void
draw_shell (cell_type *cell, int player, int source_side)
{
  int side,
      shell_size;
  static long xrand, yrand;
  shape_type *shape;

  side = Config->player_to_side[player];
  shape = Board->shapes[side][cell->shape_index];

  /** Determine shell size and position **/

  shell_size = (int)(shape->size_bound.y * SHELL_FRACTION);

  xrand = (xrand+2938345)%(shape->size_rectangle.x-shell_size);
  yrand = (yrand+2398321)%(shape->size_rectangle.y-shell_size);

  /** Draw shell **/

  XFillArc (XWindow[player]->display, XWindow[player]->window,
                XWindow[player]->hue[source_side],
                cell->x_center[side] + xrand - shape->center_rectangle.x,
                cell->y_center[side] + yrand - shape->center_rectangle.y,
                shell_size, shell_size, 0, ANGLE_FULL);
}



/******************************************************************************
  draw_chute (cell, player, source_side)

  Draw a parachute in <cell> of hue given by <source_side> for <player>.
******************************************************************************/
void
draw_chute (cell_type *cell, int player, int source_side)
{
  int side,
      chute_size, half_chute_size,
      x_center, y_center;
  static long xrand, yrand;
  shape_type *shape;

  side = Config->player_to_side[player];
  shape = Board->shapes[side][cell->shape_index];

  /** Determine chute size and position **/

  chute_size = (int)(shape->size_bound.y * CHUTE_FRACTION);
  half_chute_size = chute_size/2;

  xrand = (xrand+2938345)%(shape->size_rectangle.x - chute_size);
  yrand = (yrand+2398321)%(shape->size_rectangle.y - chute_size -
                half_chute_size);

  x_center = cell->x_center[side] + xrand - shape->center_rectangle.x;
  y_center = cell->y_center[side] + yrand - shape->center_rectangle.y;

  /** Draw semicircle **/

  XFillArc (XWindow[player]->display, XWindow[player]->window,
                XWindow[player]->hue[source_side],
                x_center, y_center, chute_size, chute_size,
                0, ANGLE_HALF);

  /** Draw shrouds **/

  XDrawLine (XWindow[player]->display, XWindow[player]->window,
		XWindow[player]->hue[source_side],
		x_center, y_center + half_chute_size,
		x_center + half_chute_size, y_center + chute_size);

  XDrawLine (XWindow[player]->display, XWindow[player]->window,
		XWindow[player]->hue[source_side],
		x_center + half_chute_size, y_center + half_chute_size,
		x_center + half_chute_size, y_center + chute_size);

  XDrawLine (XWindow[player]->display, XWindow[player]->window,
		XWindow[player]->hue[source_side],
		x_center + chute_size - 1, y_center + half_chute_size,
		x_center + half_chute_size, y_center + chute_size);
}



/******************************************************************************
  draw_message (text, textcount, new_side, current_player)

  Print the latest message to the screen.  Further attention can be called by
  use of CTRL_G to ring the bell.  Returns 1 if program should exit, otherwise 0.
******************************************************************************/
int
draw_message (char text[], int textcount, int new_side, int current_player)
{
  int i,
      player,
      side,
      blank_line,
      old_length,
      scroll, scroll_and_print;

  static int very_first=TRUE,
             first_time[MAX_PLAYERS];

  char line[MAX_LINE];

  /** Initialize message stuff **/

  if (very_first)
  {
    for (i=0; i<MAX_PLAYERS; i++)
      first_time[i] = TRUE;
    very_first = FALSE;
  }

  scroll = FALSE;
  scroll_and_print = FALSE;
  blank_line = FALSE;

  side = Config->player_to_side[current_player];

#if !USE_MULTITEXT
  /**** copy old message ****/
  strcpy (old_message, Config->message_all);
  if (new_side != old_side)
    strcpy (Config->message_all, "");
#else
  old_length = strlen (Config->message_single[current_player]);
#endif

  /** Get out if control, shift, or meta **/

  if (textcount==0)
    return 0;

  text[textcount] = '\0';

  /** If typing something for the first time, remove initial message **/

  if (first_time[current_player])
  {
    strcpy (Config->message_single[current_player], "");
    first_time[current_player] = FALSE;
    blank_line = TRUE;
  }

  /** Find out what player typed **/

  switch (text[0])
  {
    /** Backspace **/

    case BACKSPACE:
    case DELETE:

#if USE_MULTITEXT
      if (strlen (Config->message_single[current_player]) > 0)
        Config->message_single[current_player]
		[strlen (Config->message_single[current_player])-1] = '\0';
/**
 = Config->message_all[(strlen (Config->message_single[current_player]))];
**/
#else
      if (strlen(Config->message_all) > 0)
        Config->message_all[strlen(Config->message_all)-1] =
		Config->message_all[strlen (Config->message_all)];
#endif
      break;

    /** Newline **/

    case RETURN:

#if USE_MULTITEXT
      strcpy (Config->message_single[current_player], "");
      blank_line = TRUE;
#else
      scroll = TRUE;
#endif
      break;

    /** Quit **/

    case CTRL_C:
    case CTRL_Q:

      first_time[current_player] = TRUE;
      if (remove_player (current_player)==1) /* FIXME: return up to main */
        return 1;
      break;

    /** Dump game to file **/

    case CTRL_P:

      game_stats ();
      dump_board (Config->file_store_map, FALSE);
      break;

    /** Switch to next side (cheat) **/

    case CTRL_E:
      break;

    /** Quit game but keep watching **/

    case CTRL_W:

      /** Disable horizon **/

      XWindow[current_player]->watch = TRUE;
      if (Config->enable_all[OPTION_HORIZON])
      {
        Config->enable[OPTION_HORIZON][side] = FALSE;
        draw_board (current_player, TRUE);
      }
      first_time[current_player] = TRUE;
      sprintf (line, "%s has quit the game",
			Config->side_to_hue_name[side]);
      draw_message (line, strlen (line),
			side, current_player);
      for (player=0; player<Config->player_count; player++)
        if (XWindow[player]->open)
          XBell (XWindow[player]->display, BELL_VOLUME);
      break;

    /** Sound bell on all displays (annoying) **/

    case CTRL_G:

      for (player=0; player<Config->player_count; player++)
        if (XWindow[player]->open)
          XBell (XWindow[player]->display, BELL_VOLUME);
      break;

    /** Space **/

    case SPACE:

      /** Determine whether text should scroll **/

#if USE_MULTITEXT
      if ((strlen (Config->message_single[current_player]) + 1) > MAX_TEXT)
        scroll = TRUE;
      else
        strcat (Config->message_single[current_player], " ");
#else
      if ((strlen (Config->message_all) + 1) > MAX_TEXT)
        scroll = TRUE;
      else
        strcat (Config->message_all, " ");
#endif

      break;

    /** Normal character **/

    default:

      /** Determine whether text should scroll **/

#if USE_MULTITEXT
      if ((strlen (Config->message_single[current_player]) +
				strlen(text)) > MAX_TEXT)
        scroll = TRUE;
      else
        strcat (Config->message_single[current_player], text);
#else
      if ((strlen (Config->message_all) + strlen(text)) > 511) /* FIXME: why 511 instead of MAX_TEXT? */
        scroll = TRUE;
      else
        strcat (Config->message_all, text);
#endif

      break;
  }

#if USE_MULTITEXT
  if (old_length > strlen (Config->message_single[current_player])+2)
    blank_line = TRUE;
#else
  if (new_side != old_side)
    scroll_and_print = TRUE;
#endif

  /** Show the message string on all displays **/

  for (player=0; player<Config->player_count; player++)
  {
    if (!XWindow[player]->open)
      continue;
#if !USE_MULTITEXT
    /** Scroll bottom text line up **/

    if (scroll || scroll_and_print)
    {
      XDrawImageString (XWindow[player]->display, XWindow[player]->window,
		XWindow[player]->hue[old_side],
		Config->text_offset, XWindow[player]->text_y_pos[0],
		Blank, strlen(Blank));
      XDrawImageString (XWindow[player]->display, XWindow[player]->window,
		XWindow[player]->hue[old_side],
		Config->text_offset, XWindow[player]->text_y_pos[0],
		old_message, strlen(old_message));
      XDrawImageString (XWindow[player]->display, XWindow[player]->window,
		XWindow[player]->hue[new_side],
		Config->text_offset, XWindow[player]->text_y_pos[1],
		Blank, strlen(Blank));
    }

    /** Show new text line **/

    if (scroll_and_print || !scroll)
    {
      XDrawImageString (XWindow[player]->display, XWindow[player]->window,
		XWindow[player]->hue[new_side],
		Config->text_offset, XWindow[player]->text_y_pos[1],
		Blank, strlen(Blank));
      XDrawImageString (XWindow[player]->display, XWindow[player]->window,
		XWindow[player]->hue[new_side],
		Config->text_offset, XWindow[player]->text_y_pos[1],
		Config->message_all, strlen(Config->message_all));
    }
#else
    /** Show new text line **/

    if (scroll_and_print || !scroll)
    {
      if (blank_line)
        XDrawImageString (XWindow[player]->display, XWindow[player]->window,
		XWindow[player]->hue_inverse[new_side], Config->text_offset,
		XWindow[player]->text_y_pos[current_player],
		Blank, strlen(Blank));

      if (player == current_player)
        sprintf (line, "%s > %s_  ", XWindow[current_player]->display_name,
				Config->message_single[current_player]);
      else
        sprintf (line, "%s > %s   ", XWindow[current_player]->display_name,
				Config->message_single[current_player]);

      XDrawImageString (XWindow[player]->display, XWindow[player]->window,
		XWindow[player]->hue_inverse[new_side], Config->text_offset,
		XWindow[player]->text_y_pos[current_player],
		line, strlen(line));
    }
#endif
  }

#if !USE_MULTITEXT
  if (scroll_and_print || !scroll)
    old_side=new_side;
#endif

  /** Blank out message if scroll occurred **/

  if (scroll)
    strcpy (Config->message_all, "");

  return 0;
}


#if USE_TIMER
/******************************************************************************
  draw_timer (running_time, player)

  Draw short text string which indicates the elapsed game time.
******************************************************************************/
void
draw_timer (unsigned long running_time, int player)
{
  char line[MAX_LINE];

  sprintf (line, "%02lu:%02lu", running_time/60, running_time%60);
  XDrawImageString (XWindow[player]->display, XWindow[player]->window,
		XWindow[player]->hue_mark[0], TIMER_OFFSET,
		XWindow[player]->text_y_pos[0],
		line, strlen(line));

  /** Would be nice, but could slow things down **/
  /** XSync (XWindow[player]->display, 0);      **/
}
#endif
