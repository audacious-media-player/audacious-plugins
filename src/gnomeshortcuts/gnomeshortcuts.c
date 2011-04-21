/* -*- Mode: C; indent-tabs: t; c-basic-offset: 9; tab-width: 9 -*- */
/*
 *  This file is part of audacious-gnome-shortcut plugin for audacious
 *
 *  Copyright (c) 2007-2008    Sascha Hlusiak <contact@saschahlusiak.de>
 *  Name: plugin.c
 *  Description: plugin.c
 *
 *  audacious-gnome-shortcut is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  audacious-gnome-shortcut is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with audacious-gnome-shortcut; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include <string.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-bindings.h>
#include <glib-object.h>

#include <audacious/drct.h>
#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

static gboolean init (void);
static void about (void);
static void cleanup (void);
void gnome_remote_init();
void gnome_remote_uninit();

static gboolean loaded = FALSE;
static DBusGProxy *media_player_keys_proxy = NULL;

AUD_GENERAL_PLUGIN
(
	.name = "Gnome Shortcuts",
	.init = init,
	.about = about,
	.cleanup = cleanup
)

#define g_marshal_value_peek_string(v)   (char*) g_value_get_string (v)


static void
hotkey_marshal_VOID__STRING_STRING (GClosure     *closure,
                                   GValue       *return_value,
                                   guint         n_param_values,
                                   const GValue *param_values,
                                   gpointer      invocation_hint,
                                   gpointer      marshal_data)
{
	typedef void (*GMarshalFunc_VOID__STRING_STRING) (gpointer     data1,
							gpointer     arg_1,
							gpointer     arg_2);
	register GMarshalFunc_VOID__STRING_STRING callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1;

	g_return_if_fail (n_param_values == 3);

	if (G_CCLOSURE_SWAP_DATA (closure))
	{
		data1 = closure->data;
	} else {
		data1 = g_value_peek_pointer (param_values + 0);
	}
	callback = (GMarshalFunc_VOID__STRING_STRING) (marshal_data ? marshal_data : cc->callback);

	callback (data1,
		g_marshal_value_peek_string (param_values + 1),
		g_marshal_value_peek_string (param_values + 2));
}

static void
on_media_player_key_pressed (DBusGProxy *proxy, const gchar *application, const gchar *key)
{
	if (strcmp ("Audacious", application) == 0) {
		gint current_volume /* , old_volume */ ;
		static gint volume_static = 0;
		gboolean play, mute;

		/* playing or not */
		play = aud_drct_get_playing ();

		/* get current volume */
		aud_drct_get_volume_main (&current_volume);
		/* old_volume = current_volume; */
		if (current_volume)
		{
			/* volume is not mute */
			mute = FALSE;
		} else {
			/* volume is mute */
			mute = TRUE;
		}

		/* mute the playback */
		if (strcmp ("Mute", key) == 0)
		{
			if (!mute)
			{
				volume_static = current_volume;
				aud_drct_set_volume_main (0);
				mute = TRUE;
			} else {
				aud_drct_set_volume_main (volume_static);
				mute = FALSE;
			}
			return;
		}

		/* decreace volume */
/*		if ((keycode == plugin_cfg.vol_down) && (state == plugin_cfg.vol_down_mask))
		{
			if (mute)
			{
				current_volume = old_volume;
				old_volume = 0;
				mute = FALSE;
			}

			if ((current_volume -= plugin_cfg.vol_decrement) < 0)
			{
				current_volume = 0;
			}

			if (current_volume != old_volume)
			{
				xmms_remote_set_main_volume (audacioushotkey.xmms_session,
							current_volume);
			}

			old_volume = current_volume;
			return TRUE;
		}*/

		/* increase volume */
/*		if ((keycode == plugin_cfg.vol_up) && (state == plugin_cfg.vol_up_mask))
		{
			if (mute)
			{
				current_volume = old_volume;
				old_volume = 0;
				mute = FALSE;
			}

			if ((current_volume += plugin_cfg.vol_increment) > 100)
			{
				current_volume = 100;
			}

			if (current_volume != old_volume)
			{
				xmms_remote_set_main_volume (audacioushotkey.xmms_session,
							current_volume);
			}

			old_volume = current_volume;
			return TRUE;
		}*/

		/* play */
		if (strcmp ("Play", key) == 0)
		{
			if (!play)
			{
				aud_drct_play ();
			} else {
				aud_drct_pause ();
			}
			return;
		}

		/* pause */
		if (strcmp ("Pause", key) == 0)
		{
			if (!play) aud_drct_play ();
			else aud_drct_pause ();

			return;
		}

		/* stop */
		if (strcmp ("Stop", key) == 0)
		{
			aud_drct_stop ();
			return;
		}

		/* prev track */
		if (strcmp ("Previous", key) == 0)
		{
			aud_drct_pl_prev ();
			return;
		}

		/* next track */
		if (strcmp ("Next", key) == 0)
		{
			aud_drct_pl_next ();
			return;
		}
	}
}

void gnome_remote_uninit ()
{
	GError *error = NULL;
	if (media_player_keys_proxy == NULL) return;

	dbus_g_proxy_disconnect_signal (media_player_keys_proxy, "MediaPlayerKeyPressed",
			G_CALLBACK (on_media_player_key_pressed), NULL);

	dbus_g_proxy_call (media_player_keys_proxy,
				"ReleaseMediaPlayerKeys", &error,
				G_TYPE_STRING, "Audacious",
				G_TYPE_INVALID, G_TYPE_INVALID);
	if (error != NULL) {
		g_warning ("Could not release media player keys: %s", error->message);
		g_error_free (error);
	}
	g_object_unref(media_player_keys_proxy);
	media_player_keys_proxy = NULL;
}

void gnome_remote_init ()
{
	DBusGConnection *bus;
	GError *error = NULL;
	dbus_g_thread_init();

	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if ((bus == NULL) || error) {
		g_warning ("Error connecting to DBus: %s", error->message);
	} else {
		media_player_keys_proxy = dbus_g_proxy_new_for_name (bus,
			"org.gnome.SettingsDaemon",
			"/org/gnome/SettingsDaemon/MediaKeys",
			"org.gnome.SettingsDaemon.MediaKeys");
		if (media_player_keys_proxy == NULL) return;

		dbus_g_proxy_call (media_player_keys_proxy,
				"GrabMediaPlayerKeys", &error,
				G_TYPE_STRING, "Audacious",
				G_TYPE_UINT, 0,
				G_TYPE_INVALID,
				G_TYPE_INVALID);
		if (error != NULL) {
			g_error_free (error);
			error = NULL;
			g_object_unref(media_player_keys_proxy);
			media_player_keys_proxy = NULL;
    			media_player_keys_proxy = dbus_g_proxy_new_for_name (bus,
				"org.gnome.SettingsDaemon",
				"/org/gnome/SettingsDaemon",
				"org.gnome.SettingsDaemon");
			if (media_player_keys_proxy == NULL) return;

			dbus_g_proxy_call (media_player_keys_proxy,
					"GrabMediaPlayerKeys", &error,
					G_TYPE_STRING, "Audacious",
					G_TYPE_UINT, 0,
					G_TYPE_INVALID,
					G_TYPE_INVALID);
			if (error != NULL) {
				g_warning ("Could not grab media player keys: %s", error->message);
				g_error_free (error);
				g_object_unref(media_player_keys_proxy);
				media_player_keys_proxy = NULL;
				return;
			}
		}

		dbus_g_object_register_marshaller (hotkey_marshal_VOID__STRING_STRING,
				G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);

		dbus_g_proxy_add_signal (media_player_keys_proxy, "MediaPlayerKeyPressed",
				G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);

		dbus_g_proxy_connect_signal (media_player_keys_proxy, "MediaPlayerKeyPressed",
				G_CALLBACK (on_media_player_key_pressed), NULL, NULL);
	}
}

static void about (void)
{
    static GtkWidget * dialog = NULL;

    audgui_simple_message (& dialog, GTK_MESSAGE_INFO,
     _("About Gnome Shortcut Plugin"),
     _("Gnome Shortcut Plugin\n"
     "Let's you control the player with Gnome's shortcuts.\n\n"
     "Copyright (C) 2007-2008 Sascha Hlusiak <contact@saschahlusiak.de>\n\n"));
}

static gboolean init (void)
{
	gnome_remote_init();
	loaded = TRUE;
	return TRUE;
}

static void cleanup (void)
{
	if (!loaded) return;
	gnome_remote_uninit();
	loaded = FALSE;
}
