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

#include "aosd_style.h"
#include "aosd_style_private.h"
#include "aosd_cfg.h"

#include <libaudcore/i18n.h>
#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include "ghosd.h"


/* HOW TO ADD A NEW DECORATION STYLE
   --------------------------------------------------------------------------
   First, add a new decoration style code; then, provide the decoration style
   information by adding a new entry in the aosd_deco_styles[] array (name,
   render function, etc.); add the new decoration style code in the array
   of decoration style codes, and update the define containing the array size.
   The render function uses three parameters; the Ghosd instance, the cairo
   object you use to draw, and a aosd_deco_style_data_t object that contains
   user-defined options (look into aosd_style.h and aosd_cfg.h for details).
   Have fun! :)
*/

/* prototypes of render functions */
static void aosd_deco_rfunc_rect ( Ghosd * , cairo_t * , aosd_deco_style_data_t * );
static void aosd_deco_rfunc_roundrect ( Ghosd * , cairo_t * , aosd_deco_style_data_t * );
static void aosd_deco_rfunc_concaverect ( Ghosd * , cairo_t * , aosd_deco_style_data_t * );
static void aosd_deco_rfunc_none ( Ghosd * , cairo_t * , aosd_deco_style_data_t * );

/* map decoration style codes to decoration objects */
aosd_deco_style_t aosd_deco_styles[] =
{
  // AOSD_DECO_STYLE_RECT
  { N_("Rectangle") ,
    aosd_deco_rfunc_rect ,
    2 , { 10 , 10 , 10 , 10 } },

  // AOSD_DECO_STYLE_ROUNDRECT
  { N_("Rounded Rectangle") ,
    aosd_deco_rfunc_roundrect ,
    2 , { 10 , 10 , 10 , 10 } },

  // AOSD_DECO_STYLE_CONCAVERECT
  { N_("Concave Rectangle") ,
    aosd_deco_rfunc_concaverect ,
    2 , { 10 , 10 , 10 , 10 } },

  // AOSD_DECO_STYLE_NONE
  { N_("None") ,
    aosd_deco_rfunc_none ,
    0 , { 2 , 2 , 2 , 2 } }
};

static_assert (aud::n_elems (aosd_deco_styles) == AOSD_NUM_DECO_STYLES, "update aosd_deco_styles");


/* DECORATION STYLE API */

void
aosd_deco_style_get_padding ( int deco_code ,
                              int * ptop , int * pbottom ,
                              int * pleft , int * pright )
{
  if ( ptop != nullptr ) *ptop = aosd_deco_styles[deco_code].padding.top;
  if ( pbottom != nullptr ) *pbottom = aosd_deco_styles[deco_code].padding.bottom;
  if ( pleft != nullptr ) *pleft = aosd_deco_styles[deco_code].padding.left;
  if ( pright != nullptr ) *pright = aosd_deco_styles[deco_code].padding.right;
  return;
}


const char *
aosd_deco_style_get_desc ( int deco_code )
{
  return aosd_deco_styles[deco_code].desc;
}


int
aosd_deco_style_get_numcol ( int deco_code )
{
  return aosd_deco_styles[deco_code].colors_num;
}


void
aosd_deco_style_render ( int deco_code , void * ghosd , void * cr , void * user_data )
{
  aosd_deco_styles[deco_code].render_func ((Ghosd *) ghosd, (cairo_t *) cr,
   (aosd_deco_style_data_t *) user_data);
}


// sizing helper
static void
aosd_layout_size( PangoLayout * layout , int * width , int * height , int * bearing )
{
  PangoRectangle ink, log;

  pango_layout_get_pixel_extents( layout , &ink , &log );

  if ( width != nullptr )
    *width = ink.width;
  if ( height != nullptr )
    *height = log.height;
  if ( bearing != nullptr )
    *bearing = -ink.x;
}


/* RENDER FUNCTIONS */

static void
aosd_deco_rfunc_rect( Ghosd * osd , cairo_t * cr , aosd_deco_style_data_t * data )
{
  /* decoration information
     ----------------------
     draws a simple rectangular decoration; uses 2 colors
     (user color 1 and 2) and 1 font (user font 1), with optional shadow
  */
  PangoLayout *osd_layout = data->layout;
  aosd_color_t color0 = data->decoration->colors[0];
  aosd_color_t color1 = data->decoration->colors[1];
  aosd_color_t textcolor0 = data->text->fonts_color[0];
  aosd_color_t shadowcolor0 = data->text->fonts_shadow_color[0];
  gboolean draw_shadow = data->text->fonts_draw_shadow[0];
  int width = 0, height = 0, bearing = 0;

  aosd_layout_size( osd_layout , &width , &height , &bearing );

  /* draw rectangle container */
  cairo_set_source_rgba( cr , (double)color0.red / 65535 , (double)color0.green / 65535 ,
    (double)color0.blue / 65535 , (double)color0.alpha / 65535 );
  cairo_rectangle( cr , 0 , 0 ,
    aosd_deco_styles[AOSD_DECO_STYLE_RECT].padding.left + width +
    aosd_deco_styles[AOSD_DECO_STYLE_RECT].padding.right,
    aosd_deco_styles[AOSD_DECO_STYLE_RECT].padding.top + height +
    aosd_deco_styles[AOSD_DECO_STYLE_RECT].padding.bottom );
  cairo_fill_preserve( cr );
  cairo_set_source_rgba( cr , (double)color1.red / 65535 , (double)color1.green / 65535 ,
    (double)color1.blue / 65535 , (double)color1.alpha / 65535 );
  cairo_stroke( cr );

  if ( draw_shadow == true )
  {
    /* draw text shadow */
    cairo_set_source_rgba( cr , (double)shadowcolor0.red / 65535 , (double)shadowcolor0.green / 65535 ,
      (double)shadowcolor0.blue / 65535 , (double)shadowcolor0.alpha / 65535 );
    cairo_move_to( cr,
      aosd_deco_styles[AOSD_DECO_STYLE_RECT].padding.left + bearing + 2 ,
      aosd_deco_styles[AOSD_DECO_STYLE_RECT].padding.top + 2 );
    pango_cairo_show_layout( cr , osd_layout );
  }

  /* draw text */
  cairo_set_source_rgba( cr , (double)textcolor0.red / 65535 , (double)textcolor0.green / 65535 ,
    (double)textcolor0.blue / 65535 , (double)textcolor0.alpha / 65535 );
  cairo_move_to( cr,
    aosd_deco_styles[AOSD_DECO_STYLE_RECT].padding.left + bearing ,
    aosd_deco_styles[AOSD_DECO_STYLE_RECT].padding.top );
  pango_cairo_show_layout( cr , osd_layout );
}


static void
aosd_deco_rfunc_roundrect ( Ghosd * osd , cairo_t * cr , aosd_deco_style_data_t * data )
{
  /* decoration information
     ----------------------
     draws a rectangular decoration with rounded angles; uses 2 colors
     (user color 1 and 2) and 1 font (user font 1), with optional shadow
  */
  PangoLayout *osd_layout = data->layout;
  aosd_color_t color0 = data->decoration->colors[0];
  aosd_color_t color1 = data->decoration->colors[1];
  aosd_color_t textcolor0 = data->text->fonts_color[0];
  aosd_color_t shadowcolor0 = data->text->fonts_shadow_color[0];
  gboolean draw_shadow = data->text->fonts_draw_shadow[0];
  int width = 0, height = 0, bearing = 0;

  aosd_layout_size( osd_layout , &width , &height , &bearing );

  /* draw rounded-rectangle container */
  cairo_set_source_rgba( cr , (double)color0.red / 65535 , (double)color0.green / 65535 ,
    (double)color0.blue / 65535 , (double)color0.alpha / 65535 );
  cairo_move_to( cr , aosd_deco_styles[AOSD_DECO_STYLE_ROUNDRECT].padding.left , 0 );
  cairo_arc( cr , width + aosd_deco_styles[AOSD_DECO_STYLE_ROUNDRECT].padding.left ,
    aosd_deco_styles[AOSD_DECO_STYLE_ROUNDRECT].padding.top ,
    10. , -G_PI_2 , 0. );
  cairo_arc( cr , width + aosd_deco_styles[AOSD_DECO_STYLE_ROUNDRECT].padding.left ,
    aosd_deco_styles[AOSD_DECO_STYLE_ROUNDRECT].padding.top + height ,
    10. , -4. * G_PI_2 , -3. * G_PI_2 );
  cairo_arc( cr , aosd_deco_styles[AOSD_DECO_STYLE_ROUNDRECT].padding.left ,
    aosd_deco_styles[AOSD_DECO_STYLE_ROUNDRECT].padding.top + height ,
    10. , -3. * G_PI_2 , -2. * G_PI_2 );
  cairo_arc( cr , aosd_deco_styles[AOSD_DECO_STYLE_ROUNDRECT].padding.left ,
    aosd_deco_styles[AOSD_DECO_STYLE_ROUNDRECT].padding.top ,
    10. , -2. * G_PI_2 , -G_PI_2 );
  cairo_close_path( cr );
  cairo_fill_preserve( cr );
  cairo_set_source_rgba( cr , (double)color1.red / 65535 , (double)color1.green / 65535 ,
    (double)color1.blue / 65535 , (double)color1.alpha / 65535 );
  cairo_stroke( cr );

  if ( draw_shadow == true )
  {
    /* draw text shadow */
    cairo_set_source_rgba( cr , (double)shadowcolor0.red / 65535 , (double)shadowcolor0.green / 65535 ,
      (double)shadowcolor0.blue / 65535 , (double)shadowcolor0.alpha / 65535 );
    cairo_move_to( cr ,
      aosd_deco_styles[AOSD_DECO_STYLE_ROUNDRECT].padding.left + bearing + 2 ,
      aosd_deco_styles[AOSD_DECO_STYLE_ROUNDRECT].padding.top + 2 );
    pango_cairo_show_layout( cr , osd_layout );
  }

  /* draw text */
  cairo_set_source_rgba( cr , (double)textcolor0.red / 65535 , (double)textcolor0.green / 65535 ,
    (double)textcolor0.blue / 65535 , (double)textcolor0.alpha / 65535 );
  cairo_move_to( cr ,
    aosd_deco_styles[AOSD_DECO_STYLE_ROUNDRECT].padding.left + bearing ,
    aosd_deco_styles[AOSD_DECO_STYLE_ROUNDRECT].padding.top );
  pango_cairo_show_layout( cr , osd_layout );
}


static void
aosd_deco_rfunc_concaverect ( Ghosd * osd , cairo_t * cr , aosd_deco_style_data_t * data )
{
  /* decoration information
     ----------------------
     draws a rectangle with concave angles; uses 2 colors
     (user color 1 and 2) and 1 font (user font 1), with optional shadow
  */
  PangoLayout *osd_layout = data->layout;
  aosd_color_t color0 = data->decoration->colors[0];
  aosd_color_t color1 = data->decoration->colors[1];
  aosd_color_t textcolor0 = data->text->fonts_color[0];
  aosd_color_t shadowcolor0 = data->text->fonts_shadow_color[0];
  gboolean draw_shadow = data->text->fonts_draw_shadow[0];
  int width = 0, height = 0, bearing = 0;

  aosd_layout_size( osd_layout , &width , &height , &bearing );

  /* draw jigsaw-piece-like container */
  cairo_set_source_rgba( cr , (double)color0.red / 65535 , (double)color0.green / 65535 ,
    (double)color0.blue / 65535 , (double)color0.alpha / 65535 );
  cairo_move_to( cr , aosd_deco_styles[AOSD_DECO_STYLE_CONCAVERECT].padding.left , 0 );
  cairo_arc_negative( cr , width + aosd_deco_styles[AOSD_DECO_STYLE_CONCAVERECT].padding.left + 2 ,
    aosd_deco_styles[AOSD_DECO_STYLE_CONCAVERECT].padding.top - 2 ,
    8. , -G_PI_2 , 0. );
  cairo_arc_negative( cr , width + aosd_deco_styles[AOSD_DECO_STYLE_CONCAVERECT].padding.left + 2 ,
    aosd_deco_styles[AOSD_DECO_STYLE_CONCAVERECT].padding.top + height + 2 ,
    8. , -4. * G_PI_2 , -3. * G_PI_2 );
  cairo_arc_negative( cr , aosd_deco_styles[AOSD_DECO_STYLE_CONCAVERECT].padding.left - 2 ,
    aosd_deco_styles[AOSD_DECO_STYLE_CONCAVERECT].padding.top + height + 2 ,
    8. , -3. * G_PI_2 , -2. * G_PI_2 );
  cairo_arc_negative( cr , aosd_deco_styles[AOSD_DECO_STYLE_CONCAVERECT].padding.left - 2 ,
    aosd_deco_styles[AOSD_DECO_STYLE_CONCAVERECT].padding.top - 2 ,
    8. , -2. * G_PI_2 , -G_PI_2 );
  cairo_close_path( cr );
  cairo_fill_preserve( cr );
  cairo_set_source_rgba( cr , (double)color1.red / 65535 , (double)color1.green / 65535 ,
    (double)color1.blue / 65535 , (double)color1.alpha / 65535 );
  cairo_stroke( cr );

  if ( draw_shadow == true )
  {
    /* draw text shadow */
    cairo_set_source_rgba( cr , (double)shadowcolor0.red / 65535 , (double)shadowcolor0.green / 65535 ,
      (double)shadowcolor0.blue / 65535 , (double)shadowcolor0.alpha / 65535 );
    cairo_move_to( cr ,
      aosd_deco_styles[AOSD_DECO_STYLE_CONCAVERECT].padding.left + bearing + 2 ,
      aosd_deco_styles[AOSD_DECO_STYLE_CONCAVERECT].padding.top + 2 );
    pango_cairo_show_layout( cr , osd_layout );
  }

  /* draw text */
  cairo_set_source_rgba( cr , (double)textcolor0.red / 65535 , (double)textcolor0.green / 65535 ,
    (double)textcolor0.blue / 65535 , (double)textcolor0.alpha / 65535 );
  cairo_move_to( cr ,
    aosd_deco_styles[AOSD_DECO_STYLE_CONCAVERECT].padding.left + bearing ,
    aosd_deco_styles[AOSD_DECO_STYLE_CONCAVERECT].padding.top );
  pango_cairo_show_layout( cr , osd_layout );
}


static void
aosd_deco_rfunc_none ( Ghosd * osd , cairo_t * cr , aosd_deco_style_data_t * data )
{
  /* decoration information
     ----------------------
     does not draw any decoration around text; uses 0 colors
     and 1 font (user font 1), with optional shadow
  */
  PangoLayout *osd_layout = data->layout;
  aosd_color_t textcolor0 = data->text->fonts_color[0];
  aosd_color_t shadowcolor0 = data->text->fonts_shadow_color[0];
  gboolean draw_shadow = data->text->fonts_draw_shadow[0];
  int width = 0, height = 0, bearing = 0;

  aosd_layout_size( osd_layout , &width , &height , &bearing );

  if ( draw_shadow == true )
  {
    /* draw text shadow */
    cairo_set_source_rgba( cr , (double)shadowcolor0.red / 65535 , (double)shadowcolor0.green / 65535 ,
      (double)shadowcolor0.blue / 65535 , (double)shadowcolor0.alpha / 65535 );
    cairo_move_to( cr ,
      aosd_deco_styles[AOSD_DECO_STYLE_NONE].padding.left + bearing + 2 ,
      aosd_deco_styles[AOSD_DECO_STYLE_NONE].padding.top + 2 );
    pango_cairo_show_layout( cr , osd_layout );
  }

  /* draw text */
  cairo_set_source_rgba( cr , (double)textcolor0.red / 65535 , (double)textcolor0.green / 65535 ,
    (double)textcolor0.blue / 65535 , (double)textcolor0.alpha / 65535 );
  cairo_move_to( cr ,
    aosd_deco_styles[AOSD_DECO_STYLE_NONE].padding.left + bearing ,
    aosd_deco_styles[AOSD_DECO_STYLE_NONE].padding.top );
  pango_cairo_show_layout( cr , osd_layout );
}
