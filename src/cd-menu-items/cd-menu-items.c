/*
 * cd-menu-items.c
 * Copyright 2009-2011 John Lindgren
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

#include <gtk/gtk.h>

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>

#define N_ITEMS 2
#define N_MENUS 3

static const gchar * titles[N_ITEMS] = {N_("Play CD"), N_("Add CD")};
static const gint menus[N_MENUS] = {AUD_MENU_MAIN, AUD_MENU_PLAYLIST_ADD,
 AUD_MENU_PLAYLIST_RCLICK};

static void cd_play (void) {aud_drct_pl_open ("cdda://"); }
static void cd_add (void) {aud_drct_pl_add ("cdda://", -1); }
static MenuFunc funcs[N_ITEMS] = {cd_play, cd_add};

static gboolean cd_init (void)
{
    for (gint m = 0; m < N_MENUS; m ++)
        for (gint i = 0; i < N_ITEMS; i ++)
            aud_plugin_menu_add (menus[m], funcs[i], _(titles[i]),
             GTK_STOCK_CDROM);

    return TRUE;
}

void cd_cleanup (void)
{
    for (gint m = 0; m < N_MENUS; m ++)
        for (gint i = 0; i < N_ITEMS; i ++)
            aud_plugin_menu_remove (menus[m], funcs[i]);
}

AUD_GENERAL_PLUGIN
(
    .name = N_("Audio CD Menu Items"),
    .domain = PACKAGE,
    .enabled_by_default = TRUE,
    .init = cd_init,
    .cleanup = cd_cleanup,
)
