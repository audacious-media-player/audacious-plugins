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
#include <audacious/auddrct.h>

GeneralPlugin *statusicon_gplist[] = { &si_gp, NULL };
SIMPLE_GENERAL_PLUGIN(statusicon, statusicon_gplist);

static gboolean plugin_active = FALSE;

extern si_cfg_t si_cfg;


/* ***************** */
/* plug-in functions */

void
si_init ( void )
{
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
  aud_interface_toggle_visibility();
}

void
si_audacious_volume_change ( gint value )
{
  gint vl, vr;
  audacious_drct_get_volume( &vl , &vr );
  audacious_drct_set_volume( CLAMP(vl + value, 0, 100) , CLAMP(vr + value, 0, 100) );
}

void
si_audacious_quit ( void )
{
  audacious_drct_quit();
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
      audacious_drct_pl_prev();
      break;

    case SI_AUDACIOUS_PLAYBACK_CTRL_PLAY:
      audacious_drct_play();
      break;

    case SI_AUDACIOUS_PLAYBACK_CTRL_PAUSE:
      audacious_drct_pause();
      break;

    case SI_AUDACIOUS_PLAYBACK_CTRL_STOP:
      audacious_drct_stop();
      break;

    case SI_AUDACIOUS_PLAYBACK_CTRL_NEXT:
      audacious_drct_pl_next();
      break;

    case SI_AUDACIOUS_PLAYBACK_CTRL_EJECT:
      audacious_drct_eject();
      break;
  }
}

void
si_audacious_toggle_playback ( void )
{
  if ( audacious_drct_get_playing() )
    audacious_drct_pause();
  else
    audacious_drct_play();
}
