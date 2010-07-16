/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   File information window

   Programmed and designed by Matti 'ccr' Hamalainen <ccr@tnsp.org>
   (C) Copyright 1999-2007 Tecnic Software productions (TNSP)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <libaudcore/audstrings.h>

#include "xs_fileinfo.h"
#include "xs_player.h"
#include "xs_support.h"
#include "xs_config.h"
#include "xs_interface.h"
#include "xs_glade.h"
#include "xs_slsup.h"


static GtkWidget *xs_fileinfowin = NULL;
static stil_node_t *xs_fileinfostil = NULL;
XS_MUTEX(xs_fileinfowin);

#define LUW(x)    lookup_widget(xs_fileinfowin, x)


void xs_fileinfo_ok(void)
{
    XS_MUTEX_LOCK(xs_fileinfowin);
    if (xs_fileinfowin) {
        gtk_widget_destroy(xs_fileinfowin);
        xs_fileinfowin = NULL;
    }
    XS_MUTEX_UNLOCK(xs_fileinfowin);
}


gboolean xs_fileinfo_delete(GtkWidget * widget, GdkEvent * event, gpointer user_data)
{
    (void) widget;
    (void) event;
    (void) user_data;

    XSDEBUG("delete_event\n");
    xs_fileinfo_ok();
    return FALSE;
}


static void xs_fileinfo_subtune(GtkWidget * widget, void *data)
{
    stil_subnode_t *tmpNode;
    GtkWidget *tmpText;
    gchar *subName, *subAuthor, *subInfo;

    (void) widget;
    (void) data;

    /* Freeze text-widget and delete the old text */
    tmpText = LUW("fileinfo_sub_info");

    /* Get subtune information */
    tmpNode = (stil_subnode_t *) data;
    if (!tmpNode && xs_fileinfostil)
        tmpNode = xs_fileinfostil->subTunes[0];

    if (tmpNode) {
        subName = tmpNode->name;
        subAuthor = tmpNode->author;
        subInfo = tmpNode->info;
    } else {
        subName = NULL;
        subAuthor = NULL;
        subInfo = NULL;
    }

    /* Get and set subtune information */
    gtk_entry_set_text(GTK_ENTRY(LUW("fileinfo_sub_name")), subName ? subName : "");
    gtk_entry_set_text(GTK_ENTRY(LUW("fileinfo_sub_author")), subAuthor ? subAuthor : "");

    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(tmpText))),
        subInfo ? subInfo : "", -1);
}


void xs_fileinfo(const gchar * filename)
{
    GtkWidget *tmpMenuItem, *tmpMenu, *tmpOptionMenu;
    xs_tuneinfo_t *tmpInfo;
    stil_subnode_t *tmpNode;
    gchar tmpStr[256], *tmpFilename;
    gint n;

    /* Current implementation leaves old fileinfo window untouched if
     * no information can be found for the new file. Hmm...
     */
    tmpFilename = filename_split_subtune(filename, &n);

    /* Get new tune information */
    XS_MUTEX_LOCK(xs_fileinfowin);
    XS_MUTEX_LOCK(xs_status);
    if ((tmpInfo = xs_status.sidPlayer->plrGetSIDInfo(tmpFilename)) == NULL) {
        XS_MUTEX_UNLOCK(xs_fileinfowin);
        XS_MUTEX_UNLOCK(xs_status);
        return;
    }
    XS_MUTEX_UNLOCK(xs_status);

    xs_fileinfostil = xs_stil_get(tmpFilename);
    g_free(tmpFilename);

    /* Check if there already is an open fileinfo window */
    if (xs_fileinfowin)
        XS_WINDOW_PRESENT(xs_fileinfowin);
    else
        xs_fileinfowin = create_xs_fileinfowin();

    /* Delete current items */
    tmpOptionMenu = LUW("fileinfo_sub_tune");
    tmpMenu = gtk_option_menu_get_menu(GTK_OPTION_MENU(tmpOptionMenu));
    gtk_widget_destroy(tmpMenu);
    gtk_option_menu_remove_menu(GTK_OPTION_MENU(tmpOptionMenu));
    tmpMenu = gtk_menu_new();


    /* Set the generic song information */
    gtk_entry_set_text(GTK_ENTRY(LUW("fileinfo_filename")), filename);
    gtk_entry_set_text(GTK_ENTRY(LUW("fileinfo_songname")), tmpInfo->sidName);
    gtk_entry_set_text(GTK_ENTRY(LUW("fileinfo_composer")), tmpInfo->sidComposer);
    gtk_entry_set_text(GTK_ENTRY(LUW("fileinfo_copyright")), tmpInfo->sidCopyright);


    /* Main tune - the pseudo tune */
    tmpMenuItem = gtk_menu_item_new_with_label(_("General info"));
    gtk_widget_show(tmpMenuItem);
    gtk_menu_append(GTK_MENU(tmpMenu), tmpMenuItem);

    tmpNode = (xs_fileinfostil != NULL) ? xs_fileinfostil->subTunes[0] : NULL;
    XS_SIGNAL_CONNECT(tmpMenuItem, "activate", xs_fileinfo_subtune, tmpNode);

    /* Other menu items */
    for (n = 1; n <= tmpInfo->nsubTunes; n++) {
        if (xs_fileinfostil && n <= xs_fileinfostil->nsubTunes && xs_fileinfostil->subTunes[n]) {
            gboolean isSet = FALSE;
            tmpNode = xs_fileinfostil->subTunes[n];

            g_snprintf(tmpStr, sizeof(tmpStr), _("Tune #%i: "), n);

            if (tmpNode->name) {
                xs_pnstrcat(tmpStr, sizeof(tmpStr), tmpNode->name);
                isSet = TRUE;
            }

            if (tmpNode->title) {
                xs_pnstrcat(tmpStr, sizeof(tmpStr),
                    isSet ? " [*]" : tmpNode->title);
                isSet = TRUE;
            }

            if (tmpNode->info) {
                xs_pnstrcat(tmpStr, sizeof(tmpStr), " [!]");
                isSet = TRUE;
            }

            if (!isSet)
                xs_pnstrcat(tmpStr, sizeof(tmpStr), "---");

            tmpMenuItem = gtk_menu_item_new_with_label(tmpStr);
            gtk_widget_show(tmpMenuItem);
            gtk_menu_append(GTK_MENU(tmpMenu), tmpMenuItem);
            XS_SIGNAL_CONNECT(tmpMenuItem, "activate", xs_fileinfo_subtune, tmpNode);
        }

    }

    gtk_option_menu_set_menu(GTK_OPTION_MENU(tmpOptionMenu), tmpMenu);
    gtk_widget_show(tmpOptionMenu);

    /* Set the subtune information */
    xs_fileinfo_subtune(tmpOptionMenu, NULL);

    /* Free temporary tuneinfo */
    xs_tuneinfo_free(tmpInfo);

    /* Show the window */
    gtk_widget_show(xs_fileinfowin);

    XS_MUTEX_UNLOCK(xs_fileinfowin);
}
