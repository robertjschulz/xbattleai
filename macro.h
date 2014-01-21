#ifndef INCLUDED_MACRO_H
#define INCLUDED_MACRO_H

#define CELL(x) (Board->list[(x)])
#define CELL2(x,y) (Board->cells[(y)][(x)])

#define MODX(x) (((x)+Config->board_x_size)%Config->board_x_size)
#define MODY(y) (((y)+Config->board_y_size)%Config->board_y_size)

#endif
