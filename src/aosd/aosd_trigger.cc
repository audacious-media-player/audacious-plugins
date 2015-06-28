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
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>

#include "aosd_trigger.h"
#include "aosd_trigger_private.h"
#include "aosd_cfg.h"
#include "aosd_osd.h"

/* prototypes of trigger functions */
static void aosd_trigger_func_pb_start_onoff ( bool );
static void aosd_trigger_func_pb_start_cb ( void * , void * );
static void aosd_trigger_func_pb_titlechange_onoff ( bool );
static void aosd_trigger_func_pb_titlechange_cb ( void * , void * );
static void aosd_trigger_func_pb_pauseon_onoff ( bool );
static void aosd_trigger_func_pb_pauseon_cb ( void * , void * );
static void aosd_trigger_func_pb_pauseoff_onoff ( bool );
static void aosd_trigger_func_pb_pauseoff_cb ( void * , void * );
static void aosd_trigger_func_hook_cb ( void * markup_text , void * unused );

/* map trigger codes to trigger objects */
aosd_trigger_t aosd_triggers[] =
{
  // AOSD_TRIGGER_PB_START
  { N_("Playback Start") ,
    N_("Triggers OSD when a playlist entry is played.") ,
    aosd_trigger_func_pb_start_onoff ,
    aosd_trigger_func_pb_start_cb },

  // AOSD_TRIGGER_PB_TITLECHANGE
  { N_("Title Change") ,
    N_("Triggers OSD when the song title changes (for internet streams).") ,
    aosd_trigger_func_pb_titlechange_onoff ,
    aosd_trigger_func_pb_titlechange_cb },

  // AOSD_TRIGGER_PB_PAUSEON
  { N_("Pause On") ,
    N_("Triggers OSD when playback is paused.") ,
    aosd_trigger_func_pb_pauseon_onoff ,
    aosd_trigger_func_pb_pauseon_cb },

  // AOSD_TRIGGER_PB_PAUSEOFF
  { N_("Pause Off") ,
    N_("Triggers OSD when playback is unpaused.") ,
    aosd_trigger_func_pb_pauseoff_onoff ,
    aosd_trigger_func_pb_pauseoff_cb }
};

static_assert (aud::n_elems (aosd_triggers) == AOSD_NUM_TRIGGERS, "update aosd_triggers");

/* TRIGGER API */

const char *
aosd_trigger_get_name ( int trig_code )
{
  if (trig_code >= 0 && trig_code < aud::n_elems (aosd_triggers))
    return aosd_triggers[trig_code].name;
  return nullptr;
}


const char *
aosd_trigger_get_desc ( int trig_code )
{
  if (trig_code >= 0 && trig_code < aud::n_elems (aosd_triggers))
    return aosd_triggers[trig_code].desc;
  return nullptr;
}


void aosd_trigger_start (const aosd_cfg_osd_trigger_t & cfg_trigger)
{
  for (int i = 0; i < AOSD_NUM_TRIGGERS; i ++)
  {
    if (cfg_trigger.enabled[i])
      aosd_triggers[i].onoff_func (true);
  }

  /* When called, this hook will display the text of the user pointer
     or the current playing song, if nullptr */
  hook_associate( "aosd toggle" , aosd_trigger_func_hook_cb , nullptr );
}


void aosd_trigger_stop (const aosd_cfg_osd_trigger_t & cfg_trigger)
{
  hook_dissociate( "aosd toggle" , aosd_trigger_func_hook_cb );

  for (int i = 0; i < AOSD_NUM_TRIGGERS; i ++)
  {
    if (cfg_trigger.enabled[i])
      aosd_triggers[i].onoff_func (false);
  }
}


/* TRIGGER FUNCTIONS */

static void
aosd_trigger_func_pb_start_onoff ( bool turn_on )
{
  if (turn_on == true)
    hook_associate("playback ready", aosd_trigger_func_pb_start_cb, nullptr);
  else
    hook_dissociate("playback ready", aosd_trigger_func_pb_start_cb);
}

static void
aosd_trigger_func_pb_start_cb(void * hook_data, void * user_data)
{
  String title = aud_drct_get_title ();
  char * markup = g_markup_printf_escaped ("<span font_desc='%s'>%s</span>",
   (const char *) global_config.text.fonts_name[0], (const char *) title);

  aosd_osd_display (markup, & global_config, false);
  g_free (markup);
}

typedef struct
{
  String title;
  String filename;
}
aosd_pb_titlechange_prevs_t;

static void
aosd_trigger_func_pb_titlechange_onoff ( bool turn_on )
{
  static aosd_pb_titlechange_prevs_t *prevs = nullptr;

  if ( turn_on == true )
  {
    prevs = new aosd_pb_titlechange_prevs_t;
    hook_associate( "title change" , aosd_trigger_func_pb_titlechange_cb , prevs );
  }
  else
  {
    hook_dissociate( "title change" , aosd_trigger_func_pb_titlechange_cb );
    if ( prevs != nullptr )
    {
      delete prevs;
      prevs = nullptr;
    }
  }
}

static void
aosd_trigger_func_pb_titlechange_cb ( void * plentry_gp , void * prevs_gp )
{
  if (aud_drct_get_playing ())
  {
    aosd_pb_titlechange_prevs_t *prevs = (aosd_pb_titlechange_prevs_t *) prevs_gp;
    String pl_entry_filename = aud_drct_get_filename ();
    Tuple pl_entry_tuple = aud_drct_get_tuple ();
    String pl_entry_title = pl_entry_tuple.get_str (Tuple::FormattedTitle);

    /* same filename but title changed, useful to detect http stream song changes */

    if ( ( prevs->title != nullptr ) && ( prevs->filename != nullptr ) )
    {
      if ( ( pl_entry_filename != nullptr ) && ( !strcmp(pl_entry_filename,prevs->filename) ) )
      {
        if ( ( pl_entry_title != nullptr ) && ( strcmp(pl_entry_title,prevs->title) ) )
        {
          /* string formatting is done here a.t.m. - TODO - improve this area */
          char * markup = g_markup_printf_escaped
           ("<span font_desc='%s'>%s</span>",
           (const char *) global_config.text.fonts_name[0],
           (const char *) pl_entry_title);

          aosd_osd_display (markup, & global_config, false);
          g_free (markup);

          prevs->title = pl_entry_title;
        }
      }
      else
      {
        prevs->filename = pl_entry_filename;
        /* if filename changes, reset title as well */
        prevs->title = pl_entry_title;
      }
    }
    else
    {
      prevs->title = pl_entry_title;
      prevs->filename = pl_entry_filename;
    }
  }
}

static void
aosd_trigger_func_pb_pauseon_onoff ( bool turn_on )
{
  if ( turn_on == true )
    hook_associate( "playback pause" , aosd_trigger_func_pb_pauseon_cb , nullptr );
  else
    hook_dissociate( "playback pause" , aosd_trigger_func_pb_pauseon_cb );
}

static void
aosd_trigger_func_pb_pauseon_cb ( void * unused1 , void * unused2 )
{
  char * markup = g_markup_printf_escaped ("<span font_desc='%s'>Paused</span>",
   (const char *) global_config.text.fonts_name[0]);
  aosd_osd_display (markup, & global_config, false);
  g_free (markup);
}


static void
aosd_trigger_func_pb_pauseoff_onoff ( bool turn_on )
{
  if ( turn_on == true )
    hook_associate( "playback unpause" , aosd_trigger_func_pb_pauseoff_cb , nullptr );
  else
    hook_dissociate( "playback unpause" , aosd_trigger_func_pb_pauseoff_cb );
}

static void
aosd_trigger_func_pb_pauseoff_cb ( void * unused1 , void * unused2 )
{
  Tuple tuple = aud_drct_get_tuple ();
  int time_cur, time_tot;
  int time_cur_m, time_cur_s, time_tot_m, time_tot_s;

  time_tot = tuple.get_int (Tuple::Length) / 1000;
  time_cur = aud_drct_get_time() / 1000;
  time_cur_s = time_cur % 60;
  time_cur_m = (time_cur - time_cur_s) / 60;
  time_tot_s = time_tot % 60;
  time_tot_m = (time_tot - time_tot_s) / 60;

  String title = tuple.get_str (Tuple::FormattedTitle);
  char * markup = g_markup_printf_escaped
   ("<span font_desc='%s'>%s (%i:%02i/%i:%02i)</span>",
   (const char *) global_config.text.fonts_name[0], (const char *) title,
   time_cur_m, time_cur_s, time_tot_m, time_tot_s);

  aosd_osd_display (markup, & global_config, false);
  g_free (markup);
}


/* Call with hook_call("aosd toggle", param);
   If param != nullptr, display the supplied text in the OSD
   If param == nullptr, display the current playing song */
static void
aosd_trigger_func_hook_cb ( void * markup_text , void * unused )
{
  if ( markup_text != nullptr )
  {
    /* Display text from caller */
    aosd_osd_display ((char *) markup_text, & global_config, false);
  } else {
    /* Display currently playing song */
    aosd_trigger_func_pb_start_cb (nullptr, nullptr);
  }
}
