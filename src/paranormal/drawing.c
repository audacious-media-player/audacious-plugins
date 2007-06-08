#include <math.h>

#include "paranormal.h"
#include "actuators.h"
#include "pn_utils.h"

void
pn_draw_dot (guint x, guint y, guchar value)
{
  if (x > pn_image_data->width || x < 0 || y > pn_image_data->height || y < 0)
    return;

  pn_image_data->surface[0][PN_IMG_INDEX(x, y)] = value;
}

void
pn_draw_line (guint _x0, guint _y0, guint _x1, guint _y1, guchar value)
{
  gint x0 = _x0;
  gint y0 = _y0;
  gint x1 = _x1;
  gint y1 = _y1;

  gint dx = x1 - x0;
  gint dy = y1 - y0;

  pn_draw_dot(x0, y0, value);

  if (dx != 0)
    {
      gfloat m = (gfloat) dy / (gfloat) dx;
      gfloat b = y0 - m * x0;

      dx = (x1 > x0) ? 1 : - 1;
      while (x0 != x1)
        {
          x0 += dx;
          y0 = m * x0 + b;

          pn_draw_dot(x0, y0, value);
        }
    }
}
