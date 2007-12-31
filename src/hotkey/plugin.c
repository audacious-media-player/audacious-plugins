/* -*- Mode: C; indent-tabs: t; c-basic-offset: 9; tab-width: 9 -*- */
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
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <audacious/plugin.h>
#include <audacious/auddrct.h>
#include <audacious/configdb.h>

#include <audacious/i18n.h>

/* for audacious_info_dialog () */
#include <audacious/util.h>


/* func defs */
void x_display_init (void);
static void get_offending_modifiers (Display * dpy);
static void init (void);
static void grab_keys ();
static void ungrab_keys ();
static gboolean handle_keyevent(int keycode, int state);
static gboolean setup_filter();
static void release_filter();

static void load_config (void);
static void save_config (void);
static void configure (void);
static void clear_keyboard (GtkWidget *widget, gpointer data);

void cancel_callback (GtkWidget *widget, gpointer data);
void ok_callback (GtkWidget *widget, gpointer data);
static void about (void);
static void cleanup (void);

#define TYPE_KEY 0
#define TYPE_MOUSE 1


typedef struct {
	gint key, mask;
	gint type;
} HotkeyConfiguration;

typedef struct {
	gint vol_increment;
	gint vol_decrement;
	
	/* keyboard */
	HotkeyConfiguration mute;
	HotkeyConfiguration vol_down;
	HotkeyConfiguration vol_up;
	HotkeyConfiguration play;
	HotkeyConfiguration stop;
	HotkeyConfiguration pause;
	HotkeyConfiguration prev_track;
	HotkeyConfiguration next_track;
	HotkeyConfiguration jump_to_file;
	HotkeyConfiguration toggle_win;
	HotkeyConfiguration forward;
	HotkeyConfiguration backward;
} PluginConfig;

PluginConfig plugin_cfg;

static Display *xdisplay = NULL;
static Window x_root_window = 0;
static gint grabbed = 0;
static gboolean loaded = FALSE;
static unsigned int numlock_mask = 0;
static unsigned int scrolllock_mask = 0;
static unsigned int capslock_mask = 0;



typedef struct {
	GtkWidget *keytext;
	HotkeyConfiguration hotkey;
} KeyControls;

typedef struct {
	KeyControls play;
	KeyControls stop;
	KeyControls pause;
	KeyControls prev_track;
	KeyControls next_track;
	KeyControls vol_up;
	KeyControls vol_down;
	KeyControls mute;
	KeyControls jump_to_file;
	KeyControls forward;
	KeyControls backward;
	KeyControls toggle_win;
} ConfigurationControls;

static GeneralPlugin audacioushotkey =
{
	.description = "Global Hotkey",
	.init = init,
	.about = about,
	.configure = configure,
	.cleanup = cleanup
};

GeneralPlugin *hotkey_gplist[] = { &audacioushotkey, NULL };
SIMPLE_GENERAL_PLUGIN(hotkey, hotkey_gplist);



/* 
 * plugin activated
 */
static void init (void)
{
	x_display_init ( );
	setup_filter();
	load_config ( );
	grab_keys ();

	loaded = TRUE;
}

/* check X display */
void x_display_init (void)
{
	if (xdisplay != NULL) return;
	xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
	x_root_window = GDK_WINDOW_XID(gdk_get_default_root_window());
	get_offending_modifiers(xdisplay);
}

/* Taken from xbindkeys */
static void get_offending_modifiers (Display * dpy)
{
	int i;
	XModifierKeymap *modmap;
	KeyCode nlock, slock;
	static int mask_table[8] = {
		ShiftMask, LockMask, ControlMask, Mod1Mask,
		Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask
	};
	
	nlock = XKeysymToKeycode (dpy, XK_Num_Lock);
	slock = XKeysymToKeycode (dpy, XK_Scroll_Lock);
	
	/*
	* Find out the masks for the NumLock and ScrollLock modifiers,
	* so that we can bind the grabs for when they are enabled too.
	*/
	modmap = XGetModifierMapping (dpy);
	
	if (modmap != NULL && modmap->max_keypermod > 0)
	{
		for (i = 0; i < 8 * modmap->max_keypermod; i++)
		{
			if (modmap->modifiermap[i] == nlock && nlock != 0)
				numlock_mask = mask_table[i / modmap->max_keypermod];
			else if (modmap->modifiermap[i] == slock && slock != 0)
				scrolllock_mask = mask_table[i / modmap->max_keypermod];
		}
	}
	
	capslock_mask = LockMask;
	
	if (modmap)
		XFreeModifiermap (modmap);
}

/* handle keys */
static gboolean handle_keyevent (int keycode, int state)
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

	state &= ~(scrolllock_mask | numlock_mask | capslock_mask);
	
	/* mute the playback */
	if ((keycode == plugin_cfg.mute.key) && (state == plugin_cfg.mute.mask))
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
	if ((keycode == plugin_cfg.vol_down.key) && (state == plugin_cfg.vol_down.mask))
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
	if ((keycode == plugin_cfg.vol_up.key) && (state == plugin_cfg.vol_up.mask))
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
	if ((keycode == plugin_cfg.play.key) && (state == plugin_cfg.play.mask))
	{
		if (!play)
		{
			audacious_drct_play ();
		} else {
			audacious_drct_pause ();
		}
		return TRUE;
	}

	/* pause */
	if ((keycode == plugin_cfg.pause.key) && (state == plugin_cfg.pause.mask))
	{
		if (!play) audacious_drct_play ();
		else audacious_drct_pause ();

		return TRUE;
	}
	
	/* stop */
	if ((keycode == plugin_cfg.stop.key) && (state == plugin_cfg.stop.mask))
	{
		audacious_drct_stop ();
		return TRUE;
	}
	
	/* prev track */	
	if ((keycode == plugin_cfg.prev_track.key) && (state == plugin_cfg.prev_track.mask))
	{
		audacious_drct_playlist_prev ();
		return TRUE;
	}
	
	/* next track */
	if ((keycode == plugin_cfg.next_track.key) && (state == plugin_cfg.next_track.mask))
	{
		audacious_drct_playlist_next ();
		return TRUE;
	}

	/* forward */
	if ((keycode == plugin_cfg.forward.key) && (state == plugin_cfg.forward.mask))
	{
		gint time = audacious_drct_get_output_time();
		time += 5000; /* Jump 5s into future */
		audacious_drct_jump_to_time(time);
		return TRUE;
	}

	/* backward */
	if ((keycode == plugin_cfg.backward.key) && (state == plugin_cfg.backward.mask))
	{
		gint time = audacious_drct_get_output_time();
		if (time > 5000) time -= 5000; /* Jump 5s back */
			else time = 0;
		audacious_drct_jump_to_time(time);
		return TRUE;
	}

	/* Open Jump-To-File dialog */
	if ((keycode == plugin_cfg.jump_to_file.key) && (state == plugin_cfg.jump_to_file.mask))
	{
		audacious_drct_show_jtf_box();
		return TRUE;
	}

	/* Toggle Windows */
	if ((keycode == plugin_cfg.toggle_win.key) && (state == plugin_cfg.toggle_win.mask))
	{
		static gboolean is_main, is_eq, is_pl;
		is_main = audacious_drct_main_win_is_visible();
		if (is_main) {
			is_pl = audacious_drct_pl_win_is_visible();
			is_eq = audacious_drct_eq_win_is_visible();
			audacious_drct_main_win_toggle(FALSE);
			audacious_drct_pl_win_toggle(FALSE);
			audacious_drct_eq_win_toggle(FALSE);
		} else {
			audacious_drct_main_win_toggle(TRUE);
			audacious_drct_pl_win_toggle(is_pl);
			audacious_drct_eq_win_toggle(is_eq);
		}
		return TRUE;
	}

	return FALSE;
}

static GdkFilterReturn
gdk_filter(GdkXEvent *xevent,
	   GdkEvent *event,
	   gpointer data)
{
	XKeyEvent *keyevent = (XKeyEvent*)xevent;
	
	if (((XEvent*)keyevent)->type != KeyPress)
		return -1;
	
	if (handle_keyevent(keyevent->keycode, keyevent->state))
		return GDK_FILTER_REMOVE;
	
	return GDK_FILTER_CONTINUE;
}

static gboolean
setup_filter()
{
	gdk_window_add_filter(gdk_get_default_root_window(),
				gdk_filter,
				NULL);

	return TRUE;
}

static void release_filter()
{
	gdk_window_remove_filter(gdk_get_default_root_window(),
				gdk_filter,
				NULL);
}

/* load plugin configuration */
static void load_config (void)
{
	ConfigDb *cfdb;
	
	if (xdisplay == NULL) x_display_init();

	/* default volume level */
	plugin_cfg.vol_increment = 4;
	plugin_cfg.vol_decrement = 4;

#define load_key(hotkey,default) \
	plugin_cfg.hotkey.key = (default)?(XKeysymToKeycode(xdisplay, (default))):0; \
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

	aud_cfg_db_close (cfdb);
}

/* save plugin configuration */
static void save_config (void)
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

	aud_cfg_db_close (cfdb);
}

static int x11_error_handler (Display *dpy, XErrorEvent *error)
{
	return 0;
}

/* grab requied keys */
static void grab_key(HotkeyConfiguration hotkey)
{
	unsigned int modifier = hotkey.mask & ~(numlock_mask | capslock_mask | scrolllock_mask);
	
	if (hotkey.key == 0) return;

	XGrabKey (xdisplay, hotkey.key, modifier, x_root_window,
		False, GrabModeAsync, GrabModeAsync);
	
	if (modifier == AnyModifier)
		return;
	
	if (numlock_mask)
		XGrabKey (xdisplay, hotkey.key, modifier | numlock_mask,
			x_root_window,
			False, GrabModeAsync, GrabModeAsync);
	
	if (capslock_mask)
		XGrabKey (xdisplay, hotkey.key, modifier | capslock_mask,
			x_root_window,
			False, GrabModeAsync, GrabModeAsync);
	
	if (scrolllock_mask)
		XGrabKey (xdisplay, hotkey.key, modifier | scrolllock_mask,
			x_root_window,
			False, GrabModeAsync, GrabModeAsync);
	
	if (numlock_mask && capslock_mask)
		XGrabKey (xdisplay, hotkey.key, modifier | numlock_mask | capslock_mask,
			x_root_window,
			False, GrabModeAsync, GrabModeAsync);
	
	if (numlock_mask && scrolllock_mask)
		XGrabKey (xdisplay, hotkey.key, modifier | numlock_mask | scrolllock_mask,
			x_root_window,
			False, GrabModeAsync, GrabModeAsync);
	
	if (capslock_mask && scrolllock_mask)
		XGrabKey (xdisplay, hotkey.key, modifier | capslock_mask | scrolllock_mask,
			x_root_window,
			False, GrabModeAsync, GrabModeAsync);
	
	if (numlock_mask && capslock_mask && scrolllock_mask)
		XGrabKey (xdisplay, hotkey.key,
			modifier | numlock_mask | capslock_mask | scrolllock_mask,
			x_root_window, False, GrabModeAsync,
			GrabModeAsync);
}


static void grab_keys ()
{
	if (grabbed) return;
	if (xdisplay == NULL) x_display_init();

	XErrorHandler old_handler = 0;

	XSync(xdisplay, False);
	old_handler = XSetErrorHandler (x11_error_handler);

	grab_key(plugin_cfg.mute);
	grab_key(plugin_cfg.vol_up);
	grab_key(plugin_cfg.vol_down);
	grab_key(plugin_cfg.play);
	grab_key(plugin_cfg.pause);
	grab_key(plugin_cfg.stop);
	grab_key(plugin_cfg.prev_track);
	grab_key(plugin_cfg.next_track);
	grab_key(plugin_cfg.jump_to_file);
	grab_key(plugin_cfg.forward);
	grab_key(plugin_cfg.backward);
	grab_key(plugin_cfg.toggle_win);
	
	XSync(xdisplay, False);
	XSetErrorHandler (old_handler);

	grabbed = 1;
}
/*
 * plugin init end
 */

static void set_keytext (GtkWidget *entry, gint key, gint mask, gint type)
{
	gchar *text = NULL;

	if (key == 0 && mask == 0)
	{
		text = g_strdup(_("(none)"));
	} else {
		static char *modifier_string[] = { "Control", "Shift", "Alt", "Mod2", "Mod3", "Super", "Mod5" };
		static unsigned int modifiers[] = { ControlMask, ShiftMask, Mod1Mask, Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask };
		gchar *strings[9];
		gchar *keytext = NULL;
		int i, j;
		KeySym keysym;

		keysym = XKeycodeToKeysym(xdisplay, key, 0);
		if (keysym == 0 || keysym == NoSymbol)
		{
			keytext = g_strdup_printf("#%3d", key);
		} else {
			keytext = g_strdup(XKeysymToString(keysym));
		}

		for (i = 0, j=0; j<7; j++)
		{
			if (mask & modifiers[j])
 				strings[i++] = modifier_string[j];
		}
		if (key != 0) strings[i++] = keytext;
		strings[i] = NULL;

		text = g_strjoinv(" + ", strings);
		g_free(keytext);
	}

	gtk_entry_set_text(GTK_ENTRY(entry), text);
	gtk_editable_set_position(GTK_EDITABLE(entry), -1);
	if (text) g_free(text);
}

static gboolean
on_entry_key_press_event(GtkWidget * widget,
                         GdkEventKey * event,
                         gpointer user_data)
{
	KeyControls *controls = (KeyControls*) user_data;
	int is_mod;
	int mod;

	if (event->keyval == GDK_Tab) return FALSE;
	mod = 0;
	is_mod = 0;

	if ((event->state & GDK_CONTROL_MASK) | (!is_mod && (is_mod = (event->keyval == GDK_Control_L || event->keyval == GDK_Control_R))))
        	mod |= ControlMask;

	if ((event->state & GDK_MOD1_MASK) | (!is_mod && (is_mod = (event->keyval == GDK_Alt_L || event->keyval == GDK_Alt_R))))
        	mod |= Mod1Mask;

	if ((event->state & GDK_SHIFT_MASK) | (!is_mod && (is_mod = (event->keyval == GDK_Shift_L || event->keyval == GDK_Shift_R))))
        	mod |= ShiftMask;

	if ((event->state & GDK_MOD5_MASK) | (!is_mod && (is_mod = (event->keyval == GDK_ISO_Level3_Shift))))
        	mod |= Mod5Mask;

	if ((event->state & GDK_MOD4_MASK) | (!is_mod && (is_mod = (event->keyval == GDK_Super_L || event->keyval == GDK_Super_R))))
        	mod |= Mod4Mask;

	if (!is_mod) {
		controls->hotkey.key = event->hardware_keycode;
		controls->hotkey.mask = mod;
		controls->hotkey.type = TYPE_KEY;
	} else controls->hotkey.key = 0;

	set_keytext(controls->keytext, is_mod ? 0 : event->hardware_keycode, mod, TYPE_KEY);
	return FALSE;
}

static gboolean
on_entry_key_release_event(GtkWidget * widget,
                           GdkEventKey * event,
                           gpointer user_data)
{
	KeyControls *controls = (KeyControls*) user_data;
	if (controls->hotkey.key == 0) {
		controls->hotkey.mask = 0;
	}
	set_keytext(controls->keytext, controls->hotkey.key, controls->hotkey.mask, controls->hotkey.type);
	return FALSE;
}


static void add_event_controls(GtkWidget *table, 
				KeyControls *controls, 
				int row, 
				char* descr, 
				HotkeyConfiguration hotkey)
{
	GtkWidget *label;
	GtkWidget *button;

	controls->hotkey.key = hotkey.key;
	controls->hotkey.mask = hotkey.mask;

	label = gtk_label_new (_(descr));
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1, 
			(GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), 3, 3);
	
	controls->keytext = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), controls->keytext, 1, 2, row, row+1, 
			(GtkAttachOptions) (GTK_FILL|GTK_EXPAND), (GtkAttachOptions) (GTK_EXPAND), 0, 0);
	gtk_entry_set_editable (GTK_ENTRY (controls->keytext), FALSE);

	set_keytext(controls->keytext, hotkey.key, hotkey.mask, hotkey.type);
	g_signal_connect((gpointer)controls->keytext, "key_press_event",
                         G_CALLBACK(on_entry_key_press_event), controls);
	g_signal_connect((gpointer)controls->keytext, "key_release_event",
                         G_CALLBACK(on_entry_key_release_event), controls);

	button = gtk_button_new_with_label (_("None"));
	gtk_table_attach (GTK_TABLE (table), button, 2, 3, row, row+1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
	g_signal_connect (G_OBJECT (button), "clicked",
			G_CALLBACK (clear_keyboard), controls);
}

/* configuration window */
static void configure (void)
{
	ConfigurationControls *controls;
	GtkWidget *window;
	GtkWidget *main_vbox, *vbox;
	GtkWidget *hbox;
	GtkWidget *alignment;
	GtkWidget *frame;
	GtkWidget *label;
	GtkWidget *image;
	GtkWidget *table;
	GtkWidget *button_box, *button;
	
	if (!xdisplay) x_display_init();

	load_config ( );

	ungrab_keys();
	
	controls = (ConfigurationControls*)g_malloc(sizeof(ConfigurationControls));
	if (!controls)
	{
		printf ("Faild to allocate memory for ConfigurationControls structure!\n"
			"Aborting!");
		return;
	}
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), _("Global Hotkey Plugin Configuration"));
	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (window), 5);
	
	main_vbox = gtk_vbox_new (FALSE, 4);
	gtk_container_add (GTK_CONTAINER (window), main_vbox);
	
	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_box_pack_start (GTK_BOX (main_vbox), alignment, FALSE, TRUE, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 4, 0, 0, 0);
	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (alignment), hbox);
	image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, TRUE, 0);
	label = gtk_label_new (_("Press a key combination inside a text field."));
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	
	label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (label), _("<b>Playback:</b>"));
	frame = gtk_frame_new (NULL);
	gtk_frame_set_label_widget (GTK_FRAME (frame), label);
	gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, TRUE, 0);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_container_add (GTK_CONTAINER (frame), alignment);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 3, 3, 3, 3);
	vbox = gtk_vbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (alignment), vbox);
	label = gtk_label_new (NULL);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
	gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
	gtk_label_set_markup (GTK_LABEL (label), 
			_("<i>Configure keys which controls Audacious playback.</i>"));
	table = gtk_table_new (4, 3, FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
	gtk_table_set_col_spacings (GTK_TABLE (table), 2);
	gtk_table_set_row_spacings (GTK_TABLE (table), 2);

	/* prev track */
	add_event_controls(table, &controls->prev_track, 0, _("Previous Track:"), 
			plugin_cfg.prev_track);

	add_event_controls(table, &controls->play, 1, _("Play/Pause:"), 
			plugin_cfg.play);

	add_event_controls(table, &controls->pause, 2, _("Pause:"), 
			plugin_cfg.pause);

	add_event_controls(table, &controls->stop, 3, _("Stop:"), 
			plugin_cfg.stop);

	add_event_controls(table, &controls->next_track, 4, _("Next Track:"), 
			plugin_cfg.next_track);

	add_event_controls(table, &controls->forward, 5, _("Forward 5 sec.:"), 
			plugin_cfg.forward);

	add_event_controls(table, &controls->backward, 6, _("Rewind 5 sec.:"), 
			plugin_cfg.backward);


	label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (label), _("<b>Volume Control:</b>"));
	frame = gtk_frame_new (NULL);
	gtk_frame_set_label_widget (GTK_FRAME (frame), label);
	gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, TRUE, 0);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_container_add (GTK_CONTAINER (frame), alignment);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 3, 3, 3, 3);
	vbox = gtk_vbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (alignment), vbox);
	label = gtk_label_new (NULL);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
	gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
	gtk_label_set_markup (GTK_LABEL (label), 
			_("<i>Configure keys which controls music volume.</i>"));
	table = gtk_table_new (3, 3, FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
	gtk_table_set_col_spacings (GTK_TABLE (table), 2);
	gtk_table_set_row_spacings (GTK_TABLE (table), 2);

	add_event_controls(table, &controls->mute, 0, _("Mute:"),
			plugin_cfg.mute);

	add_event_controls(table, &controls->vol_up, 1, _("Volume Up:"), 
			plugin_cfg.vol_up);

	add_event_controls(table, &controls->vol_down, 2, _("Volume Down:"), 
			plugin_cfg.vol_down);


	label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (label), _("<b>Player:</b>"));
	frame = gtk_frame_new (NULL);
	gtk_frame_set_label_widget (GTK_FRAME (frame), label);
	gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, TRUE, 0);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_container_add (GTK_CONTAINER (frame), alignment);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 3, 3, 3, 3);
	vbox = gtk_vbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (alignment), vbox);
	label = gtk_label_new (NULL);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
	gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
	gtk_label_set_markup (GTK_LABEL (label), 
			_("<i>Configure keys which control the player.</i>"));
	table = gtk_table_new (3, 2, FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
	gtk_table_set_col_spacings (GTK_TABLE (table), 2);
	gtk_table_set_row_spacings (GTK_TABLE (table), 2);

	add_event_controls(table, &controls->jump_to_file, 0, _("Jump to File:"), 
			plugin_cfg.jump_to_file);

	add_event_controls(table, &controls->toggle_win, 1, _("Toggle Player Windows:"), 
			plugin_cfg.toggle_win);


	button_box = gtk_hbutton_box_new ( );
	gtk_box_pack_start (GTK_BOX (main_vbox), button_box, FALSE, TRUE, 6);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (button_box), GTK_BUTTONBOX_END);
	gtk_box_set_spacing (GTK_BOX (button_box), 4);
	
	button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	gtk_container_add (GTK_CONTAINER (button_box), button);
	g_signal_connect (G_OBJECT (button), "clicked",
			G_CALLBACK (cancel_callback), controls);
	
	button = gtk_button_new_from_stock (GTK_STOCK_OK);
	gtk_container_add (GTK_CONTAINER (button_box), button);
	g_signal_connect (G_OBJECT (button), "clicked",
			G_CALLBACK (ok_callback), controls);
	
	gtk_widget_show_all (GTK_WIDGET (window));
}
/* configuration window end */

static void about (void)
{
	static GtkWidget *dialog;

	dialog = audacious_info_dialog (_("About Global Hotkey Plugin"),
				_("Global Hotkey Plugin\n"
				"Control the player with global key combinations or multimedia keys.\n\n"
				"Copyright (C) 2007-2008 Sascha Hlusiak <contact@saschahlusiak.de>\n\n"
				"Contributers include:\n"
				"Copyright (C) 2006-2007 Vladimir Paskov <vlado.paskov@gmail.com>\n"
				"Copyright (C) 2000-2002 Ville Syrjälä <syrjala@sci.fi>\n"
                         	"			Bryn Davies <curious@ihug.com.au>\n"
                        	"			Jonathan A. Davis <davis@jdhouse.org>\n"
                         	"			Jeremy Tan <nsx@nsx.homeip.net>\n\n"
                         	),
                         	_("OK"), TRUE, NULL, NULL);

	gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
			   GTK_SIGNAL_FUNC(gtk_widget_destroyed), &dialog);						
}

/* Clear keys */
static void clear_keyboard (GtkWidget *widget, gpointer data)
{
	KeyControls *spins = (KeyControls*)data;
	spins->hotkey.key = 0;
	spins->hotkey.mask = 0;
	spins->hotkey.type = TYPE_KEY;
	set_keytext(spins->keytext, 0, 0, TYPE_KEY);
}

void cancel_callback (GtkWidget *widget, gpointer data)
{
	if (loaded)
	{
		grab_keys ();
	}
	if (data) g_free(data);

	gtk_widget_destroy (gtk_widget_get_toplevel (GTK_WIDGET (widget)));
}

void ok_callback (GtkWidget *widget, gpointer data)
{
	ConfigurationControls *controls= (ConfigurationControls*)data;
	
	plugin_cfg.play = controls->play.hotkey;
	plugin_cfg.pause = controls->pause.hotkey;
	plugin_cfg.stop= controls->stop.hotkey;
	plugin_cfg.prev_track= controls->prev_track.hotkey;
	plugin_cfg.next_track = controls->next_track.hotkey;
	plugin_cfg.forward = controls->forward.hotkey;
	plugin_cfg.backward = controls->backward.hotkey;
	plugin_cfg.vol_up= controls->vol_up.hotkey;
	plugin_cfg.vol_down = controls->vol_down.hotkey;
	plugin_cfg.mute = controls->mute.hotkey;
	plugin_cfg.jump_to_file= controls->jump_to_file.hotkey;
	plugin_cfg.toggle_win = controls->toggle_win.hotkey;
	
	save_config ( );
	
	if (loaded)
	{
		grab_keys ();
	}

	if (data) g_free(data);
	
	gtk_widget_destroy (gtk_widget_get_toplevel (GTK_WIDGET (widget)));
}

/* 
 * plugin cleanup 
 */
static void cleanup (void)
{
	if (!loaded) return;
	ungrab_keys ();
	release_filter();
	loaded = FALSE;
}
 
static void ungrab_keys ()
{
	if (!grabbed) return;
	if (!xdisplay) return;
	
	XUngrabKey (xdisplay, AnyKey, AnyModifier, x_root_window);
	
	grabbed = 0;
}
