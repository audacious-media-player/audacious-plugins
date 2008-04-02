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


#ifndef AUDACIOUS_PLUGIN
void xs_fileinfo_update(void)
{
    XS_MUTEX_LOCK(xs_status);
    XS_MUTEX_LOCK(xs_fileinfowin);

    /* Check if control window exists, we are currently playing and have a tune */
    if (xs_fileinfowin) {
        gboolean isEnabled;
        GtkAdjustment *tmpAdj;

        if (xs_status.tuneInfo && xs_status.isPlaying && (xs_status.tuneInfo->nsubTunes > 1)) {
            tmpAdj = gtk_range_get_adjustment(GTK_RANGE(LUW("fileinfo_subctrl_adj")));

            tmpAdj->value = xs_status.currSong;
            tmpAdj->lower = 1;
            tmpAdj->upper = xs_status.tuneInfo->nsubTunes;
            XS_MUTEX_UNLOCK(xs_status);
            XS_MUTEX_UNLOCK(xs_fileinfowin);
            gtk_adjustment_value_changed(tmpAdj);
            XS_MUTEX_LOCK(xs_status);
            XS_MUTEX_LOCK(xs_fileinfowin);
            isEnabled = TRUE;
        } else
            isEnabled = FALSE;

        /* Enable or disable subtune-control in fileinfo window */
        gtk_widget_set_sensitive(LUW("fileinfo_subctrl_prev"), isEnabled);
        gtk_widget_set_sensitive(LUW("fileinfo_subctrl_adj"), isEnabled);
        gtk_widget_set_sensitive(LUW("fileinfo_subctrl_next"), isEnabled);
    }

    XS_MUTEX_UNLOCK(xs_status);
    XS_MUTEX_UNLOCK(xs_fileinfowin);
}


static void xs_fileinfo_setsong(void)
{
    gint n;

    XS_MUTEX_LOCK(xs_status);
    XS_MUTEX_LOCK(xs_fileinfowin);

    if (xs_status.tuneInfo && xs_status.isPlaying) {
        n = (gint) gtk_range_get_adjustment(GTK_RANGE(LUW("fileinfo_subctrl_adj")))->value;
        if ((n >= 1) && (n <= xs_status.tuneInfo->nsubTunes))
            xs_status.currSong = n;
    }

    XS_MUTEX_UNLOCK(xs_fileinfowin);
    XS_MUTEX_UNLOCK(xs_status);
}
#endif /* AUDACIOUS_PLUGIN */


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
#ifndef AUDACIOUS_PLUGIN
    gtk_text_freeze(GTK_TEXT(tmpText));
    gtk_text_set_point(GTK_TEXT(tmpText), 0);
    gtk_text_forward_delete(GTK_TEXT(tmpText), gtk_text_get_length(GTK_TEXT(tmpText)));
#endif

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

#ifdef AUDACIOUS_PLUGIN
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(tmpText))),
        subInfo ? subInfo : "", -1);
#else
    gtk_text_insert(GTK_TEXT(tmpText), NULL, NULL, NULL,
        subInfo ? subInfo : "", -1);

    /* Un-freeze the widget */
    gtk_text_thaw(GTK_TEXT(tmpText));
#endif
}


void xs_fileinfo(gchar * filename)
{
    GtkWidget *tmpMenuItem, *tmpMenu, *tmpOptionMenu;
    xs_tuneinfo_t *tmpInfo;
    stil_subnode_t *tmpNode;
    gchar tmpStr[256], *tmpFilename;
    gint n;

    /* Current implementation leaves old fileinfo window untouched if
     * no information can be found for the new file. Hmm...
     */
#ifdef AUDACIOUS_PLUGIN
    xs_get_trackinfo(filename, &tmpFilename, &n);
#else
    tmpFilename = filename;
#endif    

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

#ifdef AUDACIOUS_PLUGIN
    g_free(tmpFilename);
#endif

    /* Check if there already is an open fileinfo window */
    if (xs_fileinfowin)
                gtk_window_present(GTK_WINDOW(xs_fileinfowin));
    else {
        xs_fileinfowin = create_xs_fileinfowin();
#ifndef AUDACIOUS_PLUGIN
        XS_SIGNAL_CONNECT(gtk_range_get_adjustment(GTK_RANGE(LUW("fileinfo_subctrl_adj"))),
            "value_changed", xs_fileinfo_setsong, NULL);
#endif
    }

    /* Delete current items */
    tmpOptionMenu = LUW("fileinfo_sub_tune");
    tmpMenu = gtk_option_menu_get_menu(GTK_OPTION_MENU(tmpOptionMenu));
    gtk_widget_destroy(tmpMenu);
    gtk_option_menu_remove_menu(GTK_OPTION_MENU(tmpOptionMenu));
    tmpMenu = gtk_menu_new();


    /* Set the generic song information */
    tmpFilename = XS_CS_FILENAME(filename);
    gtk_entry_set_text(GTK_ENTRY(LUW("fileinfo_filename")), tmpFilename);
    g_free(tmpFilename);
    gtk_entry_set_text(GTK_ENTRY(LUW("fileinfo_songname")), tmpInfo->sidName);
    gtk_entry_set_text(GTK_ENTRY(LUW("fileinfo_composer")), tmpInfo->sidComposer);
    gtk_entry_set_text(GTK_ENTRY(LUW("fileinfo_copyright")), tmpInfo->sidCopyright);


    /* Main tune - the pseudo tune */
    tmpMenuItem = gtk_menu_item_new_with_label(_("General info"));
    gtk_widget_show(tmpMenuItem);
    gtk_menu_append(GTK_MENU(tmpMenu), tmpMenuItem);
    if (xs_fileinfostil)
        tmpNode = xs_fileinfostil->subTunes[0];
    else
        tmpNode = NULL;
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

#ifndef AUDACIOUS_PLUGIN
    xs_fileinfo_update();
#endif
}
