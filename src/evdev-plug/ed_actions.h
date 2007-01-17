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

#ifndef _I_ED_ACTIONS_H
#define _I_ED_ACTIONS_H 1

#include "ed_common.h"
#include <glib.h>

/* ed_action_t object structure */
typedef struct
{
  const gchar * desc;
  void (*callback)( gpointer );
}
ed_action_t;

/* action codes */
enum
{
  ED_ACTION_PB_PLAY = 0,
  ED_ACTION_PB_STOP = 1,
  ED_ACTION_PB_PAUSE = 2,
  ED_ACTION_PB_PREV = 3,
  ED_ACTION_PB_NEXT = 4,
  ED_ACTION_PB_EJECT = 5,

  ED_ACTION_PL_REPEAT = 10,
  ED_ACTION_PL_SHUFFLE = 11,

  ED_ACTION_VOL_UP5 = 20,
  ED_ACTION_VOL_DOWN5 = 21,
  ED_ACTION_VOL_UP10 = 22,
  ED_ACTION_VOL_DOWN10 = 23,
  ED_ACTION_VOL_MUTE = 24,

  ED_ACTION_WIN_MAIN = 30,
  ED_ACTION_WIN_PLAYLIST = 31,
  ED_ACTION_WIN_EQUALIZER = 32,
  ED_ACTION_WIN_JTF = 33
};

void ed_action_call ( gint , gpointer );

#endif /* !_I_ED_ACTIONS_H */
