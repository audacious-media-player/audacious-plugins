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

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>
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
  AOSD_TRIGGER_PB_PAUSEON = 2,
  AOSD_TRIGGER_PB_PAUSEOFF = 3
};

/* trigger codes array size */
#define AOSD_TRIGGER_CODES_ARRAY_SIZE 4

/* trigger codes array */
gint aosd_trigger_codes[] =
{
  AOSD_TRIGGER_PB_START,
  AOSD_TRIGGER_PB_TITLECHANGE,
  AOSD_TRIGGER_PB_PAUSEON,
  AOSD_TRIGGER_PB_PAUSEOFF
};

/* prototypes of trigger functions */
static void aosd_trigger_func_pb_start_onoff ( gboolean );
static void aosd_trigger_func_pb_start_cb ( gpointer , gpointer );
static void aosd_trigger_func_pb_titlechange_onoff ( gboolean );
static void aosd_trigger_func_pb_titlechange_cb ( gpointer , gpointer );
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
}


const gchar *
aosd_trigger_get_name ( gint trig_code )
{
  if (trig_code >= 0 && trig_code < ARRAY_LEN (aosd_triggers))
    return aosd_triggers[trig_code].name;
  return NULL;
}


const gchar *
aosd_trigger_get_desc ( gint trig_code )
{
  if (trig_code >= 0 && trig_code < ARRAY_LEN (aosd_triggers))
    return aosd_triggers[trig_code].desc;
  return NULL;
}


void
aosd_trigger_start ( aosd_cfg_osd_trigger_t * cfg_trigger )
{
  gint i = 0;
  for ( i = 0 ; i < cfg_trigger->active->len ; i++ )
  {
    gint trig_code = g_array_index( cfg_trigger->active , gint , i );
    if (trig_code >= 0 && trig_code < ARRAY_LEN (aosd_triggers))
      aosd_triggers[trig_code].onoff_func (TRUE);
  }
  /* When called, this hook will display the text of the user pointer
     or the current playing song, if NULL */
  hook_associate( "aosd toggle" , aosd_trigger_func_hook_cb , NULL );
}


void
aosd_trigger_stop ( aosd_cfg_osd_trigger_t * cfg_trigger )
{
  gint i = 0;
  hook_dissociate( "aosd toggle" , aosd_trigger_func_hook_cb );
  for ( i = 0 ; i < cfg_trigger->active->len ; i++ )
  {
    gint trig_code = g_array_index( cfg_trigger->active , gint , i );
    if (trig_code >= 0 && trig_code < ARRAY_LEN (aosd_triggers))
      aosd_triggers[trig_code].onoff_func (FALSE);
  }
}


/* TRIGGER FUNCTIONS */

static void
aosd_trigger_func_pb_start_onoff(gboolean turn_on)
{
  if (turn_on == TRUE)
    hook_associate("playback begin", aosd_trigger_func_pb_start_cb, NULL);
  else
    hook_dissociate("playback begin", aosd_trigger_func_pb_start_cb);
}

static void
aosd_trigger_func_pb_start_cb(gpointer hook_data, gpointer user_data)
{
  char * title = aud_drct_get_title ();
  char * markup = g_markup_printf_escaped ("<span font_desc='%s'>%s</span>",
   global_config->osd->text.fonts_name[0], title);

  aosd_osd_display (markup, global_config->osd, FALSE);
  g_free (markup);
  str_unref (title);
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
          char * markup = g_markup_printf_escaped
           ("<span font_desc='%s'>%s</span>",
           global_config->osd->text.fonts_name[0], pl_entry_title);

          aosd_osd_display (markup, global_config->osd, FALSE);
          g_free (markup);

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

    str_unref (pl_entry_filename);
    str_unref (pl_entry_title);
  }
}

static void
aosd_trigger_func_pb_pauseon_onoff ( gboolean turn_on )
{
  if ( turn_on == TRUE )
    hook_associate( "playback pause" , aosd_trigger_func_pb_pauseon_cb , NULL );
  else
    hook_dissociate( "playback pause" , aosd_trigger_func_pb_pauseon_cb );
}

static void
aosd_trigger_func_pb_pauseon_cb ( gpointer unused1 , gpointer unused2 )
{
  char * markup = g_markup_printf_escaped ("<span font_desc='%s'>Paused</span>",
   global_config->osd->text.fonts_name[0]);
  aosd_osd_display (markup, global_config->osd, FALSE);
  g_free (markup);
}


static void
aosd_trigger_func_pb_pauseoff_onoff ( gboolean turn_on )
{
  if ( turn_on == TRUE )
    hook_associate( "playback unpause" , aosd_trigger_func_pb_pauseoff_cb , NULL );
  else
    hook_dissociate( "playback unpause" , aosd_trigger_func_pb_pauseoff_cb );
}

static void
aosd_trigger_func_pb_pauseoff_cb ( gpointer unused1 , gpointer unused2 )
{
  gint active = aud_playlist_get_active();
  gint pos = aud_playlist_get_position(active);
  gint time_cur, time_tot;
  gint time_cur_m, time_cur_s, time_tot_m, time_tot_s;

  time_tot = aud_playlist_entry_get_length (active, pos, FALSE) / 1000;
  time_cur = aud_drct_get_time() / 1000;
  time_cur_s = time_cur % 60;
  time_cur_m = (time_cur - time_cur_s) / 60;
  time_tot_s = time_tot % 60;
  time_tot_m = (time_tot - time_tot_s) / 60;

  char * title = aud_playlist_entry_get_title (active, pos, FALSE);
  char * markup = g_markup_printf_escaped
   ("<span font_desc='%s'>%s (%i:%02i/%i:%02i)</span>",
   global_config->osd->text.fonts_name[0], title, time_cur_m, time_cur_s,
   time_tot_m, time_tot_s);

  aosd_osd_display (markup, global_config->osd, FALSE);
  g_free (markup);
  str_unref (title);
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
    aosd_trigger_func_pb_start_cb (NULL, NULL);
  }
}
