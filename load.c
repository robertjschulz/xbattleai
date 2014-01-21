#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>

#include "constant.h"
#include "extern.h"

#include "file.h"


/** File local prototypes **/
static void load_cell (cell_type *cell, int load_side_count, int direction_count,
		FILE *fp, int use_brief);
static void dump_header (FILE *fp, int use_brief);
static void dump_cell (cell_type *cell, int side_count, int direction_count,
		FILE *fp, int use_brief);


/******************************************************************************
  dump_board (filename, use_brief)

  Dump all relevant information about the current state of the game to
  the file <filename>.  If <use_brief>, just dump minimal terrain and
  side info (as generated by edit, for example). 
******************************************************************************/
void
dump_board (char const filename[], int use_brief)
{
  int i;

  FILE *fp;

  if ((fp = fopen (filename, "w")) == NULL)
    throw_error ("Unable to open map file: %s", filename);

  dump_header (fp, use_brief);

  /** Save each cell **/

  for (i=0; i<Board->cell_count; i++)
    dump_cell (CELL(i), Config->side_count,
			Config->direction_count, fp, use_brief);

  fclose (fp);
}


/******************************************************************************
  dump_header (fp, use_brief)

  Dump all board and configuration information to the dump file pointed to by
  <fp>.  Also record if brief dump mode is enabled.
******************************************************************************/
static void
dump_header (FILE *fp, int use_brief)
{
  /** Save the crucial parameters which determine compatibility **/

  f_write (&Config->board_x_size,	sizeof(short), 1, fp);
  f_write (&Config->board_y_size,	sizeof(short), 1, fp);
  f_write (&Config->level_min,		sizeof(int), 1, fp);
  f_write (&Config->level_max,		sizeof(int), 1, fp);
#ifdef WITH_HILLS_AND_FOREST
  f_write (&Config->forest_level_max,   sizeof(int), 1, fp);
#endif
  f_write (&Config->tile_type,		sizeof(int), 1, fp);
  f_write (&Config->value_int[OPTION_CELL][0], sizeof(int), 1, fp);
  f_write (&Config->side_count,		sizeof(short), 1, fp);

  if (use_brief)
    fprintf (fp, "B");
  else
    fprintf (fp, "F");
}


/******************************************************************************
  dump_cell (cell, side_count, direction_count, fp, use_brief)

  Dump all relevant information about <cell> to the dump file pointed to by
  <fp>, making sure to save only relevant information.  If <use_brief>,
  just dump essential terrain and side information.
******************************************************************************/
static void
dump_cell (cell_type *cell, int side_count, int direction_count, FILE *fp, int use_brief)
{
  int fixed_size;

  s_char dummy;

  if (use_brief)
  {
    f_write (&cell->level,			sizeof(s_char), 1, fp);
#ifdef WITH_HILLS_AND_FOREST
    f_write (&cell->forest_level,      		sizeof(s_char), 1, fp);
#endif
    if (cell->side == SIDE_NONE && cell->angle == 0)
    {
      dummy = SIDE_VOID;
      f_write (&dummy,				sizeof(s_char), 1, fp);
#ifdef WITH_BASE_SIDE
      f_write(&cell->base_side,                 sizeof(s_char), 1, fp);
#endif
    }
    else
    {
      f_write (&cell->side,			sizeof(s_char), 1, fp);
#ifdef WITH_BASE_SIDE
      f_write(&cell->base_side,                 sizeof(s_char), 1, fp);
#endif
      if (cell->side != SIDE_NONE)
        f_write (&cell->value[cell->side],	sizeof(s_char), 1, fp);

      f_write (&cell->angle,			sizeof(short), 1, fp);
      if (cell->angle == ANGLE_FULL)
        f_write (&cell->growth,			sizeof(s_char), 1, fp);
      else if (cell->angle > 0)
        f_write (&cell->old_growth,		sizeof(s_char), 1, fp);
    }
  }
  else
  {
    /** Save the first part of the cell_type structure, which consists	**/
    /** of statically defined variables.				**/

    /* FIXME: this is ugly, and easy to mess up.  For example the angle
     * element was added which is a short so it won't get byte-swapped.
     * Also, the arguments to f_write were swapped.
     */
    fixed_size = &cell->side - &cell->level;
    f_write (&cell->level, sizeof(s_char), fixed_size, fp);

    fixed_size = &cell->old_value - &cell->side + 1;
    f_write (&cell->side, sizeof(s_char), fixed_size, fp);

    /** Save the second part of the cell_type structure, which contains	**/
    /** dynamically allocated arrays.					**/

    f_write (cell->dir,		sizeof(s_char), direction_count, fp);
    f_write (cell->march,	sizeof(s_char), side_count, fp);
    f_write (cell->march_dir,	sizeof(s_char), side_count, fp);
    f_write (cell->march_type,	sizeof(s_char), side_count, fp);
    f_write (cell->value,	sizeof(s_char), side_count, fp);
    f_write (cell->seen,	sizeof(s_char), side_count, fp);
    f_write (cell->draw_level,	sizeof(s_char), side_count, fp);
  }
}


/******************************************************************************
  load_board (filename, use_brief)

  Load header and each cell of the board from the specified file.  If
  <use_brief>, load a succinct version of the board even if the board
  was not saved in brief mode.
******************************************************************************/
void
load_board (char const filename[], int use_brief)
{
  FILE *fp;

  fp = fopen (filename, "r");
  if (!fp)
    throw_error ("Unable to open %s for reading", filename);

  load_board_header (fp);
  load_board_cells (fp, use_brief);

  fclose (fp);
}


/******************************************************************************
  load_board_header (fp)

  Load all relevant information about the current state of the game from
  the open file fp.
******************************************************************************/
void
load_board_header (FILE * fp)
{
  int side, player,
      cell_size;

  char dummy;

  /** Load the set of header information **/

  f_read (&Config->board_x_size,	sizeof(short), 1, fp);
  f_read (&Config->board_y_size,	sizeof(short), 1, fp);
  f_read (&Config->level_min,	sizeof(int), 1, fp);
  f_read (&Config->level_max,	sizeof(int), 1, fp);
#ifdef WITH_HILLS_AND_FOREST
  f_read (&Config->forest_level_max,    sizeof(int), 1, fp);
#endif
  f_read (&Config->tile_type,	sizeof(int), 1, fp);
  f_read (&cell_size,		sizeof(int), 1, fp);

  f_read (&Config->load_side_count, sizeof(short), 1, fp);

  f_read (&dummy, sizeof(char), 1, fp);
  if (dummy == 'B')
    Config->use_brief_load = TRUE;
  else
    Config->use_brief_load = FALSE;

  /** Correct any conflicting options **/

  if (Config->level_min < 0)
  {
    /** Set sea values if not set in command line **/

    if (!Config->enable_all[OPTION_SEA])
    {
      for (side=0; side<Config->side_count; side++)
        Config->value_double[OPTION_SEA][side] = 5;
    }

    Config->enable_all[OPTION_SEA] = TRUE;
  }

  Config->value_int_all[OPTION_HILL_TONES] = Config->level_max+1;

#ifdef WITH_HILLS_AND_FOREST
  Config->value_int_all[OPTION_FOREST_TONES] = Config->forest_level_max+1;
#else
  Config->value_int_all[OPTION_FOREST_TONES] = Config->level_max+1;
#endif
  
  Config->value_int_all[OPTION_SEA_TONES] = -Config->level_min;

#ifdef WITH_HILLS_AND_FOREST
  if (Config->level_max > 0)
  {
    /** Set default hill values if not set in command line **/

    if (!Config->enable_all[OPTION_HILLS])
    {
      for (side=0; side<Config->side_count; side++)
        Config->value_double[OPTION_HILLS][side] = 5.0*HILL_FACTOR;
    }

    Config->enable_all[OPTION_HILLS] = TRUE;
  }

  if (Config->forest_level_max > 0)
  {
    /** Set default forest values if not set in command line **/

    if (!Config->enable_all[OPTION_FOREST])
    {
      for (side=0; side<Config->side_count; side++)
        Config->value_double[OPTION_FOREST][side] = 5.0*FOREST_FACTOR;
    }

    Config->enable_all[OPTION_FOREST] = TRUE;
  }
#else
  if (Config->level_max > 0)
  {
    /** Set default hill values if not set in command line **/

    if (!Config->enable_all[OPTION_HILLS])
    {
      for (side=0; side<Config->side_count; side++)
        Config->value_double[OPTION_HILLS][side] = 5.0*HILL_FACTOR;
    }

    if (!Config->enable_all[OPTION_HILLS] &&
                        !Config->enable_all[OPTION_FOREST])
      Config->enable_all[OPTION_HILLS] = TRUE;
  }
#endif

  Config->fill_number = -Config->level_min;

  /** Set cell sizes for any side which didn't explicitly set it **/

  for (player=0; player<Config->player_count; player++)
  {
    side = Config->player_to_side[player];
    if (!Config->enable[OPTION_CELL][side])
    {
      Config->value_int[OPTION_CELL][side] = cell_size;
      Config->cell_size[side] = cell_size;
    }
  }  

  /** If side counts in file and game don't match, set OVERWRITE so	**/
  /** that troops are not loaded.					**/

  if (Config->side_count != Config->load_side_count)
    Config->enable_all[OPTION_OVERWRITE] = TRUE;
}



/******************************************************************************
  load_board_cells (fp, use_brief)

  Load each cell of the board from the already open file (from which the
  header has already been loaded).  If <use_brief>, load a succinct version
  of the board.
******************************************************************************/
void
load_board_cells (FILE *fp, int use_brief)
{
  int i;

  cell_type *cell;

  /** Load each cell **/

  for (i=0; i<Board->cell_count; i++)
  {
    cell = CELL(i);
    load_cell (cell, Config->load_side_count,
		Config->direction_count, fp, use_brief);
  }
}



/******************************************************************************
  load_cell (cell, side_count, direction_count, fp, use_brief)

  Load all relevant information about <cell> from the dump file pointed to by
  <fp>, making sure to load only relevant information.  If <use_brief>, load
  a succinct version of the board.
******************************************************************************/
static void
load_cell (cell_type *cell, int load_side_count, int direction_count, FILE *fp, int use_brief)
{
  int fixed_size;

  char ignore[1024];

  if (use_brief)
  {
    f_read (&cell->level,			sizeof(s_char), 1, fp);
#ifdef WITH_HILLS_AND_FOREST
    f_read (&cell->forest_level,       		sizeof(s_char), 1, fp);
#endif
    f_read (&cell->side,	       		sizeof(s_char), 1, fp);
#ifdef WITH_BASE_SIDE
    f_read (&cell->base_side,                   sizeof(s_char), 1, fp);
#endif
    
    if (cell->side == SIDE_VOID)
      cell->side = SIDE_NONE;
    else
    {
      if (cell->side != SIDE_NONE)
        f_read (&cell->value[cell->side],	sizeof(s_char), 1, fp);

      f_read (&cell->angle,			sizeof(short), 1, fp);
      if (cell->angle == ANGLE_FULL)
      {
        f_read (&cell->growth,			sizeof(s_char), 1, fp);
        cell->old_growth = cell->growth;
      }
      else if (cell->angle > 0)
      {
        f_read (&cell->old_growth,		sizeof(s_char), 1, fp);
        cell->growth = 0;
      }
    }
  }
  else
  {
    /** Load the first part of the cell_type structure, which consists	**/
    /** of statically defined variables.				**/

    fixed_size = &cell->side - &cell->level;
    f_read (&cell->level, sizeof(s_char), fixed_size, fp);

    fixed_size = &cell->old_value - &cell->side + 1;
    if (Config->enable_all[OPTION_OVERWRITE])
      f_read (ignore, sizeof(s_char), fixed_size, fp);
    else
      f_read (&cell->side, sizeof(s_char), fixed_size, fp);

    /** Load the second part of the cell_type structure, which contains	**/
    /** dynamically allocated arrays.  Assume arrays already allocated.	**/

    if (Config->enable_all[OPTION_OVERWRITE])
    {
      f_read (ignore, sizeof(s_char), direction_count, fp);
      f_read (ignore, sizeof(s_char), load_side_count, fp);
      f_read (ignore, sizeof(s_char), load_side_count, fp);
      f_read (ignore, sizeof(s_char), load_side_count, fp);
      f_read (ignore, sizeof(s_char), load_side_count, fp);
      f_read (ignore, sizeof(s_char), load_side_count, fp);
      f_read (ignore, sizeof(s_char), load_side_count, fp);
    }
    else
    {
      f_read (cell->dir,	sizeof(s_char), direction_count, fp);
      f_read (cell->march,	sizeof(s_char), load_side_count, fp);
      f_read (cell->march_dir,	sizeof(s_char), load_side_count, fp);
      f_read (cell->march_type,	sizeof(s_char), load_side_count, fp);
      f_read (cell->value,	sizeof(s_char), load_side_count, fp);
      f_read (cell->seen,	sizeof(s_char), load_side_count, fp);
      f_read (cell->draw_level,	sizeof(s_char), load_side_count, fp);
    }
  }
}