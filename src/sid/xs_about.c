/*  
   xmms-sid - SIDPlay input plugin for X MultiMedia System (XMMS)

   Aboutbox dialog
   
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
#include "xmms-sid.h"
#include <gtk/gtk.h>
#include "xmms-sid-logo.xpm"


static GtkWidget *xs_aboutwin = NULL;


#ifdef HAVE_THEMETUNE
#include <xmms/xmmsctrl.h>

/* (Included only if themetune support is enabled)
 * Stop playing, add XMMS-SID themetune to playlist
 * and start playing it.
 */
gint xs_about_theme(void)
{
    const gint iSession = 0;    /* Assume session 0 */
    gint iPos;

    /* Stop current song, add theme to playlist, play. */
    xmms_remote_stop(iSession);
    iPos = xmms_remote_get_playlist_length(iSession);
    xmms_remote_playlist_add_url_string(iSession, THEMETUNE_FILE);
    xmms_remote_set_playlist_pos(iSession, iPos);
    xmms_remote_play(iSession);
    return 0;
}
#endif


XS_DEF_WINDOW_CLOSE(about_ok, aboutwin)
XS_DEF_WINDOW_DELETE(about, aboutwin)


/*
 * Execute the about dialog
 */
void xs_about(void)
{
    GtkWidget *about_vbox1;
    GtkWidget *about_frame;
    GtkWidget *about_logo;
    GdkPixmap *about_logo_pixmap = NULL, *about_logo_mask = NULL;
    GtkWidget *about_scrwin;
    GtkWidget *about_text;
    GtkWidget *alignment6;
    GtkWidget *about_close;
    gchar tmpStr[64];

    /* Check if there already is an open about window */
    if (xs_aboutwin != NULL) {
                gtk_window_present(GTK_WINDOW(xs_aboutwin));
        return;
    }

    /* No, create one ... */
    xs_aboutwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_type_hint(GTK_WINDOW(xs_aboutwin), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_widget_set_name(xs_aboutwin, "xs_aboutwin");
    gtk_object_set_data(GTK_OBJECT(xs_aboutwin), "xs_aboutwin", xs_aboutwin);
    g_snprintf(tmpStr, sizeof(tmpStr), _("About %s"), XS_PACKAGE_STRING);
    gtk_window_set_title(GTK_WINDOW(xs_aboutwin), tmpStr);
    gtk_window_set_default_size(GTK_WINDOW(xs_aboutwin), 350, -1);

    XS_SIGNAL_CONNECT(xs_aboutwin, "delete_event", xs_about_delete, NULL);

    about_vbox1 = gtk_vbox_new(FALSE, 0);
    gtk_widget_set_name(about_vbox1, "about_vbox1");
    gtk_widget_ref(about_vbox1);
    gtk_object_set_data_full(GTK_OBJECT(xs_aboutwin), "about_vbox1", about_vbox1,
                 (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show(about_vbox1);
    gtk_container_add(GTK_CONTAINER(xs_aboutwin), about_vbox1);

#ifdef HAVE_THEMETUNE
    about_frame = gtk_button_new();
#else
    about_frame = gtk_frame_new(NULL);
#endif
    gtk_widget_set_name(about_frame, "about_frame");
    gtk_widget_ref(about_frame);
    gtk_object_set_data_full(GTK_OBJECT(xs_aboutwin), "about_frame", about_frame,
                 (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show(about_frame);
    gtk_box_pack_start(GTK_BOX(about_vbox1), about_frame, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(about_frame), 4);

#ifdef HAVE_THEMETUNE
    gtk_signal_connect(GTK_OBJECT(about_frame), "clicked", GTK_SIGNAL_FUNC(xs_about_theme), NULL);
#else
    gtk_frame_set_shadow_type(GTK_FRAME(about_frame), GTK_SHADOW_OUT);
#endif

    /* Create the Gdk data for logo pixmap */
    gtk_widget_realize(xs_aboutwin);
    about_logo_pixmap = gdk_pixmap_create_from_xpm_d(
        xs_aboutwin->window, &about_logo_mask,
        NULL, (gchar **) xmms_sid_logo_xpm);

    about_logo = gtk_pixmap_new(about_logo_pixmap, about_logo_mask);

    /* Create logo widget */
    gtk_widget_set_name(about_logo, "about_logo");
    gtk_widget_ref(about_logo);
    gtk_object_set_data_full(GTK_OBJECT(xs_aboutwin), "about_logo", about_logo,
        (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show(about_logo);
    gtk_container_add(GTK_CONTAINER(about_frame), about_logo);
    gtk_misc_set_padding(GTK_MISC(about_logo), 0, 6);

    about_scrwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_name(about_scrwin, "about_scrwin");
    gtk_widget_ref(about_scrwin);
    gtk_object_set_data_full(GTK_OBJECT(xs_aboutwin), "about_scrwin", about_scrwin,
        (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show(about_scrwin);
    gtk_box_pack_start(GTK_BOX(about_vbox1), about_scrwin, TRUE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(about_scrwin), 8);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(about_scrwin),
        GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

    about_text = gtk_text_view_new();
    gtk_widget_set_name(about_text, "about_text");
    gtk_widget_ref(about_text);
    gtk_object_set_data_full(GTK_OBJECT(xs_aboutwin), "about_text", about_text,
        (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show(about_text);
    gtk_container_add(GTK_CONTAINER(about_scrwin), about_text);
    gtk_widget_set_usize(about_text, -2, 100);
    gtk_text_buffer_set_text(
    GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(about_text))),
            "\n"
            "This release of XMMS-SID is dedicated to\n"
            "            Richard Joseph\n"
            "                  and\n"
            "      Lauri Sipilä (aka Xaztur/PWP)\n"
            " - Now gone, but forever in our hearts -\n"
            "\n"
            "\n"
            "(C) Copyright 1999-2007\n"
            "\tTecnic Software productions (TNSP)\n"
            "\tLicensed under GNU GPL v2\n"
            "\n"
            "Programming and design\n"
            "\tMatti 'ccr' Hämäläinen\n"
            "\n"
#ifdef HAVE_SIDPLAY1
            "libSIDPlay1 created by\n"
            "\tMichael Schwendt\n" "\n"
#endif
#ifdef HAVE_SIDPLAY2
            "libSIDPlay2 and reSID created by\n"
            "\tSimon White, Dag Lem,\n"
            "\tMichael Schwendt and rest.\n"
            "\n"
#endif
#ifdef HAVE_THEMETUNE
            "\"Kummatti City\", theme of XMMS-SID 0.8\n"
            "\tby Ari 'Agemixer' Yliaho\n"
            "\t(C) Copyright 1998 Scallop\n"
            "\t(Refer to README for license)\n" "\n"
#endif
            "Original XMMS-SID (v0.4) by\n" "\tWillem Monsuwe\n" "\n"

            "Greetings fly out to ...\n"
            "\tEveryone at #Linux.Fi, #Fireball,\n"
            "\t#TNSP and #c-64 of IRCNet, #xmms\n"
            "\tof Freenode.net.\n"
            "\n"
            "\tDekadence, PWP, Byterapers,\n"
            "\tmfx, Unique, Fairlight, iSO,\n"
            "\tWrath Designs, Padua, Extend,\n"
            "\tPHn, Creators, Cosine, tAAt,\n"
            "\tViruz, Crest and Skalaria.\n" "\n"

            "Special thanks\n"
            "\tGerfried 'Alfie' Fuchs\n"
            "\tAndreas 'mrsid' Varga\n"
            "\tAll the betatesters.\n"
            "\tAll the users!\n", -1);

    alignment6 = gtk_alignment_new(0.5, 0.5, 0.18, 1);
    gtk_widget_set_name(alignment6, "alignment6");
    gtk_widget_ref(alignment6);
    gtk_object_set_data_full(GTK_OBJECT(xs_aboutwin), "alignment6", alignment6,
         (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show(alignment6);
    gtk_box_pack_start(GTK_BOX(about_vbox1), alignment6, FALSE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(alignment6), 8);

    about_close = gtk_button_new_with_label(_("Close"));
    gtk_widget_set_name(about_close, "about_close");
    gtk_widget_ref(about_close);
    gtk_object_set_data_full(GTK_OBJECT(xs_aboutwin), "about_close", about_close,
         (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show(about_close);
    gtk_container_add(GTK_CONTAINER(alignment6), about_close);
    GTK_WIDGET_SET_FLAGS(about_close, GTK_CAN_DEFAULT);

    XS_SIGNAL_CONNECT(about_close, "clicked", xs_about_ok, NULL);

    gtk_widget_show(xs_aboutwin);
}
