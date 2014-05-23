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

#include "aosd_osd.h"
#include "aosd_style.h"
#include "aosd_style_private.h"
#include "aosd_cfg.h"
#include <glib.h>
#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <gdk/gdk.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "ghosd.h"


#define AOSD_STATUS_HIDDEN       0
#define AOSD_STATUS_FADEIN       1
#define AOSD_STATUS_SHOW         2
#define AOSD_STATUS_FADEOUT      3
#define AOSD_STATUS_DESTROY      4
/* updating OSD every 50 msec */
#define AOSD_TIMING              50


typedef struct
{
  cairo_surface_t * surface;
  gfloat alpha;
  gpointer user_data;
  gint width;
  gint height;
  gint deco_code;
}
GhosdFadeData;


typedef struct
{
  gchar * markup_message;
  gboolean cfg_is_copied;
  gfloat dalpha_in, dalpha_out, ddisplay_stay;

  PangoContext *pango_context;
  PangoLayout *pango_layout;

  aosd_cfg_osd_t * cfg_osd;

  GhosdFadeData fade_data;
}
GhosdData;


static gint osd_source_id = 0;
static gint osd_status = AOSD_STATUS_HIDDEN;
static Ghosd *osd;
static GhosdData *osd_data;


static void
aosd_osd_data_alloc ( gchar * markup_string , aosd_cfg_osd_t * cfg_osd , gboolean copy_cfg )
{
  osd_data = g_malloc0(sizeof(GhosdData));
  osd_data->markup_message = g_strdup( markup_string );
  if ( copy_cfg == TRUE )
  {
    osd_data->cfg_osd = aosd_cfg_osd_copy( cfg_osd );
    osd_data->cfg_is_copied = TRUE;
  }
  else
  {
    osd_data->cfg_osd = cfg_osd;
    osd_data->cfg_is_copied = FALSE;
  }
  return;
}


static void
aosd_osd_data_free ( void )
{
  if ( osd_data->fade_data.surface != NULL )
  {
    cairo_surface_destroy( osd_data->fade_data.surface );
    osd_data->fade_data.surface = NULL;
  }

  if ( osd_data->markup_message != NULL )
  {
    g_free( osd_data->markup_message );
    osd_data->markup_message = NULL;
  }

  if ( osd_data->cfg_is_copied == TRUE )
  {
    aosd_cfg_osd_delete( osd_data->cfg_osd );
    osd_data->cfg_osd = NULL;
  }

  if ( osd_data->pango_layout != NULL )
  {
    g_object_unref( osd_data->pango_layout );
    osd_data->pango_layout = NULL;
  }

  if ( osd_data->pango_context != NULL )
  {
    g_object_unref( osd_data->pango_context );
    osd_data->pango_context = NULL;
  }

  g_free( osd_data );
}


static void
aosd_osd_hide ( void )
{
  if ( osd != NULL )
  {
    ghosd_hide( osd );
    ghosd_main_iterations( osd );
  }
  return;
}


static void
aosd_fade_func ( Ghosd * gosd , cairo_t * cr , void * user_data )
{
  GhosdFadeData *fade_data = user_data;

  if ( fade_data->surface == NULL )
  {
    cairo_t *rendered_cr;
    fade_data->surface = cairo_surface_create_similar( cairo_get_target( cr ) ,
                           CAIRO_CONTENT_COLOR_ALPHA , fade_data->width , fade_data->height );
    rendered_cr = cairo_create( fade_data->surface );
    aosd_deco_style_render( fade_data->deco_code , gosd , rendered_cr , fade_data->user_data );
    cairo_destroy( rendered_cr );
  }

  cairo_set_source_surface( cr , fade_data->surface , 0 , 0 );
  cairo_paint_with_alpha( cr , fade_data->alpha );
}


static void
aosd_button_func ( Ghosd * gosd , GhosdEventButton * ev , void * user_data )
{
  if ( ev->button == 1 )
  {
    osd_status = AOSD_STATUS_DESTROY; /* move to status destroy */
  }
  return;
}


static void
aosd_osd_create ( void )
{
  gint max_width, layout_width, layout_height;
  PangoRectangle ink, log;
  GdkScreen *screen = gdk_screen_get_default();
  gint pos_x = 0, pos_y = 0;
  gint pad_left = 0 , pad_right = 0 , pad_top = 0 , pad_bottom = 0;
  gint screen_width, screen_height;
  aosd_deco_style_data_t style_data;

  /* calculate screen_width and screen_height */
  if ( osd_data->cfg_osd->position.multimon_id > -1 )
  {
    /* adjust coordinates and size according to selected monitor */
    GdkRectangle rect;
    gdk_screen_get_monitor_geometry( screen , osd_data->cfg_osd->position.multimon_id , &rect );
    pos_x = rect.x;
    pos_y = rect.y;
    screen_width = rect.width;
    screen_height = rect.height;
  }
  else
  {
    /* use total space available, even when composed by multiple monitor */
    screen_width = gdk_screen_get_width( screen );
    screen_height = gdk_screen_get_height( screen );
    pos_x = 0;
    pos_y = 0;
  }

  /* pick padding from selected decoration style */
  aosd_deco_style_get_padding( osd_data->cfg_osd->decoration.code ,
    &pad_top , &pad_bottom , &pad_left , &pad_right );

  if ( osd_data->cfg_osd->position.maxsize_width > 0 )
  {
    gint max_width_default = screen_width - pad_left - pad_right - abs(osd_data->cfg_osd->position.offset_x);
    max_width = osd_data->cfg_osd->position.maxsize_width - pad_left - pad_right;
    /* ignore user-defined max_width if it is too small or too large */
    if (( max_width < 1 ) || ( max_width > max_width_default ))
      max_width = max_width_default;
  }
  else
  {
    max_width = screen_width - pad_left - pad_right - abs(osd_data->cfg_osd->position.offset_x);
  }

  osd_data->pango_context = pango_font_map_create_context
   (pango_cairo_font_map_get_default ());
  osd_data->pango_layout = pango_layout_new(osd_data->pango_context);
  pango_layout_set_markup( osd_data->pango_layout, osd_data->markup_message , -1 );
  pango_layout_set_ellipsize( osd_data->pango_layout , PANGO_ELLIPSIZE_NONE );
  pango_layout_set_justify( osd_data->pango_layout , FALSE );
  pango_layout_set_width( osd_data->pango_layout , PANGO_SCALE * max_width );
  pango_layout_get_pixel_extents( osd_data->pango_layout , &ink , &log );
  layout_width = ink.width;
  layout_height = log.height;

  /* osd position */
  switch ( osd_data->cfg_osd->position.placement )
  {
    case AOSD_POSITION_PLACEMENT_TOP:
      pos_x += (screen_width - (layout_width + pad_left + pad_right)) / 2;
      pos_y += 0;
      break;
    case AOSD_POSITION_PLACEMENT_TOPRIGHT:
      pos_x += screen_width - (layout_width + pad_left + pad_right);
      pos_y += 0;
      break;
    case AOSD_POSITION_PLACEMENT_MIDDLELEFT:
      pos_x += 0;
      pos_y += (screen_height - (layout_height + pad_top + pad_bottom)) / 2;
      break;
    case AOSD_POSITION_PLACEMENT_MIDDLE:
      pos_x += (screen_width - (layout_width + pad_left + pad_right)) / 2;
      pos_y += (screen_height - (layout_height + pad_top + pad_bottom)) / 2;
      break;
    case AOSD_POSITION_PLACEMENT_MIDDLERIGHT:
      pos_x += screen_width - (layout_width + pad_left + pad_right);
      pos_y += (screen_height - (layout_height + pad_top + pad_bottom)) / 2;
      break;
    case AOSD_POSITION_PLACEMENT_BOTTOMLEFT:
      pos_x += 0;
      pos_y += screen_height - (layout_height + pad_top + pad_bottom);
      break;
    case AOSD_POSITION_PLACEMENT_BOTTOM:
      pos_x += (screen_width - (layout_width + pad_left + pad_right)) / 2;
      pos_y += screen_height - (layout_height + pad_top + pad_bottom);
      break;
    case AOSD_POSITION_PLACEMENT_BOTTOMRIGHT:
      pos_x += screen_width - (layout_width + pad_left + pad_right);
      pos_y += screen_height - (layout_height + pad_top + pad_bottom);
      break;
    case AOSD_POSITION_PLACEMENT_TOPLEFT:
    default:
      pos_x += 0;
      pos_y += 0;
      break;
  }

  /* add offset to position */
  pos_x += osd_data->cfg_osd->position.offset_x;
  pos_y += osd_data->cfg_osd->position.offset_y;

  ghosd_set_position( osd , pos_x , pos_y ,
    layout_width + pad_left + pad_right ,
    layout_height + pad_top + pad_bottom );

  ghosd_set_event_button_cb( osd , aosd_button_func , NULL );

  style_data.layout = osd_data->pango_layout;
  style_data.text = &(osd_data->cfg_osd->text);
  style_data.decoration = &(osd_data->cfg_osd->decoration);
  osd_data->fade_data.surface = NULL;
  osd_data->fade_data.user_data = &style_data;
  osd_data->fade_data.width = layout_width + pad_left + pad_right;
  osd_data->fade_data.height = layout_height + pad_top + pad_bottom;
  osd_data->fade_data.alpha = 0;
  osd_data->fade_data.deco_code = osd_data->cfg_osd->decoration.code;
  osd_data->dalpha_in = 1.0 / ( osd_data->cfg_osd->animation.timing_fadein / (gfloat)AOSD_TIMING );
  osd_data->dalpha_out = 1.0 / ( osd_data->cfg_osd->animation.timing_fadeout / (gfloat)AOSD_TIMING );
  if (osd_data->cfg_osd->animation.timing_display) {
      osd_data->ddisplay_stay = 1.0 / ( osd_data->cfg_osd->animation.timing_display / (gfloat)AOSD_TIMING );
  } else {
      osd_data->ddisplay_stay = 0;
  }
  ghosd_set_render( osd , (GhosdRenderFunc)aosd_fade_func , &(osd_data->fade_data) , NULL );

  /* show the osd (with alpha 0, invisible) */
  ghosd_show( osd );
  return;
}


static gboolean
aosd_timer_func ( gpointer none )
{
  static gfloat display_time = 0;

  switch ( osd_status )
  {
    case AOSD_STATUS_FADEIN:
    {
      /* fade in */
      osd_data->fade_data.alpha += osd_data->dalpha_in;
      if ( osd_data->fade_data.alpha < 1.0 )
      {
        ghosd_render( osd );
        ghosd_main_iterations( osd );
      }
      else
      {
        osd_data->fade_data.alpha = 1.0;
        display_time = 0;
        osd_status = AOSD_STATUS_SHOW; /* move to next phase */
        ghosd_render( osd );
        ghosd_main_iterations( osd );
      }
      return TRUE;
    }

    case AOSD_STATUS_SHOW:
    {
      display_time += osd_data->ddisplay_stay;
      if ( display_time >= 1.0 )
      {
        osd_status = AOSD_STATUS_FADEOUT; /* move to next phase */
        ghosd_main_iterations( osd );
      }
      else
      {
        ghosd_main_iterations( osd );
      }
      return TRUE;
    }

    case AOSD_STATUS_FADEOUT:
    {
      /* fade out */
      osd_data->fade_data.alpha -= osd_data->dalpha_out;
      if ( osd_data->fade_data.alpha > 0.0 )
      {
        ghosd_render( osd );
        ghosd_main_iterations( osd );
      }
      else
      {
        osd_data->fade_data.alpha = 0.0;
        osd_status = AOSD_STATUS_DESTROY; /* move to next phase */
        ghosd_render( osd );
        ghosd_main_iterations( osd );
      }
      return TRUE;
    }

    case AOSD_STATUS_DESTROY:
    {
      aosd_osd_hide();
      aosd_osd_data_free();

      osd_status = AOSD_STATUS_HIDDEN; /* reset status */
      osd_source_id = 0;
      return FALSE;
    }
  }

  return TRUE;
}


gint
aosd_osd_display ( gchar * markup_string , aosd_cfg_osd_t * cfg_osd , gboolean copy_cfg )
{
  if ( osd != NULL )
  {
    if ( osd_status == AOSD_STATUS_HIDDEN )
    {
      aosd_osd_data_alloc( markup_string , cfg_osd , copy_cfg );
      aosd_osd_create();
      osd_status = AOSD_STATUS_FADEIN;
      osd_source_id = g_timeout_add_full( G_PRIORITY_DEFAULT_IDLE , AOSD_TIMING ,
                                          aosd_timer_func , NULL , NULL );
    }
    else
    {
      g_source_remove( osd_source_id ); /* remove timer */
      osd_source_id = 0;
      aosd_osd_hide();
      aosd_osd_data_free();
      osd_status = AOSD_STATUS_HIDDEN;
      /* now display new OSD */
      aosd_osd_data_alloc( markup_string , cfg_osd , copy_cfg );
      aosd_osd_create();
      osd_status = AOSD_STATUS_FADEIN;
      osd_source_id = g_timeout_add_full( G_PRIORITY_DEFAULT_IDLE , AOSD_TIMING ,
                                          aosd_timer_func , NULL , NULL );
    }
    return 0;
  }
  else
  {
    g_warning( "OSD display requested, but no osd object is loaded!\n" );
  }
  return 1;
}


void
aosd_osd_shutdown ( void )
{
  if ( osd != NULL )
  {
    if ( osd_status != AOSD_STATUS_HIDDEN ) /* osd is being displayed */
    {
      g_source_remove( osd_source_id ); /* remove timer */
      osd_source_id = 0;
      aosd_osd_hide();
      aosd_osd_data_free();
      osd_status = AOSD_STATUS_HIDDEN;
    }
  }
  else
  {
    g_warning( "OSD shutdown requested, but no osd object is loaded!\n" );
  }
  return;
}


void
aosd_osd_init ( gint transparency_mode )
{
  if ( osd == NULL )
  {
    /* create Ghosd object */
    if ( transparency_mode == AOSD_MISC_TRANSPARENCY_FAKE )
      osd = ghosd_new();
    else
#ifdef HAVE_XCOMPOSITE
    {
      /* check if the composite module is actually loaded */
      if ( aosd_osd_check_composite_ext() )
        osd = ghosd_new_with_argbvisual(); /* ok */
      else
      {
        g_warning( "X Composite module not loaded; falling back to fake transparency.\n");
        osd = ghosd_new(); /* fall back to fake transparency */
      }
    }
#else
      osd = ghosd_new();
#endif

    if ( osd == NULL )
      g_warning( "Unable to load osd object; OSD will not work properly!\n" );
  }
  return;
}


void
aosd_osd_cleanup ( void )
{
  if ( osd != NULL )
  {
    /* destroy Ghosd object */
    ghosd_destroy( osd );
    osd = NULL;
  }
  return;
}

#ifdef HAVE_XCOMPOSITE
int
aosd_osd_check_composite_ext ( void )
{
  return ghosd_check_composite_ext();
}

int
aosd_osd_check_composite_mgr ( void )
{
  /* ghosd_check_composite_mgr() only checks for composite managers that
     adhere to the Extended Window Manager hint specification ver.1.4 from
     freedesktop ( where composite manager are identified with the hint
     _NET_WM_CM_Sn ); unfortunately, non-standard comp.managers and older
     ones (xcompmgr) do not use this hint; so let's also check if xcompmgr
     is running before reporting a likely absence of running comp.managers */
  int have_comp_mgr = ghosd_check_composite_mgr();

  if ( have_comp_mgr != 0 )
  {
    DEBUGMSG("running composite manager found\n");
    return have_comp_mgr;
  }
  else
  {
    /* check if xcompmgr is running; assumes there's a working 'ps'
       utility in the system; not the most elegant of the checking
       systems, but this is more than enough for its purpose */
    gchar *soutput = NULL, *serror = NULL;
    gint exit_status;

    if ( g_spawn_command_line_sync( "ps -eo comm" ,
           &soutput , &serror , &exit_status , NULL ) == TRUE )
    {
      if (( soutput != NULL ) && ( strstr( soutput , "\nxcompmgr\n" ) != NULL ))
      {
        DEBUGMSG("running xcompmgr found\n");
        have_comp_mgr = 1;
      }
      else
      {
        DEBUGMSG("running xcompmgr not found\n");
        have_comp_mgr = 0;
      }
    }
    else
    {
      g_warning("command 'ps -eo comm' failed, unable to check if xcompgr is running\n");
      have_comp_mgr = 0;
    }

    g_free( soutput );
    g_free( serror );
    return have_comp_mgr;
  }
}
#endif
