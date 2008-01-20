/*
 *  This file is part of audacious-hotkey plugin for audacious
 *
 *  Copyright (c) 2007 - 2008  Sascha Hlusiak <contact@saschahlusiak.de>
 *  Name: gui.c
 *  Description: gui.c
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

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#include <audacious/i18n.h>
#include <audacious/util.h>

#include "plugin.h"
#include "gui.h"
#include "grab.h"


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
	KeyControls show_aosd;
} ConfigurationControls;



static void clear_keyboard (GtkWidget *widget, gpointer data);
static void cancel_callback (GtkWidget *widget, gpointer data);
static void destroy_callback (GtkWidget *widget, gpointer data);
static void ok_callback (GtkWidget *widget, gpointer data);


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
		if (type == TYPE_KEY)
		{
			KeySym keysym;
			keysym = XKeycodeToKeysym(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), key, 0);
			if (keysym == 0 || keysym == NoSymbol)
			{
				keytext = g_strdup_printf("#%d", key);
			} else {
				keytext = g_strdup(XKeysymToString(keysym));
			}
		}
		if (type == TYPE_MOUSE)
		{
			keytext = g_strdup_printf("Button%d", key);
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
	return TRUE;
}

static gboolean
on_entry_key_release_event(GtkWidget * widget,
                           GdkEventKey * event,
                           gpointer user_data)
{
	KeyControls *controls = (KeyControls*) user_data;
	if (controls->hotkey.key == 0) {
		controls->hotkey.mask = 0;
		return TRUE;
	}
	set_keytext(controls->keytext, controls->hotkey.key, controls->hotkey.mask, controls->hotkey.type);
	return TRUE;
}

static gboolean
on_entry_button_press_event(GtkWidget * widget,
                            GdkEventButton * event,
                            gpointer user_data)
{
	KeyControls *controls = (KeyControls*) user_data;
	int mod;
	
	if (!gtk_widget_is_focus(widget)) return FALSE;

	mod = 0;
	if (event->state & GDK_CONTROL_MASK)
        	mod |= ControlMask;

	if (event->state & GDK_MOD1_MASK)
        	mod |= Mod1Mask;

	if (event->state & GDK_SHIFT_MASK)
        	mod |= ShiftMask;

	if (event->state & GDK_MOD5_MASK)
        	mod |= Mod5Mask;

	if (event->state & GDK_MOD4_MASK)
        	mod |= Mod4Mask;

	if ((event->button <= 3) && (mod == 0))
	{
		GtkWidget* dialog;
		GtkResponseType response;
		dialog = gtk_message_dialog_new (GTK_WINDOW(gtk_widget_get_toplevel(widget)),
			GTK_DIALOG_MODAL,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_YES_NO,
			_("It is not recommended to bind the primary mouse buttons without modificators.\n\n"
			  "Do you want to continue?"));
		gtk_window_set_title(GTK_WINDOW(dialog), _("Binding mouse buttons"));
		response = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy (dialog);
		if (response != GTK_RESPONSE_YES) return TRUE;
	}

	controls->hotkey.key = event->button;
	controls->hotkey.mask = mod;
        controls->hotkey.type = TYPE_MOUSE;
	set_keytext(controls->keytext, controls->hotkey.key, controls->hotkey.mask, controls->hotkey.type);
	return TRUE;
}

static gboolean
on_entry_scroll_event(GtkWidget * widget,
                            GdkEventScroll * event,
                            gpointer user_data)
{
	KeyControls *controls = (KeyControls*) user_data;
	int mod;
	
	if (!gtk_widget_is_focus(widget)) return FALSE;

	mod = 0;
	if (event->state & GDK_CONTROL_MASK)
        	mod |= ControlMask;

	if (event->state & GDK_MOD1_MASK)
        	mod |= Mod1Mask;

	if (event->state & GDK_SHIFT_MASK)
        	mod |= ShiftMask;

	if (event->state & GDK_MOD5_MASK)
        	mod |= Mod5Mask;

	if (event->state & GDK_MOD4_MASK)
        	mod |= Mod4Mask;

	if (event->direction == GDK_SCROLL_UP)
		controls->hotkey.key = 4;
	else if (event->direction == GDK_SCROLL_DOWN)
		controls->hotkey.key = 5;
	else if (event->direction == GDK_SCROLL_LEFT)
		controls->hotkey.key = 6;
	else if (event->direction == GDK_SCROLL_RIGHT)
		controls->hotkey.key = 7;
	else return FALSE;

	controls->hotkey.mask = mod;
        controls->hotkey.type = TYPE_MOUSE;
	set_keytext(controls->keytext, controls->hotkey.key, controls->hotkey.mask, controls->hotkey.type);
	return TRUE;
}

static void add_event_controls(GtkWidget *table, 
				KeyControls *controls, 
				int row, 
				char* descr,
				char* tooltip,
				HotkeyConfiguration hotkey)
{
	GtkWidget *label;
	GtkWidget *button;

	controls->hotkey.key = hotkey.key;
	controls->hotkey.mask = hotkey.mask;
	controls->hotkey.type = hotkey.type;
	if (controls->hotkey.key == 0)
		controls->hotkey.mask = 0;

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
	g_signal_connect((gpointer)controls->keytext, "button_press_event",
                         G_CALLBACK(on_entry_button_press_event), controls);
	g_signal_connect((gpointer)controls->keytext, "scroll_event",
                         G_CALLBACK(on_entry_scroll_event), controls);

	button = gtk_button_new_with_label (_("None"));
	gtk_table_attach (GTK_TABLE (table), button, 2, 3, row, row+1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
	g_signal_connect (G_OBJECT (button), "clicked",
			G_CALLBACK (clear_keyboard), controls);

	if (tooltip != NULL) {
		GtkTooltips *tip = gtk_tooltips_new();
		gtk_tooltips_set_tip(tip, controls->keytext, tooltip, NULL);
		gtk_tooltips_set_tip(tip, button, tooltip, NULL);
		gtk_tooltips_set_tip(tip, label, tooltip, NULL);
	}
}

void show_configure ()
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
	PluginConfig* plugin_cfg;
	
	load_config ( );

	plugin_cfg = get_config();

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
	add_event_controls(table, &controls->prev_track, 0, _("Previous Track:"), NULL, 
			plugin_cfg->prev_track);

	add_event_controls(table, &controls->play, 1, _("Play:"), NULL, 
			plugin_cfg->play);

	add_event_controls(table, &controls->pause, 2, _("Pause/Resume:"), NULL,
			plugin_cfg->pause);

	add_event_controls(table, &controls->stop, 3, _("Stop:"), NULL,
			plugin_cfg->stop);

	add_event_controls(table, &controls->next_track, 4, _("Next Track:"), NULL,
			plugin_cfg->next_track);

	add_event_controls(table, &controls->forward, 5, _("Forward 5 sec.:"), NULL,
			plugin_cfg->forward);

	add_event_controls(table, &controls->backward, 6, _("Rewind 5 sec.:"), NULL,
			plugin_cfg->backward);


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

	add_event_controls(table, &controls->mute, 0, _("Mute:"),NULL, 
			plugin_cfg->mute);

	add_event_controls(table, &controls->vol_up, 1, _("Volume Up:"), NULL,
			plugin_cfg->vol_up);

	add_event_controls(table, &controls->vol_down, 2, _("Volume Down:"), NULL,
			plugin_cfg->vol_down);


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
	table = gtk_table_new (3, 3, FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
	gtk_table_set_col_spacings (GTK_TABLE (table), 2);
	gtk_table_set_row_spacings (GTK_TABLE (table), 2);

	add_event_controls(table, &controls->jump_to_file, 0, _("Jump to File:"), NULL,
			plugin_cfg->jump_to_file);

	add_event_controls(table, &controls->toggle_win, 1, _("Toggle Player Windows:"), NULL,
			plugin_cfg->toggle_win);

	add_event_controls(table, &controls->show_aosd, 2, _("Show On-Screen-Display:"), 
			_("For this, the Audacious OSD plugin must be activated."),
			plugin_cfg->show_aosd);

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

	g_signal_connect (G_OBJECT (window), "destroy",
		G_CALLBACK (destroy_callback), controls);

	gtk_widget_show_all (GTK_WIDGET (window));
}

static void clear_keyboard (GtkWidget *widget, gpointer data)
{
	KeyControls *spins = (KeyControls*)data;
	spins->hotkey.key = 0;
	spins->hotkey.mask = 0;
	spins->hotkey.type = TYPE_KEY;
	set_keytext(spins->keytext, 0, 0, TYPE_KEY);
}

void destroy_callback (GtkWidget *widget, gpointer data)
{
	if (is_loaded())
	{
		grab_keys ();
	}
	if (data) g_free(data);
}

void cancel_callback (GtkWidget *widget, gpointer data)
{
	gtk_widget_destroy (gtk_widget_get_toplevel (GTK_WIDGET (widget)));
}

void ok_callback (GtkWidget *widget, gpointer data)
{
	ConfigurationControls *controls= (ConfigurationControls*)data;
	PluginConfig* plugin_cfg = get_config();

	plugin_cfg->play = controls->play.hotkey;
	plugin_cfg->pause = controls->pause.hotkey;
	plugin_cfg->stop= controls->stop.hotkey;
	plugin_cfg->prev_track= controls->prev_track.hotkey;
	plugin_cfg->next_track = controls->next_track.hotkey;
	plugin_cfg->forward = controls->forward.hotkey;
	plugin_cfg->backward = controls->backward.hotkey;
	plugin_cfg->vol_up= controls->vol_up.hotkey;
	plugin_cfg->vol_down = controls->vol_down.hotkey;
	plugin_cfg->mute = controls->mute.hotkey;
	plugin_cfg->jump_to_file= controls->jump_to_file.hotkey;
	plugin_cfg->toggle_win = controls->toggle_win.hotkey;
	plugin_cfg->show_aosd = controls->show_aosd.hotkey;
	
	save_config ( );
	
	gtk_widget_destroy (gtk_widget_get_toplevel (GTK_WIDGET (widget)));
}



void show_about (void)
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
