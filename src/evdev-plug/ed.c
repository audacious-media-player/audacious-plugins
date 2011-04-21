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

#include "ed.h"
#include "ed_types.h"
#include "ed_internals.h"
#include "ed_actions.h"
#include "ed_ui.h"
#include "ed_common.h"

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <libaudcore/hook.h>

AUD_GENERAL_PLUGIN
(
    .name = "EvDev-Plug",
    .init = ed_init,
    .about = ed_about,
    .configure = ed_config,
    .cleanup = ed_cleanup
)

GList *ed_device_listening_list = NULL;
gboolean plugin_is_active = FALSE;

/* action callbacks */
void ed_action_pb_play ( gpointer );
void ed_action_pb_stop ( gpointer );
void ed_action_pb_pause ( gpointer );
void ed_action_pb_prev ( gpointer );
void ed_action_pb_next ( gpointer );
void ed_action_pb_eject ( gpointer );
void ed_action_vol_up5 ( gpointer );
void ed_action_vol_down5 ( gpointer );
void ed_action_vol_up10 ( gpointer );
void ed_action_vol_down10 ( gpointer );
void ed_action_vol_mute ( gpointer );
void ed_action_win_main ( gpointer );
void ed_action_win_playlist ( gpointer );
void ed_action_win_equalizer ( gpointer );
void ed_action_win_jtf ( gpointer );
void ed_action_pl_repeat ( gpointer );
void ed_action_pl_shuffle ( gpointer );

/* map action codes to ed_action_t objects */
ed_action_t player_actions[] =
{
  [ED_ACTION_PB_PLAY] = { N_("Playback->Play") , ed_action_pb_play },
  [ED_ACTION_PB_STOP] = { N_("Playback->Stop") , ed_action_pb_stop },
  [ED_ACTION_PB_PAUSE] = { N_("Playback->Pause") , ed_action_pb_pause },
  [ED_ACTION_PB_PREV] = { N_("Playback->Prev") , ed_action_pb_prev },
  [ED_ACTION_PB_NEXT] = { N_("Playback->Next") , ed_action_pb_next },
  [ED_ACTION_PB_EJECT] = { N_("Playback->Eject") , ed_action_pb_eject },

  [ED_ACTION_PL_REPEAT] = { N_("Playlist->Repeat") , ed_action_pl_repeat },
  [ED_ACTION_PL_SHUFFLE] = { N_("Playlist->Shuffle") , ed_action_pl_shuffle },

  [ED_ACTION_VOL_UP5] = { N_("Volume->Up_5") , ed_action_vol_up5 },
  [ED_ACTION_VOL_DOWN5] = { N_("Volume->Down_5") , ed_action_vol_down5 },
  [ED_ACTION_VOL_UP10] = { N_("Volume->Up_10") , ed_action_vol_up10 },
  [ED_ACTION_VOL_DOWN10] = { N_("Volume->Down_10") , ed_action_vol_down10 },
  [ED_ACTION_VOL_MUTE] = { N_("Volume->Mute") , ed_action_vol_mute },

  [ED_ACTION_WIN_MAIN] = { N_("Window->Main") , ed_action_win_main },
  [ED_ACTION_WIN_PLAYLIST] = { N_("Window->Playlist") , ed_action_win_playlist },
  [ED_ACTION_WIN_EQUALIZER] = { N_("Window->Equalizer") , ed_action_win_equalizer },
  [ED_ACTION_WIN_JTF] = { N_("Window->JumpToFile") , ed_action_win_jtf }
};



/* ***************** */
/* plug-in functions */

gboolean ed_init (void)
{
  g_log_set_handler( NULL , G_LOG_LEVEL_WARNING , g_log_default_handler , NULL );

  plugin_is_active = TRUE; /* go! */

  /* read event devices and bindings from user
     configuration and start listening for active ones */
  ed_device_start_listening_from_config();

  return TRUE;
}


void
ed_cleanup ( void )
{
  /* shut down all devices being listened */
  ed_device_stop_listening_all( TRUE );

  plugin_is_active = FALSE; /* stop! */

  return;
}


void
ed_config ( void )
{
  ed_ui_config_show();
}


void
ed_about ( void )
{
  ed_ui_about_show();
}



/* ************** */
/* player actions */

void
ed_action_call ( gint code , gpointer param )
{
  DEBUGMSG( "Calling action; code %i ( %s )\n" , code , player_actions[code].desc );

  /* activate callback for requested action */
  player_actions[code].callback( param );
}


void
ed_action_pb_play ( gpointer param )
{
  aud_drct_play();
}

void
ed_action_pb_stop ( gpointer param )
{
  aud_drct_stop();
}

void
ed_action_pb_pause ( gpointer param )
{
  if (aud_drct_get_playing() || aud_drct_get_paused())
    aud_drct_pause();
  else
    aud_drct_play();
}

void
ed_action_pb_prev ( gpointer param )
{
  aud_drct_pl_prev();
}

void
ed_action_pb_next ( gpointer param )
{
  aud_drct_pl_next();
}

void
ed_action_pb_eject ( gpointer param )
{
  hook_call ("filebrowser show", GINT_TO_POINTER (TRUE));
}

void
ed_action_pl_repeat ( gpointer param )
{
  aud_drct_pl_repeat_toggle();
}

void
ed_action_pl_shuffle ( gpointer param )
{
  aud_drct_pl_shuffle_toggle();
}

void
ed_action_vol_up5 ( gpointer param )
{
  gint vl, vr;
  aud_drct_get_volume( &vl , &vr );
  aud_drct_set_volume( CLAMP(vl + 5, 0, 100) , CLAMP(vr + 5, 0, 100) );
}

void
ed_action_vol_down5 ( gpointer param )
{
  gint vl, vr;
  aud_drct_get_volume( &vl , &vr );
  aud_drct_set_volume( CLAMP(vl - 5, 0, 100) , CLAMP(vr - 5, 0, 100) );
}

void
ed_action_vol_up10 ( gpointer param )
{
  gint vl, vr;
  aud_drct_get_volume( &vl , &vr );
  aud_drct_set_volume( CLAMP(vl + 10, 0, 100) , CLAMP(vr + 10, 0, 100) );
}

void
ed_action_vol_down10 ( gpointer param )
{
  gint vl, vr;
  aud_drct_get_volume( &vl , &vr );
  aud_drct_set_volume( CLAMP(vl - 10, 0, 100) , CLAMP(vr - 10, 0, 100) );
}

void
ed_action_vol_mute ( gpointer param )
{
  static gint vl = -1;
  static gint vr = -1;

  if ( vl == -1 ) /* no previous memory of volume before mute action */
  {
    aud_drct_get_volume( &vl , &vr ); /* memorize volume before mute */
    aud_drct_set_volume( 0 , 0 ); /* mute */
  }
  else /* memorized volume values exist */
  {
    gint vl_now = 0;
    gint vr_now = 0;

    aud_drct_get_volume( &vl_now , &vr_now );
    if (( vl_now == 0 ) && ( vr_now == 0 ))
    {
      /* the volume is still muted, restore the old values */
      aud_drct_set_volume( vl , vr );
      vl = -1; vr = -1; /* reset these for next use */
    }
    else
    {
      /* the volume has been raised with other commands, act as if there wasn't a previous memory */
      aud_drct_get_volume( &vl , &vr ); /* memorize volume before mute */
      aud_drct_set_volume( 0 , 0 ); /* mute */
    }
  }
}

void
ed_action_win_main ( gpointer param )
{
  hook_call ("interface toggle visibility", NULL);
}

void
ed_action_win_playlist ( gpointer param )
{
#if 0
  aud_drct_pl_win_toggle( !aud_drct_pl_win_is_visible () );
#endif
}

void
ed_action_win_equalizer ( gpointer param )
{
#if 0
  aud_drct_eq_win_toggle( !aud_drct_eq_win_is_visible () );
#endif
}

void
ed_action_win_jtf ( gpointer param )
{
  hook_call ("interface show jump to track", NULL);
}
