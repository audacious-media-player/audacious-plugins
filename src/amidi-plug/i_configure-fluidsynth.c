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

#include "i_configure-fluidsynth.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <gtk/gtk.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>

#include "i_common.h"
#include "i_configure.h"

#include "backend-fluidsynth/backend-fluidsynth-icon.xpm"

enum
{
    LISTSFONT_FILENAME_COLUMN = 0,
    LISTSFONT_FILESIZE_COLUMN,
    LISTSFONT_N_COLUMNS
};


void i_configure_ev_toggle_default (GtkToggleButton * togglebutton, void * hbox)
{
    if (gtk_toggle_button_get_active (togglebutton))
        gtk_widget_set_sensitive (GTK_WIDGET (hbox), FALSE);
    else
        gtk_widget_set_sensitive (GTK_WIDGET (hbox), TRUE);
}


void i_configure_ev_sflist_add (void * sfont_lv)
{
    GtkWidget * parent_window = gtk_widget_get_toplevel (sfont_lv);

    if (gtk_widget_is_toplevel (parent_window))
    {
        GtkTreeSelection * listsel = gtk_tree_view_get_selection (GTK_TREE_VIEW (sfont_lv));
        GtkTreeIter itersel, iterapp;
        GtkWidget * browse_dialog = gtk_file_chooser_dialog_new (_("AMIDI-Plug - select SoundFont file"),
                                    GTK_WINDOW (parent_window),
                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                    GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

        if (gtk_tree_selection_get_selected (listsel, NULL, &itersel))
        {
            char * selfilename = NULL, *selfiledir = NULL;
            GtkTreeModel * store = gtk_tree_view_get_model (GTK_TREE_VIEW (sfont_lv));
            gtk_tree_model_get (GTK_TREE_MODEL (store), &itersel, LISTSFONT_FILENAME_COLUMN, &selfilename, -1);
            selfiledir = g_path_get_dirname (selfilename);
            gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (browse_dialog), selfiledir);
            free (selfiledir);
            free (selfilename);
        }

        if (gtk_dialog_run (GTK_DIALOG (browse_dialog)) == GTK_RESPONSE_ACCEPT)
        {
            struct stat finfo;
            GtkTreeModel * store = gtk_tree_view_get_model (GTK_TREE_VIEW (sfont_lv));
            char * filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (browse_dialog));
            int filesize = -1;

            if (stat (filename, &finfo) == 0)
                filesize = finfo.st_size;

            gtk_list_store_append (GTK_LIST_STORE (store), &iterapp);
            gtk_list_store_set (GTK_LIST_STORE (store), &iterapp,
                                LISTSFONT_FILENAME_COLUMN, filename,
                                LISTSFONT_FILESIZE_COLUMN, filesize, -1);
            DEBUGMSG ("selected file: %s\n", filename);
            free (filename);
        }

        gtk_widget_destroy (browse_dialog);
    }
}


void i_configure_ev_sflist_rem (void * sfont_lv)
{
    GtkTreeModel * store;
    GtkTreeIter iter;
    GtkTreeSelection * listsel = gtk_tree_view_get_selection (GTK_TREE_VIEW (sfont_lv));

    if (gtk_tree_selection_get_selected (listsel, &store, &iter))
        gtk_list_store_remove (GTK_LIST_STORE (store), &iter);
}


void i_configure_ev_sflist_swap (GtkWidget * button, void * sfont_lv)
{
    GtkTreeModel * store;
    GtkTreeIter iter;
    GtkTreeSelection * listsel = gtk_tree_view_get_selection (GTK_TREE_VIEW (sfont_lv));

    if (gtk_tree_selection_get_selected (listsel, &store, &iter))
    {
        unsigned swapdire = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button), "swapdire"));

        if (swapdire == 0)   /* move up */
        {
            GtkTreePath * treepath = gtk_tree_model_get_path (store, &iter);

            if (gtk_tree_path_prev (treepath))
            {
                GtkTreeIter iter_prev;

                if (gtk_tree_model_get_iter (store, &iter_prev, treepath))
                    gtk_list_store_swap (GTK_LIST_STORE (store), &iter, &iter_prev);
            }

            gtk_tree_path_free (treepath);
        }
        else /* move down */
        {
            GtkTreeIter iter_prev = iter;

            if (gtk_tree_model_iter_next (store, &iter))
                gtk_list_store_swap (GTK_LIST_STORE (store), &iter, &iter_prev);
        }
    }
}


void i_configure_ev_sflist_commit (void * sfont_lv)
{
    GtkTreeIter iter;
    GtkTreeModel * store = gtk_tree_view_get_model (GTK_TREE_VIEW (sfont_lv));
    GString * sflist_string = g_string_new ("");

    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter) == TRUE)
    {
        bool_t iter_is_valid = FALSE;

        do
        {
            char * fname;
            gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                                LISTSFONT_FILENAME_COLUMN, &fname, -1);
            g_string_prepend_c (sflist_string, ';');
            g_string_prepend (sflist_string, fname);
            free (fname);
            iter_is_valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter);
        }
        while (iter_is_valid == TRUE);
    }

    if (sflist_string->len > 0)
        g_string_truncate (sflist_string, sflist_string->len - 1);

    aud_set_string ("amidiplug", "fsyn_soundfont_file", sflist_string->str);

    g_string_free (sflist_string, TRUE);
}


void i_configure_ev_sygain_commit (void * gain_spinbt)
{
    int gain = -1;

    if (gtk_widget_get_sensitive (gain_spinbt))
        gain = gtk_spin_button_get_value (GTK_SPIN_BUTTON (gain_spinbt)) * 10;

    aud_set_int ("amidiplug", "fsyn_synth_gain", gain);
}


void i_configure_ev_sypoly_commit (void * poly_spinbt)
{
    int polyphony = -1;

    if (gtk_widget_get_sensitive (poly_spinbt))
        polyphony = gtk_spin_button_get_value (GTK_SPIN_BUTTON (poly_spinbt));

    aud_set_int ("amidiplug", "fsyn_synth_polyphony", polyphony);
}


void i_configure_ev_syreverb_commit (void * reverb_yes_radiobt)
{
    int reverb = -1;

    if (gtk_widget_get_sensitive (reverb_yes_radiobt))
    {
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (reverb_yes_radiobt)))
            reverb = 1;
        else
            reverb = 0;
    }

    aud_set_int ("amidiplug", "fsyn_synth_reverb", reverb);
}


void i_configure_ev_sychorus_commit (void * chorus_yes_radiobt)
{
    int chorus = -1;

    if (gtk_widget_get_sensitive (chorus_yes_radiobt))
    {
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chorus_yes_radiobt)))
            chorus = 1;
        else
            chorus = 0;
    }

    aud_set_int ("amidiplug", "fsyn_synth_chorus", chorus);
}


void i_configure_ev_sysamplerate_togglecustom (GtkToggleButton * custom_radiobt, void * custom_entry)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (custom_radiobt)))
        gtk_widget_set_sensitive (GTK_WIDGET (custom_entry), TRUE);
    else
        gtk_widget_set_sensitive (GTK_WIDGET (custom_entry), FALSE);
}


void i_configure_ev_sysamplerate_commit (void * samplerate_custom_radiobt)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (samplerate_custom_radiobt)))
    {
        GtkWidget * customentry = g_object_get_data (G_OBJECT (samplerate_custom_radiobt), "customentry");
        int customvalue = strtol (gtk_entry_get_text (GTK_ENTRY (customentry)), NULL, 10);

        if (customvalue < 22050 || customvalue > 96000)
            customvalue = 44100;

        aud_set_int ("amidiplug", "fsyn_synth_samplerate", customvalue);
    }
    else
    {
        GSList * group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (samplerate_custom_radiobt));

        while (group != NULL)
        {
            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (group->data)))
                aud_set_int ("amidiplug", "fsyn_synth_samplerate",
                 GPOINTER_TO_INT (g_object_get_data (G_OBJECT (group->data), "val")));

            group = group->next;
        }
    }
}


void i_configure_gui_tab_fsyn (GtkWidget * fsyn_page_alignment,
                               void * commit_button)
{
    GtkWidget * content_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

    if (1)
    {
        GtkWidget * soundfont_frame, *soundfont_vbox;
        GtkListStore * soundfont_file_store;
        GtkCellRenderer * soundfont_file_lv_text_rndr;
        GtkTreeViewColumn * soundfont_file_lv_fname_col, *soundfont_file_lv_fsize_col;
        GtkWidget * soundfont_file_hbox, *soundfont_file_lv, *soundfont_file_lv_sw;
        GtkTreeSelection * soundfont_file_lv_sel;
        GtkWidget * soundfont_file_bbox_vbox, *soundfont_file_bbox_addbt, *soundfont_file_bbox_rembt;
        GtkWidget * soundfont_file_bbox_mvupbt, *soundfont_file_bbox_mvdownbt;
        GtkWidget * synth_frame, *synth_hbox, *synth_leftcol_vbox, *synth_rightcol_vbox;
        GtkWidget * synth_samplerate_frame, *synth_samplerate_vbox, *synth_samplerate_option[4];
        GtkWidget * synth_samplerate_optionhbox, *synth_samplerate_optionentry, *synth_samplerate_optionlabel;
        GtkWidget * synth_gain_frame, *synth_gain_hbox, *synth_gain_value_hbox;
        GtkWidget * synth_gain_value_label, *synth_gain_value_spin, *synth_gain_defcheckbt;
        GtkWidget * synth_poly_frame, *synth_poly_hbox, *synth_poly_value_hbox;
        GtkWidget * synth_poly_value_label, *synth_poly_value_spin, *synth_poly_defcheckbt;
        GtkWidget * synth_reverb_frame, *synth_reverb_hbox, *synth_reverb_value_hbox;
        GtkWidget * synth_reverb_value_option[2], *synth_reverb_defcheckbt;
        GtkWidget * synth_chorus_frame, *synth_chorus_hbox, *synth_chorus_value_hbox;
        GtkWidget * synth_chorus_value_option[2], *synth_chorus_defcheckbt;

        /* soundfont settings */
        soundfont_frame = gtk_frame_new (_("SoundFont settings"));
        soundfont_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
        gtk_container_set_border_width (GTK_CONTAINER (soundfont_vbox), 4);
        gtk_container_add (GTK_CONTAINER (soundfont_frame), soundfont_vbox);
        /* soundfont settings - soundfont files - listview */
        soundfont_file_store = gtk_list_store_new (LISTSFONT_N_COLUMNS, G_TYPE_STRING, G_TYPE_INT);

        char * soundfont_file = aud_get_string ("amidiplug", "fsyn_soundfont_file");

        if (soundfont_file[0])
        {
            /* fill soundfont list with fsyn_soundfont_file information */
            char ** sffiles = g_strsplit (soundfont_file, ";", 0);
            GtkTreeIter iter;
            int i = 0;

            while (sffiles[i] != NULL)
            {
                int filesize = -1;
                struct stat finfo;

                if (stat (sffiles[i], &finfo) == 0)
                    filesize = finfo.st_size;

                gtk_list_store_prepend (GTK_LIST_STORE (soundfont_file_store), &iter);
                gtk_list_store_set (GTK_LIST_STORE (soundfont_file_store), &iter,
                                    LISTSFONT_FILENAME_COLUMN, sffiles[i],
                                    LISTSFONT_FILESIZE_COLUMN, filesize, -1);
                i++;
            }

            g_strfreev (sffiles);
        }

        free (soundfont_file);

        soundfont_file_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
        soundfont_file_lv = gtk_tree_view_new_with_model (GTK_TREE_MODEL (soundfont_file_store));
        gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (soundfont_file_lv), TRUE);
        g_object_unref (soundfont_file_store);
        soundfont_file_lv_text_rndr = gtk_cell_renderer_text_new();
        soundfont_file_lv_fname_col = gtk_tree_view_column_new_with_attributes (
                                          _("Filename"), soundfont_file_lv_text_rndr, "text",
                                          LISTSFONT_FILENAME_COLUMN, NULL);
        gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (soundfont_file_lv_fname_col), TRUE);
        soundfont_file_lv_fsize_col = gtk_tree_view_column_new_with_attributes (
                                          _("Size (bytes)"), soundfont_file_lv_text_rndr, "text",
                                          LISTSFONT_FILESIZE_COLUMN, NULL);
        gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (soundfont_file_lv_fsize_col), FALSE);
        gtk_tree_view_append_column (GTK_TREE_VIEW (soundfont_file_lv), soundfont_file_lv_fname_col);
        gtk_tree_view_append_column (GTK_TREE_VIEW (soundfont_file_lv), soundfont_file_lv_fsize_col);
        soundfont_file_lv_sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (soundfont_file_lv));
        gtk_tree_selection_set_mode (GTK_TREE_SELECTION (soundfont_file_lv_sel), GTK_SELECTION_SINGLE);

        soundfont_file_lv_sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) soundfont_file_lv_sw, GTK_SHADOW_IN);
        gtk_scrolled_window_set_policy ((GtkScrolledWindow *) soundfont_file_lv_sw,
                                         GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_container_add (GTK_CONTAINER (soundfont_file_lv_sw), soundfont_file_lv);

        /* soundfont settings - soundfont files - buttonbox */
        soundfont_file_bbox_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        soundfont_file_bbox_addbt = gtk_button_new();
        gtk_button_set_image (GTK_BUTTON (soundfont_file_bbox_addbt),
                              gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU));
        g_signal_connect_swapped (G_OBJECT (soundfont_file_bbox_addbt), "clicked",
                                  G_CALLBACK (i_configure_ev_sflist_add), soundfont_file_lv);
        gtk_box_pack_start (GTK_BOX (soundfont_file_bbox_vbox), soundfont_file_bbox_addbt, FALSE, FALSE, 0);
        soundfont_file_bbox_rembt = gtk_button_new();
        gtk_button_set_image (GTK_BUTTON (soundfont_file_bbox_rembt),
                              gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
        g_signal_connect_swapped (G_OBJECT (soundfont_file_bbox_rembt), "clicked",
                                  G_CALLBACK (i_configure_ev_sflist_rem), soundfont_file_lv);
        gtk_box_pack_start (GTK_BOX (soundfont_file_bbox_vbox), soundfont_file_bbox_rembt, FALSE, FALSE, 0);
        soundfont_file_bbox_mvupbt = gtk_button_new();
        gtk_button_set_image (GTK_BUTTON (soundfont_file_bbox_mvupbt),
                              gtk_image_new_from_stock (GTK_STOCK_GO_UP, GTK_ICON_SIZE_MENU));
        g_object_set_data (G_OBJECT (soundfont_file_bbox_mvupbt), "swapdire", GUINT_TO_POINTER (0));
        g_signal_connect (G_OBJECT (soundfont_file_bbox_mvupbt), "clicked",
                          G_CALLBACK (i_configure_ev_sflist_swap), soundfont_file_lv);
        gtk_box_pack_start (GTK_BOX (soundfont_file_bbox_vbox), soundfont_file_bbox_mvupbt, FALSE, FALSE, 0);
        soundfont_file_bbox_mvdownbt = gtk_button_new();
        gtk_button_set_image (GTK_BUTTON (soundfont_file_bbox_mvdownbt),
                              gtk_image_new_from_stock (GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_MENU));
        g_object_set_data (G_OBJECT (soundfont_file_bbox_mvdownbt), "swapdire", GUINT_TO_POINTER (1));
        g_signal_connect (G_OBJECT (soundfont_file_bbox_mvdownbt), "clicked",
                          G_CALLBACK (i_configure_ev_sflist_swap), soundfont_file_lv);
        gtk_box_pack_start (GTK_BOX (soundfont_file_bbox_vbox), soundfont_file_bbox_mvdownbt, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (soundfont_file_hbox), soundfont_file_lv_sw, TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (soundfont_file_hbox), soundfont_file_bbox_vbox, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (soundfont_vbox), soundfont_file_hbox, FALSE, FALSE, 0);

        gtk_box_pack_start (GTK_BOX (content_vbox), soundfont_frame, FALSE, FALSE, 0);

        /* synth settings */
        synth_frame = gtk_frame_new (_("Synthesizer settings"));
        synth_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
        gtk_container_set_border_width (GTK_CONTAINER (synth_hbox), 4);
        gtk_container_add (GTK_CONTAINER (synth_frame), synth_hbox);
        synth_leftcol_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        synth_rightcol_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_box_pack_start (GTK_BOX (synth_hbox), synth_leftcol_vbox, TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (synth_hbox), synth_rightcol_vbox, FALSE, FALSE, 0);
        /* synth settings - gain */
        synth_gain_frame = gtk_frame_new (_("gain"));
        gtk_frame_set_label_align (GTK_FRAME (synth_gain_frame), 0.5, 0.5);
        gtk_box_pack_start (GTK_BOX (synth_leftcol_vbox), synth_gain_frame, TRUE, TRUE, 0);
        synth_gain_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
        gtk_container_set_border_width (GTK_CONTAINER (synth_gain_hbox), 2);
        gtk_container_add (GTK_CONTAINER (synth_gain_frame), synth_gain_hbox);
        synth_gain_defcheckbt = gtk_check_button_new_with_label (_("use default"));
        gtk_box_pack_start (GTK_BOX (synth_gain_hbox), synth_gain_defcheckbt, FALSE, FALSE, 0);
        synth_gain_value_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
        synth_gain_value_label = gtk_label_new (_("value:"));
        synth_gain_value_spin = gtk_spin_button_new_with_range (0.0, 10.0, 0.1);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (synth_gain_value_spin), 0.2);
        g_signal_connect (G_OBJECT (synth_gain_defcheckbt), "toggled",
                          G_CALLBACK (i_configure_ev_toggle_default), synth_gain_value_hbox);

        int gain = aud_get_int ("amidiplug", "fsyn_synth_gain");
        int polyphony = aud_get_int ("amidiplug", "fsyn_synth_polyphony");
        int reverb = aud_get_int ("amidiplug", "fsyn_synth_reverb");
        int chorus = aud_get_int ("amidiplug", "fsyn_synth_chorus");

        if (gain < 0)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (synth_gain_defcheckbt), TRUE);
        else
        {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (synth_gain_defcheckbt), FALSE);
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (synth_gain_value_spin), gain / 10.0);
        }

        gtk_box_pack_start (GTK_BOX (synth_gain_hbox), synth_gain_value_hbox, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (synth_gain_value_hbox), synth_gain_value_label, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (synth_gain_value_hbox), synth_gain_value_spin, FALSE, FALSE, 0);
        /* synth settings - polyphony */
        synth_poly_frame = gtk_frame_new (_("polyphony"));
        gtk_frame_set_label_align (GTK_FRAME (synth_poly_frame), 0.5, 0.5);
        gtk_box_pack_start (GTK_BOX (synth_leftcol_vbox), synth_poly_frame, TRUE, TRUE, 0);
        synth_poly_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
        gtk_container_set_border_width (GTK_CONTAINER (synth_poly_hbox), 2);
        gtk_container_add (GTK_CONTAINER (synth_poly_frame), synth_poly_hbox);
        synth_poly_defcheckbt = gtk_check_button_new_with_label (_("use default"));
        gtk_box_pack_start (GTK_BOX (synth_poly_hbox), synth_poly_defcheckbt, FALSE, FALSE, 0);
        synth_poly_value_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
        synth_poly_value_label = gtk_label_new (_("value:"));
        synth_poly_value_spin = gtk_spin_button_new_with_range (16, 4096, 1);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (synth_poly_value_spin), 256);
        g_signal_connect (G_OBJECT (synth_poly_defcheckbt), "toggled",
                          G_CALLBACK (i_configure_ev_toggle_default), synth_poly_value_hbox);

        if (polyphony < 0)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (synth_poly_defcheckbt), TRUE);
        else
        {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (synth_poly_defcheckbt), FALSE);
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (synth_poly_value_spin), polyphony);
        }

        gtk_box_pack_start (GTK_BOX (synth_poly_hbox), synth_poly_value_hbox, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (synth_poly_value_hbox), synth_poly_value_label, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (synth_poly_value_hbox), synth_poly_value_spin, FALSE, FALSE, 0);
        /* synth settings - reverb */
        synth_reverb_frame = gtk_frame_new (_("reverb"));
        gtk_frame_set_label_align (GTK_FRAME (synth_reverb_frame), 0.5, 0.5);
        gtk_box_pack_start (GTK_BOX (synth_leftcol_vbox), synth_reverb_frame, TRUE, TRUE, 0);
        synth_reverb_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
        gtk_container_set_border_width (GTK_CONTAINER (synth_reverb_hbox), 2);
        gtk_container_add (GTK_CONTAINER (synth_reverb_frame), synth_reverb_hbox);
        synth_reverb_defcheckbt = gtk_check_button_new_with_label (_("use default"));
        gtk_box_pack_start (GTK_BOX (synth_reverb_hbox), synth_reverb_defcheckbt, FALSE, FALSE, 0);
        synth_reverb_value_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
        synth_reverb_value_option[0] = gtk_radio_button_new_with_label (NULL, _("yes"));
        synth_reverb_value_option[1] = gtk_radio_button_new_with_label_from_widget (
                                           GTK_RADIO_BUTTON (synth_reverb_value_option[0]), _("no"));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (synth_reverb_value_option[0]), TRUE);
        g_signal_connect (G_OBJECT (synth_reverb_defcheckbt), "toggled",
                          G_CALLBACK (i_configure_ev_toggle_default), synth_reverb_value_hbox);

        if (reverb < 0)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (synth_reverb_defcheckbt), TRUE);
        else
        {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (synth_reverb_defcheckbt), FALSE);

            if (reverb == 0)
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (synth_reverb_value_option[1]), TRUE);
            else
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (synth_reverb_value_option[0]), TRUE);
        }

        gtk_box_pack_start (GTK_BOX (synth_reverb_hbox), synth_reverb_value_hbox, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (synth_reverb_value_hbox), synth_reverb_value_option[0], FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (synth_reverb_value_hbox), synth_reverb_value_option[1], FALSE, FALSE, 0);
        /* synth settings - chorus */
        synth_chorus_frame = gtk_frame_new (_("chorus"));
        gtk_frame_set_label_align (GTK_FRAME (synth_chorus_frame), 0.5, 0.5);
        gtk_box_pack_start (GTK_BOX (synth_leftcol_vbox), synth_chorus_frame, TRUE, TRUE, 0);
        synth_chorus_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
        gtk_container_set_border_width (GTK_CONTAINER (synth_chorus_hbox), 2);
        gtk_container_add (GTK_CONTAINER (synth_chorus_frame), synth_chorus_hbox);
        synth_chorus_defcheckbt = gtk_check_button_new_with_label (_("use default"));
        gtk_box_pack_start (GTK_BOX (synth_chorus_hbox), synth_chorus_defcheckbt, FALSE, FALSE, 0);
        synth_chorus_value_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
        synth_chorus_value_option[0] = gtk_radio_button_new_with_label (NULL, _("yes"));
        synth_chorus_value_option[1] = gtk_radio_button_new_with_label_from_widget (
                                           GTK_RADIO_BUTTON (synth_chorus_value_option[0]), _("no"));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (synth_chorus_value_option[0]), TRUE);
        g_signal_connect (G_OBJECT (synth_chorus_defcheckbt), "toggled",
                          G_CALLBACK (i_configure_ev_toggle_default), synth_chorus_value_hbox);

        if (chorus < 0)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (synth_chorus_defcheckbt), TRUE);
        else
        {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (synth_chorus_defcheckbt), FALSE);

            if (chorus == 0)
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (synth_chorus_value_option[1]), TRUE);
            else
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (synth_chorus_value_option[0]), TRUE);
        }

        gtk_box_pack_start (GTK_BOX (synth_chorus_hbox), synth_chorus_value_hbox, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (synth_chorus_value_hbox), synth_chorus_value_option[0], FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (synth_chorus_value_hbox), synth_chorus_value_option[1], FALSE, FALSE, 0);
        /* synth settings - samplerate */
        synth_samplerate_frame = gtk_frame_new (_("sample rate"));
        gtk_frame_set_label_align (GTK_FRAME (synth_samplerate_frame), 0.5, 0.5);
        synth_samplerate_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_set_border_width (GTK_CONTAINER (synth_samplerate_vbox), 6);
        gtk_container_add (GTK_CONTAINER (synth_samplerate_frame), synth_samplerate_vbox);
        gtk_box_pack_start (GTK_BOX (synth_rightcol_vbox), synth_samplerate_frame, FALSE, FALSE, 0);
        synth_samplerate_option[0] = gtk_radio_button_new_with_label (NULL, _("22050 Hz "));
        g_object_set_data (G_OBJECT (synth_samplerate_option[0]), "val", GINT_TO_POINTER (22050));
        synth_samplerate_option[1] = gtk_radio_button_new_with_label_from_widget (
                                         GTK_RADIO_BUTTON (synth_samplerate_option[0]), _("44100 Hz "));
        g_object_set_data (G_OBJECT (synth_samplerate_option[1]), "val", GINT_TO_POINTER (44100));
        synth_samplerate_option[2] = gtk_radio_button_new_with_label_from_widget (
                                         GTK_RADIO_BUTTON (synth_samplerate_option[0]), _("96000 Hz "));
        g_object_set_data (G_OBJECT (synth_samplerate_option[2]), "val", GINT_TO_POINTER (96000));
        synth_samplerate_option[3] = gtk_radio_button_new_with_label_from_widget (
                                         GTK_RADIO_BUTTON (synth_samplerate_option[0]), _("custom "));
        synth_samplerate_optionhbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
        synth_samplerate_optionentry = gtk_entry_new();
        gtk_widget_set_sensitive (GTK_WIDGET (synth_samplerate_optionentry), FALSE);
        gtk_entry_set_width_chars (GTK_ENTRY (synth_samplerate_optionentry), 8);
        gtk_entry_set_max_length (GTK_ENTRY (synth_samplerate_optionentry), 5);
        g_object_set_data (G_OBJECT (synth_samplerate_option[3]), "customentry", synth_samplerate_optionentry);
        g_signal_connect (G_OBJECT (synth_samplerate_option[3]), "toggled",
                          G_CALLBACK (i_configure_ev_sysamplerate_togglecustom), synth_samplerate_optionentry);
        synth_samplerate_optionlabel = gtk_label_new (_("Hz "));
        gtk_box_pack_start (GTK_BOX (synth_samplerate_optionhbox), synth_samplerate_optionentry, TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (synth_samplerate_optionhbox), synth_samplerate_optionlabel, FALSE, FALSE, 0);

        int samplerate = aud_get_int ("amidiplug", "fsyn_synth_samplerate");

        switch (samplerate)
        {
        case 22050:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (synth_samplerate_option[0]), TRUE);
            break;

        case 44100:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (synth_samplerate_option[1]), TRUE);
            break;

        case 96000:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (synth_samplerate_option[2]), TRUE);
            break;

        default:
            if (samplerate > 22050 && samplerate < 96000)
            {
                char * samplerate_value = g_strdup_printf ("%i", samplerate);
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (synth_samplerate_option[3]), TRUE);
                gtk_entry_set_text (GTK_ENTRY (synth_samplerate_optionentry), samplerate_value);
                g_free (samplerate_value);
            }
            else
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (synth_samplerate_option[1]), TRUE);
        }

        gtk_box_pack_start (GTK_BOX (synth_samplerate_vbox), synth_samplerate_option[0], FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (synth_samplerate_vbox), synth_samplerate_option[1], FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (synth_samplerate_vbox), synth_samplerate_option[2], FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (synth_samplerate_vbox), synth_samplerate_option[3], FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (synth_samplerate_vbox), synth_samplerate_optionhbox, FALSE, FALSE, 0);

        gtk_box_pack_start (GTK_BOX (content_vbox), synth_frame, TRUE, TRUE, 0);

        /* commit events  */
        g_signal_connect_swapped (G_OBJECT (commit_button), "ap-commit",
                                  G_CALLBACK (i_configure_ev_sflist_commit), soundfont_file_lv);
        g_signal_connect_swapped (G_OBJECT (commit_button), "ap-commit",
                                  G_CALLBACK (i_configure_ev_sygain_commit), synth_gain_value_spin);
        g_signal_connect_swapped (G_OBJECT (commit_button), "ap-commit",
                                  G_CALLBACK (i_configure_ev_sypoly_commit), synth_poly_value_spin);
        g_signal_connect_swapped (G_OBJECT (commit_button), "ap-commit",
                                  G_CALLBACK (i_configure_ev_syreverb_commit), synth_reverb_value_option[0]);
        g_signal_connect_swapped (G_OBJECT (commit_button), "ap-commit",
                                  G_CALLBACK (i_configure_ev_sychorus_commit), synth_chorus_value_option[0]);
        g_signal_connect_swapped (G_OBJECT (commit_button), "ap-commit",
                                  G_CALLBACK (i_configure_ev_sysamplerate_commit), synth_samplerate_option[3]);
    }

    gtk_container_add ((GtkContainer *) fsyn_page_alignment, content_vbox);
}


void i_configure_gui_tablabel_fsyn (GtkWidget * fsyn_page_alignment,
                                    void * commit_button)
{
    GtkWidget * pagelabel_vbox, *pagelabel_image, *pagelabel_label;
    GdkPixbuf * pagelabel_image_pix;
    pagelabel_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
    pagelabel_image_pix = gdk_pixbuf_new_from_xpm_data ((const char **) backend_fluidsynth_icon_xpm);
    pagelabel_image = gtk_image_new_from_pixbuf (pagelabel_image_pix);
    g_object_unref (pagelabel_image_pix);
    pagelabel_label = gtk_label_new ("");
    gtk_label_set_markup (GTK_LABEL (pagelabel_label), _("<span size=\"smaller\">FluidSynth\nbackend</span>"));
    gtk_label_set_justify (GTK_LABEL (pagelabel_label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start (GTK_BOX (pagelabel_vbox), pagelabel_image, FALSE, FALSE, 1);
    gtk_box_pack_start (GTK_BOX (pagelabel_vbox), pagelabel_label, FALSE, FALSE, 1);
    gtk_container_add (GTK_CONTAINER (fsyn_page_alignment), pagelabel_vbox);
    gtk_widget_show_all (fsyn_page_alignment);
    return;
}
