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

#ifndef _I_ED_INTERNALS_H
#define _I_ED_INTERNALS_H 1

#include "ed_types.h"
#include "ed_common.h"
#include <glib.h>

#define ED_DEVCHECK_OK		0
#define ED_DEVCHECK_ABSENT	1

typedef enum
{
  ED_CONFIG_INFO_END = -1,
  ED_CONFIG_INFO_FILENAME = 0,
  ED_CONFIG_INFO_PHYS,
  ED_CONFIG_INFO_ISCUSTOM,
  ED_CONFIG_INFO_ISACTIVE
}
ed_config_info_t;

ed_device_t * ed_device_new ( gchar * , gchar * , gchar * , gint );
gint ed_device_delete ( ed_device_t * );

ed_device_info_t * ed_device_info_new ( gchar * , gchar * , gchar * , gint );
gint ed_device_info_delete ( ed_device_info_t * );

gint ed_device_start_listening ( ed_device_t * );
void ed_device_start_listening_from_config ( void );
gint ed_device_stop_listening ( ed_device_t * );
gint ed_device_stop_listening_from_info ( ed_device_info_t * );
void ed_device_stop_listening_all ( gboolean );
gboolean ed_device_check_listening_from_info ( ed_device_info_t * );

gboolean ed_inputevent_check_equality( ed_inputevent_t * , ed_inputevent_t * );
gboolean ed_device_info_check_equality( ed_device_info_t * , ed_device_info_t * );

/* these function return a GList of ed_device_info_t items */
GList * ed_device_get_list_from_system ( void );
GList * ed_device_get_list_from_config ( void );
void ed_device_free_list ( GList * );

gint ed_device_check ( GList * , gchar * , gchar ** , gchar ** );

gint ed_config_save_from_list ( GList * );

#endif /* !_I_ED_INTERNALS_H */
