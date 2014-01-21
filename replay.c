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

#include "file.h"


/******************************************************************************
  store_draw_cell (cell, fp)

  Draw <cell> to the currently open store file.  Actually, just store enough
  information for replay can show the drawing.
******************************************************************************/
void
store_draw_cell (cell_type *cell, FILE *fp)
{
  int i,
      side; 
  s_char side_first, value_first,
         side_second, value_second;

  /** Store basic cell information **/
               
  f_write (&cell->x,		sizeof(s_char), 1, fp);
  f_write (&cell->y,		sizeof(s_char), 1, fp);
  f_write (&cell->level,	sizeof(s_char), 1, fp);
  f_write (&cell->redraw_status, sizeof(s_char), 1, fp);
  if (cell->redraw_status == REDRAW_BLANK)
    return;

  f_write (&cell->side, sizeof(s_char), 1, fp);

  /** If a fighting cell, have to store extra info **/

  if (cell->side == SIDE_FIGHT)
  {
    /** Find two strongest sides **/

    side_first = -1;
    side_second = -1;
    value_first = 0;
    for (side=0; side<Config->side_count; side++)
    {  
      if (cell->value[side] > value_first)
      {
        value_second = value_first;
        side_second = side_first;

        value_first = cell->value[side];
        side_first = side;
      }
      else if (cell->value[side] > value_second)
      {
        value_second = cell->value[side];
        side_second = side;
      }
    }  

    /** Store info on two strongest sides **/

    f_write (&side_first,	sizeof(s_char), 1, fp);
    f_write (&value_first,	sizeof(s_char), 1, fp);
    f_write (&side_second,	sizeof(s_char), 1, fp);
    f_write (&value_second,	sizeof(s_char), 1, fp);
  }
  else if (cell->side != SIDE_NONE)
    f_write (&cell->value[cell->side], sizeof(s_char), 1, fp);

  /** Store town information **/

  f_write (&cell->angle, sizeof(short), 1, fp);
  if (cell->angle > 0)
  {
    if (cell->angle < ANGLE_FULL)
      f_write (&cell->old_growth, sizeof(s_char), 1, fp);
    else
      f_write (&cell->growth, sizeof(s_char), 1, fp);
  }

  /** Store direction vector information **/

  for (i=0; i<Config->direction_count; i++)
    f_write (&cell->dir[i], sizeof(s_char), 1, fp);
}



/******************************************************************************
  replay_game (fp)

  Load and replay an entire game from the previously opened file fp.
******************************************************************************/
void
replay_game (FILE *fp)
{
  int i,
      side,
      player,
      count,
      done, mini_done,
      textcount;

  char text[10];

  s_char x, y;
  s_char side_first, value_first,
         side_second, value_second;

  KeySym key;

  cell_type *cell;

  XEvent event;

  done = FALSE;

  /** While there are still load-draws left to process **/

  for (count=0; !done; count++)
  {
    /** This is the hack to get REPLAY message to work.  Note that the	**/
    /** number of loops to drawtime (500 below) appears to have an	**/
    /** allowable minimum of 2 or 3 hundred.				**/

    /** Draw the REPLAY message at the bottom of the screen **/

    if (count == 500)
    {
      for (player=0; player<Config->player_count; player++)
      {
#if USE_MULTITEXT
        for (side=0; side<Config->side_count; side++)
          XDrawImageString(XWindow[player]->display,XWindow[player]->window,
		XWindow[player]->hue_terrain[0],
		Config->text_offset, XWindow[player]->text_y_pos[side],
		Config->message_all,strlen(Config->message_all));
#else
        XDrawImageString(XWindow[player]->display,XWindow[player]->window,
		XWindow[player]->hue_terrain[0], Config->text_offset,
		XWindow[player]->text_y_pos[0],
		Config->message_all,strlen(Config->message_all));
        XDrawImageString(XWindow[player]->display,XWindow[player]->window,
		XWindow[player]->hue_terrain[0], Config->text_offset,
		XWindow[player]->text_y_pos[1],
		Config->message_all,strlen(Config->message_all));
#endif
        XSync (XWindow[player]->display, 0);
        XFlush(XWindow[player]->display);
      }
    }

    /** Load the cell information **/

    f_read (&x, sizeof(s_char), 1, fp);

    if (x == REPLAY_TERMINATE)
    {
      for (player=0; player<Config->player_count; player++)
        remove_player (player);

      exit (0);
    }

    f_read (&y, sizeof(s_char), 1, fp);
    cell = CELL2(x,y);

    f_read (&cell->level, sizeof(s_char), 1, fp);
    f_read (&cell->redraw_status, sizeof(s_char), 1, fp);

    if (cell->redraw_status == REDRAW_BLANK)
    {
    }
    else
    {
      f_read (&cell->side, sizeof(s_char), 1, fp);

      /** If fighting cell, load appropriate info **/

      if (cell->side == SIDE_FIGHT)
      {
        f_read (&side_first,	sizeof(s_char), 1, fp);
        f_read (&value_first,	sizeof(s_char), 1, fp);
        f_read (&side_second,	sizeof(s_char), 1, fp);
        f_read (&value_second,	sizeof(s_char), 1, fp);

        for (i=0; i<Config->side_count; i++)
          cell->value[i] = 0;

        cell->value[side_first] = value_first;
        cell->value[side_second] = value_second;
      }  
      else if (cell->side != SIDE_NONE)
        f_read (&cell->value[cell->side], sizeof(s_char), 1, fp);

      f_read (&cell->angle, sizeof(short), 1, fp);

      /** If town, load appropriate info **/

      if (cell->angle > 0)
      {
        if (cell->angle < ANGLE_FULL)
          f_read (&cell->old_growth, sizeof(s_char), 1, fp);
        else
          f_read (&cell->growth, sizeof(s_char), 1, fp);
      }

      for (i=0; i<Config->direction_count; i++)
        f_read (&cell->dir[i], sizeof(s_char), 1, fp);
    }

    /** For each player (window) **/

    for (player=0; player<Config->player_count; player++)
    {
      /** Process any XEvents **/

      while (XEventsQueued(XWindow[player]->display, QueuedAfterReading))
      {
        XNextEvent (XWindow[player]->display, &event);
        switch (event.type)
        {
          /** Process keystroke to see if player wants to quit **/

          case KeyPress:

            mini_done = FALSE;
            while (!mini_done)
            {
              XNextEvent (XWindow[player]->display, &event);
              if (event.type == KeyPress)
              {
                textcount = XLookupString (&event.xkey,
				text, 10, &key, NULL);
                if (textcount != 0 && player == 0)
                {
                  if (text[0] == CTRL_C || text[0] == CTRL_Q)
                  {
                    for (i=0; i<Config->player_count; i++)
                      close_xwindow (XWindow[i]);
                    exit (0);
                  }
                }
                mini_done = TRUE;
              }
            }
            break;
        }
      }

      /** Draw the cell **/

      draw_cell (cell, player, TRUE);

      /** Sync every so often **/

      if ((count % REPLAY_UPDATE) == 0)
        XSync (XWindow[player]->display, 0);
    }
  }

  for (;;);
}


/******************************************************************************
  store_parameters (fp)

  Store relevant game parameters to the previously opened file fp.
******************************************************************************/
void
store_parameters (FILE *fp)
{
  int i,
      side;

  f_write (&Config->board_x_size,	sizeof(short), 1, fp);
  f_write (&Config->board_y_size,	sizeof(short), 1, fp);
  f_write (&Config->level_min,		sizeof(int), 1, fp);
  f_write (&Config->level_max,		sizeof(int), 1, fp);
  f_write (&Config->enable_all[OPTION_HEX], sizeof(n_char), 1, fp);
  f_write (&Config->value_int[OPTION_CELL][0], sizeof(int), 1, fp);
  f_write (&Config->side_count,		sizeof(short), 1, fp);

  /** Store the whole palette **/

  f_write (&Config->hue_count, sizeof(short), 1, fp);
  for (i=0; i<Config->hue_count; i++)
    f_write (Config->palette[i], sizeof(int), 3, fp);

  for (side=0; side<Config->side_count; side++)
  {
    f_write (&Config->side_to_hue[side],		sizeof(short), 1, fp);
    f_write (&Config->hue_to_inverse[side],	sizeof(short), 1, fp);
    f_write (&Config->side_to_letter[side][0],	sizeof(char), 1, fp);
  }
}



/******************************************************************************
  load_parameters (fp)

  Load relevant game parameters from the previously opened file fp.
******************************************************************************/
void
load_parameters (FILE *fp)
{
  int i,
      side,
      player;

  int cell_size;


  f_read (&Config->board_x_size,	sizeof(short), 1, fp);
  f_read (&Config->board_y_size,	sizeof(short), 1, fp);
  f_read (&Config->level_min,		sizeof(int), 1, fp);
  f_read (&Config->level_max,		sizeof(int), 1, fp);
  f_read (&Config->enable_all[OPTION_HEX], sizeof(n_char), 1, fp);
  f_read (&cell_size,			sizeof(int), 1, fp);
  f_read (&Config->side_count,		sizeof(short), 1, fp);

  /** Reconcile options with command line **/

  if (Config->level_min < 0)
    Config->enable_all[OPTION_SEA] = TRUE;

  if (Config->level_max > 0)
  {
    if (!Config->enable_all[OPTION_HILLS] &&
			!Config->enable_all[OPTION_HILLS])
      Config->enable_all[OPTION_HILLS] = TRUE;
  }

  Config->value_int_all[OPTION_HILL_TONES] = Config->level_max+1;
  Config->value_int_all[OPTION_FOREST_TONES] = Config->level_max+1;
  Config->value_int_all[OPTION_SEA_TONES] = -Config->level_min;

  for (player=0; player<Config->player_count; player++)
  {
    side = Config->player_to_side[player]; 
    if (!Config->enable[OPTION_CELL][side])
    {
      Config->value_int[OPTION_CELL][side] = cell_size;
      Config->cell_size[side] = cell_size;
    }
  }

  /** Load the whole palette **/

  f_read (&Config->hue_count, sizeof(short), 1, fp);
  for (i=0; i<Config->hue_count; i++)
    f_read (Config->palette[i], sizeof(int), 3, fp);

  for (side=0; side<Config->side_count; side++)
  {
    f_read (&Config->side_to_hue[side],		sizeof(short), 1, fp);
    f_read (&Config->hue_to_inverse[side],	sizeof(short), 1, fp);
    f_read (&Config->side_to_letter[side][0],	sizeof(char), 1, fp);
  }
}


/******************************************************************************
  game_stats ()

  Generate some elementary statistics on the state of the game and display
  them to stdout.
******************************************************************************/
void
game_stats (void)
{
  int i, j,
      side,
      force[MAX_SIDES],
      coverage[MAX_SIDES];

  for (side=0; side<Config->side_count; side++)
  {
    force[side] = 0;
    coverage[side] = 0;
  }

  /** Compute the number of troops and cells for each side **/

  for (i=0; i<Config->board_x_size; i++)
    for (j=0; j<Config->board_y_size; j++)
    {
      side = CELL2(i,j)->side;
      if (side != SIDE_FIGHT && side != SIDE_NONE)
      {
        force[side] += CELL2(i,j)->value[side];
        coverage[side]++;
      }
    }

  /** Print statistics to stdout **/

  for (side=0; side<Config->side_count; side++)
    printf ("%s:  %3d cells   %5d force\n", Config->side_to_hue_name[side],
			coverage[side], force[side]);
  printf ("\n");
}
