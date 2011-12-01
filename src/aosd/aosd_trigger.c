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

#include <glib.h>
#include <string.h>

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>

#include "aosd_trigger.h"
#include "aosd_trigger_private.h"
#include "aosd_cfg.h"
#include "aosd_osd.h"

extern aosd_cfg_t * global_config;


/* trigger codes ( the code values do not need to be sequential ) */
enum
{
  AOSD_TRIGGER_PB_START = 0,
  AOSD_TRIGGER_PB_TITLECHANGE = 1,
  AOSD_TRIGGER_VOL_CHANGE = 2,
  AOSD_TRIGGER_PB_PAUSEON = 3,
  AOSD_TRIGGER_PB_PAUSEOFF = 4
};

/* trigger codes array size */
#define AOSD_TRIGGER_CODES_ARRAY_SIZE 5

/* trigger codes array */
gint aosd_trigger_codes[] =
{
  AOSD_TRIGGER_PB_START,
  AOSD_TRIGGER_PB_TITLECHANGE,
  AOSD_TRIGGER_VOL_CHANGE,
  AOSD_TRIGGER_PB_PAUSEON,
  AOSD_TRIGGER_PB_PAUSEOFF
};

/* prototypes of trigger functions */
static void aosd_trigger_func_pb_start_onoff ( gboolean );
static void aosd_trigger_func_pb_start_cb ( gpointer , gpointer );
static void aosd_trigger_func_pb_titlechange_onoff ( gboolean );
static void aosd_trigger_func_pb_titlechange_cb ( gpointer , gpointer );
static void aosd_trigger_func_vol_change_onoff ( gboolean );
static void aosd_trigger_func_vol_change_cb ( gpointer , gpointer );
static void aosd_trigger_func_pb_pauseon_onoff ( gboolean );
static void aosd_trigger_func_pb_pauseon_cb ( gpointer , gpointer );
static void aosd_trigger_func_pb_pauseoff_onoff ( gboolean );
static void aosd_trigger_func_pb_pauseoff_cb ( gpointer , gpointer );
static void aosd_trigger_func_hook_cb ( gpointer markup_text , gpointer unused );

/* map trigger codes to trigger objects */
aosd_trigger_t aosd_triggers[] =
{
  [AOSD_TRIGGER_PB_START] = { N_("Playback Start") ,
                              N_("Triggers OSD when a playlist entry is played.") ,
                              aosd_trigger_func_pb_start_onoff ,
                              aosd_trigger_func_pb_start_cb },

  [AOSD_TRIGGER_PB_TITLECHANGE] = { N_("Title Change") ,
                                    N_("Triggers OSD when, during playback, the song title changes "
                                       "but the filename is the same. This is mostly useful to display "
                                       "title changes in internet streams.") ,
                                    aosd_trigger_func_pb_titlechange_onoff ,
                                    aosd_trigger_func_pb_titlechange_cb },

  [AOSD_TRIGGER_VOL_CHANGE] = { N_("Volume Change") ,
                                N_("Triggers OSD when volume is changed.") ,
                                aosd_trigger_func_vol_change_onoff ,
                                aosd_trigger_func_vol_change_cb },

  [AOSD_TRIGGER_PB_PAUSEON] = { N_("Pause On") ,
                                N_("Triggers OSD when playback is paused.") ,
                                aosd_trigger_func_pb_pauseon_onoff ,
                                aosd_trigger_func_pb_pauseon_cb },

  [AOSD_TRIGGER_PB_PAUSEOFF] = { N_("Pause Off") ,
                                 N_("Triggers OSD when playback is unpaused.") ,
                                 aosd_trigger_func_pb_pauseoff_onoff ,
                                 aosd_trigger_func_pb_pauseoff_cb }
};



/* TRIGGER API */

void
aosd_trigger_get_codes_array ( gint ** array , gint * array_size )
{
  *array = aosd_trigger_codes;
  *array_size = AOSD_TRIGGER_CODES_ARRAY_SIZE;
  return;
}


const gchar *
aosd_trigger_get_name ( gint trig_code )
{
  return aosd_triggers[trig_code].name;
}


const gchar *
aosd_trigger_get_desc ( gint trig_code )
{
  return aosd_triggers[trig_code].desc;
}


void
aosd_trigger_start ( aosd_cfg_osd_trigger_t * cfg_trigger )
{
  gint i = 0;
  for ( i = 0 ; i < cfg_trigger->active->len ; i++ )
  {
    gint trig_code = g_array_index( cfg_trigger->active , gint , i );
    aosd_triggers[trig_code].onoff_func( TRUE );
  }
  /* When called, this hook will display the text of the user pointer
     or the current playing song, if NULL */
  hook_associate( "aosd toggle" , aosd_trigger_func_hook_cb , NULL );
  return;
}


void
aosd_trigger_stop ( aosd_cfg_osd_trigger_t * cfg_trigger )
{
  gint i = 0;
  hook_dissociate( "aosd toggle" , aosd_trigger_func_hook_cb );
  for ( i = 0 ; i < cfg_trigger->active->len ; i++ )
  {
    gint trig_code = g_array_index( cfg_trigger->active , gint , i );
    aosd_triggers[trig_code].onoff_func( FALSE );
  }
  return;
}


/* HELPER FUNCTIONS */

static gchar * aosd_trigger_utf8convert (const gchar * str)
{
  if ( global_config->osd->text.utf8conv_disable == FALSE )
    return str_to_utf8( str );
  else
    return g_strdup( str );
}


/* TRIGGER FUNCTIONS */

static void
aosd_trigger_func_pb_start_onoff(gboolean turn_on)
{
  if (turn_on == TRUE)
    hook_associate("playback begin", aosd_trigger_func_pb_start_cb, NULL);
  else
    hook_dissociate("playback begin", aosd_trigger_func_pb_start_cb);
  return;
}

static void
aosd_trigger_func_pb_start_cb(gpointer hook_data, gpointer user_data)
{
    gchar *title = aud_drct_pl_get_title(aud_drct_pl_get_pos());

    if (title != NULL)
    {
        gchar *utf8_title = aosd_trigger_utf8convert(title);

        if (g_utf8_validate(utf8_title, -1, NULL) == TRUE)
        {
            gchar *utf8_title_markup = g_markup_printf_escaped(
                "<span font_desc='%s'>%s</span>", global_config->osd->text.fonts_name[0], utf8_title);
            aosd_osd_display(utf8_title_markup, global_config->osd, FALSE);
            g_free(utf8_title_markup);
        }
        g_free(utf8_title);
    }
    return;
}

typedef struct
{
  gchar *title;
  gchar *filename;
}
aosd_pb_titlechange_prevs_t;

static void
aosd_trigger_func_pb_titlechange_onoff ( gboolean turn_on )
{
  static aosd_pb_titlechange_prevs_t *prevs = NULL;

  if ( turn_on == TRUE )
  {
    prevs = g_malloc0(sizeof(aosd_pb_titlechange_prevs_t));
    prevs->title = NULL;
    prevs->filename = NULL;
    hook_associate( "title change" , aosd_trigger_func_pb_titlechange_cb , prevs );
  }
  else
  {
    hook_dissociate( "title change" , aosd_trigger_func_pb_titlechange_cb );
    if ( prevs != NULL )
    {
      if ( prevs->title != NULL ) g_free( prevs->title );
      if ( prevs->filename != NULL ) g_free( prevs->filename );
      g_free( prevs );
      prevs = NULL;
    }
  }
  return;
}

static void
aosd_trigger_func_pb_titlechange_cb ( gpointer plentry_gp , gpointer prevs_gp )
{
  if (aud_drct_get_playing ())
  {
    aosd_pb_titlechange_prevs_t *prevs = prevs_gp;
    gint playlist = aud_playlist_get_playing();
    gint pl_entry = aud_playlist_get_position(playlist);
    gchar * pl_entry_filename = aud_playlist_entry_get_filename (playlist, pl_entry);
    gchar * pl_entry_title = aud_playlist_entry_get_title (playlist, pl_entry, FALSE);

    /* same filename but title changed, useful to detect http stream song changes */

    if ( ( prevs->title != NULL ) && ( prevs->filename != NULL ) )
    {
      if ( ( pl_entry_filename != NULL ) && ( !strcmp(pl_entry_filename,prevs->filename) ) )
      {
        if ( ( pl_entry_title != NULL ) && ( strcmp(pl_entry_title,prevs->title) ) )
        {
          /* string formatting is done here a.t.m. - TODO - improve this area */
          gchar *utf8_title = aosd_trigger_utf8convert( pl_entry_title );
          if ( g_utf8_validate( utf8_title , -1 , NULL ) == TRUE )
          {
            gchar *utf8_title_markup = g_markup_printf_escaped(
              "<span font_desc='%s'>%s</span>" , global_config->osd->text.fonts_name[0] , utf8_title );
            aosd_osd_display( utf8_title_markup , global_config->osd , FALSE );
            g_free( utf8_title_markup );
          }
          g_free( utf8_title );
          g_free( prevs->title );
          prevs->title = g_strdup(pl_entry_title);
        }
      }
      else
      {
        g_free(prevs->filename);
        prevs->filename = g_strdup(pl_entry_filename);
        /* if filename changes, reset title as well */
        if ( prevs->title != NULL )
          g_free(prevs->title);
        prevs->title = g_strdup(pl_entry_title);
      }
    }
    else
    {
      if ( prevs->title != NULL )
        g_free(prevs->title);
      prevs->title = g_strdup(pl_entry_title);
      if ( prevs->filename != NULL )
        g_free(prevs->filename);
      prevs->filename = g_strdup(pl_entry_filename);
    }

    g_free (pl_entry_filename);
    g_free (pl_entry_title);
  }
}

static void
aosd_trigger_func_vol_change_onoff ( gboolean turn_on )
{
  if ( turn_on == TRUE )
    hook_associate( "volume set" , aosd_trigger_func_vol_change_cb , NULL );
  else
    hook_dissociate( "volume set" , aosd_trigger_func_vol_change_cb );
  return;
}

typedef struct
{
  gint h_vol[2];
  gint sid;
}
aosd_vol_change_bucket_t;

static gboolean
aosd_trigger_func_vol_change_timeout ( gpointer bucket_gp )
{
  aosd_vol_change_bucket_t *bucket = bucket_gp;
  gchar *utf8_title_markup = g_markup_printf_escaped(
    "<span font_desc='%s'>Volume Change - L: %i , R: %i</span>" ,
    global_config->osd->text.fonts_name[0] , bucket->h_vol[0] , bucket->h_vol[1] );
  aosd_osd_display( utf8_title_markup , global_config->osd , FALSE );
  g_free( utf8_title_markup );
  bucket->sid = 0; /* reset source id value */
  return FALSE;
}

static void
aosd_trigger_func_vol_change_cb ( gpointer h_vol_gp , gpointer unused )
{
  gint *h_vol = h_vol_gp;
  static aosd_vol_change_bucket_t bucket = { { 0 , 0 } , 0 };

  bucket.h_vol[0] = h_vol[0];
  bucket.h_vol[1] = h_vol[1];

  /* in order to avoid repeated display of osd for each volume variation, use a
     timer to prevent it from appearing more than once when multiple volume
     changes are performed in a short time interval (500 msec) */
  if ( bucket.sid == 0 )
  {
    /* first call in the time interval */
    bucket.sid = g_timeout_add( 500 , aosd_trigger_func_vol_change_timeout , &bucket );
  }
  else
  {
    /* another call in the same interval, reset the interval */
    g_source_remove( bucket.sid );
    bucket.sid = g_timeout_add( 500 , aosd_trigger_func_vol_change_timeout , &bucket );
  }
  return;
}


static void
aosd_trigger_func_pb_pauseon_onoff ( gboolean turn_on )
{
  if ( turn_on == TRUE )
    hook_associate( "playback pause" , aosd_trigger_func_pb_pauseon_cb , NULL );
  else
    hook_dissociate( "playback pause" , aosd_trigger_func_pb_pauseon_cb );
  return;
}

static void
aosd_trigger_func_pb_pauseon_cb ( gpointer unused1 , gpointer unused2 )
{
  gchar *utf8_title_markup = g_markup_printf_escaped(
    "<span font_desc='%s'>Paused</span>" , global_config->osd->text.fonts_name[0] );
  aosd_osd_display( utf8_title_markup , global_config->osd , FALSE );
  g_free( utf8_title_markup );
  return;
}


static void
aosd_trigger_func_pb_pauseoff_onoff ( gboolean turn_on )
{
  if ( turn_on == TRUE )
    hook_associate( "playback unpause" , aosd_trigger_func_pb_pauseoff_cb , NULL );
  else
    hook_dissociate( "playback unpause" , aosd_trigger_func_pb_pauseoff_cb );
  return;
}

static void
aosd_trigger_func_pb_pauseoff_cb ( gpointer unused1 , gpointer unused2 )
{
  gint active = aud_playlist_get_active();
  gint pos = aud_playlist_get_position(active);
  gchar *utf8_title, *utf8_title_markup;
  gint time_cur, time_tot;
  gint time_cur_m, time_cur_s, time_tot_m, time_tot_s;

  time_tot = aud_playlist_entry_get_length (active, pos, FALSE) / 1000;
  time_cur = aud_drct_get_time() / 1000;
  time_cur_s = time_cur % 60;
  time_cur_m = (time_cur - time_cur_s) / 60;
  time_tot_s = time_tot % 60;
  time_tot_m = (time_tot - time_tot_s) / 60;

  utf8_title = aud_playlist_entry_get_title (active, pos, FALSE);
  utf8_title_markup = g_markup_printf_escaped(
    "<span font_desc='%s'>%s (%i:%02i/%i:%02i)</span>" ,
    global_config->osd->text.fonts_name[0] , utf8_title , time_cur_m , time_cur_s , time_tot_m , time_tot_s );
  aosd_osd_display( utf8_title_markup , global_config->osd , FALSE );
  g_free( utf8_title_markup );
  g_free( utf8_title );
  return;
}


/* Call with hook_call("aosd toggle", param);
   If param != NULL, display the supplied text in the OSD
   If param == NULL, display the current playing song */
static void
aosd_trigger_func_hook_cb ( gpointer markup_text , gpointer unused )
{
  if ( markup_text != NULL )
  {
    /* Display text from caller */
    aosd_osd_display( markup_text , global_config->osd , FALSE );
  } else {
    /* Display currently playing song */
    aosd_trigger_func_pb_start_cb ( GINT_TO_POINTER(aud_drct_pl_get_pos()), NULL );
  }
  return;
}
