/*
 * Audacious ALSA Plugin (-ng)
 * Copyright (c) 2009 William Pitcock <nenolod@dereferenced.org>
 * Portions copyright (C) 2001-2003 Matthieu Sozeau <mattam@altern.org>
 * Portions copyright (C) 2003-2005 Haavard Kvaalen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "alsa-stdinc.h"
#include <stdio.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

static GtkWidget *configure_win = NULL;
static GtkWidget *devices_combo, *mixer_devices_combo;

gint alsaplug_mixer_new_for_card(snd_mixer_t **mixer, const gchar *card);

#define GET_TOGGLE(tb) gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(tb))
#define GET_CHARS(edit) gtk_editable_get_chars(GTK_EDITABLE(edit), 0, -1)

static void configure_win_ok_cb(GtkWidget * w, gpointer data)
{
	g_free(alsaplug_cfg.pcm_device);
	alsaplug_cfg.pcm_device = GET_CHARS(GTK_COMBO(devices_combo)->entry);
	alsaplug_cfg.mixer_device = GET_CHARS(GTK_COMBO(mixer_devices_combo)->entry);

	gtk_widget_destroy(configure_win);

	/* Save configuration */
	mcs_handle_t *cfgfile = aud_cfg_db_open();
	aud_cfg_db_set_string(cfgfile, "alsaplug", "pcm_device", alsaplug_cfg.pcm_device);
	aud_cfg_db_set_string(cfgfile, "alsaplug", "mixer_device", alsaplug_cfg.mixer_device);
	aud_cfg_db_close(cfgfile);
}

void alsaplug_get_config(void)
{
	mcs_handle_t *cfgfile = aud_cfg_db_open();
	aud_cfg_db_get_string(cfgfile, "alsaplug", "pcm_device", &alsaplug_cfg.pcm_device);
	aud_cfg_db_get_string(cfgfile, "alsaplug", "mixer_device", &alsaplug_cfg.mixer_device);
	aud_cfg_db_close(cfgfile);
}

static gint get_mixer_devices(GtkCombo *combo, const gchar *card)
{
	GList *items = NULL;
	gint err;
	snd_mixer_t *mixer;
	snd_mixer_elem_t *current;

	if ((err = alsaplug_mixer_new_for_card(&mixer, card)) < 0)
		return err;

	current = snd_mixer_first_elem(mixer);

	while (current)
	{
		if (snd_mixer_selem_is_active(current) &&
		    snd_mixer_selem_has_playback_volume(current))
		{
			const gchar *sname = snd_mixer_selem_get_name(current);
			gint index = snd_mixer_selem_get_index(current);
			if (index)
				items = g_list_append(items, g_strdup_printf("%s,%d", sname, index));
			else
				items = g_list_append(items, g_strdup(sname));
		}
		current = snd_mixer_elem_next(current);
	}

	gtk_combo_set_popdown_strings(combo, items);

	return 0;
}

static void get_devices_for_card(GtkCombo *combo, gint card)
{
	GtkWidget *item;
	gint pcm_device = -1, err;
	snd_pcm_info_t *pcm_info = NULL;
	snd_ctl_t *ctl;
	gchar dev[64], *card_name;

	sprintf(dev, "hw:%i", card);

	if ((err = snd_ctl_open(&ctl, dev, 0)) < 0)
	{
		printf("snd_ctl_open() failed: %s", snd_strerror(err));
		return;
	}

	if ((err = snd_card_get_name(card, &card_name)) != 0)
	{
		g_warning("snd_card_get_name() failed: %s", snd_strerror(err));
		card_name = _("Unknown soundcard");
	}

	snd_pcm_info_alloca(&pcm_info);

	for (;;)
	{
		char *device, *descr;
		if ((err = snd_ctl_pcm_next_device(ctl, &pcm_device)) < 0)
		{
			g_warning("snd_ctl_pcm_next_device() failed: %s",
				  snd_strerror(err));
			pcm_device = -1;
		}
		if (pcm_device < 0)
			break;

		snd_pcm_info_set_device(pcm_info, pcm_device);
		snd_pcm_info_set_subdevice(pcm_info, 0);
		snd_pcm_info_set_stream(pcm_info, SND_PCM_STREAM_PLAYBACK);

		if ((err = snd_ctl_pcm_info(ctl, pcm_info)) < 0)
		{
			if (err != -ENOENT)
				g_warning("get_devices_for_card(): "
					  "snd_ctl_pcm_info() "
					  "failed (%d:%d): %s.", card,
					  pcm_device, snd_strerror(err));
			continue;
		}

		device = g_strdup_printf("hw:%d,%d", card, pcm_device);
		descr = g_strconcat(card_name, ": ",
				    snd_pcm_info_get_name(pcm_info),
				    " (", device, ")", NULL);
		item = gtk_list_item_new_with_label(descr);
		gtk_widget_show(item);
		g_free(descr);
		gtk_combo_set_item_string(combo, GTK_ITEM(item), device);
		g_free(device);
		gtk_container_add(GTK_CONTAINER(combo->list), item);
	}

	snd_ctl_close(ctl);
}

static void get_devices(GtkCombo *combo)
{
	GtkWidget *item;
	gint card = -1;
	gint err = 0;
	gchar *descr;
	gpointer *hints = NULL;

	descr = g_strdup_printf(_("Default PCM device (%s)"), "default");
	item = gtk_list_item_new_with_label(descr);
	gtk_widget_show(item);
	g_free(descr);
	gtk_combo_set_item_string(combo, GTK_ITEM(item), "default");
	gtk_container_add(GTK_CONTAINER(combo->list), item);

	if ((err = snd_device_name_hint(-1, "pcm", &hints)) == 0)
	{
		for (card = 0; hints[card] != NULL; card++)
		{
			gchar *name = snd_device_name_get_hint(hints[card], "NAME");
			gchar *desc = snd_device_name_get_hint(hints[card], "DESC");
			gchar **descnames = g_strsplit(desc, "\n", 0);

			gchar *buf = g_strconcat(descnames[0],
					descnames[1] != NULL ? " - " : "",
					descnames[1] != NULL ? descnames[1] : "",
					" (", name, ")", NULL);

			item = gtk_list_item_new_with_label(buf);
			gtk_widget_show(item);

			gtk_combo_set_item_string(combo, GTK_ITEM(item), name);
			gtk_container_add(GTK_CONTAINER(combo->list), item);
		
			g_strfreev(descnames);
			g_free(buf);
		}

		snd_device_name_free_hint(hints);
	}

	if ((err = snd_card_next(&card)) != 0)
	{
		g_warning("snd_next_card() failed: %s", snd_strerror(err));
		return;
	}

	while (card > -1)
	{
		get_devices_for_card(combo, card);
		if ((err = snd_card_next(&card)) != 0)
		{
			g_warning("snd_next_card() failed: %s",
				  snd_strerror(err));
			break;
		}
	}
}

void alsaplug_configure(void)
{
        GtkWidget * vbox, * adevice_frame, * adevice_box;
	GtkWidget *mixer_frame;
	GtkWidget *bbox, *ok, *cancel;

	if (configure_win != NULL)
	{
                gtk_window_present(GTK_WINDOW(configure_win));
		return;
	}

        configure_win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title ((GtkWindow *) configure_win, _("ALSA Output Plugin Preferences"));
        gtk_window_set_type_hint ((GtkWindow *) configure_win, GDK_WINDOW_TYPE_HINT_DIALOG);
        gtk_window_set_resizable ((GtkWindow *) configure_win, FALSE);

        gtk_container_set_border_width ((GtkContainer *) configure_win, 6);
        g_signal_connect ((GObject *) configure_win, "destroy", G_CALLBACK(gtk_widget_destroyed), &configure_win);

        vbox = gtk_vbox_new (FALSE, 6);
        gtk_container_add ((GtkContainer *) configure_win, vbox);

        adevice_frame = gtk_frame_new (_("Device:"));
        gtk_box_pack_start ((GtkBox *) vbox, adevice_frame, FALSE, FALSE, 0);

        adevice_box = gtk_vbox_new (FALSE, 6);
        gtk_container_set_border_width ((GtkContainer *) adevice_box, 6);
        gtk_container_add ((GtkContainer *) adevice_frame, adevice_box);

	devices_combo = gtk_combo_new();
	gtk_box_pack_start(GTK_BOX(adevice_box), devices_combo,
			   FALSE, FALSE, 0);
	get_devices(GTK_COMBO(devices_combo));
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(devices_combo)->entry),
			   alsaplug_cfg.pcm_device);

        mixer_frame = gtk_frame_new (_("Mixer:"));
        gtk_box_pack_start ((GtkBox *) vbox, mixer_frame, FALSE, FALSE, 0);

	mixer_devices_combo = gtk_combo_new();
	get_mixer_devices(GTK_COMBO(mixer_devices_combo), alsaplug_cfg.pcm_device);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(mixer_devices_combo)->entry),
			   alsaplug_cfg.mixer_device);

	gtk_container_add(GTK_CONTAINER(mixer_frame), mixer_devices_combo);

	bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 5);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

	ok = gtk_button_new_with_label(_("OK"));
	gtk_signal_connect(GTK_OBJECT(ok), "clicked", (GCallback)configure_win_ok_cb, NULL);
	GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 0);
	gtk_widget_grab_default(ok);

	cancel = gtk_button_new_with_label(_("Cancel"));
	gtk_signal_connect_object(GTK_OBJECT(cancel), "clicked",
				  (GCallback)gtk_widget_destroy, GTK_OBJECT(configure_win));
	GTK_WIDGET_SET_FLAGS(cancel, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);

	gtk_widget_show_all(configure_win);
}
