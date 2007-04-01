/* ghosd -- OSD with fake transparency, cairo, and pango.
 * Copyright (C) 2006 Evan Martin <martine@danga.com>
 *
 * With further development by Giacomo Lozito <james@develia.org>
 * for the ghosd-based Audacious OSD
 * - added real transparency with X Composite Extension
 * - added mouse event handling on OSD window
 * - added/changed some other stuff
 */

#include <X11/Xlib.h>

#include "ghosd.h"

typedef struct {
  GhosdRenderFunc func;
  void *data;
  void (*data_destroy)(void*);
} RenderCallback;

typedef struct {
  GhosdEventButtonCb func;
  void *data;
} EventButtonCallback;

typedef struct {
  Pixmap pixmap;
  int set;
} GhosdBackground;

struct _Ghosd {
  Display *dpy;
  Window win;
  Window root_win;
  Visual *visual;
  Colormap colormap;
  int screen_num;
  unsigned int depth;
  int transparent;
  int composite;
  int x, y, width, height;

  GhosdBackground background;
  RenderCallback render;
  EventButtonCallback eventbutton;
};

/* vim: set ts=2 sw=2 et cino=(0 : */
