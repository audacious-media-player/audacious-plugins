#ifndef _PLUGIN_H_INCLUDED_
#define _PLUGIN_H_INCLUDED_

#include <glib.h>

#define TYPE_KEY 0
#define TYPE_MOUSE 1

#ifdef _WIN32
#define HK_CONTROL_MASK MOD_CONTROL
#define HK_SHIFT_MASK MOD_SHIFT
#define HK_MOD1_ALT_MASK MOD_ALT
#define HK_MOD2_MASK MOD_WIN
#define HK_MOD3_MASK 0x0016
#define HK_MOD4_MASK 0x0032
#define HK_MOD5_MASK 0x0064
#define HK_WIN_NONREPEAT MOD_NOREPEAT
#else
#define HK_CONTROL_MASK ControlMask
#define HK_SHIFT_MASK ShiftMask
#define HK_MOD1_ALT_MASK Mod1Mask
#define HK_MOD2_MASK Mod2Mask
#define HK_MOD3_MASK Mod3Mask
#define HK_MOD4_MASK Mod4Mask
#define HK_MOD5_MASK Mod5Mask
#endif

typedef enum
{
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

typedef struct _HotkeyConfiguration
{
    unsigned key, mask;
    unsigned type;
    EVENT event;
    struct _HotkeyConfiguration * next;
} HotkeyConfiguration;

typedef struct
{
    /* keyboard */
    HotkeyConfiguration first;
} PluginConfig;

void load_config();
void save_config();
PluginConfig * get_config();
gboolean handle_keyevent(EVENT event);
#ifdef _WIN32
void win_init();
extern PluginConfig plugin_cfg;
#endif

#endif
