/*
 *  This file is part of audacious-hotkey plugin for audacious
 *
 *  Copyright (c) 2007 - 2008  Sascha Hlusiak <contact@saschahlusiak.de>
 *  Name: plugin.c
 *  Description: plugin.c
 *
 *  Part of this code is from itouch-ctrl plugin.
 *  Authors of itouch-ctrl are listed below:
 *
 *  Copyright (c) 2006 - 2007 Vladimir Paskov <vlado.paskov@gmail.com>
 *
 *  Part of this code are from xmms-itouch plugin.
 *  Authors of xmms-itouch are listed below:
 *
 *  Copyright (C) 2000-2002 Ville Syrjälä <syrjala@sci.fi>
 *                         Bryn Davies <curious@ihug.com.au>
 *                         Jonathan A. Davis <davis@jdhouse.org>
 *                         Jeremy Tan <nsx@nsx.homeip.net>
 *
 *  audacious-hotkey is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  audacious-hotkey is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with audacious-hotkey; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include <stdio.h>
#include <X11/XF86keysym.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <audacious/configdb.h>
#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <libaudcore/hook.h>

#include "plugin.h"
#include "gui.h"
#include "grab.h"


/* func defs */
static void init (void);
static void cleanup (void);


/* global vars */
static PluginConfig plugin_cfg;
static gboolean loaded = FALSE;



static GeneralPlugin audacioushotkey =
{
	.description = "Global Hotkey",
	.init = init,
	.about = show_about,
	.configure = show_configure,
	.cleanup = cleanup
};

GeneralPlugin *hotkey_gplist[] = { &audacioushotkey, NULL };
SIMPLE_GENERAL_PLUGIN(hotkey, hotkey_gplist);



PluginConfig* get_config(void)
{
	return &plugin_cfg;
}


/*
 * plugin activated
 */
static void init (void)
{
	setup_filter();
	load_config ( );
	grab_keys ( );

	loaded = TRUE;
}

/* handle keys */
gboolean handle_keyevent (EVENT event)
{
	gint current_volume, old_volume;
	static gint volume_static = 0;
	gboolean play, mute;

	/* playing or not */
	play = aud_drct_get_playing ();

	/* get current volume */
	aud_drct_get_volume_main (&current_volume);
	old_volume = current_volume;
	if (current_volume)
	{
		/* volume is not mute */
		mute = FALSE;
	} else {
		/* volume is mute */
		mute = TRUE;
	}

	/* mute the playback */
	if (event == EVENT_MUTE)
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
		return TRUE;
	}

	/* decreace volume */
	if (event == EVENT_VOL_DOWN)
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
			aud_drct_set_volume_main (current_volume);
		}

		old_volume = current_volume;
		return TRUE;
	}

	/* increase volume */
	if (event == EVENT_VOL_UP)
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
			aud_drct_set_volume_main (current_volume);
		}

		old_volume = current_volume;
		return TRUE;
	}

	/* play */
	if (event == EVENT_PLAY)
	{
		aud_drct_play ();
		return TRUE;
	}

	/* pause */
	if (event == EVENT_PAUSE)
	{
		if (!play) aud_drct_play ();
		else aud_drct_pause ();

		return TRUE;
	}

	/* stop */
	if (event == EVENT_STOP)
	{
		aud_drct_stop ();
		return TRUE;
	}

	/* prev track */
	if (event == EVENT_PREV_TRACK)
	{
		aud_drct_pl_prev ();
		return TRUE;
	}

	/* next track */
	if (event == EVENT_NEXT_TRACK)
	{
		aud_drct_pl_next ();
		return TRUE;
	}

	/* forward */
	if (event == EVENT_FORWARD)
	{
		aud_drct_seek (aud_drct_get_time () + 5000);
		return TRUE;
	}

	/* backward */
	if (event == EVENT_BACKWARD)
	{
		gint time = aud_drct_get_time ();
		if (time > 5000) time -= 5000; /* Jump 5s back */
			else time = 0;
		aud_drct_seek (time);
		return TRUE;
	}

	/* Open Jump-To-File dialog */
	if (event == EVENT_JUMP_TO_FILE)
	{
		hook_call ("interface show jump to track", NULL);
		return TRUE;
	}

	/* Toggle Windows */
	if (event == EVENT_TOGGLE_WIN)
	{
		hook_call ("interface toggle visibility", NULL);
		return TRUE;
	}

	/* Show OSD through AOSD plugin*/
	if (event == EVENT_SHOW_AOSD)
	{
		hook_call("aosd toggle", NULL);
		return TRUE;
	}

	return FALSE;
}

void add_hotkey(HotkeyConfiguration** pphotkey, KeySym keysym, gint mask, gint type, EVENT event)
{
	KeyCode keycode;
	HotkeyConfiguration *photkey;
	if (keysym == 0) return;
	if (pphotkey == NULL) return;
	photkey = *pphotkey;
	if (photkey == NULL) return;
	keycode = XKeysymToKeycode(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), keysym);
	if (keycode == 0) return;
	if (photkey->key) {
		photkey->next = (HotkeyConfiguration*)
			malloc(sizeof (HotkeyConfiguration));
		photkey = photkey->next;
		*pphotkey = photkey;
		photkey->next = NULL;
	}
	photkey->key = (gint)keycode;
	photkey->mask = mask;
	photkey->event = event;
	photkey->type = type;
}

void load_defaults (void)
{
	HotkeyConfiguration* hotkey;

	hotkey = &(plugin_cfg.first);

	add_hotkey(&hotkey, XF86XK_AudioPrev, 0, TYPE_KEY, EVENT_PREV_TRACK);
	add_hotkey(&hotkey, XF86XK_AudioPlay, 0, TYPE_KEY, EVENT_PLAY);
	add_hotkey(&hotkey, XF86XK_AudioPause, 0, TYPE_KEY, EVENT_PAUSE);
	add_hotkey(&hotkey, XF86XK_AudioStop, 0, TYPE_KEY, EVENT_STOP);
	add_hotkey(&hotkey, XF86XK_AudioNext, 0, TYPE_KEY, EVENT_NEXT_TRACK);

/*	add_hotkey(&hotkey, XF86XK_AudioRewind, 0, TYPE_KEY, EVENT_BACKWARD); */

	add_hotkey(&hotkey, XF86XK_AudioMute, 0, TYPE_KEY, EVENT_MUTE);
	add_hotkey(&hotkey, XF86XK_AudioRaiseVolume, 0, TYPE_KEY, EVENT_VOL_UP);
	add_hotkey(&hotkey, XF86XK_AudioLowerVolume, 0, TYPE_KEY, EVENT_VOL_DOWN);

/*	add_hotkey(&hotkey, XF86XK_AudioMedia, 0, TYPE_KEY, EVENT_JUMP_TO_FILE);
	add_hotkey(&hotkey, XF86XK_Music, 0, TYPE_KEY, EVENT_TOGGLE_WIN); */
}

/* load plugin configuration */
void load_config (void)
{
	mcs_handle_t *cfdb;
	HotkeyConfiguration *hotkey;
	int i,max;

	/* default volume level */
	plugin_cfg.vol_increment = 4;
	plugin_cfg.vol_decrement = 4;

	/* open configuration database */
	cfdb = aud_cfg_db_open ( );
	hotkey = &(plugin_cfg.first);
	hotkey->next = NULL;
	hotkey->key = 0;
	hotkey->mask = 0;
	hotkey->event = 0;
	hotkey->type = TYPE_KEY;
	max = 0;
	aud_cfg_db_get_int (cfdb, "globalHotkey", "NumHotkeys", &max);
	if (max == 0)
		load_defaults();
	else for (i=0; i<max; i++)
	{
		gchar *text = NULL;
		gint value;
		if (hotkey->key) {
			hotkey->next = (HotkeyConfiguration*)
				malloc(sizeof (HotkeyConfiguration));
			hotkey = hotkey->next;
			hotkey->next = NULL;
			hotkey->key = 0;
			hotkey->mask = 0;
			hotkey->event = 0;
			hotkey->type = TYPE_KEY;
		}
		text = g_strdup_printf("Hotkey_%d_key", i);
		aud_cfg_db_get_int (cfdb, "globalHotkey", text, &(hotkey->key));
		g_free(text);

		text = g_strdup_printf("Hotkey_%d_mask", i);
		aud_cfg_db_get_int (cfdb, "globalHotkey", text, &(hotkey->mask));
		g_free(text);

		text = g_strdup_printf("Hotkey_%d_type", i);
		aud_cfg_db_get_int (cfdb, "globalHotkey", text, &(hotkey->type));
		g_free(text);

		text = g_strdup_printf("Hotkey_%d_event", i);
		value = (gint)hotkey->event;
		aud_cfg_db_get_int (cfdb, "globalHotkey", text, &value);
		hotkey->event = (EVENT) value;
		g_free(text);
	}

	aud_cfg_db_close (cfdb);
}

/* save plugin configuration */
void save_config (void)
{
	mcs_handle_t *cfdb;
	int max;
	HotkeyConfiguration *hotkey;

	/* open configuration database */
	cfdb = aud_cfg_db_open ( );
	hotkey = &(plugin_cfg.first);
	max = 0;
	while (hotkey) {
		gchar *text = NULL;
		if (hotkey->key) {
			text = g_strdup_printf("Hotkey_%d_key", max);
			aud_cfg_db_set_int (cfdb, "globalHotkey", text, hotkey->key);
			g_free(text);

			text = g_strdup_printf("Hotkey_%d_mask", max);
			aud_cfg_db_set_int (cfdb, "globalHotkey", text, hotkey->mask);
			g_free(text);

			text = g_strdup_printf("Hotkey_%d_type", max);
			aud_cfg_db_set_int (cfdb, "globalHotkey", text, hotkey->type);
			g_free(text);

			text = g_strdup_printf("Hotkey_%d_event", max);
			aud_cfg_db_set_int (cfdb, "globalHotkey", text, hotkey->event);
			g_free(text);
			max++;
		}

		hotkey = hotkey->next;
	}
	aud_cfg_db_set_int (cfdb, "globalHotkey", "NumHotkeys", max);

	aud_cfg_db_close (cfdb);
}

static void cleanup (void)
{
	HotkeyConfiguration* hotkey;
	if (!loaded) return;
	ungrab_keys ();
	release_filter();
	hotkey = &(plugin_cfg.first);
	hotkey = hotkey->next;
	while (hotkey)
	{
		HotkeyConfiguration * old;
		old = hotkey;
		hotkey = hotkey->next;
		free(old);
	}
	plugin_cfg.first.next = NULL;
	plugin_cfg.first.key = 0;
	plugin_cfg.first.event = 0;
	plugin_cfg.first.mask = 0;
	loaded = FALSE;
}

gboolean is_loaded (void)
{
	return loaded;
}
