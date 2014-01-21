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
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
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
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
# include <netdb.h>
#endif
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#ifdef HAVE_PTHREAD_H
# include <pthread.h>
#endif

/**** x include files ****/
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>

#include "constant.h"
#if USE_LONGJMP
# ifdef HAVE_SETJMP_H
#  include <setjmp.h>
# endif
#endif
#include "extern.h"

#include "keyboard.h"
#include "ai.h"


/** Program globals **/
xwindow_type *XWindow[MAX_PLAYERS];
config_info  *Config;
board_type   *Board;
#if USE_LONGJMP
jmp_buf	      saved_environment;
#endif

/** File local prototypes **/
static void set_windows (void);
static int process_event (XEvent event, int player);
static void server_init(void);
static void client_init(void);
static void server_fn(void *foo);
static void client_fn(void *foo);

/** File local variables **/
static fd_set disp_fds;
static pthread_mutex_t board_mutex;
static server_info *ServerInfo;
static int do_threading=FALSE;

/******************************************************************************
  main (argc,argv)

  Orchestrates the whole shebang.
******************************************************************************/
int
main (int argc, char *argv[])
{ 
  pthread_t server_thread, client_thread;

  int do_client, do_server, do_sound, side;

  /** Initialize default values **/

  init_defaults();

  /** Parse the command line **/

  if (load_options (argc, argv)==0)
  {
    /** Determine if we are running in client/server mode **/

    do_server = FALSE;
    for (side=0; side<Config->side_count; side++)
	if (Config->value_int[OPTION_IS_SERVER][side])
            do_server = TRUE;
    do_client = !!Config->server_port;
    do_threading = do_server || do_client;
	do_sound = 1; /*Config->enable_all[OPTION_SOUND];*/

	/** Initialize soundsystem if used **/
	if(do_sound)
	  snd_init();

    /** Initialize the entire board **/
    init_board ();

    /** Setup network **/

    if (do_server)
      server_init();
    if (do_client)
      client_init();

    /** Set up global XWindows[] **/

    set_windows ();

    /** Edit the board, if edit enabled **/

    if (Config->enable_all[OPTION_EDIT])
      edit_board ();

    /** Replay a game, if replay enabled **/

    if (Config->enable_all[OPTION_REPLAY])
      replay_game (Config->fp);

    /** Set up some error handler stuff **/

    XSetErrorHandler (my_error_handler);
    XSetIOErrorHandler (my_io_error_handler);

    /** Start networking threads **/

    if (do_threading)
    {
      pthread_mutex_init(&board_mutex,NULL);
      if (do_server)
        pthread_create(&server_thread,NULL,(void*)&server_fn,(void*)NULL);
      if (do_client)
        pthread_create(&client_thread,NULL,(void*)&client_fn,(void*)NULL); 
    }

    /** Run the appropriate event loop **/

    run_unix_loop ();

    /** Game is over, call cleanup functions **/

    free_board();
    free_options();
	if(do_sound) 
	  snd_close();
  }

  free_config();
  return 0;
}

  
/******************************************************************************
  run_unix_loop (do_threading)

  Controls the event loop which is the basis of xbattle, handling X events
  and board updates as necessary.  Assumes the host actually is running Unix,
  using several important timing functions and error recovery schemes.
******************************************************************************/
void
run_unix_loop (void)
{
  int player,
      update_timer;

  XEvent event;

  int fd,
      maxfd,
      selectback;

  unsigned long new_time,
                old_time,
                extra_time,
                target_time,
                running_time;

  fd_set rfds;

  struct timeval tv_timeout,
                 tv_current,
                 tv_timer_base;

  struct timezone tz_timezone;

  /** Set up time between updates **/

  extra_time = (Config->delay/10000)*1000000 + (Config->delay%10000)*100;

  /** Set up FD stuff which we'll use for generation of timing signals **/
  
  maxfd = 0;
  FD_ZERO (&disp_fds);

  for (player=0; player<Config->player_count; player++)
  {
    if (XWindow[player]->open)
    {
      if ((fd = ConnectionNumber (XWindow[player]->display)) > maxfd)
        maxfd = fd;
      FD_SET (fd, &disp_fds); 
    }
  }  

#if USE_TIMER
  running_time = 0;
  update_timer = FALSE;
  gettimeofday (&tv_timer_base, &tz_timezone);
#endif

  /** Establish baseline time **/

  gettimeofday (&tv_current, &tz_timezone);
  old_time = (tv_current.tv_sec%10000)*1000000 + tv_current.tv_usec;
  target_time = old_time + extra_time;

#if USE_LONGJMP
  setjmp (saved_environment);
#endif

  /** Loop forever **/

  if (do_threading)
    pthread_mutex_lock(&board_mutex);
  while (TRUE)
  {
    /** Initialize a timeout event and wait for any event **/

    rfds = disp_fds;
    tv_timeout.tv_sec = (target_time-old_time)/1000000;
    tv_timeout.tv_usec = (target_time-old_time)%1000000;

    /** We simply assume that we will need the lock regardless of what kind of event was received **/
    if (do_threading)
      pthread_mutex_unlock(&board_mutex);
    selectback = select (maxfd+1, &rfds, NULL, NULL, &tv_timeout);
    if (do_threading)
      pthread_mutex_lock(&board_mutex);	

    /** Mark the time when event received **/

    gettimeofday (&tv_current, &tz_timezone);
    new_time = (tv_current.tv_sec%10000)*1000000 + tv_current.tv_usec;

    if (new_time < old_time)
      target_time = new_time - 1;

    /** Coarsely process the event to determine what type it is **/

    switch (selectback)
    {
      case -1: /* FIXME: EGAIN, EWOULDBLOCK, etc. */
        perror ("select()");
        return;

      /** A real X event **/ /* FIXME: No, this means a timeout without any events */

      case 0:

#if USE_TIMER
        if (tv_current.tv_sec - tv_timer_base.tv_sec != running_time)
        {
          running_time = tv_current.tv_sec - tv_timer_base.tv_sec;
          update_timer = TRUE;
        }
        else
          update_timer = FALSE;
#endif
        /** For each player with open window **/

        for (player=0; player<Config->player_count; player++)
        {
          if (XWindow[player]->open)
          {
#if USE_TIMER
            if (update_timer)
              draw_timer (running_time, player);
#endif
            /** Process all queued X events **/

            XFlush (XWindow[player]->display);
            while (XWindow[player]->open &&
		(XEventsQueued (XWindow[player]->display,QueuedAfterReading)>0))
            {
	      XNextEvent (XWindow[player]->display, &event);
	      if (process_event (event, player)==1)
		return;
            }

#if USE_MULTIFLUSH
            if (XWindow[player]->open)
              XFlush (XWindow[player]->display);
#endif
          }
        }
        break;

      /** Some type of other select() event? **/ /** No, this means one or more events **/

      default:

        /** For each player with open window **/

        for (player=0; player<Config->player_count; player++)
        {
          if (XWindow[player]->open)
          {
            if (FD_ISSET((fd=ConnectionNumber(XWindow[player]->display)),
				&rfds))
            {
              while (XWindow[player]->open &&
				(XEventsQueued (XWindow[player]->display,
				QueuedAfterReading)>0))
              {
	        XNextEvent (XWindow[player]->display, &event);
	        if (process_event (event, player)==1)
		  return;
              }

#if USE_MULTIFLUSH
              XFlush (XWindow[player]->display);
#endif

              if (!XWindow[player]->open)
                FD_CLR (fd, &disp_fds);
            }
          }
        }
        break;
    }
	
	/*
	server_fn(NULL);
	client_fn(NULL);
	*/

	/** Set new target time for timeout **/
	
	if (new_time > target_time) {
	  if (!Config->enable_all[OPTION_STEP])
		update_board ();
	  target_time = new_time + extra_time;
	}
    old_time = new_time;  
	
#if USE_TIMER
    if (update_timer)
      update_timer = FALSE;
#endif
  }
}


/******************************************************************************
  process_event (event, player)

  Handles each <event> from <player>, regardless of type.
  Returns 1 if program should exit, otherwise 0.
******************************************************************************/
static int
process_event (XEvent event, int player)
{
  int i, j,
      side,
      textcount,
      tdir[MAX_DIRECTIONS],
      xlow, xhigh, ylow, yhigh,
      control,
      is_shifted;

  char text[10];

  KeySym key;

  Atom wm_protocols, wm_delete_window;

  cell_type *cell;

  is_shifted = FALSE;
  side = Config->player_to_side[player];
  
  /** Process event **/

  switch (event.type)
  {
    case GraphicsExpose:
    case NoExpose:
      break;

    /** Screen has been exposed **/

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
    
    /** Mouse button has been pressed **/

    case ButtonPress:

      if (XWindow[player]->watch)
        break;

#if USE_PAUSE
      if (Config->is_paused)
        break;
#endif
      
      switch (event.xbutton.button)
      {
        /** Left or middle button **/

        case Button1:
        case Button2:
          is_shifted = event.xbutton.state & ShiftMask;

          /** If shift down, run march command **/

          if (Config->enable[OPTION_MARCH][side] && is_shifted)
          {
            cell = get_cell (event.xbutton.x, event.xbutton.y, tdir,
				side, is_shifted);

            if (cell == NULL)
              break;
				
            run_march (cell, player, side, cell->x, cell->y,
				event.xbutton.button, tdir);
            break;
          }

          /** Get cell and direction vector array **/

          cell = get_cell (event.xbutton.x, event.xbutton.y,
		Config->dir[player], side, is_shifted);

          if (cell == NULL)
            break;

          /** Set corner coords if bound enabled **/

          if (Config->enable[OPTION_BOUND][side])
          {
            Config->old_x[player] = cell->x;
            Config->old_y[player] = cell->y;
            break;
          }
        
          /** Install direction vectors if appropriate **/

          if (cell->side == side ||
		(!Config->enable[OPTION_DISRUPT][side] &&
		cell->side==SIDE_FIGHT &&
		cell->old_side == side))
          {
#if USE_ALT_MOUSE
            if (event.xbutton.button == Button1)
            {
	      set_move_on (cell, Config->dir[player], 0);
              Config->dir_type[player] = MOVE_SET;
            }
            else
            {
	      set_move_off (cell, Config->dir[player], 0);
              Config->dir_type[player] = MOVE_FORCE;
            }
#else
            if (event.xbutton.button == Button1)
            {
	      set_move (cell, Config->dir[player], 0);
              Config->dir_type[player] = MOVE_SET;
            }
            else
            {
	      set_move_force (cell, Config->dir[player], 0);
              Config->dir_type[player] = MOVE_FORCE;
            }
#endif
            Config->dir_factor[player] =
		Board->shapes[side][cell->shape_index]->direction_factor;

            /** Draw the cell to each display **/

            draw_multiple_cell (cell);
          }

          /** If cell had a march command, kill it **/

          if (Config->enable[OPTION_MARCH][side])
            if (cell->march[side] || cell->any_march)
            {
              if (cell->any_march == MARCH_ACTIVE &&
			cell->march_side == side)
              {
                cell->any_march = FALSE;
                cell->march[side] = FALSE;
              }
              else
              {
                cell->march[side] = FALSE;
                cell->outdated = OUTDATE_ALL;
                draw_multiple_cell (cell);
              }
            }

          break;

        /** Right button **/
  
        case Button3:

          /** If artillery, paratroops, or repeat is enabled **/
  
          if (Config->enable[OPTION_ARTILLERY][side] ||
			Config->enable[OPTION_PARATROOPS][side] ||
			Config->enable[OPTION_REPEAT][side])
          {
            /** Get cell and direction vector array **/

            cell = get_cell (event.xbutton.x, event.xbutton.y, tdir,
			side, is_shifted);
            if (cell == NULL)
              break;

            /** If side occupies cell **/

            if (cell->side == side)
            {
              is_shifted = event.xbutton.state & ShiftMask;
              control = event.xbutton.state & ControlMask;

              /** If shift down, run paratroops **/

              if (is_shifted && Config->enable[OPTION_PARATROOPS][side])
                run_shoot (cell, player, event.xbutton.x, event.xbutton.y,
					TRUE, FALSE);
              else if (control && Config->enable[OPTION_ARTILLERY][side])
              {
                /** Else if control down, run paratroops **/

                run_shoot (cell, player, event.xbutton.x, event.xbutton.y,
					TRUE, TRUE);
              }
              else if (Config->enable[OPTION_REPEAT][side]) 
              {
                /** Else repeat last direction command **/

#if USE_ALT_MOUSE
                if (Config->dir_type[player] == MOVE_SET)
	          set_move_on (cell, Config->dir[player],
					Config->dir_factor[player]);
                else
	          set_move_off (cell, Config->dir[player],
					Config->dir_factor[player]);
#else
                if (Config->dir_type[player] == MOVE_SET)
	          set_move (cell, Config->dir[player],
					Config->dir_factor[player]);
                else
	          set_move_force (cell, Config->dir[player],
					Config->dir_factor[player]);
#endif
                /** Draw the cell to each display **/

                draw_multiple_cell (cell);
              }
            }
          }
          break;
      }
      break;

    /** Mouse button has been released **/

    case ButtonRelease:

      if (!Config->enable[OPTION_BOUND][side])
        break;

      if ((Config->old_x[player] == -1) && (Config->old_y[player] == -1))
        break;

      /** Get cell and direction vector array **/

      cell = get_cell (event.xbutton.x, event.xbutton.y, tdir,
			side, is_shifted);

      if (cell == NULL)
        break;

      /** Rectify grid indices **/

      if (cell->y >= Config->board_y_size)
        cell->y = Config->board_y_size-1;
      if (cell->x >= Config->board_x_size)
        cell->x = Config->board_x_size-1;

      /** Swap corners if not left->right, top->bottom **/

      if (Config->old_x[player] < cell->x)
      {
        xlow = Config->old_x[player]; 
        xhigh = cell->x;
      }
      else
      {
        xlow = cell->x;
        xhigh = Config->old_x[player]; 
      }

      if (Config->old_y[player] < cell->y)
      {
        ylow = Config->old_y[player]; 
        yhigh = cell->y;
      }
      else
      {
        ylow = cell->y;
        yhigh = Config->old_y[player]; 
      }

      switch (event.xbutton.button)
      {
        /** Left or middle button **/

        case Button1:
        case Button2:

          /** For every cell in bounded range **/

          for (i=xlow; i<=xhigh; i++)
          {
            for (j=ylow; j<=yhigh; j++)
            {
              cell = CELL2(i,j);

              /** Install direction vectors when appropriate **/

              if (cell->side == side ||
                     (!Config->enable[OPTION_DISRUPT][side] &&
				cell->side==SIDE_FIGHT &&
				cell->old_side == side))
              {
#if USE_ALT_MOUSE
                if (event.xbutton.button == Button1)
	          set_move_on (cell, Config->dir[player], 0);
                else
	          set_move_off (cell, Config->dir[player], 0);
#else
                if (event.xbutton.button == Button1)
	          set_move (cell, Config->dir[player], 0);
                else
	          set_move_force (cell, Config->dir[player], 0);
#endif

                draw_multiple_cell (cell);

              }
            }
          }

          break;

        case Button3:
          break;
      }
      break;
  
    /** Keyboard key has been released **/

    case KeyPress:

      textcount = XLookupString (&event.xkey, text, sizeof(text), &key, NULL);
      if (textcount != 0)
      {
        /** If keypress not within playing board or if message command used **/

        if (event.xkey.x >= XWindow[player]->size_play.x ||
        	event.xkey.y >= XWindow[player]->size_play.y ||
		Config->in_message[player])
        {
          /** Else mouse not in playing board, treat keypress as message **/

          if (draw_message (text,textcount,side,player)==1)
	    return 1;
	  break;
        }
        
        /** Get cell and direction vector array **/

        cell = get_cell (event.xkey.x, event.xkey.y,
			Config->dir[player], side, is_shifted);

        if (cell == NULL)
          break;

	/** Observers can't do anything **/

        if (XWindow[player]->watch)
          break;

        switch (text[0]) /* FIXME: what about multiple chars? */
        {
          /** Set a keyboard direction command **/

          case KEY_DIR_0:
          case KEY_DIR_1:
          case KEY_DIR_2:
          case KEY_DIR_3:
          case KEY_DIR_4:
          case KEY_DIR_5:

            if (cell->side == side)
            {
              for (i=0; i<Config->direction_count; i++)
                Config->dir[player][i] = 0;

              switch (text[0])
              {
                case KEY_DIR_0:
                  Config->dir[player][0] = 1;
                  break;
                case KEY_DIR_1:
                  Config->dir[player][1] = 1;
                  break;
                case KEY_DIR_2:
                  Config->dir[player][2] = 1;
                  break;
                case KEY_DIR_3:
                  Config->dir[player][3] = 1;
                  break;
                case KEY_DIR_4:
                  Config->dir[player][4] = 1;
                  break;
                case KEY_DIR_5:
                  Config->dir[player][5] = 1;
                  break;
              }

             /** ALTER: need way to choose force/set **/

              set_move_force (cell, Config->dir[player], 0);
              draw_multiple_cell (cell);
            }

            break;

#if USE_PAUSE
          /** Pause play **/

          case KEY_PAUSE:

            Config->is_paused = TRUE;
            break;

          /** Resume play **/

          case KEY_QUIT:

            Config->is_paused = FALSE;
            break;
#endif

          /** Attack cell **/

          case KEY_ATTACK:

            run_attack (cell, side);
            break;

          /** Zero out cell's direction vectors **/

          case KEY_ZERO:

            if (cell->side == side)
              run_zero (cell);
            break;

          /** Zero out cell's management command **/

          case KEY_UNMANAGE:

            if (cell->side == side)
              cell->manage_update = FALSE;
            break;

          /** Send out some paratroops **/

          case KEY_PARATROOPS:

            if (!Config->enable[OPTION_PARATROOPS][side])
              break;
            cell = get_cell (event.xkey.x, event.xkey.y, tdir,
				side, is_shifted);
            if (cell->side == side) {
			  snd_para();
              run_shoot (cell, player, event.xkey.x, event.xkey.y,
						 TRUE, FALSE);
			}
            break;

          /** Manage some paratroops **/

          case KEY_PARATROOPS_MANAGE:

            if (!Config->enable[OPTION_PARATROOPS][side])
              break;
            if (!Config->enable[OPTION_MANAGE][side])
              break;
            cell = get_cell (event.xkey.x, event.xkey.y, tdir,
				side, is_shifted);

            if (cell->side == side)
            {
              if (cell->manage_update == MANAGE_PARATROOP)
              {
                cell->manage_update = FALSE;
              }
              else
              {
                run_shoot (cell, player, event.xkey.x, event.xkey.y,
				TRUE, FALSE);
                cell->manage_dir = -1;
                cell->manage_update = MANAGE_PARATROOP;
              }
            }
            break;

          /** Shoot an artillery shell **/

          case KEY_ARTILLERY:

            if (!Config->enable[OPTION_ARTILLERY][side])
              break;
            cell = get_cell (event.xkey.x, event.xkey.y, tdir,
				side, is_shifted);
            if (cell->side == side){
			  snd_gun();
              run_shoot (cell, player, event.xkey.x, event.xkey.y,
						 TRUE, TRUE);
			}
            break;

          /** Manage shoot an artillery shell **/

          case KEY_ARTILLERY_MANAGE:

            if (!Config->enable[OPTION_ARTILLERY][side])
              break;
            if (!Config->enable[OPTION_MANAGE][side])
              break;
            cell = get_cell (event.xkey.x, event.xkey.y, tdir,
				side, is_shifted);
             if (cell->side == side)
            {
              if (cell->manage_update == MANAGE_ARTILLERY)
              {
                cell->manage_update = FALSE;
              }
              else
              {
                run_shoot (cell, player, event.xkey.x, event.xkey.y,
				TRUE, TRUE);
                cell->manage_dir = -1;
                cell->manage_update = MANAGE_ARTILLERY;
              }
            }
            break;

          /** Dig a cell **/

          case KEY_DIG:

            if (Config->enable[OPTION_DIG][side])
              if (cell->side == side && cell->value[cell->side] >
				Config->value_int_all[OPTION_DIG_COST])
                run_dig (cell);
            break;

          /** Manage dig a cell **/

          case KEY_DIG_MANAGE:

            if (!Config->enable[OPTION_DIG][side])
              break;
            if (!Config->enable[OPTION_MANAGE][side])
              break;

            if (cell->side == side)
            {
              if (cell->manage_update == MANAGE_DIG)
                cell->manage_update = FALSE;
              else
              {
                cell->manage_update = MANAGE_DIG;
                run_dig (cell);
              }
            }
            break;

          /** Fill a cell **/

          case KEY_FILL:

            if (Config->enable[OPTION_FILL][side])
              if (cell->side == side && cell->value[cell->side] >
				Config->value_int_all[OPTION_FILL_COST] &&
				cell->move <= 1)
                run_fill (cell);
            break;

          /** Manage fill a cell **/

          case KEY_FILL_MANAGE:

            if (!Config->enable[OPTION_FILL][side])
              break;
            if (!Config->enable[OPTION_MANAGE][side])
              break;

            if (cell->side == side)
            {
              if (cell->manage_update == MANAGE_FILL)
                cell->manage_update = FALSE;
              else
              {
                cell->manage_update = MANAGE_FILL;
                run_fill (cell);
              }
            }
            break;

          /** Scuttle a city **/

          case KEY_SCUTTLE:

            if (Config->enable[OPTION_SCUTTLE][side])
              if (cell->side == side)
                run_scuttle (cell);
            break;

          /** Build a city **/
  
          case KEY_BUILD:

            if (Config->enable[OPTION_BUILD][side])
              if (cell->side == side && cell->angle < ANGLE_FULL)
                run_build (cell, side);
            break;

          /** Manage build a city **/

          case KEY_BUILD_MANAGE:

            if (!Config->enable[OPTION_MANAGE][side])
              break;
            if (Config->enable[OPTION_BUILD][side])
              if (cell->side == side)
              {
                if (cell->manage_update == MANAGE_CONSTRUCTION)
                  cell->manage_update = FALSE;
                else
                {
                  cell->manage_dir = -1;
                  cell->manage_update = MANAGE_CONSTRUCTION;
                }
              }
            break;

          /** Reserve troops in a cell **/

          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':

            if (Config->enable[OPTION_RESERVE][side])
              if (cell->side == side)
                run_reserve (cell, player, side, text);
            break;

          case KEY_STEP:

            if (Config->enable_all[OPTION_STEP])
              update_board ();
            break;
  
          case KEY_MESSAGE:

            Config->in_message[player] = !Config->in_message[player];
            break;
  
          default:
            break;
        }
      }
      break;

    /** Handle the case of an untimely closed window **/

    case ClientMessage:

      wm_protocols = XInternAtom (event.xclient.display, "WM_PROTOCOLS", False);
      wm_delete_window =
		XInternAtom (event.xclient.display, "WM_DELETE_WINDOW", False);
      if ((event.xclient.message_type == wm_protocols) && 
		(event.xclient.data.l[0] == wm_delete_window))
        if (remove_player (player)==1)
	  return 1;
      break;
  }

  return 0;
}



/******************************************************************************
  cell_type *
  get_cell (win_x, win_y, dir, cell_size, shift)

  Gets the grid cell corresponding to the window position (<winx>,<winy>).
  Also determine what type of direction vectors should be activated if
  <dir> is not NULL.  Returns pointer to cell.
******************************************************************************/
cell_type *
get_cell (int win_x, int win_y, int dir[MAX_DIRECTIONS], int side, int shift)
{
  int i,
      board_x, board_y,
      select_x, select_y,
      off_x, off_y;

  shape_type *shape;
  cell_type *cell;
  select_type *select;

  /** Set selection chart **/

  select = Config->selects[side];

  /** Determine which selection block coordinate falls in **/

  board_x = (win_x /
	(select->dimension.x - select->offset.x)) * select->multiplier.x;
  board_y = (win_y /
	(select->dimension.y - select->offset.x)) * select->multiplier.y;

  /** Determine offset into selection chart **/

  select_x = win_x % (select->dimension.x - select->offset.x);
  select_y = win_y % (select->dimension.y - select->offset.x);

  /** Determine cell index offset from selection chart **/

  board_x += select->matrix[select_y][select_x].x;
  board_y += select->matrix[select_y][select_x].y;

  /** If invalid cell, return NULL **/

  if (board_x < 0 || board_y < 0 ||
	board_x >= Config->board_x_size || board_y >= Config->board_y_size)
    return (NULL);

  /** Set cell and its shape **/

  cell = CELL2 (board_x, board_y);
  shape = Board->shapes[side][cell->shape_index];

  /** Determine offset of coordinate from upper left corner of cell **/

  off_x = win_x - cell->x_center[side] + shape->center_bound.x;
  off_y = win_y - cell->y_center[side] + shape->center_bound.y;

  /** If routine should copy direction vectors **/

  if (dir != NULL)
  {
    /** Zero old direction vectors **/

    for (i=0; i<Config->direction_count; i++)
      dir[i] = 0;

    /** If direction chart indicates a valid vector **/

    if (shape->chart[off_x][off_y][0] >= 0)
    {
      dir[shape->chart[off_x][off_y][0]] = 1;

      /** If secondary vector should be set **/

      if ((shift || shape->use_secondary) && shape->chart[off_x][off_y][1] >= 0)
        dir[shape->chart[off_x][off_y][1]] = 1;
    }
  }

  return (cell);
}



/******************************************************************************
  set_windows ()

  Sets up each of the global XWindow[] structures, initializing the necessary
  variables and actually opening the associate windows.
******************************************************************************/
static void
set_windows (void)
{
  int i,
      player,
      side;

  char hue_title[MAX_LINE],
       bw_title[MAX_LINE];

  xwindow_type *xwindow;

  /** For each player **/

  for (player=0; player<Config->player_count; player++)
  {
    xwindow = XWindow[player];

    xwindow->player = player;

    /** Set offset to playing area of window **/

    xwindow->offset_play.x = 0;
    xwindow->offset_play.y = 0;

    /** Set playing area size **/

    side = Config->player_to_side[player];
    xwindow->size_play.x = Board->size[side].x;
    xwindow->size_play.y = Board->size[side].y;

    xwindow->offset_text.x = 0;
    xwindow->offset_text.y = 2*xwindow->offset_play.y + xwindow->size_play.y;

    xwindow->size_window.x = 2*xwindow->offset_play.x + xwindow->size_play.x;
    xwindow->size_text.x = xwindow->size_window.x;

    /** Set position for text messages **/

#if USE_MULTITEXT
    for (side=0; side<Config->side_count; side++)
      xwindow->text_y_pos[side] = xwindow->offset_text.y + 12 + 16*side;

    xwindow->size_text.y = Config->side_count*Config->text_size;
#else
    xwindow->text_y_pos[0] = xwindow->offset_text.y + 12;
    xwindow->text_y_pos[1] = xwindow->offset_text.y + 28;

    xwindow->size_text.y = 2*Config->text_size;
#endif

    xwindow->size_window.y = xwindow->offset_text.y + xwindow->size_text.y;

    /** Set title at top of window **/

    side = Config->player_to_side[player];

    if (strcmp (Config->side_to_bw_name[side], Config->side_to_hue_name[side]))
    {
      sprintf (hue_title, "xbattle: %s (%s) side",
		Config->side_to_bw_name[side], Config->side_to_hue_name[side]);
      sprintf (bw_title, "xbattle: %s (%s) side",
		Config->side_to_hue_name[side], Config->side_to_bw_name[side]);
    }
    else
    {
      sprintf (hue_title, "xbattle: %s side", Config->side_to_hue_name[side]);
      sprintf (bw_title, "xbattle: %s side", Config->side_to_bw_name[side]);
    }

    if (Config->side_to_letter[side][0])
      strcpy (hue_title, bw_title);

    /** Open the window **/
    
    open_xwindow (xwindow, hue_title, bw_title);

    /** If window is b&w and using any form of terrain, must disallow	**/
    /** most ways of drawing since terrain hues are not defined.	**/

#ifdef WITH_HILLS_AND_FOREST
    if (xwindow->depth == 1 &&
        (Config->level_max > 0 || Config->level_min < 0 ||
         Config->forest_level_max >0))
#else
    if (xwindow->depth == 1 &&
        (Config->level_max > 0 || Config->level_min < 0))
#endif
    {
      if (Config->value_int[OPTION_DRAW][side] != DRAW_PIXMAP)
      {
        Config->value_int[OPTION_DRAW][side] = DRAW_MASKING;
        for (i=0; i<Board->shape_count; i++)
          shape_set_draw_method (Board->shapes[side][i], side, FALSE);
      }
    }
  }
}



/******************************************************************************
  remove_player (current_player)

  Completely eliminates <current_player> from the game.  This basically just
  involves freeing up all the allocated resources and setting a flag to let
  the system know not to send any more drawing commands to this display.
  Returns 1 if program should exit, otherwise 0.
******************************************************************************/
int
remove_player (int current_player)
{
  int side,
      player,
      done,
      fd;

  char line[512],
       end_signal;

  XEvent event;

  if (XWindow[current_player]->open == FALSE)
    return 0;

  /** Flush the remainder of the player's events **/

  while (XEventsQueued (XWindow[current_player]->display,
		QueuedAfterReading) > 0)
  {
    XNextEvent (XWindow[current_player]->display, &event);
    process_event (event, current_player);
  }

  fd = ConnectionNumber (XWindow[current_player]->display);

  /** Set flags indicating player's absence **/

  XWindow[current_player]->open = FALSE;
  done = TRUE;
  for (player=0; player<Config->player_count; player++)
    if (XWindow[player]->open && !XWindow[player]->watch)
      done = FALSE;

  close_xwindow (XWindow[current_player]);

  /** If there are no more players remaining **/

  if (done)
  {
    /** Could be some people watching, so kill them **/

    for (player=0; player<Config->player_count; player++)
    {
      if (XWindow[player]->open)
        remove_player (player);
    }

    /** Send end of game signal if storing game **/

    if (Config->enable_all[OPTION_STORE] || Config->enable_all[OPTION_REPLAY])
    {
      if (Config->enable_all[OPTION_STORE])
      {
        end_signal = REPLAY_TERMINATE;
        fwrite (&end_signal, sizeof(char), 1, Config->fp);
      }
      fclose (Config->fp);
    }

    /** Tell main to exit **/

    return 1;
  }
  else
  {
    /** Else there are still players, show quit message on displays **/

    side = Config->player_to_side[current_player];
    sprintf (line, "%s has quit the game", Config->side_to_hue_name[side]);
    draw_message (line, strlen(line), side, current_player);

    /** Ring bells of remaining players **/

    for (player=0; player<Config->player_count; player++)
      if (XWindow[player]->open)
        XBell (XWindow[player]->display, 100);
  }

  FD_CLR (fd, &disp_fds);
  return 0;
}


/*************************************************************
  server_init()

  Setup all network connections if acting as a server
**************************************************************/
static void server_init(void)
{
  int side,socket_fd,client_addr_len,len;
  struct sockaddr_in addr;
  struct sockaddr client_addr;
  FILE *fp;
  char *boarddata;
  unsigned char headers[5];
  char tmpfile[256];

  /** Setup some server data **/
  ServerInfo = (server_info*)calloc(1,sizeof(server_info));
  for(side=0,ServerInfo->num_clients=0;side<Config->side_count;side++)
    if(Config->value_int[OPTION_IS_SERVER][side]) {
      ServerInfo->num_clients++;
      fprintf(stderr,"Side %d needs a server\n",side);
    }

  /** If we are acting as a server, dump board information to a file which 
	  we can send to the other players */
  /** FIXME: This is just ugly.  The load.c code should be rewritten so that it  **/
  /** works with a character array   Then load_boad() can fread an array  **/
  /** and then call pack_board().  dump_board can call unpack_boad() and  **/
  /** then fwrite(). (maybe serialize()/unserialize())                    **/
  if(ServerInfo->num_clients) {
    /* FIXME: symlink attack possible, */
    sprintf(tmpfile,"/tmp/xbattleai-server-%lu.tmp",(unsigned long)getpid());
    dump_board(tmpfile,0);
    fp = fopen(tmpfile,"rb");
    if(!fp) {
      fprintf(stderr,"error, could not open %s for reading\n",tmpfile);
      exit(1);
    }
    fseek(fp,0,SEEK_END); len=ftell(fp); fseek(fp,0,SEEK_SET);
    boarddata=malloc(len);
    fread(boarddata,1,len,fp);
    fclose(fp);
    remove(tmpfile);
  }

  /** If we should act as a server for some player, wait for connections **/  
  for(side=0;side<Config->side_count;side++)
    if(Config->value_int[OPTION_IS_SERVER][side]) {
      if((socket_fd = socket(PF_INET,SOCK_STREAM,0))<0) {
	fprintf(stderr,"Failed to create socket for incoming connections\n"); 
	exit(1);
      }

      /* FIXME: use SO_REUSEADDR */
      printf("Binding to port %d for side %d\n",Config->value_int[OPTION_IS_SERVER][side],side);
      bzero(&addr,sizeof(addr));
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = htonl(INADDR_ANY);
      addr.sin_port = htons(Config->value_int[OPTION_IS_SERVER][side]);
      if(bind(socket_fd,(struct sockaddr*) &addr,sizeof(addr)) < 0) {
	fprintf(stderr,"Failed to bind socket to local address on port %d\n",Config->value_int[OPTION_IS_SERVER][side]);
	exit(1);
      }
      if (listen(socket_fd,5) < 0) {
	fprintf(stderr,"Could not listen for connection\n");
	exit(1);
      }

      printf("Waiting for side %d to connect\n",side);
      client_addr_len=sizeof(client_addr);
      ServerInfo->socket[side] = accept(socket_fd,(struct sockaddr*)&client_addr,&client_addr_len);
      if(ServerInfo->socket[side]<0) {
	fprintf(stderr,"could not accept incoming connection\n");
	exit(1);
      }
      close(socket_fd);
      
      /** Send some headers */
      headers[0] = side;
      headers[1] = len&0xff;
      headers[2] = (len>>8)&0xff;
      headers[3] = (len>>16)&0xff;
      headers[4] = (len>>24)&0xff;
      send(ServerInfo->socket[side],(void*)headers,5,0);	      
      /** Now send him the boarddata **/
      send(ServerInfo->socket[side],(void*)boarddata,len,0);	      

      printf("Side %d has succesfully connected\n",side);
    }

  if(ServerInfo->num_clients) {
    printf("All sides have connected\n");
    free(boarddata);
  }
}


/******************************************************
  client_init()

  Setup all network connections if playing as a client
*******************************************************/
static void client_init(void)
{
  int socket_fd,len,len_left,tot_len;
  struct sockaddr_in addr;
  FILE *fp;
  char *boarddata,*pkg;
  unsigned char headers[5];
  struct hostent *host;
  char tmpfile[256];

  printf("Attempting to connect to server %s:%d\n",Config->server_name,Config->server_port);
  bzero(&addr,sizeof(addr));
  addr.sin_family=AF_INET;
  host=gethostbyname(Config->server_name);
  addr.sin_addr.s_addr=*((long*) host->h_addr); 
  addr.sin_port = htons(Config->server_port);
  if((socket_fd=socket(PF_INET,SOCK_STREAM,0))<0) {
    fprintf(stderr,"Failed to create socket\n"); 
    exit(1);
  }
  if(connect(socket_fd,(struct sockaddr*)&addr,sizeof(addr))<0) {
    fprintf(stderr,"Failed to connect to server\n");
    exit(1);
  }    
  Config->socket_fd=socket_fd;

  /** Listen for board data **/
  len=recvfrom(Config->socket_fd,(void*)&headers,5,0,NULL,NULL);    
  Config->player_no = headers[0];
  tot_len=headers[1]+(headers[2]<<8)+(headers[3]<<16)+(headers[4]<<24);
  boarddata=malloc(tot_len);
  pkg=boarddata;
  len_left=tot_len;
  while(len_left) {
    len=recvfrom(Config->socket_fd,(void*)pkg,len_left,0,NULL,NULL);
    len_left -= len;
    pkg += len;
  }

  /* FIXME: symlink attack possible */
  sprintf(tmpfile,"/tmp/xbattleai-client-%lu.tmp",(unsigned long)getpid());
  fp=fopen(tmpfile,"wb");
  if(!fp) {
    fprintf(stderr,"error - could not open %s for writing\n",tmpfile);
    exit(1);
  }
  fwrite(boarddata,1,tot_len,fp);
  fclose(fp);
  free(boarddata);

  load_board(tmpfile,0);
  remove(tmpfile);

  printf("Connection successfull\n");
}


static void server_fn(void *foo)
{
  int i,j,x,y,len_left,len,vec,dir_cnt;
  s_char *board_package,*move_package;
  s_char *pkg;
  cell_type *cell;

  board_package = (s_char*) malloc(sizeof(s_char)*Config->board_x_size*Config->board_y_size*Config->side_count);
  move_package = (s_char*) malloc(sizeof(s_char)*Config->board_x_size*Config->board_y_size);
  
  while(1) {
    /** Act as server if neccessary **/
    for(i=0;i<Config->side_count;i++)
	if(Config->value_int[OPTION_IS_SERVER][i]) {
	  /** Right now this is just a crude prototype updating the complete board every turn */

	  pthread_mutex_lock(&board_mutex);
	  for(y=0;y<Config->board_y_size;y++)
		for(x=0;x<Config->board_x_size;x++) {
		  /* First, cleanup this cell slightly... */
		  cell=Board->cells[y][x];
		  if(cell->side == SIDE_NONE) 
			for(j=0;j<Config->side_count;j++)
			  cell->value[j] = 0;
		  /* Now create the values for this cell */
		  for(j=0;j<Config->side_count;j++)
			board_package[(y+x*Config->board_y_size)*Config->side_count+j] = cell->value[j];
		}
	  pthread_mutex_unlock(&board_mutex);

	  send(ServerInfo->socket[i],(void*)board_package,sizeof(s_char)*Config->board_x_size*Config->board_y_size*Config->side_count,0);	      
	  
	  pkg=move_package;
	  len_left=sizeof(s_char)*Config->board_x_size*Config->board_y_size;
	  while(len_left) {
		len=recvfrom(ServerInfo->socket[i],(void*)pkg,len_left,0,NULL,NULL);
		len_left -= len;
		pkg += len;
	  }

	  pthread_mutex_lock(&board_mutex);
	  for(y=0;y<Config->board_y_size;y++)
		for(x=0;x<Config->board_x_size;x++)
		  if(Board->cells[y][x]->side == i) {
			vec=move_package[y+x*Config->board_y_size]; cell=Board->cells[y][x];
			dir_cnt=Board->shapes[0][cell->shape_index]->direction_count;
			cell->move=0;
			for(j=0;j<dir_cnt;j++)
			  if(vec & (1<<j)) { cell->dir[j] = 1; cell->move++; }
			  else cell->dir[j]=0;
			if(cell->dir[j]) vec |= 1<<j;	      		
		  }
	  pthread_mutex_unlock(&board_mutex);

	}
  }

  free(board_package);
  free(move_package);
  free(ServerInfo);
}


static void client_fn(void *foo)
{
  int j,x,y,len_left,len,vec,dir_cnt;
  s_char *board_package,*move_package;
  s_char *pkg;
  cell_type *cell;

  board_package = (s_char*) malloc(sizeof(s_char)*Config->board_x_size*Config->board_y_size*Config->side_count);
  move_package = (s_char*) malloc(sizeof(s_char)*Config->board_x_size*Config->board_y_size);

  while(1) {

    /** Now, act as client if neccessary */
    if(Config->server_port) {
	pkg=board_package;
	len_left=sizeof(s_char)*Config->board_x_size*Config->board_y_size*Config->side_count;
	while(len_left) {
	  len=recvfrom(Config->socket_fd,(void*)pkg,len_left,0,NULL,NULL);
	  len_left -= len;
	  pkg += len;
	}

	pthread_mutex_lock(&board_mutex);

	for(y=0;y<Config->board_y_size;y++)
	  for(x=0;x<Config->board_x_size;x++) {
		cell=Board->cells[y][x];
		cell->side=SIDE_NONE;
		for(j=0;j<Config->side_count;j++) {
		  cell->value[j] = board_package[(y+x*Config->board_y_size)*Config->side_count+j];
		  if(cell->value[j] > 0) cell->side = (cell->side==SIDE_NONE?j:SIDE_FIGHT);
		}
	  }	
	for(y=0;y<Config->board_y_size;y++)
	  for(x=0;x<Config->board_x_size;x++) {
		vec=0; cell=Board->cells[y][x];
		dir_cnt=Board->shapes[0][cell->shape_index]->direction_count;
		for(j=0;j<dir_cnt;j++)
		  if(cell->dir[j]) vec |= 1<<j;	      
		move_package[y+x*Config->board_y_size] = vec;
	  }

	pthread_mutex_unlock(&board_mutex);

	send(Config->socket_fd,(void*)move_package,sizeof(s_char)*Config->board_x_size*Config->board_y_size,0);
    }
  }

  free(move_package);
  free(board_package);
}

