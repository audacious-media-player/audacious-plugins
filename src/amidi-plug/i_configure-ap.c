/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2006
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

#include "i_configure-ap.h"

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>

#include "i_backend.h"
#include "i_configure.h"

#include "amidi-plug-icon.xpm"

enum
{
    LISTBACKEND_NAME_COLUMN = 0,
    LISTBACKEND_LONGNAME_COLUMN,
    LISTBACKEND_DESC_COLUMN,
    LISTBACKEND_FILENAME_COLUMN,
    LISTBACKEND_PPOS_COLUMN,
    LISTBACKEND_N_COLUMNS
};


void i_configure_ev_settplay_commit (void * settplay_vbox)
{
    GtkWidget * settplay_transpose_spinbt = g_object_get_data (G_OBJECT (settplay_vbox),
                                            "ap_opts_transpose_value");
    GtkWidget * settplay_drumshift_spinbt = g_object_get_data (G_OBJECT (settplay_vbox),
                                            "ap_opts_drumshift_value");

    aud_set_int ("amidiplug", "ap_opts_transpose_value",
     gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (settplay_transpose_spinbt)));
    aud_set_int ("amidiplug", "ap_opts_drumshift_value",
     gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (settplay_drumshift_spinbt)));
}


void i_configure_ev_settadva_commit (void * settadva_vbox)
{
    GtkWidget * settadva_extractlyr_checkbt = g_object_get_data (G_OBJECT (settadva_vbox),
            "ap_opts_lyrics_extract");
    GtkWidget * settadva_extractcomm_checkbt = g_object_get_data (G_OBJECT (settadva_vbox),
            "ap_opts_comments_extract");

    aud_set_int ("amidiplug", "ap_opts_lyrics_extract",
     gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (settadva_extractlyr_checkbt)));
    aud_set_int ("amidiplug", "ap_opts_comments_extract",
     gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (settadva_extractcomm_checkbt)));
}


int i_configure_backendlist_sortf (GtkTreeModel * model, GtkTreeIter * a,
                                   GtkTreeIter * b, void * data)
{
    int v_a = 0, v_b = 0;
    gtk_tree_model_get (model, a, LISTBACKEND_PPOS_COLUMN, &v_a, -1);
    gtk_tree_model_get (model, b, LISTBACKEND_PPOS_COLUMN, &v_b, -1);
    return (v_a - v_b);
}


void i_configure_gui_tab_ap (GtkWidget * ap_page_alignment,
                             void * commit_button)
{
    GtkWidget * settings_vbox; /* this vbox will contain all settings vbox (playback, advanced) */
    GtkWidget * settplay_frame, *settplay_vbox;
    GtkWidget * settplay_transpose_and_drumshift_hbox;
    GtkWidget * settplay_transpose_hbox, *settplay_transpose_label1, *settplay_transpose_spinbt;
    GtkWidget * settplay_drumshift_hbox, *settplay_drumshift_label1, *settplay_drumshift_spinbt;
    GtkWidget * settadva_frame, *settadva_vbox;
    GtkWidget * settadva_extractcomm_checkbt, *settadva_extractlyr_checkbt;

    GtkWidget * content_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

    settings_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

    settplay_frame = gtk_frame_new (_("Playback settings"));
    settplay_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width (GTK_CONTAINER (settplay_vbox), 4);
    settplay_transpose_and_drumshift_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    settplay_transpose_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    settplay_transpose_label1 = gtk_label_new (_("Transpose: "));
    settplay_transpose_spinbt = gtk_spin_button_new_with_range (-20, 20, 1);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (settplay_transpose_spinbt),
     aud_get_int ("amidiplug", "ap_opts_transpose_value"));
    gtk_box_pack_start (GTK_BOX (settplay_transpose_hbox), settplay_transpose_label1, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (settplay_transpose_hbox), settplay_transpose_spinbt, FALSE, FALSE, 2);
    gtk_box_pack_start (GTK_BOX (settplay_transpose_and_drumshift_hbox),
                        settplay_transpose_hbox, FALSE, FALSE, 0);
    settplay_drumshift_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    settplay_drumshift_label1 = gtk_label_new (_("Drum shift: "));
    settplay_drumshift_spinbt = gtk_spin_button_new_with_range (0, 127, 1);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (settplay_drumshift_spinbt),
     aud_get_int ("amidiplug", "ap_opts_drumshift_value"));
    gtk_box_pack_start (GTK_BOX (settplay_drumshift_hbox), settplay_drumshift_label1, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (settplay_drumshift_hbox), settplay_drumshift_spinbt, FALSE, FALSE, 2);
    gtk_box_pack_start (GTK_BOX (settplay_transpose_and_drumshift_hbox),
                        settplay_drumshift_hbox, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (settplay_vbox),
                        settplay_transpose_and_drumshift_hbox, FALSE, FALSE, 2);
    gtk_container_add (GTK_CONTAINER (settplay_frame), settplay_vbox);
    /* attach pointers of options to settplay_vbox so we can handle all of them in a single callback */
    g_object_set_data (G_OBJECT (settplay_vbox), "ap_opts_transpose_value", settplay_transpose_spinbt);
    g_object_set_data (G_OBJECT (settplay_vbox), "ap_opts_drumshift_value", settplay_drumshift_spinbt);
    g_signal_connect_swapped (G_OBJECT (commit_button), "ap-commit",
                              G_CALLBACK (i_configure_ev_settplay_commit), settplay_vbox);
    gtk_box_pack_start (GTK_BOX (settings_vbox), settplay_frame, TRUE, TRUE, 0);

    settadva_frame = gtk_frame_new (_("Advanced settings"));
    settadva_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width (GTK_CONTAINER (settadva_vbox), 4);
    settadva_extractcomm_checkbt = gtk_check_button_new_with_label (
                                       _("extract comments from MIDI file (if available)"));

    if (aud_get_int ("amidiplug", "ap_opts_comments_extract"))
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (settadva_extractcomm_checkbt), TRUE);

    gtk_box_pack_start (GTK_BOX (settadva_vbox), settadva_extractcomm_checkbt, FALSE, FALSE, 2);
    settadva_extractlyr_checkbt = gtk_check_button_new_with_label (
                                      _("extract lyrics from MIDI file (if available)"));

    if (aud_get_int ("amidiplug", "ap_opts_lyrics_extract"))
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (settadva_extractlyr_checkbt), TRUE);

    gtk_box_pack_start (GTK_BOX (settadva_vbox), settadva_extractlyr_checkbt, FALSE, FALSE, 2);
    gtk_container_add (GTK_CONTAINER (settadva_frame), settadva_vbox);
    /* attach pointers of options to settadva_vbox so we can handle all of them in a single callback */
    g_object_set_data (G_OBJECT (settadva_vbox), "ap_opts_comments_extract", settadva_extractcomm_checkbt);
    g_object_set_data (G_OBJECT (settadva_vbox), "ap_opts_lyrics_extract", settadva_extractlyr_checkbt);
    g_signal_connect_swapped (G_OBJECT (commit_button), "ap-commit",
                              G_CALLBACK (i_configure_ev_settadva_commit), settadva_vbox);
    gtk_box_pack_start (GTK_BOX (settings_vbox), settadva_frame, TRUE, TRUE, 0);

    gtk_box_pack_start (GTK_BOX (content_vbox), settings_vbox, TRUE, TRUE, 0);

    gtk_container_add ((GtkContainer *) ap_page_alignment, content_vbox);
}


void i_configure_gui_tablabel_ap (GtkWidget * ap_page_alignment,
                                  void * commit_button)
{
    GtkWidget * pagelabel_vbox, *pagelabel_image, *pagelabel_label;
    GdkPixbuf * pagelabel_image_pix;
    pagelabel_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
    pagelabel_image_pix = gdk_pixbuf_new_from_xpm_data ((const char **) amidi_plug_icon_xpm);
    pagelabel_image = gtk_image_new_from_pixbuf (pagelabel_image_pix);
    g_object_unref (pagelabel_image_pix);
    pagelabel_label = gtk_label_new ("");
    gtk_label_set_markup (GTK_LABEL (pagelabel_label), _("<span size=\"smaller\">AMIDI\nPlug</span>"));
    gtk_label_set_justify (GTK_LABEL (pagelabel_label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start (GTK_BOX (pagelabel_vbox), pagelabel_image, FALSE, FALSE, 1);
    gtk_box_pack_start (GTK_BOX (pagelabel_vbox), pagelabel_label, FALSE, FALSE, 1);
    gtk_container_add (GTK_CONTAINER (ap_page_alignment), pagelabel_vbox);
    gtk_widget_show_all (ap_page_alignment);
    return;
}
