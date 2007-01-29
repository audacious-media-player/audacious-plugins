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
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>
#include "ghosd.h"
#include "ghosd-text.h"


#define AOSD_STATUS_HIDDEN 0
#define AOSD_STATUS_SHOWN  1
#define AOSD_STATUS_UPDATE 2


static pthread_t * aosd_thread = NULL;
static pthread_mutex_t aosd_status_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t aosd_status_cond = PTHREAD_COND_INITIALIZER;
static gboolean aosd_status = AOSD_STATUS_HIDDEN;


typedef struct
{
  gchar * markup_message;
  aosd_cfg_osd_t * cfg_osd;
  gboolean cfg_is_copied;
}
aosd_thread_data_t;


typedef struct
{
  cairo_surface_t * surface;
  float alpha;
  void * user_data;
  gint width;
  gint height;
  gint deco_code;
}
GhosdFadeData;


static void
aosd_fade_func ( Ghosd * osd , cairo_t * cr , void * user_data )
{
  GhosdFadeData *fade_data = user_data;

  if ( fade_data->surface == NULL )
  {
    cairo_t *rendered_cr;
    fade_data->surface = cairo_surface_create_similar( cairo_get_target( cr ) ,
                           CAIRO_CONTENT_COLOR_ALPHA , fade_data->width , fade_data->height );
    rendered_cr = cairo_create( fade_data->surface );
    aosd_deco_style_render( fade_data->deco_code , osd , rendered_cr , fade_data->user_data );
    cairo_destroy( rendered_cr );
  }

  cairo_set_source_surface( cr , fade_data->surface , 0 , 0 );
  cairo_paint_with_alpha( cr , fade_data->alpha );
}


static void *
aosd_thread_func ( void * arg )
{
  aosd_thread_data_t *thread_data = (aosd_thread_data_t*)arg;
  gchar *markup_string = thread_data->markup_message;
  aosd_cfg_osd_t *cfg_osd = thread_data->cfg_osd;
  Ghosd *osd;
  GhosdFadeData fade_data = { NULL , 0 , NULL , 0 , 0 , 0 };
  PangoContext *context;
  PangoLayout *osd_layout;
  gint max_width, layout_width, layout_height, pos_x = 0, pos_y = 0;
  gint pad_left = 0 , pad_right = 0 , pad_top = 0 , pad_bottom = 0;
  const gint screen_width = gdk_screen_get_width( gdk_screen_get_default() );
  const gint screen_height = gdk_screen_get_height( gdk_screen_get_default() );
  const gint STEP_MS = 50;
  const gfloat dalpha_in = 1.0 / ( cfg_osd->animation.timing_fadein / (gfloat)STEP_MS );
  const gfloat dalpha_out = 1.0 / ( cfg_osd->animation.timing_fadeout / (gfloat)STEP_MS );
  struct timeval tv_nextupdate;
  gboolean stop_now = FALSE;
  aosd_deco_style_data_t style_data;

  /* pick padding from selected decoration style */
  aosd_deco_style_get_padding( cfg_osd->decoration.code ,
    &pad_top , &pad_bottom , &pad_left , &pad_right );

  max_width = screen_width - pad_left - pad_right - abs(cfg_osd->position.offset_x);
  context = pango_cairo_font_map_create_context(
              PANGO_CAIRO_FONT_MAP(pango_cairo_font_map_get_default()));
  osd_layout = pango_layout_new(context);
  pango_layout_set_markup( osd_layout, markup_string , -1 );
  pango_layout_set_ellipsize( osd_layout , PANGO_ELLIPSIZE_NONE );
  pango_layout_set_justify( osd_layout , FALSE );
  pango_layout_set_width( osd_layout , PANGO_SCALE * max_width );
  pango_layout_get_pixel_size( osd_layout , &layout_width , &layout_height );

  osd = ghosd_new();

  /* osd position */
  switch ( cfg_osd->position.placement )
  {
    case AOSD_POSITION_PLACEMENT_TOP:
      pos_x = (screen_width - (layout_width + pad_left + pad_right)) / 2;
      pos_y = 0;
      break;
    case AOSD_POSITION_PLACEMENT_TOPRIGHT:
      pos_x = screen_width - (layout_width + pad_left + pad_right);
      pos_y = 0;
      break;
    case AOSD_POSITION_PLACEMENT_MIDDLELEFT:
      pos_x = 0;
      pos_y = (screen_height - (layout_height + pad_top + pad_bottom)) / 2;
      break;
    case AOSD_POSITION_PLACEMENT_MIDDLE:
      pos_x = (screen_width - (layout_width + pad_left + pad_right)) / 2;
      pos_y = (screen_height - (layout_height + pad_top + pad_bottom)) / 2;
      break;
    case AOSD_POSITION_PLACEMENT_MIDDLERIGHT:
      pos_x = screen_width - (layout_width + pad_left + pad_right);
      pos_y = (screen_height - (layout_height + pad_top + pad_bottom)) / 2;
      break;
    case AOSD_POSITION_PLACEMENT_BOTTOMLEFT:
      pos_x = 0;
      pos_y = screen_height - (layout_height + pad_top + pad_bottom);
      break;
    case AOSD_POSITION_PLACEMENT_BOTTOM:
      pos_x = (screen_width - (layout_width + pad_left + pad_right)) / 2;
      pos_y = screen_height - (layout_height + pad_top + pad_bottom);
      break;
    case AOSD_POSITION_PLACEMENT_BOTTOMRIGHT:
      pos_x = screen_width - (layout_width + pad_left + pad_right);
      pos_y = screen_height - (layout_height + pad_top + pad_bottom);
      break;
    case AOSD_POSITION_PLACEMENT_TOPLEFT:
    default:
      pos_x = 0;
      pos_y = 0;
      break;
  }

  /* add offset to position */
  pos_x += cfg_osd->position.offset_x;
  pos_y += cfg_osd->position.offset_y;

  ghosd_set_position( osd , pos_x , pos_y ,
    layout_width + pad_left + pad_right ,
    layout_height + pad_top + pad_bottom );

  /* the aosd_status must be checked during the fade and display process
     (if another message arrives, the current transition must be abandoned ) */

  style_data.layout = osd_layout;
  style_data.text = &(cfg_osd->text);
  style_data.decoration = &(cfg_osd->decoration);
  fade_data.user_data = &style_data;
  fade_data.width = layout_width + pad_left + pad_right;
  fade_data.height = layout_height + pad_top + pad_bottom;
  fade_data.alpha = 0;
  fade_data.deco_code = cfg_osd->decoration.code;
  ghosd_set_render( osd , (GhosdRenderFunc)aosd_fade_func , &fade_data , NULL );

  /* show the osd (with alpha 0, invisible) */
  ghosd_show( osd );

  if ( stop_now != TRUE )
  {
    /* fade in */
    for ( fade_data.alpha = 0 ; fade_data.alpha < 1.0 ; fade_data.alpha += dalpha_in )
    {
      pthread_mutex_lock( &aosd_status_mutex );
      if ( aosd_status == AOSD_STATUS_UPDATE )
        { pthread_mutex_unlock( &aosd_status_mutex ); stop_now = TRUE; break; }
      pthread_mutex_unlock( &aosd_status_mutex );

      if (fade_data.alpha > 1.0) fade_data.alpha = 1.0;
      ghosd_render( osd );
      gettimeofday( &tv_nextupdate , NULL );
      tv_nextupdate.tv_usec += STEP_MS*1000;
      ghosd_main_until( osd , &tv_nextupdate );
    }
  }

  if ( stop_now != TRUE )
  {
    /* show the osd for the desidered amount of time */
    gint time_iter_ms = 0;

    fade_data.alpha = 1.0;
    ghosd_render( osd );

    while ( time_iter_ms < cfg_osd->animation.timing_display )
    {
      pthread_mutex_lock( &aosd_status_mutex );
      if ( aosd_status == AOSD_STATUS_UPDATE )
        { pthread_mutex_unlock( &aosd_status_mutex ); stop_now = TRUE; break; }
      pthread_mutex_unlock( &aosd_status_mutex );

      gettimeofday(&tv_nextupdate, NULL);
      tv_nextupdate.tv_usec += STEP_MS*1000;
      time_iter_ms += STEP_MS;
      ghosd_main_until( osd , &tv_nextupdate);
    }
  }

  if ( stop_now != TRUE )
  {
    /* fade out */
    for ( fade_data.alpha = 1 ; fade_data.alpha > 0.0 ; fade_data.alpha -= dalpha_out )
    {
      pthread_mutex_lock( &aosd_status_mutex );
      if ( aosd_status == AOSD_STATUS_UPDATE )
        { pthread_mutex_unlock( &aosd_status_mutex ); stop_now = TRUE; break; }
      pthread_mutex_unlock( &aosd_status_mutex );

      if (fade_data.alpha < 0.0) fade_data.alpha = 0;
      ghosd_render( osd );
      gettimeofday( &tv_nextupdate , NULL );
      tv_nextupdate.tv_usec += STEP_MS*1000;
      ghosd_main_until( osd , &tv_nextupdate );
    }
  }

  fade_data.alpha = 0;
  ghosd_render( osd );

  ghosd_hide( osd );
  ghosd_main_iterations( osd );
  ghosd_destroy( osd );
  if ( fade_data.surface != NULL )
    cairo_surface_destroy( fade_data.surface );

  pthread_mutex_lock( &aosd_status_mutex );
  if ( aosd_status == AOSD_STATUS_UPDATE )
  {
    aosd_status = AOSD_STATUS_SHOWN;
    pthread_mutex_unlock( &aosd_status_mutex );
    pthread_cond_signal( &aosd_status_cond );
  }
  else
  {
    aosd_status = AOSD_STATUS_HIDDEN;
    pthread_mutex_unlock( &aosd_status_mutex );
  }

  g_free( markup_string );
  if ( thread_data->cfg_is_copied == TRUE )
    aosd_cfg_osd_delete( cfg_osd );
  g_free( thread_data );
  g_object_unref( osd_layout );
  g_object_unref( context );
  pthread_exit(NULL);
}


gint
aosd_display ( gchar * markup_string , aosd_cfg_osd_t * cfg_osd , gboolean copy_cfg )
{
  aosd_thread_data_t *thread_data = g_malloc(sizeof(aosd_thread_data_t));
  thread_data->markup_message = g_strdup( markup_string );
  if ( copy_cfg == TRUE )
  {
    thread_data->cfg_osd = aosd_cfg_osd_copy( cfg_osd );
    thread_data->cfg_is_copied = TRUE;
  }
  else
  {
    thread_data->cfg_osd = cfg_osd;
    thread_data->cfg_is_copied = FALSE;
  }

  /* check if osd is already displaying a message now */
  pthread_mutex_lock( &aosd_status_mutex );
  if ( aosd_status == AOSD_STATUS_SHOWN )
  {
    /* a message is already being shown in osd, stop the
       display of that message cause there is a new one */
    aosd_status = AOSD_STATUS_UPDATE;
    pthread_cond_wait( &aosd_status_cond , &aosd_status_mutex );
  }
  else
  {
    /* no message is being shown in osd, show one now */
    aosd_status = AOSD_STATUS_SHOWN;
  }
  pthread_mutex_unlock( &aosd_status_mutex );

  if ( aosd_thread != NULL )
    pthread_join( *aosd_thread , NULL );
  else
    aosd_thread = g_malloc(sizeof(pthread_t));

  pthread_create( aosd_thread , NULL , aosd_thread_func , thread_data );
  return 0;
}


void
aosd_shutdown ( void )
{
  if ( aosd_thread != NULL )
  {
    pthread_mutex_lock( &aosd_status_mutex );
    if ( aosd_status == AOSD_STATUS_SHOWN )
    {
      /* a message is being shown in osd,
         stop the display of that message */
      aosd_status = AOSD_STATUS_UPDATE;
      pthread_cond_wait( &aosd_status_cond , &aosd_status_mutex );
    }
    pthread_mutex_unlock( &aosd_status_mutex );
    pthread_join( *aosd_thread , NULL );
    g_free( aosd_thread );
    aosd_thread = NULL;
    aosd_status = AOSD_STATUS_HIDDEN; /* aosd_thread joined, no need to mutex this */
  }
  return;
}
