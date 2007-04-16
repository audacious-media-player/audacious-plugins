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

#include "si.h"
#include "si_ui.h"
#include "si_audacious.h"
#include "si_cfg.h"
#include "si_common.h"
#include <audacious/beepctrl.h>


static gboolean plugin_active = FALSE;

extern si_cfg_t si_cfg;


/* ***************** */
/* plug-in functions */

GeneralPlugin *get_gplugin_info()
{
   return &si_gp;
}


void
si_init ( void )
{
  g_type_init();
  g_log_set_handler( NULL , G_LOG_LEVEL_WARNING , g_log_default_handler , NULL );
  plugin_active = TRUE;
  si_cfg_load();
  si_ui_statusicon_enable( TRUE );
  return;
}


void
si_cleanup ( void )
{
  if ( plugin_active == TRUE )
  {
    plugin_active = FALSE;
    si_ui_statusicon_enable( FALSE );
    si_cfg_save(); /* required to save windows visibility status */
  }
  return;
}


void
si_prefs ( void )
{
  si_ui_prefs_show();
}


void
si_about ( void )
{
  si_ui_about_show();
}



/* ***************** */
/* audacious actions */


void
si_audacious_toggle_visibility ( void )
{
  /* use the window visibility status to toggle show/hide
     (if at least one is visible, hide) */
  if (( xmms_remote_is_main_win( si_gp.xmms_session ) == TRUE ) ||
      ( xmms_remote_is_eq_win( si_gp.xmms_session ) == TRUE ) ||
      ( xmms_remote_is_pl_win( si_gp.xmms_session ) == TRUE ))
  {
    /* remember the visibility status of the player windows */
    si_cfg.mw_visib_prevstatus = xmms_remote_is_main_win( si_gp.xmms_session );
    si_cfg.ew_visib_prevstatus = xmms_remote_is_eq_win( si_gp.xmms_session );
    si_cfg.pw_visib_prevstatus = xmms_remote_is_pl_win( si_gp.xmms_session );
    /* now hide all of them */
    if ( si_cfg.mw_visib_prevstatus == TRUE )
      xmms_remote_main_win_toggle( si_gp.xmms_session , FALSE );
    if ( si_cfg.ew_visib_prevstatus == TRUE )
      xmms_remote_eq_win_toggle( si_gp.xmms_session , FALSE );
    if ( si_cfg.pw_visib_prevstatus == TRUE )
      xmms_remote_pl_win_toggle( si_gp.xmms_session , FALSE );
  }
  else
  {
    /* show the windows that were visible before */
    if ( si_cfg.mw_visib_prevstatus == TRUE )
      xmms_remote_main_win_toggle( si_gp.xmms_session , TRUE );
    if ( si_cfg.ew_visib_prevstatus == TRUE )
      xmms_remote_eq_win_toggle( si_gp.xmms_session , TRUE );
    if ( si_cfg.pw_visib_prevstatus == TRUE )
      xmms_remote_pl_win_toggle( si_gp.xmms_session , TRUE );
  }
}

void
si_audacious_volume_change ( gint value )
{
  gint vl, vr;
  xmms_remote_get_volume( si_gp.xmms_session , &vl , &vr );
  xmms_remote_set_volume( si_gp.xmms_session ,
    CLAMP(vl + value, 0, 100) , CLAMP(vr + value, 0, 100) );
}

void
si_audacious_quit ( void )
{
  xmms_remote_quit( si_gp.xmms_session );
}

void
si_audacious_playback_skip ( gint numsong )
{
  gpointer ctrl_code_gp = NULL;
  gint i = 0;

  if ( numsong >= 0 )
  {
    ctrl_code_gp = GINT_TO_POINTER(SI_AUDACIOUS_PLAYBACK_CTRL_NEXT);
  }
  else
  {
    ctrl_code_gp = GINT_TO_POINTER(SI_AUDACIOUS_PLAYBACK_CTRL_PREV);
    numsong *= -1;
  }

  for ( i = 0 ; i < numsong ; i++ )
    si_audacious_playback_ctrl( ctrl_code_gp );
}

void
si_audacious_playback_ctrl ( gpointer ctrl_code_gp )
{
  gint ctrl_code = GPOINTER_TO_INT(ctrl_code_gp);
  switch ( ctrl_code )
  {
    case SI_AUDACIOUS_PLAYBACK_CTRL_PREV:
      xmms_remote_playlist_prev( si_gp.xmms_session );
      break;

    case SI_AUDACIOUS_PLAYBACK_CTRL_PLAY:
      xmms_remote_play( si_gp.xmms_session );
      break;

    case SI_AUDACIOUS_PLAYBACK_CTRL_PAUSE:
      xmms_remote_pause( si_gp.xmms_session );
      break;

    case SI_AUDACIOUS_PLAYBACK_CTRL_STOP:
      xmms_remote_stop( si_gp.xmms_session );
      break;

    case SI_AUDACIOUS_PLAYBACK_CTRL_NEXT:
      xmms_remote_playlist_next( si_gp.xmms_session );
      break;

    case SI_AUDACIOUS_PLAYBACK_CTRL_EJECT:
      xmms_remote_eject( si_gp.xmms_session );
      break;
  }
}
