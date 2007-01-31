/* ghosd -- OSD with fake transparency, cairo, and pango.
 * Copyright (C) 2006 Evan Martin <martine@danga.com>
 *
 * With further development by Giacomo Lozito <james@develia.org>
 * for the ghosd-based Audacious OSD
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

struct _Ghosd {
  Display *dpy;
  Window win;
  int transparent;
  int x, y, width, height;
  
  Pixmap background;
  RenderCallback render;
  EventButtonCallback eventbutton;
};

/* vim: set ts=2 sw=2 et cino=(0 : */
