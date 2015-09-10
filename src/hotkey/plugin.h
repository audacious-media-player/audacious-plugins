#ifndef _PLUGIN_H_INCLUDED_
#define _PLUGIN_H_INCLUDED_

#include <glib.h>

#define TYPE_KEY 0
#define TYPE_MOUSE 1

typedef enum {
    EVENT_PREV_TRACK = 0,
    EVENT_PLAY,
    EVENT_PAUSE,
    EVENT_STOP,
    EVENT_NEXT_TRACK,

    EVENT_FORWARD,
    EVENT_BACKWARD,
    EVENT_MUTE,
    EVENT_VOL_UP,
    EVENT_VOL_DOWN,
    EVENT_JUMP_TO_FILE,
    EVENT_TOGGLE_WIN,
    EVENT_SHOW_AOSD,

    EVENT_TOGGLE_REPEAT,
    EVENT_TOGGLE_SHUFFLE,
    EVENT_TOGGLE_STOP,

    EVENT_RAISE,

    EVENT_MAX
} EVENT;


typedef struct _HotkeyConfiguration {
    unsigned key, mask;
    unsigned type;
    EVENT event;
    struct _HotkeyConfiguration *next;
} HotkeyConfiguration;

typedef struct {
    int vol_increment;
    int vol_decrement;

    /* keyboard */
    HotkeyConfiguration first;
} PluginConfig;



void load_config ();
void save_config ();
PluginConfig* get_config ();
gboolean handle_keyevent(EVENT event);

#endif
