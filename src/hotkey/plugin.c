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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <stdio.h>
#include <X11/XF86keysym.h>

#include <gdk/gdkx.h>
#include <audacious/plugin.h>
#include <audacious/auddrct.h>
#include <audacious/configdb.h>

#include <audacious/i18n.h>

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
gboolean handle_keyevent (int keycode, int state, int type)
{
	gint current_volume, old_volume;
	static gint volume_static = 0;
	gboolean play, mute;
	
	/* playing or not */
	play = audacious_drct_is_playing ();
	
	/* get current volume */
	audacious_drct_get_volume_main (&current_volume);
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
	if ((keycode == plugin_cfg.mute.key) && (state == plugin_cfg.mute.mask) && (type == plugin_cfg.mute.type))
	{
		if (!mute)
		{
			volume_static = current_volume;
			audacious_drct_set_main_volume (0);
			mute = TRUE;
		} else {
			audacious_drct_set_main_volume (volume_static);
			mute = FALSE;
		}
		return TRUE;
	}
	
	/* decreace volume */
	if ((keycode == plugin_cfg.vol_down.key) && (state == plugin_cfg.vol_down.mask) && (type == plugin_cfg.vol_down.type))
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
			audacious_drct_set_main_volume (current_volume);
		}
			
		old_volume = current_volume;
		return TRUE;
	}
	
	/* increase volume */
	if ((keycode == plugin_cfg.vol_up.key) && (state == plugin_cfg.vol_up.mask) && (type == plugin_cfg.vol_up.type))
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
			audacious_drct_set_main_volume (current_volume);
		}
			
		old_volume = current_volume;
		return TRUE;
	}
	
	/* play */
	if ((keycode == plugin_cfg.play.key) && (state == plugin_cfg.play.mask) && (type == plugin_cfg.play.type))
	{
		audacious_drct_play ();
		return TRUE;
	}

	/* pause */
	if ((keycode == plugin_cfg.pause.key) && (state == plugin_cfg.pause.mask) && (type == plugin_cfg.pause.type))
	{
		if (!play) audacious_drct_play ();
		else audacious_drct_pause ();

		return TRUE;
	}
	
	/* stop */
	if ((keycode == plugin_cfg.stop.key) && (state == plugin_cfg.stop.mask) && (type == plugin_cfg.stop.type))
	{
		audacious_drct_stop ();
		return TRUE;
	}
	
	/* prev track */	
	if ((keycode == plugin_cfg.prev_track.key) && (state == plugin_cfg.prev_track.mask) && (type == plugin_cfg.prev_track.type))
	{
		audacious_drct_playlist_prev ();
		return TRUE;
	}
	
	/* next track */
	if ((keycode == plugin_cfg.next_track.key) && (state == plugin_cfg.next_track.mask) && (type == plugin_cfg.next_track.type))
	{
		audacious_drct_playlist_next ();
		return TRUE;
	}

	/* forward */
	if ((keycode == plugin_cfg.forward.key) && (state == plugin_cfg.forward.mask) && (type == plugin_cfg.forward.type))
	{
		gint time = audacious_drct_get_output_time();
		time += 5000; /* Jump 5s into future */
		audacious_drct_jump_to_time(time);
		return TRUE;
	}

	/* backward */
	if ((keycode == plugin_cfg.backward.key) && (state == plugin_cfg.backward.mask) && (type == plugin_cfg.backward.type))
	{
		gint time = audacious_drct_get_output_time();
		if (time > 5000) time -= 5000; /* Jump 5s back */
			else time = 0;
		audacious_drct_jump_to_time(time);
		return TRUE;
	}

	/* Open Jump-To-File dialog */
	if ((keycode == plugin_cfg.jump_to_file.key) && (state == plugin_cfg.jump_to_file.mask) && (type == plugin_cfg.jump_to_file.type))
	{
		audacious_drct_show_jtf_box();
		return TRUE;
	}

	/* Toggle Windows */
	if ((keycode == plugin_cfg.toggle_win.key) && (state == plugin_cfg.toggle_win.mask) && (type == plugin_cfg.toggle_win.type))
	{
		static gboolean is_main, is_eq, is_pl;
		is_main = audacious_drct_main_win_is_visible();
		if (is_main) { /* Hide windows */
			is_pl = audacious_drct_pl_win_is_visible();
			is_eq = audacious_drct_eq_win_is_visible();
			audacious_drct_main_win_toggle(FALSE);
			audacious_drct_pl_win_toggle(FALSE);
			audacious_drct_eq_win_toggle(FALSE);
		} else { /* Show hidden windows */
			audacious_drct_main_win_toggle(TRUE);
			audacious_drct_pl_win_toggle(is_pl);
			audacious_drct_eq_win_toggle(is_eq);
			audacious_drct_activate();
		}
		return TRUE;
	}

	/* Show OSD through AOSD plugin*/
	if ((keycode == plugin_cfg.show_aosd.key) && (state == plugin_cfg.show_aosd.mask) && (type == plugin_cfg.show_aosd.type))
	{
		aud_hook_call("aosd toggle", NULL);
		return TRUE;
	}

	return FALSE;
}

/* load plugin configuration */
void load_config (void)
{
	ConfigDb *cfdb;
	
	/* default volume level */
	plugin_cfg.vol_increment = 4;
	plugin_cfg.vol_decrement = 4;

#define load_key(hotkey,default) \
	plugin_cfg.hotkey.key = (default)?(XKeysymToKeycode(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), (default))):0; \
	plugin_cfg.hotkey.mask = 0; \
	plugin_cfg.hotkey.type = TYPE_KEY; \
	aud_cfg_db_get_int (cfdb, "globalHotkey", #hotkey, &plugin_cfg.hotkey.key); \
	aud_cfg_db_get_int (cfdb, "globalHotkey", #hotkey "_mask", &plugin_cfg.hotkey.mask); \
	aud_cfg_db_get_int (cfdb, "globalHotkey", #hotkey "_type", &plugin_cfg.hotkey.type);


	/* open configuration database */
	cfdb = aud_cfg_db_open ( );

	load_key(mute, XF86XK_AudioMute);
	load_key(vol_down, XF86XK_AudioLowerVolume);
	load_key(vol_up, XF86XK_AudioRaiseVolume);
	load_key(play, XF86XK_AudioPlay);
	load_key(pause, XF86XK_AudioPause);
	load_key(stop, XF86XK_AudioStop);
	load_key(prev_track, XF86XK_AudioPrev);
	load_key(next_track, XF86XK_AudioNext);
	load_key(jump_to_file, XF86XK_AudioMedia);
	load_key(toggle_win, 0);
	load_key(forward, 0);
	load_key(backward, XF86XK_AudioRewind);
	load_key(show_aosd, 0);

	aud_cfg_db_close (cfdb);
}

/* save plugin configuration */
void save_config (void)
{
	ConfigDb *cfdb;

#define save_key(hotkey) \
	aud_cfg_db_set_int (cfdb, "globalHotkey", #hotkey, plugin_cfg.hotkey.key); \
	aud_cfg_db_set_int (cfdb, "globalHotkey", #hotkey "_mask", plugin_cfg.hotkey.mask); \
	aud_cfg_db_set_int (cfdb, "globalHotkey", #hotkey "_type", plugin_cfg.hotkey.type);
	
	/* open configuration database */
	cfdb = aud_cfg_db_open ( );
	
	save_key(mute);
	save_key(vol_up);
	save_key(vol_down);
	save_key(play);
	save_key(pause);
	save_key(stop);
	save_key(prev_track);
	save_key(next_track);
	save_key(jump_to_file);
	save_key(forward);
	save_key(backward);
	save_key(toggle_win);
	save_key(show_aosd);

	aud_cfg_db_close (cfdb);
}

static void cleanup (void)
{
	if (!loaded) return;
	ungrab_keys ();
	release_filter();
	loaded = FALSE;
}

gboolean is_loaded (void)
{
	return loaded;
}
