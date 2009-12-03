/*
 * cd-menu-items.c
 * Copyright 2009 John Lindgren
 *
 * This file is part of Audacious.
 *
 * Audacious is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2 or version 3 of the License.
 *
 * Audacious is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Audacious. If not, see <http://www.gnu.org/licenses/>.
 *
 * The Audacious team does not consider modular code linking to Audacious or
 * using our public API to be a derived work.
 */

#include "config.h"

#include <glib.h>
#include <gtk/gtk.h>

#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <audacious/ui_plugin_menu.h>

#define N_MENUS 3

static const int menus[N_MENUS] = {AUDACIOUS_MENU_MAIN,
 AUDACIOUS_MENU_PLAYLIST_ADD, AUDACIOUS_MENU_PLAYLIST_RCLICK};
static GtkWidget * menu_items[2 * N_MENUS];

static void cd_play (void)
{
    audacious_drct_pl_open ("cdda://");
}

static void cd_add (void)
{
    audacious_drct_pl_add_url_string ("cdda://");
}

static void cd_init (void)
{
    gint count;
    GtkWidget * item, * menu;

    for (count = 0; count < N_MENUS; count ++)
    {
        menu = aud_get_plugin_menu (menus[count]);

        item = gtk_image_menu_item_new_with_label (_("Play CD"));
        gtk_image_menu_item_set_image ((GtkImageMenuItem *) item,
         gtk_image_new_from_stock (GTK_STOCK_CDROM, GTK_ICON_SIZE_MENU));
        gtk_widget_show (item);
        menu_items[2 * count] = item;
        gtk_menu_shell_append ((GtkMenuShell *) menu, item);
        g_signal_connect (item, "activate", (GCallback) cd_play, NULL);

        item = gtk_image_menu_item_new_with_label (_("Add CD"));
        gtk_image_menu_item_set_image ((GtkImageMenuItem *) item,
         gtk_image_new_from_stock (GTK_STOCK_CDROM, GTK_ICON_SIZE_MENU));
        gtk_widget_show (item);
        menu_items[2 * count + 1] = item;
        gtk_menu_shell_append ((GtkMenuShell *) menu, item);
        g_signal_connect (item, "activate", (GCallback) cd_add, NULL);
    }
}

void cd_cleanup (void)
{
    gint count;

    for (count = 0; count < N_MENUS * 2; count ++)
        gtk_widget_destroy (menu_items[count]);
}

static GeneralPlugin plugin =
{
	.description = "CD Menu Items Plugin",
	.init = cd_init,
	.cleanup = cd_cleanup,
};

static GeneralPlugin * plugin_list[] = {& plugin, NULL};

SIMPLE_GENERAL_PLUGIN (cd-menu-items, plugin_list);
