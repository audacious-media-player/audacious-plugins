/*
 *  Audacious Bluetooth headset suport plugin
 *
 *  Copyright (c) 2008 Paula Stanciu <paula.stanciu@gmail.com>
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2006-2007  Bastien Nocera <hadess@hadess.net>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <gtk/gtk.h>
#include <glib.h>
#include <dbus/dbus-glib.h>
gint bonding_finish;
void run_agents(void);
int setup_agents(DBusGConnection *conn);
void cleanup_agents(void);

int register_agents(void);
void unregister_agents(void);

void show_agents(void);
void set_auto_authorize(gboolean value);
