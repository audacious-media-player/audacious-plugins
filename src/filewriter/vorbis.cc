/*  FileWriter Vorbis Plugin
 *  Copyright (c) 2007 William Pitcock <nenolod@sacredspiral.co.uk>
 *
 *  Partially derived from Og(g)re - Ogg-Output-Plugin:
 *  Copyright (c) 2002 Lars Siebold <khandha5@gmx.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "plugins.h"

#ifdef FILEWRITER_VORBIS

#include <stdlib.h>
#include <vorbis/vorbisenc.h>
#include <gtk/gtk.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>

static ogg_stream_state os;
static ogg_page og;
static ogg_packet op;

static vorbis_dsp_state vd;
static vorbis_block vb;
static vorbis_info vi;
static vorbis_comment vc;

static const char * const vorbis_defaults[] = {
 "base_quality", "0.5",
 nullptr};

static double v_base_quality;
static int channels;

static void vorbis_init ()
{
    aud_config_set_defaults ("filewriter_vorbis", vorbis_defaults);

    v_base_quality = aud_get_double ("filewriter_vorbis", "base_quality");
}

static void add_string_from_tuple (vorbis_comment * vc, const char * name,
 const Tuple & tuple, Tuple::Field field)
{
    String val = tuple.get_str (field);
    if (val)
        vorbis_comment_add_tag (vc, name, val);
}

static bool vorbis_open (VFSFile & file, const format_info & info, const Tuple & tuple)
{
    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;

    vorbis_init();

    vorbis_info_init(&vi);
    vorbis_comment_init(&vc);

    int scrint;

    add_string_from_tuple (& vc, "title", tuple, Tuple::Title);
    add_string_from_tuple (& vc, "artist", tuple, Tuple::Artist);
    add_string_from_tuple (& vc, "album", tuple, Tuple::Album);
    add_string_from_tuple (& vc, "genre", tuple, Tuple::Genre);
    add_string_from_tuple (& vc, "date", tuple, Tuple::Date);
    add_string_from_tuple (& vc, "comment", tuple, Tuple::Comment);

    if ((scrint = tuple.get_int (Tuple::Track)) > 0)
        vorbis_comment_add_tag(&vc, "tracknumber", int_to_str (scrint));

    if ((scrint = tuple.get_int (Tuple::Year)) > 0)
        vorbis_comment_add_tag(&vc, "year", int_to_str (scrint));

    if (vorbis_encode_init_vbr (& vi, info.channels, info.frequency, v_base_quality))
    {
        vorbis_info_clear(&vi);
        return false;
    }

    vorbis_analysis_init(&vd, &vi);
    vorbis_block_init(&vd, &vb);

    ogg_stream_init(&os, rand());

    vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);

    ogg_stream_packetin(&os, &header);
    ogg_stream_packetin(&os, &header_comm);
    ogg_stream_packetin(&os, &header_code);

    while (ogg_stream_flush (& os, & og))
    {
        if (file.fwrite (og.header, 1, og.header_len) != og.header_len ||
         file.fwrite (og.body, 1, og.body_len) != og.body_len)
            AUDERR ("write error\n");
    }

    channels = info.channels;
    return true;
}

static void vorbis_write_real (VFSFile & file, const void * data, int length)
{
    int samples = length / sizeof (float);
    int channel;
    float * end = (float *) data + samples;
    float * * buffer = vorbis_analysis_buffer (& vd, samples / channels);
    float * from, * to;

    for (channel = 0; channel < channels; channel ++)
    {
        to = buffer[channel];

        for (from = (float *) data + channel; from < end; from += channels)
            * to ++ = * from;
    }

    vorbis_analysis_wrote (& vd, samples / channels);

    while(vorbis_analysis_blockout(&vd, &vb) == 1)
    {
        vorbis_analysis(&vb, &op);
        vorbis_bitrate_addblock(&vb);

        while (vorbis_bitrate_flushpacket(&vd, &op))
        {
            ogg_stream_packetin(&os, &op);

            while (ogg_stream_pageout(&os, &og))
            {
                if (file.fwrite (og.header, 1, og.header_len) != og.header_len ||
                 file.fwrite (og.body, 1, og.body_len) != og.body_len)
                    AUDERR ("write error\n");
            }
        }
    }
}

static void vorbis_write (VFSFile & file, const void * data, int length)
{
    if (length > 0) /* don't signal end of file yet */
        vorbis_write_real (file, data, length);
}

static void vorbis_close (VFSFile & file)
{
    vorbis_write_real (file, nullptr, 0); /* signal end of file */

    while (ogg_stream_flush (& os, & og))
    {
        if (file.fwrite (og.header, 1, og.header_len) != og.header_len ||
         file.fwrite (og.body, 1, og.body_len) != og.body_len)
            AUDERR ("write error\n");
    }

    ogg_stream_clear(&os);

    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_info_clear(&vi);
}

/* configuration stuff */
static GtkWidget *configure_win = nullptr;
static GtkWidget *quality_frame, *quality_vbox, *quality_hbox1, *quality_spin, *quality_label;
static GtkAdjustment * quality_adj;

static void quality_change(GtkAdjustment *adjustment, void * user_data)
{
    v_base_quality = gtk_spin_button_get_value ((GtkSpinButton *) quality_spin) / 10;
    aud_set_double ("filewriter_vorbis", "base_quality", v_base_quality);
}

static void vorbis_configure(void)
{
    if (! configure_win)
    {
        configure_win = gtk_dialog_new_with_buttons
         (_("Vorbis Encoder Configuration"), nullptr, (GtkDialogFlags) 0,
          _("_Close"), GTK_RESPONSE_CLOSE, nullptr);

        g_signal_connect (configure_win, "response", (GCallback) gtk_widget_destroy, nullptr);
        g_signal_connect (configure_win, "destroy", (GCallback)
         gtk_widget_destroyed, & configure_win);

        GtkWidget * vbox = gtk_dialog_get_content_area ((GtkDialog *) configure_win);

        /* quality options */
        quality_frame = gtk_frame_new(_("Quality"));
        gtk_container_set_border_width(GTK_CONTAINER(quality_frame), 5);
        gtk_box_pack_start(GTK_BOX(vbox), quality_frame, false, false, 2);

        quality_vbox = gtk_vbox_new (false, 5);
        gtk_container_set_border_width(GTK_CONTAINER(quality_vbox), 10);
        gtk_container_add(GTK_CONTAINER(quality_frame), quality_vbox);

        /* quality option: vbr level */
        quality_hbox1 = gtk_hbox_new (false, 5);
        gtk_container_set_border_width(GTK_CONTAINER(quality_hbox1), 10);
        gtk_container_add(GTK_CONTAINER(quality_vbox), quality_hbox1);

        quality_label = gtk_label_new(_("Quality level (0 - 10):"));
        gtk_misc_set_alignment(GTK_MISC(quality_label), 0, 0.5);
        gtk_box_pack_start(GTK_BOX(quality_hbox1), quality_label, true, true, 0);

        quality_adj = (GtkAdjustment *) gtk_adjustment_new (5, 0, 10, 0.1, 1, 0);
        quality_spin = gtk_spin_button_new(GTK_ADJUSTMENT(quality_adj), 1, 2);
        gtk_box_pack_start(GTK_BOX(quality_hbox1), quality_spin, true, true, 0);
        g_signal_connect(G_OBJECT(quality_adj), "value-changed", G_CALLBACK(quality_change), nullptr);

        gtk_spin_button_set_value(GTK_SPIN_BUTTON(quality_spin), (v_base_quality * 10));
    }

    gtk_widget_show_all(configure_win);
}

static int vorbis_format_required (int fmt)
{
    return FMT_FLOAT;
}

FileWriterImpl vorbis_plugin = {
    vorbis_init,
    vorbis_configure,
    vorbis_open,
    vorbis_write,
    vorbis_close,
    vorbis_format_required,
};

#endif
