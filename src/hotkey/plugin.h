#ifndef _PLUGIN_H_INCLUDED_
#define _PLUGIN_H_INCLUDED_

#include <glib.h>

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
	HotkeyConfiguration show_aosd;
} PluginConfig;

void load_config (void);
void save_config (void);
PluginConfig* get_config(void);
gboolean is_loaded (void);
gboolean handle_keyevent(int keycode, int state, int type);

#endif
