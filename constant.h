#ifndef INCLUDED_CONSTANT_H
#define INCLUDED_CONSTANT_H

#ifndef TRUE
# define TRUE				1
#endif
#ifndef FALSE
# define FALSE				0
#endif

#define CONST_PI			3.14159265358979323846
#define CONST_2DSQ2			1.41421356237309504880
#define CONST_SQ2D2			0.70710678118654752440
#define CONST_SQ3D2			0.86602540378443864676
#define CONST_2DSQ3			1.15470053837925152902
#define CONST_SQ3D6			0.28867513459481288225

#define USE_INVERT			TRUE
#define USE_STRIPE			FALSE
#define USE_PAUSE			TRUE
#define USE_ALT_MOUSE			FALSE
#define USE_MULTITEXT			TRUE
#define USE_TIMER			TRUE
#define USE_MULTIFLUSH			FALSE
#define USE_HOME_ENV                    FALSE
#define USE_FATAL_RECOVER		FALSE
/* FIXME: what is the bitwise and trying to do here? */
#define USE_LONGJMP			(TRUE & USE_FATAL_RECOVER)
#define USE_PROCEDURE			(!USE_LONGJMP & USE_FATAL_RECOVER)

/* FIXME: some of these can be calculated from others */
#define MAX_BOARDSIZE			128
#define MAX_CELLSIZE			128
#define MAX_PLAYERS			11
#define MAX_SIDES			11
#define MAX_HUES			30
#define MAX_BWS				MAX_HUES
#define MAX_HILL_TONES			9
#define MAX_FOREST_TONES		9
#define MAX_SEA_TONES			9
#define MAX_SHAPES			3
#define MAX_VICTORY_POSITIONS		10

#define MAX_DIRECTIONS			8
#define MAX_LINE			100
#define MAX_TEXT			2048
#define MAX_NAME			100

#define MAX_MAXVAL			64  /* FIXME: 50 wasn't enough, see BUG in shape.c. How to compute max? */

#define MAX_POINTS			12

#define MAX_FARMS			50

#define SIDE_VALID_LIMIT		MAX_SIDES
#define SIDE_FIGHT			(MAX_SIDES+1)
#define SIDE_NONE			(MAX_SIDES+2)
#define SIDE_VOID			(MAX_SIDES+3)

#define REDRAW_NORMAL			0
#define REDRAW_FULL			1
#define REDRAW_COPY			2
#define REDRAW_BLANK			3

#define LOOP_LIMIT			200

#define REPLAY_UPDATE			1

#define REPLAY_TERMINATE		127

#define HILLS_DIVISOR			11.0

#define DIFFUSE_MIN_LEVEL		0.25
#define DIFFUSE_MAX_LEVEL		0.75
#define DIFFUSE_MEAN_LEVEL		0.50
#define DIFFUSE_SPAN			0.50
#define DIFFUSE_QUASI_FACTOR		0.0005
#define DIFFUSE_QUASI_THRESHOLD		5
#define DIFFUSE_BUFFER			20
#define DIFFUSE_BUFFER_HALF		10

#define DIFFUSE_4_SI			0.3
#define DIFFUSE_4_IS			0.3
#define DIFFUSE_4_II			0.2
#define DIFFUSE_4_ID			0.2

#define DIFFUSE_3_SI			0.4
#define DIFFUSE_3_IS			0.4
#define DIFFUSE_3_II			0.2

#define DIFFUSE_2_IS			0.7
#define DIFFUSE_2_ID			0.3

#define DIFFUSE_1_SI			1.0


#define HILL_FACTOR			0.04
#define FOREST_FACTOR			0.2
#define MOVE_FACTOR			0.6

#define TROOP_MIN_SIZE			2
#define TROOP_MIN_BUFFER		4
#define TROOP_MIN_FRACTION		0.05
#define TROOP_MAX_FRACTION		0.90

#define TOWN_MIN_RADIUS			5
#define TOWN_MIN			50
#define TOWN_MAX			100
#define TOWN_MULTIPLIER			4
#define TOWN_MIN_FRACTION		0.2
#define TOWN_MAX_FRACTION		0.85

#define ELEVATION_BINS			200
#define ELEVATION_OFFSET		100
#define MAX_PEAKS			200
#define PEAK_MULTIPLIER			0.025
#define PEAK_X_FACTOR			1.50
#define PEAK_Y_FACTOR			1.00

#define SHELL_FRACTION			0.25
#define SHELL_MULTIPLIER		40

#define CHUTE_FRACTION			0.35
#define CHUTE_MULTIPLIER		40

#define MILITIA_MULTIPLIER		0.0075

#define ARTILLERY_FACTOR		2.0
#define PARATROOPS_FACTOR		2.0

#define ANGLE_FULL			23040
#define ANGLE_HALF			11520
#define ANGLE_ROUND_DOWN		200
#define ANGLE_ROUND_UP			23000

#define SEA_BLOCK_MIN			0.25

#define SCUTTLE_BASIS			10

#define BW_NONE				255
#define HUE_NONE			255

#define DEFAULT_TEXTSIZE		16
#if USE_TIMER
#define DEFAULT_TEXT_X_OFFSET		50
#else
#define DEFAULT_TEXT_X_OFFSET		0
#endif

#define DEFAULT_FILE			"~/.xbattle" /* FIXME: tilde is interpreted by the shell, not the C library */
#define DEFAULT_FONT		"-adobe-courier-bold-r-normal--*-100-*-*-*-*-iso8859-1"
#define DEFAULT_BORDER			1
#define DEFAULT_XPOS			0
#define DEFAULT_YPOS			0

#define DEFAULT_CENTERSIZE		5
#define DEFAULT_MARCHSIZE		7

#define DEFAULT_HUES			17
#define DEFAULT_BWS			11

#ifndef DEFAULT_XBO_DIR
#define DEFAULT_XBO_DIR			"./xbos"
#endif

#ifndef DEFAULT_XBA_DIR
#define DEFAULT_XBA_DIR			"./xbas"
#endif

#ifndef DEFAULT_XBT_DIR
#define DEFAULT_XBT_DIR			"./xbts"
#endif

#ifndef DEFAULT_SND_DIR
#define DEFAULT_SND_DIR			"./xbts"
#endif

#define OUTDATE_ALL			(MAX_HUES + 2)
#define OUTDATE_NONE			-1

#define MARCH_ACTIVE			1
#define MARCH_PASSIVE			2
#define MARCH_TEMP			3
#define MARCH_HALT			-1

#define MOVE_SET			0
#define MOVE_FORCE			1

#define TILE_SQUARE			0
#define TILE_HEX			1
#define TILE_OCTAGON			2
#define TILE_DIAMOND			3
#define TILE_TRIANGLE			4

#define SHAPE_CIRCLE			0
#define SHAPE_SQUARE			1
#define SHAPE_RECTANGLE			2
#define SHAPE_POLYGON			3

#define COPY_NONE			0
#define COPY_PIXMAP			1
#define COPY_WINDOW			2
#define COPY_BACK			3

#define ERASE_DRAW			0
#define ERASE_MASK			1
#define ERASE_NONE			2

#define DRAW_SIMPLE			0
#define DRAW_BACKING			1
#define DRAW_PIXMAP			2
#define DRAW_WINDOW			3
#define DRAW_MASKING			4
#define DRAW_POLYGON			5

#define MANAGE_CONSTRUCTION		1
#define MANAGE_ARTILLERY		2
#define MANAGE_PARATROOP		3
#define MANAGE_FILL			4
#define MANAGE_DIG			5

#define TRI_UP				0
#define TRI_LEFT_DOWN			1
#define TRI_RIGHT_DOWN			2

#define TRI_LEFT_UP			0
#define TRI_DOWN			1
#define TRI_RIGHT_UP			2

#define SQUARE_UP			0
#define SQUARE_LEFT			1
#define SQUARE_DOWN			2
#define SQUARE_RIGHT			3

#define HEX_UP				0
#define HEX_LEFT_UP			1
#define HEX_LEFT_DOWN			2
#define HEX_DOWN			3
#define HEX_RIGHT_DOWN			4
#define HEX_RIGHT_UP			5

#define OCT_UP				0
#define OCT_LEFT_UP			1
#define OCT_LEFT			2
#define OCT_LEFT_DOWN			3
#define OCT_DOWN			4
#define OCT_RIGHT_DOWN			5
#define OCT_RIGHT			6
#define OCT_RIGHT_UP			7


#define BELL_VOLUME			100

#define TIMER_OFFSET			10

/**** ascii codes ****/
/* FIXME: the control characters are probably useless */
#define RETURN				 13
#define BACKSPACE			  8
#define DELETE				127
#define CTRL_E				  5
#define CTRL_C				  3
#define CTRL_Q				 17
#define CTRL_G				  7
#define CTRL_W				 23
#define SPACE				 32
#define CTRL_F				  6
#define CTRL_D				  4
#define CTRL_S				 19
#define CTRL_B				  2
#define CTRL_P				 16

#include "options2.h"

#endif /* INCLUDED_CONSTANT_H */
