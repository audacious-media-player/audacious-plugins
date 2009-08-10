/*  BMP - Cross-platform multimedia player
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
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

#include "OSS4.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include <sys/ioctl.h>

static GtkWidget *configure_win = NULL;
static GtkWidget *mixer_save_check,*buffer_size_spin, *buffer_pre_spin;
static GtkWidget *adevice_use_alt_check, *audio_alt_device_entry;
static gint audio_device;

static void
configure_win_ok_cb(GtkWidget * w, gpointer data)
{
    mcs_handle_t *db;

    oss_cfg.audio_device = audio_device;
    oss_cfg.buffer_size =
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(buffer_size_spin));
    oss_cfg.prebuffer =
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(buffer_pre_spin));
    oss_cfg.save_volume =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
                                     (mixer_save_check));

    oss_cfg.use_alt_audio_device =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
                                     (adevice_use_alt_check));
    g_free(oss_cfg.alt_audio_device);
    oss_cfg.alt_audio_device =
        gtk_editable_get_chars(GTK_EDITABLE(audio_alt_device_entry), 0, -1);
    g_strstrip(oss_cfg.alt_audio_device);
    
    if (oss_cfg.use_alt_audio_device)
        /* do a minimum of sanity checking */
        if (oss_cfg.alt_audio_device[0] != '/')
            oss_cfg.use_alt_audio_device = FALSE;
    
    db = aud_cfg_db_open();

    aud_cfg_db_set_int(db, "OSS", "audio_device", oss_cfg.audio_device);
    aud_cfg_db_set_int(db, "OSS", "buffer_size", oss_cfg.buffer_size);
    aud_cfg_db_set_int(db, "OSS", "prebuffer", oss_cfg.prebuffer);
    aud_cfg_db_set_bool(db, "OSS", "save_volume", oss_cfg.save_volume);
    aud_cfg_db_set_bool(db, "OSS", "use_alt_audio_device",
                        oss_cfg.use_alt_audio_device);
    aud_cfg_db_set_string(db, "OSS", "alt_audio_device",
                          oss_cfg.alt_audio_device);
    aud_cfg_db_close(db);
}

static void
configure_win_audio_dev_cb(GtkWidget * widget, gpointer data)
{
    audio_device = GPOINTER_TO_INT(data);
}

static void
audio_device_toggled(GtkToggleButton * widget, gpointer data)
{
    gboolean use_alt_audio_device = gtk_toggle_button_get_active(widget);
    gtk_widget_set_sensitive(GTK_WIDGET(data), !use_alt_audio_device);
    gtk_widget_set_sensitive(audio_alt_device_entry, use_alt_audio_device);
}

static void
scan_devices(gchar * type, GtkWidget * option_menu, GtkSignalFunc sigfunc)
{
    GtkWidget *menu, *item;
    oss_sysinfo sysinfo;
    gint mixerfd = -1;
    gint i, acc;
    
    menu = gtk_menu_new();
    
    /*
     * Open the mixer device used for calling SNDCTL_SYSINFO and
     * SNDCTL_AUDIOINFO.
     */    
    if ((mixerfd = open(DEFAULT_MIXER, O_RDWR, 0)) == -1)
    {
        perror(DEFAULT_MIXER);
        oss_describe_error();
    }
    else
    {
        if (ioctl(mixerfd, SNDCTL_SYSINFO, &sysinfo) == -1)
        {
            perror("SNDCTL_SYSINFO");
            oss_describe_error();
            close(mixerfd);
        }
        else
        {        
            for (i = 0; i < sysinfo.numaudios; i++)
            {
                oss_audioinfo ainfo;
                ainfo.dev = i;
            
                if (ioctl(mixerfd, SNDCTL_AUDIOINFO, &ainfo) == -1)
                {
                    perror("SNDCTL_AUDIOINFO");
                    oss_describe_error();
                    close(mixerfd);
                }
                else
                {
                    acc = ainfo.caps & (PCM_CAP_INPUT | PCM_CAP_OUTPUT);
                
                    switch (acc)
                    {
                        case PCM_CAP_OUTPUT:
                            item = gtk_menu_item_new_with_label(g_strdup_printf("%d. %s (OUTPUT)", i, ainfo.name));
                            break;

                        case PCM_CAP_INPUT | PCM_CAP_OUTPUT:
                            item = gtk_menu_item_new_with_label(g_strdup_printf("%d. %s (INPUT/OUTPUT)", i, ainfo.name));
                            break;
                        
                        case PCM_CAP_INPUT:
                            item = gtk_menu_item_new_with_label(g_strdup_printf("%d. %s (INPUT)", i, ainfo.name));
                            break;
                            
                        default:
                            continue;
                    }

                    gtk_widget_show(item);
                    gtk_menu_append(GTK_MENU(menu), item);
                
                    g_signal_connect(G_OBJECT(item), "activate",
                                     G_CALLBACK(sigfunc), GINT_TO_POINTER(i));
                }
            }

            close(mixerfd);
        }
    }

    gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
}

void
oss_configure(void)
{
    GtkWidget *vbox, *notebook;
    GtkWidget *dev_vbox, *adevice_frame, *adevice_box, *adevice;
    /* GtkWidget *mdevice_frame, *mdevice_box, *mdevice; */
    GtkWidget *buffer_frame, *buffer_vbox, *buffer_table;
    GtkWidget *buffer_size_box, *buffer_size_label;
    GtkObject *buffer_size_adj, *buffer_pre_adj;
    GtkWidget *buffer_pre_box, *buffer_pre_label;
    GtkWidget *audio_alt_box;
    GtkWidget *bbox, *ok, *cancel;
    GtkWidget *mixer_table, *mixer_frame;

    if (configure_win) {
        gtk_window_present(GTK_WINDOW(configure_win));
        return;
    }

    configure_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(configure_win), "destroy",
                     G_CALLBACK(gtk_widget_destroyed), &configure_win);
    gtk_window_set_title(GTK_WINDOW(configure_win),
                         _("OSS Driver configuration"));
    gtk_window_set_type_hint(GTK_WINDOW(configure_win),
                             GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_resizable(GTK_WINDOW(configure_win), FALSE);
    gtk_window_set_position(GTK_WINDOW(configure_win), GTK_WIN_POS_CENTER);
    gtk_container_border_width(GTK_CONTAINER(configure_win), 10);

    vbox = gtk_vbox_new(FALSE, 10);
    gtk_container_add(GTK_CONTAINER(configure_win), vbox);

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

    dev_vbox = gtk_vbox_new(FALSE, 5);
    gtk_container_set_border_width(GTK_CONTAINER(dev_vbox), 5);

    adevice_frame = gtk_frame_new(_("Audio device:"));
    gtk_box_pack_start(GTK_BOX(dev_vbox), adevice_frame, FALSE, FALSE, 0);

    adevice_box = gtk_vbox_new(FALSE, 5);
    gtk_container_set_border_width(GTK_CONTAINER(adevice_box), 5);
    gtk_container_add(GTK_CONTAINER(adevice_frame), adevice_box);

    adevice = gtk_option_menu_new();
    gtk_box_pack_start(GTK_BOX(adevice_box), adevice, TRUE, TRUE, 0);
#if defined(HAVE_NEWPCM)
    scan_devices("Installed devices:", adevice,
                 GTK_SIGNAL_FUNC(configure_win_audio_dev_cb));
#else
    scan_devices("Audio devices:", adevice,
                 GTK_SIGNAL_FUNC(configure_win_audio_dev_cb));
#endif
    audio_device = oss_cfg.audio_device;
    gtk_option_menu_set_history(GTK_OPTION_MENU(adevice),
                                oss_cfg.audio_device);
    audio_alt_box = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start_defaults(GTK_BOX(adevice_box), audio_alt_box);
    adevice_use_alt_check =
        gtk_check_button_new_with_label(_("Use alternate device:"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(adevice_use_alt_check),
                                 oss_cfg.use_alt_audio_device);
    g_signal_connect(G_OBJECT(adevice_use_alt_check), "toggled",
                     G_CALLBACK(audio_device_toggled), adevice);
    gtk_box_pack_start(GTK_BOX(audio_alt_box), adevice_use_alt_check,
                       FALSE, FALSE, 0);
    audio_alt_device_entry = gtk_entry_new();
    if (oss_cfg.alt_audio_device != NULL)
        gtk_entry_set_text(GTK_ENTRY(audio_alt_device_entry),
                           oss_cfg.alt_audio_device);
    else
        gtk_entry_set_text(GTK_ENTRY(audio_alt_device_entry), DEV_DSP);
    gtk_box_pack_start_defaults(GTK_BOX(audio_alt_box),
                                audio_alt_device_entry);

    if (oss_cfg.use_alt_audio_device)
        gtk_widget_set_sensitive(adevice, FALSE);
    else
        gtk_widget_set_sensitive(audio_alt_device_entry, FALSE);
   
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), dev_vbox,
                             gtk_label_new(_("Devices")));

    buffer_frame = gtk_frame_new(_("Buffering:"));
    gtk_container_set_border_width(GTK_CONTAINER(buffer_frame), 5);

    buffer_vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(buffer_frame), buffer_vbox);

    buffer_table = gtk_table_new(2, 1, TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(buffer_table), 5);
    gtk_box_pack_start(GTK_BOX(buffer_vbox), buffer_table, FALSE, FALSE, 0);

    buffer_size_box = gtk_hbox_new(FALSE, 5);
    gtk_table_attach_defaults(GTK_TABLE(buffer_table), buffer_size_box, 0,
                              1, 0, 1);
    buffer_size_label = gtk_label_new(_("Buffer size (ms):"));
    gtk_box_pack_start(GTK_BOX(buffer_size_box), buffer_size_label, FALSE,
                       FALSE, 0);
    buffer_size_adj =
        gtk_adjustment_new(oss_cfg.buffer_size, 200, 10000, 100, 100, 100);
    buffer_size_spin =
        gtk_spin_button_new(GTK_ADJUSTMENT(buffer_size_adj), 8, 0);
    gtk_widget_set_usize(buffer_size_spin, 60, -1);
    gtk_box_pack_start(GTK_BOX(buffer_size_box), buffer_size_spin, FALSE,
                       FALSE, 0);

    buffer_pre_box = gtk_hbox_new(FALSE, 5);
    gtk_table_attach_defaults(GTK_TABLE(buffer_table), buffer_pre_box, 1,
                              2, 0, 1);
    buffer_pre_label = gtk_label_new(_("Pre-buffer (percent):"));
    gtk_box_pack_start(GTK_BOX(buffer_pre_box), buffer_pre_label, FALSE,
                       FALSE, 0);
    buffer_pre_adj = gtk_adjustment_new(oss_cfg.prebuffer, 0, 90, 1, 1, 1);
    buffer_pre_spin =
        gtk_spin_button_new(GTK_ADJUSTMENT(buffer_pre_adj), 1, 0);
    gtk_widget_set_usize(buffer_pre_spin, 60, -1);
    gtk_box_pack_start(GTK_BOX(buffer_pre_box), buffer_pre_spin, FALSE,
                       FALSE, 0);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), buffer_frame,
                             gtk_label_new(_("Buffering")));
    
    mixer_frame = gtk_frame_new(_("Mixer Settings:"));
    gtk_container_set_border_width(GTK_CONTAINER(mixer_frame), 5);
    mixer_table = gtk_table_new(1, 1, TRUE);
    gtk_container_add(GTK_CONTAINER(mixer_frame), mixer_table);
    gtk_container_set_border_width(GTK_CONTAINER(mixer_table), 5);
    mixer_save_check =
        gtk_check_button_new_with_label(_("Save VMIX volume between sessions"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mixer_save_check),
                                 oss_cfg.save_volume);
    gtk_table_attach_defaults(GTK_TABLE(mixer_table),
                              mixer_save_check, 0, 1, 0, 1);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), mixer_frame,
                             gtk_label_new(_("Mixer")));

    bbox = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

    cancel = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
    g_signal_connect_swapped(G_OBJECT(cancel), "clicked",
                             G_CALLBACK(gtk_widget_destroy),
                             GTK_OBJECT(configure_win));
    GTK_WIDGET_SET_FLAGS(cancel, GTK_CAN_DEFAULT);
    gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);

    ok = gtk_button_new_from_stock(GTK_STOCK_APPLY);
    g_signal_connect(G_OBJECT(ok), "clicked",
                     G_CALLBACK(configure_win_ok_cb), NULL);
    GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT);
    gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 0);
    gtk_widget_grab_default(ok);

    gtk_widget_show_all(configure_win);
}
