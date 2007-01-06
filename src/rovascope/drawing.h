#include <glib.h>

#ifndef PN_DRAWING_H
#define PN_DRAWING_H

void pn_draw_dot (guint x, guint y, guchar value);
void pn_draw_line (guint _x0, guint _y0, guint _x1, guint _y1, guchar value);

#endif
