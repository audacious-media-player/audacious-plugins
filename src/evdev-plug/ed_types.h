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

#ifndef _I_ED_TYPES_H
#define _I_ED_TYPES_H 1

#include "ed_common.h"
#include <glib.h>

typedef struct
{
  gchar * name;
  gchar * filename;
  gchar * phys;

  gint reg;

  gint is_custom;
  gboolean is_active;

  gpointer * bindings;
}
ed_device_info_t;

typedef struct
{
  gint fd;

  GIOChannel * iochan;
  guint iochan_sid;
  gboolean is_listening;

  ed_device_info_t * info;
}
ed_device_t;

typedef struct
{
  guint type;
  guint code;
  gint value;
}
ed_inputevent_t;

#endif /* !_I_ED_TYPES_H */
