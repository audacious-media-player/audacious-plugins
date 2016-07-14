/* ghosd -- OSD with fake transparency, cairo, and pango.
 * Copyright (C) 2006 Evan Martin <martine@danga.com>
 *
 * With further development by Giacomo Lozito <james@develia.org>
 * for the ghosd-based Audacious OSD
 * - added real transparency with X Composite Extension
 * - added mouse event handling on OSD window
 * - added/changed some other stuff
 */

#include <stdio.h>
#include <stdlib.h>
#include <cairo/cairo-xlib-xrender.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <X11/extensions/Xcomposite.h>

#include <glib.h>

#include "ghosd.h"
#include "ghosd-internal.h"

static Bool
composite_find_manager(Display *dpy, int scr)
{
  Atom comp_manager_atom;
  char comp_manager_hint[32];
  Window win;

  snprintf(comp_manager_hint, 32, "_NET_WM_CM_S%d", scr);
  comp_manager_atom = XInternAtom(dpy, comp_manager_hint, False);
  win = XGetSelectionOwner(dpy, comp_manager_atom);

  if (win != None)
  {
    return True;
  }
  else
  {
    return False;
  }
}

static Visual *
composite_find_argb_visual(Display *dpy, int scr)
{
  XVisualInfo	*xvi;
  XVisualInfo	template;
  int nvi, i;
  XRenderPictFormat *format;
  Visual *visual;

  template.screen = scr;
  template.depth = 32;
  template.class = TrueColor;
  xvi = XGetVisualInfo (dpy,
          VisualScreenMask | VisualDepthMask | VisualClassMask,
          &template, &nvi);
  if (xvi == NULL)
    return NULL;

  visual = NULL;
  for (i = 0; i < nvi; i++)
  {
    format = XRenderFindVisualFormat (dpy, xvi[i].visual);
    if (format->type == PictTypeDirect && format->direct.alphaMask)
    {
      visual = xvi[i].visual;
      break;
    }
  }
  XFree (xvi);

  return visual;
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

  if (ghosd->composite)
  {
    pixmap = XCreatePixmap(ghosd->dpy, ghosd->win, ghosd->width, ghosd->height, 32);
    gc = XCreateGC(ghosd->dpy, pixmap, 0, NULL);
    XFillRectangle(ghosd->dpy, pixmap, gc,
      0, 0, ghosd->width, ghosd->height);
  }
  else
  {
    pixmap = XCreatePixmap(ghosd->dpy, ghosd->win, ghosd->width, ghosd->height,
      DefaultDepth(ghosd->dpy, DefaultScreen(ghosd->dpy)));
    gc = XCreateGC(ghosd->dpy, pixmap, 0, NULL);
    if (ghosd->transparent) {
      /* make our own copy of the background pixmap as the initial surface. */
      XCopyArea(ghosd->dpy, ghosd->background.pixmap, pixmap, gc,
        0, 0, ghosd->width, ghosd->height, 0, 0);
    } else {
      XFillRectangle(ghosd->dpy, pixmap, gc,
        0, 0, ghosd->width, ghosd->height);
    }
  }
  XFreeGC(ghosd->dpy, gc);

  /* render with cairo. */
  if (ghosd->render.func) {
    /* create cairo surface using the pixmap. */
    XRenderPictFormat *xrformat;
    cairo_surface_t *surf;
    if (ghosd->composite) {
      xrformat = XRenderFindVisualFormat(ghosd->dpy, ghosd->visual);
      surf = cairo_xlib_surface_create_with_xrender_format(
               ghosd->dpy, pixmap,
               ScreenOfDisplay(ghosd->dpy, ghosd->screen_num),
               xrformat, ghosd->width, ghosd->height);
    } else {
      xrformat = XRenderFindVisualFormat(ghosd->dpy,
                   DefaultVisual(ghosd->dpy, DefaultScreen(ghosd->dpy)));
      surf = cairo_xlib_surface_create_with_xrender_format(
               ghosd->dpy, pixmap,
               ScreenOfDisplay(ghosd->dpy, DefaultScreen(ghosd->dpy)),
               xrformat, ghosd->width, ghosd->height);
    }

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
make_window(Display *dpy, Window root_win, Visual *visual, Colormap colormap, Bool use_argbvisual) {
  Window win;
  XSetWindowAttributes att;

  att.backing_store = WhenMapped;
  att.background_pixel = 0x0;
  att.border_pixel = 0;
  att.background_pixmap = None;
  att.save_under = True;
  att.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask;
  att.override_redirect = True;

  if ( use_argbvisual )
  {
    att.colormap = colormap;
    win = XCreateWindow(dpy, root_win,
                      -1, -1, 1, 1, 0, 32, InputOutput, visual,
                      CWBackingStore | CWBackPixel | CWBackPixmap | CWBorderPixel |
                      CWColormap | CWEventMask | CWSaveUnder | CWOverrideRedirect,
                      &att);
  } else {
    win = XCreateWindow(dpy, root_win,
                      -1, -1, 1, 1, 0, CopyFromParent, InputOutput, CopyFromParent,
                      CWBackingStore | CWBackPixel | CWBackPixmap | CWBorderPixel |
                      CWEventMask | CWSaveUnder | CWOverrideRedirect,
                      &att);
  }

  set_hints(dpy, win);

  return win;
}

void
ghosd_show(Ghosd *ghosd) {
  if ((!ghosd->composite) && (ghosd->transparent)) {
    if (ghosd->background.set)
    {
      XFreePixmap(ghosd->dpy, ghosd->background.pixmap);
      ghosd->background.set = 0;
    }
    ghosd->background.pixmap = take_snapshot(ghosd);
    ghosd->background.set = 1;
  }

  ghosd_render(ghosd);
  XMapRaised(ghosd->dpy, ghosd->win);
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

Ghosd*
ghosd_new(void) {
  Ghosd *ghosd;
  Display *dpy;
  Window win, root_win;
  int screen_num;
  Visual *visual;
  Colormap colormap;
  Bool use_argbvisual = False;

  dpy = XOpenDisplay(NULL);
  if (dpy == NULL) {
    fprintf(stderr, "Couldn't open display: (XXX FIXME)\n");
    return NULL;
  }

  screen_num = DefaultScreen(dpy);
  root_win = RootWindow(dpy, screen_num);
  visual = NULL; /* unused */
  colormap = None; /* unused */

  win = make_window(dpy, root_win, visual, colormap, use_argbvisual);

  ghosd = g_new0(Ghosd, 1);
  ghosd->dpy = dpy;
  ghosd->visual = visual;
  ghosd->colormap = colormap;
  ghosd->win = win;
  ghosd->root_win = root_win;
  ghosd->screen_num = screen_num;
  ghosd->transparent = 1;
  ghosd->composite = 0;
  ghosd->eventbutton.func = NULL;
  ghosd->background.set = 0;

  return ghosd;
}

Ghosd *
ghosd_new_with_argbvisual(void) {
  Ghosd *ghosd;
  Display *dpy;
  Window win, root_win;
  int screen_num;
  Visual *visual;
  Colormap colormap;
  Bool use_argbvisual = True;

  dpy = XOpenDisplay(NULL);
  if (dpy == NULL) {
    fprintf(stderr, "Couldn't open display: (XXX FIXME)\n");
    return NULL;
  }

  screen_num = DefaultScreen(dpy);
  root_win = RootWindow(dpy, screen_num);
  visual = composite_find_argb_visual(dpy, screen_num);
  if (visual == NULL)
    return NULL;
  colormap = XCreateColormap(dpy, root_win, visual, AllocNone);

  win = make_window(dpy, root_win, visual, colormap, use_argbvisual);

  ghosd = g_new0(Ghosd, 1);
  ghosd->dpy = dpy;
  ghosd->visual = visual;
  ghosd->colormap = colormap;
  ghosd->win = win;
  ghosd->root_win = root_win;
  ghosd->screen_num = screen_num;
  ghosd->transparent = 1;
  ghosd->composite = 1;
  ghosd->eventbutton.func = NULL;
  ghosd->background.set = 0;

  return ghosd;
}

int
ghosd_check_composite_ext(void)
{
  Display *dpy;
  int have_composite_x = 0;
  int composite_event_base = 0, composite_error_base = 0;

  dpy = XOpenDisplay(NULL);
  if (dpy == NULL) {
    fprintf(stderr, "Couldn't open display: (XXX FIXME)\n");
    return 0;
  }

  if (!XCompositeQueryExtension(dpy,
        &composite_event_base, &composite_error_base))
    have_composite_x = 0;
  else
    have_composite_x = 1;

  XCloseDisplay(dpy);
  return have_composite_x;
}

int
ghosd_check_composite_mgr(void)
{
  Display *dpy;
  int have_composite_m = 0;

  dpy = XOpenDisplay(NULL);
  if (dpy == NULL) {
    fprintf(stderr, "Couldn't open display: (XXX FIXME)\n");
    return 0;
  }

  if (!composite_find_manager(dpy, DefaultScreen(dpy)))
    have_composite_m = 0;
  else
    have_composite_m = 1;

  XCloseDisplay(dpy);
  return have_composite_m;
}

void
ghosd_destroy(Ghosd* ghosd) {
  if (ghosd->background.set)
  {
    XFreePixmap(ghosd->dpy, ghosd->background.pixmap);
    ghosd->background.set = 0;
  }
  if (ghosd->composite)
  {
    XFreeColormap(ghosd->dpy, ghosd->colormap);
  }
  XDestroyWindow(ghosd->dpy, ghosd->win);
  XCloseDisplay(ghosd->dpy);
}

int
ghosd_get_socket(Ghosd *ghosd) {
  return ConnectionNumber(ghosd->dpy);
}

/* vim: set ts=2 sw=2 et cino=(0 : */
