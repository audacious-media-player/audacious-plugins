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

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>

#define N_ITEMS 2
#define N_MENUS 3

static const gchar * titles[N_ITEMS] = {N_("Play CD"), N_("Add CD")};
static const gint menus[N_MENUS] = {AUDACIOUS_MENU_MAIN,
 AUDACIOUS_MENU_PLAYLIST_ADD, AUDACIOUS_MENU_PLAYLIST_RCLICK};

static GtkWidget * items[N_ITEMS * N_MENUS];

static void cd_play (void)
{
    aud_drct_pl_open ("cdda://");
}

static void cd_add (void)
{
    aud_drct_pl_add ("cdda://", -1);
}

static void (* funcs[N_ITEMS]) (void) = {cd_play, cd_add};

static void cd_init (void)
{
    gint mcount, icount;
    GtkWidget * menu, * item;

    for (mcount = 0; mcount < N_MENUS; mcount ++)
    {
        menu = aud_get_plugin_menu (menus[mcount]);

        for (icount = 0; icount < N_ITEMS; icount ++)
        {
            item = gtk_image_menu_item_new_with_label (_(titles[icount]));
            gtk_image_menu_item_set_image ((GtkImageMenuItem *) item,
             gtk_image_new_from_stock (GTK_STOCK_CDROM, GTK_ICON_SIZE_MENU));
            gtk_widget_show (item);
            items[N_ITEMS * mcount + icount] = item;
            gtk_menu_shell_append ((GtkMenuShell *) menu, item);
            g_signal_connect (item, "activate", (GCallback) funcs[icount], NULL);
        }
    }
}

void cd_cleanup (void)
{
    gint count;

    for (count = 0; count < N_ITEMS * N_MENUS; count ++)
        gtk_widget_destroy (items[count]);
}

DECLARE_PLUGIN (cd-menu-items, cd_init, cd_cleanup)
