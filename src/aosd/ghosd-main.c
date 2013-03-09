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
#include <sys/time.h>
#include <sys/poll.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <gdk/gdk.h>

#include "ghosd.h"
#include "ghosd-internal.h"

static void
ghosd_main_iteration(Ghosd *ghosd) {
  XEvent ev, pev;
  XNextEvent(ghosd->dpy, &ev);

  /* smash multiple configure/exposes into one. */
  if (ev.type == ConfigureNotify) {
    while (XPending(ghosd->dpy)) {
      XPeekEvent(ghosd->dpy, &pev);
      if (pev.type != ConfigureNotify && pev.type != Expose)
        break;
      XNextEvent(ghosd->dpy, &ev);
    }
  }

  switch (ev.type) {
  case Expose:
    break;
  case ConfigureNotify:
    if (ghosd->width > 0) {
      /* XXX if the window manager disagrees with our positioning here,
       * we loop. */
      if (ghosd->x != ev.xconfigure.x ||
          ghosd->y != ev.xconfigure.y) {
        /*width = ev.xconfigure.width;
        height = ev.xconfigure.height;*/
        XMoveResizeWindow(ghosd->dpy, ghosd->win,
                          ghosd->x, ghosd->y, ghosd->width, ghosd->height);
      }
    }
    break;
  case ButtonPress:
    /* create a GhosdEventButton event and pass it to callback function */
    if ( ghosd->eventbutton.func != NULL )
    {
      GhosdEventButton gevb;
      gevb.x = ev.xbutton.x;
      gevb.y = ev.xbutton.y;
      gevb.x_root = ev.xbutton.x_root;
      gevb.y_root = ev.xbutton.y_root;
      gevb.button = ev.xbutton.button;
      gevb.send_event = ev.xbutton.send_event;
      gevb.time = ev.xbutton.time;
      ghosd->eventbutton.func( ghosd , &gevb , ghosd->eventbutton.data );
    }
    break;
  }
}

void
ghosd_main_iterations(Ghosd *ghosd) {
  while (XPending(ghosd->dpy))
    ghosd_main_iteration(ghosd);
}

void
ghosd_main_until(Ghosd *ghosd, struct timeval *until) {
  struct timeval tv_now;

  ghosd_main_iterations(ghosd);

  for (;;) {
    gettimeofday(&tv_now, NULL);
    int dt = (until->tv_sec  - tv_now.tv_sec )*1000 +
             (until->tv_usec - tv_now.tv_usec)/1000;
    if (dt <= 0) break;

    struct pollfd pollfd = { ghosd_get_socket(ghosd), POLLIN, 0 };
    int ret = poll(&pollfd, 1, dt);
    if (ret < 0) {
      if ( errno != EINTR )
      {
        perror("poll");
        exit(1);
      }
      /* else go on, ignore EINTR */
    } else if (ret > 0) {
      ghosd_main_iterations(ghosd);
    } else {
      /* timer expired. */
      break;
    }
  }
}

typedef struct {
  cairo_surface_t* surface;
  float alpha;
  RenderCallback user_render;
} GhosdFlashData;

static void
flash_render(Ghosd *ghosd, cairo_t *cr, void* data) {
  GhosdFlashData *flash = data;

  /* the first time we render, let the client render into their own surface. */
  if (flash->surface == NULL) {
    cairo_t *rendered_cr;
    flash->surface = cairo_surface_create_similar(cairo_get_target(cr),
                                                  CAIRO_CONTENT_COLOR_ALPHA,
                                                  ghosd->width, ghosd->height);
    rendered_cr = cairo_create(flash->surface);
    flash->user_render.func(ghosd, rendered_cr, flash->user_render.data);
    cairo_destroy(rendered_cr);
  }

  /* now that we have a rendered surface, all we normally do is copy that to
   * the screen. */
  cairo_set_source_surface(cr, flash->surface, 0, 0);
  cairo_paint_with_alpha(cr, flash->alpha);
}

/* we don't need to free the flashdata object, because we stack-allocate that.
 * but we do need to let the old user data free itself... */
static void
flash_destroy(void *data) {
  GhosdFlashData *flash = data;
  if (flash->user_render.data_destroy)
    flash->user_render.data_destroy(flash->user_render.data);
}

void
ghosd_flash(Ghosd *ghosd, int fade_ms, int total_display_ms) {
  GhosdFlashData flash = {0};
  memcpy(&flash.user_render, &ghosd->render, sizeof(RenderCallback));
  ghosd_set_render(ghosd, flash_render, &flash, flash_destroy);

  ghosd_show(ghosd);

  const int STEP_MS = 50;
  const float dalpha = 1.0 / (fade_ms / (float)STEP_MS);
  struct timeval tv_nextupdate;

  /* fade in. */
  for (flash.alpha = 0; flash.alpha < 1.0; flash.alpha += dalpha) {
    if (flash.alpha > 1.0) flash.alpha = 1.0;
    ghosd_render(ghosd);

    gettimeofday(&tv_nextupdate, NULL);
    tv_nextupdate.tv_usec += STEP_MS*1000;
    ghosd_main_until(ghosd, &tv_nextupdate);
  }

  /* full display. */
  flash.alpha = 1.0;
  ghosd_render(ghosd);

  gettimeofday(&tv_nextupdate, NULL);
  tv_nextupdate.tv_usec += (total_display_ms - (2*fade_ms))*1000;
  ghosd_main_until(ghosd, &tv_nextupdate);

  /* fade out. */
  for (flash.alpha = 1.0; flash.alpha > 0.0; flash.alpha -= dalpha) {
    ghosd_render(ghosd);

    gettimeofday(&tv_nextupdate, NULL);
    tv_nextupdate.tv_usec += STEP_MS*1000;
    ghosd_main_until(ghosd, &tv_nextupdate);
  }

  flash.alpha = 0;
  ghosd_render(ghosd);

  /* display for another half-second,
   * because otherwise the fade out attracts your eye
   * and then you'll see a flash while it repaints where the ghosd was.
   */
  gettimeofday(&tv_nextupdate, NULL);
  tv_nextupdate.tv_usec += 500*1000;
  ghosd_main_until(ghosd, &tv_nextupdate);
}

/* vim: set ts=2 sw=2 et cino=(0 : */
