#include "paranormal.h"
#include "actuators.h"
#include "pn_utils.h"

extern SDL_Surface *screen;

void
pn_draw_line (guint _x0, guint _y0, guint _x1, guint _y1, guchar value)
{
  gint x0 = _x0;
  gint y0 = _y0;
  gint x1 = _x1;
  gint y1 = _y1;

  gint dy = y1 - y0;
  gint dx = x1 - x0;
  gint stepx, stepy;
  gint fraction;

  if (dy < 0)
    {
      dy = -dy;
      stepy = -(screen->pitch >> 2);
    }
  else
    {
      stepy = screen->pitch>>2;
    }
  if (dx < 0)
    {
      dx = -dx;
      stepx = -1;
    }
  else
    {
      stepx = 1;
    }
  dy <<= 1;
  dx <<= 1;

  y0 *= screen->pitch>>2;
  y1 *= screen->pitch>>2;
  pn_image_data->surface[0][PN_IMG_INDEX(x0, y0)] = value;
  if (dx > dy)
    {
      fraction = dy - (dx >> 1);
      while (x0 != x1)
	{
	  if (fraction >= 0)
	    {
	      y0 += stepy;
	      fraction -= dx;
	    }
	  x0 += stepx;
	  fraction += dy;
	  pn_image_data->surface[0][PN_IMG_INDEX(x0, y0)] = value;
	}
    }
  else
    {
      fraction = dx - (dy >> 1);
      while (y0 != y1)
	{
	  if (fraction >= 0)
	    {
	      x0 += stepx;
	      fraction -= dy;
	    }
	  y0 += stepy;
	  fraction += dx;
	  pn_image_data->surface[0][PN_IMG_INDEX(x0, y0)] = value;
	}
    }
}
