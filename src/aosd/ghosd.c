/* ghosd -- OSD with fake transparency, cairo, and pango.
 * Copyright (C) 2006 Evan Martin <martine@danga.com>
 *
 * With further development by Giacomo Lozito <james@develia.org>
 * for the ghosd-based Audacious OSD
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <cairo/cairo-xlib-xrender.h>
#include <X11/Xatom.h>

#include "ghosd.h"
#include "ghosd-internal.h"

static unsigned long
get_current_workspace(Ghosd *ghosd) {
  Atom cur_workspace_atom;
  Atom type;
  int format;
  unsigned long nitems, bytes_after;
  unsigned char *data;

  cur_workspace_atom = XInternAtom(ghosd->dpy, "_NET_CURRENT_DESKTOP", False);
  XGetWindowProperty(ghosd->dpy, DefaultRootWindow(ghosd->dpy), cur_workspace_atom,
    0, ULONG_MAX, False, XA_CARDINAL, &type, &format, &nitems, &bytes_after, &data);

  if ( type == XA_CARDINAL )
  {
    unsigned long cur_workspace = (unsigned long)*data;
    g_print("debug: %i\n", cur_workspace);
    XFree( data );
    return cur_workspace;
  }

  /* fall back to desktop number 0 */
  return 0;
}

static Pixmap
take_snapshot(Ghosd *ghosd) {
  Pixmap pixmap;
  GC gc;

  /* create a pixmap to hold the screenshot. */
  pixmap = XCreatePixmap(ghosd->dpy, ghosd->win,
                         ghosd->width, ghosd->height,
                         DefaultDepth(ghosd->dpy, DefaultScreen(ghosd->dpy)));

  /* then copy the screen into the pixmap. */
  gc = XCreateGC(ghosd->dpy, pixmap, 0, NULL);
  XSetSubwindowMode(ghosd->dpy, gc, IncludeInferiors);
  XCopyArea(ghosd->dpy, DefaultRootWindow(ghosd->dpy), pixmap, gc,
            ghosd->x, ghosd->y, ghosd->width, ghosd->height,
            0, 0);
  XSetSubwindowMode(ghosd->dpy, gc, ClipByChildren);
  XFreeGC(ghosd->dpy, gc);

  return pixmap;
}

void
ghosd_render(Ghosd *ghosd) {
  Pixmap pixmap;
  GC gc;

  /* make our own copy of the background pixmap as the initial surface. */
  pixmap = XCreatePixmap(ghosd->dpy, ghosd->win, ghosd->width, ghosd->height,
                         DefaultDepth(ghosd->dpy, DefaultScreen(ghosd->dpy)));

  gc = XCreateGC(ghosd->dpy, pixmap, 0, NULL);
  if (ghosd->transparent) {
    XCopyArea(ghosd->dpy, ghosd->background.pixmap, pixmap, gc,
              0, 0, ghosd->width, ghosd->height, 0, 0);
  } else {
    XFillRectangle(ghosd->dpy, pixmap, gc,
                  0, 0, ghosd->width, ghosd->height);
  }
  XFreeGC(ghosd->dpy, gc);

  /* render with cairo. */
  if (ghosd->render.func) {
    /* create cairo surface using the pixmap. */
    XRenderPictFormat *xrformat = 
      XRenderFindVisualFormat(ghosd->dpy,
                              DefaultVisual(ghosd->dpy,
                                            DefaultScreen(ghosd->dpy)));
    cairo_surface_t *surf =
      cairo_xlib_surface_create_with_xrender_format(
        ghosd->dpy, pixmap,
        ScreenOfDisplay(ghosd->dpy, DefaultScreen(ghosd->dpy)),
        xrformat,
        ghosd->width, ghosd->height);

    /* draw some stuff. */
    cairo_t *cr = cairo_create(surf);
    ghosd->render.func(ghosd, cr, ghosd->render.data);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
  }

  /* point window at its new backing pixmap. */
  XSetWindowBackgroundPixmap(ghosd->dpy, ghosd->win, pixmap);
  /* I think it's ok to free it here because XCreatePixmap(3X11) says: "the X
   * server frees the pixmap storage when there are no references to it".
   */
  XFreePixmap(ghosd->dpy, pixmap);

  /* and tell the window to redraw with this pixmap. */
  XClearWindow(ghosd->dpy, ghosd->win);
}

static void
set_hints(Display *dpy, Window win) {
  XClassHint *classhints;
  char *res_class = "Audacious";
  char *res_name = "aosd";

  /* we're almost a _NET_WM_WINDOW_TYPE_SPLASH, but we don't want
   * to be centered on the screen.  instead, manually request the
   * behavior we want. */

  /* turn off window decorations.
   * we could pull this in from a motif header, but it's easier to
   * use this snippet i found on a mailing list.  */
  Atom mwm_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
#define MWM_HINTS_DECORATIONS (1<<1)
  struct {
    long flags, functions, decorations, input_mode;
  } mwm_hints_setting = {
    MWM_HINTS_DECORATIONS, 0, 0, 0
  };
  XChangeProperty(dpy, win,
    mwm_hints, mwm_hints, 32, PropModeReplace,
    (unsigned char *)&mwm_hints_setting, 4);

  /* always on top, not in taskbar or pager. */
  Atom win_state = XInternAtom(dpy, "_NET_WM_STATE", False);
  Atom win_state_setting[] = {
    XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False),
    XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False),
    XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False)
  };
  XChangeProperty(dpy, win, win_state, XA_ATOM, 32,
                  PropModeReplace, (unsigned char*)&win_state_setting, 3);

  /* give initial pos/size information to window manager
     about the window, this prevents flickering */
  /* NOTE: unneeded if override_redirect is set to True
  sizehints = XAllocSizeHints();
  sizehints->flags = USPosition | USSize;
  sizehints->x = -1;
  sizehints->y = -1;
  sizehints->width = 1;
  sizehints->height = 1;
  XSetWMNormalHints(dpy, win, sizehints);
  XFree( sizehints );*/

  classhints = XAllocClassHint();
  classhints->res_name = res_name;
  classhints->res_class = res_class;
  XSetClassHint(dpy, win, classhints);
  XFree( classhints );
}

static Window
make_window(Display *dpy) {
  Window win;
  XSetWindowAttributes att;

  att.backing_store = WhenMapped;
  att.background_pixel = None;
  att.border_pixel = 0;
  att.background_pixmap = None;
  att.save_under = True;
  att.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask;
  att.override_redirect = True;

  win = XCreateWindow(dpy, DefaultRootWindow(dpy),
                      -1, -1, 1, 1, 0,
                      CopyFromParent, InputOutput, CopyFromParent,
                      CWBackingStore | CWBackPixel | CWBackPixmap |
                      CWEventMask | CWSaveUnder | CWOverrideRedirect,
                      &att);

  set_hints(dpy, win);

  return win;
}

void
ghosd_show(Ghosd *ghosd) {
  if (ghosd->transparent) {
    if (ghosd->background.set)
    {
      XFreePixmap(ghosd->dpy, ghosd->background.pixmap);
      ghosd->background.set = 0;
    }
    ghosd->background.pixmap = take_snapshot(ghosd);
    ghosd->background.set = 1;
  }

  ghosd_render(ghosd);

  XMapWindow(ghosd->dpy, ghosd->win);
}

void
ghosd_hide(Ghosd *ghosd) {
  XUnmapWindow(ghosd->dpy, ghosd->win);
}

void
ghosd_set_transparent(Ghosd *ghosd, int transparent) {
  ghosd->transparent = (transparent != 0);
}

void
ghosd_set_render(Ghosd *ghosd, GhosdRenderFunc render_func,
                 void *user_data, void (*user_data_d)(void*)) {
  ghosd->render.func = render_func;
  ghosd->render.data = user_data;
  ghosd->render.data_destroy = user_data_d;
}

void
ghosd_set_position(Ghosd *ghosd, int x, int y, int width, int height) {
  const int dpy_width  = DisplayWidth(ghosd->dpy,  DefaultScreen(ghosd->dpy));
  const int dpy_height = DisplayHeight(ghosd->dpy, DefaultScreen(ghosd->dpy));

  if (x == GHOSD_COORD_CENTER) {
    x = (dpy_width - width) / 2;
  } else if (x < 0) {
    x = dpy_width - width + x;
  }

  if (y == GHOSD_COORD_CENTER) {
    y = (dpy_height - height) / 2;
  } else if (y < 0) {
    y = dpy_height - height + y;
  }

  ghosd->x      = x;
  ghosd->y      = y;
  ghosd->width  = width;
  ghosd->height = height;

  XMoveResizeWindow(ghosd->dpy, ghosd->win,
                    ghosd->x, ghosd->y, ghosd->width, ghosd->height);
}

void
ghosd_set_event_button_cb(Ghosd *ghosd, GhosdEventButtonCb func, void *user_data)
{
  ghosd->eventbutton.func = func;
  ghosd->eventbutton.data = user_data;
}

#if 0
static int
x_error_handler(Display *dpy, XErrorEvent* evt) {
  /* segfault so we can get a backtrace. */
  char *x = NULL;
  *x = 0;
  return 0;
}
#endif

Ghosd*
ghosd_new(void) {
  Ghosd *ghosd;
  Display *dpy;
  Window win;

  dpy = XOpenDisplay(NULL);
  if (dpy == NULL) {
    fprintf(stderr, "Couldn't open display: (XXX FIXME)\n");
    return NULL;
  }

  win = make_window(dpy);
  
  ghosd = calloc(1, sizeof(Ghosd));
  ghosd->dpy = dpy;
  ghosd->win = win;
  ghosd->transparent = 1;
  ghosd->eventbutton.func = NULL;
  ghosd->background.set = 0;

  return ghosd;
}

void
ghosd_destroy(Ghosd* ghosd) {
  if (ghosd->background.set)
  {
    XFreePixmap(ghosd->dpy, ghosd->background.pixmap);
    ghosd->background.set = 0;
  }
  XDestroyWindow(ghosd->dpy, ghosd->win);
}

int
ghosd_get_socket(Ghosd *ghosd) {
  return ConnectionNumber(ghosd->dpy);
}

/* vim: set ts=2 sw=2 et cino=(0 : */
