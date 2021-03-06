#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* Note. Changes made 2000-09-08 to handle parsing of AI options
   -- Mathias Broxvall

   For changes: Copyright (c) 2000 - Mathias Broxvall
   other parts of file copyrighted as per the file "COPYRIGHT"  
   */

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
#else
# ifdef HAVE_MALLOC_H
#  include <malloc.h>
# endif
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include <ctype.h>
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include <math.h>

#include "constant.h"
#include "options.h"
#include "extern.h"
#include "ai.h"

#include "parse.h"


/** File local prototypes **/
static void free_command_line(int argc, char * const argv[]);
static void check_options (int argc, char * const argv[], int ignore_colors);
static int load_command_line (int argc, char * const argv[], char * command_line[]);
static void preload_options (int argc, char * const argv[]);
static int parse_options (int argc, char * const argv[]);
static int install_option (int option_index, char * const argv[], int parameter_count,
	       	int side, int is_enabled);
static void clean_config (void);
static void set_move_parameters (void);
static void free_move_parameters(void);
static void set_palette (short palette[][3], short count, short max_count);
static int find_option (char const *option);
static int find_parameter_count (int argc, char * const argv[], int offset);
static int find_load_filename (char const *option, char *filename);
static int find_color_match (char const *option, char *color_name, int use_second_color);
static int load_color (char const *hue_name, int red, int green, int blue);
static void load_color_inverse (char const *hue_name, char const *inverse_hue_name);
static int load_stipple (char const *hue_name, char * const stipples[]);
static void print_usage_message (void);
#ifdef UNUSED
static int copy_first (char *dest, char *src);
static char * strip_first (char *src);
#endif
static void set_double_array (double *array, double value, int index);
static void set_int_array (int *array, int value, int index);
static void set_uchar_array (unsigned char *array, unsigned char value, int index);
static void read_options(FILE *fp, char **commandline, int *pos);


/******************************************************************************
  load_options (argc, argv)

  Initialize all variables, parse the command line arguments, and clean up the
  variables for later use.
******************************************************************************/
int
load_options (int argc, char * const argv[])
{
  char *argv_new[MAX_TEXT];
  int argc_new;

  check_options (argc, argv, TRUE);
 
  argc_new = load_command_line (argc, argv, argv_new);

  preload_options (argc_new, argv_new);

  check_options (argc_new, argv_new, FALSE);

  if (parse_options (argc_new, argv_new)==-1)
  {
    free_command_line(argc_new,argv_new);
    return -1;
  }
  
  free_command_line(argc_new,argv_new);
  clean_config ();

  return 0;
}


/******************************************************************************
  free_options()

  Free resources allocated in load_options.
******************************************************************************/
void
free_options(void)
{
  free_move_parameters();
}



/******************************************************************************
  free_command_line(argc, argv)

  Free parsed command line options.
******************************************************************************/
static void
free_command_line(int argc, char * const argv[])
{
  int i;

  for (i=0; i<argc; i++)
    free(argv[i]);
}



/******************************************************************************
  check_options (argc, argv, ignore_colors)

  Step through each of the command line arguments specified in <argv[]>, 
  checking for compliance with list of options in global <Options[]>.  Flag
  any unknown options or argument count mismatches.  Call find_color_match()
  to check for side declarations (which are never in <Options[]>).  If
  <ignore_colors> then all unknown options are assumed to be colors and are
  treated as such (basically just makes sure parameter counts match).
******************************************************************************/
static void
check_options (int argc, char * const argv[], int ignore_colors)
{
  int i,
      all_valid,
      option_index,
      side_index,
      active_side_index,
      in_bracket,
      is_enabled,
      parameter_count,
      target_parameter_count;
  char color_name[200],
       load_name[200];

  in_bracket = FALSE;
  active_side_index = -1;
  all_valid = TRUE;

  /** Step through each command line argument **/

  for (i=1; i<argc; i++)
  {
    /** Check to see if argument is valid option **/

    option_index = find_option (argv[i]);

    if (option_index >= OPTION_COUNT)
    {
      /** Argument was negated **/
      is_enabled = FALSE;
      option_index -= OPTION_COUNT;
    }
    else
      is_enabled = TRUE;

    /** Check to see if it is a short-hand "-options x.xbo" = "-x.xbo" **/

    if (option_index < 0)
      option_index = find_load_filename (argv[i], load_name);

    /** If within side brackets ("-red { <options> } me") **/

    if (active_side_index >= 0)
    {
      /** Set state of brackethood **/

      if (option_index == 0)
        in_bracket = TRUE;
      else if (option_index == 1)
      {
        in_bracket = FALSE;
        side_index = active_side_index;
        active_side_index = -1;
      }
    }

    /** If not a valid option, and not in brackets, find color **/

    if (option_index < 0 && !in_bracket)
    {
      if (ignore_colors)
        side_index = 0;
      else
        side_index = find_color_match (argv[i], color_name, 0);
    }
    else
      side_index = -1;

    /** Find number of parameters **/

    parameter_count = find_parameter_count (argc, argv, i);

    /** If argument is some type of valid color **/

    if (side_index >= 0)
    {
      /** Make sure that next <argv[]> entry is display name or '{' **/

      if (parameter_count == 0 && (i+1)<argc && argv[i+1][0] == '{')
        active_side_index = side_index;
      else if (parameter_count != 1)
      {
        all_valid = FALSE;
        throw_warning ("Unresolvable color %s", argv[i]);
      }
    }
    else if (option_index < 0)
    {
      /** Else invalid option **/

      all_valid = FALSE;
      throw_warning ("Unresolvable argument %s", argv[i]);
    }
    else
    {
      /** Else valid option **/

      target_parameter_count = Options[option_index].count;
      if (!is_enabled && target_parameter_count == 1)
        target_parameter_count = 99;

      /** If option supports either 0 or 1 parameters **/

      if (target_parameter_count > 98)
      {
        /** If parameter count mismatch **/

        if (parameter_count > 1)
        {
          all_valid = FALSE;
          throw_warning ("Parameter miscount %s", argv[i]);
        }
      }
      else if (parameter_count != target_parameter_count)
      {
        /** Else if parameter count mismatch **/

        all_valid = FALSE;
        throw_warning ("Parameter miscount %s", argv[i]);
      }
    }

    /** Step to next option **/

    i += parameter_count;
  }

  /** If still in bracket, something is wrong with command line **/

  if (in_bracket)
  {
    all_valid = FALSE;
    throw_warning ("Bracket mismatch", NULL);
  }

  /** If not all arguments are valid, exit the program **/

  if (!all_valid)
    throw_error ("Poorly formed command line", NULL);
}



/******************************************************************************
  int
  load_command_line (argc, argv, command_line)

  Copy over all arguments from <argv[]> into <command_line[]>, expanding any
  default and option files inline.  Return the new number of arguments.
******************************************************************************/
static int
load_command_line (int argc, char * const argv[], char *command_line[])
{
  int i, j,
      argc_new,
      option_index,
      parameter_count;

  char filename[200],
       alternative_filename[500];

  FILE *fp;

  /** Copy over invocation argument **/

  argc_new = 0;
  command_line[argc_new++] = strdup (argv[0]);

  /** Open up default file, if it exists **/

  if ((fp = fopen (".xbattle", "r")) == NULL)
  {
#if USE_HOME_ENV
/* FIXME: why not on by default? */
    char line[200],
        *home_dir;

    home_dir = getenv("HOME");
    if (home_dir != NULL)
    {
      sprintf (line, "%s/.xbattle", home_dir);
      /** Not in current directory, so check home directory **/
      fp = fopen (line, "r");
    }
#endif
  }

  /** If there is a default file, copy all options into command line **/

  if (fp)
  {
    read_options(fp,command_line,&argc_new);
    fclose (fp);
  }

  /** Step through all original command line arguments **/

  for (i=1; i<argc; i++)
  {
    option_index = find_option (argv[i]);

    /** Check to see if it is a short-hand "-options x.xbo" = "-x.xbo" **/

    if (option_index < 0)
      option_index = find_load_filename (argv[i], filename);
    else
      filename[0] = '\0';

    /** Find the parameter count **/

    parameter_count = find_parameter_count (argc, argv, i);

    /** If the argument is "-options", load option file **/

    if (option_index == OPTION_OPTIONS)
    {
      /** If no parameter load default, else load specified file **/

      if (parameter_count == 0)
      {
        if (filename[0] == '\0')
          strcpy (filename, "default.xbo");
      }
      else
        strcpy (filename, argv[i+1]);

      /** If cannot open file, try default directory **/

      if ((fp = fopen (filename, "r")) == NULL)
      {
        sprintf (alternative_filename, "%s/%s", DEFAULT_XBO_DIR, filename);
        if ((fp = fopen (alternative_filename, "r")) == NULL)
          throw_error ("Cannot open option file %s", filename);
      }

      /** Copy all options into command line **/ 

      read_options(fp,command_line,&argc_new);
      fclose (fp);
    }
    else
    {
      /** Else not "-options" argument so copy directly to command line **/

      command_line[argc_new++] = strdup (argv[i]);
      for (j=0; j<parameter_count; j++)
        command_line[argc_new++] = strdup (argv[i+j+1]);
    }

    /** Increment <argv[]> list pointer to next option **/

    i += parameter_count;
  }

  /** Return new number of command line arguments **/

  return (argc_new);
}



/******************************************************************************
  preload_options (argc, argv)

  Handle the preloading of certain options which are necessary for the
  proper installation of other options.  For example, custom colors must be
  preloaded so that they are not flagged as illegal options when used.
******************************************************************************/
static void
preload_options (int argc, char * const argv[])
{
  int i, j,
      parameter_count,
      index,
      option_index;

  for (i=1; i<argc; i++)
  {
    /** Find option index and parameter count **/

    option_index = find_option (argv[i]);
    parameter_count = find_parameter_count (argc, argv, i);

    switch (option_index)
    {
      case OPTION_COLOR:

        load_color (argv[i+1], atoi(argv[i+2]),
				atoi(argv[i+3]), atoi(argv[i+4]));
        break;

      case OPTION_COLOR_INVERSE:

        load_color_inverse (argv[i+1], argv[i+2]);
        break;

      case OPTION_STIPPLE:

        load_stipple (argv[i+1], &argv[i+2]);
        break;

      case OPTION_HILL_COLOR:

        index = atoi (argv[i+1]);
        if (index >= MAX_HILL_TONES)
          index = MAX_HILL_TONES-1;
        for (j=0; j<3; j++)
          Config->palette_hills[index][j] = atoi (argv[i+j+2]);
        break;

      case OPTION_FOREST_COLOR:

        index = atoi (argv[i+1]);
        if (index >= MAX_FOREST_TONES)
          index = MAX_FOREST_TONES-1;
        for (j=0; j<3; j++)
          Config->palette_forest[index][j] = atoi (argv[i+j+2]);
        break;

      case OPTION_SEA_COLOR:

        index = atoi (argv[i+1]);
        if (index >= MAX_SEA_TONES)
          index = MAX_SEA_TONES-1;
        for (j=0; j<3; j++)
          Config->palette_sea[index][j] = atoi (argv[i+j+2]);
        break;
    }

    i += parameter_count;
  }
}




/******************************************************************************
  parse_options (argc, argv)

  Given the command line arguments, with default and option files already
  expanded, parse the options and assign player sides.  On success the
  function returns 0, otherwise it returns -1.
******************************************************************************/
static int
parse_options (int argc, char * const argv[])
{
  int i, j,
      parameter_count,
      option_index,
      primary_index,
      secondary_index,
      is_enabled,
      side,
      active_side,
      side_count,
      player_count,
      has_colon;

  char primary_color_name[200],
       secondary_color_name[200],
       display_name[200];

  /** No players, no sides, no active side **/

  active_side = -1;
  player_count = 0;
  side_count = 0;

  /** Set all sides names to nothingness **/ 

  for (side=0; side<MAX_SIDES; side++)
  {
    strcpy (Config->side_to_hue_name[side], "");
    strcpy (Config->side_to_bw_name[side], "");
  }
  Config->message_all[0] = '\0';

  /** Step through each argument in <argv[]> **/

  for (i=1; i<argc; i++)
  {
    /** Check to see if argument is option or negated option **/

    option_index = find_option (argv[i]);
    if (option_index >= OPTION_COUNT)
    {
      is_enabled = FALSE;
      option_index -= OPTION_COUNT;
    }
    else
      is_enabled = TRUE;

    /** Find the number of paramters the argument takes **/

    parameter_count = find_parameter_count (argc, argv, i);

    /** If argument is option or negated option, install it **/

    if (option_index >= 0)
    {
      if (install_option (option_index, &argv[i],
		parameter_count, active_side, is_enabled)==-1)
	return -1;
    }
    else if (active_side == -1)
    {
      /** Else if there is no active side yet **/

      /** Set primary and secondary color indices **/

      primary_index = find_color_match (argv[i], primary_color_name, 0);
      if (primary_index >= 0)
        secondary_index = find_color_match (argv[i], secondary_color_name, 1);
      else
        secondary_index = -1;

      /** Set active side, checking to see if side color already observed **/

      active_side = side_count;
      for (side=0; side<side_count; side++)
        if (strcmp (primary_color_name, Config->side_to_hue_name[side]) == 0)
          active_side = side;

      /** If there is a single parameter, we're dealing with a case	**/	
      /** like "-red cnsx45", as opposed to "-red { -guns 5 } cnsx45",	**/
      /** so artificially set the option_index to "}" (1).		**/

      if (parameter_count == 1)
        option_index = 1;
    }

    /** If there is an active side and option is a "}" **/

    if (active_side >= 0 && option_index == 1)
    {
      /** Copy the display name **/

      strcpy (display_name, argv[i+1]);

      /** If the display name isn't the official dummy display "you" **/

      if (strcmp (display_name, "you") != 0 &&
	  strcmp (display_name, "null") != 0)
      {
        /** Initialize the xwindow_type structure for the player **/

        XWindow[player_count] = (xwindow_type *)malloc(sizeof(xwindow_type)); /* FIXME: free this */
 
        if (strcmp (argv[i+1], "me") == 0)
          strcpy (XWindow[player_count]->display_name, "");
        else
        {
          /** If no ":x.y" suffix given in display name add ":0.0" ****/

          has_colon = FALSE;
          for (j=0; display_name[j] != '\0'; j++)
            if (display_name[j] == ':')
              has_colon = TRUE;
          strcpy (display_name, argv[i+1]);
          if (!has_colon)
            strcat (display_name, ":0.0");
 
          strcpy (XWindow[player_count]->display_name, display_name);
        }
      }
      
      /** If player represents a new side **/

      if (active_side == side_count)
      {
        /** Set player_to_side and side_to_hue_name mappings **/ 

        strcpy (Config->side_to_hue_name[side_count], primary_color_name);
        Config->player_to_side[player_count] = side_count;

        /** Assign palette index to side **/

        Config->side_to_hue[side_count] = primary_index;

        /** If no secondary color specified, set side_to_bw mapping **/

        if (secondary_index < 0)
        {
          if (Config->hue_has_bw[primary_index])
          {
            Config->side_to_bw[side_count] = primary_index;
          }
          else
          {
            /** Else primary color was a non b&w custom color **/

            if (Config->palette[primary_index][0] > 128)
              Config->side_to_bw[side_count] = 2;
            else
              Config->side_to_bw[side_count] = 1;
          }

          /** Set side_to_bw_name mapping **/
 
          strcpy (Config->side_to_bw_name[side_count], primary_color_name);
        }
        else
        {
          /** Else a secondary color was specified, set side_to_bw mapping **/

          strcpy (Config->side_to_bw_name[side_count], secondary_color_name);

          if (Config->hue_has_bw[secondary_index])
            Config->side_to_bw[side_count] = secondary_index;
          else
            Config->side_to_bw[side_count] = secondary_index%Config->bw_count;
        }

        /** If not secondary color and non b&w primary color **/

        if (secondary_index < 0 && !Config->hue_has_bw[primary_index])
        {
          /** Set up side_to_letter mappings for letter-in-troop display **/

          Config->side_to_letter[side_count][0] =
			Config->side_to_hue_name[side_count][0];
          Config->side_to_letter[side_count][1] = '\0';
        }
        else
          Config->side_to_letter[side_count][0] = FALSE;

        side_count++;
      }
      else
      {
        /** Player represents old side, so set mapping **/

        Config->player_to_side[player_count] = active_side;
      }

      /** If not a dummy player, increment player count **/

      if (strcmp (display_name, "you") != 0 &&
	  strcmp (display_name, "null") != 0)
        player_count++;

      /** Reset to no active side **/

      active_side = -1;
    }

    /** Step ahead to next option/color in <argv[]> **/

    i += parameter_count;
  }

  /** Set global count values **/

  Config->side_count = side_count;
  Config->player_count = player_count;

  /** If there are no valid sides, show usage message **/

  if (Config->side_count < 1 && !Config->enable_all[OPTION_REPLAY])
  {
    print_usage_message ();
    return -1;
  }

  return 0;
}



/******************************************************************************
  install_option (option_index, argv, parameter_count, side, is_enabled)

  Given the option indexed by <option_index>, with <parameter_count> arguments
  specified in <argv>, install (or disinstall if NOT <is_enabled>) the option
  and related variables.  Option is installed to all sides if <side> is < 0,
  else it is installed to just <side>.  Returns 0 on success, -1 if program
  should exit.
******************************************************************************/
static int
install_option (int option_index, char * const argv[], int parameter_count, int side, int is_enabled)
{
  int value_int, i;
  double value_double;
  char option[MAX_TEXT];
  char *s;

  /** If there are parameters, set int and double values **/

  if (parameter_count)
  {
    value_int = atoi (argv[1]);
    value_double = atof (argv[1]);
  }
  else
  {
    value_int = Options[option_index].value_int;
    value_double = Options[option_index].value_double;
  }

  switch (option_index)
  {
    case OPTION_LEFT_BRACKET:
    case OPTION_RIGHT_BRACKET:

      break;

    case OPTION_COLOR:
    case OPTION_HILL_COLOR:
    case OPTION_FOREST_COLOR:
    case OPTION_SEA_COLOR:

      break;

    case OPTION_STORE:
    case OPTION_REPLAY:
    case OPTION_LOAD:
    case OPTION_DUMP:
    case OPTION_EDIT:

      break;

    /** Only add certain options to player text message **/

    case OPTION_AREA:
    case OPTION_ATTACK:
    case OPTION_ARMIES:
    case OPTION_BASEMAP:
    case OPTION_BASES:
    case OPTION_BOUND:
    case OPTION_BUILD:
    case OPTION_BUILD_COST:
    case OPTION_BUILD_LIMIT:
    case OPTION_DECAY:
    case OPTION_DIG:
    case OPTION_DIG_COST:
    case OPTION_DIGIN:
    case OPTION_DISRUPT:
    case OPTION_ERODE:
    case OPTION_ERODE_THRESH:
    case OPTION_FARMS:
    case OPTION_FIGHT:
    case OPTION_FILL:
    case OPTION_FILL_COST:
    case OPTION_FOREST:
    case OPTION_ARTILLERY:
    case OPTION_ARTILLERY_COST:
    case OPTION_ARTILLERY_DAMAGE:
    case OPTION_HIDDEN:
    case OPTION_HILLS:
    case OPTION_HORIZON:
    case OPTION_LOCALMAP:
    case OPTION_MANAGE:
    case OPTION_MARCH:
    case OPTION_MAXVAL:
    case OPTION_MILITIA:
    case OPTION_MOVE:
    case OPTION_NEW_COLORMAP:
    case OPTION_NOSPIGOT:
    case OPTION_PARATROOPS:
    case OPTION_PARATROOPS_COST:
    case OPTION_PARATROOPS_DAMAGE:
    case OPTION_RBASES:
    case OPTION_RBASE_RANGE:
    case OPTION_REPEAT:
    case OPTION_RESERVE:
    case OPTION_SCUTTLE:
    case OPTION_SCUTTLE_COST:
    case OPTION_SPEED:
    case OPTION_WRAP:

      if (is_enabled)
      {
        if (parameter_count)
          sprintf (option, "%s=%d ",
			&Options[option_index].option[1], value_int);
        else
          sprintf (option, "%s ", &Options[option_index].option[1]);
      }
      else
        sprintf (option, "no_%s ", &Options[option_index].option[1]);
      strcat (Config->message_all, option);

      break;

    default:

      break;
  }

  /** Following if allows something like "-red { -no_guns } me" without	**/
  /** disabling guns for everyone.					**/

  if (is_enabled || side < 0)
    Config->enable_all[option_index] = is_enabled;

  /** Set all and single enables and values **/

  set_uchar_array (Config->enable[option_index], is_enabled, side);
  Config->value_int_all[option_index] = value_int;
  set_int_array (Config->value_int[option_index], value_int, side);
  Config->value_double_all[option_index] = value_double;
  set_double_array (Config->value_double[option_index], value_double, side);

  /** Handle some of the peskier options that require special treatment **/

  switch (option_index)
  {
    /** Need to set x and y board sizes **/

    case OPTION_BOARD:

      Config->value_int_all[OPTION_BOARDX] = value_int;
      Config->value_int_all[OPTION_BOARDY] = value_int;
      break;

    /** Always must be linked to fill and sea tones **/

    case OPTION_DIG:

      if (parameter_count)
      {
        Config->value_int_all[OPTION_FILL] = value_int;
        Config->value_int_all[OPTION_SEA_TONES] = value_int;
      }
      break;

    /** If has parameter, link to sea tones **/

    case OPTION_FILL:

      if (parameter_count)
        Config->value_int_all[OPTION_SEA_TONES] = value_int;
      break;

    /** Just print usage message and quit **/

    case OPTION_HELP:

      if (is_enabled)
      {
        print_usage_message ();
        return -1;
      }
      break;

    /** Handle the four options which take file name parameters **/

    case OPTION_EDIT:

      Config->use_brief_load = FALSE;
      if (parameter_count)
        strcpy (Config->file_store_map, argv[1]);
      else
        strcpy (Config->file_store_map, "xbattle.xbt");
      break;

    case OPTION_LOAD:

      if (parameter_count)
        strcpy (Config->file_map, argv[1]);
      else
        strcpy (Config->file_map, "xbattle.xbt");
      break;

    case OPTION_REPLAY:

      if (parameter_count)
        strcpy (Config->file_replay, argv[1]);
      else
        strcpy (Config->file_replay, "xbattle.xba");
      break;

    case OPTION_STORE:

      if (parameter_count)
        strcpy (Config->file_store, argv[1]);
      else
        strcpy (Config->file_store, "xbattle.xba");
      break;

  case OPTION_VICTORY_ARMY_RATIO:
	s=strdup(argv[2]);
	if(side<0)
	  for (i=0; i<MAX_SIDES; i++)
		Config->victory_string_army[i]=s;
	else
	  Config->victory_string_army[side]=s;
	break;
  case OPTION_VICTORY_LAND_RATIO:
	s=strdup(argv[2]);
	if(side<0)
	  for (i=0; i<MAX_SIDES; i++)
		Config->victory_string_land[i]=s;
	else
	  Config->victory_string_land[side]=s;
	break;
  case OPTION_VICTORY_TIMEOUT:
	s=strdup(argv[2]);
	if(side<0)
	  for (i=0; i<MAX_SIDES; i++)
		Config->victory_string_timeout[i]=s;
	else
	  Config->victory_string_timeout[side]=s;
	break;

    case OPTION_VICTORY_POSITION:
	  s=strdup(argv[3]);

      {
        short x,y;

        if (!isdigit(argv[1][0]) || !isdigit(argv[2][0]))
        {
          fprintf(stderr,"Error - victory position should be a number\n");
          return -1;
        }
	x = atoi(argv[1]);
	y = atoi(argv[2]);
        if (x<0 || y<0)
        {
          fprintf(stderr,"Error - victory position should be positive\n");
          return -1;
        }

        if (side<0)
          for (i=0; i<MAX_SIDES; i++)
            victory_add_position(i, x, y, is_enabled, s);
        else
          victory_add_position(side, x, y, is_enabled, s);
      }
      break;

  case OPTION_AI:

    if (!isdigit(argv[1][0]) || value_int<0 || value_int>=MAX_PLAYERS)
    {
      fprintf(stderr,"Error - AI player should be a number (not side color)\n");
      return -1;
    }

    Config->ai_algo_id[value_int] = atof(argv[2]);
    if (!isdigit(argv[2][0]) || Config->ai_algo_id[value_int]<0 || Config->ai_algo_id[value_int]>=AI_MAX_ALGO)
    {
      /* TODO: Same as above! */
      fprintf(stderr,"Error - AI algo number unknown (should be %d - %d)\n",0,AI_MAX_ALGO-1);
      return -1;
    }

    Config->ai_skill[value_int] = atof(argv[3]);
    if (Config->ai_skill[value_int] <= 0.0)
    {
      /* TODO: Use a more standardised error handling! */
      fprintf(stderr,"Error - AI skill level *must* be positive\n");
      return -1;
    }

    break;

  case OPTION_USE_SERVER:

    strcpy(Config->server_name,argv[1]);

    {
      int temp;

      temp = atoi(argv[2]);
      Config->server_port = temp;
      if (!isdigit(argv[2][0]) || temp<0 || temp>65535)
      {
        fprintf(stderr,"Error - Server port should be a number\n");
        return -1;
      }
    }

    Config->player_no = atoi(argv[3]);
    if (!isdigit(argv[3][0]) || Config->player_no<0 || Config->player_no>=MAX_PLAYERS)
    {
      fprintf(stderr,"Error - Player should be a number (no side color)\n");
      return -1;
    }

    break;
  }

  return 0;
}



/******************************************************************************
  clean_config ()

  Set global variables based on previously installed options.  Reconcile any
  paradoxical settings and initialize any variables that couldn't be set
  before all other options were installed.
******************************************************************************/
static void
clean_config (void)
{
  int side,
      value_int;

  double value_double,
         game_speed;

  char filename[500];

  game_speed =				Config->value_double_all[OPTION_SPEED];
  if (game_speed < 0.0)
    game_speed = 0.0;
  Config->delay =			(int)(25000.0/game_speed + 0.5);

  if (Config->value_int_all[OPTION_SEED] == 0)
    Config->value_int_all[OPTION_SEED] = (int)getpid() + (int)time(NULL);

  if (Config->value_int_all[OPTION_BOARDX]>MAX_BOARDSIZE)
  {
    Config->value_int_all[OPTION_BOARDX] = MAX_BOARDSIZE;
    fprintf(stderr,"WARNING: board width reduced to maximum (%d)\n",MAX_BOARDSIZE);
  }
  if (Config->value_int_all[OPTION_BOARDY]>MAX_BOARDSIZE)
  {
    Config->value_int_all[OPTION_BOARDY] = MAX_BOARDSIZE;
    fprintf(stderr,"WARNING: board height reduced to maximum (%d)\n",MAX_BOARDSIZE);
  }

  Config->board_x_size =		Config->value_int_all[OPTION_BOARDX];
  Config->board_y_size =		Config->value_int_all[OPTION_BOARDY];

  Config->fill_number =			Config->value_int_all[OPTION_SEA_TONES];
  Config->value_int_all[OPTION_FILL] =	Config->value_int_all[OPTION_SEA_TONES];
  Config->value_int_all[OPTION_DIG] =	Config->value_int_all[OPTION_SEA_TONES];

  for (side=0; side<Config->side_count; side++)
    Config->max_value[side] = Config->value_int[OPTION_MAXVAL][side];

  /** Find and set the maximum max_value - used as baseline **/

  Config->max_max_value = 0;
  for (side=0; side<Config->side_count; side++)
    if (Config->max_value[side] > Config->max_max_value)
      Config->max_max_value = Config->max_value[side];

  for (side=0; side<Config->side_count; side++)
  {
    /** Set costs as fractional parts of maxval, if necessary **/

    if (Config->value_int[OPTION_DIG_COST][side] <= 1)
      Config->value_int[OPTION_DIG_COST][side] = (int)
        (Config->value_double[OPTION_DIG_COST][side] * Config->max_max_value);

    if (Config->value_int[OPTION_FILL_COST][side] <= 1)
      Config->value_int[OPTION_FILL_COST][side] = (int)
	(Config->value_double[OPTION_FILL_COST][side] * Config->max_max_value);

    if (Config->value_int[OPTION_BUILD_COST][side] <= 1)
      Config->value_int[OPTION_BUILD_COST][side] = (int)
	(Config->value_double[OPTION_BUILD_COST][side] * Config->max_max_value);

    if (Config->value_int[OPTION_SCUTTLE_COST][side] < 1)
      Config->value_int[OPTION_SCUTTLE_COST][side] = (int)
  	(Config->value_double[OPTION_SCUTTLE_COST][side] * Config->max_max_value);

    Config->max_value[side] = Config->value_int[OPTION_MAXVAL][side];

    Config->view_range[side] =	Config->value_int[OPTION_HORIZON][side];

    if (Config->value_int[OPTION_CELL][side]>MAX_CELLSIZE)
    {
      Config->value_int[OPTION_CELL][side] = MAX_CELLSIZE;
      fprintf(stderr,"WARNING: cell size reduced to maximum (%d)\n",MAX_CELLSIZE);
    }
    Config->cell_size[side] =	Config->value_int[OPTION_CELL][side];

    value_int = 	Config->value_int[OPTION_BUILD][side];
    Config->value_int[OPTION_BUILD][side] = ANGLE_FULL/value_int;

    value_double = 	Config->value_double[OPTION_DECAY][side];
    Config->value_double[OPTION_DECAY][side] =
			10.0*value_double/((double)(Config->max_max_value));

    value_double = 	Config->value_double[OPTION_FIGHT][side];
    Config->value_double[OPTION_FIGHT][side] =
		value_double/(1.0+(game_speed/5.0));

    value_double = 	Config->value_double[OPTION_MOVE][side];
    Config->value_double[OPTION_MOVE][side] = (10.0-value_double)*MOVE_FACTOR;

    value_double = 	Config->value_double[OPTION_HILLS][side];
    Config->value_double[OPTION_HILLS][side] = value_double*HILL_FACTOR;

    value_double = 	Config->value_double[OPTION_FOREST][side];
    Config->value_double[OPTION_FOREST][side] = value_double*FOREST_FACTOR;
  }

#ifdef WITH_HILLS_AND_FOREST
  /** Using hills and forest together is now possible **/
#else
  /** Eliminate hills vs. forest paradox, hills get precedence **/

  if (Config->enable_all[OPTION_HILLS] && Config->enable_all[OPTION_FOREST])
    Config->enable_all[OPTION_FOREST] = FALSE;
#endif
  
  /** Eliminate any inconsistencies in mapping/horizon techniques **/

  for (side=0; side<Config->side_count; side++)
  {
    if (Config->enable[OPTION_MAP][side] ||
		Config->enable[OPTION_LOCALMAP][side])
    {
      Config->enable_all[OPTION_HORIZON] = TRUE;
      Config->enable[OPTION_HORIZON][side] = TRUE;
    }

    if (Config->enable[OPTION_LOCALMAP][side])
    {
      Config->enable[OPTION_MAP][side] = TRUE;
      Config->enable[OPTION_BASEMAP][side] = FALSE;
    }
  }

  /** Make sure maximum view range accurately reflects all view ranges **/

  if (Config->enable_all[OPTION_HORIZON])
  { 
    for (side=0; side<Config->side_count; side++)
    {
      if (Config->enable[OPTION_HORIZON][side])
        if (Config->view_range[side] > Config->view_range_max)
          Config->view_range_max = Config->view_range[side];
    }
  }

  /** Set up level (elevation) limits **/

  if (Config->enable_all[OPTION_SEA])
    Config->level_min = -(Config->value_int_all[OPTION_FILL]);
  else
      Config->level_min = 0;
  
#ifdef WITH_HILLS_AND_FOREST
  if (Config->enable_all[OPTION_FOREST])
      Config->forest_level_max = Config->value_int_all[OPTION_FOREST_TONES]-1;
  else
      Config->forest_level_max = 0;

  if (Config->enable_all[OPTION_HILLS])
      Config->level_max = Config->value_int_all[OPTION_HILL_TONES]-1;
  else
      Config->level_max = 0;
#else
  if (Config->enable_all[OPTION_FOREST])
    Config->level_max = Config->value_int_all[OPTION_FOREST_TONES]-1;
  else if (Config->enable_all[OPTION_HILLS])
    Config->level_max = Config->value_int_all[OPTION_HILL_TONES]-1;
  else
    Config->level_max = 0;
#endif

  /** Set up game storage **/

  if (Config->enable_all[OPTION_STORE])
  {
    if ((Config->fp = fopen (Config->file_store, "w")) == NULL)
      throw_error ("Cannot open storage file %s", Config->file_store);
    store_parameters (Config->fp);
  }

  /** Set up game replay **/

  if (Config->enable_all[OPTION_REPLAY])
  {
    if (strcmp (".Z", strchr (Config->file_replay, '\0')-2) == 0)
    {
      sprintf (filename, "zcat %s", Config->file_replay);
      Config->fp = popen (filename, "r");
    }
    else
      Config->fp = fopen (Config->file_replay, "r");

    /** If cannot open file, try default directory **/

    if (Config->fp == NULL)
    {
      if (strcmp (".Z", strchr (Config->file_replay, '\0')-2) == 0)
      {
        sprintf (filename, "zcat %s/%s", DEFAULT_XBA_DIR, Config->file_replay);
        Config->fp = popen (filename, "r");
      }
      else
      {
        sprintf (filename, "%s/%s", DEFAULT_XBA_DIR, Config->file_replay);
        Config->fp = fopen (filename, "r");
      }

      if (Config->fp == NULL)
        throw_error ("Cannot open replay file %s", Config->file_replay);
    }

    load_parameters (Config->fp);

    /** Don't want to load the board when replaying **/

    Config->enable_all[OPTION_LOAD] = FALSE;
  }

  /** Set up game load **/

  if (Config->enable_all[OPTION_LOAD])
  {
    /** If cannot open file, try default directory **/

    if ((Config->fp = fopen (Config->file_map, "r")) == NULL)
    {
      sprintf (filename, "%s/%s", DEFAULT_XBT_DIR, Config->file_map);
      if ((Config->fp = fopen (filename, "r")) == NULL)
        throw_error ("Cannot open map file %s", Config->file_map);
    }

    load_board_header (Config->fp);
  }
  else
  {
    if (Config->enable_all[OPTION_SQUARE])
      Config->tile_type = TILE_SQUARE;
    else if (Config->enable_all[OPTION_HEX])
      Config->tile_type = TILE_HEX;
    else if (Config->enable_all[OPTION_OCTAGON])
      Config->tile_type = TILE_OCTAGON;
    else if (Config->enable_all[OPTION_DIAMOND])
      Config->tile_type = TILE_DIAMOND;
    else if (Config->enable_all[OPTION_TRIANGLE])
      Config->tile_type = TILE_TRIANGLE;
    else
      Config->tile_type = TILE_SQUARE;
  }

  /** Set full hill/forest/sea palette **/

  set_palette (Config->palette_hills,
			 Config->value_int_all[OPTION_HILL_TONES],
			 MAX_HILL_TONES);
  set_palette (Config->palette_forest,
			Config->value_int_all[OPTION_FOREST_TONES],
			MAX_FOREST_TONES);
  set_palette (Config->palette_sea,
			Config->value_int_all[OPTION_SEA_TONES],
			MAX_SEA_TONES);

  /** Set movement biasing (due to hills, forests, etc.) **/

  set_move_parameters ();

  /** Need to set maximum directions before cell allocation **/

  switch (Config->tile_type)
  {
    case TILE_HEX:
      Config->direction_count = 6;
      break;

    case TILE_OCTAGON:
      Config->direction_count = 8;
      break;

    case TILE_SQUARE:
      Config->direction_count = 4;
      break;

    case TILE_DIAMOND:
      Config->direction_count = 4;
      break;

    case TILE_TRIANGLE:
      Config->direction_count = 3;
      break;
  }

  /** Copy global message to each player's local message **/

#if USE_MULTITEXT
  for (side=0; side<Config->side_count; side++)
    strcpy (Config->message_single[side], Config->message_all);
#endif
}



/******************************************************************************
  set_move_parameters ()

  Set movement factors which determine how troops move between cells.  This
  includes handling hills, forests, moves, and digins.  Eases the computational
  load on update_slope().
******************************************************************************/
static void
set_move_parameters (void)
{
  int side,
      level,
      value,
      moves;
  double *ptr;

  /** For each side **/

  for (side=0; side<Config->side_count; side++)
  {
    /** Handle level differences due to HILLS **/

    ptr = (double *)(malloc(sizeof(double)*(2*Config->level_max+1)));
    Config->move_slope[side] = ptr + Config->level_max;

    if (Config->level_max == 0)
      Config->move_slope[side][0] = 0.0;
    else
      for (level=-Config->level_max; level<=Config->level_max; level++)
        Config->move_slope[side][level] = Config->value_int[OPTION_HILLS][side] *
                ((double)level)/Config->level_max/HILLS_DIVISOR;

    /** Handle level effect due to MOVE and FOREST **/

#ifdef WITH_HILLS_AND_FOREST
    for (level=0; level<=Config->forest_level_max; level++)
    {
      Config->move_hinder[side][level] = Config->value_int[OPTION_MOVE][side] +
		Config->value_int[OPTION_FOREST][side] * level;
      Config->move_hinder[side][level] = 1.0/Config->move_hinder[side][level];
    }
#else
    for (level=0; level<=Config->level_max; level++)
    {
      Config->move_hinder[side][level] = Config->value_int[OPTION_MOVE][side] +
		Config->value_int[OPTION_FOREST][side] * level;
      Config->move_hinder[side][level] = 1.0/Config->move_hinder[side][level];
    }
#endif

    /** Handle troop effect due to DIGIN **/

    for (value=0; value<=MAX_MAXVAL+1; value++)
    {
      Config->move_shunt[side][value] = Config->value_int[OPTION_DIGIN][side] *
		((double)value)/Config->max_value[side] + 1.0;
      Config->move_shunt[side][value] = 1.0/Config->move_shunt[side][value];
    }
  }

  /** Take inverse of moves values **/

  for (moves=1; moves<MAX_DIRECTIONS; moves++)
    Config->move_moves[moves] = 1.0/moves;
  Config->move_moves[0] = 0.0;
}



/******************************************************************************
  free_move_parameters()

  Free resources allocated in set_move_paramters().
******************************************************************************/
static void
free_move_parameters(void)
{
  int side;

  for (side=0; side<Config->side_count; side++)
    free(Config->move_slope[side]-Config->level_max);
}



/******************************************************************************
  set_palette (palette, count, max_count)

  Fill in <palette> with <count> entries by interpolating between entries
  which are already in the palette.  Use the <max_count> entry as the top
  of the palette if there isn't a valid top entry already.
******************************************************************************/
static void
set_palette (short palette[][3], short count, short max_count)
{
  int i, j, k,
      last_solid_index,
      miss_count,
      source[3],
      target[3];

  double fraction;

  if (count < 2 || count >= max_count)
  {
    fprintf(stderr, "Palette entry count %d out of range (2>=c<%d)\n", count, max_count);
    return;
  }

  /** Set the top entry if there isn't one already **/

  if (palette[count-1][0] < 0)  
  {
    for (j=0; j<3; j++)
      palette[count-1][j] = palette[max_count-1][j];
  }

  /** For each palette entry **/

  for (i=0; i<count; i++)
  {
    /** If there is an entry **/

    if (palette[i][0] >= 0)
    {
      last_solid_index = i;

      /** Set interpolation anchor point **/

      for (j=0; j<3; j++)
        source[j] = palette[i][j];

      /** Find out how many missing entries there are in a row **/

      miss_count = 0;
      for (k=1; palette[i+k][0] < 0; k++)
        miss_count++;

      /** Set the other interpolation anchor point **/

      for (j=0; j<3; j++)
        target[j] = palette[i+k][j];
    }
    else
    {
      /** Else there is no entry, interpolate one **/

      fraction = ((double)(i - last_solid_index))/(miss_count+1);

      for (j=0; j<3; j++)
        palette[i][j] = source[j] + (int)(fraction*(target[j] - source[j]));
    }
  }
}



/******************************************************************************
  init_defaults ()

  Initialize all global variables, via the global <Option[]> and through
  explicit assignments.
******************************************************************************/
void
init_defaults (void)
{
  int i, j;

  /** Initialize global <Config> structure **/

  Config = (config_info *)(malloc(sizeof(config_info)));

  /** Initialize all of the values from global <Options[]> **/

  for (i=0; i<OPTION_COUNT; i++)
  {
    Config->enable_all[i] =			Options[i].enable;
    Config->value_int_all[i] =			Options[i].value_int;
    Config->value_double_all[i] =		Options[i].value_double;

    for (j=0; j<MAX_SIDES; j++)
    {
      Config->enable[i][j] =			Options[i].enable;
      Config->value_int[i][j] =			Options[i].value_int;
      Config->value_double[i][j] =		Options[i].value_double;
    }
  }

  /** Initialize command state arrays **/

  for (i=0; i<MAX_SIDES; i++)
  {
    Config->dir_type[i] =			MOVE_FORCE;
    for (j=0; j<4; j++)
      Config->dir[i][j] =			0;
    Config->in_message[i] =			FALSE;
  }

  /** Initialize other global variables **/

  Config->view_range_max =			0;
  Config->is_paused =				FALSE;

  Config->direction_count =			4;

  Config->text_size =				DEFAULT_TEXTSIZE;
  Config->text_offset =				DEFAULT_TEXT_X_OFFSET;

  Config->center_size =				DEFAULT_CENTERSIZE;
  Config->march_size =				DEFAULT_MARCHSIZE;

  /** Initialize AI variables **/

  for (i=0; i<MAX_PLAYERS;i++)
  {
    Config->ai_algo_id[i] =			0;
    Config->ai_skill[i] =			0.0;
  }

  /** Initialize hue and bw to inverse mappings **/

  Config->hue_count =				0;
  Config->bw_count =				0;

  for (i=0; i<MAX_HUES; i++)
    Config->hue_has_bw[i] = FALSE;

  for (i=0; Hues[i].hue_inverse != HUE_NONE; i++)
  {
    strcpy (Config->hue_name[i], Hues[i].name);
    for (j=0; j<3; j++)
      Config->palette[i][j] =			Hues[i].hue_triplet[j];
    Config->hue_to_inverse[i] =			Hues[i].hue_inverse;

    Config->hue_count++;

    if (Hues[i].bw_inverse != BW_NONE)
    {
      for (j=0; j<8; j++)
        Config->palette_gray[i][j] =		Hues[i].bw_octet[j];

      Config->bw_to_inverse[i] =		Hues[i].bw_inverse;
      Config->hue_has_bw[i] =			TRUE;

      Config->bw_count++;
    }
  }

  for (i=0; i<MAX_HILL_TONES; i++)
    Config->palette_hills[i][0] =		-1;
  for (i=0; i<MAX_FOREST_TONES; i++)
    Config->palette_forest[i][0] =		-1;
  for (i=0; i<MAX_SEA_TONES; i++)
    Config->palette_sea[i][0] =			-1;

  for (j=0; j<3; j++)
  {
    Config->palette_hills[0][j] =			Palette_Hills[0][j];
    Config->palette_hills[MAX_HILL_TONES-1][j] =	Palette_Hills[1][j];

    Config->palette_forest[0][j] =			Palette_Forest[0][j];
    Config->palette_forest[MAX_FOREST_TONES-1][j] =	Palette_Forest[1][j];

    Config->palette_sea[0][j] =				Palette_Sea[0][j];
    Config->palette_sea[MAX_SEA_TONES-1][j] =		Palette_Sea[1][j];
  }

  strcpy (Config->font, DEFAULT_FONT);
  sprintf (Config->message_all, "seed=%d ", Config->value_int_all[OPTION_SEED]);

  strcpy (Config->file_store_map, "xbattle.xbt");

  Config->server_port = 0;

  /** Set victory positions and parameters to default values **/

  victory_defaults ();
}


/******************************************************************************
  free_config ()

  Free resources allocated in init_defaults().
******************************************************************************/
void
free_config (void)
{
  free(Config);
}


/******************************************************************************
  int
  find_option (option)

  Search through the global <Options[]> for an option or negated option which
  matches <option> (eg "-no_grid" is negation of "-grid").  Return -1 if there
  is no match, <Options[]> index if there is a match, or <Options[]> index
  plus OPTION_COUNT if there is a negation match.
******************************************************************************/
static int
find_option (char const *option)
{
  int i;
  char string[MAX_TEXT];

  /** Search through all the normal options **/

  for (i=0; i<OPTION_COUNT; i++)
    if (!strcmp (option, Options[i].option))
      return (i);

  /** If <option> starts with "-no_" or "-no", remove and search again **/

  if (option[1] == 'n' && option[2] == 'o')
  {
    if (option[3] == '_')
      sprintf (string, "-%s", &option[4]);
    else
      sprintf (string, "-%s", &option[3]);

    for (i=0; i<OPTION_COUNT; i++)
      if (!strcmp (string, Options[i].option))
        return (i+OPTION_COUNT);
  }

  /** No option or negated option match, so return -1 **/

  return (-1);
}



/******************************************************************************
  int
  find_parameter_count (argc, argv, offset)

  Given a list of arguments in <argv[]>, and an <offset> into the list,
  return the number of parameters for that entry, where a parameter is defined
  as a consecutive entry which does not start with one of ('-','{','}').
******************************************************************************/
static int
find_parameter_count (int argc, char * const argv[], int offset)
{
  int count;

  for (count=0; (offset+count+1)<argc &&
			argv[offset+count+1][0] != '-' &&
			argv[offset+count+1][0] != '{' &&
			argv[offset+count+1][0] != '}'; count++);
  return (count);
}



/******************************************************************************
  int
  find_load_filename (option, filename)

  Determine if the option is actually a .xbo file, returning TRUE if so.
******************************************************************************/
static int
find_load_filename (char const *option, char *filename)
{
  char *suffix;

  FILE *fp;

  strcpy (filename, &option[1]);

  suffix = strstr (filename, "xbo");
  if (suffix == NULL)
    return (-1);

  if ((fp=fopen(filename, "r")) == NULL)
    return (-1);
  else
  {
    fclose (fp);
    return (OPTION_OPTIONS);
  }
}



/******************************************************************************
  find_color_match (option, color_name, use_second_color)

  Given an unknown argument <option>, presumably a color, search the custom
  color list and the X color list for a match, setting <color_name> to the
  name of the matched color.  If <use_second_color>, skip over any charcters
  before a "_" (ie, "red_black" becomes "black"), else just use characters
  before the "_" (ie, "red_black" becomes "red").
******************************************************************************/
static int
find_color_match (char const *option, char *color_name, int use_second_color)
{
  int i, j;
  char const *line;
  Display *display;
  int screen;
  Colormap cmap;
  XColor color;

  /** Either set pointer to first or second color **/

  if (use_second_color)
  {
    for (i=0; option[i] != '_' && option[i] != '\0'; i++);
    if (option[i] == '_')
      line = &option[i+1];
    else
      return (-1);
  }
  else
    line = &option[1];

  /** Check for match with custom colors **/

  for (j=0; j<Config->hue_count; j++)
  {
    if (matchstr (line, Config->hue_name[j]))
    {
      strcpy (color_name, Config->hue_name[j]);
      return (j);
    }
  }

  /** No custom color match, open up a dummy display for X color check **/

  display = XOpenDisplay ("");
  screen  = DefaultScreen (display);
  cmap = DefaultColormap (display, screen);

  /** Copy color name into dedicated string **/

  for (i=0; line[i] != '_' && line[i] != '\0'; i++)
    color_name[i] = line[i];
  color_name[i] = '\0';

  /** If color matches some X color, assign that X color to the custom	**/
  /** color palette and return the index.				**/

  if (XParseColor (display, cmap, color_name, &color))
  {
    return (load_color (color_name,
		color.red>>8, color.green>>8, color.blue>>8));
  }

  /** No custom or X color match, so return -1 **/

  return (-1);
}



/******************************************************************************
  load_color (hue_name, red, green, blue)

  Load custom color from option list into global <Config->hue_name>.
******************************************************************************/
static int
load_color (char const *hue_name, int red, int green, int blue)
{
  int i,
      hue_index;

  /** Search through existing hues, checking for match **/

  hue_index = Config->hue_count;
  for (i=0; i<Config->hue_count; i++)
  {
    if (strcmp (hue_name, Config->hue_name[i]) == 0)
      hue_index = i;
  }

  /** If there are too many hues, just overwrite the last one **/

  if (hue_index >= MAX_HUES)
    hue_index = MAX_HUES-1;
  else if (hue_index == Config->hue_count)
    Config->hue_count++;

  /** Set the hue **/

  strcpy (Config->hue_name[hue_index], hue_name);
  Config->palette[hue_index][0] = red;
  Config->palette[hue_index][1] = green;
  Config->palette[hue_index][2] = blue;

  /** Arbitrarily set inverse hue based on blue value **/

  if (blue > 128)
    Config->hue_to_inverse[hue_index] = 1;
  else
    Config->hue_to_inverse[hue_index] = 2;

  return (hue_index);
}



/******************************************************************************
  load_color_inverse (hue_name, inverse_hue_name)

  Establish line between color <hue_name> and color <inverse_hue_name>, if
  both colors can be found in palette.
******************************************************************************/
static void
load_color_inverse (char const *hue_name, char const *inverse_hue_name)
{
  int i,
      hue_index,
      inverse_hue_index;

  hue_index = -1;
  for (i=0; i<Config->hue_count; i++)
  {
    if (strcmp (hue_name, Config->hue_name[i]) == 0)
      hue_index = i;
  }

  inverse_hue_index = -1;
  for (i=0; i<Config->hue_count; i++)
  {
    if (strcmp (inverse_hue_name, Config->hue_name[i]) == 0)
      inverse_hue_index = i;
  }

  if (hue_index < 0 || inverse_hue_index < 0)
    throw_warning ("Unable to assign inverse colors", NULL);
  else
    Config->hue_to_inverse[hue_index] = inverse_hue_index;
}



/******************************************************************************
  load_stipple (hue_name, stipples)

******************************************************************************/
static int
load_stipple (char const *hue_name, char * const stipples[])
{
  int i,
      hue_index;

  /** Search through existing hues, checking for match **/

  hue_index = Config->hue_count;
  for (i=0; i<Config->hue_count; i++)
  {
    if (strcmp (hue_name, Config->hue_name[i]) == 0)
      hue_index = i;
  }

  /** If there are too many hues, just overwrite the last one **/

  if (hue_index >= MAX_HUES)
    hue_index = MAX_HUES-1;
  else if (hue_index == Config->hue_count)
    Config->hue_count++;

  /** Set the hue name **/

  strcpy (Config->hue_name[hue_index], hue_name);

  /** Let program know that hue has a stipple equivalent **/

  Config->hue_has_bw[hue_index] = TRUE;

  /** Set the stipple **/

  for (i=0; i<8; i++)
    Config->palette_gray[hue_index][i] = strtol (stipples[i], NULL, 0);

  return (hue_index);
}



/******************************************************************************
  print_usage_message ()

  Print the xbattle usage message (in global <Usage[]>) to stdout
******************************************************************************/
static void
print_usage_message (void)
{
  int i;

  printf ("%s\n", Usage[0]);
  for (i=1; Usage[i]; i++)
    printf ("\t%s\n", Usage[i]);
}



#ifdef UNUSED
/******************************************************************************
  int
  copy_first (dest, src)

  Copy the first white space delimited entry from <src[]> to <dest[]>.  Return
  the length of the entry.
******************************************************************************/
static int
copy_first (char *dest, char *src)
{
  int i;

  if (src == NULL)
    return (0);

  for (i=0; !isspace(src[i]); i++)
    dest[i] = src[i];
  dest[i] = '\0';

  return (i);
}
#endif



#ifdef UNUSED
/******************************************************************************
  char *
  strip_first (src)

  Return a pointer to the second space delimited entry of <src[]>.
******************************************************************************/
static char *
strip_first (char *src)
{
  int i;

  for (i=0; !isspace(src[i]); i++);
  if (src[i] == '\0' || src[i] == '\n')
    return (NULL);

  for (; isspace(src[i]); i++);
  if (src[i] == '\0')
    return (NULL);
  return (&src[i]);
}
#endif



/******************************************************************************
  set_double_array (array, value, index)

  Fill <array[]> with <value> if <index> < 0, else just set <array[index]>
******************************************************************************/
static void
set_double_array (double *array, double value, int index)
{
  int i;

  if (index < 0)
  {
    for (i=0; i<MAX_SIDES; i++)
      array[i] = value;
  }
  else
    array[index] = value;
}



/******************************************************************************
  set_int_array (array, value, index)

  Fill <array[]> with <value> if <index> < 0, else just set <array[index]>
******************************************************************************/
static void
set_int_array (int *array, int value, int index)
{
  int i;

  if (index < 0)
  {
    for (i=0; i<MAX_SIDES; i++)
      array[i] = value;
  }
  else
    array[index] = value;
}



/******************************************************************************
  set_uchar_array (array, value, index)

  Fill <array[]> with <value> if <index> < 0, else just set <array[index]>
******************************************************************************/
static void
set_uchar_array (unsigned char *array, unsigned char value, int index)
{
  int i;

  if (index < 0)
  {
    for (i=0; i<MAX_SIDES; i++)
      array[i] = value;
  }
  else
    array[index] = value;
}


/******************************************************************************
  read_options(fp, commandline, pos)

  Reads options from <fp> into <commandline> begining at <*pos> and
  incrementing <*pos> as options are read.
*****************************************************************************/
static void read_options(FILE *fp, char **commandline, int *pos)
{
  char c;
  char str[256];
  int i;

  /* FIXME: can't call feof() until after a read function is called */
  while(!feof(fp)) {
    /* Remove whitespaces */
    do { 
      c = (char) fgetc(fp); 
      if(c == '#') do { c = (char) fgetc(fp); if(feof(fp)) return; } while(c != '\n');
    } while(c == ' ' || c == '\t' || c == '\n');
    if(feof(fp)) return;

    for(i=0;i<255&&c!=' '&&c!='\t'&&c!='\n'&&c!='#';i++) {
      str[i] = c;
      c = fgetc(fp);
    }
    str[i] = 0;
    commandline[*pos] = strdup(str);
    (*pos)++;
  }
}
