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

#include "constant.h"
  
/**** x include files ****/
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>

#include "extern.h"

/* FIXME: should these be in constant.h? */
#define	SEA_VALUE_MAX		0.95
#define COLOR_ADD               100


static void create_xwindow (xwindow_type *xwindow, char const *hue_title,
	       	char const *bw_title, int current_side);
static void init_stipple (Display *display, Window window, Pixmap stipple[]);
static void init_terrain_pixmaps (xwindow_type *xwindow, shape_type const *shape,
	       	GC gc_on, GC gc_off, int index);


/******************************************************************************
  open_xwindow (xwindow, hue_title, bw_title)

  Open the window specified by <xwindow> and its fields, with title <bw_title>
  or <hue_title>, depending on whether b&w or color display.  This routine also
  handles everything else that has to do with graphics initialization, such as
  setting up colors, cell drawings, stipples, and so on.
******************************************************************************/
void
open_xwindow (xwindow_type *xwindow, char const *hue_title, char const *bw_title)
{
  int i, k,
      index,
      side,
      flag,
      current_side,
      level,
      pixel_count,
      hill_tone_count,
      forest_tone_count,
      sea_tone_count;

  double fraction;

  shape_type *shape=NULL;

  XPoint small_points[MAX_POINTS+1],
         size_bound;

  XColor xcolor_side[MAX_HUES+2],
         xcolor_inverse[MAX_HUES+2],
         xcolor_mark[2],
         *xcolor_terrain;

  GC gc_and,
     gc_all,
     gc_none,
     gc_on, gc_off;

#ifdef WITH_HILLS_AND_FOREST
  XColor *xcolor_forest;
#endif

#ifdef WITH_BASE_SIDE
  XColor xcolor_base[MAX_HUES+2];
  n_char red,green,blue;
#endif
  
  static Pixmap stipple[MAX_HUES+1];

  /** Set some convenience variables **/

  hill_tone_count = Config->value_int_all[OPTION_HILL_TONES];
  forest_tone_count = Config->value_int_all[OPTION_FOREST_TONES];
  sea_tone_count = Config->value_int_all[OPTION_SEA_TONES];

  pixel_count = 0;

  current_side = Config->player_to_side[xwindow->player];

  /** Create the basic window **/

  create_xwindow (xwindow, hue_title, bw_title, current_side);

  /** Allocate hue GC arrays **/

  xwindow->hue = (GC *)(malloc(sizeof(GC)*(Config->side_count+1)));
  xwindow->hue_inverse = (GC *)(malloc(sizeof(GC)*(Config->side_count+1)));
  xwindow->hue_mark = (GC *)(malloc(sizeof(GC)*(2)));
  xwindow->hue_terrain = (GC *)(malloc(sizeof(GC)*
                (Config->level_max - Config->level_min + 2)));
  xwindow->hue_terrain -= Config->level_min;
#ifdef WITH_HILLS_AND_FOREST
  xwindow->hue_forest = (GC *)(malloc(sizeof(GC)*
                (Config->forest_level_max + 2)));
  xwindow->hue_forest -= 0;
#endif
#ifdef WITH_BASE_SIDE
  xwindow->hue_base = (GC *)(malloc(sizeof(GC)*(Config->side_count)));
#endif

  /** Allocate terrain pixmaps, set global pointer with offset to sea-level **/

  for (index=0; index<Board->shape_count; index++)
  {
    xwindow->terrain[index] = (Pixmap *)(malloc(sizeof(Pixmap)*
                 (Config->level_max - Config->level_min + 2)));
    xwindow->terrain[index] -= Config->level_min;
#ifdef WITH_HILLS_AND_FOREST
    xwindow->forest[index] = (Pixmap *)(malloc(sizeof(Pixmap)*
                 (Config->forest_level_max + 2)));
    xwindow->forest[index] -= 0;
#endif

  }

  xcolor_terrain = malloc(sizeof(XColor)*(Config->level_max - Config->level_min + 2));
  xcolor_terrain -= Config->level_min;

#ifdef WITH_HILLS_AND_FOREST
  xcolor_forest = malloc(sizeof(XColor)*(Config->forest_level_max + 2));
#endif


  /** If color display, load colormap ***/

  if (xwindow->cmap != None)
  {
    /** Set each player's color **/

    for (side=0; side<Config->side_count; side++)
    {
      /** If its a custom color, set RGB from color list, else set RGB	**/
      /** using color name and X function XParseColor().		**/

      if (Config->side_to_hue[side] >= 0)
      {
#ifdef WITH_BASE_SIDE
        red = Config->palette[Config->side_to_hue[side]][0];
        green = Config->palette[Config->side_to_hue[side]][1];
        blue = Config->palette[Config->side_to_hue[side]][2];

        xcolor_side[side].red = red << 8;
        xcolor_side[side].green = green << 8;
        xcolor_side[side].blue =  blue << 8;

        /** Compute color for bases **/
        if (red > green && red > blue)
        {
          green+= COLOR_ADD;
          blue+= COLOR_ADD;
        }
       	else
       	{
          if (green > red && green > blue)
          {
            red+= COLOR_ADD;
            blue+= COLOR_ADD;
          }
	  else
	  {
            red+= COLOR_ADD;
            green+= COLOR_ADD;
          }
        }

        xcolor_base[side].red = red << 8;
        xcolor_base[side].green = green << 8;
        xcolor_base[side].blue = blue << 8;
#else
        xcolor_side[side].red =  
			Config->palette[Config->side_to_hue[side]][0]<<8;
        xcolor_side[side].green =
			Config->palette[Config->side_to_hue[side]][1]<<8;
        xcolor_side[side].blue = 
			Config->palette[Config->side_to_hue[side]][2]<<8;
#endif

        /** If custom color, directly set inverse index **/

        index = Config->hue_to_inverse[Config->side_to_hue[side]];
      }

      /** Set rest of normal color **/

      xcolor_side[side].flags = DoRed | DoGreen | DoBlue;
      xcolor_side[side].pixel = pixel_count++;
#ifdef WITH_BASE_SIDE
      xcolor_base[side].flags = DoRed | DoGreen | DoBlue;
      xcolor_base[side].pixel = pixel_count++;
#endif

      /** Set rest of inverse color **/

      xcolor_inverse[side].red =   Config->palette[index][0]<<8;
      xcolor_inverse[side].green = Config->palette[index][1]<<8;
      xcolor_inverse[side].blue =  Config->palette[index][2]<<8;
      xcolor_inverse[side].flags = DoRed | DoGreen | DoBlue;
      xcolor_inverse[side].pixel = pixel_count++;
    }

    /** Set special marking colors (used to be inverse background) **/

    index = match_color_name ("mark", 0);

    xcolor_mark[0].flags =	DoRed | DoGreen | DoBlue;
    xcolor_mark[0].pixel =	pixel_count++;
    xcolor_mark[0].red =	Config->palette[index][0]<<8;
    xcolor_mark[0].green =	Config->palette[index][1]<<8;
    xcolor_mark[0].blue =	Config->palette[index][2]<<8;

    index = match_color_name ("border", 0);

    xcolor_mark[1].flags =	DoRed | DoGreen | DoBlue;
    xcolor_mark[1].pixel =	pixel_count++;
    xcolor_mark[1].red =	Config->palette[index][0]<<8;
    xcolor_mark[1].green =	Config->palette[index][1]<<8;
    xcolor_mark[1].blue =	Config->palette[index][2]<<8;

    /** This will color the seas **/

    for (level=Config->level_min; level<0; level++)
    {
      if (Config->enable[OPTION_SEA_BLOCK][current_side])
        index = 0;
      else
        index = -level - 1;

      xcolor_terrain[level].flags =	DoRed | DoGreen | DoBlue;
      xcolor_terrain[level].pixel =	pixel_count++;
      xcolor_terrain[level].red =	Config->palette_sea[index][0]<<8;
      xcolor_terrain[level].green =	Config->palette_sea[index][1]<<8;
      xcolor_terrain[level].blue =	Config->palette_sea[index][2]<<8;
    }

#ifdef WITH_HILLS_AND_FOREST
    /** Set hill tones **/

    if (Config->enable_all[OPTION_HILLS])
        for (level=0; level<=Config->level_max; level++)
        {
            /** Use a linear interpolation through color space **/

            xcolor_terrain[level].flags =	DoRed | DoGreen | DoBlue;
            xcolor_terrain[level].pixel =	pixel_count++;
            xcolor_terrain[level].red =	        Config->palette_hills[level][0]<<8;
            xcolor_terrain[level].green =	Config->palette_hills[level][1]<<8;
            xcolor_terrain[level].blue =	Config->palette_hills[level][2]<<8;
        }
    else
    {
        /** Set basic background color **/

        xcolor_terrain[0].flags =	DoRed | DoGreen | DoBlue;
        xcolor_terrain[0].pixel =	pixel_count++;
        xcolor_terrain[0].red =		Config->palette[0][0]<<8;
        xcolor_terrain[0].green =	Config->palette[0][1]<<8;
        xcolor_terrain[0].blue =	Config->palette[0][2]<<8;
    }

    /** Set forest tones **/

    if (Config->enable_all[OPTION_FOREST])
        for (level=0; level<=Config->forest_level_max; level++)
        {
            /** Use a linear interpolation through color space **/
            xcolor_forest[level].flags =	DoRed | DoGreen | DoBlue;
            xcolor_forest[level].pixel =	pixel_count++;
            xcolor_forest[level].red   =	Config->palette_forest[level][0]<<8;
            xcolor_forest[level].green =	Config->palette_forest[level][1]<<8;
            xcolor_forest[level].blue  =	Config->palette_forest[level][2]<<8;
        }
#else
    for (level=0; level<=Config->level_max; level++)
    {
      /** Set hill tones **/

      if (Config->enable_all[OPTION_HILLS])
      {
        /** Use a linear interpolation through color space **/

        xcolor_terrain[level].flags =	DoRed | DoGreen | DoBlue;
        xcolor_terrain[level].pixel =	pixel_count++;
        xcolor_terrain[level].red =	Config->palette_hills[level][0]<<8;
        xcolor_terrain[level].green =	Config->palette_hills[level][1]<<8;
        xcolor_terrain[level].blue =	Config->palette_hills[level][2]<<8;
      }
      else if (Config->enable_all[OPTION_FOREST])
      {
        /** Use a linear interpolation through color space **/

        xcolor_terrain[level].flags =	DoRed | DoGreen | DoBlue;
        xcolor_terrain[level].pixel =	pixel_count++;
        xcolor_terrain[level].red =	Config->palette_forest[level][0]<<8;
        xcolor_terrain[level].green =	Config->palette_forest[level][1]<<8;
        xcolor_terrain[level].blue =	Config->palette_forest[level][2]<<8;
      }
      else
      {
        /** Set basic background color **/

        xcolor_terrain[0].flags =	DoRed | DoGreen | DoBlue;
        xcolor_terrain[0].pixel =	pixel_count++;
        xcolor_terrain[0].red =		Config->palette[0][0]<<8;
        xcolor_terrain[0].green =	Config->palette[0][1]<<8;
        xcolor_terrain[0].blue =	Config->palette[0][2]<<8;
      }
    }
#endif
    
    /** Terrain Config->level_max+1 is the "unknown" terrain in MAP modes **/
    /* P.Bureau - Use a specific color for "unknown" terrain */
    
    index = match_color_name ("map", 0);

    level=Config->level_max+1;
    
    xcolor_terrain[level].flags =	DoRed | DoGreen | DoBlue;
    xcolor_terrain[level].pixel =	pixel_count++;
    xcolor_terrain[level].red =		60<<8;
    xcolor_terrain[level].green =	60<<8;
    xcolor_terrain[level].blue =	60<<8;

#ifdef WITH_HILLS_AND_FOREST
    level=Config->forest_level_max+1;

    xcolor_forest[level].flags =	DoRed | DoGreen | DoBlue;
    xcolor_forest[level].pixel =	pixel_count++;
    xcolor_forest[level].red =		60<<8;
    xcolor_forest[level].green =	60<<8;
    xcolor_forest[level].blue =	        60<<8;
#endif

    /** Old code for "unknown" terrain color **/
    /*
    index = match_color_name ("map", 0);

    level=Config->level_max+1;
    
    xcolor_terrain[level].flags =	DoRed | DoGreen | DoBlue;
    xcolor_terrain[level].pixel =	pixel_count++;
    xcolor_terrain[level].red =		Config->palette[index][0]<<8;
    xcolor_terrain[level].green =	Config->palette[index][1]<<8;
    xcolor_terrain[level].blue =	Config->palette[index][2]<<8;
    */
    
    /** Either store colors in new or old colormap **/

    if (xwindow->use_new_colormap)
    {
      XStoreColors (xwindow->display, xwindow->cmap,
		        xcolor_side, Config->side_count);
      XStoreColors (xwindow->display, xwindow->cmap,
		        xcolor_inverse, Config->side_count);
      XStoreColors (xwindow->display, xwindow->cmap,
		        xcolor_mark, 2);
      XStoreColors (xwindow->display, xwindow->cmap,
                    xcolor_terrain + Config->level_min,
                    Config->level_max - Config->level_min + 2);
#ifdef WITH_HILLS_AND_FOREST
      XStoreColors (xwindow->display, xwindow->cmap,
                    xcolor_forest,
                    Config->forest_level_max + 1);
#endif
#ifdef WITH_BASE_SIDE
      XStoreColors (xwindow->display, xwindow->cmap,
                    xcolor_base, Config->side_count);
#endif
    }
    else
    {
      flag = FALSE;

      for (i=0; i<Config->side_count; i++)
	if (!XAllocColor (xwindow->display, xwindow->cmap, &xcolor_side[i]))
	  flag = TRUE;

      for (i=0; i<(Config->side_count+1); i++)
	if (!XAllocColor (xwindow->display, xwindow->cmap, &xcolor_inverse[i]))
	  flag = TRUE;

      for (i=0; i<2; i++)
	if (!XAllocColor (xwindow->display, xwindow->cmap, &xcolor_mark[i]))
	  flag = TRUE;

      for (level=Config->level_min; level<=Config->level_max+1; level++)
	if (!XAllocColor (xwindow->display, xwindow->cmap, &xcolor_terrain[level]))
            flag = TRUE;

#ifdef WITH_HILLS_AND_FOREST
      for (level=0; level<=Config->forest_level_max+1; level++)
          if (!XAllocColor (xwindow->display, xwindow->cmap, &xcolor_forest[level]))
              flag = TRUE;
#endif
#ifdef WITH_BASE_SIDE
      for (i=0; i<Config->side_count; i++)
          if (!XAllocColor (xwindow->display, xwindow->cmap, &xcolor_base[i]))
              flag = TRUE;
#endif

      if (flag)
	throw_warning ("Could not allocate all color cells\n%s",
		       "         Kill other colorful applications or use -new_colormap");
    }

    for (side=0; side<Config->side_count; side++)
      xwindow->draw_letter[side] = FALSE;
  }

  /** Raise window **/

  XMapRaised (xwindow->display, xwindow->window);

  /** Create GCs for each side and link to hues **/

  for (side=0; side<Config->side_count; side++)
  {
    xwindow->hue[side] = XCreateGC (xwindow->display, xwindow->window, 0, 0);
    XSetFunction (xwindow->display, xwindow->hue[side], GXcopy);
    xwindow->hue_inverse[side] = XCreateGC (xwindow->display,
                xwindow->window, 0, 0);
    XSetFunction (xwindow->display, xwindow->hue_inverse[side], GXcopy);
#ifdef WITH_BASE_SIDE
    xwindow->hue_base[side] = XCreateGC (xwindow->display, xwindow->window, 0, 0);
    XSetFunction (xwindow->display, xwindow->hue_base[side], GXcopy);
#endif
  }

  /** Create mark GCs and link to hues **/

  for (i=0; i<2; i++)
  {
    xwindow->hue_mark[i] = XCreateGC (xwindow->display, xwindow->window, 0, 0);
    XSetFunction (xwindow->display, xwindow->hue_mark[i], GXcopy);
  }

  /** Set the special purpose GCs for pixmap manipulation **/

  xwindow->gc_flip  = XCreateGC (xwindow->display, xwindow->window, 0, 0);
  XSetFunction (xwindow->display, xwindow->gc_flip,  GXinvert);

  xwindow->gc_clear  = XCreateGC (xwindow->display, xwindow->window, 0, 0);
  XSetFunction (xwindow->display, xwindow->gc_clear, GXandInverted);

  xwindow->gc_or = XCreateGC (xwindow->display, xwindow->window, 0, 0);
  XSetFunction (xwindow->display, xwindow->gc_or, GXor);

  /** If b&w display, create black and white GCs **/

  if (xwindow->cmap == None)
  {
    gc_on = XCreateGC (xwindow->display, xwindow->window, 0, 0);
    XSetFunction (xwindow->display, gc_on, GXcopy);

    gc_off = XCreateGC (xwindow->display, xwindow->window, 0, 0);
    XSetFunction (xwindow->display, gc_off, GXcopy);
  }

  /** Create terrain GCs and link to hues **/

  for (level=Config->level_min; level<=Config->level_max+1; level++)
  {
    xwindow->hue_terrain[level] = XCreateGC (xwindow->display,
                xwindow->window, 0, 0);
    XSetFunction (xwindow->display, xwindow->hue_terrain[level], GXcopy);
  }
#ifdef WITH_HILLS_AND_FOREST
  for (level=0; level<=Config->forest_level_max+1; level++)
  {
    xwindow->hue_forest[level] = XCreateGC (xwindow->display,
            xwindow->window, 0, 0);
    XSetFunction (xwindow->display, xwindow->hue_forest[level], GXcopy);
  }
#endif
  
  /** Set drawing GCs fow b&w displays **/

  if (xwindow->cmap == None)
  { 
    /** For each side, set b&w GCs (with inverses) **/

    for (side=0; side<Config->side_count; side++)
    {
      XSetForeground (xwindow->display, xwindow->hue[side],
                      BlackPixel (xwindow->display, xwindow->screen));
      XSetBackground (xwindow->display, xwindow->hue[side],
                      WhitePixel (xwindow->display, xwindow->screen));

      XSetForeground (xwindow->display, xwindow->hue_inverse[side],
                      BlackPixel (xwindow->display, xwindow->screen));
      XSetBackground (xwindow->display, xwindow->hue_inverse[side],
                      WhitePixel (xwindow->display, xwindow->screen));
    }

    /** Set background GC **/

    XSetForeground (xwindow->display, xwindow->hue_terrain[0],
                    BlackPixel (xwindow->display, xwindow->screen));
    XSetBackground (xwindow->display, xwindow->hue_terrain[0],
                    WhitePixel (xwindow->display, xwindow->screen));

    /** Set invisible terrain GC **/

    XSetForeground (xwindow->display, xwindow->hue_terrain[1],
                    BlackPixel (xwindow->display, xwindow->screen));
    XSetBackground (xwindow->display, xwindow->hue_terrain[1],
                    WhitePixel (xwindow->display, xwindow->screen));

    /** Set marking GC **/

    XSetForeground (xwindow->display, xwindow->hue_mark[0],
                    BlackPixel (xwindow->display, xwindow->screen));
    XSetBackground (xwindow->display, xwindow->hue_mark[0],
                    WhitePixel (xwindow->display, xwindow->screen));

    XSetForeground (xwindow->display, xwindow->hue_mark[1],
                    BlackPixel (xwindow->display, xwindow->screen));
    XSetBackground (xwindow->display, xwindow->hue_mark[1],
                    WhitePixel (xwindow->display, xwindow->screen));

    /** Set basic on and off GCs **/

    XSetForeground (xwindow->display, gc_on,
                    BlackPixel (xwindow->display, xwindow->screen));
    XSetBackground (xwindow->display, gc_on,
                    WhitePixel (xwindow->display, xwindow->screen));
    XSetForeground (xwindow->display, gc_off,
                    WhitePixel (xwindow->display, xwindow->screen));
    XSetBackground (xwindow->display, gc_off,
                    BlackPixel (xwindow->display, xwindow->screen));
  }

  /** Set drawing GCs for color displays **/

  if (xwindow->cmap != None)
  {
    /** Set colors for each side **/

    for (side=0; side<Config->side_count; side++)
    {
      XSetForeground (xwindow->display, xwindow->hue[side],
			xcolor_side[side].pixel);
      XSetBackground (xwindow->display, xwindow->hue[side],
			xcolor_inverse[side].pixel);
      XSetForeground (xwindow->display, xwindow->hue_inverse[side],
			xcolor_inverse[side].pixel);
      XSetBackground (xwindow->display, xwindow->hue_inverse[side],
                        xcolor_side[side].pixel);
#ifdef WITH_BASE_SIDE
      XSetForeground (xwindow->display, xwindow->hue_base[side],
			xcolor_base[side].pixel);
      XSetBackground (xwindow->display, xwindow->hue_base[side],
                        xcolor_inverse[side].pixel);
#endif
    }

    /** Set mark colors **/

    for (i=0; i<2; i++)
    {
      XSetForeground (xwindow->display, xwindow->hue_mark[i],
			xcolor_mark[i].pixel);
      XSetBackground (xwindow->display, xwindow->hue_mark[i],
			xcolor_terrain[0].pixel);
    }

    /** Set colors for each terrain level **/

    for (level=Config->level_min; level<=Config->level_max+1; level++)
    {
      XSetForeground (xwindow->display, xwindow->hue_terrain[level],
			xcolor_terrain[level].pixel);
      XSetBackground (xwindow->display, xwindow->hue_terrain[level],
			xcolor_terrain[0].pixel);
    }

#ifdef WITH_HILLS_AND_FOREST
    /** Set colors for each forest level **/

    for (level=0; level<=Config->forest_level_max + 1; level++)
    {
        XSetForeground (xwindow->display, xwindow->hue_forest[level],
                        xcolor_forest[level].pixel);
        XSetBackground (xwindow->display, xwindow->hue_forest[level],
                        xcolor_forest[0].pixel);
    }
#endif
    
  }
  else
  {
    /** Set all the stipple patterns for b&w display **/

    init_stipple (xwindow->display, xwindow->window, stipple);

    /** Set stipple for each side **/

    for (side=0; side<Config->side_count; side++)
    {
      XSetStipple (xwindow->display,
		xwindow->hue[side], stipple[Config->side_to_bw[side]]);
      XSetFillStyle (xwindow->display,
		xwindow->hue[side], FillOpaqueStippled);
      XSetStipple (xwindow->display,
		xwindow->hue_inverse[side],
		stipple[Config->bw_to_inverse[Config->side_to_bw[side]]]);
      XSetFillStyle (xwindow->display,
		xwindow->hue_inverse[side], FillOpaqueStippled);
    }

    /** Set background stipple **/

    XSetStipple (xwindow->display, xwindow->hue_terrain[0], stipple[0]);
    XSetFillStyle (xwindow->display,
		xwindow->hue_terrain[0], FillOpaqueStippled);

    /** Set invisible background stipple **/

    XSetStipple (xwindow->display, xwindow->hue_terrain[1], stipple[0]);
    XSetFillStyle (xwindow->display,
		xwindow->hue_terrain[1], FillOpaqueStippled);

    /** Set mark stipple **/

    XSetStipple (xwindow->display, xwindow->hue_mark[0],
		stipple[Config->bw_to_inverse[0]]);
    XSetFillStyle (xwindow->display, xwindow->hue_mark[0], FillOpaqueStippled);

    XSetStipple (xwindow->display, xwindow->hue_mark[1],
		stipple[Config->bw_to_inverse[1]]);
    XSetFillStyle (xwindow->display, xwindow->hue_mark[1], FillOpaqueStippled);
  }

#ifdef WITH_HILLS_AND_FOREST
  free(xcolor_forest);
#endif
  free(xcolor_terrain+Config->level_min);
  
  /** Create a work space pixmap (for double buffering), first finding	**/
  /** the maximum size of all the board shapes.				**/

  size_bound.x = 0;
  size_bound.y = 0;
  for (index=0; index<Board->shape_count; index++)
  {
    shape = Board->shapes[current_side][index];
    if (shape->size_bound.x > size_bound.x)
      size_bound.x = shape->size_bound.x;
    if (shape->size_bound.y > size_bound.y)
      size_bound.y = shape->size_bound.y;
  }

  xwindow->work_space = XCreatePixmap (xwindow->display, xwindow->window,
		size_bound.x, size_bound.y, xwindow->depth);

  /** If needed, create a backing pixmap (for full double buffering) **/

  if (shape->copy_method == COPY_BACK)
    xwindow->backing_space = XCreatePixmap (xwindow->display, xwindow->window,
		xwindow->size_window.x, xwindow->size_window.y, xwindow->depth);
  else
    xwindow->backing_space = None;

  /** Create the individual terrain pixmaps **/

  for (index=0; index<Board->shape_count; index++)
  {
    shape = Board->shapes[current_side][index];

    for (level=Config->level_min; level<=Config->level_max+1; level++)
        xwindow->terrain[index][level] =
            XCreatePixmap (xwindow->display, xwindow->window,
                           shape->size_bound.x, 2*shape->size_bound.y, xwindow->depth);

#ifdef WITH_HILLS_AND_FOREST
    for (level=0; level<=Config->forest_level_max + 1; level++)
        xwindow->forest[index][level] =
            XCreatePixmap (xwindow->display, xwindow->window,
                           shape->size_bound.x, 2*shape->size_bound.y, xwindow->depth);
#endif
  }

#ifdef XDEBUGGING
  /** Don't turn this on by default because it causes a round trip for every **/
  /** X request which is really slow over a network **/
  XSynchronize(xwindow->display, 1);
#endif

  /** If a b&w display, initialize error diffused terrain "colors" **/

  if (xwindow->cmap == None)
  {
    for (index=0; index<Board->shape_count; index++)
    {
      shape = Board->shapes[current_side][index];

      init_terrain_pixmaps (xwindow, shape, gc_on, gc_off, index);
    }

    /** Done with B&W GCs **/

    XFreeGC(xwindow->display, gc_off);
    XFreeGC(xwindow->display, gc_on);
  }

  /** Set up temporary GCs for pixmap manipulation **/

  gc_and  = XCreateGC (xwindow->display, xwindow->window, 0, 0);
  XSetFunction (xwindow->display, gc_and, GXand);

  gc_all  = XCreateGC (xwindow->display, xwindow->window, 0, 0);
  XSetFunction (xwindow->display, gc_all, GXset);

  gc_none  = XCreateGC (xwindow->display, xwindow->window, 0, 0);
  XSetFunction (xwindow->display, gc_none, GXclear);

  /** For each cell shape **/

  for (index=0; index<Board->shape_count; index++)
  {
    shape = Board->shapes[current_side][index];

    /** For each terrain level **/

    for (level=Config->level_min; level<=Config->level_max+1; level++)
    {
      /** Fill top frame with appropriate terrain color **/

      shape->points[0].x = shape->corner_vertex.x;
      shape->points[0].y = shape->corner_vertex.y;

      if (xwindow->cmap != None)
        XFillPolygon (xwindow->display, xwindow->terrain[index][level],
		xwindow->hue_terrain[level],
		shape->points, shape->point_count-1,
		Convex, CoordModePrevious);

      if (Config->enable[OPTION_GRID][side])
        XDrawLines (xwindow->display, xwindow->terrain[index][level],
		xwindow->hue_mark[0],
		shape->points, shape->point_count,
		CoordModePrevious);
      else
        XDrawLines (xwindow->display, xwindow->terrain[index][level],
		xwindow->hue_terrain[level],
		shape->points, shape->point_count,
		CoordModePrevious);

      /** Fill bottom frame with gc_none to "black-out" everything **/

      XFillRectangle (xwindow->display, xwindow->terrain[index][level], gc_none,
		0, shape->size_bound.y,
		shape->size_bound.x, shape->size_bound.y);

      /** Draw polygonal template in bottom frame with gc_all **/

      shape->points[0].x = shape->corner_vertex.x;
      shape->points[0].y = shape->corner_vertex.y + shape->size_bound.y;

      XFillPolygon (xwindow->display, xwindow->terrain[index][level], gc_all,
		shape->points, shape->point_count-1,
		Convex, CoordModePrevious);

      XDrawLines (xwindow->display, xwindow->terrain[index][level], gc_all,
		shape->points, shape->point_count,
		CoordModePrevious);

      /** If using blocky sea method **/

      if (Config->enable[OPTION_SEA_BLOCK][current_side] &&
		level < 0 && level > Config->level_min)
      {
        /** Draw terrain 0 part of cell in top frame **/

        shape->points[0].x = shape->corner_vertex.x;
        shape->points[0].y = shape->corner_vertex.y;

        if (xwindow->cmap != None)
          XFillPolygon (xwindow->display, xwindow->terrain[index][level],
		xwindow->hue_terrain[0],
		shape->points, shape->point_count-1,
		Convex, CoordModePrevious);

        if (Config->enable[OPTION_GRID][side])
          XDrawLines (xwindow->display, xwindow->terrain[index][level],
		xwindow->hue_mark[0],
		shape->points, shape->point_count,
		CoordModePrevious);
        else
          XDrawLines (xwindow->display, xwindow->terrain[index][level],
		xwindow->hue_terrain[0],
		shape->points, shape->point_count,
		CoordModePrevious);

        /** Compute fraction which should be sea filled **/

        fraction = ((double)(-level-1))*
		((1.0 - SEA_BLOCK_MIN)/((double)(sea_tone_count-1))) +
		SEA_BLOCK_MIN;

        /** Set upper left vertex of sea part of hex **/

        small_points[0].x = shape->corner_vertex.x + (int)((1.0-fraction)*
		(shape->center_bound.x - shape->corner_vertex.x) + 0.5);
        small_points[0].y = shape->corner_vertex.y + (int)((1.0-fraction)*
		(shape->center_bound.y - shape->corner_vertex.y) + 0.5);

        /** Set relative vertices of sea part of cell **/

        for (k=1; k<shape->point_count; k++)
        {
          if (shape->points[k].x < 0)
            small_points[k].x = (int)(fraction * shape->points[k].x - 0.5);
          else
            small_points[k].x = (int)(fraction * shape->points[k].x + 0.5);

          if (shape->points[k].y < 0)
            small_points[k].y = (int)(fraction * shape->points[k].y - 0.5);
          else
            small_points[k].y = (int)(fraction * shape->points[k].y + 0.5);
        }

        /** Draw small sea polygon **/

        if (xwindow->cmap != None)
          XFillPolygon (xwindow->display, xwindow->terrain[index][level],
		xwindow->hue_terrain[level],
		small_points, shape->point_count-1,
		Convex, CoordModePrevious);
      }

      /** Copy the cell from the lower position to upper position **/

      XCopyArea (xwindow->display, xwindow->terrain[index][level],
		xwindow->terrain[index][level], gc_and,
		0, shape->size_bound.y,
		shape->size_bound.x, shape->size_bound.y,
		0, 0);
    }

#ifdef WITH_HILLS_AND_FOREST
    /** For each forest level **/
    
    for (level=0; level<=Config->forest_level_max +1; level++)
    {
      /** Fill top frame with appropriate terrain color **/

      shape->points[0].x = shape->corner_vertex.x;
      shape->points[0].y = shape->corner_vertex.y;

      if (xwindow->cmap != None)
        XFillPolygon (xwindow->display, xwindow->forest[index][level],
		xwindow->hue_forest[level],
		shape->points, shape->point_count-1,
		Convex, CoordModePrevious);

      if (Config->enable[OPTION_GRID][side])
        XDrawLines (xwindow->display, xwindow->forest[index][level],
		xwindow->hue_mark[0],
		shape->points, shape->point_count,
		CoordModePrevious);
      else
        XDrawLines (xwindow->display, xwindow->forest[index][level],
		xwindow->hue_forest[level],
		shape->points, shape->point_count,
		CoordModePrevious);

      /** Fill bottom frame with gc_none to "black-out" everything **/

      XFillRectangle (xwindow->display, xwindow->forest[index][level], gc_none,
		0, shape->size_bound.y,
		shape->size_bound.x, shape->size_bound.y);

      /** Draw polygonal template in bottom frame with gc_all **/

      shape->points[0].x = shape->corner_vertex.x;
      shape->points[0].y = shape->corner_vertex.y + shape->size_bound.y;

      XFillPolygon (xwindow->display, xwindow->forest[index][level], gc_all,
		shape->points, shape->point_count-1,
		Convex, CoordModePrevious);

      XDrawLines (xwindow->display, xwindow->forest[index][level], gc_all,
		shape->points, shape->point_count,
		CoordModePrevious);

      /** Copy the cell from the lower position to upper position **/

      XCopyArea (xwindow->display, xwindow->forest[index][level],
		xwindow->forest[index][level], gc_and,
		0, shape->size_bound.y,
		shape->size_bound.x, shape->size_bound.y,
		0, 0);
    }
#endif
  }

  XFreeGC(xwindow->display, gc_none);
  XFreeGC(xwindow->display, gc_all);
  XFreeGC(xwindow->display, gc_and);

  /** Load information font **/

  xwindow->font_struct = XLoadQueryFont (xwindow->display, Config->font);

  /** If can't find font, use default **/

  if (!xwindow->font_struct)
  {
    throw_warning ("Could not find font %s, using default", Config->font);
    Config->font[0] = '\0';
    xwindow->font_struct = XQueryFont (xwindow->display,
		XGContextFromGC(DefaultGC(xwindow->display,xwindow->screen)));
  }
  else
  {
    /** Else found font, link font to each side GC **/

    for (side=0; side<Config->side_count; side++)
    {
      XSetFont (xwindow->display,
		xwindow->hue[side], xwindow->font_struct->fid);
      XSetFont (xwindow->display,
		xwindow->hue_inverse[side], xwindow->font_struct->fid);
    }
    XSetFont (xwindow->display, xwindow->hue_mark[0], xwindow->font_struct->fid);
    XSetFont (xwindow->display, xwindow->hue_mark[1], xwindow->font_struct->fid);
    XSetFont (xwindow->display, xwindow->gc_flip,  xwindow->font_struct->fid);
  }

  /** Get font with and height **/

  XGetFontProperty (xwindow->font_struct, XA_QUAD_WIDTH, &xwindow->char_width);
  XGetFontProperty (xwindow->font_struct, XA_CAP_HEIGHT, &xwindow->char_height);

  /** If a b&w display, check to see if letters should be drawn in cells **/

  if (xwindow->cmap == None)
  {
    for (side=0; side<Config->side_count; side++)
    {
      if (Config->side_to_letter[side][0])
      {
        xwindow->draw_letter[side] = TRUE;
        strcpy (xwindow->letter[side], Config->side_to_letter[side]);
      }
      else
        xwindow->draw_letter[side] = FALSE;
    }
  }

  /** Map window to make it visible **/

  if (xwindow->cmap != None)
    XMapWindow (xwindow->display, xwindow->window);

}



/******************************************************************************
  create_xwindow (xwindow, hue_title, bw_title, current_side)

  Perform the actual window creation as specfied by <xwindow>.
******************************************************************************/
static void
create_xwindow (xwindow_type *xwindow, char const *hue_title,
	       	char const *bw_title, int current_side)
{
  XSizeHints hint;
  XWMHints xwm_hint;
  Visual *visual;
  unsigned long valuemask;
  XSetWindowAttributes attrib;
  XVisualInfo vinfo;
  long event_mask;
  Atom wm_delete_window;

  /** Open display and screen **/

  xwindow->display = XOpenDisplay (xwindow->display_name);
  if (xwindow->display == NULL)
    throw_error ("Cannot open display %s", xwindow->display_name);

  xwindow->screen  = DefaultScreen (xwindow->display);

  /** Set player viewing flags **/

  xwindow->open  =	TRUE;
  xwindow->watch  =	FALSE;
  xwindow->use_new_colormap = FALSE;
  xwindow->cmap  = None;

  /** Set window position and size hints for window manager **/

  hint.x =	Config->enable[OPTION_MANPOS][current_side] ?
			-1 : Config->value_int_all[OPTION_XPOS];
  hint.y =	Config->enable[OPTION_MANPOS][current_side] ?
			-1 : Config->value_int_all[OPTION_YPOS];
  hint.width =	xwindow->size_window.x;
  hint.height =	xwindow->size_window.y;

  if (hint.x < 0 || hint.y < 0)
    hint.flags = ( PPosition | PSize);
  else
    hint.flags = (USPosition | PSize);

  /** Set bitplane depth to default **/

  xwindow->depth = DefaultDepth (xwindow->display, xwindow->screen);

  /** Get a visual **/

  visual = DefaultVisual (xwindow->display, xwindow->screen);

  if (Config->enable[OPTION_VISUAL][current_side] != 0)
  {
    XVisualInfo *v;
    int nelem;
    vinfo.visualid=Config->value_int[OPTION_VISUAL][current_side];
    /* printf("switcing to visual 0x%x\n", vinfo.visualid); */
    v=XGetVisualInfo(xwindow->display, VisualIDMask, &vinfo, &nelem);
    if (nelem == 1)
      visual = v->visual;
  }

  /*
  if (XMatchVisualInfo(xwindow->display, xwindow->screen,
		       24, DirectColor, &vinfo))
    visual = vinfo.visual;
  */

  /*
  printf("visualid %d, class %d, depth %d\n",
	 (int)visual->visualid,
	 (int)visual->class,
	 (int)xwindow->depth);
  */


  switch (visual->class)
  {
    case DirectColor:
      /** If visual is DirectColor, this seems to work **/
      /* FIXME: default colormap should work, no need to create a new one */

      xwindow->cmap = XCreateColormap (xwindow->display,
			   RootWindow (xwindow->display, xwindow->screen),
			   visual, AllocNone);
      break;
      
    case TrueColor:
      /** If visual is TrueColor, have to create a dedicated colormap **/
      /* FIXME: default colormap should work, no need to create a new one */

      xwindow->cmap = XCreateColormap (xwindow->display,
			RootWindow (xwindow->display, xwindow->screen),
			visual, AllocNone);
      break;

    case PseudoColor:
      /** If visual is PseudoColor, we let the user choose between
	  using the system colormap or making a new (private) colormap **/

      if (Config->enable[OPTION_NEW_COLORMAP][current_side])
      {
	xwindow->cmap = XCreateColormap (xwindow->display,
			  RootWindow (xwindow->display, xwindow->screen),
			  visual, AllocAll);
	xwindow->use_new_colormap=TRUE;
      }
      else
      {
	xwindow->cmap = DefaultColormap (xwindow->display, xwindow->screen);
      }
      break;

    default:
      /** If we do not handle the color model of the visual, just use
          black/white stripple patterns **/
      /* FIXME: why was depth forced to 1 here? -- we should set a flag for
         ourselves because the display may have more than one plane. */
      /* xwindow->depth=1; */
      break;
  }

  /** If using color **/
  if (xwindow->cmap != None)
  {
    /** Set attributes **/

    valuemask = CWBackPixel | CWBorderPixel | CWBitGravity | CWColormap;
    attrib.background_pixel = BlackPixel (xwindow->display, xwindow->screen);
    attrib.border_pixel = BlackPixel (xwindow->display, xwindow->screen);
    attrib.bit_gravity = CenterGravity;
    attrib.colormap = xwindow->cmap;

    /** Create the x window **/

    xwindow->window = XCreateWindow (xwindow->display,
		RootWindow (xwindow->display, xwindow->screen),
		hint.x, hint.y, hint.width,
		hint.height, DEFAULT_BORDER, xwindow->depth,
		InputOutput, visual, valuemask, &attrib);
  }
  else
  {
    /** Else create b&w window **/

    xwindow->window = XCreateSimpleWindow (xwindow->display,
		DefaultRootWindow (xwindow->display),
		hint.x, hint.y,
		hint.width, hint.height, DEFAULT_BORDER,
		BlackPixel (xwindow->display, xwindow->screen),
		WhitePixel (xwindow->display, xwindow->screen));
  }

  /** Set up system to gracefully close windows **/

  wm_delete_window = XInternAtom (xwindow->display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(xwindow->display, xwindow->window, &wm_delete_window, 1);

  /** Set standard properties (from hints) for window manager **/

  if (xwindow->cmap != None)
    XSetStandardProperties (xwindow->display, xwindow->window, hue_title,
			hue_title, None, NULL, None, &hint);
  else
    XSetStandardProperties (xwindow->display, xwindow->window, bw_title,
			bw_title, None, NULL, None, &hint);

  /** Set window manager hints **/

  xwm_hint.flags = (InputHint|StateHint);  
  xwm_hint.input = TRUE;
  xwm_hint.initial_state = NormalState;
  XSetWMHints (xwindow->display, xwindow->window, &xwm_hint);

  /** Make window sensitive selected events **/

  if (Config->enable_all[OPTION_BOUND])
    event_mask = ButtonPressMask|ButtonReleaseMask|KeyPressMask|ExposureMask;
  else
    event_mask = ButtonPressMask|KeyPressMask|ExposureMask;
  XSelectInput (xwindow->display, xwindow->window, event_mask);
}



/******************************************************************************
  close_xwindow (xwindow)

  Gracefully close a window and free resources.
******************************************************************************/
void
close_xwindow (xwindow_type *xwindow)
{
  int index,
      side,
      level,
      i;
 
  if (xwindow->font_struct)
  {
    if (Config->font[0]!='\0')
      XFreeFont(xwindow->display, xwindow->font_struct);
    else
      XFreeFontInfo(NULL, xwindow->font_struct, 0);
  }
  for (index=0; index<Board->shape_count; index++)
  {
#ifdef WITH_HILLS_AND_FOREST
    for (level=0; level<Config->forest_level_max; level++)
      XFreePixmap(xwindow->display, xwindow->forest[index][level]);
#endif
    for (level=Config->level_min; level<=Config->level_max; level++)
      XFreePixmap(xwindow->display, xwindow->terrain[index][level]);
  }
  if (xwindow->backing_space!=None)
      XFreePixmap(xwindow->display, xwindow->backing_space);
  XFreePixmap(xwindow->display, xwindow->work_space);
#ifdef WITH_HILLS_AND_FOREST
  for (level=0; level<=Config->forest_level_max+1; level++)
    XFreeGC(xwindow->display, xwindow->hue_forest[level]);
#endif
  for (level=Config->level_min; level<=Config->level_max+1; level++)
    XFreeGC(xwindow->display, xwindow->hue_terrain[level]);

  XFreeGC(xwindow->display, xwindow->gc_or);
  XFreeGC(xwindow->display, xwindow->gc_clear);
  XFreeGC(xwindow->display, xwindow->gc_flip);
  for (i=0; i<2; i++)
    XFreeGC(xwindow->display, xwindow->hue_mark[i]);
  for (side=0; side<Config->side_count; side++)
  {
#ifdef WITH_BASE_SIDE
    XFreeGC(xwindow->display, xwindow->hue_base[side]);
#endif
    XFreeGC(xwindow->display, xwindow->hue_inverse[side]);
    XFreeGC(xwindow->display, xwindow->hue[side]);
  }

  for (index=0; index<Board->shape_count; index++)
  {
#ifdef WITH_HILLS_AND_FOREST
    free(xwindow->forest[index]);
#endif
    free(xwindow->terrain[index]+Config->level_min);
  }
#ifdef WITH_BASE_SIDE
  free(xwindow->hue_base);
#endif
#ifdef WITH_HILLS_AND_FOREST
  free(xwindow->hue_forest);
#endif
  free(xwindow->hue_terrain+Config->level_min);
  free(xwindow->hue_mark);
  free(xwindow->hue_inverse);
  free(xwindow->hue);

  XDestroyWindow(xwindow->display, xwindow->window);
  if (xwindow->cmap!=None &&
      xwindow->cmap!=DefaultColormap(xwindow->display, xwindow->screen))
    XFreeColormap(xwindow->display, xwindow->cmap);
  XCloseDisplay(xwindow->display);
}



/******************************************************************************
  init_stipple (display, window, stipple)

  Create the stipple patterns which are used on b&w displays.
******************************************************************************/
static void
init_stipple (Display *display, Window window, Pixmap stipple[])
{
  int i;

  for (i=0; i<Config->hue_count; i++)
  {
    if (Config->hue_has_bw[i])
      stipple[i] = XCreateBitmapFromData (display, window,
				Config->palette_gray[i], 8, 8);
  }
}



/******************************************************************************
  init_terrain_pixmaps (xwindow, shape, gc_on, gc_off, index)

  Using an error diffusion algorithm, initialize the terrain patterns for b&w
  displays.  The number of distinct patterns is governed by the terrain limits,
  with <gc_on> and <gc_off> providing the GCs for on and off bits respectively.
******************************************************************************/
static void
init_terrain_pixmaps (xwindow_type *xwindow, shape_type const *shape, GC gc_on, GC gc_off, int index)
{
  int x, y,
      tone_count,
      level,
      enable_quasi;

  double value,
         quasi,
         sea_value_min,
         target[2*MAX_CELLSIZE+DIFFUSE_BUFFER]
			[2*MAX_CELLSIZE+DIFFUSE_BUFFER],
         error; /* FIXME: why 2xMAX ? */

  /** Set the number of diffused tones to create **/ 

  tone_count = Config->level_max;
  sea_value_min = Config->value_double_all[OPTION_SEA_VALUE];

  /** For each desired level of greytone **/

  for (level=Config->level_min; level<=Config->level_max; level++)
  {
    /** Compute desired graylevel in [0,1] **/

    if (level < 0)
    {
      if (Config->level_min == -1)
        value = sea_value_min;
      else
        value = sea_value_min - (1 + level)*
		(sea_value_min - SEA_VALUE_MAX)/(Config->level_min+1);
    }
    else if (Config->enable_all[OPTION_FOREST])
      value = DIFFUSE_MAX_LEVEL - DIFFUSE_SPAN +
		level * DIFFUSE_SPAN/tone_count;
    else if (Config->level_max == 0)
      value = 0.5;
    else
      value = DIFFUSE_MAX_LEVEL - level * DIFFUSE_SPAN/tone_count;

    /** Enable quasi-random error diffusion if many levels **/

    if (level < 0)
      enable_quasi = TRUE;
    else if (tone_count <= DIFFUSE_QUASI_THRESHOLD)
      enable_quasi = FALSE;
    else
      enable_quasi = TRUE;

    /** Set the target gray values in a buffered grid **/

    for (x=0; x<shape->size_bound.x+DIFFUSE_BUFFER; x++)
      for (y=0; y<shape->size_bound.y+DIFFUSE_BUFFER; y++)
        target[x][y] = value;

    /** For each pixel do threshold and diffuse error **/

    for (x=0; x<(shape->size_bound.x + DIFFUSE_BUFFER); x++)
    {
      for (y=0; y<(shape->size_bound.y + DIFFUSE_BUFFER); y++)
      {
        /** Draw target point into pixmap with appropriate GC, update grid **/

        if (target[x][y] > DIFFUSE_MEAN_LEVEL)
        {
          if (x>=DIFFUSE_BUFFER_HALF &&
			x<(shape->size_bound.x + DIFFUSE_BUFFER_HALF) &&
			y>=DIFFUSE_BUFFER_HALF &&
			y<(shape->size_bound.y + DIFFUSE_BUFFER_HALF))
          {
            XDrawPoint (xwindow->display, xwindow->terrain[index][level],
			gc_on,
			x - DIFFUSE_BUFFER_HALF, y - DIFFUSE_BUFFER_HALF);
          }
          error = target[x][y] - 1.0;
        }
        else
        {
          if (x>=DIFFUSE_BUFFER_HALF &&
			x<(shape->size_bound.x + DIFFUSE_BUFFER_HALF) &&
			y>=DIFFUSE_BUFFER_HALF &&
			y<shape->size_bound.y + DIFFUSE_BUFFER_HALF)
          {
            XDrawPoint (xwindow->display, xwindow->terrain[index][level],
			gc_off,
			x - DIFFUSE_BUFFER_HALF, y - DIFFUSE_BUFFER_HALF);
          }
          error = target[x][y];
        }

        /** Enable randomness if over 5 levels (removes worms) **/

        if (enable_quasi)
          quasi = DIFFUSE_QUASI_FACTOR * get_random(100);
        else
          quasi = 0;

        /** Distribute error amongst the pixel neighbors, being careful **/
        /** to handle edge conditions properly.				**/

        if (y == 0)
        {
          if (x == shape->size_bound.x+DIFFUSE_BUFFER-1)
          {
            target[x][y+1] +=	DIFFUSE_1_SI * error;
          }
          else
          {
            target[x][y+1] +=	(DIFFUSE_3_SI + quasi) * error;
            target[x+1][y] +=	(DIFFUSE_3_IS + quasi) * error;
            target[x+1][y+1] += DIFFUSE_3_II * error;
          }
        }
        else if (y == shape->size_bound.y+DIFFUSE_BUFFER-1)
        {
          if (x != shape->size_bound.x+DIFFUSE_BUFFER-1)
          {
            target[x+1][y] +=	(DIFFUSE_2_IS + quasi) * error;
            target[x+1][y-1] +=	(DIFFUSE_2_ID - quasi) * error;
          }
        }
        else if (x == shape->size_bound.x+DIFFUSE_BUFFER-1)
        {
          target[x][y+1] += 	DIFFUSE_1_SI * error;
        }
        else
        {
          target[x][y+1] +=	(DIFFUSE_4_SI + quasi) * error;
          target[x+1][y] +=	(DIFFUSE_4_IS - quasi) * error;
          target[x+1][y+1] +=	DIFFUSE_4_II * error;
          target[x+1][y-1] +=	DIFFUSE_4_ID * error;
        }
      }
    }
  }
}
