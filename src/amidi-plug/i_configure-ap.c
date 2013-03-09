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


void i_configure_ev_backendlv_info (void * backend_lv)
{
    GtkTreeModel * store;
    GtkTreeIter iter;
    GtkTreeSelection * sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (backend_lv));

    /* get the selected item */
    if (gtk_tree_selection_get_selected (sel, &store, &iter))
    {
        GtkWidget * bidialog;
        GtkWidget * title_label;
        GtkWidget * filename_label;
        GtkWidget * description_label;
        GtkWidget * parent_window = gtk_widget_get_toplevel (backend_lv);
        char * longname_title, *longname, *filename, *description;
        gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                            LISTBACKEND_LONGNAME_COLUMN, &longname,
                            LISTBACKEND_DESC_COLUMN, &description,
                            LISTBACKEND_FILENAME_COLUMN, &filename, -1);
        bidialog = gtk_dialog_new_with_buttons (_("AMIDI-Plug - backend information"),
                                                GTK_WINDOW (parent_window),
                                                GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                                GTK_STOCK_OK, GTK_RESPONSE_NONE, NULL);
        gtk_window_set_resizable ((GtkWindow *) bidialog, FALSE);

        longname_title = g_markup_printf_escaped ("<span size=\"larger\" weight=\"bold\" >%s</span>", longname);
        title_label = gtk_label_new ("");
        gtk_label_set_markup (GTK_LABEL (title_label), longname_title);
        free (longname_title);
        free (longname);
        gtk_box_pack_start ((GtkBox *) gtk_dialog_get_content_area ((GtkDialog *)
                             bidialog), title_label, FALSE, FALSE, 0);

        filename_label = gtk_label_new (filename);
        free (filename);
        gtk_box_pack_start ((GtkBox *) gtk_dialog_get_content_area ((GtkDialog *)
                             bidialog), filename_label, FALSE, FALSE, 0);

        description_label = gtk_label_new (description);
        gtk_label_set_line_wrap (GTK_LABEL (description_label), TRUE);
        free (description);
        gtk_box_pack_start ((GtkBox *) gtk_dialog_get_content_area ((GtkDialog *)
                             bidialog), description_label, FALSE, FALSE, 0);

        gtk_widget_show_all (bidialog);
        gtk_dialog_run (GTK_DIALOG (bidialog));
        gtk_widget_destroy (bidialog);
    }

    return;
}


void i_configure_ev_backendlv_commit (void * backend_lv)
{
    GtkTreeModel * store;
    GtkTreeIter iter;
    GtkTreeSelection * sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (backend_lv));

    /* get the selected item */
    if (gtk_tree_selection_get_selected (sel, &store, &iter))
    {
        free (amidiplug_cfg_ap->ap_seq_backend);  /* free previous */
        /* update amidiplug_cfg_ap->ap_seq_backend */
        gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                            LISTBACKEND_NAME_COLUMN, &amidiplug_cfg_ap->ap_seq_backend, -1);
    }

    return;
}


void i_configure_ev_settplay_commit (void * settplay_vbox)
{
    GtkWidget * settplay_transpose_spinbt = g_object_get_data (G_OBJECT (settplay_vbox),
                                            "ap_opts_transpose_value");
    GtkWidget * settplay_drumshift_spinbt = g_object_get_data (G_OBJECT (settplay_vbox),
                                            "ap_opts_drumshift_value");
    amidiplug_cfg_ap->ap_opts_transpose_value = gtk_spin_button_get_value_as_int (
                GTK_SPIN_BUTTON (settplay_transpose_spinbt));
    amidiplug_cfg_ap->ap_opts_drumshift_value = gtk_spin_button_get_value_as_int (
                GTK_SPIN_BUTTON (settplay_drumshift_spinbt));
    return;
}


void i_configure_ev_settadva_commit (void * settadva_vbox)
{
    GtkWidget * settadva_precalc_checkbt = g_object_get_data (G_OBJECT (settadva_vbox),
                                           "ap_opts_length_precalc");
    GtkWidget * settadva_extractlyr_checkbt = g_object_get_data (G_OBJECT (settadva_vbox),
            "ap_opts_lyrics_extract");
    GtkWidget * settadva_extractcomm_checkbt = g_object_get_data (G_OBJECT (settadva_vbox),
            "ap_opts_comments_extract");

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (settadva_precalc_checkbt)))
        amidiplug_cfg_ap->ap_opts_length_precalc = 1;
    else
        amidiplug_cfg_ap->ap_opts_length_precalc = 0;

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (settadva_extractlyr_checkbt)))
        amidiplug_cfg_ap->ap_opts_lyrics_extract = 1;
    else
        amidiplug_cfg_ap->ap_opts_lyrics_extract = 0;

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (settadva_extractcomm_checkbt)))
        amidiplug_cfg_ap->ap_opts_comments_extract = 1;
    else
        amidiplug_cfg_ap->ap_opts_comments_extract = 0;

    return;
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
                             void * backend_list_p,
                             void * commit_button)
{
    GtkWidget * settings_vbox; /* this vbox will contain all settings vbox (playback, advanced) */
    GtkWidget * settplay_frame, *settplay_vbox;
    GtkWidget * settplay_transpose_and_drumshift_hbox;
    GtkWidget * settplay_transpose_hbox, *settplay_transpose_label1, *settplay_transpose_spinbt;
    GtkWidget * settplay_drumshift_hbox, *settplay_drumshift_label1, *settplay_drumshift_spinbt;
    GtkWidget * settadva_frame, *settadva_vbox;
    GtkWidget * settadva_precalc_checkbt, *settadva_extractcomm_checkbt, *settadva_extractlyr_checkbt;
    GtkWidget * backend_lv_frame, *backend_lv, *backend_lv_sw;
    GtkWidget * backend_lv_hbox, *backend_lv_vbbox, *backend_lv_infobt;
    GtkListStore * backend_store;
    GtkCellRenderer * backend_lv_text_rndr;
    GtkTreeViewColumn * backend_lv_name_col;
    GtkTreeIter backend_lv_iter_selected;
    GtkTreeSelection * backend_lv_sel;
    GtkTreeIter iter;
    GSList * backend_list = backend_list_p;

    GtkWidget * content_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

    backend_store = gtk_list_store_new (LISTBACKEND_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING,
                                        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
    gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (backend_store),
            i_configure_backendlist_sortf, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (backend_store),
                                          GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                          GTK_SORT_ASCENDING);

    bool_t backend_lv_iter_selected_valid = FALSE;

    while (backend_list != NULL)
    {
        amidiplug_sequencer_backend_name_t * mn = backend_list->data;
        gtk_list_store_append (backend_store, &iter);
        gtk_list_store_set (backend_store, &iter,
                            LISTBACKEND_NAME_COLUMN, mn->name,
                            LISTBACKEND_LONGNAME_COLUMN, mn->longname,
                            LISTBACKEND_DESC_COLUMN, mn->desc,
                            LISTBACKEND_FILENAME_COLUMN, mn->filename,
                            LISTBACKEND_PPOS_COLUMN, mn->ppos, -1);

        if (!strcmp (mn->name, amidiplug_cfg_ap->ap_seq_backend))
        {
            backend_lv_iter_selected = iter;
            backend_lv_iter_selected_valid = TRUE;
        }

        backend_list = backend_list->next;
    }

    backend_lv_frame = gtk_frame_new (_("Backend selection"));

    backend_lv = gtk_tree_view_new_with_model (GTK_TREE_MODEL (backend_store));
    gtk_tree_view_set_headers_visible ((GtkTreeView *) backend_lv, FALSE);
    g_object_unref (backend_store);

    backend_lv_text_rndr = gtk_cell_renderer_text_new();
    backend_lv_name_col = gtk_tree_view_column_new_with_attributes (NULL,
                          backend_lv_text_rndr,
                          "text", 1, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (backend_lv), backend_lv_name_col);

    backend_lv_sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (backend_lv));
    gtk_tree_selection_set_mode (GTK_TREE_SELECTION (backend_lv_sel), GTK_SELECTION_BROWSE);

    if (backend_lv_iter_selected_valid)
        gtk_tree_selection_select_iter (backend_lv_sel, & backend_lv_iter_selected);

    backend_lv_sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) backend_lv_sw, GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy ((GtkScrolledWindow *) backend_lv_sw,
                                     GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (backend_lv_sw), backend_lv);
    g_signal_connect_swapped (G_OBJECT (commit_button), "ap-commit",
                              G_CALLBACK (i_configure_ev_backendlv_commit), backend_lv);

    backend_lv_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (backend_lv_hbox), backend_lv_sw, TRUE, TRUE, 0);
    backend_lv_vbbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start (GTK_BOX (backend_lv_hbox), backend_lv_vbbox, FALSE, FALSE, 3);
    backend_lv_infobt = gtk_button_new();
    gtk_button_set_image (GTK_BUTTON (backend_lv_infobt),
                          gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_BUTTON));
    g_signal_connect_swapped (G_OBJECT (backend_lv_infobt), "clicked",
                              G_CALLBACK (i_configure_ev_backendlv_info), backend_lv);
    gtk_box_pack_start (GTK_BOX (backend_lv_vbbox), backend_lv_infobt, FALSE, FALSE, 0);
    gtk_container_add (GTK_CONTAINER (backend_lv_frame), backend_lv_hbox);

    settings_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

    settplay_frame = gtk_frame_new (_("Playback settings"));
    settplay_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width (GTK_CONTAINER (settplay_vbox), 4);
    settplay_transpose_and_drumshift_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    settplay_transpose_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    settplay_transpose_label1 = gtk_label_new (_("Transpose: "));
    settplay_transpose_spinbt = gtk_spin_button_new_with_range (-20, 20, 1);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (settplay_transpose_spinbt),
                               amidiplug_cfg_ap->ap_opts_transpose_value);
    gtk_box_pack_start (GTK_BOX (settplay_transpose_hbox), settplay_transpose_label1, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (settplay_transpose_hbox), settplay_transpose_spinbt, FALSE, FALSE, 2);
    gtk_box_pack_start (GTK_BOX (settplay_transpose_and_drumshift_hbox),
                        settplay_transpose_hbox, FALSE, FALSE, 0);
    settplay_drumshift_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    settplay_drumshift_label1 = gtk_label_new (_("Drum shift: "));
    settplay_drumshift_spinbt = gtk_spin_button_new_with_range (0, 127, 1);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (settplay_drumshift_spinbt),
                               amidiplug_cfg_ap->ap_opts_drumshift_value);
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
    settadva_precalc_checkbt = gtk_check_button_new_with_label (
                                   _("pre-calculate length of MIDI files in playlist"));

    if (amidiplug_cfg_ap->ap_opts_length_precalc)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (settadva_precalc_checkbt), TRUE);

    gtk_box_pack_start (GTK_BOX (settadva_vbox), settadva_precalc_checkbt, FALSE, FALSE, 2);
    settadva_extractcomm_checkbt = gtk_check_button_new_with_label (
                                       _("extract comments from MIDI file (if available)"));

    if (amidiplug_cfg_ap->ap_opts_comments_extract)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (settadva_extractcomm_checkbt), TRUE);

    gtk_box_pack_start (GTK_BOX (settadva_vbox), settadva_extractcomm_checkbt, FALSE, FALSE, 2);
    settadva_extractlyr_checkbt = gtk_check_button_new_with_label (
                                      _("extract lyrics from MIDI file (if available)"));

    if (amidiplug_cfg_ap->ap_opts_lyrics_extract)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (settadva_extractlyr_checkbt), TRUE);

    gtk_box_pack_start (GTK_BOX (settadva_vbox), settadva_extractlyr_checkbt, FALSE, FALSE, 2);
    gtk_container_add (GTK_CONTAINER (settadva_frame), settadva_vbox);
    /* attach pointers of options to settadva_vbox so we can handle all of them in a single callback */
    g_object_set_data (G_OBJECT (settadva_vbox), "ap_opts_length_precalc", settadva_precalc_checkbt);
    g_object_set_data (G_OBJECT (settadva_vbox), "ap_opts_comments_extract", settadva_extractcomm_checkbt);
    g_object_set_data (G_OBJECT (settadva_vbox), "ap_opts_lyrics_extract", settadva_extractlyr_checkbt);
    g_signal_connect_swapped (G_OBJECT (commit_button), "ap-commit",
                              G_CALLBACK (i_configure_ev_settadva_commit), settadva_vbox);
    gtk_box_pack_start (GTK_BOX (settings_vbox), settadva_frame, TRUE, TRUE, 0);

    gtk_box_pack_start (GTK_BOX (content_vbox), backend_lv_frame, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (content_vbox), settings_vbox, TRUE, TRUE, 0);

    gtk_container_add ((GtkContainer *) ap_page_alignment, content_vbox);
}


void i_configure_gui_tablabel_ap (GtkWidget * ap_page_alignment,
                                  void * backend_list_p,
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
