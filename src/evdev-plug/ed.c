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
#include <glib/gi18n.h>
#include <audacious/beepctrl.h>


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
void ed_action_win_main ( gpointer );
void ed_action_win_playlist ( gpointer );
void ed_action_win_equalizer ( gpointer );
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

  [ED_ACTION_WIN_MAIN] = { N_("Window->Main") , ed_action_win_main },
  [ED_ACTION_WIN_PLAYLIST] = { N_("Window->Playlist") , ed_action_win_playlist },
  [ED_ACTION_WIN_EQUALIZER] = { N_("Window->Equalizer") , ed_action_win_equalizer }
};



/* ***************** */
/* plug-in functions */

GeneralPlugin *get_gplugin_info()
{
   return &ed_gp;
}


void
ed_init ( void )
{
  g_log_set_handler( NULL , G_LOG_LEVEL_WARNING , g_log_default_handler , NULL );

  plugin_is_active = TRUE; /* go! */

  /* read event devices and bindings from user
     configuration and start listening for active ones */
  ed_device_start_listening_from_config();

  return;
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
  xmms_remote_play( ed_gp.xmms_session );
}

void
ed_action_pb_stop ( gpointer param )
{
  xmms_remote_stop( ed_gp.xmms_session );
}

void
ed_action_pb_pause ( gpointer param )
{
  if (xmms_remote_is_playing( ed_gp.xmms_session ) || xmms_remote_is_paused( ed_gp.xmms_session ))
    xmms_remote_pause( ed_gp.xmms_session );
  else
    xmms_remote_play( ed_gp.xmms_session );
}

void
ed_action_pb_prev ( gpointer param )
{
  xmms_remote_playlist_prev( ed_gp.xmms_session );
}

void
ed_action_pb_next ( gpointer param )
{
  xmms_remote_playlist_next( ed_gp.xmms_session );
}

void
ed_action_pb_eject ( gpointer param )
{
  xmms_remote_eject( ed_gp.xmms_session );
}

void
ed_action_pl_repeat ( gpointer param )
{
  xmms_remote_toggle_repeat( ed_gp.xmms_session );
}

void
ed_action_pl_shuffle ( gpointer param )
{
  xmms_remote_toggle_shuffle( ed_gp.xmms_session );
}

void
ed_action_vol_up5 ( gpointer param )
{
  gint vl, vr;
  xmms_remote_get_volume( ed_gp.xmms_session , &vl , &vr );
  xmms_remote_set_volume( ed_gp.xmms_session , CLAMP(vl + 5, 0, 100) , CLAMP(vr + 5, 0, 100) );
}

void
ed_action_vol_down5 ( gpointer param )
{
  gint vl, vr;
  xmms_remote_get_volume( ed_gp.xmms_session , &vl , &vr );
  xmms_remote_set_volume( ed_gp.xmms_session , CLAMP(vl - 5, 0, 100) , CLAMP(vr - 5, 0, 100) );
}

void
ed_action_vol_up10 ( gpointer param )
{
  gint vl, vr;
  xmms_remote_get_volume( ed_gp.xmms_session , &vl , &vr );
  xmms_remote_set_volume( ed_gp.xmms_session , CLAMP(vl + 10, 0, 100) , CLAMP(vr + 10, 0, 100) );
}

void
ed_action_vol_down10 ( gpointer param )
{
  gint vl, vr;
  xmms_remote_get_volume( ed_gp.xmms_session , &vl , &vr );
  xmms_remote_set_volume( ed_gp.xmms_session , CLAMP(vl - 10, 0, 100) , CLAMP(vr - 10, 0, 100) );
}

void
ed_action_win_main ( gpointer param )
{
  xmms_remote_main_win_toggle( ed_gp.xmms_session ,
    !xmms_remote_is_main_win ( ed_gp.xmms_session ) );
}

void
ed_action_win_playlist ( gpointer param )
{
  xmms_remote_pl_win_toggle( ed_gp.xmms_session ,
    !xmms_remote_is_pl_win ( ed_gp.xmms_session ) );
}

void
ed_action_win_equalizer ( gpointer param )
{
  xmms_remote_eq_win_toggle( ed_gp.xmms_session ,
    !xmms_remote_is_eq_win ( ed_gp.xmms_session ) );
}
