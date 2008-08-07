/*
 * Audacious Bluetooth headset suport plugin
 *
 * Copyright (c) 2008 Paula Stanciu paula.stanciu@gmail.com
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 */

#include <config.h>
#include <glib.h>
#include <sys/types.h>
#include <audacious/plugin.h>
#include <audacious/ui_plugin_menu.h>
#include <audacious/i18n.h>
#include <gtk/gtk.h>
#include <audacious/util.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <glib-object.h>
#include <stdio.h>
#include <audacious/configdb.h>
#include <audacious/i18n.h>
#include <audacious/util.h>
#include "gui.h"
typedef struct {
    guint class;
    gchar* address;
    gchar* name;
}DeviceData;



void refresh_call(void);
void connect_call(void);
void play_call(void);
GList * audio_devices;
gint discover_finish ;
DBusGConnection * bus;
DBusGProxy * obj;
gchar *bonded_dev;
GMutex *bonded_dev_mutex;

