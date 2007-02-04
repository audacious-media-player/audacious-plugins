
#include "mpg123.h"

#include <glib.h>
#include <audacious/i18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <audacious/configdb.h>
#include <audacious/titlestring.h>


static GtkWidget *mpgdec_configurewin = NULL;
static GtkWidget *vbox, *notebook;
static GtkWidget *decode_vbox, *decode_hbox1;
static GtkWidget *decode_res_frame, *decode_res_vbox, *decode_res_16,
    *decode_res_8;
static GtkWidget *decode_ch_frame, *decode_ch_vbox, *decode_ch_stereo,
    *decode_ch_mono;
static GtkWidget *decode_freq_frame, *decode_freq_vbox, *decode_freq_1to1,
    *decode_freq_1to2, *decode_freq_1to4;

static GtkWidget *title_id3_box, *title_tag_desc;
static GtkWidget *title_override, *title_id3_entry, *title_id3v2_disable;

MPG123Config mpgdec_cfg;

static void
mpgdec_configurewin_ok(GtkWidget * widget, gpointer data)
{
    ConfigDb *db;

    if (GTK_TOGGLE_BUTTON(decode_res_16)->active)
        mpgdec_cfg.resolution = 16;
    else if (GTK_TOGGLE_BUTTON(decode_res_8)->active)
        mpgdec_cfg.resolution = 8;

    if (GTK_TOGGLE_BUTTON(decode_ch_stereo)->active)
        mpgdec_cfg.channels = 2;
    else if (GTK_TOGGLE_BUTTON(decode_ch_mono)->active)
        mpgdec_cfg.channels = 1;

    if (GTK_TOGGLE_BUTTON(decode_freq_1to1)->active)
        mpgdec_cfg.downsample = 0;
    else if (GTK_TOGGLE_BUTTON(decode_freq_1to2)->active)
        mpgdec_cfg.downsample = 1;
    if (GTK_TOGGLE_BUTTON(decode_freq_1to4)->active)
        mpgdec_cfg.downsample = 2;

    mpgdec_cfg.title_override =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(title_override));
    mpgdec_cfg.disable_id3v2 =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(title_id3v2_disable));
    g_free(mpgdec_cfg.id3_format);
    mpgdec_cfg.id3_format =
        g_strdup(gtk_entry_get_text(GTK_ENTRY(title_id3_entry)));

    db = bmp_cfg_db_open();
    bmp_cfg_db_set_int(db, "MPG123", "resolution", mpgdec_cfg.resolution);
    bmp_cfg_db_set_int(db, "MPG123", "channels", mpgdec_cfg.channels);
    bmp_cfg_db_set_int(db, "MPG123", "downsample", mpgdec_cfg.downsample);
    bmp_cfg_db_set_int(db, "MPG123", "http_buffer_size",
                       mpgdec_cfg.http_buffer_size);
    bmp_cfg_db_set_int(db, "MPG123", "http_prebuffer",
                       mpgdec_cfg.http_prebuffer);
    bmp_cfg_db_set_bool(db, "MPG123", "save_http_stream",
                        mpgdec_cfg.save_http_stream);
    bmp_cfg_db_set_string(db, "MPG123", "save_http_path",
                          mpgdec_cfg.save_http_path);
    bmp_cfg_db_set_bool(db, "MPG123", "use_udp_channel",
                        mpgdec_cfg.use_udp_channel);
    bmp_cfg_db_set_bool(db, "MPG123", "title_override",
                        mpgdec_cfg.title_override);
    bmp_cfg_db_set_bool(db, "MPG123", "disable_id3v2",
                        mpgdec_cfg.disable_id3v2);
    bmp_cfg_db_set_string(db, "MPG123", "id3_format", mpgdec_cfg.id3_format);

    bmp_cfg_db_close(db);
    gtk_widget_destroy(mpgdec_configurewin);
}

static void
title_override_cb(GtkWidget * w, gpointer data)
{
    gboolean override;
    override =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(title_override));
    gtk_widget_set_sensitive(title_id3_box, override);
    gtk_widget_set_sensitive(title_tag_desc, override);
}

static void
configure_destroy(GtkWidget * w, gpointer data)
{
	// nothing at the moment?
}

void
mpgdec_configure(void)
{
    GtkWidget *title_frame, *title_id3_vbox, *title_id3_label;
    GtkWidget *bbox, *ok, *cancel;

    if (mpgdec_configurewin != NULL) {
        gtk_window_present(GTK_WINDOW(mpgdec_configurewin));
        return;
    }
    mpgdec_configurewin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_type_hint(GTK_WINDOW(mpgdec_configurewin),
                             GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_position(GTK_WINDOW(mpgdec_configurewin),
                            GTK_WIN_POS_CENTER);
    g_signal_connect(G_OBJECT(mpgdec_configurewin), "destroy",
                     G_CALLBACK(gtk_widget_destroyed), &mpgdec_configurewin);
    g_signal_connect(G_OBJECT(mpgdec_configurewin), "destroy",
                     G_CALLBACK(configure_destroy), &mpgdec_configurewin);
    gtk_window_set_title(GTK_WINDOW(mpgdec_configurewin),
                         _("MPEG Audio Plugin Configuration"));
    gtk_window_set_resizable(GTK_WINDOW(mpgdec_configurewin), FALSE);
    /*  gtk_window_set_position(GTK_WINDOW(mpgdec_configurewin), GTK_WIN_POS_MOUSE); */
    gtk_container_border_width(GTK_CONTAINER(mpgdec_configurewin), 10);

    vbox = gtk_vbox_new(FALSE, 10);
    gtk_container_add(GTK_CONTAINER(mpgdec_configurewin), vbox);

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

    decode_vbox = gtk_vbox_new(FALSE, 5);
    gtk_container_set_border_width(GTK_CONTAINER(decode_vbox), 5);

    decode_hbox1 = gtk_hbox_new(TRUE, 5);
    gtk_box_pack_start(GTK_BOX(decode_vbox), decode_hbox1, FALSE, FALSE, 0);

    decode_res_frame = gtk_frame_new(_("Resolution:"));
    gtk_box_pack_start(GTK_BOX(decode_hbox1), decode_res_frame, TRUE, TRUE,
                       0);

    decode_res_vbox = gtk_vbox_new(FALSE, 5);
    gtk_container_set_border_width(GTK_CONTAINER(decode_res_vbox), 5);
    gtk_container_add(GTK_CONTAINER(decode_res_frame), decode_res_vbox);

    decode_res_16 = gtk_radio_button_new_with_label(NULL, _("16 bit"));
    if (mpgdec_cfg.resolution == 16)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_res_16), TRUE);
    gtk_box_pack_start(GTK_BOX(decode_res_vbox), decode_res_16, FALSE,
                       FALSE, 0);

    decode_res_8 =
        gtk_radio_button_new_with_label(gtk_radio_button_group
                                        (GTK_RADIO_BUTTON(decode_res_16)),
                                        _("8 bit"));
    if (mpgdec_cfg.resolution == 8)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_res_8), TRUE);

    gtk_box_pack_start(GTK_BOX(decode_res_vbox), decode_res_8, FALSE,
                       FALSE, 0);

    decode_ch_frame = gtk_frame_new(_("Channels:"));
    gtk_box_pack_start(GTK_BOX(decode_hbox1), decode_ch_frame, TRUE, TRUE, 0);

    decode_ch_vbox = gtk_vbox_new(FALSE, 5);
    gtk_container_set_border_width(GTK_CONTAINER(decode_ch_vbox), 5);
    gtk_container_add(GTK_CONTAINER(decode_ch_frame), decode_ch_vbox);

    decode_ch_stereo =
        gtk_radio_button_new_with_label(NULL, _("Stereo (if available)"));
    if (mpgdec_cfg.channels == 2)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_ch_stereo),
                                     TRUE);

    gtk_box_pack_start(GTK_BOX(decode_ch_vbox), decode_ch_stereo, FALSE,
                       FALSE, 0);

    decode_ch_mono =
        gtk_radio_button_new_with_label(gtk_radio_button_group
                                        (GTK_RADIO_BUTTON
                                         (decode_ch_stereo)), _("Mono"));
    if (mpgdec_cfg.channels == 1)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_ch_mono), TRUE);

    gtk_box_pack_start(GTK_BOX(decode_ch_vbox), decode_ch_mono, FALSE,
                       FALSE, 0);

    decode_freq_frame = gtk_frame_new(_("Down sample:"));
    gtk_box_pack_start(GTK_BOX(decode_vbox), decode_freq_frame, FALSE,
                       FALSE, 0);

    decode_freq_vbox = gtk_vbox_new(FALSE, 5);
    gtk_container_set_border_width(GTK_CONTAINER(decode_freq_vbox), 5);
    gtk_container_add(GTK_CONTAINER(decode_freq_frame), decode_freq_vbox);

    decode_freq_1to1 =
        gtk_radio_button_new_with_label(NULL, _("1:1 (44 kHz)"));
    if (mpgdec_cfg.downsample == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_freq_1to1),
                                     TRUE);
    gtk_box_pack_start(GTK_BOX(decode_freq_vbox), decode_freq_1to1, FALSE,
                       FALSE, 0);

    decode_freq_1to2 =
        gtk_radio_button_new_with_label(gtk_radio_button_group
                                        (GTK_RADIO_BUTTON
                                         (decode_freq_1to1)),
                                        _("1:2 (22 kHz)"));
    if (mpgdec_cfg.downsample == 1)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_freq_1to2),
                                     TRUE);
    gtk_box_pack_start(GTK_BOX(decode_freq_vbox), decode_freq_1to2, FALSE,
                       FALSE, 0);

    decode_freq_1to4 =
        gtk_radio_button_new_with_label(gtk_radio_button_group
                                        (GTK_RADIO_BUTTON
                                         (decode_freq_1to1)),
                                        _("1:4 (11 kHz)"));
    if (mpgdec_cfg.downsample == 2)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_freq_1to4),
                                     TRUE);

    gtk_box_pack_start(GTK_BOX(decode_freq_vbox), decode_freq_1to4, FALSE,
                       FALSE, 0);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), decode_vbox,
                             gtk_label_new(_("Decoder")));

    title_frame = gtk_frame_new(_("ID3 Tags:"));
    gtk_container_border_width(GTK_CONTAINER(title_frame), 5);

    title_id3_vbox = gtk_vbox_new(FALSE, 10);
    gtk_container_border_width(GTK_CONTAINER(title_id3_vbox), 5);
    gtk_container_add(GTK_CONTAINER(title_frame), title_id3_vbox);

    title_id3v2_disable =
        gtk_check_button_new_with_label(_("Disable ID3V2 tags"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(title_id3v2_disable),
                                 mpgdec_cfg.disable_id3v2);
    gtk_box_pack_start(GTK_BOX(title_id3_vbox), title_id3v2_disable, FALSE,
                       FALSE, 0);

    title_override =
        gtk_check_button_new_with_label(_("Override generic titles"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(title_override),
                                 mpgdec_cfg.title_override);
    g_signal_connect(G_OBJECT(title_override), "clicked",
                     G_CALLBACK(title_override_cb), NULL);
    gtk_box_pack_start(GTK_BOX(title_id3_vbox), title_override, FALSE,
                       FALSE, 0);

    title_id3_box = gtk_hbox_new(FALSE, 5);
    gtk_widget_set_sensitive(title_id3_box, mpgdec_cfg.title_override);
    gtk_box_pack_start(GTK_BOX(title_id3_vbox), title_id3_box, FALSE,
                       FALSE, 0);

    title_id3_label = gtk_label_new(_("ID3 format:"));
    gtk_box_pack_start(GTK_BOX(title_id3_box), title_id3_label, FALSE,
                       FALSE, 0);

    title_id3_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(title_id3_entry), mpgdec_cfg.id3_format);
    gtk_box_pack_start(GTK_BOX(title_id3_box), title_id3_entry, TRUE, TRUE,
                       0);

    title_tag_desc = xmms_titlestring_descriptions("pafFetnygc", 2);
    gtk_widget_set_sensitive(title_tag_desc, mpgdec_cfg.title_override);
    gtk_box_pack_start(GTK_BOX(title_id3_vbox), title_tag_desc, FALSE,
                       FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), title_frame,
                             gtk_label_new(_("Title")));

    bbox = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

    cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    g_signal_connect_swapped(G_OBJECT(cancel), "clicked",
                             G_CALLBACK(gtk_widget_destroy),
                             GTK_OBJECT(mpgdec_configurewin));
    GTK_WIDGET_SET_FLAGS(cancel, GTK_CAN_DEFAULT);
    gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);


    ok = gtk_button_new_from_stock(GTK_STOCK_OK);
    g_signal_connect(G_OBJECT(ok), "clicked",
                     G_CALLBACK(mpgdec_configurewin_ok), NULL);
    GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT);
    gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 0);
    gtk_widget_grab_default(ok);

    gtk_widget_show_all(mpgdec_configurewin);
}
