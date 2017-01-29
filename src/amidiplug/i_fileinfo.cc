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

#include "i_fileinfo.h"

#ifdef USE_GTK

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>

#include "i_configure.h"
/* this is needed to retrieve information */
#include "i_midi.h"
/* icon from gnome-mime-audio-midi.png of the GNOME ICON SET */
#include "amidi-plug.midiicon.xpm"


void i_fileinfo_ev_close (GtkWidget * button, void * fileinfowin)
{
    gtk_widget_destroy (GTK_WIDGET (fileinfowin));
}


void i_fileinfo_grid_add_entry (char * field_text, char * value_text,
                                GtkWidget * grid, unsigned line, PangoAttrList * attrlist)
{
    GtkWidget * field, *value;
    field = gtk_label_new (field_text);
    gtk_label_set_attributes (GTK_LABEL (field), attrlist);
    gtk_misc_set_alignment (GTK_MISC (field), 0, 0);
    gtk_table_attach (GTK_TABLE (grid), field, 0, 1, line, line + 1, GTK_FILL, GTK_FILL, 0, 0);
    value = gtk_label_new (value_text);
    gtk_misc_set_alignment (GTK_MISC (value), 0, 0);
    gtk_table_attach (GTK_TABLE (grid), value, 1, 2, line, line + 1, GTK_FILL, GTK_FILL, 0, 0);
}


/* COMMENT: this will also reset current position in each track! */
void i_fileinfo_text_fill (midifile_t * mf, GtkTextBuffer * text_tb, GtkTextBuffer * lyrics_tb)
{
    /* initialize current position in each track */
    for (midifile_track_t & track : mf->tracks)
        track.current_event = track.events.head ();

    for (;;)
    {
        midievent_t * event = nullptr;
        midifile_track_t * event_track = nullptr;
        int min_tick = INT_MAX;  /* meta-events may go past max_tick */

        /* search next event */
        for (midifile_track_t & track : mf->tracks)
        {
            midievent_t * e2 = track.current_event;

            if (e2 && e2->tick < min_tick)
            {
                min_tick = e2->tick;
                event = e2;
                event_track = & track;
            }
        }

        if (!event)
            break; /* end of song reached */

        /* advance pointer to next event */
        event_track->current_event = event_track->events.next (event);

        switch (event->type)
        {
        case SND_SEQ_EVENT_META_TEXT:
            gtk_text_buffer_insert_at_cursor (text_tb, event->metat, -1);
            break;

        case SND_SEQ_EVENT_META_LYRIC:
            gtk_text_buffer_insert_at_cursor (lyrics_tb, event->metat, -1);
            break;
        }
    }
}


bool i_fileinfo_gui (const char * filename_uri, VFSFile & file)
{
    // TODO: Qt support
    if (aud_get_mainloop_type () != MainloopType::GLib)
        return false;

    static GtkWidget * fileinfowin = nullptr;
    GtkWidget * fileinfowin_vbox, *fileinfowin_columns_hbox;
    GtkWidget * midiinfoboxes_vbox, *miditextboxes_vbox, *miditextboxes_paned;
    GtkWidget * title_hbox, *title_icon_image, *title_name_f_label, *title_name_v_entry;
    GtkWidget * info_frame, *info_frame_tl, *info_grid;
    GtkWidget * text_frame, *text_frame_tl, *text_tv, *text_tv_sw;
    GtkWidget * lyrics_frame, *lyrics_tv, *lyrics_tv_sw;
    GtkTextBuffer * text_tb, *lyrics_tb;
    GtkWidget * footer_hbbox, *footer_bclose;
    GdkPixbuf * title_icon_pixbuf;
    PangoAttrList * pangoattrlist;
    PangoAttribute * pangoattr;
    GString * value_gstring;
    char * title, *filename, *filename_utf8;
    int bpm = 0, wavg_bpm = 0;

    if (fileinfowin)
        return true;

    midifile_t mf;

    /****************** midifile parser ******************/
    if (! mf.parse_from_file (filename_uri, file))
        return false;

    /* midifile is filled with information at this point,
       bpm information is needed too */
    mf.get_bpm (& bpm, & wavg_bpm);
    /*****************************************************/

    fileinfowin = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size (GTK_WINDOW (fileinfowin), 500, 400);
    gtk_window_set_type_hint (GTK_WINDOW (fileinfowin), GDK_WINDOW_TYPE_HINT_DIALOG);
    g_signal_connect (G_OBJECT (fileinfowin), "destroy", G_CALLBACK (gtk_widget_destroyed), &fileinfowin);
    gtk_container_set_border_width (GTK_CONTAINER (fileinfowin), 10);

    fileinfowin_vbox = gtk_vbox_new (false, 10);
    gtk_container_add (GTK_CONTAINER (fileinfowin), fileinfowin_vbox);

    /* pango attributes */
    pangoattrlist = pango_attr_list_new();
    pangoattr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
    pangoattr->start_index = 0;
    pangoattr->end_index = G_MAXINT;
    pango_attr_list_insert (pangoattrlist, pangoattr);

    /******************
     *** TITLE LINE ***/
    title_hbox = gtk_hbox_new (false, 5);
    gtk_box_pack_start (GTK_BOX (fileinfowin_vbox), title_hbox, false, false, 0);

    title_icon_pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) amidiplug_xpm_midiicon);
    title_icon_image = gtk_image_new_from_pixbuf (title_icon_pixbuf);
    g_object_unref (title_icon_pixbuf);
    gtk_misc_set_alignment (GTK_MISC (title_icon_image), 0, 0);
    gtk_box_pack_start (GTK_BOX (title_hbox), title_icon_image, false, false, 0);

    title_name_f_label = gtk_label_new (_("Name:"));
    gtk_label_set_attributes (GTK_LABEL (title_name_f_label), pangoattrlist);
    gtk_box_pack_start (GTK_BOX (title_hbox), title_name_f_label, false, false, 0);

    title_name_v_entry = gtk_entry_new();
    gtk_editable_set_editable (GTK_EDITABLE (title_name_v_entry), false);
    gtk_widget_set_size_request (GTK_WIDGET (title_name_v_entry), 200, -1);
    gtk_box_pack_start (GTK_BOX (title_hbox), title_name_v_entry, true, true, 0);

    fileinfowin_columns_hbox = gtk_hbox_new (false, 2);
    gtk_box_pack_start (GTK_BOX (fileinfowin_vbox), fileinfowin_columns_hbox, true, true, 0);

    /*********************
     *** MIDI INFO BOX ***/
    midiinfoboxes_vbox = gtk_vbox_new (false, 2);
    gtk_box_pack_start (GTK_BOX (fileinfowin_columns_hbox), midiinfoboxes_vbox, false, false, 0);

    info_frame_tl = gtk_label_new ("");
    gtk_label_set_markup (GTK_LABEL (info_frame_tl), _("<span size=\"smaller\"> MIDI Info </span>"));
    gtk_box_pack_start (GTK_BOX (midiinfoboxes_vbox), info_frame_tl, false, false, 0);

    info_frame = gtk_frame_new (nullptr);
    gtk_box_pack_start (GTK_BOX (midiinfoboxes_vbox), info_frame, true, true, 0);
    info_grid = gtk_table_new (0, 0, false);
    gtk_table_set_row_spacings (GTK_TABLE (info_grid), 2);
    gtk_table_set_col_spacings (GTK_TABLE (info_grid), 6);
    gtk_container_set_border_width (GTK_CONTAINER (info_grid), 6);
    gtk_container_add (GTK_CONTAINER (info_frame), info_grid);
    value_gstring = g_string_new ("");

    /* midi format */
    g_string_printf (value_gstring, "type %i", mf.format);
    i_fileinfo_grid_add_entry (_("Format:"), value_gstring->str, info_grid, 0, pangoattrlist);
    /* midi length */
    g_string_printf (value_gstring, "%i", (int) (mf.length / 1000));
    i_fileinfo_grid_add_entry (_("Length (msec):"), value_gstring->str, info_grid, 1, pangoattrlist);
    /* midi num of tracks */
    g_string_printf (value_gstring, "%i", mf.tracks.len ());
    i_fileinfo_grid_add_entry (_("No. of Tracks:"), value_gstring->str, info_grid, 2, pangoattrlist);

    /* midi bpm */
    if (bpm > 0)
        g_string_printf (value_gstring, "%i", bpm);  /* fixed bpm */
    else
        g_string_printf (value_gstring, _("variable")); /* variable bpm */

    i_fileinfo_grid_add_entry (_("BPM:"), value_gstring->str, info_grid, 3, pangoattrlist);

    /* midi weighted average bpm */
    if (bpm > 0)
        g_string_printf (value_gstring, "/");  /* fixed bpm, don't care about wavg_bpm */
    else
        g_string_printf (value_gstring, "%i", wavg_bpm);  /* variable bpm, display wavg_bpm */

    i_fileinfo_grid_add_entry (_("BPM (wavg):"), value_gstring->str, info_grid, 4, pangoattrlist);
    /* midi time division */
    g_string_printf (value_gstring, "%i", mf.time_division);
    i_fileinfo_grid_add_entry (_("Time Div:"), value_gstring->str, info_grid, 5, pangoattrlist);

    g_string_free (value_gstring, true);

    /**********************************
     *** MIDI COMMENTS/LYRICS BOXES ***/
    miditextboxes_vbox = gtk_vbox_new (false, 2);
    gtk_box_pack_start (GTK_BOX (fileinfowin_columns_hbox), miditextboxes_vbox, true, true, 0);

    text_frame_tl = gtk_label_new ("");
    gtk_label_set_markup (GTK_LABEL (text_frame_tl),
                          _("<span size=\"smaller\"> MIDI Comments and Lyrics </span>"));
    gtk_box_pack_start (GTK_BOX (miditextboxes_vbox), text_frame_tl, false, false, 0);

    miditextboxes_paned = gtk_vpaned_new ();
    gtk_box_pack_start (GTK_BOX (miditextboxes_vbox), miditextboxes_paned, true, true, 0);

    text_frame = gtk_frame_new (nullptr);
    gtk_paned_pack1 (GTK_PANED (miditextboxes_paned), text_frame, true, true);
    text_tv = gtk_text_view_new();
    gtk_text_view_set_editable (GTK_TEXT_VIEW (text_tv), false);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (text_tv), false);
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_tv), GTK_WRAP_WORD);
    gtk_text_view_set_right_margin (GTK_TEXT_VIEW (text_tv), 4);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW (text_tv), 4);
    gtk_widget_set_size_request (text_tv, 300, 113);
    text_tv_sw = gtk_scrolled_window_new (nullptr, nullptr);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (text_tv_sw),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (text_frame), text_tv_sw);
    gtk_container_add (GTK_CONTAINER (text_tv_sw), text_tv);

    lyrics_frame = gtk_frame_new (nullptr);
    gtk_paned_pack2 (GTK_PANED (miditextboxes_paned), lyrics_frame, true, true);
    lyrics_tv = gtk_text_view_new();
    gtk_text_view_set_editable (GTK_TEXT_VIEW (lyrics_tv), false);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (lyrics_tv), false);
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (lyrics_tv), GTK_WRAP_WORD);
    gtk_text_view_set_right_margin (GTK_TEXT_VIEW (lyrics_tv), 4);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW (lyrics_tv), 4);
    gtk_widget_set_size_request (lyrics_tv, 300, 113);
    lyrics_tv_sw = gtk_scrolled_window_new (nullptr, nullptr);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (lyrics_tv_sw),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (lyrics_frame), lyrics_tv_sw);
    gtk_container_add (GTK_CONTAINER (lyrics_tv_sw), lyrics_tv);

    text_tb = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_tv));
    lyrics_tb = gtk_text_view_get_buffer (GTK_TEXT_VIEW (lyrics_tv));

    i_fileinfo_text_fill (& mf, text_tb, lyrics_tb);

    if (! gtk_text_buffer_get_char_count (text_tb))
    {
        GtkTextIter start, end;
        GtkTextTag * tag = gtk_text_buffer_create_tag (text_tb, "italicstyle",
                           "style", PANGO_STYLE_ITALIC, nullptr);
        /*gtk_text_view_set_justification( GTK_TEXT_VIEW(text_tv), GTK_JUSTIFY_CENTER );*/
        gtk_text_buffer_set_text (text_tb, _("* no comments available in this MIDI file *"), -1);
        gtk_text_buffer_get_iter_at_offset (text_tb, &start, 0);
        gtk_text_buffer_get_iter_at_offset (text_tb, &end, -1);
        gtk_text_buffer_apply_tag (text_tb, tag, &start, &end);
    }

    if (! gtk_text_buffer_get_char_count (lyrics_tb))
    {
        GtkTextIter start, end;
        GtkTextTag * tag = gtk_text_buffer_create_tag (lyrics_tb, "italicstyle",
                           "style", PANGO_STYLE_ITALIC, nullptr);
        /*gtk_text_view_set_justification( GTK_TEXT_VIEW(lyrics_tv), GTK_JUSTIFY_CENTER );*/
        gtk_text_buffer_set_text (lyrics_tb, _("* no lyrics available in this MIDI file *"), -1);
        gtk_text_buffer_get_iter_at_offset (lyrics_tb, &start, 0);
        gtk_text_buffer_get_iter_at_offset (lyrics_tb, &end, -1);
        gtk_text_buffer_apply_tag (lyrics_tb, tag, &start, &end);
    }

    /**************
     *** FOOTER ***/
    footer_hbbox = gtk_hbutton_box_new ();
    gtk_button_box_set_layout (GTK_BUTTON_BOX (footer_hbbox), GTK_BUTTONBOX_END);
    footer_bclose = gtk_button_new_with_mnemonic (_("_Close"));
    g_signal_connect (G_OBJECT (footer_bclose), "clicked", G_CALLBACK (i_fileinfo_ev_close), fileinfowin);
    gtk_container_add (GTK_CONTAINER (footer_hbbox), footer_bclose);
    gtk_box_pack_start (GTK_BOX (fileinfowin_vbox), footer_hbbox, false, false, 0);


    /* utf8-ize filename and set window title */
    filename = g_filename_from_uri (filename_uri, nullptr, nullptr);

    if (!filename)
        filename = g_strdup (filename_uri);

    filename_utf8 = g_strdup (g_filename_to_utf8 (filename, -1, nullptr, nullptr, nullptr));

    if (!filename_utf8)
    {
        /* utf8 fallback */
        char * chr, *convert_str = g_strdup (filename);

        for (chr = convert_str ; *chr ; chr++)
        {
            if (*chr & 0x80)
                *chr = '?';
        }

        filename_utf8 = g_strconcat (convert_str, _("  (invalid UTF-8)"), nullptr);
        g_free (convert_str);
    }

    title = g_path_get_basename (filename_utf8);
    gtk_window_set_title (GTK_WINDOW (fileinfowin), title);
    g_free (title);
    /* set the text for the filename header too */
    gtk_entry_set_text (GTK_ENTRY (title_name_v_entry), filename_utf8);
    gtk_editable_set_position (GTK_EDITABLE (title_name_v_entry), -1);
    g_free (filename_utf8);
    g_free (filename);

    gtk_widget_grab_focus (GTK_WIDGET (footer_bclose));
    gtk_widget_show_all (fileinfowin);

    return true;
}

#endif // USE_GTK
