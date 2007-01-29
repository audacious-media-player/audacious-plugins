/* ghosd -- OSD with fake transparency, cairo, and pango.
 * Copyright (C) 2006 Evan Martin <martine@danga.com>
 */

#include <X11/Xlib.h>

#include "ghosd.h"

typedef struct {
  GhosdRenderFunc func;
  void *data;
  void (*data_destroy)(void*);
} RenderCallback;

struct _Ghosd {
  Display *dpy;
  Window win;
  int transparent;
  int x, y, width, height;
  
  Pixmap background;
  RenderCallback render;
};

/* vim: set ts=2 sw=2 et cino=(0 : */
