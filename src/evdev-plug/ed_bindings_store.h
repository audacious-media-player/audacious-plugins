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

#ifndef _I_ED_BINDINGS_STORE_H
#define _I_ED_BINDINGS_STORE_H 1

#include "ed_types.h"
#include "ed_common.h"
#include <glib.h>

typedef void (*ed_bindings_store_foreach_func)( ed_inputevent_t * , gint , gpointer , gpointer );

gpointer ed_bindings_store_new ( void );
gint ed_bindings_store_insert ( gpointer , ed_inputevent_t * , gint );
void ed_bindings_store_foreach ( gpointer , ed_bindings_store_foreach_func , gpointer , gpointer );
guint ed_bindings_store_size ( gpointer );
gboolean ed_bindings_store_lookup( gpointer , ed_inputevent_t * , gint * );
gint ed_bindings_store_delete ( gpointer );

#endif /* !_I_ED_BINDINGS_STORE_H */
