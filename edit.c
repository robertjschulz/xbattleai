#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#ifdef HAVE_STDDEF_H
# include <stddef.h>
#else
# ifndef NULL
#  define NULL ((void *)0)
# endif
#endif
#ifdef STDC_HEADERS
# include <stdlib.h>
#endif
 
/**** x include files ****/
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>

#include "constant.h"
#include "extern.h"

#include "edit.h"


/******************************************************************************
  edit_board ()

  Allows the user to interactively edit the board, placing troops, terrain,
  bases, and towns as desired.
******************************************************************************/
void
edit_board (void)
{
  XEvent event;

  int player,
      current_side,
      base_side,
      value,
      textcount,
      tdir[MAX_DIRECTIONS];

  short max_value;

  char text[20];

  KeySym key;

  cell_type *cell;

  player = 0;
  current_side = Config->player_to_side[player];
  base_side = current_side;

  /** Draw the baseline board **/

  draw_board (player, TRUE);

  /** Loop forever **/

  while (TRUE)
  {
    XNextEvent (XWindow[player]->display, &event);

    /** Find out what type of action to take **/

    switch (event.type)
    {
      /** Redraw whole board **/

      case Expose:
        {
          int old_xmin,old_xmax,old_ymin,old_ymax,old_size;

          int tmp_xmin,tmp_xmax,tmp_ymin,tmp_ymax,tmp_size;

          int new_xmin,new_xmax,new_ymin,new_ymax,new_size;

          XEvent exp_event;

          /** Find the bounding box of all expose events in the queue **/

          old_xmin = event.xexpose.x;
          old_xmax = event.xexpose.x+event.xexpose.width;
          old_ymin = event.xexpose.y;
          old_ymax = event.xexpose.y+event.xexpose.height;
          old_size = (old_xmax-old_xmin)*(old_ymax-old_ymin);

          while (XCheckMaskEvent(event.xexpose.display, ExposureMask, &exp_event))
          {
            tmp_xmin = exp_event.xexpose.x;
            tmp_xmax = exp_event.xexpose.x+exp_event.xexpose.width;
            tmp_ymin = exp_event.xexpose.y;
            tmp_ymax = exp_event.xexpose.y+exp_event.xexpose.height;
            tmp_size = exp_event.xexpose.width*exp_event.xexpose.height;

            if (tmp_xmin<old_xmin)
              new_xmin = tmp_xmin;
            else
              new_xmin = old_xmin;
            if (tmp_xmax>old_xmax)
              new_xmax = tmp_xmax;
            else
              new_xmax = old_xmax;
            if (tmp_ymin<old_ymin)
              new_ymin = tmp_ymin;
            else
              new_ymin = old_ymin;
            if (tmp_ymax>old_ymax)
              new_ymax = tmp_ymax;
            else
              new_ymax = old_ymax;
            new_size = (new_xmax-new_xmin)*(new_ymax-new_ymin);

            /** Don't merge if new area is a lot more than the sum of **/
            /** the separate areas.  This is to prevent massive overdraw **/
            /** at the expense of extra calls to draw_partial_boad(). **/
            if (new_size>256+(old_size+tmp_size)*2)
              draw_partial_board(player, tmp_xmin, tmp_ymin, tmp_xmax, tmp_ymax, FALSE);
            else
            {
              old_xmin = new_xmin;
              old_xmax = new_xmax;
              old_ymin = new_ymin;
              old_ymax = new_ymax;
              old_size = new_size;
            }
          }

          draw_partial_board(player, old_xmin, old_ymin, old_xmax, old_ymax, FALSE);
        }
        break;

      /** Add terrain **/

      case ButtonPress:
        cell = get_cell (event.xbutton.x, event.xbutton.y, tdir, base_side, FALSE);

        /** Event outside of board -- ignore it **/

        if (!cell)
          continue;
    
        /** Decrement/increment terrain with left and middle buttons **/

        if (event.xbutton.button == Button1)
        {
#ifdef WITH_HILLS_AND_FOREST
          if( cell->level == 0 && cell->forest_level > 0)
            cell->forest_level -= 1;
          else
#endif
            cell->level -= 1;
        }
        else if (event.xbutton.button == Button2)
        {
#ifdef WITH_HILLS_AND_FOREST
          if( cell->level == 0 && cell->forest_level > 0)
            cell->forest_level += 1;
          else
#endif
            cell->level += 1;
        }
        else
        {
          /** Else clear to default terrain with right button **/

          cell->level = 0;
#ifdef WITH_HILLS_AND_FOREST
          cell->forest_level = 0;
#endif
        }

        /** Make sure terrain level within proper bounds **/

        if (cell->level < Config->level_min)
          cell->level = Config->level_min;
        else if (cell->level > Config->level_max)
          cell->level = Config->level_max;
#ifdef WITH_HILLS_AND_FOREST
        if (cell->forest_level < 0)
          cell->forest_level = 0;
        else if (cell->forest_level > Config->forest_level_max)
            cell->forest_level = Config->forest_level_max;

        fprintf(stderr,"(%d,%d)-hill:%d, forest:%d\n",
                cell->x,cell->y,cell->level,cell->forest_level);
#endif

        if (cell->level < 0)
        {
          if (cell->side != SIDE_NONE)
          {
            cell->value[cell->side] = 0;
            cell->side = SIDE_NONE;
          }

          cell->growth = 0;
          cell->old_growth = 0;
          cell->angle = 0;
        }

        /** Need to redraw entire polygon, not just erase contents **/

        cell->redraw_status = REDRAW_FULL;

        /** Draw the newly altered cell **/

        draw_cell (cell, player, TRUE);

        cell->redraw_status = REDRAW_NORMAL;
        break;

      case KeyPress:
        
        cell = get_cell (event.xkey.x, event.xkey.y, tdir, base_side, FALSE);

        /** Event outside of board -- ignore it **/

        if (!cell)
            continue;
    
        textcount = XLookupString(&event.xkey, text, 10, &key, NULL);
        if (textcount == 0)
          break;

        switch (text[0])
        {
          /** Create city, town, or village **/

          case EDIT_CREATE_CITY:
          case EDIT_CREATE_TOWN:
          case EDIT_CREATE_VILLAGE:

            if (cell->level < 0)
              break;
          
            if (text[0] == EDIT_CREATE_CITY)
              cell->growth = TOWN_MAX;
            else if (text[0] == EDIT_CREATE_TOWN)
              cell->growth = (TOWN_MAX-TOWN_MIN)/2 + TOWN_MIN;
            else
              cell->growth = TOWN_MIN;

            cell->old_growth = cell->growth;
            cell->angle = ANGLE_FULL;
            break;

          /** Partially scuttle a town **/

          case EDIT_SCUTTLE_TOWN:

            if (cell->old_growth > TOWN_MIN)
            {
              cell->growth = 0;
              cell->angle -= EDIT_SCUTTLE_STEP;

              if (cell->angle < ANGLE_ROUND_DOWN)
              {
                cell->growth = 0;
                cell->old_growth = 0;
                cell->angle = 0;
              }
            }
            break;

          /** Partially build a town **/

          case EDIT_BUILD_TOWN:

            if (cell->old_growth > TOWN_MIN)
            {
              cell->angle += EDIT_BUILD_STEP;

              if (cell->angle > ANGLE_ROUND_UP)
              {
                cell->growth = cell->old_growth;
                cell->angle = ANGLE_FULL;
              }
            }
            break;

          /** Shrink a town **/

          case EDIT_SHRINK_TOWN:

            if (cell->growth > TOWN_MIN)
            {
              cell->growth *= EDIT_SHRINK_FACTOR;
              cell->old_growth = cell->growth;
            }

            if (cell->growth < TOWN_MIN)
            {
              cell->growth = 0;
              cell->old_growth = 0;
              cell->angle = 0;
            }
            break;

          /** Enlarge a town **/

          case EDIT_ENLARGE_TOWN:

            if (cell->growth > TOWN_MIN)
            {
              cell->growth *= EDIT_ENLARGE_FACTOR;
              cell->old_growth = cell->growth;
            }

            if (cell->growth > TOWN_MAX)
            {
              cell->growth = TOWN_MAX;
              cell->old_growth = TOWN_MAX;
            }
            break;

          /** Rotate active side to next side **/

          case EDIT_ROTATE_SIDE:

            current_side += 1;
            if (current_side >= Config->side_count)
              current_side = 0;
            break;

          /** Rotate troop in cell to next side **/

          case EDIT_ROTATE_TROOPS:

            if (cell->side != SIDE_NONE)
            {
              value = cell->value[cell->side];
              cell->value[cell->side] = 0;
              cell->side += 1;
              if (cell->side >= Config->side_count)
                cell->side = 0;
              cell->value[cell->side] = value;
            }
            break;

          /** Add (X-'0') troops of current_side to cell **/

          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
          case '0':
          case EDIT_INCREMENT_TROOPS:
          case EDIT_DECREMENT_TROOPS:

            if (cell->side == SIDE_NONE)
              cell->side = current_side;

            max_value =
		Board->shapes[cell->side][cell->shape_index]->max_value;

            if (text[0] == EDIT_INCREMENT_TROOPS)
              cell->value[cell->side] += 1;
            else if (text[0] == EDIT_DECREMENT_TROOPS)
            {
              if (cell->value[cell->side] > 0)
                cell->value[cell->side] -= 1;
            }
            else
              cell->value[cell->side] =
		(text[0] - '0')*max_value/9;


            if (cell->value[cell->side] == 0)
              cell->side = SIDE_NONE;
            else if (cell->value[cell->side] > max_value)
                cell->value[cell->side] = max_value;
            break;

#ifdef WITH_HILLS_AND_FOREST
            /** Create a level 1 forest cell **/
            
          case EDIT_CREATE_FOREST:
            cell->level = 0;
            cell->forest_level = 1;
            break;
#endif
            
#ifdef WITH_BASE_SIDE
        case EDIT_ROTATE_BASE_SIDE:
            /** Rotate the side owning target base **/
            if (cell->growth > TOWN_MIN)
                cell->base_side += 1;
            if (cell->base_side >= Config->side_count)
                cell->base_side = -1;
            break;
   
#endif
          /** Dump the board to a file **/

          case EDIT_DUMP_BOARD:

            dump_board (Config->file_store_map, Config->use_brief_load);
            draw_board (player, TRUE);
            break;

          /** Exit **/

          case EDIT_EXIT:

            exit (0);
            break;
        }

        /** Draw the newly altered cell **/

        if (cell != NULL)
          draw_cell (cell, player, TRUE);

        break;
    }
  }
}
