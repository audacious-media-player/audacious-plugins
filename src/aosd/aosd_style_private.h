/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2007
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*
*/

#ifndef _I_AOSD_STYLE_PRIVATE_H
#define _I_AOSD_STYLE_PRIVATE_H 1

#include "aosd_cfg.h"
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include "ghosd.h"


/* decoration render function parameter object
   ----------------------------------------------------------
   layout         pango layout that contains OSD text
   text           user-defined options for OSD text
   decoration     user-defined options for OSD decorations
*/
typedef struct
{
  PangoLayout * layout;
  aosd_cfg_osd_text_t * text;
  aosd_cfg_osd_decoration_t * decoration;
}
aosd_deco_style_data_t;


/* decoration object
   ----------------------------------------------------------
   desc           description
   render_func    render function used to draw the decoration
   colors_num     number of user-definable colors (no more than AOSD_DECO_STYLE_MAX_COLORS)
   padding        drawable space available around the text
*/
typedef struct
{
  const char * desc;
  void (*render_func)( Ghosd * , cairo_t * , aosd_deco_style_data_t * );
  int colors_num;
  struct
  {
    int top;
    int bottom;
    int left;
    int right;
  }
  padding;
}
aosd_deco_style_t;

#endif /* !_I_AOSD_STYLE_PRIVATE_H */
