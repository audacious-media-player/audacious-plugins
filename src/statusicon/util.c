/*
* Status Icon Plugin for Audacious
*
* Copyright 2005-2007 Giacomo Lozito <james@develia.org>
* Copyright 2010 Michał Lipski <tallica@o2.pl>
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

#include <audacious/drct.h>
#include <libaudgui/libaudgui.h>

#include "statusicon.h"

void si_volume_change(gint value)
{
    gint vl, vr;
    aud_drct_get_volume(&vl, &vr);
    aud_drct_set_volume(CLAMP(vl + value, 0, 100), CLAMP(vr + value, 0, 100));
}

void si_playback_skip(gint numsong)
{
    gpointer ctrl_code_gp = NULL;
    gint i = 0;

    if (numsong >= 0)
    {
        ctrl_code_gp = GINT_TO_POINTER(SI_PLAYBACK_CTRL_NEXT);
    }
    else
    {
        ctrl_code_gp = GINT_TO_POINTER(SI_PLAYBACK_CTRL_PREV);
        numsong *= -1;
    }

    for (i = 0; i < numsong; i++)
        si_playback_ctrl(ctrl_code_gp);
}

void si_playback_ctrl(gpointer ctrl_code_gp)
{
    gint ctrl_code = GPOINTER_TO_INT(ctrl_code_gp);

    switch (ctrl_code)
    {
      case SI_PLAYBACK_CTRL_PREV:
          aud_drct_pl_prev();
          break;

      case SI_PLAYBACK_CTRL_PLAY:
          if (aud_drct_get_playing())
              aud_drct_pause();
          else
              aud_drct_play();
          break;

      case SI_PLAYBACK_CTRL_PAUSE:
          aud_drct_pause();
          break;

      case SI_PLAYBACK_CTRL_STOP:
          aud_drct_stop();
          break;

      case SI_PLAYBACK_CTRL_NEXT:
          aud_drct_pl_next();
          break;

      case SI_PLAYBACK_CTRL_EJECT:
          audgui_run_filebrowser(TRUE);
          break;
    }
}
