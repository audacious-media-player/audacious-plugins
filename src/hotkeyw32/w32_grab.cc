

#include "../hotkey/grab.h"

#include <gtk/gtk.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <libaudcore/runtime.h>

#include "../hotkey/plugin.h"

void grab_keys() { AUDDBG("lHotkeyFlow:w_grab: grab_keys"); }
void ungrab_keys() { AUDDBG("lHotkeyFlow:w_grab: ungrab_keys"); }